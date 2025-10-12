#include <rad/net/tcp.h>
#include <rad/net/http/http_parser.h>
#include <rad/coro/task.h>
#include <rad/coro/when_all.h>
#include <rad/async/io_loop.h>
#include <rad/coro/spawn.h>
#include <rad/json/json.h>
#include <rad/io/files.h>
#include <rad/cli.h>
#include <rad/net/ssl/stream.h>
#include <rad/net/ssl/mbedtls_ctx.h>
#include <rad/net/ssl/openssl_ctx.h>
#include <rad/net/ssl/wolfssl_ctx.h>
#include <algorithm>
#include <iostream>

using namespace rad;
namespace http = net::http;
namespace ssl = net::ssl;
using SSLStream = ssl::stream<net::tcp::socket>;

struct ServerConfig {
    std::string bind_endpoint;
    std::string config_file;
    uint16_t port = 0;
    uint16_t ssl_port = 0;
};

struct ServerResource {
    std::string target;
    std::string content_type;
    const_buffer body;
};

struct ServerState {
    net::ipv4_endpoint bind_endpoint;
    net::ipv4_endpoint ssl_bind_endpoint;
    std::vector<std::vector<uint8_t>> buffers;
    std::vector<ServerResource> resources;
    std::string bad_request_response;
    std::string not_found_response;
    std::string method_not_allowed_response;

    auto find_resource(std::string_view target) const noexcept {
        return std::find_if(
            resources.begin(), resources.end(),
            [&](const ServerResource& r) { return r.target == target; });
    }
};

std::string_view make_index_message() {
    return "This is the index page!\nServed by http server!";
}

ServerState load_server_state(const ServerConfig& config) {
    ServerState state;
    state.bind_endpoint = net::ipv4_endpoint{config.bind_endpoint, config.port};
    state.ssl_bind_endpoint =
        net::ipv4_endpoint{config.bind_endpoint, config.ssl_port};
    state.resources.emplace_back("/index", "text/html; charset=utf-8",
                                 buffer(make_index_message()));
    std::string json_buff;
    io::files::read_all_file(config.config_file, dynamic_buffer(json_buff));
    auto jconfig = json::parse(json_buff);
    for (const auto& jmapping : jconfig.as_array()) {
        std::vector<uint8_t> file_buff;
        std::string mapping_target = jmapping.at("target").as_string();
        std::string mapping_file = jmapping.at("file").as_string();
        std::string mapping_type = jmapping.at("type").as_string();
        io::files::read_all_file(mapping_file, dynamic_buffer(file_buff));
        auto& mapped_buff = state.buffers.emplace_back(std::move(file_buff));
        state.resources.emplace_back(std::move(mapping_target),
                                     std::move(mapping_type),
                                     buffer(mapped_buff));
    }

    http::response res;
    res.status = static_cast<uint32_t>(http::response_status::bad_request);
    res.body = "The server cannot process the request due to malformed syntax "
               "or an invalid request message.";
    res.headers.insert(http::field::content_length,
                       std::to_string(res.body.size()));
    res.headers.insert(http::field::connection, "close");
    res.serialize(dynamic_buffer(state.bad_request_response));

    res.clear();
    res.status = static_cast<uint32_t>(http::response_status::not_found);
    res.body = "The server couldn't find the requested resource.";
    res.headers.insert(http::field::content_length,
                       std::to_string(res.body.size()));
    res.headers.insert(http::field::connection, "close");
    res.serialize(dynamic_buffer(state.not_found_response));

    res.clear();
    res.status =
        static_cast<uint32_t>(http::response_status::method_not_allowed);
    res.body = "The HTTP method used in the request is not supported for the "
               "target resource.";
    res.headers.insert(http::field::content_length,
                       std::to_string(res.body.size()));
    res.headers.insert(http::field::connection, "close");
    res.serialize(dynamic_buffer(state.method_not_allowed_response));

    return state;
}

using Spawner = spawner<io_loop>;

