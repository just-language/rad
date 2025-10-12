#pragma once
#include <rad/libbase.h>

#include <system_error>

namespace rad::json {
    enum class error {
        // parse errors
        syntax = 1,
        extra_data,
        incomplete,
        too_deep,
        illegal_leading_surrogate,
        illegal_trailing_surrogate,
        expected_hex_digit,
        expected_utf16_escape,
        object_too_large,
        array_too_large,
        key_too_large,
        string_too_large,
        number_too_large,
        input_error,
        invalid_escape,

        not_null,
        not_bool,
        not_array,
        not_object,
        not_string,
        not_number,
        not_int64,
        not_uint64,
        not_double,
        not_exact,
        out_of_range,
        unknown_name,
    };

    RAD_EXPORT_DECL const std::error_category& error_category() noexcept;

    inline std::error_code make_error(error e) noexcept {
        return std::error_code{static_cast<int>(e), error_category()};
    }
} // namespace rad::json