#include <rad/coro/execute_timeout.h>
#include <rad/io/async_read_until.h>
#include <rad/net/http/http_client.h>
#include <rad/net/http/http_parser.h>

using namespace rad;
using namespace net;
using namespace http;

task<std::size_t> http_client::read_some() {
    assert(!rbuf_.full());
    auto read_fn = [&]() -> task<size_t> {
        if (is_ssl_) {
            co_return co_await stream_.async_read_some(rbuf_.available_space());
        }
        else {
            co_return co_await stream_.next_layer().async_read_some(
                rbuf_.available_space());
        }
    };
    std::size_t n = co_await execute_timeout(
        resolver_.timer_exutor(), timeout_secs_, [this] { cancel(); }, read_fn);
    last_transfer_time_ = std::chrono::steady_clock::now();
    rbuf_.commit(n);
    co_return n;
}

task<std::size_t> http_client::read_some(std::error_code& ec) {
    assert(!rbuf_.full());
    auto read_fn = [&]() -> task<size_t> {
        if (is_ssl_) {
            co_return co_await stream_.async_read_some(rbuf_.available_space(),
                                                       ec);
        }
        else {
            co_return co_await stream_.next_layer().async_read_some(
                rbuf_.available_space(), ec);
        }
    };
    std::size_t n = co_await execute_timeout(
        resolver_.timer_exutor(), timeout_secs_, [this] { cancel(); }, read_fn);
    last_transfer_time_ = std::chrono::steady_clock::now();
    rbuf_.commit(n);
    co_return n;
}

task<> http_client::read_all(mutable_buffer buff) {
    constexpr std::size_t max_chunk = 16 * 1024;
    auto read_fn = [&](mutable_buffer read_buff) -> task<size_t> {
        if (is_ssl_) {
            co_return co_await stream_.async_read_some(read_buff);
        }
        else {
            co_return co_await stream_.next_layer().async_read_some(read_buff);
        }
    };
    while (!buff.empty()) {
        auto to_read = std::min(buff.size(), max_chunk);
        auto read_buff = buff.sub_buffer(0, to_read);
        buff +=
            co_await execute_timeout(timer_executor(), timeout_secs_,
                                     read_fn(read_buff), [this] { cancel(); });
    }
}

task<std::pair<bool, url>>
http_client::connect_to_host(std::string_view url_string) {
    using namespace std::string_view_literals;

    url target_url{url_string};
    bool is_ssl = false;
    if (target_url.scheme() == "https"sv) {
        is_ssl = true;
    }
    else if (target_url.scheme() == "http"sv) {
        is_ssl = false;
    }
    else {
        throw std::system_error{make_error(error::bad_scheme)};
    }

    if (is_ssl == is_ssl_ && !connected_host_.empty() &&
        connected_host_ == target_url.host_view() && is_still_connected()) {
        co_return std::pair{true, std::move(target_url)};
    }

    is_ssl_ = false;
    connected_host_.clear();
    server_keepalive_ = {};
    last_transfer_time_ = {};
    stream_.next_layer().close();
    if (target_url.is_host_domain()) {
        std::vector<endpoint> results;
        std::error_code ec;
        if (use_doh_resolver_) {
            results = co_await resolver_.async_resolve(
                target_url.host_view(), target_url.port(), tcp::ipv4(), ec);
        }
        if (ec || !use_doh_resolver_) {
            // DOH servers may be blocked so try the system resolver
            tcp::resolver sys_resolver{executor()};
            results = co_await sys_resolver.async_resolve(
                target_url.host_view(), target_url.port(), tcp::ipv4());
        }
        co_await stream_.next_layer().async_connect(results);
    }
    else {
        co_await stream_.next_layer().async_connect(target_url.make_endpoint());
    }

    if (is_ssl) {
        stream_.reopen();
    }
    stream_.next_layer().set_option(socket_options::tcp_nodelay(true));
    is_ssl_ = is_ssl;
    connected_host_ = target_url.host_view();
    co_return std::pair{false, std::move(target_url)};
}

