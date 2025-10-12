#pragma once
#include <rad/libbase.h>

namespace RAD_LIB_NAMESPACE::crypto {
    enum class aes_error_code : int {
        no_error,
        empty_input,
        not_aligned,
        no_key,
        no_iv,
        small_padding_buffer,
    };

    RAD_EXPORT_DECL const std::error_category& aes_category() noexcept;

    inline std::error_code make_aes_error(aes_error_code code) noexcept {
        return std::error_code{static_cast<int>(code), aes_category()};
    }
} // namespace RAD_LIB_NAMESPACE::crypto