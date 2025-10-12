#include <rad/json/basic_parser.h>
#include <rad/string.h>
#include <cstdlib>

using namespace rad;
using namespace json;
using namespace json::detail;

namespace {
    void put_hex_char_of_unicode_sequence(codepoint_t cp,
                                          string_escape_context& esc_ctx,
                                          std::size_t char_pos,
                                          escape_state next_state,
                                          std::error_code& ec) {
        if (!((cp >= '0' && cp <= '9') || (cp >= 'A' && cp <= 'F') ||
              (cp >= 'a' && cp <= 'f'))) {
            ec = make_error(error::expected_hex_digit);
            return;
        }
        char* hex_buff_ptr =
            esc_ctx.has_hex_buff ? esc_ctx.hex_buff2 : esc_ctx.hex_buff;
        hex_buff_ptr[char_pos] = static_cast<char>(cp);
        esc_ctx.esc = next_state;
    }

    std::string_view encode_unicode_sequence(char* hex_buff,
                                             std::array<char, 4>& utf8_buff,
                                             std::error_code& ec) {
        codepoint_t escaped_cp = 0;
        const auto res =
            std::from_chars(hex_buff, hex_buff + 4, escaped_cp, 16);
        if (res.ec != std::errc{}) {
            return "";
        }
        std::errc validate_ec{};
        if (!validate_codepoint(escaped_cp, validate_ec)) {
            // ec = std::make_error_code(validate_ec);
            return "";
        }
        const std::size_t cp_len = utf8_codecvt::encode(
            escaped_cp, utf8_buff.data(), utf8_buff.size());
        return std::string_view{utf8_buff.data(), cp_len};
    }

    std::string_view encode_utf16_pairs(char* hex_buff1, char* hex_buff2,
                                        std::array<char, 4>& utf8_buff,
                                        std::error_code& ec) {
        uint16_t pair1 = 0, pair2 = 0;
        auto res = std::from_chars(hex_buff1, hex_buff1 + 4, pair1, 16);
        if (res.ec != std::errc{}) {
            ec = make_error(error::illegal_leading_surrogate);
            return "";
        }
        res = std::from_chars(hex_buff2, hex_buff2 + 4, pair2, 16);
        if (res.ec != std::errc{}) {
            ec = make_error(error::illegal_trailing_surrogate);
            return "";
        }
        const std::array<uint16_t, 2> utf16_buff{{pair1, pair2}};
        const uint16_t* utf16_ptr = utf16_buff.data();
        std::size_t utf16_size = 2;
        std::errc utf16_ec = {};
        codepoint_t escaped_cp =
            utf16_codecvt::decode(utf16_ptr, utf16_size, utf16_ec);
        if (utf16_ec != std::errc{}) {
            ec = make_error(error::illegal_leading_surrogate);
            return "";
        }

        const std::size_t cp_len = utf8_codecvt::encode(
            escaped_cp, reinterpret_cast<uint8_t*>(utf8_buff.data()),
            utf8_buff.size());
        if (cp_len == 0) {
            ec = make_error(error::illegal_leading_surrogate);
            return "";
        }
        return std::string_view{utf8_buff.data(), cp_len};
    }

