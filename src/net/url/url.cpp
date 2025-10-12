#include <rad/net/idna/idna.h>
#include <rad/net/types.h>
#include <rad/net/url/percent_encoding.h>
#include <rad/net/url/url.h>

using namespace rad;
using namespace net;
using namespace net::detail;

namespace {
    enum class parse_state {
        scheme_start,
        scheme,
        no_scheme,
        special_relative_or_authority,
        path_or_authority,
        relative,
        relative_slash,
        special_authority_slashes,
        special_authority_ignore_slashes,
        authority,
        host,
        port,
        file,
        file_slash,
        file_host,
        path_start,
        path,
        opaque_path,
        query,
        fragment,
    };

    // The C0 control percent-encode set are the C0 controls and all code
    // points greater than U+007E (~).
    constexpr std::array<perecent_encode_set_item, 2>
        c0_control_percent_encode_set = {
            perecent_encode_set_upto{0x1ful},
            perecent_encode_set_greater_than{0x7eul}};

    // The fragment percent-encode set is the C0 control percent-encode set
    // and U+0020 SPACE, U+0022 ("), U+003C (<), U+003E (>), and U+0060 (`).
    constexpr std::array<perecent_encode_set_item, 7>
        fragment_percent_encode_set = {
            perecent_encode_set_upto{0x1ful},
            perecent_encode_set_greater_than{0x7eul},
            perecent_encode_set_codepoint{0x20ul},
            perecent_encode_set_codepoint{0x22ul},
            perecent_encode_set_codepoint{0x3cul},
            perecent_encode_set_codepoint{0x3eul},
            perecent_encode_set_codepoint{0x60ul},
    };

    // The query percent-encode set is the C0 control percent-encode set
    // and U+0020 SPACE, U+0022 ("), U+0023 (#), U+003C (<), and U+003E (>).
    constexpr std::array<perecent_encode_set_item, 7> query_percent_encode_set =
        {
            perecent_encode_set_upto{0x1ful},
            perecent_encode_set_greater_than{0x7eul},
            perecent_encode_set_codepoint{0x20ul},
            perecent_encode_set_codepoint{0x22ul},
            perecent_encode_set_codepoint{0x3cul},
            perecent_encode_set_codepoint{0x3eul},
            perecent_encode_set_codepoint{0x23ul},
    };

    // The special-query percent-encode set is the query percent-encode set
    // and U+0027 (').
    constexpr std::array<perecent_encode_set_item, 8>
        special_query_percent_encode_set = {
            perecent_encode_set_upto{0x1ful},
            perecent_encode_set_greater_than{0x7eul},
            perecent_encode_set_codepoint{0x20ul},
            perecent_encode_set_codepoint{0x22ul},
            perecent_encode_set_codepoint{0x3cul},
            perecent_encode_set_codepoint{0x3eul},
            perecent_encode_set_codepoint{0x23ul},
            perecent_encode_set_codepoint{0x27ul},
    };

    // The path percent-encode set is the query percent-encode set and
    // U+003F (?), U+005E (^), U+0060 (`), U+007B ({), and U+007D (}).
    constexpr std::array<perecent_encode_set_item, 12> path_percent_encode_set =
        {
            perecent_encode_set_upto{0x1ful},
            perecent_encode_set_greater_than{0x7eul},
            perecent_encode_set_codepoint{0x20ul},
            perecent_encode_set_codepoint{0x22ul},
            perecent_encode_set_codepoint{0x3cul},
            perecent_encode_set_codepoint{0x3eul},
            perecent_encode_set_codepoint{0x23ul},
            perecent_encode_set_codepoint{0x3ful},
            perecent_encode_set_codepoint{0x5eul},
            perecent_encode_set_codepoint{0x60ul},
            perecent_encode_set_codepoint{0x7bul},
            perecent_encode_set_codepoint{0x7dul},
    };

    // The userinfo percent-encode set is the path percent-encode set and
    // U+002F
    // (/), U+003A (:), U+003B (;), U+003D (=), U+0040 (@), U+005B ([) to
    // U+005D
    // (]), inclusive, and U+007C (|).
    constexpr std::array<perecent_encode_set_item, 21>
        userinfo_percent_encode_set = {
            perecent_encode_set_upto{0x1ful},
            perecent_encode_set_greater_than{0x7eul},
            perecent_encode_set_codepoint{0x20ul},
            perecent_encode_set_codepoint{0x22ul},
            perecent_encode_set_codepoint{0x3cul},
            perecent_encode_set_codepoint{0x3eul},
            perecent_encode_set_codepoint{0x23ul},
            perecent_encode_set_codepoint{0x3ful},
            perecent_encode_set_codepoint{0x5eul},
            perecent_encode_set_codepoint{0x60ul},
            perecent_encode_set_codepoint{0x7bul},
            perecent_encode_set_codepoint{0x7dul},
            perecent_encode_set_codepoint{0x2ful},
            perecent_encode_set_codepoint{0x3aul},
            perecent_encode_set_codepoint{0x3bul},
            perecent_encode_set_codepoint{0x3dul},
            perecent_encode_set_codepoint{0x40ul},
            perecent_encode_set_codepoint{0x5bul},
            perecent_encode_set_codepoint{0x5cul},
            perecent_encode_set_codepoint{0x5dul},
            perecent_encode_set_codepoint{0x7cul},
    };

    // The component percent-encode set is the userinfo percent-encode set
    // and U+0024 ($) to U+0026 (&), inclusive, U+002B (+), and U+002C (,)
    /*
    constexpr std::array<perecent_encode_set_item, 26>
    component_percent_encode_set = { perecent_encode_set_upto{ 0x1ful },
    perecent_encode_set_greater_than{ 0x7eul
    }, perecent_encode_set_codepoint{ 0x20ul },
    perecent_encode_set_codepoint{ 0x22ul }, perecent_encode_set_codepoint{
    0x3cul }, perecent_encode_set_codepoint{ 0x3eul },
    perecent_encode_set_codepoint{ 0x23ul
    }, perecent_encode_set_codepoint{ 0x3ful },
    perecent_encode_set_codepoint{ 0x5eul }, perecent_encode_set_codepoint{
    0x60ul }, perecent_encode_set_codepoint{ 0x7bul },
    perecent_encode_set_codepoint{ 0x7dul }, perecent_encode_set_codepoint{
    0x2ful }, perecent_encode_set_codepoint{ 0x3aul },
    perecent_encode_set_codepoint{ 0x3bul
    }, perecent_encode_set_codepoint{ 0x3dul },
    perecent_encode_set_codepoint{ 0x40ul }, perecent_encode_set_codepoint{
    0x5bul }, perecent_encode_set_codepoint{ 0x5cul },
    perecent_encode_set_codepoint{ 0x5dul }, perecent_encode_set_codepoint{
    0x7cul }, perecent_encode_set_codepoint{ 0x24ul },
    perecent_encode_set_codepoint{ 0x25ul
    }, perecent_encode_set_codepoint{ 0x26ul },
    perecent_encode_set_codepoint{ 0x2bul }, perecent_encode_set_codepoint{
    0x2cul },
    };
    */
    // The application/x-www-form-urlencoded percent-encode set is the
    // component percent-encode set and U+0021 (!), U+0027 (') to U+0029
    // RIGHT PARENTHESIS, inclusive, and U+007E (~).

    constexpr bool is_c0_control(codepoint_t c) {
        return c <= 0x1f;
    }

    constexpr bool is_c0_or_space_ch(uint8_t ch) {
        return is_c0_control(ch) || ch == ' ';
    }

    constexpr bool is_ascii_tab_or_newline(uint8_t ch) {
        return ch == '\t' || ch == 0xa || ch == 0xd;
    }

    constexpr bool is_ascii_uppercase_alpha(codepoint_t cp) {
        return cp >= 'A' && cp <= 'Z';
    }

    constexpr bool is_ascii_lowercase_alpha(codepoint_t cp) {
        return cp >= 'a' && cp <= 'z';
    }

    constexpr bool is_ascii_alpha(codepoint_t cp) {
        return is_ascii_lowercase_alpha(cp) || is_ascii_uppercase_alpha(cp);
    }

    constexpr bool is_ascii_numeric(codepoint_t c) {
        return c >= '0' && c <= '9';
    }

    constexpr bool is_ascii_alphanumeric(codepoint_t c) {
        return is_ascii_alpha(c) || is_ascii_numeric(c);
    }

    constexpr char get_ascii_lowercase(codepoint_t c) {
        if (is_ascii_uppercase_alpha(c)) {
            return static_cast<char>(c - 'A' + 'a');
        }
        return static_cast<char>(c);
    }

