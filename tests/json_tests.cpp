#include <rad/json/json.h>
#include <rad/unittest/unittest.h>

#include <array>
#include <format>
#include <iostream>

using namespace RAD_LIB_NAMESPACE;
using namespace unittest;

namespace {

    constexpr auto valid_json_test_cases = std::array{
        // Basic valid cases
        R"##({"key": "value"})##",             // Simple key-value
        R"##({"number": 42})##",               // Integer
        R"##({"pi": 3.14159})##",              // Float
        R"##({"active": true})##",             // True boolean
        R"##({"active": false})##",            // False boolean
        R"##({"empty": null})##",              // Null value
        R"##({"array": [1, 2, 3]})##",         // Number array
        R"##({"nested": {"key": "value"}})##", // Nested object

        // Complex nested structures
        R"##({"users": [{"id": 1, "name": "Alice", "roles": ["admin", "user"]}, {"id": 2, "name": "Bob", "roles": ["user"]}]})##",
        R"##({"data": {"matrix": [[1, 2, 3], [4, 5, 6], [7, 8, 9]], "metadata": {"version": 1.0, "created": "2023-01-01"}}})##",
        R"##({"menu": {"id": "file", "value": "File", "popup": {"menuitem": [{"value": "New", "onclick": "CreateNewDoc()"}, {"value": "Open", "onclick": "OpenDoc()"}, {"value": "Close", "onclick": "CloseDoc()"}]}}})##",

        // Unicode escape sequences in JSON
        R"##({"arabic_escaped": "\u0627\u0644\u0633\u0644\u0627\u0645 \u0639\u0644\u064a\u0643\u0645"})##", // \u0627\u0644\u0633\u0644\u0627\u0645
                                                                                                            // \u0639\u0644\u064a\u0643\u0645
                                                                                                            // (As-salamu
                                                                                                            // alaykum)
        R"##({"japanese_escaped": "\u3053\u3093\u306B\u3061\u306F\u4E16\u754C"})##", // \u3053\u3093\u306B\u3061\u306F\u4E16\u754C
        // (Konnichiwa sekai)
        R"##({"emoji_escaped": "\u1F600\u1F603\u1F605"})##", // \u1F600\u1F603\u1F605
                                                             // (Smileys)
        R"##({"mixed_escaped": "Hello \u4E16\u754C \u0645\u0631\u062D\u0628\u0627"})##", // Hello \u4E16\u754C
        // \u0645\u0631\u062D\u0628\u0627
        // (Hello world welcome)

        // Multi-byte Unicode characters (UTF-16 surrogate pairs)
        R"##({"musical_symbol": "\uD834\uDD1E"})##", // \uD834\uDD1E
                                                     // (U+1D11E) - Musical
                                                     // Symbol G Clef
        R"##({"math_symbol": "\uD835\uDC00"})##",    // \uD835\uDC00 (U+1D400)
        // - Mathematical Bold A
        R"##({"emoji_surrogate": "\uD83D\uDE00"})##", // \uD83D\uDE00
                                                      // (U+1F600) -
                                                      // Grinning Face
        R"##({"cuneiform": "\uD808\uDF45"})##", // \uD808\uDF45 (U+12345) -
                                                // Cuneiform sign
        R"##({"complex_emoji": "\uD83D\uDC68\u200D\uD83C\uDFA8"})##", // \uD83D\uDC68\u200D\uD83C\uDFA8
        // (Man
        // Artist)

        // Complex mixed Unicode
        R"##({"multilingual": {"english": "Hello", "arabic": "\u0645\u0631\u062D\u0628\u0627", "japanese": "\u3053\u3093\u306B\u3061\u306F", "emoji": "\u1F44B"}})##", // \u0645\u0631\u062D\u0628\u0627 (Marhaba), \u3053\u3093\u306B\u3061\u306F (Konnichiwa), \u1F44B (Wave)
        R"##({"users": [{"name": "Mohamed", "country": "\u0645\u0635\u0631", "age": 30}, {"name": "Anna", "country": "Deutschland", "age": 25}]})##", // \u0645\u0635\u0631 (Egypt)

        // Edge cases
        R"##({"empty_string": ""})##",
        R"##({"special_chars": "\\\/\b\f\n\r\t\"'"})##",
        R"##({"numbers": [-2147483648, 2147483647, 1.23456789e+100, -1.23456789e-100]})##",
        R"##({"deep_nest": {"level1": {"level2": {"level3": {"level4": {"level5": "value"}}}}}})##",
        R"##({"mixed_types": [null, true, false, 42, 3.14, "string", {"nested": "object"}, [1, 2, 3]]})##",

        // Unicode characters in C strings
        "{\"\u0627\u0644\u0623\u0633\u0645\": "
        "\"\u0645\u062d\u0645\u062f\"}", // \u0627\u0644\u0623\u0633\u0645
                                         // (Al-asam),
                                         // \u0645\u062d\u0645\u062f
                                         // (Mohamed)
        "{\"\u30ad\u30fc\": \"\u30c7\u30fc\u30bf\"}", // \u30ad\u30fc
                                                      // (Key),
                                                      // \u30c7\u30fc\u30bf
                                                      // (Data)
        "{\"\u0438\u043c\u044f\": "
        "\"\u0410\u043b\u0435\u043a\u0441\u0435\u0439\"}", // \u0438\u043c\u044f
                                                           // (Name),
        // \u0410\u043b\u0435\u043a\u0441\u0435\u0439
        // (Alexey)
        "{\"emoji\": \"\u1F600\u1F603\u1F605\"}", // \u1F600\u1F603\u1F605
        // (Smileys)
        "{\"greek\": \"\u03B1\u03B2\u03B3\u03B4\"}", // \u03B1\u03B2\u03B3\u03B4
                                                     // (Alpha, Beta,
                                                     // Gamma, Delta)
        "{\"math\": \"\u221E > \u2211\"}" // \u221E (Infinity), \u2211
                                          // (Sum)
    };

    const std::vector<std::pair<std::string, size_t>> invalid_json_test_cases =
        {
            {"", 0}, // Empty string is invalid json
            {R"##({"unclosed_string": "value})##",
             23}, // Missing closing quote (offset at end)
            {R"##({"trailing_comma": true,})##",
             22}, // Trailing comma (offset at comma)
            {R"##({"invalid_number": 12.34.56})##",
             22}, // Invalid number format (offset at second decimal)
            {R"##({"unquoted_key": value})##",
             16}, // Unquoted value (offset at 'v')
            {R"##({"bad_unicode": "\uINVALID"})##",
             19}, // Invalid Unicode escape (offset at 'I')
            {R"##({"unclosed_array": [1, 2})##",
             22}, // Unclosed array (offset at end)
            {R"##({"mismatched_braces": {[]}})##",
             24}, // Mismatched braces (offset at ']')
            {R"##({"comma_error": ["a", "b", "c",]})##",
             28}, // Trailing comma in array (offset at comma)

            // Illegal Unicode code points
            {R"##({"illegal_unicode": "\uD800"})##",
             21}, // Unpaired high surrogate (offset at escape start)
            {R"##({"illegal_unicode": "\uDFFF"})##",
             21}, // Unpaired low surrogate (offset at escape start)
            {R"##({"illegal_escape": "\uZZZZ"})##",
             19}, // Invalid hex digits (offset at 'Z')
            {R"##({"incomplete_escape": "\u12"})##",
             24}, // Incomplete escape sequence (offset at end)
            {R"##({"incomplete_escape": "\u"})##",
             21}, // Incomplete escape sequence (offset at end)

            // Invalid surrogate pairs
            {R"##({"invalid_surrogate": "\uD800x"})##",
             26}, // High surrogate not followed by low surrogate (offset
                  // at 'x')
            {R"##({"invalid_surrogate": "\uDD1Ex"})##",
             26}, // Low surrogate not preceded by high surrogate (offset
                  // at 'x')
            {R"##({"invalid_surrogate_order": "\uDD1E\uD834"})##",
             35} // Surrogates in wrong order (offset at second escape)
    };

    constexpr auto serialization_test_cases = std::array{
        // Basic types
        R"##({"key": "value"})##", R"##({"number": 42})##",
        R"##({"boolean": true})##", R"##({"boolean": false})##",
        R"##({"null": null})##",

        // Arrays
        R"##({"array": [1, 2, 3]})##", R"##({"strings": ["a", "b", "c"]})##",
        R"##({"mixed": [true, false, null]})##",
        R"##({"nested_arrays": [[1, 2], [3, 4]]})##",

        // Objects
        R"##({"object": {"key": "value"}})##",
        R"##({"nested": {"level1": {"level2": "value"}}})##",

        // Empty structures
        R"##({"empty_object": {}})##", R"##({"empty_array": []})##",
        R"##({"empty_string": ""})##",

        // Unicode characters (properly escaped valid code points)
        "{\"latin\": \"abcABC123\"}", // \u0061\u0062\u0063\u0041\u0042\u0043\u0031\u0032\u0033
        "{\"greek\": \"\u03B1\u03B2\u03B3\"}", // \u03B1\u03B2\u03B3 (Greek
                                               // letters:
                                               // alpha, beta, gamma)
        "{\"arabic\": \"\u0645\u0631\u062D\u0628\u0627\"}", // \u0645\u0631\u062D\u0628\u0627
                                                            // (Arabic:
                                                            // Marhaba)
        "{\"japanese\": \"\u3053\u3093\u306B\u3061\u306F\"}", // \u3053\u3093\u306B\u3061\u306F
                                                              // (Japanese:
        // Konnichiwa)

        // Valid emoji and symbols (using proper Unicode code points)
        "{\"emoji\": \"\u263A\u2665\"}", // \u263A\u2665 (Smiling face,
                                         // heart)
        "{\"symbols\": \"\u2660\u2663\u2665\u2666\"}", // \u2660\u2663\u2665\u2666
        // (Card suits: spades,
        // clubs, hearts, diamonds)
        "{\"mixed_unicode\": \"Hello \u4E16\u754C\"}", // Hello
                                                       // \u4E16\u754C
                                                       // (Hello
        // World in Chinese/Japanese)

        // Required escapes
        R"##({"quotes": "\""})##",                // Double quote
        R"##({"backslash": "\\"})##",             // Backslash
        R"##({"control_chars": "\b\f\n\r\t"})##", // Control characters

        // Complex structures
        R"##({"users": [{"name": "Alice", "age": 30, "active": true}, {"name": "Bob", "age": 25, "active": false}]})##",
        R"##({"config": {"version": 1, "settings": {"theme": "dark", "notifications": true}, "plugins": ["plugin1", "plugin2"]}})##",

        // Special values
        R"##({"max_int": 2147483647})##", R"##({"min_int": -2147483648})##",

        R"##({"max_int64": 9223372036854775807})##",
        R"##({"min_int64": -9223372036854775808})##",

        R"##({"max_uint64": 18446744073709551615})##",

        // Edge cases
        R"##({"special_chars": "!@#$%^&*()_+-=[]{}|;:,.<>?/"})##",
        R"##({"spaces": " leading and trailing "})##",
        "{\"unicode_identifier\": \"r\u00E9sum\u00E9\"}", // r\u00E9sum\u00E9

        // Large structure (stress test)
        R"##({"data": {"items": [{"id": 1, "value": "a"}, {"id": 2, "value": "b"}, {"id": 3, "value": "c"}, {"id": 4, "value": "d"}, {"id": 5, "value": "e"}], "metadata": {"count": 5, "valid": true}}})##"};

    void test_serialize_json() {
        for (std::string_view input : serialization_test_cases) {
            auto parsed = json::parse(input);
            auto text = json::serialize(parsed);
            assert_eq(input, text, "parsed and serialized json don't match");
        }
    }

    void test_dom_parser() {
        // valid cases

        for (std::string_view input : valid_json_test_cases) {
            // test one buffer parser
            std::error_code ec;
            {
                json::parser parser;
                parser.write(input, ec);
                assert_true(!ec,
                            ("failed to parse json input: " + input).c_str());
            }
            // test multi buffer parser
            for (size_t i = 1; i < input.size(); ++i) {
                json::stream_parser parser;
                std::string_view json_text = input;
                while (!json_text.empty() && !parser.done()) {
                    std::string_view part = json_text.substr(0, i);
                    parser.write(part, ec);
                    assert_true(
                        !ec, ("failed to parse json input: " + input).c_str());
                    json_text.remove_prefix(part.size());
                }
                parser.finish(ec);
                assert_true(!ec,
                            ("failed to parse json input: " + input).c_str());
                assert_true(
                    json_text.empty(),
                    ("didn't consume the full json input: " + input).c_str());
            }
        }

        // invalid cases

        for (auto&& [input, offset] : invalid_json_test_cases) {
            // test one buffer parser
            std::error_code ec;
            {
                json::parser parser;
                std::size_t fail_pos = parser.write(input, ec);
                assert_false(!ec,
                             ("parsed invalid json input: " + input).c_str());
                std::ignore = fail_pos;
            }
            // test multi buffer parser
            for (size_t i = 1; i < input.size(); ++i) {
                json::stream_parser parser;
                std::string_view json_text = input;
                while (!json_text.empty() && !parser.done()) {
                    std::string_view part = json_text.substr(0, i);
                    // error may not appear here
                    parser.write(part, ec);
                    if (ec) {
                        break;
                    }
                    json_text.remove_prefix(part.size());
                }
                parser.finish(ec);
                assert_false(!ec,
                             ("parsed invalid json input: " + input).c_str());
            }
        }
    }
} // namespace

namespace tests_fn {
    bool do_json_tests() {
        try {
            test_serialize_json();
            test_dom_parser();
        }
        catch (const exception& ex) {
            std::cout << "[!] json tests failed ! " << ex.detailed() << "\n";
            return false;
        }
        catch (const std::exception& ex) {
            std::cout << "[!] json tests failed ! " << ex.what() << "\n";
            return false;
        }
        std::cout << "[*] json tests passed\n";
        return true;
    }
} // namespace tests_fn