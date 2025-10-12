#include <rad/async/io_loop.h>
#include <rad/async/strand.h>
#include <rad/coro/spawn.h>
#include <rad/coro/when_all.h>
#include <rad/io/files.h>
#include <rad/json/json.h>
#include <rad/net/dns/dns_parser.h>
#include <rad/net/http2/http2_client.h>
#include <rad/net/dns/ares_resolver.h>
#include <rad/net/ssl/mbedtls_ctx.h>
#include <rad/net/ssl/openssl_ctx.h>
#include <rad/net/ssl/wolfssl_ctx.h>
#include <rad/unittest/unittest.h>
#include <rad/views/zip.h>

#include <format>
#include <iostream>

#include "http_tests_json_utils.h"

using namespace RAD_LIB_NAMESPACE;
using namespace unittest;
namespace ssl = net::ssl;
namespace http2 = net::http2;
namespace http = net::http;
using namespace std::string_view_literals;

namespace {
    constexpr std::string_view user_agent_value =
        "Mozilla/5.0 (Windows NT 10.0; Win64; x64)"
        " AppleWebKit/537.36 (KHTML, like Gecko) Chrome/96.0.4664.45 "
        "Safari/537.36";

    struct request_entry {
        http2::verb method = {};
        std::string url;
        http2::headers headers;
        json::object body_object;
        std::string body;
    };

    std::vector<request_entry> make_requests(size_t n, std::string_view host,
                                             std::string_view user_agent,
                                             std::default_random_engine& reng) {
        std::vector<request_entry> reqs;
        for (size_t i = 1; i <= n; ++i) {
            auto& req = reqs.emplace_back();
            req.method = http::verb::get;
            req.url =
                std::format("https://{}/"
                            "some-request-path-{}?client-{}=radclient-h2-{}",
                            host, i, i, i);
            req.headers.insert(http::field::user_agent, user_agent);
            req.headers.insert(http::field::content_type, "application/json");
            req.body_object = make_random_object(i, reng, 0);
            req.body = json::serialize(req.body_object);
            // req.body = std::format(R"#({{"param_{}": "value_{}",
            // "count_{}": 70{}, "type_{}": "json payload
            // ({})"}})#", i, i, i, i, i, i);
        }
        return reqs;
    }

    task<> test_http2_client(std::string ssl_name, net::ssl::context_base& ctx,
                             io_loop& loop, std::default_random_engine& reng) {
        // std::string host = "www.google.com";
        std::string host = "echo.free.beeceptor.com";
        constexpr size_t streams_count = 400;

        http2::client client{loop, ctx};
        const auto reqs =
            make_requests(streams_count, host,
                          "Mozilla/5.0 (Windows NT 10.0; Win64; x64) "
                          "AppleWebKit/537.36 (KHTML, like Gecko)"
                          "Chrome/96.0.4664.45 Safari/537.36",
                          reng);

        co_await client.connect(host, 443, true);
        auto on_exit = scope_exit([&] { client.stop(); });

        std::vector<http::response> resps;
        // reserve for resps as many requests to ensure no
        // reallocations!
        resps.reserve(reqs.size());
        std::vector<task<>> reqs_tasks1;
        std::vector<task<>> reqs_tasks2;
        reqs_tasks1.reserve(reqs.size());
        reqs_tasks2.reserve(reqs.size());

        assert(reqs.size() % 2 == 0);
        const auto reqs1 = std::span{reqs}.subspan(0, reqs.size() / 2);
        const auto reqs2 = std::span{reqs}.subspan(reqs.size() / 2);

        for (const auto& req : reqs1) {
            // resps must not reallocate, or the previous references
            // will be invalid!
            auto& res = resps.emplace_back();
            reqs_tasks1.emplace_back(client.request(
                req.url, req.method, req.headers, buffer(req.body), res,
                dynamic_buffer(res.body)));
        }

        for (const auto& req : reqs2) {
            // resps must not reallocate, or the previous references
            // will be invalid!
            auto& res = resps.emplace_back();
            reqs_tasks2.emplace_back(client.request(
                req.url, req.method, req.headers, buffer(req.body), res,
                dynamic_buffer(res.body)));
        }

        using namespace std::chrono;
        auto start = steady_clock::now();

        // co_await client.ping();
        co_await when_all(std::move(reqs_tasks1));
        // co_await client.ping();
        co_await when_all(std::move(reqs_tasks2));

        auto end = steady_clock::now();

        for (size_t i = 0; i < reqs.size(); ++i) {
            validate_response_body(resps[i].body, reqs[i].body_object,
                                   reqs[i].headers);
        }

        auto elapsed = duration_cast<milliseconds>(end - start);
        std::cout << "[*] http2 tests (" << ssl_name
                  << ") finished requests after " << elapsed.count()
                  << "ms, sent: " << client.total_sent_bytes()
                  << " bytes, received: " << client.total_received_bytes()
                  << " bytes\n";
    }