    constexpr bool is_hex_digit(char c) {
        return is_ascii_numeric(c) || (c >= 'a' && c <= 'f') ||
               (c >= 'A' && c <= 'F');
    }

    constexpr bool starts_with_two_hex_digits(std::string_view input) {
        return input.size() >= 2 && is_hex_digit(input[0]) &&
               is_hex_digit(input[1]);
    }

    constexpr bool is_special_scheme(std::string_view scheme) {
        return scheme == "ftp" || scheme == "file" || scheme == "http" ||
               scheme == "https" || scheme == "ws" || scheme == "wss";
    }

    constexpr std::optional<uint16_t>
    default_scheme_port(std::string_view scheme) {
        if (scheme == "http" || scheme == "ws") {
            return 80;
        }
        if (scheme == "https" || scheme == "wss") {
            return 443;
        }
        if (scheme == "ftp") {
            return 21;
        }
        return std::nullopt;
    }

    void get_username_and_password_from_buffer(std::string_view buffer,
                                               url_record& url,
                                               bool& password_token_seen) {
        const char* p = buffer.data();
        std::size_t n = buffer.size();
        // For each codePoint in buffer:
        while (n > 0) {
            const char* saved_p = p;
            codepoint_t c = utf8_codecvt::decode_unchecked(p, n);
            // 1 If codePoint is U+003A (:) and passwordTokenSeen is
            // false, then set passwordTokenSeen to true and
            // continue.
            if (c == ':' && !password_token_seen) {
                password_token_seen = true;
                url.password_pos =
                    static_cast<std::int32_t>(url.userinfo.size());
                continue;
            }
            // 2 Let encodedCodePoints be the result of running
            // UTF-8 percent-encode codePoint using the userinfo
            // percent-encode set. 3 If passwordTokenSeen is true,
            // then append encodedCodePoints to url's password.
            if (password_token_seen) {
                url.set_has_password();
            }
            // 4 Otherwise, append encodedCodePoints to url's
            // username.
            else {
                url.set_has_username();
            }
            std::error_code ec;
            percent_encode(std::string_view{saved_p, p},
                           userinfo_percent_encode_set,
                           dynamic_buffer(url.userinfo), ec);
            assert(!ec);
            std::ignore = ec;
        }
    }

    constexpr bool is_non_character(codepoint_t c) {
        // Check for the range U+FDD0 to U+FDEF
        if (c >= 0xFDD0 && c <= 0xFDEF) {
            return true;
        }
        // Check if the last two bytes are 0xFFFE or 0xFFFF
        uint16_t trailing_bytes = c & 0xFFFF;
        return (trailing_bytes == 0xFFFE) || (trailing_bytes == 0xFFFF);
    }

    constexpr bool is_url_codepoint(codepoint_t c) {
        if (is_ascii_alphanumeric(c)) {
            return true;
        }
        // Check for Unicode characters in the range U+00A0 to U+10FFFD
        // c <= 0x10FFFD is not necessary because since c is a valid
        // code point
        if (c >= 0x00A0) {
            // Exclude noncharacters
            return !is_non_character(c);
        }
        switch (c) {
        case '!':  // U+0021
        case '$':  // U+0024
        case '&':  // U+0026
        case '\'': // U+0027
        case '(':  // U+0028
        case ')':  // U+0029
        case '*':  // U+002A
        case '+':  // U+002B
        case ',':  // U+002C
        case '-':  // U+002D
        case '.':  // U+002E
        case '/':  // U+002F
        case ':':  // U+003A
        case ';':  // U+003B
        case '=':  // U+003D
        case '?':  // U+003F
        case '@':  // U+0040
        case '_':  // U+005F
        case '~':  // U+007E
        case '[':  // U+005B
        case ']':  // U+005D
            return true;
        default:
            return false;
        }
    }

    constexpr bool is_forbidden_host_code_point(codepoint_t c) {
        return c == 0 || c == '\t' || c == 0xa || c == 0xd || c == ' ' ||
               c == '#' || c == '/' || c == ':' || c == '<' || c == '>' ||
               c == '?' || c == '@' || c == '[' || c == ']' || c == '\\' ||
               c == '^' || c == '|';
    }

    constexpr bool is_forbidden_domain_code_point(codepoint_t c) {
        // A forbidden domain code point is a forbidden host code point,
        // a C0 control, U+0025 (%), or U+007F DELETE.
        return is_forbidden_host_code_point(c) || is_c0_control(c) ||
               c == '%' || c == 0x7f;
    }

    bool contains_forbidden_domain_code_point(std::string_view input) {
        const char* p = input.data();
        std::size_t n = input.size();
        while (n > 0) {
            codepoint_t c = utf8_codecvt::decode_unchecked(p, n);
            if (is_forbidden_domain_code_point(c)) {
                return true;
            }
        }
        return false;
    }

    constexpr bool is_windows_drive_letter(std::string_view input) {
        // A Windows drive letter is two code points, of which the first
        // is an ASCII alpha and the second is either U+003A (:) or
        // U+007C (|).
        return input.size() == 2 && (input[1] == ':' || input[1] == '|') &&
               (is_ascii_alpha(input[0]));
    }

    constexpr bool starts_with_windows_drive_letter(std::string_view input) {
        // A string starts with a Windows drive letter if all of the
        // following are true: its length is greater than or equal to 2
        // its first two code points are a Windows drive letter its
        // length is 2 or its third code point is U+002F (/), U+005C
        // (\), U+003F (?), or U+0023 (#).
        if (input.size() < 2) {
            return false;
        }
        if (!is_windows_drive_letter(input.substr(0, 2))) {
            return false;
        }
        return input.size() == 2 || input[2] == '/' || input[2] == '\\' ||
               input[2] == '?' || input[2] == '#';
    }

    constexpr bool is_windows_normalized_drive_letter(std::string_view input) {
        // A normalized Windows drive letter is a Windows drive letter
        // of which the second code point is U+003A (:).
        return input.size() == 2 && input[1] == ':' &&
               (is_ascii_alpha(input[0]));
    }

    bool is_signle_dot_path_segment(std::string_view input) {
        // single-dot URL path segment is a URL path segment that is "."
        // or an ASCII case-insensitive match for "%2e"
        return input == "." || iequal(input, "%2e");
    }

    bool is_double_dot_path_segment(std::string_view input) {
        // A double-dot URL path segment is a URL path segment that is
        // ".." or an ASCII case-insensitive match for ".%2e", "%2e.",
        // or "%2e%2e"
        return input == ".." || iequal(input, ".%2e") ||
               iequal(input, "%2e.") || iequal(input, "%2e%2e");
    }

    void shorten_url_path(std::string_view scheme,
                          std::vector<std::string>& path) {
        // 1 Assert: url does not have an opaque path. (done)
        // 2 Let path be url's path.
        // 3 If url's scheme is "file", path's size is 1, and path[0]
        // is a normalized Windows drive letter, then return.
        if (path.size() == 1 && is_windows_normalized_drive_letter(path[0]) &&
            scheme == "file") {
            return;
        }
        // 4 Remove path's last item, if any.
        if (!path.empty()) {
            path.pop_back();
        }
    }

    void split_url_path(std::string_view path_string,
                        std::vector<std::string>& path) {
        path.clear();
        if (path_string.empty()) {
            return;
        }
        if (!path_string.empty() && path_string.front() == '/') {
            path_string.remove_prefix(1);
        }
        if (path_string.empty()) {
            path.emplace_back();
            return;
        }
        for (auto segment : path_string | split("/")) {
            path.emplace_back(segment);
        }
    }

    // The URL path serializer takes a URL url and then runs these steps.
    // They return an ASCII string.
    std::string url_path_serializer(std::span<const std::string> path) {
        // 1 If url has an opaque path, then return url's path (false)
        // 2 Let output be the empty string.
        std::string output;
        // 3 For each segment of url's path: append U+002F (/) followed
        // by segment to output.
        for (const auto& segment : path) {
            output += '/';
            output += segment;
        }
        // 4 Return output.
        return output;
    }

    struct url_pointer {
        const char* next = nullptr;
        std::size_t remaining_size = 0;

        const char* current = nullptr;
        const char* end = nullptr;

        std::string_view original_input;

        url_pointer(std::string_view input) {
            original_input = input;
            restart();
        }

        codepoint_t advance(std::error_code& ec) noexcept {
            assert(!is_eof());
            if (is_eof()) {
                ec = std::make_error_code(std::errc::invalid_argument);
                return 0;
            }
            if (remaining_size == 0) {
                next = end + 1;
                return 0;
            }
            current = next;
            std::errc e{};
            codepoint_t c = utf8_codecvt::decode(next, remaining_size, e);
            if (e != std::errc{}) {
                ec = make_error(url_error_code::invalid_url_unit);
                return 0;
            }
            return c;
        }

