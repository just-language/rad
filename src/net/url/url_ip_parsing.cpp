#include <rad/net/url/url.h>

#include <cmath>

using namespace rad;
using namespace net;
using namespace net::detail;

namespace {
    constexpr bool is_valid_for_radix(uint8_t c, uint8_t R) {
        if (R == 10) {
            return c >= '0' && c <= '9';
        }
        if (R == 8) {
            return c >= '0' && c <= '7';
        }
        if (R == 16) {
            return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') ||
                   (c >= 'A' && c <= 'F');
        }
        return false;
    }

    // The IPv4 number parser takes an ASCII string input and then runs
    // these steps. They return failure or a tuple of a number and a
    // boolean.
    std::optional<std::pair<uint32_t, bool>>
    parse_ipv4_number(std::string_view input) {
        // 1 If input is the empty string, then return failure.
        if (input.empty()) {
            return std::nullopt;
        }
        // 2 Let validationError be false.
        bool validation_error = false;
        // 3 Let R be 10.
        uint32_t R = 10;
        // 4 If input contains at least two code points and
        // the first two code points are either "0X" or "0x", then:
        if (input.size() >= 2 && input[0] == '0' &&
            (input[1] == 'x' || input[1] == 'X')) {
            // 1 Set validationError to true.
            validation_error = true;
            // 2 Remove the first two code points from input.
            input.remove_prefix(2);
            // 3 Set R to 16.
            R = 16;
        }
        // 5 Otherwise, if input contains at least two code points and
        // the first code point is U+0030 (0), then:
        else if (input.size() >= 2 && input[0] == '0') {
            // 1 Set validationError to true.
            validation_error = true;
            // 2 Remove the first code point from input.
            input.remove_prefix(1);
            // 3 Set R to 8.
            R = 8;
        }
        // 6 If input is the empty string, then return (0, true).
        if (input.empty()) {
            return std::pair{0, true};
        }
        // 7 If input contains a code point that is not a radix-R digit,
        // then return failure.
        for (char ch : input) {
            if (!is_valid_for_radix(static_cast<uint8_t>(ch),
                                    static_cast<uint8_t>(R))) {
                return std::nullopt;
            }
        }
        // 8 Let output be the mathematical integer value that is
        // represented by input in radix-R notation, using ASCII hex
        // digits for digits with values 0 through 15.
        std::error_code ec;
        uint32_t output = to_uint32(input, R, ec);
        if (ec) {
            return std::nullopt;
        }
        return std::pair{output, validation_error};
    }

    std::optional<uint16_t>
    find_ipv6_compressed_piece_index(const ipv6& address) {
        // To find the IPv6 address compressed piece index given an IPv6
        // address address: 1 Let longestIndex be null.
        std::optional<uint16_t> longest_index;
        // 2 Let longestSize be 1.
        uint16_t longest_size = 1;
        // 3 Let foundIndex be null.
        std::optional<uint16_t> found_index;
        // 4 Let foundSize be 0.
        uint16_t found_size = 0;
        // 5 For each pieceIndex of address’s pieces’s indices:
        auto pieces = address.to_words();
        for (uint16_t piece_index = 0; piece_index < pieces.size();
             ++piece_index) {
            // 1 If address’s pieces[pieceIndex] is not 0:
            if (pieces[piece_index] != 0) {
                // 1 If foundSize is greater than longestSize,
                // then set longestIndex to foundIndex and
                // longestSize to foundSize.
                if (found_size > longest_size) {
                    longest_index = found_index;
                    longest_size = found_size;
                }
                // 2 Set foundIndex to null.
                found_index = std::nullopt;
                // 3 Set foundSize to 0.
                found_size = 0;
            }
            // 2 Otherwise:
            else {
                // 1 If foundIndex is null, then set foundIndex
                // to pieceIndex.
                if (!found_index.has_value()) {
                    found_index = piece_index;
                }
                // 2 Increment foundSize by 1.
                found_size += 1;
            }
        }
        // 6 If foundSize is greater than longestSize, then return
        // foundIndex.
        if (found_size > longest_size) {
            return found_index;
        }
        // 7 Return longestIndex.
        return longest_index;
    }
} // namespace

// The ends in a number checker takes an ASCII string input and
// then runs these steps. They return a boolean.
bool net::detail::url_domain_ends_in_number(std::string_view input) {
    // 1 Let parts be the result of strictly splitting input on U+002E (.).
    std::vector<std::string_view> parts;
    parts.reserve(4);
    split(input, ".",
          [&parts](std::string_view part) { parts.push_back(part); });
    if (parts.empty()) {
        return false;
    }
    // 2 If the last item in parts is the empty string, then:
    if (parts.back().empty()) {
        // 1 If parts’s size is 1, then return false.
        if (parts.size() == 1) {
            return false;
        }
        // 2 Remove the last item from parts.
        parts.pop_back();
    }
    // 3 Let last be the last item in parts.
    auto last = parts.back();
    // 4 If last is non-empty and contains only ASCII digits, then return
    // true. The erroneous input "09" will be caught by the IPv4 parser at a
    // later stage.
    std::string_view sublast = last;
    bool is_hex = false;
    if (last.starts_with("0x") || last.starts_with("0X")) {
        sublast = sublast.substr(2);
        is_hex = true;
    }
    if (!sublast.empty()) {
        bool all_digits = true;
        for (char ch : sublast) {
            if (!(ch >= '0' && ch <= '9')) {
                if (is_hex &&
                    ((ch >= 'a' && ch <= 'f') || (ch >= 'A' && ch <= 'F'))) {
                    continue;
                }
                all_digits = false;
                break;
            }
        }
        if (all_digits) {
            return true;
        }
    }
    // 5 If parsing last as an IPv4 number does not return failure, then
    // return true.
    if (parse_ipv4_number(last).has_value()) {
        return true;
    }
    // 6 Return false.
    return false;
}

