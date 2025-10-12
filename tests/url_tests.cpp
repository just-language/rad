#include <rad/io/files.h>
#include <rad/json/parser.h>
#include <rad/net/url/url.h>
#include <rad/unittest/unittest.h>

#include <iostream>
#include <sstream>
#include <vector>

using namespace RAD_LIB_NAMESPACE;
using namespace net;
using namespace unittest;

namespace {
    json::array load_url_test_cases() {
        std::string json_buff;
        io::files::read_all_file("urltestdata.json", dynamic_buffer(json_buff));
        return json::parse(json_buff).as_array();
    }

    void do_url_tests_internal() {
        auto url_test_cases = load_url_test_cases();
        size_t test_nth = 0;
        size_t failure_nth = 0;
        size_t non_passed_failures = 0;
        for (const auto& test_entry : url_test_cases) {
            if (!test_entry.is_object()) {
                continue;
            }
            test_nth += 1;
            auto& test_obj = test_entry.as_object();
            const url* base_ptr = nullptr;
            url base_url;
            if (test_obj.contains("base") && !test_obj.at("base").is_null()) {
                const std::string& base_input = test_obj.at("base").as_string();
                std::error_code ec;
                base_url.parse(base_input, ec);
                if (ec) {
                    assert_true(
                        !ec,
                        ("failed to parse base url: " + base_input).c_str());
                }
                base_ptr = &base_url;
            }
            const std::string& input = test_obj.at("input").as_string();
            const bool expects_failure = test_obj.contains("failure");
            if (expects_failure) {
                failure_nth += 1;
            }
            url test_url;
            std::error_code ec;
            test_url.parse(base_ptr, input, ec);
            if (expects_failure) {
                if (!ec) {
                    assert_false(!ec,
                                 ("parsed invalid input: " + input).c_str());
                }
                continue;
            }
            else {
                if (ec) {
                    assert_true(!ec,
                                ("failed to parse input: " + input).c_str());
                }
            }
            if (test_url.href() != test_obj.at("href").as_string()) {
                std::string url_href = test_url.href();
                const std::string& test_href = test_obj.at("href").as_string();
                printf("href '%s' != '%s'\n", url_href.c_str(),
                       test_href.c_str());
            }
            assert_eq(test_url.href(), test_obj.at("href").as_string(),
                      ("parsed href mismatch: " + input).c_str());
            assert_eq(test_url.protocol(), test_obj.at("protocol").as_string(),
                      ("parsed protocol mismatch: " + input).c_str());
            assert_eq(test_url.username(), test_obj.at("username").as_string(),
                      ("parsed username mismatch: " + input).c_str());
            assert_eq(test_url.password(), test_obj.at("password").as_string(),
                      ("parsed password mismatch: " + input).c_str());
            assert_eq(test_url.host(), test_obj.at("host").as_string(),
                      ("parsed host mismatch: " + input).c_str());
            assert_eq(test_url.hostname(), test_obj.at("hostname").as_string(),
                      ("parsed hostname mismatch: " + input).c_str());
            assert_eq(test_url.port_string(), test_obj.at("port").as_string(),
                      ("parsed port mismatch: " + input).c_str());
            assert_eq(test_url.pathname(), test_obj.at("pathname").as_string(),
                      ("parsed pathname mismatch: " + input).c_str());
            assert_eq(test_url.search(), test_obj.at("search").as_string(),
                      ("parsed search mismatch: " + input).c_str());
            assert_eq(test_url.hash(), test_obj.at("hash").as_string(),
                      ("parsed hash mismatch: " + input).c_str());
        }
        std::ignore = non_passed_failures;
        std::ignore = test_nth;
        std::ignore = failure_nth;
    }
} // namespace

namespace tests_fn {
    bool do_url_tests() {
        try {
            do_url_tests_internal();
        }
        catch (const exception& ex) {
            std::cout << "[!] url tests failed ! " << ex.what() << "\n";
            return false;
        }
        catch (const std::exception& ex) {
            std::cout << "[!] url tests failed ! " << ex.what() << "\n";
            return false;
        }
        std::cout << "[*] url tests passed\n";
        return true;
    }
} // namespace tests_fn