        bool is_eof() const noexcept {
            return next > end;
        }

        void restart() noexcept {
            current = next = original_input.data();
            remaining_size = original_input.size();
            end = current + remaining_size;
        }

        void decrease() noexcept {
            next = is_eof() ? end : current;
            remaining_size = end - next;
        }

        void decrease_by_bytes_plus_1codepoint(std::size_t nbytes) noexcept {
            // 1 code point
            if (is_eof()) {
                current = next = end;
            }
            else {
                decrease();
            }
            // by n bytes
            next -= nbytes;
            current -= nbytes;
            remaining_size += nbytes;
            assert(remaining_size <= original_input.size());
        }

        void increase() noexcept {
            assert(!is_eof());
            assert(remaining_size > 0);
            assert(static_cast<uint8_t>(*next) <= 127);
            current = next;
            next += 1;
            remaining_size -= 1;
        }

        std::string_view encoded_codepoint() const noexcept {
            assert(!is_eof());
            return std::string_view{current, next};
        }

        std::string_view remaining() const noexcept {
            return std::string_view{next, remaining_size};
        }

        std::string_view codepoint_substring() const noexcept {
            return std::string_view{current, end};
        }
    };

    struct url_parse_context {
        std::string buffer;
        std::vector<std::string> path;
        bool at_sign_seen = false;
        bool inside_brackets = false;
        bool password_token_seen = false;
    };

    std::string parse_opaque_host(std::string_view input, std::error_code& ec) {
        if (input.empty()) {
            return {};
        }
        const char* p = input.data();
        std::size_t n = input.size();
        while (n > 0) {
            const codepoint_t c = utf8_codecvt::decode_unchecked(p, n);
            // 1 If input contains a forbidden host code point,
            // host-invalid-code-point validation error, return
            // failure.
            if (is_forbidden_host_code_point(c)) {
                ec = make_error(url_error_code::host_invalid_code_point);
                return {};
            }
            // 2 If input contains a code point that is not a URL
            // code point and not U+0025 (%), invalid-URL-unit
            // validation error.
            if (c != '%' && !is_url_codepoint(c)) {
                // non fatal error!
                // ec =
                // make_error(url_error_code::invalid_url_unit);
                // return {};
            }
            // 3 If input contains a U+0025 (%) and the two code
            // points following it are not ASCII hex digits,
            // invalid-URL-unit validation error
            if (c == '%') {
                if (n >= 2) {
                    if (is_hex_digit(*p) && is_hex_digit(*(p + 1))) {
                        p += 2;
                        n -= 2;
                        continue;
                    }
                }
                // non fatal error!
                // ec =
                // make_error(url_error_code::invalid_url_unit);
                // return {};
            }
        }
        // 4 Return the result of running UTF-8 percent-encode on input
        // using the C0 control percent-encode set.
        std::string host;
        percent_encode(input, c0_control_percent_encode_set,
                       dynamic_buffer(host), ec);
        return host;
    }

    bool url_domain_to_ascii(std::string& domain, bool be_strict,
                             std::error_code& ec) {
        // 1 Let result be the result of running Unicode ToASCII with
        // domain_name set to domain, CheckHyphens set to beStrict,
        // CheckBidi set to true, CheckJoiners set to true,
        // UseSTD3ASCIIRules set to beStrict, Transitional_Processing
        // set to false, VerifyDnsLength set to beStrict, and
        // IgnoreInvalidPunycode set to false. [UTS46]
        const bool res =
            idna::domain_to_ascii(domain, be_strict, true, true, be_strict,
                                  false, be_strict, false, ec);
        // 2 If result is a failure value, domain-to-ASCII validation
        // error, return failure.
        if (!res) {
            ec = make_error(url_error_code::domain_to_ascii);
            return false;
        }
        // 2 If beStrict is false:
        if (!be_strict) {
            // 1 If result is the empty string, domain-to-ASCII
            // validation error, return failure.
            if (domain.empty()) {
                ec = make_error(url_error_code::domain_to_ascii);
                return false;
            }
            // 2 If result contains a forbidden domain code point,
            // domain-invalid-code-point validation error, return
            // failure.
            if (contains_forbidden_domain_code_point(domain)) {
                ec = make_error(url_error_code::domain_invalid_code_point);
                return false;
            }
        }
        // 4 Assert: result is not the empty string and does not contain
        // a forbidden domain code point.
        assert(!domain.empty() &&
               !contains_forbidden_domain_code_point(domain));
        // 5 Return result.
        return true;
    }

    bool parse_host(std::string_view input, bool is_opaque, url_record& url,
                    std::error_code& ec) {
        if (input.empty()) {
            url.set_has_empty_host();
            url.host.clear();
            return true;
        }
        // 1 If input starts with U+005B ([), then:
        if (input.front() == '[') {
            // 1 If input does not end with U+005D (]),
            // IPv6-unclosed validation error, return failure.
            if (input.size() == 1 || input.back() != ']') {
                ec = make_error(url_error_code::ipv6_unclosed);
                return false;
            }
            // 2 Return the result of IPv6 parsing input with its
            // leading U+005B
            // ([) and trailing U+005D (]) removed.
            input.remove_prefix(1);
            input.remove_suffix(1);
            ipv6 ipv6_number;
            if (!ipv6_number.from_string(input)) {
                ec = make_error(url_error_code::ipv6_invalid);
                return false;
            }
            url.set_has_ipv6_host();
            // url.host = ipv6_number.to_string();
            url.host = url_serialize_ipv6(ipv6_number);
            return true;
        }
        // 2 If isOpaque is true, then return the result of opaque-host
        // parsing input.
        if (is_opaque) {
            std::string host = parse_opaque_host(input, ec);
            if (!ec) {
                url.set_has_opaque_host();
                url.host = std::move(host);
                return true;
            }
            return false;
        }
        // 3 Assert: input is not the empty string.
        assert(!input.empty());
        // 4 Let domain be the result of running UTF-8 decode without
        // BOM on the percent-decoding of input.
        std::string domain;
        percent_decode(input, dynamic_buffer(domain), ec);
        if (ec) {
            // non fatal error!
            ec.clear();
            domain = input;
        }
        if (ec) {
            return false;
        }
        // 5 Let asciiDomain be the result of running domain to ASCII
        // with domain and false. 6 If asciiDomain is failure, then
        // return failure.
        if (!url_domain_to_ascii(domain, false, ec)) {
            return false;
        }
        // 7 If asciiDomain ends in a number, then return the result of
        // IPv4 parsing asciiDomain.
        if (url_domain_ends_in_number(domain)) {
            auto result_ipv4 = parse_url_ipv4_host(domain, ec);
            if (!ec) {
                url.set_has_ipv4_host();
                url.host = result_ipv4.to_string();
                return true;
            }
            return false;
        }

        // 8 Return asciiDomain.
        url.set_has_domain_host();
        url.host = std::move(domain);
        return true;
    }

    void
    remove_leading_and_trailing_c0_and_space(std::string_view& input) noexcept {
        while (!input.empty() && is_c0_or_space_ch(input.front())) {
            input.remove_prefix(1);
        }
        while (!input.empty() && is_c0_or_space_ch(input.back())) {
            input.remove_suffix(1);
        }
    }

    void remove_tabs_and_newlines(std::string& input) noexcept {
        for (auto it = input.begin(); it != input.end();) {
            if (is_ascii_tab_or_newline(*it)) {
                it = input.erase(it);
            }
            else {
                ++it;
            }
        }
    }

    bool scheme_start_state(codepoint_t c, url_pointer& pointer,
                            url_parse_context& ctx, parse_state& state,
                            std::error_code& ec) {
        // 1 If c is an ASCII alpha, append c, lowercased, to buffer,
        // and set state to scheme state.
        if (is_ascii_alpha(c)) {
            ctx.buffer += get_ascii_lowercase(c);
            state = parse_state::scheme;
            return true;
        }
        // 2 Otherwise, if state override is not given,
        // set state to no scheme state and decrease pointer by 1
        else {
            pointer.decrease();
            state = parse_state::no_scheme;
            return true;
        }
        // 3 Otherwise, return failure.
        // state override is not given
        assert(false);
    }

