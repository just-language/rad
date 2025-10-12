#include <rad/net/idna/punycode.h>
#include <rad/unittest/unittest.h>

#include <array>
#include <format>
#include <iostream>
#include <vector>

using namespace RAD_LIB_NAMESPACE;
using namespace net;
using namespace idna;
using namespace unittest;

namespace {
    struct punycode_test_entry {
        std::string cleartext;
        std::string encoded;
        std::string description;
    };

    std::vector<punycode_test_entry> make_punycode_test_entries() {
        std::vector<punycode_test_entry> entries;

        entries.emplace_back(
            std::string{"\u0644\u064A\u0647\u0645\u0627\u0628\u062A"
                        "\u0643\u0644\u0645"
                        "\u0648\u0634\u0639\u0631\u0628\u064A\u061F",
                        34},
            std::string{"egbpdaj6bu4bxfgehfvwxn"}, "Arabic (Egyptian)");

        entries.emplace_back(
            std::string{"\u4ED6\u4EEC\u4E3A\u4EC0\u4E48\u4E0D\u8BF4"
                        "\u4E2D\u6587",
                        27},
            std::string{"ihqwcrb4cv8a8dqg056pqjye"}, "Chinese (simplified)");

        entries.emplace_back(
            std::string{"\u4ED6\u5011\u7232\u4EC0\u9EBD\u4E0D\u8AAA"
                        "\u4E2D\u6587",
                        27},
            std::string{"ihqwctvzc91f659drss3x8bo0yb"},
            "Chinese (traditional)");

        entries.emplace_back(
            std::string{"\u0050\u0072\u006F\u010D\u0070\u0072\u006F\u0073\u0074"
                        "\u011B\u006E\u0065\u006D\u006C\u0075\u0076\u00ED\u010D"
                        "\u0065"
                        "\u0073\u006B\u0079",
                        26},
            std::string{"Proprostnemluvesky-uyb24dma41a"}, "Czech");

        entries.emplace_back(
            std::string{"\u05DC\u05DE\u05D4\u05D4\u05DD\u05E4\u05E9\u05D5\u05D8"
                        "\u05DC\u05D0\u05DE\u05D3\u05D1\u05E8\u05D9\u05DD\u05E2"
                        "\u05D1"
                        "\u05E8\u05D9\u05EA",
                        44},
            std::string{"4dbcagdahymbxekheh6e0a7fei0b"}, "Hebrew");

        entries.emplace_back(
            std::string{"\u092F\u0939\u0932\u094B\u0917\u0939\u093F\u0928\u094D"
                        "\u0926\u0940\u0915\u094D\u092F\u094B\u0902\u0928\u0939"
                        "\u0940"
                        "\u0902\u092C\u094B\u0932"
                        "\u0938\u0915\u0924\u0947\u0939\u0948\u0902",
                        90},
            std::string{"i1baa7eci9glrd9b2ae1bj0hfcgg6iyaf8o0a1dig0cd"},
            "Hindi (Devanagari)");

        entries.emplace_back(
            std::string{"\u306A\u305C\u307F\u3093\u306A\u65E5\u672C\u8A9E\u3092"
                        "\u8A71\u3057\u3066\u304F\u308C\u306A\u3044\u306E"
                        "\u304B",
                        54},
            std::string{"n8jok5ay5dzabd5bym9f0cm5685rrjetr6pdxa"},
            "Japanese (kanji and hiragana)");

        // case 8 Korean (Hangul syllables)
        entries.emplace_back(
            std::string{"\uC138\uACC4\uC758\uBAA8\uB4E0\uC0AC\uB78C\uB4E4\uC774"
                        "\uD55C\uAD6D\uC5B4\uB97C\uC774\uD574\uD55C\uB2E4\uBA74"
                        "\uC5BC"
                        "\uB9C8\uB098\uC88B\uC744\uAE4C",
                        72},
            std::string{"989aomsvi5e83db1d2a355cv1e0vak1dwrv93d5xbh15a0"
                        "dt30a5jpsd879c"
                        "cm6fea98c"});

        entries.emplace_back(
            std::string{"\u043F\u043E\u0447\u0435\u043C\u0443\u0436\u0435\u043E"
                        "\u043D\u0438\u043D\u0435\u0433\u043E\u0432\u043E\u0440"
                        "\u044F"
                        "\u0442\u043F\u043E\u0440"
                        "\u0443\u0441\u0441\u043A\u0438",
                        56},
            std::string{"b1abfaaepdrnnbgefbadotcwatmq2g4l"},
            "Russian (Cyrillic)");

        entries.emplace_back(
            std::string{"\u0050\u006F\u0072\u0071\u0075\u00E9\u006E\u006F\u0070"
                        "\u0075\u0065\u0064\u0065\u006E\u0073\u0069\u006D\u0070"
                        "\u006C"
                        "\u0065\u006D\u0065\u006E"
                        "\u0074\u0065\u0068\u0061\u0062\u006C\u0061\u0072\u0065"
                        "\u006E"
                        "\u0045\u0073\u0070"
                        "\u0061\u00F1\u006F\u006C",
                        42},
            std::string{"PorqunopuedensimplementehablarenEspaol-fmd56a"},
            "Spanish");

        entries.emplace_back(
            std::string{"\u0054\u1EA1\u0069\u0073\u0061\u006F\u0068\u1ECD\u006B"
                        "\u0068\u00F4\u006E\u0067\u0074\u0068\u1EC3\u0063\u0068"
                        "\u1EC9"
                        "\u006E\u00F3\u0069\u0074"
                        "\u0069\u1EBF\u006E\u0067\u0056\u0069\u1EC7\u0074",
                        45},
            std::string{"TisaohkhngthchnitingVit-kjcr8268qyxafd2f1b9g"},
            "Vietnamese");

        return entries;
    }

    void do_punycode_tests_internal() {
        const auto entries = make_punycode_test_entries();
        for (size_t i = 0; i < entries.size(); ++i) {
            std::string_view expected_cleartext = entries[i].cleartext;
            std::string_view expected_encoded = entries[i].encoded;

            std::string encoded;
            std::error_code ec;
            punycode_encode(expected_cleartext, dynamic_buffer(encoded), ec);

            assert_false(static_cast<bool>(ec),
                         ("punycode test case (" + entries[i].description +
                          ") failed to encode: " + ec.message())
                             .c_str());
            assert_eq(encoded, expected_encoded,
                      ("punycode test case (" + entries[i].description +
                       ") encoded mismatch")
                          .c_str());

            std::string cleartext;
            punycode_decode(encoded, dynamic_buffer(cleartext), ec);

            assert_false(static_cast<bool>(ec),
                         ("punycode test case (" + entries[i].description +
                          ") failed to decode: " + ec.message())
                             .c_str());
            assert_eq(cleartext, expected_cleartext,
                      ("punycode test case (" + entries[i].description +
                       ") decoded mismatch")
                          .c_str());
        }
    }
} // namespace

namespace tests_fn {
    bool do_punycode_tests() {
        try {
            do_punycode_tests_internal();
        }
        catch (const exception& ex) {
            std::cout << "[!] punycode tests failed ! " << ex.what() << "\n";
            return false;
        }
        catch (const std::exception& ex) {
            std::cout << "[!] punycode tests failed ! " << ex.what() << "\n";
            return false;
        }
        std::cout << "[*] punycode tests passed\n";
        return true;
    }
} // namespace tests_fn