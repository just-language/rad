#include <rad/crypto/base64.h>
#include <rad/string.h>

#include <iostream>

using namespace RAD_LIB_NAMESPACE;

namespace {
    void test_base64_encoding_decoding() {
        struct base64_data {
            std::string_view source;
            std::string_view encoded;
        };

        auto vectors = std::array{base64_data{"", ""},
                                  base64_data{"f", "Zg=="},
                                  base64_data{"fo", "Zm8="},
                                  base64_data{"foo", "Zm9v"},
                                  base64_data{"foob", "Zm9vYg=="},
                                  base64_data{"fooba", "Zm9vYmE="},
                                  base64_data{"foobar", "Zm9vYmFy"}};

        std::string output;

        for (const auto& bdata : vectors) {
            output.clear();

            base64::encode(buffer(bdata.source), dynamic_buffer(output));
            if (output != bdata.encoded) {
                std::string msg = "failed to encode '" + bdata.source +
                                  "' expected : '" + bdata.encoded +
                                  "' but got '" + output + "'";
                throw std::runtime_error(msg);
            }

            output.clear();

            base64::decode(buffer(bdata.encoded), dynamic_buffer(output));
            if (output != bdata.source) {
                std::string msg = "failed to decode '" + bdata.encoded +
                                  "' expected : '" + bdata.source +
                                  "' but got '" + output + "'";
                throw std::runtime_error(msg);
            }
        }
    }
} // namespace

namespace tests_fn {
    bool do_base64_tests() {
        try {
            test_base64_encoding_decoding();
        }
        catch (const std::exception& ex) {
            std::cout << "[!] base64 tests failed ! " << ex.what() << "\n";
            return false;
        }

        std::cout << "[*] base64 tests passed\n";
        return true;
    }
} // namespace tests_fn