    bool scheme_state(codepoint_t c, url_pointer& pointer, url_record& url,
                      const url_record* base, url_parse_context& ctx,
                      parse_state& state, std::error_code& ec) {
        // 1 If c is an ASCII alphanumeric, U+002B (+), U+002D (-), or
        // U+002E
        // (.), append c, lowercased, to buffer.
        if (is_ascii_alphanumeric(c) || c == '+' || c == '-' || c == '.') {
            ctx.buffer += get_ascii_lowercase(c);
            return true;
        }
        // 2 Otherwise, if c is U+003A (:), then:
        else if (c == ':') {
            // 1 If state override is given, then:
            // skip, state is not given.

            // 2 Set url's scheme to buffer.
            url.scheme = ctx.buffer;
            if (is_special_scheme(url.scheme)) {
                url.set_special();
            }
            // 3 If state override is given, then: (not given.)

            // 4 Set buffer to the empty string.
            ctx.buffer.clear();
            // 5 If url's scheme is "file", then:
            if (url.scheme == "file") {
                // 1 If remaining does not start with "//",
                // special-scheme-missing-following-solidus
                // validation error.
                if (!pointer.remaining().starts_with("//")) {
                    // non fatal error!
                    // ec =
                    // make_error(url_error_code::special_scheme_missing_following_solidus);
                    // return false;
                }
                // 2 Set state to file state.
                state = parse_state::file;
                return true;
            }
            // 6 Otherwise, if url is special, base is non-null, and
            // base's scheme is url's scheme:
            else if (base != nullptr && url.is_special() &&
                     base->scheme == url.scheme) {
                // 1 Assert: base is special (and therefore does
                // not have an opaque path).
                assert(base->is_special());
                // 2 Set state to special relative or authority
                // state.
                state = parse_state::special_relative_or_authority;
                return true;
            }
            // 7 Otherwise, if url is special, set state to special
            // authority slashes state.
            else if (url.is_special()) {
                state = parse_state::special_authority_slashes;
                return true;
            }
            // 8 Otherwise, if remaining starts with an U+002F (/),
            // set state to path or authority state and increase
            // pointer by
            // 1
            else if (pointer.remaining().starts_with("/")) {
                state = parse_state::path_or_authority;
                pointer.increase();
                return true;
            }
            // 9 Otherwise, set url's path to the empty string and
            // set state to opaque path state.
            else {
                url.set_has_opaque_path();
                state = parse_state::opaque_path;
                return true;
            }
        }
        // Otherwise, if state override is not given, set buffer to the
        // empty string, state to no scheme state, and start over (from
        // the first code point in input).
        ctx.buffer.clear();
        state = parse_state::no_scheme;
        pointer.restart();
        return true;
        // Otherwise, return failure.
        // state override is not given
    }

    bool no_scheme_state(codepoint_t c, url_pointer& pointer, url_record& url,
                         const url_record* base, parse_state& state,
                         std::error_code& ec) {
        // 1 If base is null, or base has an opaque path and c is not
        // U+0023
        // (#), missing-scheme-non-relative-URL validation error, return
        // failure.
        if (base == nullptr || (base->is_opaque_path() && c != '#')) {
            ec = make_error(url_error_code::missing_scheme_non_relative_url);
            return false;
        }
        // 2 Otherwise, if base has an opaque path and c is U+0023 (#),
        // set url's scheme to base's scheme, url's path to base's path,
        // url's query to base's query, url's fragment to the empty
        // string, and set state to fragment state.
        if (base->is_opaque_path() && c == '#') {
            url.scheme = base->scheme;
            url.path = base->path;
            url.query = base->query;
            url.fragment.clear();
            url.flags = base->flags;
            url.clear_host_flags();
            url.clear_port_flag();
            url.set_has_fragment();
            state = parse_state::fragment;
        }
        // 3 Otherwise, if base's scheme is not "file",
        // set state to relative state and decrease pointer by 1.
        else if (base->scheme != "file") {
            state = parse_state::relative;
            pointer.decrease();
        }
        // 4 Otherwise, set state to file state and decrease pointer
        // by 1.
        else {
            state = parse_state::file;
            pointer.decrease();
        }
        return true;
    }

    bool special_relative_or_authority_state(codepoint_t c,
                                             url_pointer& pointer,
                                             parse_state& state) {
        // only entered if base is non null
        // 1 If c is U+002F (/) and remaining starts with U+002F (/),
        // then set state to special authority ignore slashes state and
        // increase pointer by 1.
        if (c == '/' && pointer.remaining().starts_with('/')) {
            state = parse_state::special_authority_ignore_slashes;
            pointer.increase();
        }
        // 2 Otherwise, special-scheme-missing-following-solidus
        // validation error, set state to relative state and decrease
        // pointer by 1.
        else {
            // special-scheme-missing-following-solidus is non fatal
            // error!
            state = parse_state::relative;
            pointer.decrease();
        }
        return true;
    }

    bool path_or_authority_state(codepoint_t c, url_pointer& pointer,
                                 parse_state& state) {
        // 1 If c is U+002F (/), then set state to authority state.
        if (c == '/') {
            state = parse_state::authority;
            return true;
        }
        // 2 Otherwise, set state to path state, and decrease pointer
        // by 1.
        else {
            state = parse_state::path;
            pointer.decrease();
            return true;
        }
    }

    bool relative_state(codepoint_t c, url_pointer& pointer, url_record& url,
                        const url_record* base, url_parse_context& ctx,
                        parse_state& state, std::error_code& ec) {
        // only entered if base is non null
        // 1 Assert: base's scheme is not "file".
        assert(base != nullptr && base->scheme != "file");
        // 2 Set url's scheme to base's scheme.
        url.scheme = base->scheme;
        url.flags |= base->flags & url_record_flags::special_scheme;
        // 3 If c is U+002F (/), then set state to relative slash state.
        if (c == '/') {
            state = parse_state::relative_slash;
            return true;
        }
        // 4 Otherwise, if url is special and c is U+005C (\),
        // invalid-reverse-solidus validation error, set state to
        // relative slash state.
        else if (url.is_special() && c == '\\') {
            // invalid-reverse-solidus is non fatal error!
            state = parse_state::relative_slash;
            return true;
        }
        // 5 Otherwise:
        else {
            // 1 Set url's username to base's username, url's
            // password to base's password, url's host to base's
            // host, url's port to base's port, url's path to a
            // clone of base's path, and url's query to base's
            // query.
            url.userinfo = base->userinfo;
            url.password_pos = base->password_pos;
            url.host = base->host;
            url.port = base->port;
            split_url_path(base->path, ctx.path);
            url.query = base->query;
            url.clear_host_flags();
            url.flags |=
                base->flags & ~(url_record_flags::fragment |
                                url_record_flags::empty_first_path_segment);
            // 2 If c is U+003F (?), then set url's query to the
            // empty string, and state to query state.
            if (c == '?') {
                url.set_has_query();
                url.query.clear();
                state = parse_state::query;
            }
            // 3 Otherwise, if c is U+0023 (#), set url's fragment
            // to the empty string and state to fragment state.
            else if (c == '#') {
                url.set_has_fragment();
                url.fragment.clear();
                state = parse_state::fragment;
            }
            // 4 Otherwise, if c is not the EOF code point:
            else if (!pointer.is_eof()) {
                // 1 Set url's query to null.
                url.clear_has_query();
                url.query.clear();
                // 2 Shorten url's path.
                shorten_url_path(url.scheme, ctx.path);
                // 3 Set state to path state and decrease
                // pointer by 1.
                state = parse_state::path;
                pointer.decrease();
            }
            return true;
        }
    }

    bool relative_slash_state(codepoint_t c, url_pointer& pointer,
                              url_record& url, const url_record* base,
                              url_parse_context& ctx, parse_state& state,
                              std::error_code& ec) {
        // only entered if base is non null
        // 1 If url is special and c is U+002F (/) or U+005C (\), then:
        if (url.is_special() && (c == '/' || c == '\\')) {
            // 1 If c is U+005C (\), invalid-reverse-solidus
            // validation error. non fatal error! 2 Set state to
            // special authority ignore slashes state.
            state = parse_state::special_authority_ignore_slashes;
        }
        // 2 Otherwise, if c is U+002F (/), then set state to authority
        // state.
        else if (c == '/') {
            state = parse_state::authority;
        }
        // Otherwise, set url's username to base's username, url's
        // password to base's password, url's host to base's host, url's
        // port to base's port, state to path state, and then, decrease
        // pointer by 1.
        else {
            url.userinfo = base->userinfo;
            url.password_pos = base->password_pos;
            url.host = base->host;
            url.port = base->port;
            url.flags |=
                base->flags &
                ~(url_record_flags::opaque_path |
                  url_record_flags::empty_first_path_segment |
                  url_record_flags::query | url_record_flags::fragment);
            state = parse_state::path;
            pointer.decrease();
        }
        return true;
    }

