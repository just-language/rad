#include <rad/net/idna/idna.h>
#include <rad/net/idna/punycode.h>
#include <rad/string.h>
#include <rad/utf.h>

#include <algorithm>
#include <array>
#include <vector>

using namespace RAD_LIB_NAMESPACE;
using namespace net;
using namespace idna;

namespace {
    struct idna_codepoints_range_t {
        codepoint_t start;
        codepoint_t end = start;

        friend constexpr bool
        operator<(codepoint_t c, const idna_codepoints_range_t& r) noexcept {
            return c < r.start;
        }

        friend constexpr bool operator<(const idna_codepoints_range_t& r,
                                        codepoint_t c) noexcept {
            return c > r.end;
        }
    };

    // Array of IDNA ignored code point ranges
    constexpr std::array<idna_codepoints_range_t, 15> idna_ignored_ranges = {{
        {0x00AD, 0x00AD},   // Soft Hyphen
        {0x034F},           // Combining Grapheme Joiner
        {0x115F, 0x1160},   // HANGUL CHOSEONG FILLER, HANGUL JUNGSEONG FILLER
        {0x17B4, 0x17B5},   // KHMER VOWEL INHERENT AQ, KHMER VOWEL INHERENT AA
        {0x180B, 0x180F},   // MONGOLIAN FREE VARIATION SELECTOR ONE..MONGOLIAN
                            // VOWEL SEPARATOR (180E) MONGOLIAN FREE VARIATION
                            // SELECTOR FOUR (180F)
        {0x200B},           // ZERO WIDTH SPACE
        {0x2060, 0x2064},   // WORD JOINER..INVISIBLE SEPARATOR
                            // (2060..2063) INVISIBLE PLUS (2064)
        {0x206A, 0x206F},   // INHIBIT SYMMETRIC SWAPPING..NOMINAL DIGIT SHAPES
        {0x3164},           // HANGUL FILLER
        {0xFE00, 0xFE0F},   // VARIATION SELECTOR-1..VARIATION SELECTOR-16
        {0xFEFF},           // ZERO WIDTH NO-BREAK SPACE
        {0xFFA0},           // HALFWIDTH HANGUL FILLER
        {0x1BCA0, 0x1BCA3}, // SHORTHAND FORMAT LETTER
                            // OVERLAP..SHORTHAND FORMAT UP STEP
        {0x1D173, 0x1D17A}, // MUSICAL SYMBOL BEGIN BEAM..MUSICAL
                            // SYMBOL END PHRASE
        {0xE0100, 0xE01EF}  // VARIATION SELECTOR-17..VARIATION SELECTOR-256
    }};

#include "idna_disallowed_ranges.h"

    constexpr bool is_ignored_idna_code_point(codepoint_t c) {
        return std::binary_search(idna_ignored_ranges.begin(),
                                  idna_ignored_ranges.end(), c);
    }

    constexpr bool is_disallowed_idna_code_point(codepoint_t c) {
        return std::binary_search(idna_disallowed_ranges.begin(),
                                  idna_disallowed_ranges.end(), c);
    }

    void map_codepoint(codepoint_t& c) noexcept {
        // upper case to lower case
        if (c >= 'A' && c <= 'Z') {
            c = c - 'A' + 'a';
        }
        // A to Z bold
        if (c >= 0x1D400 && c <= 0x1D419) {
            c = c - 0x1D400 + 'a';
        }
        // a to z bold
        else if (c >= 0x1D41A && c <= 0x1D433) {
            c = c - 0x1D41A + 'a';
        }
        // U+3000 is mapped to U+0020 (space) which is disallowed
        if (c == 0x3000) {
            c = 0x20;
        }
        // Ideographic full stop (full-width period for Chinese, etc.)
        // should be treated as a dot. U+3002 is mapped to U+002E (dot)
        if (c == 0x3002) {
            c = 0x2e;
        }
        // full width and half width (! -> ~ ascii printable chars)
        if (c >= 0xff01 && c <= 0xff5e) {
            c = c - 0xff01 + '!';
            if (c >= 'A' && c <= 'Z') {
                c = c - 'A' + 'a';
            }
        }
    }

