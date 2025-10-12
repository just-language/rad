#include <rad/json/error.h>

using namespace rad;
using namespace json;

namespace {
    const char* json_error_to_string(error e) {
        switch (e) {
        default:
        case error::syntax:
            return "syntax error";
        case error::extra_data:
            return "extra data";
        case error::incomplete:
            return "incomplete JSON";
        case error::too_deep:
            return "too deep";
        case error::illegal_leading_surrogate:
            return "illegal leading surrogate";
        case error::illegal_trailing_surrogate:
            return "illegal trailing surrogate";
        case error::expected_hex_digit:
            return "expected hex digit";
        case error::expected_utf16_escape:
            return "expected utf16 escape";
        case error::object_too_large:
            return "object contains too many elements";
        case error::array_too_large:
            return "array contains too many elements";
        case error::key_too_large:
            return "key is too large";
        case error::string_too_large:
            return "string is too large";
        case error::number_too_large:
            return "number is too large";
        case error::input_error:
            return "error occured when trying to read "
                   "input";
        case error::invalid_escape:
            return "invalid escape sequence";

        case error::not_null:
            return "JSON null was expected during "
                   "conversion";
        case error::not_bool:
            return "JSON bool was expected during "
                   "conversion";
        case error::not_array:
            return "JSON array was expected during "
                   "conversion";
        case error::not_object:
            return "JSON object was expected during "
                   "conversion";
        case error::not_string:
            return "JSON string was expected during "
                   "conversion";
        case error::not_number:
            return "JSON number was expected during "
                   "conversion";
        case error::not_int64:
            return "int64 was expected during conversion";
        case error::not_uint64:
            return "uint64 was expected during conversion";
        case error::not_double:
            return "double was expected during conversion";
        case error::not_exact:
            return "number cast is not exact";
        case error::out_of_range:
            return "the requested element is outside of "
                   "container's range";
        case error::unknown_name:
            return "the key does not correspond to a known "
                   "name";
        }
    }

    class json_error_category_t final : public std::error_category {
    public:
        const char* name() const noexcept override {
            return "json";
        }

        std::string message(int code) const override {
            return json_error_to_string(static_cast<error>(code));
        }
    };

    const json_error_category_t json_error_category;
} // namespace

const std::error_category& json::error_category() noexcept {
    return json_error_category;
}