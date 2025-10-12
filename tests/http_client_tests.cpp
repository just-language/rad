#include <rad/async/io_loop.h>
#include <rad/coro/spawn.h>
#include <rad/json/json.h>
#include <rad/net/http/http_client.h>
#include <rad/net/ssl/mbedtls_ctx.h>
#include <rad/net/ssl/openssl_ctx.h>
#include <rad/net/ssl/wolfssl_ctx.h>
#include <rad/unittest/unittest.h>

#include <format>
#include <fstream>
#include <iostream>
#include <random>

#include "http_tests_json_utils.h"

using namespace RAD_LIB_NAMESPACE;
using namespace unittest;
using namespace std::string_view_literals;

#define SSL_CTX_NAMESPACE wolfssl

namespace {
    using ssl_context_type = net::ssl::SSL_CTX_NAMESPACE::context;
    constexpr std::string_view test_user_agent1 =
        "Mozilla/5.0 (Windows NT 10.0; Win64; x64)"
        " AppleWebKit/537.36 (KHTML, like Gecko) Chrome/96.0.4664.45 "
        "Safari/537.36";
    constexpr std::string_view test_user_agent2 = "Yet! A new user agent";

    task<> run_echo_client(io_loop& loop, net::ssl::context_base& ssl_ctx,
                           std::size_t n, std::default_random_engine& reng) {
        namespace http = net::http;

        std::string_view url_start = "http://echo.free.beeceptor.com";
        http::http_client client(loop, ssl_ctx);

        for (auto i : range(n)) {
            auto body_object = make_random_object(i, reng, 0);
            auto body_text = json::serialize(body_object);
            http::headers headers;
            std::string_view user_agent = test_user_agent1;
            if ((i ^ reng()) % 2 == 0) {
                user_agent = test_user_agent2;
            }
            headers.insert(http::field::user_agent, user_agent);
            headers.insert(http::field::content_type, "application/json");
            if ((i ^ reng()) % 2 == 0) {
                headers.insert(http::field::expect, "100-continue");
            }
            std::string path_query;
            if ((i ^ reng()) % 4 == 0) {
                path_query = "/";
            }
            else if ((i ^ reng()) % 3 == 0) {
                path_query = "/" + make_random_string(i ^ reng(), reng);
            }
            else if ((i ^ reng()) % 2 == 0) {
                path_query = "/" + make_random_string(i ^ reng(), reng);
                path_query += "?" + make_random_string(i ^ reng(), reng);
            }
            else {
                path_query = "/?" + make_random_string(i ^ reng(), reng);
            }

            std::uniform_int_distribution<int> method_dis{
                static_cast<int>(http::verb::get),
                static_cast<int>(http::verb::put)};
            auto method = static_cast<http::verb>(method_dis(reng));

            auto res = co_await client.request(url_start + path_query, method,
                                               headers, buffer(body_text));

            validate_response_body(res.body, body_object, headers);
            /*
            std::cout << " ============= request (" << i + 1
                      << ") ===========\n"
                      << res.body << "\n";
            */
        }
    }

    task<> test_http_download(io_loop& loop, ssl_context_type& ssl_ctx) {
        namespace http = net::http;
        auto urls = std::array{
            "https://github.com/"sv,
            "https://superuser.com/questions/367780/how-to-connect-to-a-website-that-has-only-ipv6-addresses-without-a-domain-name"sv,
            "https://www.facebook.com/"sv,
            "https://www.youtube.com/"sv,
            "https://www.wolfssl.com/documentation/manuals/wolfssl/index.html"sv,
            "http://splendidastoundinglushsong.neverssl.com/online"sv,
            "http://splendidastoundinglushsong.neverssl.com/online"sv,
        };

        int page_n = 0;
        std::vector<uint8_t> buff;
        http::headers hdrs;
        hdrs.insert(http::field::user_agent,
                    "Mozilla/5.0 (Windows NT 10.0; Win64; x64) "
                    "AppleWebKit/537.36 "
                    "(KHTML, like Gecko) Chrome/96.0.4664.45 Safari/537.36");
        hdrs.insert(http::field::accept,
                    "text/html,application/xhtml+xml,application/"
                    "xml;q=0.9,image/"
                    "avif,image/webp,image/apng,*/*;q=0.8,application/"
                    "signed-exchange;v=b3;q=0.9");
        hdrs.insert(http::field::accept_language, "en");

        http::http_client client(loop, ssl_ctx);
        for (auto url : urls) {
            buff.clear();
            std::string page_name =
                "page-" + std::to_string(++page_n) + ".html";
            try {
                co_await client.request(url, http::verb::get, hdrs,
                                        buffer(nullptr), dynamic_buffer(buff));
                std::cout << "[*] http client downloaded url '" << url << "' ("
                          << buff.size() << " bytes)\n";
                std::ofstream(page_name).write(
                    reinterpret_cast<const char*>(buff.data()), buff.size());
            }
            catch (const std::system_error& ex) {
                std::cout << "[!] failed to download '" << url << "' ! ("
                          << ex.code() << ") " << ex.what() << "\n";
                throw;
            }
            catch (const std::exception& ex) {
                std::cout << "[!] failed to download '" << url << "' ! "
                          << ex.what() << "\n";
                throw;
            }
        }
    }
} // namespace

namespace tests_fn {
    bool do_http_tests() {
        std::random_device rd;
        std::default_random_engine reng{rd()};

        try {
            io_loop loop;
            ssl_context_type ctx{net::ssl::version::tlsv12_client};
            ctx.set_verify_mode(net::ssl::verify_mode::none);
            std::exception_ptr ex_ptr1;
            std::exception_ptr ex_ptr2;
            spawn(loop, test_http_download(loop, ctx),
                  [&](std::exception_ptr eptr) { ex_ptr1 = eptr; });
            spawn(loop, run_echo_client(loop, ctx, 2, reng),
                  [&](std::exception_ptr eptr) { ex_ptr2 = eptr; });
            loop.run();
            if (ex_ptr1) {
                std::rethrow_exception(ex_ptr1);
            }
            if (ex_ptr2) {
                std::rethrow_exception(ex_ptr2);
            }
        }
        catch (const std::system_error& ex) {
            std::cout << "[!] http client tests failed : " << ex.what() << "\n";
            return false;
        }
        catch (const std::exception& ex) {
            std::cout << "[!] http client tests failed : " << ex.what() << "\n";
            return false;
        }

        std::cout << "[*] http client tests passed\n";
        return true;
    }
} // namespace tests_fn