task<> handshake_client(net::tcp::socket&) {
    co_return;
}

task<> handshake_client(SSLStream& s) {
    s.ssl_engine().set_alpn_protocols(std::array<std::string, 1>{"http/1.1"});
    co_await s.async_handshake(ssl::handshake_type::server);
}

task<> shutdown_stream(net::tcp::socket& s, std::error_code& ec) {
    s.shutdown(net::socket_shutdown::write, ec);
    co_return;
}

task<> shutdown_stream(SSLStream& s, std::error_code& ec) {
    co_await s.async_shutdown(ec);
    if (!ec) {
        s.next_layer().shutdown(net::socket_shutdown::write, ec);
    }
}

template <class AsyncStream>
task<> serve_http_client(io_loop& loop, AsyncStream client,
                         const ServerState& state) {
    co_await handshake_client(client);
    // read http request
    std::string read_buffer;
    read_buffer.resize(18 * 1024);
    ring_consumer_producer rbuf{buffer(read_buffer)};
    http::request request;
    {
        http::request_incremental_parser parser{request};
        std::size_t total_read = 0;
        while (parser.need_more()) {
            if (total_read >= rbuf.capacity() || rbuf.full()) {
                throw std::system_error{
                    http::make_error(http::error::too_large_message)};
            }
            std::size_t n =
                co_await client.async_read_some(rbuf.available_space());
            rbuf.commit(n);
            std::error_code ec;
            parser.parse(rbuf, ec);
        }
        assert(!parser.need_more());
        if (parser.has_error()) {
            co_await client.async_write(buffer(state.bad_request_response));
            co_return;
        }
        assert(parser.done());
        if (!parser.done()) {
            throw std::system_error{
                http::make_error(http::error::partial_message)};
        }
    }
    if (request.method != http::verb::get &&
        request.method != http::verb::head) {
        std::cout << "[!] received request with method: "
                  << http::verb_to_string(request.method) << "\n";
        co_await client.async_write(buffer(state.method_not_allowed_response));
        co_return;
    }

    std::string_view request_target = request.target;
    auto res_it = state.find_resource(request_target);
    if (res_it == state.resources.end()) {
        co_await client.async_write(buffer(state.not_found_response));
        co_return;
    }

    std::string body_size = std::to_string(res_it->body.size());
    {
        http::response_view res;
        res.status = static_cast<uint32_t>(http::response_status::ok);
        res.version = http::version::v1_1;
        res.reason = "OK";
        res.headers.insert(http::field::content_length, body_size);
        res.headers.insert(http::field::content_type, res_it->content_type);
        res.headers.insert(http::field::accept_ranges, "none");
        read_buffer.clear();
        res.serialize(dynamic_buffer(read_buffer));
    }
    auto write_buffs =
        std::array<const_buffer, 2>{buffer(read_buffer), res_it->body};
    if (request.method == http::verb::head) {
        write_buffs.back() = {};
    }
    std::size_t written = co_await client.async_write(write_buffs);
    assert(written == write_buffs[0].size() + write_buffs[1].size());
    std::error_code ec;
    co_await shutdown_stream(client, ec);
    if (ec) {
        co_return;
    }
    read_buffer.resize(read_buffer.capacity());
    while (!ec) {
        co_await client.async_read_some(buffer(read_buffer), ec);
    }
}

task<> accept_clients(Spawner& sp, const ServerState& state) {
    std::cout << "[*] The http server is listening on '"
              << state.bind_endpoint.to_string() << "' ...\n";
    auto acceptor = net::tcp::listen(sp.executor(), state.bind_endpoint);
    auto acceptor_endpoint = acceptor.local_endpoint();
    std::cout << "[*] The http server is running on '"
              << acceptor_endpoint.to_string() << "' ...\n";
    while (1) {
        auto [client, epoint] = co_await acceptor.async_accept();
        std::cout << "[*] received an http client from '" << epoint.to_string()
                  << "\n";
        sp.spawn(serve_http_client(sp.executor(), std::move(client), state),
                 [](std::exception_ptr eptr) {
                     if (eptr) {
                         try {
                             std::rethrow_exception(eptr);
                         }
                         catch (const std::exception& ex) {
                             std::cout << "[!] failed to serve an http client: "
                                       << ex.what() << "\n";
                         }
                     }
                 });
    }
}