    std::string_view parse_json_escape_sequence(codepoint_t cp,
                                                string_escape_context& esc_ctx,
                                                char& unescaped_char,
                                                std::array<char, 4>& utf8_buff,
                                                std::error_code& ec) {
        if (esc_ctx.esc == escape_state::want_u) {
            if (cp != 'u') {
                ec = make_error(error::expected_utf16_escape);
                return "";
            }
            esc_ctx.esc = escape_state::want_4hex_digits;
            return "";
        }
        else if (esc_ctx.esc == escape_state::want_escape) {
            const bool is_escaped_char = cp == '"' || cp == '\\' || cp == '/' ||
                                         cp == 'b' || cp == 'f' || cp == 'n' ||
                                         cp == 'r' || cp == 't' || cp == 'u';
            if (!is_escaped_char) {
                ec = make_error(error::invalid_escape);
                return "";
            }
            if (cp == 'u') {
                esc_ctx.esc = escape_state::want_4hex_digits;
                return "";
            }

            esc_ctx.esc = escape_state::none;
            unescaped_char = 0;
            if (cp == '"' || cp == '\\' || cp == '/') {
                unescaped_char = static_cast<char>(cp);
            }
            else if (cp == 'b') {
                unescaped_char = '\b';
            }
            else if (cp == 'f') {
                unescaped_char = '\f';
            }
            else if (cp == 'n') {
                unescaped_char = '\n';
            }
            else if (cp == 'r') {
                unescaped_char = '\r';
            }
            else if (cp == 't') {
                unescaped_char = '\t';
            }
            assert(unescaped_char != 0);
            return std::string_view{&unescaped_char, 1};
        }
        else if (esc_ctx.esc == escape_state::want_4hex_digits) {
            put_hex_char_of_unicode_sequence(
                cp, esc_ctx, 0, escape_state::want_3hex_digits, ec);
            return "";
        }
        else if (esc_ctx.esc == escape_state::want_3hex_digits) {
            put_hex_char_of_unicode_sequence(
                cp, esc_ctx, 1, escape_state::want_2hex_digits, ec);
            return "";
        }
        else if (esc_ctx.esc == escape_state::want_2hex_digits) {
            put_hex_char_of_unicode_sequence(
                cp, esc_ctx, 2, escape_state::want_1hex_digits, ec);
            return "";
        }
        else if (esc_ctx.esc == escape_state::want_1hex_digits) {
            put_hex_char_of_unicode_sequence(cp, esc_ctx, 3, escape_state::none,
                                             ec);
            if (ec) {
                return "";
            }
            // try to encode this sequence
            if (!esc_ctx.has_hex_buff) {
                auto encoded_seq =
                    encode_unicode_sequence(esc_ctx.hex_buff, utf8_buff, ec);
                if (ec) {
                    return "";
                }
                if (encoded_seq.empty()) {
                    // look for the other pair
                    esc_ctx.has_hex_buff = true;
                    esc_ctx.esc = escape_state::want_reverse_solidus;
                    return "";
                }
                return encoded_seq;
            }
            else {
                esc_ctx.has_hex_buff = false;
                auto encoded_seq = encode_utf16_pairs(
                    esc_ctx.hex_buff, esc_ctx.hex_buff2, utf8_buff, ec);
                if (ec) {
                    return "";
                }
                assert(!encoded_seq.empty());
                return encoded_seq;
            }
        }
        else if (esc_ctx.esc == escape_state::want_reverse_solidus) {
            if (cp != '\\') {
                ec = make_error(error::illegal_leading_surrogate);
                return "";
            }
            esc_ctx.esc = escape_state::want_u;
            return "";
        }
        else {
            assert(false);
            return "";
        }
    }
} // namespace