    [[maybe_unused]] task<>
    get_multiple_pages(std::string ssl_name, net::ssl::context_base& ctx,
                       io_loop& loop, std::string_view host,
                       std::span<const std::string_view> paths) {
        http2::client client{loop, ctx};
        net::ares::resolver rsv{loop};
        auto results = co_await rsv.async_resolve(host, 443, net::tcp::ipv4());
        co_await client.connect(host, 443, true);
        std::vector<request_entry> reqs;
        reqs.reserve(paths.size());
        for (std::string_view path : paths) {
            reqs.push_back(request_entry{
                .method = http2::verb::get,
                .url = "https://" + host + path,
                .headers = {{http::field::user_agent, user_agent_value}},
            });
        }
        std::vector<http2::response> resps;
        resps.resize(paths.size());

        std::vector<task<>> reqs_tasks;
        for (auto [req, res] : zip(reqs, resps)) {
            reqs_tasks.emplace_back(client.request(
                req.url, req.method, req.headers, buffer(req.body), res,
                dynamic_buffer(res.body)));
        }

        co_await when_all(std::move(reqs_tasks));

        size_t res_i = 0;
        for (const auto& res : resps) {
            std::string u8str;
            std::string_view res_body = res.body;
            auto it = res.headers.find(http::field::content_type);
            if (it != res.headers.end()) {
                size_t idx = ifind(it->second, "charset=");
                if (idx != std::string_view::npos &&
                    idx + 8 < it->second.size()) {
                    auto encoding = subview(it->second, idx + 8);
                    while (!encoding.empty() && encoding.front() == ' ') {
                        encoding.remove_prefix(1);
                    }
                    while (!encoding.empty() && encoding.back() == ' ') {
                        encoding.remove_suffix(1);
                    }
                    if (encoding == "windows-1256") {
                        cp1256_to_utf8(res.body, u8str);
                        res_body = u8str;
                    }
                }
            }
            using namespace io::files;
            std::string file_name =
                ssl_name + "_res_" + std::to_string(res_i + 1) + ".html";
            file f{file_name, create_mode::overwrite};
            f.write(buffer(res_body));
            res_i += 1;
        }
    }

    /*
    void process_dns_answer(std::string domain, std::string_view ssl_name,
            std::vector<net::endpoint>& results, const_buffer buff, bool
    want_ipv4) { using namespace net::dns; assert(!buff.empty());

            auto whole_message = buff;

            auto header = buff.data_as<const dns_header_raw>();
            if (header->answers_count() == 0 && !want_ipv4) {
                    return;
            }

            if (header->response_code() != response_code::no_error ||
    header->questions_count() != 1 || header->answers_count() == 0 ||
    !header->response() || header->id() != 0) { printf("[!] received invalid
    dns response with no answers for domain '%s'!\n", domain.c_str());
    return;
            }
            // can't tolerate truncation on tcp
            if (header->truncation()) {
                    printf("[!] received truncated dns response for domain
    '%s'!\n", domain.c_str()); return;
            }

            buff += sizeof(dns_header);

            auto question_len = process_question(buff, whole_message,
    want_ipv4); if (!question_len) { printf("[!] failed to process question
    for domain '%s'!\n", domain.c_str()); return;
            }
            buff += question_len;

            process_record_params params;
            params.hostname = domain;
            params.whole_message = whole_message;
            params.want_ipv4 = want_ipv4;
            params.port = 0;
            params.results = &results;

            for (int i = 0; i < header->answers_count(); ++i) {
                    size_t len = process_answer_record(buff, params);
                    if (!len) {
                            printf("[!] failed to process answer record (%d)
    for domain '%s'!\n", i, domain.c_str()); return;
                    }
                    buff += len;
            }
    }
    */

