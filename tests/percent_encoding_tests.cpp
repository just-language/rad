#include <rad/net/url/percent_encoding.h>
#include <rad/unittest/unittest.h>

#include <array>
#include <format>
#include <iostream>
#include <vector>

using namespace RAD_LIB_NAMESPACE;
using namespace net;
using namespace unittest;

namespace {
    struct percent_test_entry {
        std::string decoded;
        std::string encoded;
        std::string description;
    };

    std::vector<percent_test_entry> make_percent_test_entries() {
        return {
            // Basic characters (should not be encoded)
            {"abcABC123", "abcABC123", "Alphanumeric characters"},
            {"-._~", "-._~", "Special characters that don't need encoding"},

            // Reserved characters (should be encoded)
            {"!@#$%^&*()", "%21%40%23%24%25%5E%26%2A%28%29",
             "Various reserved characters"},
            {"hello world", "hello%20world", "Space encoding"},
            {"path/with/slashes", "path%2Fwith%2Fslashes", "Forward slashes"},
            {"query?param=value", "query%3Fparam%3Dvalue",
             "Question mark and equals"},
            {"email@example.com", "email%40example.com", "At symbol"},
            {"key:value", "key%3Avalue", "Colon"},
            {"semi;colon", "semi%3Bcolon", "Semicolon"},
            {"comma,separated", "comma%2Cseparated", "Comma"},
            {"plus+sign", "plus%2Bsign", "Plus sign"},

            // Edge cases
            {"", "", "Empty string"},
            {" ", "%20", "Single space"},
            {"%", "%25", "Percent sign itself"},
            {std::string{"\x00\x01\x02", 3}, "%00%01%02", "Control characters"},
            {"\t\n\r", "%09%0A%0D", "Whitespace characters"},

            // Unicode characters (using escape sequences)
            {"caf\u00E9", "caf%C3%A9", "Latin-1 supplement (\u00E9)"},
            {"\u4E2D\u6587", "%E4%B8%AD%E6%96%87",
             "Chinese characters (\u4E2D\u6587)"},
            {"\U0001F30D", "%F0%9F%8C%8D", "Emoji (Earth globe \U0001F30D)"},
            {"caf\u00E9_\u4E2D\u6587_\U0001F30D",
             "caf%C3%A9_%E4%B8%AD%E6%96%87_%F0%9F%8C%8D",
             "Mixed Unicode characters "
             "(caf\u00E9_\u4E2D\u6587_\U0001F30D)"},

            // Query parameter specific cases
            {"key=value&another=param", "key%3Dvalue%26another%3Dparam",
             "Query string special chars"},
            {"param with spaces", "param%20with%20spaces",
             "Spaces in query params"},

            // Path segment cases
            {"path/segment/with/special*chars",
             "path%2Fsegment%2Fwith%2Fspecial%2Achars",
             "Path with special chars"},

            // Mixed cases
            {"user@example.com/path/to%file?name=John Doe&age=30",
             "user%40example.com%2Fpath%2Fto%25file%3Fname%3DJohn%"
             "20Doe%26age%"
             "3D30",
             "Complex URL with multiple components"}};
    }

    void do_percent_test() {
        const auto entries = make_percent_test_entries();
        for (const auto& entry : entries) {
            std::string encoded;
            std::error_code ec;
            percent_encode(entry.decoded, dynamic_buffer(encoded), ec);
            if (encoded != entry.encoded) {
                printf("[!] encoded: \"%s\", expected: \"%s\"", encoded.c_str(),
                       entry.encoded.c_str());
            }
            assert_false(static_cast<bool>(ec),
                         ("percent test case (" + entry.description +
                          ") failed to encode: " + ec.message())
                             .c_str());
            assert_eq(encoded, entry.encoded,
                      ("percent test case (" + entry.description +
                       ") encoded mismatch")
                          .c_str());

            std::string decoded;
            percent_decode(encoded, dynamic_buffer(decoded), ec);
            if (decoded != entry.decoded) {
                printf("[!] decoded: \"%s\", expected: \"%s\"", decoded.c_str(),
                       entry.decoded.c_str());
            }
            assert_false(static_cast<bool>(ec),
                         ("percent test case (" + entry.description +
                          ") failed to decode: " + ec.message())
                             .c_str());
            assert_eq(decoded, entry.decoded,
                      ("percent test case (" + entry.description +
                       ") decoded mismatch")
                          .c_str());
        }
    }
} // namespace

namespace tests_fn {
    bool do_percent_encoding_tests() {
        try {
            do_percent_test();
        }
        catch (const exception& ex) {
            std::cout << "[!] percent encoding tests failed ! " << ex.what()
                      << "\n";
            return false;
        }
        catch (const std::exception& ex) {
            std::cout << "[!] percent encoding tests failed ! " << ex.what()
                      << "\n";
            return false;
        }
        std::cout << "[*] percent encoding tests passed\n";
        return true;
    }
} // namespace tests_fn