task<> http_client::send_request(const url& req_url, verb v,
                                 const headers& hdrs, const_buffer body,
                                 std::error_code& ec) {
    if (is_ssl_ && use_exceptions(ec)) {
        stream_.set_hostname(req_url.host_view());
        co_await execute_timeout(
            timer_executor(), timeout_secs_, [this] { cancel(); },
            [this]() -> task<> {
                co_await stream_.async_handshake(ssl::handshake_type::client);
            });
    }

    {
        std::string size_buff;
        std::string target;
        request_view req;
        req.method = v;
        std::error_code target_ec;
        make_request_target(req_url, v, false, target, target_ec);
        if (target_ec) {
            throw std::system_error{ec};
        }
        req.target = target;
        req.version = version::v1_1;

        req.headers.append(hdrs);
        if (!req.headers.contains(field::host)) {
            req.headers.insert(field::host, req_url.host_view());
        }
        if (!req.headers.contains(field::connection)) {
            req.headers.insert(field::connection, "keep-alive");
        }
        if (!req.headers.contains(field::content_length)) {
            size_buff = std::to_string(body.size());
            req.headers.insert(field::content_length, size_buff);
        }
        // this invalidates rbuf_ so it must be reset
        buffer_storage_.clear();
        req.serialize(dynamic_buffer(buffer_storage_), false);
    }

    auto write_buffs =
        std::array<const_buffer, 2>{buffer(buffer_storage_), body};
    if (expects_100_continue_) {
        // don't send the body until 100 Continue is received
        write_buffs.back() = {};
    }

    if (is_ssl_) {
        co_await stream_.async_write(write_buffs, ec);
    }
    else {
        co_await stream_.next_layer().async_write(write_buffs, ec);
    }

    last_transfer_time_ = std::chrono::steady_clock::now();
}

task<> http_client::read_http_response(response& res,
                                       const_buffer pending_body) {
    buffer_storage_.resize(read_buffer_size);
    rbuf_ = ring_consumer_producer{buffer(buffer_storage_)};
    // may receive 1xx informational responses
    bool received_100_continue = false;
    while (1) {
        response_incremental_parser res_parser{res};
        while (res_parser.need_more()) {
            if (canceled_) {
                throw std::system_error{
                    std::make_error_code(std::errc::operation_canceled)};
            }
            const std::size_t n = co_await read_some();
            assert(n > 0);
            std::ignore = n;
            std::error_code ec;
            res_parser.parse(rbuf_, ec);
            if (ec) {
                assert(res_parser.has_error());
                throw std::system_error(ec);
            }
        }
        assert(res_parser.done());

        // extract Keep-Alive header
        get_keep_alive_settings(res.headers);

        // check if 100 Continue was expected
        if (expects_100_continue_ && !received_100_continue) {
            if (res.status_code() != response_status::continue_) {
                throw std::system_error(make_error(error::bad_response_code));
            }
            received_100_continue = true;
            // send the body now
            if (!pending_body.empty()) {
                if (is_ssl_) {
                    co_await stream_.async_write(pending_body);
                }
                else {
                    co_await stream_.next_layer().async_write(pending_body);
                }
            }
            res.clear();
            continue;
        }
        if (res.status_code() == response_status::switching_protocols) {
            // 101 switching protocols is not supported even if the
            // client requested it!
            throw std::system_error(make_error(error::bad_response_code));
        }
        if (res.is_informational()) {
            // read the next response
            res.clear();
            continue;
        }
        // received the final non informational response
        break;
    }
}

task<> http_client::request(std::string_view url, verb v, headers hdrs,
                            const_buffer body, response& res,
                            dynamic_buffer res_buff) {
    canceled_ = false;
    std::string url_str{url};
    uint8_t redirections = 0;
    const std::size_t res_buff_init_size = res_buff.size();
    while (1) {
        if (canceled_) {
            throw std::system_error{
                std::make_error_code(std::errc::operation_canceled)};
        }
        auto redirect_result =
            co_await try_request(url_str, res, v, hdrs, body, res_buff);
        if (!follow_redirections_ || !redirect_result.has_value() ||
            redirect_result->url.empty()) {
            break;
        }
        ++redirections;
        if (redirections > max_redirections()) {
            throw std::system_error{make_error(error::too_many_redirections)};
        }
        v = redirect_result->method;
        net::url base_url{url_str};
        net::url redirect_url{base_url, redirect_result->url};
        url_str = redirect_url.href();
        // clear any unwanted response from previous request
        res_buff.resize(res_buff_init_size);
        res.clear(false);
    }
}