    task<> bulk_doh_resolver(std::string ssl_name, net::ssl::context_base& ctx,
                             io_loop& loop, net::ipv4_endpoint server_address,
                             std::string_view host, std::string_view path,
                             std::span<const std::string_view> domains) {
        using namespace std::chrono;

        http2::client client{loop, ctx};
        co_await client.connect(host, server_address, true);
        std::vector<request_entry> reqs;
        reqs.reserve(domains.size() * 2);
        for (std::string_view domain : domains) {
            std::string dns_query;
            std::error_code ec;
            net::dns::make_dns_query(dynamic_buffer(dns_query), domain, true, 0,
                                     ec);
            assert_true(
                !ec,
                ("failed to make ipv4 dns query for '" + domain + "'").c_str());
            size_t dns_query_size = dns_query.size();
            reqs.push_back(request_entry{
                .method = http2::verb::post,
                .url = "https://" + host + path,
                .headers = {{http::field::accept, "application/dns-message"},
                            {http::field::content_type,
                             "application/dns-message"},
                            {http::field::content_length,
                             std::to_string(dns_query_size)}},
                .body = std::move(dns_query),
            });
            net::dns::make_dns_query(dynamic_buffer(dns_query), domain, false,
                                     0, ec);
            assert_true(
                !ec,
                ("failed to make ipv6 dns query for '" + domain + "'").c_str());
            dns_query_size = dns_query.size();
            reqs.push_back(request_entry{
                .method = http2::verb::post,
                .url = "https://" + host + path,
                .headers = {{http::field::accept, "application/dns-message"},
                            {http::field::content_type,
                             "application/dns-message"},
                            {http::field::content_length,
                             std::to_string(dns_query_size)}},
                .body = std::move(dns_query)});
        }

        std::vector<http2::response> resps;
        resps.resize(reqs.size());

        std::vector<task<>> reqs_tasks;
        for (auto [req, res] : zip(reqs, resps)) {
            reqs_tasks.emplace_back(client.request(
                req.url, req.method, req.headers, buffer(req.body), res,
                dynamic_buffer(res.body)));
        }

        auto start = steady_clock::now();
        co_await when_all(std::move(reqs_tasks));
        auto end = steady_clock::now();

        size_t domain_i = 0;
        for (size_t i = 0; i < resps.size(); i += 2) {
            std::vector<net::endpoint> results;
            std::string_view domain = domains[domain_i];

            assert_eq(resps[i].status_code(), http::response_status::ok,
                      ("failed to resolve '" + domain + "'").c_str());
            assert_eq(resps[i + 1].status_code(), http::response_status::ok,
                      ("failed to resolve '" + domain + "'").c_str());

            std::error_code ec;
            {
                net::dns::dns_ip_answers_handler parse_handler{domain, results,
                                                               443};
                net::dns::parse_dns_message(buffer(resps[i].body),
                                            parse_handler, ec);
            }
            assert_true(
                !ec, ("failed to parse ipv4 dns message for '" + domain + "'")
                         .c_str());
            {
                net::dns::dns_ip_answers_handler parse_handler{domain, results,
                                                               443};
                net::dns::parse_dns_message(buffer(resps[i + 1].body),
                                            parse_handler, ec);
            }
            assert_true(
                !ec, ("failed to parse ipv6 dns message for '" + domain + "'")
                         .c_str());
            assert_false(results.empty(),
                         ("didn't get any ip for '" + domain + "'").c_str());
            domain_i += 1;
            std::cout << "[*] " << domain << " resolves to (doh[" << ssl_name
                      << "]) : [ ";
            for (const auto& address :
                 std::span(results).subspan(0, results.size() - 1)) {
                std::cout << address.to_string() << ", ";
            }
            std::cout << results.back().to_string() << " ]\n";
        }

        auto elapsed = duration_cast<milliseconds>(end - start);
        std::cout << "[*] http2 bulk DOH resolver (" << ssl_name
                  << ") finished requests after " << elapsed.count()
                  << "ms, sent: " << client.total_sent_bytes()
                  << " bytes, received: " << client.total_received_bytes()
                  << " bytes\n";
    }
} // namespace

