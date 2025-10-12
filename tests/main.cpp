#include <cassert>
#include <cstdint>
#include <iostream>
#include <rad/sysinfo.h>

#ifdef _WIN32
#include <Windows.h>
#endif // _WIN32

namespace tests_fn {
    bool do_coro_tests();

    bool do_crypto_tests();

    bool do_base64_tests();

    bool do_matrix_tests();

    bool do_channels_tests();

    bool do_sqlite_tests();

    bool do_odbc_tests();

    bool do_win_services_tests();

    bool do_unnamed_pipes_test();

    bool do_pipe_tests();

    bool do_strand_tests();

    bool do_ring_buffer_tests();

    bool do_test_views();

    bool do_trackable_tests();

    bool do_stack_list_tests();

    bool do_string_tests();

    bool do_net_tests();

    bool do_socket_tests();

    bool do_cli_tests();

    bool do_wolfssl_client_test();

    bool do_resolver_tests();

    bool do_http_parser_tests();

    bool do_hpack_tests();

    bool do_http_tests();

    bool do_http2_tests();

    bool do_json_tests();

    bool do_punycode_tests();

    bool do_percent_encoding_tests();

    bool do_url_tests();
} // namespace tests_fn

int main() {
    uint32_t total = 0;
    uint32_t passed = 0;

    auto do_test_fn = [&](auto fn) {
        ++total;
        if (fn()) {
            ++passed;
        }
    };

#ifdef _WIN32
    ::SetConsoleOutputCP(CP_UTF8);
    const auto do_win_test_fn = do_test_fn;
    std::ignore = do_win_test_fn;
#else
#define do_win_test_fn(x)
#endif // _WIN32

    printf("tests started on (%s)\n", rad::sysinfo::os_name().c_str());
    do_win_test_fn(tests_fn::do_odbc_tests);
    do_win_test_fn(tests_fn::do_win_services_tests);
    do_test_fn(tests_fn::do_ring_buffer_tests);
    do_test_fn(tests_fn::do_trackable_tests);
    do_test_fn(tests_fn::do_coro_tests);
    do_test_fn(tests_fn::do_test_views);
    do_test_fn(tests_fn::do_stack_list_tests);
    do_test_fn(tests_fn::do_string_tests);
    do_test_fn(tests_fn::do_net_tests);
    do_test_fn(tests_fn::do_socket_tests);
    do_test_fn(tests_fn::do_cli_tests);

    do_test_fn(tests_fn::do_json_tests);
    do_test_fn(tests_fn::do_punycode_tests);
    do_test_fn(tests_fn::do_percent_encoding_tests);
    do_test_fn(tests_fn::do_url_tests);
    do_test_fn(tests_fn::do_crypto_tests);
    do_test_fn(tests_fn::do_base64_tests);
    do_test_fn(tests_fn::do_matrix_tests);
    do_test_fn(tests_fn::do_channels_tests);
#ifdef RAD_HAVE_SQLITE
    do_test_fn(tests_fn::do_sqlite_tests);
#endif // RAD_HAVE_SQLITE
    do_test_fn(tests_fn::do_unnamed_pipes_test);
    do_win_test_fn(tests_fn::do_pipe_tests);
    do_test_fn(tests_fn::do_strand_tests);
#ifdef RAD_HAVE_WOLFSSL
    do_test_fn(tests_fn::do_wolfssl_client_test);
#endif // RAD_HAVE_WOLFSSL
    do_test_fn(tests_fn::do_http_parser_tests);
    do_test_fn(tests_fn::do_hpack_tests);
#ifdef RAD_HAVE_SSL
    do_test_fn(tests_fn::do_resolver_tests);
    do_test_fn(tests_fn::do_http_tests);
    do_test_fn(tests_fn::do_http2_tests);
#endif // RAD_HAVE_SSL

    uint32_t failed = total - passed;
    std::cout << "[*] passed in [" << passed << "/" << total << "] tests\n";
    if (failed) {
        std::cout << "[!] failed in [" << failed << "/" << total << "] tests\n";
    }
    std::cout << "Press any key to exit ...";

    getchar();
    return 0;
}