auto http_client::try_request(std::string_view url, response& res, verb v,
                              const headers& hdrs, const_buffer body,
                              dynamic_buffer res_buff)
    -> task<std::optional<redirect_location>> {
    using namespace std::string_view_literals;

    expects_100_continue_ = false;
    if (auto it = hdrs.find(field::expect); it != hdrs.end()) {
        expects_100_continue_ = it->second == "100-continue";
    }

    {
        auto [was_connected, target_url] = co_await connect_to_host(url);
        std::error_code ec;
        std::error_code& ec_ref = was_connected ? ec : no_ec;
        // try to reuse the connection if one was open
        // this is not guaranteed to work because the server may
        // have closed the connection any way!
        co_await send_request(target_url, v, hdrs, body, ec_ref);
        if (ec) {
            assert(was_connected);
            server_keepalive_ = {};
            last_transfer_time_ = {};
            connected_host_.clear();
            std::tie(was_connected, target_url) = co_await connect_to_host(url);
            assert(!was_connected);
            co_await send_request(target_url, v, hdrs, body, no_ec);
        }
    }

    // read the response headers and the start of the body if possible
    // also send the body if it wasn't after receiving 100 Continue
    co_await read_http_response(res,
                                expects_100_continue_ ? body : buffer(nullptr));
    // the final response can't be informational!
    assert(!res.is_informational());
    // HEAD responses have no body
    if (v == verb::head || !res.may_have_body()) {
        if (!rbuf_.empty() || res.headers.contains(field::transfer_encoding)) {
            throw std::system_error(make_error(error::unexpected_body));
        }
        co_return std::nullopt;
    }

    // check if only ok response is desired
    if (require_ok_response_ && !res.is_ok()) {
        throw std::system_error(make_error(error::bad_response_code));
    }

    // get body length and read strategy
    auto body_size_type = determine_message_body_length(
        v, res.status_code(), res.version, res.get_content_length(),
        res.get_transfer_encoding());

    if (std::uint64_t* len = std::get_if<std::uint64_t>(&body_size_type)) {
        // known body size
        if (*len < rbuf_.size()) {
            // received more data than the body!
            throw std::system_error(make_error(error::unexpected_body));
        }
        else if (*len > max_body_size_) {
            // too large body!
            throw std::system_error(make_error(error::too_large_message));
        }
        *len -= rbuf_.write_to(res_buff);
        if (*len > 0) {
            co_await read_with_content_length(static_cast<std::size_t>(*len),
                                              res_buff);
        }
    }
    else if (std::holds_alternative<body_until_eof>(body_size_type)) {
        // unknown body size. read until eof is reached.
        // can't use keep-alive value!
        server_keepalive_ = {};
        connected_host_.clear();
        if (rbuf_.size() > max_body_size_) {
            // too large body!
            throw std::system_error(make_error(error::too_large_message));
        }
        const std::size_t consumed_size = rbuf_.write_to(res_buff);
        co_await read_until_eof(consumed_size, res_buff);
    }
    else if (std::holds_alternative<chunked_body>(body_size_type)) {
        // chunk encoded body
        co_await read_chunked(res_buff);
    }
    else {
        // bad message body
        assert(std::holds_alternative<bad_message_body>(body_size_type));
        throw std::system_error(make_error(error::partial_message));
    }

    if (!rbuf_.empty()) {
        // treat additional data after the body as an error!
        throw std::system_error(make_error(error::unexpected_body));
    }

    if (!is_still_connected()) {
        // close the tcp connection if no server keep alive
        // don't close the ssl session to be able to reuse it later!
        stream_.next_layer().close();
    }

    // if there is redirection return the new location url
    // must checked after body because 3XX responses may have a body!
    if (res.is_redirect()) {
        auto it = res.headers.find(field::location);
        if (it == res.headers.end()) {
            co_return std::nullopt;
        }
        co_return redirect_location{
            change_method_on_redirect(v, res.status_code()),
            std::move(it->second)};
    }

    co_return std::nullopt;
}

