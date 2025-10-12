#include <rad/async/io_loop.h>
#include <rad/coro/spawn.h>
#include <rad/coro/task.h>
#include <rad/net/dns/ares_resolver.h>
#include <rad/net/dns/doh_resolver.h>
#include <rad/net/ssl/mbedtls_ctx.h>
#include <rad/net/ssl/openssl_ctx.h>
#include <rad/net/ssl/wolfssl_ctx.h>
#include <rad/net/tcp.h>

#include <chrono>
#include <iostream>

using namespace RAD_LIB_NAMESPACE;
namespace ssl = net::ssl;
using namespace std::string_view_literals;

namespace {
    constexpr auto test_hosts = std::array{
        "cppreference.com"sv,
        "www.google.com"sv,
        "stackoverflow.com"sv,
        "github.com"sv,
        "chat.deepseek.com"sv,
        "chatgpt.com"sv,
        "mail.google.com"sv,
        "gmail.com"sv,
        "splendidastoundinglushsong.neverssl.com"sv,
        "www.cloudflare.com"sv,
    };

    task<> sys_resolve_hosts(io_loop& loop) {
        net::tcp::resolver resolver{loop};
        for (auto host : test_hosts) {
            auto results =
                co_await resolver.async_resolve(host, "https", net::tcp::any());
            std::cout << "[*] " << host << " resolves to (sys) : [ ";
            for (const auto& address :
                 std::span(results).subspan(0, results.size() - 1)) {
                std::cout << address.to_string() << ", ";
            }
            std::cout << results.back().to_string() << " ]\n";
        }
    }

    task<> ares_resolve_hosts(io_loop& loop) {
        net::ares::resolver resolver{loop};
        for (auto host : test_hosts) {
            auto results =
                co_await resolver.async_resolve(host, "https", net::tcp::any());
            std::cout << "[*] " << host << " resolves to (ares) : [ ";
            for (const auto& address :
                 std::span(results).subspan(0, results.size() - 1)) {
                std::cout << address.to_string() << ", ";
            }
            std::cout << results.back().to_string() << " ]\n";
        }
    }

    task<> doh_resolve_hosts(io_loop& loop, ssl::context_base& ctx,
                             std::string_view name) {
        net::doh::resolver resolver(loop, ctx);
        for (auto host : test_hosts) {
            auto results =
                co_await resolver.async_resolve(host, "https", net::tcp::any());
            std::cout << "[*] " << host << " resolves to (doh[" << name
                      << "]) : [ ";
            for (const auto& address :
                 std::span(results).subspan(0, results.size() - 1)) {
                std::cout << address.to_string() << ", ";
            }
            std::cout << results.back().to_string() << " ]\n";
        }
    }

    task<> run_resolvers(
        io_loop& loop,
        std::span<std::pair<std::string, std::unique_ptr<ssl::context_base>>>
            ssl_contexts) {
        using namespace std::chrono;

        auto start = steady_clock::now();

        try {
            co_await sys_resolve_hosts(loop);
        }
        catch (const std::exception& ex) {
            throw std::runtime_error{std::string{"system resolver failed: "} +
                                     ex.what()};
        }

        auto elapsed = steady_clock::now() - start;

        std::cout << "[*] system resolver took "
                  << duration_cast<milliseconds>(elapsed).count() << " ms\n";

        start = steady_clock::now();

        try {
            co_await ares_resolve_hosts(loop);
        }
        catch (const std::exception& ex) {
            throw std::runtime_error{std::string{"ares resolver failed: "} +
                                     ex.what()};
        }

        elapsed = steady_clock::now() - start;
        std::cout << "[*] ares resolver took "
                  << duration_cast<milliseconds>(elapsed).count() << " ms\n";

        net::doh::config.enable_cache = false;
        for (auto& [name, ctx] : ssl_contexts) {
            start = steady_clock::now();
            try {
                co_await doh_resolve_hosts(loop, *ctx, name);
            }
            catch (const std::exception& ex) {
                std::string error_msg = "doh resolver (" + name + ") failed: ";
                throw std::runtime_error{error_msg + ex.what()};
            }
            elapsed = steady_clock::now() - start;
            std::cout << "[*] doh resolver (" << name << ") took "
                      << duration_cast<milliseconds>(elapsed).count()
                      << " ms\n";
        }
    }
} // namespace

namespace tests_fn {
    bool do_resolver_tests() {
        try {
            io_loop loop;

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
                ssl_contexts.emplace_back("mbedtls", std::move(ctx));
            }
#endif // RAD_HAVE_MBEDTLS

            std::string failure_reason;
            std::exception_ptr ex_ptr;
            spawn(loop, run_resolvers(loop, ssl_contexts),
                  [&ex_ptr](std::exception_ptr ep) { ex_ptr = ep; });
            loop.run();
            if (ex_ptr) {
                std::rethrow_exception(ex_ptr);
            }
        }
        catch (const std::exception& ex) {
            std::cout << "[!] resolver tests failed : " << ex.what() << "\n";
            return false;
        }

        std::cout << "[*] resolver tests passed\n";
        return true;
    }
} // namespace tests_fn