    bool special_authority_slashes_state(codepoint_t c, url_pointer& pointer,
                                         parse_state& state,
                                         std::error_code& ec) {
        // 1 If c is U+002F (/) and remaining starts with U+002F (/),
        // then set state to special authority ignore slashes state and
        // increase pointer by 1.
        if (c == '/' && pointer.remaining().starts_with("/")) {
            state = parse_state::special_authority_ignore_slashes;
            pointer.increase();
            return true;
        }
        // 2 Otherwise, special-scheme-missing-following-solidus
        // validation error, set state to special authority ignore
        // slashes state and decrease pointer by 1. non fatal error! ec
        // =
        // make_error(url_error_code::special_scheme_missing_following_solidus);
        state = parse_state::special_authority_ignore_slashes;
        pointer.decrease();
        return true;
    }

    bool special_authority_ignore_slashes_state(codepoint_t c,
                                                url_pointer& pointer,
                                                parse_state& state,
                                                std::error_code& ec) {
        // 1 If c is neither U+002F (/) nor U+005C (\),
        // then set state to authority state and decrease pointer by 1.
        if (c != '/' && c != '\\') {
            state = parse_state::authority;
            pointer.decrease();
            return true;
        }
        // 2 Otherwise, special-scheme-missing-following-solidus
        // validation error. non fatal error! ec =
        // make_error(url_error_code::special_scheme_missing_following_solidus);
        return true;
    }

    bool authority_state(codepoint_t c, url_pointer& pointer, url_record& url,
                         url_parse_context& ctx, parse_state& state,
                         std::error_code& ec) {
        // 1 If c is U+0040 (@), then:
        if (c == '@') {
            // 1 Invalid-credentials validation error. ??
            // 2 If atSignSeen is true, then prepend "%40" to
            // buffer. note: prepend not append!
            if (ctx.at_sign_seen) {
                ctx.buffer.insert(0, "%40");
            }
            // 3 Set atSignSeen to true.
            ctx.at_sign_seen = true;
            // 4 For each codePoint in buffer:
            get_username_and_password_from_buffer(ctx.buffer, url,
                                                  ctx.password_token_seen);
            // 5 Set buffer to the empty string.
            ctx.buffer.clear();
            return true;
        }
        // 2 Otherwise, if one of the following is true:
        // c is the EOF code point, U+002F (/), U+003F (?), or U+0023
        // (#) url is special and c is U+005C (\)
        else if ((pointer.is_eof() || c == '/' || c == '?' || c == '#') ||
                 (c == '\\' && is_special_scheme(url.scheme))) {
            // 1 If atSignSeen is true and buffer is the empty
            // string, host-missing validation error, return
            // failure.
            if (ctx.at_sign_seen && ctx.buffer.empty()) {
                ec = make_error(url_error_code::host_missing);
                return false;
            }
            // 2 Decrease pointer by buffer's code point length + 1,
            // set buffer to the empty string, and set state to host
            // state.
            pointer.decrease_by_bytes_plus_1codepoint(ctx.buffer.size());
            ctx.buffer.clear();
            state = parse_state::host;
            return true;
        }
        // 3 Otherwise, append c to buffer.
        ctx.buffer.append(pointer.encoded_codepoint());
        return true;
    }

    bool host_state(codepoint_t c, url_pointer& pointer, url_record& url,
                    url_parse_context& ctx, parse_state& state,
                    std::error_code& ec) {
        // 1 If state override is given and url's scheme is "file",
        // then decrease pointer by 1 and set state to file host state.
        // state override is not given

        // 2 Otherwise, if c is U+003A (:) and insideBrackets is false:
        if (c == ':' && !ctx.inside_brackets) {
            // 1 If buffer is the empty string, host-missing
            // validation error, return failure.
            if (ctx.buffer.empty()) {
                ec = make_error(url_error_code::host_missing);
                return false;
            }
            // 2 If state override is given and state override is
            // hostname state, then return failure. state override
            // is not given 3 Let host be the result of host parsing
            // buffer with url is not special.
            const bool host_result =
                parse_host(ctx.buffer, !is_special_scheme(url.scheme), url, ec);
            // 4 If host is failure, then return failure.
            if (!host_result) {
                return false;
            }
            // 5 Set url's host to host, buffer to the empty string,
            // and state to port state.
            state = parse_state::port;
            ctx.buffer.clear();
            return true;
        }
        // 3 Otherwise, if one of the following is true:
        // c is the EOF code point, U+002F (/), U+003F (?), or U+0023
        // (#) url is special and c is U+005C (\)
        if ((pointer.is_eof() || c == '/' || c == '?' || c == '#') ||
            (c == '\\' && is_special_scheme(url.scheme))) {
            // then decrease pointer by 1, and:
            pointer.decrease();
            // 1 If url is special and buffer is the empty string,
            // host-missing validation error, return failure.
            if (ctx.buffer.empty() && is_special_scheme(url.scheme)) {
                ec = make_error(url_error_code::host_missing);
                return false;
            }
            // 2 Otherwise, if state override is given, buffer is
            // the empty string, and either url includes credentials
            // or url's port is non-null, then return failure. state
            // override is not given 3 Let host be the result of
            // host parsing buffer with url is not special.
            const bool host_result =
                parse_host(ctx.buffer, !is_special_scheme(url.scheme), url, ec);
            // 4 If host is failure, then return failure.
            if (!host_result) {
                return false;
            }
            // 5 Set url's host to host, buffer to the empty string,
            // and state to path start state.
            ctx.buffer.clear();
            state = parse_state::path_start;
            return true;
            // 6 If state override is given, then return.
            // state override is not given
        }
        // 3 Otherwise:
        // 1 If c is U+005B ([), then set insideBrackets to true.
        if (c == '[') {
            ctx.inside_brackets = true;
        }
        // 2 If c is U+005D (]), then set insideBrackets to false.
        if (c == ']') {
            ctx.inside_brackets = false;
        }
        // 3 Append c to buffer.
        ctx.buffer.append(pointer.encoded_codepoint());
        return true;
    }

    bool port_state(codepoint_t c, url_pointer& pointer, url_record& url,
                    url_parse_context& ctx, parse_state& state,
                    std::error_code& ec) {
        // If c is an ASCII digit, append c to buffer.
        if (is_ascii_numeric(c)) {
            ctx.buffer.append(pointer.encoded_codepoint());
            return true;
        }
        // 2 Otherwise, if one of the following is true:
        // c is the EOF code point, U+002F (/), U+003F (?), or U+0023
        // (#); url is special and c is U+005C (\); or state override is
        // given
        if ((pointer.is_eof() || c == '/' || c == '?' || c == '#') ||
            (c == '\\' && is_special_scheme(url.scheme))) {
            // 1 If buffer is not the empty string:
            if (!ctx.buffer.empty()) {
                // 1 Let port be the mathematical integer value
                // that is represented by buffer in radix-10
                // using ASCII digits for digits with values 0
                // through 9.
                const uint16_t port = to_uint16(ctx.buffer, 10, ec);
                // 2 If port is not a 16-bit unsigned integer,
                // port-out-of-range validation error, return
                // failure.
                if (ec) {
                    ec = make_error(url_error_code::port_out_of_range);
                    return false;
                }
                // 3 Set url's port to null, if port is url's
                // scheme's default port; otherwise to port.
                if (default_scheme_port(url.scheme) == port) {
                    url.clear_port_flag();
                }
                else {
                    url.set_has_port();
                    url.port = port;
                }
                // 4 Set buffer to the empty string.
                ctx.buffer.clear();
                // 5 If state override is given, then return.
                // (not given)
            }
            // 2 If state override is given, then return failure.
            // (not given) 3 Set state to path start state and
            // decrease pointer by 1.
            pointer.decrease();
            state = parse_state::path_start;
            return true;
        }
        // 3 Otherwise, port-invalid validation error, return failure.
        else {
            ec = make_error(url_error_code::port_invalid);
            return false;
        }
    }