    void to_ascii_domain_processing(
        std::string& domain_name, bool use_std3_ascii_rules, bool check_hyphens,
        bool check_bidi, bool check_joiners, bool transitional_processing,
        bool ignore_invalid_punycode, std::error_code& ec) {
        std::string output;
        output.reserve(domain_name.size());
        const char* p = domain_name.data();
        std::size_t n = domain_name.size();
        while (n > 0) {
            std::errc decode_ec{};
            codepoint_t c = utf8_codecvt::decode(p, n, decode_ec);
            if (decode_ec != std::errc{}) {
                ec = std::make_error_code(decode_ec);
                return;
            }
            map_codepoint(c);
            if (is_ignored_idna_code_point(c)) {
                continue;
            }
            if (is_disallowed_idna_code_point(c)) {
                ec = std::make_error_code(std::errc::illegal_byte_sequence);
                return;
            }
            std::array<char, 4> encode_buff{};
            std::size_t encode_len =
                utf8_codecvt::encode_unchecked(c, encode_buff.data());
            output.append(encode_buff.data(), encode_len);
        }
        domain_name = std::move(output);
    }

    bool to_ascii_main(std::string& domain_name, bool check_hyphens,
                       bool check_bidi, bool check_joiners,
                       bool use_std3_ascii_rules, bool transitional_processing,
                       bool verify_dns_length, bool ignore_invalid_punycode,
                       std::error_code& ec) {
        assert(!domain_name.empty());
        bool domain_was_changed = false;
        // 1 To the input domain_name, apply the Processing Steps in
        // Section 4, Processing, using the input boolean flags
        // Transitional_Processing, CheckHyphens, CheckBidi,
        // CheckJoiners, and UseSTD3ASCIIRules. This may record an
        // error. not implemented yet!
        to_ascii_domain_processing(domain_name, use_std3_ascii_rules,
                                   check_hyphens, check_bidi, check_joiners,
                                   transitional_processing,
                                   ignore_invalid_punycode, ec);
        if (ec) {
            return false;
        }
        // domain names can't start or end in '-'
        if (!domain_name.empty() &&
            (domain_name.front() == '-' || domain_name.back() == '-')) {
            ec = std::make_error_code(std::errc::invalid_argument);
            return false;
        }
        // 2 Break the result into labels at U+002E FULL STOP
        std::vector<std::string> labels;
        const std::size_t labels_count = count_occurance(domain_name, ".") + 1;
        if (labels_count < 2) {
            // is this an error?
        }
        labels.reserve(labels_count);
        std::size_t total_length = 0;
        for (auto label : split(domain_name, ".")) {
            if (label.starts_with(punycode_prefix) || label == "-") {
                ec = std::make_error_code(std::errc::invalid_argument);
                return false;
            }
            // 3 Convert each label with non-ASCII characters into
            // Punycode [RFC3492], and prefix by “xn--”. This may
            // record an error
            if (needs_punycode_encode(label)) {
                std::string encoded_label;
                punycode_encode(label, dynamic_buffer(encoded_label), ec);
                if (ec) {
                    return false;
                }
                encoded_label.insert(0, punycode_prefix);
                labels.push_back(std::move(encoded_label));
                domain_was_changed = true;
            }
            else {
                labels.emplace_back(label);
            }
            total_length += labels.back().size();
            // 4 If the VerifyDnsLength flag is true, then verify
            // DNS length restrictions. This may record an error.
            if (verify_dns_length) {
                // 1 The length of the domain name, excluding
                // the root label and its dot, is from 1 to 253
                if (total_length < 1 || total_length > 253) {
                    ec = std::make_error_code(std::errc::value_too_large);
                    return false;
                }
                // 2 The length of each label is from 1 to 63.
                if (labels.back().size() < 1 || labels.back().size() > 63) {
                    ec = std::make_error_code(std::errc::value_too_large);
                    return false;
                }
            }
        }
        // 5 If an error was recorded in steps 1-4, then the operation
        // has failed and a failure value is returned. No DNS lookup
        // should be done.
        assert(!ec);
        if (ec) {
            return false;
        }
        // 6 Otherwise join the labels using U+002E FULL STOP as a
        // separator, and return the result.
        if (domain_was_changed) {
            domain_name.clear();
            for (size_t i = 0; i < labels.size() - 1; ++i) {
                domain_name += labels[i] + '.';
            }
            domain_name += labels.back();
        }
        return true;
    }
} // namespace

bool idna::domain_to_ascii(std::string& domain_name, bool check_hyphens,
                           bool check_bidi, bool check_joiners,
                           bool use_std3_ascii_rules,
                           bool transitional_processing, bool verify_dns_length,
                           bool ignore_invalid_punycode, std::error_code& ec) {
    ec.clear();
    return to_ascii_main(domain_name, check_hyphens, check_bidi, check_joiners,
                         use_std3_ascii_rules, transitional_processing,
                         verify_dns_length, ignore_invalid_punycode, ec);
}