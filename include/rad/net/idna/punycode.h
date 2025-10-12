#pragma once
#include <rad/dynamic_buffer.h>
#include <rad/libbase.h>

#include <cstdint>
#include <string>
#include <string_view>
#include <system_error>

namespace rad::net::idna {
    inline constexpr std::string_view punycode_prefix = "xn--";

    /*!
     * @brief Check if @p input contains any non basic code points that
     * needs to be encoded in punycode. @p input must be valid UTF-8 string.
     * @param input The UTF-8 string to check if it needs to be encoded in
     * punycode.
     * @return True if encoding in punycode is required, otherwise false.
     */
    RAD_EXPORT_DECL bool needs_punycode_encode(std::string_view input) noexcept;

    /*!
     * @brief Encode UTF-8 text using punycode into ASCII text.
     * @param input The UTF-8 string to encode.
     * @param output The output dynamic buffer where encoded punycode
     * ASCII text will be appneded.
     * @param ec Set to indicate error occured, if any.
     * @return The count of consumed bytes from @p input.
     * After successful encoding, the consumed size is the size of @p input.
     */
    RAD_EXPORT_DECL std::size_t punycode_encode(std::string_view input,
                                                dynamic_buffer output,
                                                std::error_code& ec);

    /*!
     * @brief Decode punycode encoded ASCII text into UTF-8 text.
     * @param input The punycode encoded ASCII string to deencode.
     * @param output The output dynamic buffer where decoded UTF-8
     * text will be appneded.
     * @param ec Set to indicate error occured, if any.
     * @return The count of consumed bytes from @p input.
     * After successful encoding, the consumed size is the size of @p input.
     */
    RAD_EXPORT_DECL std::size_t punycode_decode(std::string_view input,
                                                dynamic_buffer output,
                                                std::error_code& ec);
} // namespace rad::net::idna