    bool file_state(codepoint_t c, url_pointer& pointer, url_record& url,
                    const url_record* base, url_parse_context& ctx,
                    parse_state& state, std::error_code& ec) {
        // 1 Set url's scheme to "file".
        url.scheme = "file";
        url.set_special();
        // 2 Set url's host to the empty string.
        url.set_has_empty_host();
        url.host.clear();
        // 3 If c is U+002F (/) or U+005C (\), then:
        if (c == '/' || c == '\\') {
            // 1 If c is U+005C (\), invalid-reverse-solidus
            // validation error.
            if (c == '\\') {
                // non fatal error!
                // ec =
                // make_error(url_error_code::invalid_reverse_solidus);
                // return false;
            }
            // 2 Set state to file slash state.
            state = parse_state::file_slash;
            return true;
        }
        // 4 Otherwise, if base is non-null and base's scheme is "file":
        else if (base != nullptr && base->scheme == "file") {
            // 1 Set url's host to base's host, url's path to a
            // clone of base's path, and url's query to base's
            // query.
            url.host = base->host;
            url.query = base->query;
            split_url_path(base->path, ctx.path);
            url.flags |=
                base->flags &
                ~(url_record_flags::username | url_record_flags::password |
                  url_record_flags::fragment | url_record_flags::port |
                  url_record_flags::special_scheme |
                  url_record_flags::empty_first_path_segment);
            // 2 If c is U+003F (?), then set url's query to the
            // empty string and state to query state.
            if (c == '?') {
                url.query.clear();
                url.set_has_query();
                state = parse_state::query;
            }
            // 3 Otherwise, if c is U+0023 (#), set url's fragment
            // to the empty string and state to fragment state.
            else if (c == '#') {
                url.fragment.clear();
                url.set_has_fragment();
                state = parse_state::fragment;
            }
            // 4 Otherwise, if c is not the EOF code point:
            else if (!pointer.is_eof()) {
                // 1 Set url's query to null.
                url.query.clear();
                url.clear_has_query();
                // 2 If the code point substring from pointer to
                // the end of input does not start with a
                // Windows drive letter, then shorten url's
                // path.
                if (!starts_with_windows_drive_letter(
                        pointer.codepoint_substring())) {
                    shorten_url_path(url.scheme, ctx.path);
                }
                // 3 Otherwise:
                else {
                    // 1 File-invalid-Windows-drive-letter
                    // validation error. (non fatal error) 2
                    // Set url's path to � �.
                    ctx.path.clear();
                    // note: This is a
                    // (platform-independent) Windows drive
                    // letter quirk.
                }
                // 4 Set state to path state and decrease
                // pointer by 1.
                state = parse_state::path;
                pointer.decrease();
            }
            return true;
        }
        // 5 Otherwise, set state to path state, and decrease pointer by
        // 1
        else {
            state = parse_state::path;
            pointer.decrease();
            return true;
        }
    }

    bool file_slash_state(codepoint_t c, url_pointer& pointer, url_record& url,
                          const url_record* base, url_parse_context& ctx,
                          parse_state& state, std::error_code& ec) {
        // 1 If c is U+002F (/) or U+005C (\), then:
        if (c == '/' || c == '\\') {
            // 1 If c is U+005C (\), invalid-reverse-solidus
            // validation error.
            if (c == '\\') {
                // non fatal error!
                // ec =
                // make_error(url_error_code::invalid_reverse_solidus);
                // return false;
            }
            // 2 Set state to file host state.
            state = parse_state::file_host;
            return true;
        }
        // 2 Otherwise:
        else {
            // 1 If base is non-null and base's scheme is "file",
            // then:
            if (base != nullptr && base->scheme == "file") {
                // 1 Set url's host to base's host.
                url.host = base->host;
                // 2 If the code point substring from pointer to
                // the end of input does not start with a
                // Windows drive letter and base's path[0] is a
                // normalized Windows drive letter, then append
                // base's path[0] to url's path.
                std::vector<std::string> base_path;
                split_url_path(base->path, base_path);
                if (!starts_with_windows_drive_letter(
                        pointer.codepoint_substring()) &&
                    !base_path.empty() &&
                    is_windows_normalized_drive_letter(base_path[0])) {
                    ctx.path.emplace_back(std::move(base_path[0]));
                }
            }
            // 2 Set state to path state, and decrease pointer by 1.
            state = parse_state::path;
            pointer.decrease();
            return true;
        }
    }

    bool file_host_state(codepoint_t c, url_pointer& pointer, url_record& url,
                         url_parse_context& ctx, parse_state& state,
                         std::error_code& ec) {
        // 1 If c is the EOF code point, U+002F (/), U+005C (\), U+003F
        // (?), or U+0023 (#), then decrease pointer by 1 and then:
        if (pointer.is_eof() || c == '/' || c == '\\' || c == '?' || c == '#') {
            pointer.decrease();
            // 1 If state override is not given and buffer is a
            // Windows drive letter,
            // file-invalid-Windows-drive-letter-host validation
            // error, set state to path state.
            if (is_windows_drive_letter(ctx.buffer)) {
                // This is a (platform-independent) Windows
                // drive letter quirk. buffer is not reset here
                // and instead used in the path state.
                state = parse_state::path;
                return true;
            }
            // 2 Otherwise, if buffer is the empty string, then:
            else if (ctx.buffer.empty()) {
                // 1 Set url's host to the empty string.
                url.set_has_empty_host();
                url.host.clear();
                // 2 If state override is given, then return.
                // (not given) 3 Set state to path start state.
                state = parse_state::path_start;
                return true;
            }
            // 3 Otherwise, run these steps:
            else {
                // 1 Let host be the result of host parsing
                // buffer with url is not special. 2 If host is
                // failure, then return failure.
                const bool host_res = parse_host(
                    ctx.buffer, !is_special_scheme(url.scheme), url, ec);
                if (!host_res) {
                    return false;
                }
                assert(url.has_host());
                // 3 If host is "localhost", then set host to
                // the empty string.
                if (url.host == "localhost") {
                    url.set_has_empty_host();
                    url.host.clear();
                }
                // 4 Set url's host to host. (done)
                // 5 If state override is given, then return.
                // (not given) 6 Set buffer to the empty string
                // and state to path start state.
                ctx.buffer.clear();
                state = parse_state::path_start;
                return true;
            }
        }
        // 2 Otherwise, append c to buffer.
        else {
            ctx.buffer.append(pointer.encoded_codepoint());
            return true;
        }
    }

    bool path_start_state(codepoint_t c, url_pointer& pointer, url_record& url,
                          parse_state& state, std::error_code& ec) {
        // 1 If url is special, then:
        if (is_special_scheme(url.scheme)) {
            // 1 If c is U+005C (\), invalid-reverse-solidus
            // validation error.
            if (c == '\\') {
                // non fatal error!
                // ec =
                // make_error(url_error_code::invalid_reverse_solidus);
                // return false;
            }
            // 2 Set state to path state.
            state = parse_state::path;
            // 3 If c is neither U+002F (/) nor U+005C (\), then
            // decrease pointer by 1.
            if (c != '/' && c != '\\') {
                pointer.decrease();
            }
            return true;
        }
        // 2 Otherwise, if state override is not given and c is U+003F
        // (?), set url's query to the empty string and state to query
        // state.
        else if (c == '?') {
            url.set_has_query();
            url.query.clear();
            state = parse_state::query;
            return true;
        }
        // 3 Otherwise, if state override is not given and c is U+0023
        // (#), set url's fragment to the empty string and state to
        // fragment state.
        else if (c == '#') {
            url.set_has_fragment();
            url.fragment.clear();
            state = parse_state::fragment;
            return true;
        }
        // 4 Otherwise, if c is not the EOF code point:
        else if (!pointer.is_eof()) {
            // 1 Set state to path state.
            state = parse_state::path;
            // 2 If c is not U+002F (/), then decrease pointer by 1.
            if (c != '/') {
                pointer.decrease();
            }
            return true;
        }
        // 5 Otherwise, if state override is given and url's host is
        // null, append the empty string to url's path.
        else {
            return true;
        }
    }

