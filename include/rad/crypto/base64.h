#pragma once
#include <rad/buffer.h>
#include <rad/libbase.h>

namespace RAD_LIB_NAMESPACE::base64 {
    /*!
     * @brief Perform BASE64 encoding.
     * @param input The raw text input.
     * @param output The encoded base64 result is appnded to this dynamic
     * buffer.
     */
    RAD_EXPORT_DECL void encode(const_buffer input, dynamic_buffer output);

    /*!
     * @brief Perform BASE64 decoding.
     * @param input The encoded base64 input.
     * @param output The decoded base64 result is appnded to this dynamic
     * buffer.
     */
    RAD_EXPORT_DECL void decode(const_buffer input, dynamic_buffer output);
} // namespace RAD_LIB_NAMESPACE::base64