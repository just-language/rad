#include <rad/crypto/error.h>

#include <array>

namespace {
    constexpr std::array<std::string_view, 6> aes_error_messages = {
        "No error",
        "Input buffer is empty",
        "Input buffer not aligned to aes block length (16 bytes)",
        "Aes key was not set",
        "Aes iv was not set",
        "Insufficient buffer to store padding",
    };

    struct aes_error_category_type : public std::error_category {
        const char* name() const noexcept override {
            return "aes";
        }

        std::string message(int code) const override {
            if (static_cast<size_t>(code) >= aes_error_messages.size() ||
                code < 0) {
                return "unknown error";
            }
            return std::string(aes_error_messages[static_cast<size_t>(code)]);
        }
    };

    const aes_error_category_type aes_error_category_inst;
} // namespace

namespace RAD_LIB_NAMESPACE::crypto {
    const std::error_category& aes_category() noexcept {
        return aes_error_category_inst;
    }
} // namespace RAD_LIB_NAMESPACE::crypto