namespace tests_fn {
    bool do_http2_tests() {
        std::random_device rd;
        std::default_random_engine reng{rd()};

        try {
            std::vector<
                std::pair<std::string, std::unique_ptr<ssl::context_base>>>
                ssl_contexts;
#ifdef RAD_HAVE_OPENSSL
            {
                auto ctx = std::make_unique<ssl::openssl::context>(
                    ssl::version::tls_client);
                ctx->set_default_verify_paths();
                ssl_contexts.emplace_back("openssl", std::move(ctx));
            }
#endif // RAD_HAVE_OPENSSL
#ifdef RAD_HAVE_WOLFSSL
            {
                auto ctx = std::make_unique<ssl::wolfssl::context>(
                    ssl::version::tlsv12_client);
                ctx->set_default_verify_paths();
                ssl_contexts.emplace_back("wolfssl", std::move(ctx));
            }
#endif // RAD_HAVE_WOLFSSL
#ifdef RAD_HAVE_MBEDTLS
            {
                auto ctx = std::make_unique<ssl::mbedtls::context>(
                    ssl::version::tls_client);
                ctx->set_default_verify_paths();
                ctx->set_verify_mode_callback(
                    ssl::verify_mode::required,
                    [](bool pre_ok, void* cert) { return true; });
                ssl_contexts.emplace_back("mbedtls", std::move(ctx));
            }
#endif // RAD_HAVE_MBEDTLS

            assert_false(ssl_contexts.empty(),
                         "http2 tests can't run with all ssl "
                         "backends disabled");

            io_loop loop;

            // const std::string_view httpwg_host = "codeload.github.com";
            // const std::string_view httpwg_host = "github.com";
            // const std::string_view httpwg_host = "httpwg.org";
            // const std::string_view httpwg_host = "httpbin.org";
            //  const std::string_view httpwg_host = "www.reddit.com";
            //  const std::string_view httpwg_host = "www.google.com";

            // const std::array<std::string_view, 1> httpwg_paths{
            //"/status/302",
            //"/r/cpp/new",
            //"/webhp?hl=ar&sa=X&ved=0ahUKEwj2jdrA1_GPAxUlSaQEHehDB20QPAgI&zx=1758727143087&no_sw_cr=1",
            //"/microsoft/vcpkg/zip/refs/heads/master",
            //"/Kitware/CMake/releases/download/v4.1.2/"
            //"cmake-4.1.2-linux-x86_64.sh",
            //"/specs/rfc9112.html",
            //"/specs/rfc9113.html",
            //"/specs/rfc9114.html",
            //};

            const std::array<std::string_view, 20> domains{
                "chat.deepseek.com"sv,
                "cppreference.com"sv,
                "www.google.com"sv,
                "stackoverflow.com"sv,
                "github.com"sv,
                "chatgpt.com"sv,
                "mail.google.com"sv,
                "gmail.com"sv,
                "splendidastoundinglushsong.neverssl.com"sv,
                "www.cloudflare.com",
                "www.reddit.com",
                "www.rust-lang.org",
                "wikipedia.org",
                "visualstudio.microsoft.com",
                "isocpp.org",
                "www.w3schools.com",
                "www.python.org",
                "go.dev",
                "learn.microsoft.com",
                "www.qt.io"};

            std::array<std::exception_ptr, 3> ex_ptrs;
            for (size_t i = 0; i < ssl_contexts.size(); ++i) {
                const auto& [name, ctx] = ssl_contexts[i];
                spawn(loop, test_http2_client(name, *ctx, loop, reng),
                      [i, &ex_ptrs](std::exception_ptr e) { ex_ptrs[i] = e; });
                /*
                spawn(loop,
                      get_multiple_pages(name, *ctx, loop, httpwg_host,
                                         httpwg_paths),
                      [i, &ex_ptrs](std::exception_ptr e) { ex_ptrs[i] = e; });
                */
                spawn(loop,
                      bulk_doh_resolver(name, *ctx, loop,
                                        net::ipv4_endpoint{{1, 1, 1, 1}, 443},
                                        "cloudflare-dns.com", "/dns-query",
                                        domains),
                      [i, &ex_ptrs](std::exception_ptr e) { ex_ptrs[i] = e; });
            }

            loop.run();

            for (const auto& ex_ptr : ex_ptrs) {
                if (ex_ptr) {
                    std::rethrow_exception(ex_ptr);
                }
            }
        }
        catch (const exception& ex) {
            std::cout << "[!] http2 tests failed ! " << ex.what() << "\n";
            return false;
        }
        catch (const std::exception& ex) {
            std::cout << "[!] http2 tests failed ! " << ex.what() << "\n";
            return false;
        }
        std::cout << "[*] http2 tests passed\n";
        return true;
    }
} // namespace tests_fn