    bool path_state(codepoint_t c, url_pointer& pointer, url_record& url,
                    url_parse_context& ctx, parse_state& state,
                    std::error_code& ec) {
        // 1 If one of the following is true:
        // c is the EOF code point or U+002F (/)
        // url is special and c is U+005C (\)
        // state override is not given and c is U+003F (?) or U+0023 (#)
        if ((pointer.is_eof() || c == '/') ||
            (c == '\\' && is_special_scheme(url.scheme)) ||
            (c == '?' || c == '#')) {
            // 1 If url is special and c is U+005C (\),
            // invalid-reverse-solidus validation error.
            if (c == '\\' && is_special_scheme(url.scheme)) {
                // non fatal error!
                // ec =
                // make_error(url_error_code::invalid_reverse_solidus);
                // return false;
            }
            // 2 If buffer is a double-dot URL path segment, then:
            if (is_double_dot_path_segment(ctx.buffer)) {
                // 1 Shorten url's path.
                shorten_url_path(url.scheme, ctx.path);
                // 2 If neither c is U+002F (/), nor url is
                // special and c is U+005C (\), append the empty
                // string to url's path. This means that for
                // input /usr/.. the result is / and not a lack
                // of a path.
                if (c != '/') {
                    // If url is special and c is U+005C (\)
                    // is false.
                    ctx.path.emplace_back();
                }
            }
            // 3 Otherwise, if buffer is a single-dot URL path
            // segment and if neither c is U+002F (/), nor url is
            // special and c is U+005C (\), append the empty string
            // to url's path.
            else if (is_signle_dot_path_segment(ctx.buffer) && c != '/') {
                // If url is special and c is U+005C (\) is
                // false.
                ctx.path.emplace_back();
            }
            // 4 Otherwise, if buffer is not a single-dot URL path
            // segment, then:
            else if (!is_signle_dot_path_segment(ctx.buffer)) {
                // 1 If url's scheme is "file", url's path is
                // empty, and buffer is a Windows drive letter,
                // then replace the second code point in buffer
                // with U+003A (:). This is a
                // (platform-independent) Windows drive letter
                // quirk.
                if (url.scheme == "file" && ctx.path.empty() &&
                    is_windows_drive_letter(ctx.buffer)) {
                    ctx.buffer[1] = ':';
                }
                // 2 Append buffer to url's path.
                ctx.path.emplace_back(ctx.buffer);
            }
            // 5 Set buffer to the empty string.
            ctx.buffer.clear();
            // 6 If c is U+003F (?), then set url's query to the
            // empty string and state to query state.
            if (c == '?') {
                url.set_has_query();
                url.query.clear();
                state = parse_state::query;
            }
            // 7 If c is U+0023 (#), then set url's fragment to the
            // empty string and state to fragment state.
            else if (c == '#') {
                url.set_has_fragment();
                url.fragment.clear();
                state = parse_state::fragment;
            }
            return true;
        }
        // 2 Otherwise, run these steps:
        else {
            // 1 If c is not a URL code point and not U+0025 (%),
            // invalid-URL-unit validation error.
            if (c != '%' && !is_url_codepoint(c)) {
                // non fatal error!
                // ec =
                // make_error(url_error_code::invalid_url_unit);
                // return false;
            }
            // 2 If c is U+0025 (%) and remaining does not start
            // with two ASCII hex digits, invalid-URL-unit
            // validation error.
            if (c == '%') {
                if (!starts_with_two_hex_digits(pointer.remaining())) {
                    // non fatal!
                    // ec =
                    // make_error(url_error_code::invalid_url_unit);
                    // return false;
                }
            }
            // 3 UTF-8 percent-encode c using the path
            // percent-encode set and append the result to buffer.
            percent_encode(pointer.encoded_codepoint(), path_percent_encode_set,
                           dynamic_buffer(ctx.buffer), ec);
            return !ec;
        }
    }

    bool opaque_path_state(codepoint_t c, url_pointer& pointer, url_record& url,
                           url_parse_context& ctx, parse_state& state,
                           std::error_code& ec) {
        // 1 If c is U+003F (?), then set url's query to the empty
        // string and state to query state.
        if (c == '?') {
            url.set_has_query();
            url.query.clear();
            state = parse_state::query;
            return true;
        }
        // 2 Otherwise, if c is U+0023 (#), then set url's fragment to
        // the empty string and state to fragment state.
        if (c == '#') {
            url.set_has_fragment();
            url.fragment.clear();
            state = parse_state::fragment;
            return true;
        }
        // 3 Otherwise, if c is U+0020 SPACE:
        if (c == ' ') {
            // 1 If remaining starts with U+003F (?) or U+003F (#),
            // then append "%20" to url's path
            auto remaining = pointer.remaining();
            if (remaining.size() > 1 &&
                (remaining[0] == '?' || remaining[0] == '#')) {
                url.path.append("%20");
            }
            // 2 Otherwise, append U+0020 SPACE to url's path.
            else {
                url.path += ' ';
            }
            return true;
        }
        // 4 Otherwise, if c is not the EOF code point:
        if (!pointer.is_eof()) {
            // 1 If c is not a URL code point and not U+0025 (%),
            // invalid-URL-unit validation error.
            if (c != '%' && !is_url_codepoint(c)) {
                // non fatal error!
                // ec =
                // make_error(url_error_code::invalid_url_unit);
                // return false;
            }
            // 2 If c is U+0025 (%) and remaining does not start
            // with two ASCII hex digits, invalid-URL-unit
            // validation error.
            if (c == '%') {
                if (!starts_with_two_hex_digits(pointer.remaining())) {
                    // non fatal error!
                    // ec =
                    // make_error(url_error_code::invalid_url_unit);
                    // return false;
                }
            }
            // 3 UTF-8 percent-encode c using the C0 control
            // percent-encode set and append the result to url's
            // path.
            percent_encode(pointer.encoded_codepoint(),
                           c0_control_percent_encode_set,
                           dynamic_buffer(url.path), ec);
        }
        return !ec;
    }

    bool query_state(codepoint_t c, url_pointer& pointer, url_record& url,
                     url_parse_context& ctx, parse_state& state,
                     std::error_code& ec) {
        // 1 If encoding is not UTF-8 and one of the following is true:
        // encoding is UTF-8

        // 2 If one of the following is true:
        // state override is not given and c is U+0023 (#)
        // c is the EOF code point
        if (c == '#' || pointer.is_eof()) {
            // 1 Let queryPercentEncodeSet be the special-query
            // percent-encode set if url is special; otherwise the
            // query percent-encode set. not implemented yet 2
            // Percent-encode after encoding, with encoding, buffer,
            // and queryPercentEncodeSet, and append the result to
            // url's query.
            assert(url.has_query());
            auto percent_encode_set =
                std::span{query_percent_encode_set.data(),
                          query_percent_encode_set.size()};
            if (is_special_scheme(url.scheme)) {
                percent_encode_set =
                    std::span{special_query_percent_encode_set};
            }
            percent_encode(ctx.buffer, percent_encode_set,
                           dynamic_buffer(url.query), ec);
            if (ec) {
                return false;
            }
            // 3 Set buffer to the empty string.
            ctx.buffer.clear();
            // 4 If c is U+0023 (#), then set url's fragment to the
            // empty string and state to fragment state.
            if (c == '#') {
                url.set_has_fragment();
                url.fragment.clear();
                state = parse_state::fragment;
            }
            return true;
        }
        // 3 Otherwise, if c is not the EOF code point:
        if (!pointer.is_eof()) {
            // 1 If c is not a URL code point and not U+0025 (%),
            // invalid-URL-unit validation error.
            if (c != '%' && !is_url_codepoint(c)) {
                // non fatal error!
                // ec =
                // make_error(url_error_code::invalid_url_unit);
                // return false;
            }
            // 2 If c is U+0025 (%) and remaining does not start
            // with two ASCII hex digits, invalid-URL-unit
            // validation error.
            if (c == '%') {
                if (!starts_with_two_hex_digits(pointer.remaining())) {
                    // non fatal error!
                    // ec =
                    // make_error(url_error_code::invalid_url_unit);
                    // return false;
                }
            }
            // 3 Append c to buffer.
            ctx.buffer.append(pointer.encoded_codepoint());
        }
        return true;
    }

    bool fragment_state(codepoint_t c, url_pointer& pointer, url_record& url,
                        url_parse_context& ctx, parse_state& state,
                        std::error_code& ec) {
        // 1 If c is not the EOF code point, then:
        if (!pointer.is_eof()) {
            // 1 If c is not a URL code point and not U+0025 (%),
            // invalid-URL-unit validation error.
            if (c != '%' && !is_url_codepoint(c)) {
                // non fatal error!
                // ec =
                // make_error(url_error_code::invalid_url_unit);
                // return false;
            }
            // 2 If c is U+0025 (%) and remaining does not start
            // with two ASCII hex digits, invalid-URL-unit
            // validation error.
            if (c == '%') {
                if (!starts_with_two_hex_digits(pointer.remaining())) {
                    // non fatal error!
                    // ec =
                    // make_error(url_error_code::invalid_url_unit);
                    // return false;
                }
            }
            // 3 UTF-8 percent-encode c using the fragment
            // percent-encode set and append the result to url's
            // fragment.
            assert(url.has_fragment());
            percent_encode(pointer.encoded_codepoint(),
                           fragment_percent_encode_set,
                           dynamic_buffer(url.fragment), ec);
            return !ec;
        }
        return true;
    }

