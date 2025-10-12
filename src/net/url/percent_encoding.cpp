#include <rad/buffer.h>
#include <rad/match.h>
#include <rad/net/url/percent_encoding.h>
#include <rad/utf.h>

#include <charconv>

using namespace RAD_LIB_NAMESPACE;
using namespace net;

namespace {
    constexpr bool is_unreserved_character(codepoint_t cp) noexcept {
        return (cp >= 'A' && cp <= 'Z') || (cp >= 'a' && cp <= 'z') ||
               (cp >= '0' && cp <= '9') || cp == '-' || cp == '_' ||
               cp == '~' || cp == '.';
    }

    constexpr bool is_unreserved_character_set(
        codepoint_t cp,
        std::span<const perecent_encode_set_item> encode_set) noexcept {
        for (auto es : encode_set) {
            const bool to_encode = match(
                es, [&](perecent_encode_set_codepoint p) { return p.c == cp; },
                [&](perecent_encode_set_upto p) {
                    return cp <= p.stop_inclusive;
                },
                [&](perecent_encode_set_greater_than p) {
                    return cp > p.start_exclusive;
                });
            if (to_encode) {
                return false;
            }
        }
        return true;
    }

    /*!
     * @brief Get the utf8 encoded size of @p input
     * @tparam CodeCvt The utf code converter type
     * @tparam CharT The character type of string
     * @param input The non encoded string
     * @param ec Cleared on success and set to error on failure
     * @return The length of utf8 bytes required to percent encode @p input
     */
    template <class CodeCvt, class CharT, class Fn>
    constexpr std::pair<std::errc, std::size_t>
    get_perecent_encoded_size(std::basic_string_view<CharT> input,
                              Fn encode_set_f) noexcept {
        if (input.empty()) {
            return {};
        }
        const CharT* ptr = input.data();
        std::size_t n = input.size();
        std::size_t encoded_len = 0;
        while (n > 0) {
            std::errc e = {};
            codepoint_t cp = CodeCvt::decode(ptr, n, e);
            if (e != std::errc{}) {
                return std::pair{e, 0};
            }
            if (encode_set_f(cp)) {
                encoded_len += 1;
            }
            else {
                encoded_len += utf8_codecvt::codepoint_size(cp) * 3;
            }
        }
        return std::pair{std::errc{}, encoded_len};
    }

    constexpr uint8_t hex_char_to_decimal(char ch, std::errc& e) noexcept {
        if (ch >= '0' && ch <= '9') {
            return ch - '0' + 0;
        }
        if (ch >= 'a' && ch <= 'f') {
            return ch - 'a' + 10;
        }
        if (ch >= 'A' && ch <= 'F') {
            return ch - 'A' + 10;
        }
        e = std::errc::illegal_byte_sequence;
        return 0;
    }

    template <class CharT>
    constexpr uint8_t
    decode_percent_encoded_byte(std::basic_string_view<CharT>& input,
                                std::errc& ec) {
        assert(input.size() >= 2);

        const uint8_t byte1 =
            hex_char_to_decimal(static_cast<char>(input[0]), ec);
        if (ec != std::errc{}) {
            return 0;
        }
        const uint8_t byte2 =
            hex_char_to_decimal(static_cast<char>(input[1]), ec);
        if (ec != std::errc{}) {
            return 0;
        }
        input.remove_prefix(2);
        return byte1 * 16 + byte2;
    }