ipv4 net::detail::parse_url_ipv4_host(std::string_view input,
                                      std::error_code& ec) {
    // 1 Let parts be the result of strictly splitting input on U+002E (.).
    std::vector<std::string_view> parts;
    parts.reserve(4);
    split(input, ".",
          [&parts](std::string_view part) { parts.push_back(part); });
    // 2 If the last item in parts is the empty string, then:
    if (!parts.empty() && parts.back().empty()) {
        // 1 IPv4-empty-part validation error. (non fatal error!)
        // 2 If parts’s size is greater than 1, then remove the last
        // item from parts.
        if (parts.size() > 1) {
            parts.pop_back();
        }
    }
    // 3 If parts’s size is greater than 4, IPv4-too-many-parts validation
    // error, return failure.
    if (parts.size() > 4) {
        ec = make_error(url_error_code::ipv4_too_many_parts);
        return {};
    }
    // 4 Let numbers be an empty list.
    std::vector<uint32_t> numbers;
    numbers.reserve(parts.size());
    // 5 For each part of parts:
    for (auto part : parts) {
        // 1 Let result be the result of parsing part.
        auto result = parse_ipv4_number(part);
        // 2 If result is failure, IPv4-non-numeric-part validation
        // error, return failure.
        if (!result.has_value()) {
            ec = make_error(url_error_code::ipv4_non_numeric_part);
            return {};
        }
        // 3 If result[1] is true, IPv4-non-decimal-part validation
        // error. (non fatal error!) 4 Append result[0] to numbers.
        numbers.push_back(result->first);
    }
    // 6 If any item in numbers is greater than 255, IPv4-out-of-range-part
    // validation error. fatal error except for the last part!
    for (uint32_t n : std::span{numbers}.subspan(0, numbers.size() - 1)) {
        if (n > 255) {
            ec = make_error(url_error_code::ipv4_out_of_range);
            return {};
        }
    }
    // 7 If any but the last item in numbers is greater than 255, then
    // return failure. (done) 8 If the last item in numbers is greater than
    // or equal to 256(5 − numbers’s size), then return failure.
    if (numbers.back() >= std::pow(256, 5 - numbers.size())) {
        ec = make_error(url_error_code::ipv4_out_of_range);
        return {};
    }
    // 9 Let ipv4 be the last item in numbers.
    uint32_t ipv4_num = numbers.back();
    // 10 Remove the last item from numbers.
    numbers.pop_back();
    // 11 Let counter be 0.
    uint32_t counter = 0;
    // 12 For each n of numbers:
    for (uint32_t n : numbers) {
        // 1 Increment ipv4 by n × 256(3 − counter).
        ipv4_num += n * static_cast<uint32_t>(std::pow(256, 3 - counter));
        // 2 Increment counter by 1.
        counter += 1;
    }
    // 13 Return ipv4.
    return ipv4{ipv4_num};
}

std::string net::detail::url_serialize_ipv6(const ipv6& address) {
    // The IPv6 serializer takes an IPv6 address address and
    // then runs these steps. They return an ASCII string.
    // 1 Let output be the empty string.
    std::string output;
    output.reserve(47);
    output += '[';
    // 2 Let compress be the result of finding the IPv6 address
    // compressed piece index given address.
    auto compress = find_ipv6_compressed_piece_index(address);
    // 3 Let ignore0 be false.
    bool ignore0 = false;
    // 4 For each pieceIndex of address’s pieces’s indices:
    auto pieces = address.to_words();
    for (uint16_t piece_index = 0; piece_index < pieces.size(); ++piece_index) {
        // 1 If ignore0 is true and address[pieceIndex] is 0, then
        // continue.
        if (ignore0 && pieces[piece_index] == 0) {
            continue;
        }
        // 2 Otherwise, if ignore0 is true, set ignore0 to false.
        ignore0 = false;
        // 3 If compress is pieceIndex, then:
        if (compress == piece_index) {
            // 1 Let separator be "::" if pieceIndex is 0; otherwise
            // U+003A (:).
            const std::string_view separator = piece_index == 0 ? "::" : ":";
            // 2 Append separator to output.
            output += separator;
            // 3 Set ignore0 to true and continue.
            ignore0 = true;
            continue;
        }
        // 4 Append address[pieceIndex], represented as the shortest
        // possible lowercase hexadecimal number, to output.
        std::array<char, 4> hex_buff;
        const auto res =
            std::to_chars(hex_buff.data(), hex_buff.data() + hex_buff.size(),
                          pieces[piece_index], 16);
        assert(res.ec == std::errc{});
        if (res.ec != std::errc{}) {
            return {};
        }
        output.append(hex_buff.data(), res.ptr);
        // 5 If pieceIndex is not 7, then append U+003A (:) to output.
        if (piece_index != 7) {
            output += ':';
        }
    }
    // 5 Return output.
    output += ']';
    return output;
}