    bool run_state(codepoint_t c, url_pointer& pointer, url_record& url,
                   const url_record* base, url_parse_context& ctx,
                   parse_state& state, std::error_code& ec) {
        switch (state) {
        case parse_state::scheme_start:
            return scheme_start_state(c, pointer, ctx, state, ec);
        case parse_state::scheme:
            return scheme_state(c, pointer, url, base, ctx, state, ec);
        case parse_state::no_scheme:
            return no_scheme_state(c, pointer, url, base, state, ec);
        case parse_state::special_relative_or_authority:
            return special_relative_or_authority_state(c, pointer, state);
        case parse_state::path_or_authority:
            return path_or_authority_state(c, pointer, state);
        case parse_state::relative:
            return relative_state(c, pointer, url, base, ctx, state, ec);
        case parse_state::relative_slash:
            return relative_slash_state(c, pointer, url, base, ctx, state, ec);
        case parse_state::special_authority_slashes:
            return special_authority_slashes_state(c, pointer, state, ec);
        case parse_state::special_authority_ignore_slashes:
            return special_authority_ignore_slashes_state(c, pointer, state,
                                                          ec);
        case parse_state::authority:
            return authority_state(c, pointer, url, ctx, state, ec);
        case parse_state::host:
            return host_state(c, pointer, url, ctx, state, ec);
        case parse_state::port:
            return port_state(c, pointer, url, ctx, state, ec);
        case parse_state::file:
            return file_state(c, pointer, url, base, ctx, state, ec);
        case parse_state::file_slash:
            return file_slash_state(c, pointer, url, base, ctx, state, ec);
        case parse_state::file_host:
            return file_host_state(c, pointer, url, ctx, state, ec);
        case parse_state::path_start:
            return path_start_state(c, pointer, url, state, ec);
        case parse_state::path:
            return path_state(c, pointer, url, ctx, state, ec);
        case parse_state::opaque_path:
            return opaque_path_state(c, pointer, url, ctx, state, ec);
        case parse_state::query:
            return query_state(c, pointer, url, ctx, state, ec);
        case parse_state::fragment:
            return fragment_state(c, pointer, url, ctx, state, ec);
        default:
            assert(false);
            return false;
        }
    }

    class url_error_category_t : public std::error_category {
        const char* name() const noexcept override {
            return "url";
        }

        std::string message(int condition) const override {
            url_error_code e = static_cast<url_error_code>(condition);
            switch (e) {
            case url_error_code::none:
                return "no error";
            case url_error_code::invalid_url_unit:
                return "A code point is found "
                       "that is "
                       "not a "
                       "URL unit";
            case url_error_code::special_scheme_missing_following_solidus:
                return "Scheme is not followed "
                       "by "
                       "\"//\"";
            case url_error_code::missing_scheme_non_relative_url:
                return "Missing scheme";
            case url_error_code::host_missing:
                return "The input has a "
                       "special "
                       "scheme, but "
                       "does not "
                       "contain a host";
            case url_error_code::host_invalid_code_point:
                return "An opaque host "
                       "contains a "
                       "forbidden "
                       "host code "
                       "point";
            case url_error_code::domain_invalid_code_point:
                return "Host contains a "
                       "forbidden "
                       "domain code "
                       "point";
            case url_error_code::ipv6_unclosed:
                return "An IPv6 address is "
                       "missing the "
                       "closing "
                       "(])";
            case url_error_code::ipv6_invalid:
                return "An IPv6 address is "
                       "invalid";
            case url_error_code::ipv4_too_many_parts:
                return "An IPv4 address does "
                       "not "
                       "consist of "
                       "exactly 4 "
                       "parts";
            case url_error_code::ipv4_non_numeric_part:
                return "An IPv4 address part "
                       "is not "
                       "numeric";
            case url_error_code::ipv4_out_of_range:
                return "An IPv4 address part "
                       "exceeds "
                       "255";
            case url_error_code::domain_to_ascii:
                return "Unicode ToASCII "
                       "recorded an "
                       "error or "
                       "returned "
                       "the empty string";
            case url_error_code::port_invalid:
                return "The port is invalid";
            case url_error_code::port_out_of_range:
                return "The port is too big";
            default:
                return "unknown error";
            }
        }
    };

    const url_error_category_t url_error_category_inst;
} // namespace

const std::error_category& net::url_category() noexcept {
    return url_error_category_inst;
}

void net::detail::parse_url_record(std::string_view input, url_record& url,
                                   const url_record* base,
                                   std::error_code& ec) {
    ec.clear();
    // 1 If url is not given:
    {
        // 1 Set url to a new URL.
        url = {};
        // 2 If input contains any leading or trailing C0 control or
        // space, invalid-URL-unit validation error. non fatal error! 3
        // Remove any leading and trailing C0 control or space from
        // input.
        remove_leading_and_trailing_c0_and_space(input);
    }

    // 2 If input contains any ASCII tab or newline, invalid-URL-unit
    // validation error. non fatal error! 3 Remove all ASCII tab or newline
    // from input.
    std::string owned_input{input};
    remove_tabs_and_newlines(owned_input);
    input = owned_input;

    // 4 Let state be state override if given, or scheme start state
    // otherwise.
    parse_state state = parse_state::scheme_start;
    // 5 Set encoding to the result of getting an output encoding from
    // encoding. use utf-8 6 Let buffer be the empty string. 7 Let
    // atSignSeen, insideBrackets, and passwordTokenSeen be false.
    url_parse_context ctx;
    ctx.buffer.reserve(input.size());
    // 8 Let pointer be a pointer for input.
    url_pointer pointer{input};

    // 9 Keep running the following state machine by switching on state.
    // If after a run pointer points to the EOF code point, go to the next
    // step. Otherwise, increase pointer by 1 and continue with the state
    // machine.

    while (!ec) {
        codepoint_t c = pointer.advance(ec);
        if (ec) {
            return;
        }
        run_state(c, pointer, url, base, ctx, state, ec);
        if (ec) {
            return;
        }
        if (pointer.is_eof()) {
            break;
        }
    }

    // 10 Return url.
    if (url.path.empty() && !ctx.path.empty()) {
        assert(!url.is_opaque_path());
        if (ctx.path.front().empty()) {
            url.set_empty_first_path_segment();
        }
        url.path = url_path_serializer(ctx.path);
    }
}

void net::detail::serialize_url(const url_record& url, bool exclude_fragment,
                                std::string& output) {
    // 1 Let output be url's scheme and U+003A (:) concatenated.
    output.append(url.scheme);
    output += ':';
    // 2 If url's host is non-null:
    if (url.has_host()) {
        // 1 Append "//" to output.
        output.insert(output.end(), 2, '/');
        // 2 If url includes credentials, then:
        if (url.has_username() || url.has_password()) {
            // 1 Append url's username to output.
            output += url.username();
            // 2 If url's password is not the empty string,
            // then append U+003A (:), followed by url's password,
            // to output.
            if (auto password = url.password(); !password.empty()) {
                output += ':';
                output += password;
            }
            // 3 Append U+0040 (@) to output.
            output += '@';
        }
        // 3 Append url's host, serialized, to output.
        output += url.host;
        // 4 If url's port is non-null, append U+003A (:)
        // followed by url's port, serialized, to output.
        if (url.has_port()) {
            output += ':';
            output += std::to_string(url.port);
        }
    }
    // 3 If url's host is null, url does not have an opaque path,
    // url's path's size is greater than 1, and url's path[0] is the empty
    // string, then append U+002F (/) followed by U+002E (.) to output.
    if (!url.has_host() && !url.is_opaque_path() && url.path.size() > 1 &&
        url.is_empty_first_path_segment()) {
        output += '/';
        output += '.';
    }
    // 4 Append the result of URL path serializing url to output.
    output += url.path;
    // 5 If url's query is non-null, append U+003F (?), followed by url's
    // query, to output.
    if (url.has_query()) {
        output += '?';
        output += url.query;
    }
    // 6 If exclude fragment is false and url's fragment is non-null,
    // then append U+0023 (#), followed by url's fragment, to output.
    if (!exclude_fragment && url.has_fragment()) {
        output += '#';
        output += url.fragment;
    }
    // 7 Return output.
}

std::optional<uint16_t>
net::detail::get_default_url_scheme_port(std::string_view scheme) {
    return default_scheme_port(scheme);
}

void net::detail::url_percent_decode_userinfo(std::string_view userinfo,
                                              std::string& out,
                                              std::error_code& ec) {
    ec.clear();
    percent_decode(userinfo, userinfo_percent_encode_set, dynamic_buffer(out),
                   ec);
}

void net::detail::url_percent_decode_host(std::string_view host,
                                          std::string& out,
                                          std::error_code& ec) {
    ec.clear();
    percent_decode(host, c0_control_percent_encode_set, dynamic_buffer(out),
                   ec);
}

void net::detail::url_percent_decode_path(std::string_view path,
                                          std::string& out,
                                          std::error_code& ec) {
    ec.clear();
    percent_decode(path, path_percent_encode_set, dynamic_buffer(out), ec);
}

void net::detail::url_percent_decode_query(std::string_view query,
                                           std::string& out,
                                           std::error_code& ec) {
    ec.clear();
    percent_decode(query, query_percent_encode_set, dynamic_buffer(out), ec);
}

void net::detail::url_percent_decode_fragment(std::string_view fragment,
                                              std::string& out,
                                              std::error_code& ec) {
    ec.clear();
    percent_decode(fragment, fragment_percent_encode_set, dynamic_buffer(out),
                   ec);
}