    template <class CharT, class Fn>
    constexpr std::pair<std::errc, std::size_t>
    get_perecent_decoded_size(std::basic_string_view<CharT> input,
                              Fn encode_set_f) noexcept {
        if (input.empty()) {
            return {};
        }
        std::size_t decoded_len = 0;
        while (!input.empty()) {
            const char ch = static_cast<char>(input.front());
            decoded_len += 1;
            input.remove_prefix(1);
            if (encode_set_f(ch)) {
                continue;
            }
            if (ch != '%') {
                return std::pair{std::errc::illegal_byte_sequence, 0};
            }
            if (input.size() < 2) {
                return std::pair{std::errc::no_buffer_space, 0};
            }
            std::errc e = {};
            const uint8_t byte_value = decode_percent_encoded_byte(input, e);
            if (e != std::errc{}) {
                return std::pair{e, 0};
            }
            const std::size_t seq_len = utf8_sequence_size(byte_value);
            if (seq_len == 0 || seq_len > 4) {
                return std::pair{std::errc::illegal_byte_sequence, 0};
            }
            if (seq_len == 1) {
                // decode_percent_encoded_byte already removed 2
                // chars from input
                continue;
            }
            if (input.size() < (seq_len - 1) * 3) {
                return std::pair{std::errc::no_buffer_space, 0};
            }
            std::array<uint8_t, 4> utf8_seq_buff{};
            utf8_seq_buff[0] = byte_value;
            for (auto i : range(seq_len - 1)) {
                std::errc e = {};
                if (static_cast<char>(input.front()) != '%') {
                    return std::pair{std::errc::illegal_byte_sequence, 0};
                }
                input.remove_prefix(1);
                utf8_seq_buff[i + 1] = decode_percent_encoded_byte(input, e);
                if (e != std::errc{}) {
                    return std::pair{e, 0};
                }
            }
            const uint8_t* decode_ptr = utf8_seq_buff.data();
            std::size_t decode_len = seq_len;
            const codepoint_t cp =
                utf8_codecvt::decode(decode_ptr, decode_len, e);
            if (e != std::errc{} || decode_len != 0) {
                return std::pair{std::errc::illegal_byte_sequence, 0};
            }
            std::ignore = cp;
            // already increased for the first byte
            decoded_len += seq_len - 1;
        }
        return std::pair{std::errc{}, decoded_len};
    }