task<> http_client::read_with_content_length(std::size_t content_length,
                                             dynamic_buffer output) {
    assert(content_length != 0);
    // existing data in the read buffer was inserted
    if (content_length == 0) {
        co_return;
    }

    constexpr std::size_t max_read_step = 16 * 1024;
    while (content_length != 0) {
        if (canceled_) {
            throw std::system_error{
                std::make_error_code(std::errc::operation_canceled)};
        }
        const std::size_t to_read = std::min(content_length, max_read_step);
        auto read_buff = output.prepare(to_read);
        co_await read_all(read_buff);
        content_length -= to_read;
    }
}

task<> http_client::read_until_eof(std::size_t consumed_size,
                                   dynamic_buffer output) {
    // existing data in the read buffer was inserted
    std::error_code ec;
    while (1) {
        if (canceled_) {
            throw std::system_error{
                std::make_error_code(std::errc::operation_canceled)};
        }
        const std::size_t n = co_await read_some(ec);
        if (ec) {
            break;
        }
        assert(n > 0);
        consumed_size += n;
        if (consumed_size > max_body_size_) {
            // too large body!
            throw std::system_error(make_error(error::too_large_message));
        }
        rbuf_.write_to(output);
    }

    const auto eof_ec = io::detail::make_eof_error_code();
    if (ec.value() != eof_ec.value() || ec.category() != eof_ec.category()) {
        throw std::system_error(ec);
    }
}

task<> http_client::read_chunked(dynamic_buffer output) {
    std::size_t consumed_size = 0;
    chunks_incremental_parser chunks_parser;
    // parse the ready body if any
    if (!rbuf_.empty()) {
        std::error_code ec;
        const std::size_t old_size = output.size();
        chunks_parser.parse(rbuf_, output, nullptr, nullptr, ec);
        consumed_size += output.size() - old_size;
        if (ec) {
            assert(chunks_parser.has_error());
            throw std::system_error{ec};
        }
        if (consumed_size > max_body_size_) {
            // too large body!
            throw std::system_error(make_error(error::too_large_message));
        }
    }
    while (chunks_parser.need_more()) {
        if (canceled_) {
            throw std::system_error{
                std::make_error_code(std::errc::operation_canceled)};
        }
        const std::size_t n = co_await read_some();
        assert(n > 0);
        std::ignore = n;
        std::error_code ec;
        const std::size_t old_size = output.size();
        chunks_parser.parse(rbuf_, output, nullptr, nullptr, ec);
        consumed_size += output.size() - old_size;
        if (ec) {
            assert(chunks_parser.has_error());
            throw std::system_error(ec);
        }
        if (consumed_size > max_body_size_) {
            // too large body!
            throw std::system_error(make_error(error::too_large_message));
        }
    }
    assert(chunks_parser.done());
    if (!chunks_parser.done()) {
        throw std::system_error(make_error(error::partial_message));
    }
}

bool http_client::get_keep_alive_settings(const headers& hdrs) noexcept {
    auto it = hdrs.find(field::keep_alive);
    if (it == hdrs.end()) {
        return false;
    }
    for (auto param : it->second | split(",")) {
        constexpr std::string_view timeout_param = "timeout=";
        while (!param.empty() && param.front() == ' ') {
            param.remove_prefix(1);
        }
        if (param.starts_with(timeout_param)) {
            std::string_view timeout_value = param.substr(timeout_param.size());
            // remove white space
            while (!timeout_value.empty() && timeout_value.front() == ' ') {
                timeout_value.remove_prefix(1);
            }
            while (!timeout_value.empty() && timeout_value.back() == ' ') {
                timeout_value.remove_suffix(1);
            }
            // parse the timeout in seconds
            server_keepalive_ = {};
            std::error_code ec;
            int new_server_keep_alive_secs = to_int(timeout_value, 10, ec);
            if (!ec && new_server_keep_alive_secs > 0) {
                server_keepalive_ =
                    std::chrono::seconds{new_server_keep_alive_secs};
                return true;
            }
            return false;
        }
    }
    return false;
}

bool http_client::is_still_connected() const noexcept {
    using namespace std::chrono;
    if (!stream_.is_open()) {
        return false;
    }
    return server_keepalive_.count() > 0 &&
           last_transfer_time_.time_since_epoch().count() > 0 &&
           (server_keepalive_ - (steady_clock::now() - last_transfer_time_)) >
               1s;
}