task<> accept_https_clients(ssl::context_base& ctx, Spawner& sp,
                            const ServerState& state) {
    std::cout << "[*] The https server is listening on '"
              << state.ssl_bind_endpoint.to_string() << "' ...\n";
    auto acceptor = net::tcp::listen(sp.executor(), state.ssl_bind_endpoint);
    auto acceptor_endpoint = acceptor.local_endpoint();
    std::cout << "[*] The https server is running on '"
              << acceptor_endpoint.to_string() << "' ...\n";
    while (1) {
        auto [client, epoint] = co_await acceptor.async_accept();
        std::cout << "[*] received an https client from '" << epoint.to_string()
                  << "\n";
        sp.spawn(serve_http_client(sp.executor(),
                                   SSLStream{ctx, std::move(client)}, state),
                 [](std::exception_ptr eptr) {
                     if (eptr) {
                         try {
                             std::rethrow_exception(eptr);
                         }
                         catch (const std::exception& ex) {
                             std::cout
                                 << "[!] failed to serve an https client: "
                                 << ex.what() << "\n";
                         }
                     }
                 });
    }
}

task<> run_server(ssl::context_base& ctx, io_loop& loop,
                  const ServerState& state) {
    co_await scoped_spawn(loop, [&](Spawner& sp) -> task<> {
        co_await (accept_clients(sp, state) &&
                  accept_https_clients(ctx, sp, state));
    });
}

int main(int argc, char** argv) {
    ServerConfig config;
    config.bind_endpoint = "127.0.0.1";
    config.port = 1508;
    config.ssl_port = 1509;
    config.config_file = "config.json";

    {
        cli::parser p;
        p.add_option("bind,b", cli::value(config.bind_endpoint), true);
        p.add_option("port,p", cli::value(config.port), true);
        p.add_option("sport,s", cli::value(config.ssl_port), true);
        p.add_option("config,c", cli::value(config.config_file), true);
        try {
            p.parse(argc, argv);
        }
        catch (const std::exception& ex) {
            std::cout << "[!] failed to parse the command line ! " << ex.what()
                      << '\n';
            return -1;
        }
    }

    try {
        ServerState server_state = load_server_state(config);
        ssl::mbedtls::context ssl_ctx{ssl::version::tls_server};
        ssl_ctx.set_password_callback(
            [](mutable_buffer password_buff,
               ssl::password_purpose) -> std::optional<std::size_t> {
                std::string_view passtext = "123456789";
                if (password_buff.size() < passtext.size()) {
                    return std::nullopt;
                }
                std::copy(passtext.begin(), passtext.end(),
                          password_buff.to_span<uint8_t>().data());
                return passtext.size();
            });
        ssl_ctx.set_verify_mode(ssl::verify_mode::none);
        ssl_ctx.use_own_certificate_file("creds/cert.pem",
                                         ssl::file_format::pem);
        ssl_ctx.use_private_key_file("creds/key.pem", ssl::file_format::pem);

        io_loop loop;
        spawn(loop, run_server(ssl_ctx, loop, server_state),
              [](std::exception_ptr eptr) {
                  if (eptr) {
                      try {
                          std::rethrow_exception(eptr);
                      }
                      catch (const std::exception& ex) {
                          std::cout
                              << "[!] The server has stopped: " << ex.what()
                              << "\n";
                      }
                  }
              });
        loop.run();
    }
    catch (const std::exception& ex) {
        std::cout << "[!] The server loop exited with exception: " << ex.what()
                  << "\n";
    }
    return 0;
}
