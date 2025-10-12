#pragma once
#include <rad/dynamic_buffer.h>
#include <rad/libbase.h>

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <variant>

namespace rad::net {
    struct perecent_encode_set_upto {
        uint32_t stop_inclusive = 0;
    };

    struct perecent_encode_set_greater_than {
        uint32_t start_exclusive = 0;
    };

    struct perecent_encode_set_codepoint {
        uint32_t c = 0;
    };

    using perecent_encode_set_item =
        std::variant<perecent_encode_set_codepoint, perecent_encode_set_upto,
                     perecent_encode_set_greater_than>;

    RAD_EXPORT_DECL std::size_t
    perecent_encoded_size(std::string_view input, std::error_code& ec) noexcept;

    RAD_EXPORT_DECL std::size_t
    perecent_encoded_size(std::string_view input,
                          std::span<const perecent_encode_set_item> encode_set,
                          std::error_code& ec) noexcept;

    RAD_EXPORT_DECL std::size_t
    perecent_decoded_size(std::string_view input, std::error_code& ec) noexcept;

    RAD_EXPORT_DECL std::size_t
    perecent_decoded_size(std::string_view input,
                          std::span<const perecent_encode_set_item> encode_set,
                          std::error_code& ec) noexcept;

    RAD_EXPORT_DECL std::size_t percent_encode(std::string_view input,
                                               dynamic_buffer output,
                                               std::error_code& ec);

    RAD_EXPORT_DECL std::size_t
    percent_encode(std::string_view input,
                   std::span<const perecent_encode_set_item> encode_set,
                   dynamic_buffer output, std::error_code& ec);

    RAD_EXPORT_DECL std::size_t percent_decode(std::string_view input,
                                               dynamic_buffer output,
                                               std::error_code& ec);

    RAD_EXPORT_DECL std::size_t
    percent_decode(std::string_view input,
                   std::span<const perecent_encode_set_item> encode_set,
                   dynamic_buffer output, std::error_code& ec);
} // namespace rad::net