std::string_view json::detail::parse_json_string(
    utf8_reader& utf8_reader, std::string_view& jtext,
    string_escape_context& esc_ctx, char& unescaped_char,
    std::array<char, 4>& utf8_buff, bool& finished, std::error_code& ec) {
    assert(!finished && !ec && !jtext.empty());

    const char* data = jtext.data();
    std::size_t n = jtext.size();
    const std::size_t total_size = n;
    const char* saved_data = data;

    while (!jtext.empty()) {
        std::errc e{};
        const std::size_t saved_n = n;
        const std::size_t expected_cache_size = utf8_reader.expected_cached_len;
        codepoint_t cp = utf8_reader.decode(data, n, e);
        jtext.remove_prefix(saved_n - n);
        if (e != std::errc{}) {
            ec = std::make_error_code(e);
            return std::string_view{saved_data, total_size - n};
        }

        if (esc_ctx.esc != escape_state::none) {
            auto result = parse_json_escape_sequence(
                cp, esc_ctx, unescaped_char, utf8_buff, ec);
            if (ec) {
                // don't consume the invalid code point
                return result;
            }
            // update saved_data
            saved_data = data;
            if (!result.empty()) {
                return result;
            }
            continue;
        }

        assert(!esc_ctx.has_hex_buff);

        if (expected_cache_size > 0) {
            return std::string_view{
                reinterpret_cast<const char*>(utf8_reader.cached_bytes.data()),
                expected_cache_size};
        }

        if (cp == '\\') {
            assert(esc_ctx.esc == escape_state::none);
            esc_ctx.esc = escape_state::want_escape;
            // report existing parsed string before '\'
            if (total_size != saved_n) {
                return std::string_view{saved_data, total_size - saved_n};
            }
            // update saved_data
            saved_data = reinterpret_cast<const char*>(data);
            continue;
        }

        if (cp == '"') {
            finished = true;
            // don't include '"' in the reported string
            return std::string_view{saved_data, total_size - saved_n};
        }
    }

    if (esc_ctx.esc == escape_state::none) {
        return std::string_view{saved_data, total_size - n};
    }
    else {
        return "";
    }
}

namespace {
#if __cpp_lib_to_chars == 201611L
    double parse_json_double(parsing_number& i, std::string_view number_text,
                             std::error_code& ec) {
        double val = 0;
        const auto res = std::from_chars(
            number_text.data(), number_text.data() + number_text.size(), val,
            i.e_pos != 0 ? std::chars_format::scientific
                         : std::chars_format::fixed);
        if (res.ec != std::errc{}) {
            if (res.ec == std::errc::result_out_of_range) {
                ec = make_error(error::number_too_large);
            }
            else {
                ec = make_error(error::syntax);
            }
            return 0.0;
        }
        return val;
    }
#else
    double parse_json_double(parsing_number& i, std::string_view number_text,
                             std::error_code& ec) {
        char* end_ptr = nullptr;
        double val = std::strtod(number_text.data(), &end_ptr);
        if (errno == ERANGE) {
            ec = make_error(error::number_too_large);
            return 0;
        }
        if (end_ptr == number_text.data()) {
            ec = make_error(error::syntax);
            return 0;
        }
        return val;
    }
#endif // __cpp_lib_to_chars == 201611L
} // namespace

std::variant<int64_t, uint64_t, double>
json::detail::parse_json_number(parsing_number& i, std::string_view number_text,
                                std::error_code& ec) noexcept {
    assert(!number_text.empty());
    const std::size_t digits_start_pos = i.has_minus ? 1 : 0;
    if (number_text[digits_start_pos] == '0') {
        if (number_text.size() == digits_start_pos + 1) {
            // 0 or -0
            return i.has_minus ? std::int64_t{0} : std::uint64_t{0};
        }
        else {
            // leading 0 is not allowed except for 0, 0.123, 0e123,
            // 0E123
            const char next_char = number_text[digits_start_pos + 1];
            if (next_char != '.' && next_char != 'e' && next_char != 'E') {
                ec = make_error(error::syntax);
                return std::int64_t{0};
            }
        }
    }

    constexpr std::size_t max_u64_digits =
        std::numeric_limits<uint64_t>::digits10 + 1;
    const bool must_parse_as_double =
        i.point_pos != 0 || i.e_pos != 0 || number_text.size() > max_u64_digits;
    if (!i.has_minus && !must_parse_as_double) {
        // the maximum uint64_t is 20 digits not 19!
        // but not all 20 digits numbers are valid uint64_t
        // try to convert it to uint64_t
        std::uint64_t res = to_uint64(number_text, 10, ec);
        if (!ec) {
            return res;
        }
        // treat it as a double
        ec.clear();
    }
    else if (i.has_minus && !must_parse_as_double) {
        // try to convert it to int64_t
        std::int64_t res = to_int64(number_text, 10, ec);
        if (!ec) {
            return res;
        }
        // treat it as a double
        ec.clear();
    }

    return parse_json_double(i, number_text, ec);
}