    constexpr std::array<char, 2> make_hex_seq(uint8_t v) noexcept {
        constexpr std::array<char, 16> hex_digits = {
            '0', '1', '2', '3', '4', '5', '6', '7',
            '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};
        const uint8_t first_digit = v & 0b00001111;
        const uint8_t second_digit = v >> 4;
        return std::array{hex_digits[second_digit], hex_digits[first_digit]};
    }

    template <class CharT, class Fn>
    constexpr std::size_t
    do_percent_encode(std::basic_string_view<CharT> input, char* output,
                      std::size_t output_size, Fn encode_set_f) {
        if (input.size() == output_size) {
            std::copy(input.begin(), input.end(), output);
            return input.size();
        }
        while (!input.empty()) {
            const uint8_t ch = static_cast<uint8_t>(input.front());
            if (encode_set_f(static_cast<codepoint_t>(ch))) {
                output[0] = static_cast<char>(ch);
                output += 1;
                input.remove_prefix(1);
                continue;
            }
            const std::size_t in_seq_len = utf8_sequence_size(ch);
            for (auto i : range(in_seq_len)) {
                std::ignore = i;
                output[0] = '%';
                const std::array<char, 2> hex_seq =
                    make_hex_seq(static_cast<uint8_t>(input.front()));
                output[1] = hex_seq[0];
                output[2] = hex_seq[1];
                output += 3;
                input.remove_prefix(1);
            }
        }
        return output_size;
    }

    template <class CharT, class Fn>
    constexpr std::size_t
    do_percent_decode(std::basic_string_view<CharT> input, char* output,
                      std::size_t output_size, Fn encode_set_f) {
        if (input.size() == output_size) {
            std::copy(input.begin(), input.end(), output);
            return input.size();
        }
        while (!input.empty()) {
            const char ch = static_cast<char>(input.front());
            input.remove_prefix(1);
            if (encode_set_f(static_cast<codepoint_t>(ch))) {
                output[0] = ch;
                output += 1;
                continue;
            }
            assert(ch == '%');
            std::errc e = {};
            const uint8_t byte_value = decode_percent_encoded_byte(input, e);
            assert(e == std::errc{});
            std::size_t seq_len = utf8_sequence_size(byte_value);
            assert(seq_len > 0 && seq_len <= 4);
            output[0] = byte_value;
            output += 1;
            for (auto i : range(seq_len - 1)) {
                std::ignore = i;
                assert(static_cast<char>(input.front()) == '%');
                input.remove_prefix(1);
                const uint8_t u8byte = decode_percent_encoded_byte(input, e);
                assert(e == std::errc{});
                output[0] = u8byte;
                output += 1;
            }
        }
        return output_size;
    }
} // namespace

std::size_t net::perecent_encoded_size(std::string_view input,
                                       std::error_code& ec) noexcept {
    ec.clear();
    auto [e, n] =
        get_perecent_encoded_size<utf8_codecvt>(input, is_unreserved_character);
    if (e != std::errc{}) {
        ec = std::make_error_code(e);
        return 0;
    }
    return n;
}

std::size_t
net::perecent_encoded_size(std::string_view input,
                           std::span<const perecent_encode_set_item> encode_set,
                           std::error_code& ec) noexcept {
    ec.clear();
    auto [e, n] = get_perecent_encoded_size<utf8_codecvt>(
        input, [&encode_set](codepoint_t c) {
            return is_unreserved_character_set(c, encode_set);
        });
    if (e != std::errc{}) {
        ec = std::make_error_code(e);
        return 0;
    }
    return n;
}

std::size_t net::perecent_decoded_size(std::string_view input,
                                       std::error_code& ec) noexcept {
    ec.clear();
    auto [e, n] = get_perecent_decoded_size(input, is_unreserved_character);
    if (e != std::errc{}) {
        ec = std::make_error_code(e);
        return 0;
    }
    return n;
}

std::size_t
net::perecent_decoded_size(std::string_view input,
                           std::span<const perecent_encode_set_item> encode_set,
                           std::error_code& ec) noexcept {
    ec.clear();
    auto [e, n] =
        get_perecent_decoded_size(input, [&encode_set](codepoint_t c) {
            return is_unreserved_character_set(c, encode_set);
        });
    if (e != std::errc{}) {
        ec = std::make_error_code(e);
        return 0;
    }
    return n;
}

std::size_t net::percent_encode(std::string_view input, dynamic_buffer output,
                                std::error_code& ec) {
    ec.clear();
    std::size_t encoded_len = perecent_encoded_size(input, ec);
    if (ec || encoded_len == 0) {
        return 0;
    }
    auto out_buff = output.prepare(encoded_len).to_span<char>();
    return do_percent_encode(input, out_buff.data(), out_buff.size(),
                             is_unreserved_character);
}

std::size_t
net::percent_encode(std::string_view input,
                    std::span<const perecent_encode_set_item> encode_set,
                    dynamic_buffer output, std::error_code& ec) {
    ec.clear();
    std::size_t encoded_len = perecent_encoded_size(input, encode_set, ec);
    if (ec || encoded_len == 0) {
        return 0;
    }
    auto out_buff = output.prepare(encoded_len).to_span<char>();
    return do_percent_encode(
        input, out_buff.data(), out_buff.size(), [&encode_set](codepoint_t c) {
            return is_unreserved_character_set(c, encode_set);
        });
}

std::size_t net::percent_decode(std::string_view input, dynamic_buffer output,
                                std::error_code& ec) {
    ec.clear();
    std::size_t decoded_len = perecent_decoded_size(input, ec);
    if (ec || decoded_len == 0) {
        return 0;
    }
    auto out_buff = output.prepare(decoded_len).to_span<char>();
    return do_percent_decode(input, out_buff.data(), out_buff.size(),
                             is_unreserved_character);
}

std::size_t
net::percent_decode(std::string_view input,
                    std::span<const perecent_encode_set_item> encode_set,
                    dynamic_buffer output, std::error_code& ec) {
    ec.clear();
    std::size_t decoded_len = perecent_decoded_size(input, ec);
    if (ec || decoded_len == 0) {
        return 0;
    }
    auto out_buff = output.prepare(decoded_len).to_span<char>();
    return do_percent_decode(
        input, out_buff.data(), out_buff.size(), [&encode_set](codepoint_t c) {
            return is_unreserved_character_set(c, encode_set);
        });
}