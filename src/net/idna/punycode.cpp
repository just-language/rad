#include <rad/buffer.h>
#include <rad/net/idna/punycode.h>
#include <rad/string.h>
#include <rad/utf.h>

#include <algorithm>
#include <array>
#include <charconv>
#include <vector>

using namespace RAD_LIB_NAMESPACE;
using namespace net;
using namespace idna;

namespace {
    struct punycode_params_t {
        uint32_t base = 36;
        uint32_t tmin = 1;
        uint32_t tmax = 26;
        uint32_t skew = 38;
        uint32_t damp = 700;
        uint32_t initial_bias = 72;
        uint32_t initial_n = 128;
    };

    constexpr uint32_t adapt(uint32_t delta, uint32_t numpoints, bool firsttime,
                             const punycode_params_t& params) {
        if (firsttime) {
            delta = delta / params.damp;
        }
        else {
            delta = delta / 2;
        }
        delta = delta + delta / numpoints;
        uint32_t k = 0;
        while (delta > ((params.base - params.tmin) * params.tmax) / 2) {
            delta = delta / (params.base - params.tmin);
            k = k + params.base;
        }
        return k + (((params.base - params.tmin + 1) * delta) /
                    (delta + params.skew));
    }

    constexpr std::errc count_codepoints(std::string_view input,
                                         uint32_t& basic, uint32_t& non_basic) {
        const char* p = input.data();
        std::size_t n = input.size();
        basic = non_basic = 0;
        while (n > 0) {
            std::errc e = {};
            codepoint_t cp = utf8_codecvt::decode(p, n, e);
            if (e != std::errc{}) {
                return e;
            }
            basic += cp <= 127;
            non_basic += cp > 127;
        }
        return {};
    }

    bool consume_basic_codepoints(std::string_view& codepoints,
                                  std::vector<codepoint_t>& output) {
        const auto rit =
            std::find(codepoints.rbegin(), codepoints.rend(), codepoint_t('-'));
        if (rit == codepoints.rend()) {
            output.reserve(codepoints.size() * 2);
            return true;
        }
        auto delim_it = std::prev(rit.base());
        const std::size_t count_of_basic =
            std::distance(codepoints.begin(), delim_it);
        output.reserve(count_of_basic +
                       (codepoints.size() - count_of_basic) * 2);
        for (auto it = codepoints.begin(); it != delim_it; ++it) {
            if (static_cast<uint8_t>(*it) > 127) {
                return false;
            }
            output.push_back(*it);
        }

        codepoints.remove_prefix(std::distance(codepoints.begin(), delim_it) +
                                 1);
        assert(codepoints.empty() || codepoints.front() != '-');
        return true;
    }

    void fill_codepoints(std::string_view input, dynamic_buffer basic,
                         std::vector<codepoint_t>& non_basic,
                         std::vector<codepoint_t>& all_codepoints) {
        const char* p = input.data();
        std::size_t n = input.size();
        bool has_basic = false;
        while (n > 0) {
            const codepoint_t cp = utf8_codecvt::decode_unchecked(p, n);
            all_codepoints.push_back(cp);
            if (cp <= 127) {
                basic.push_back(static_cast<char>(cp));
                has_basic = true;
            }
            else {
                non_basic.push_back(cp);
            }
        }
        if (has_basic) {
            basic.push_back('-');
        }
    }

    constexpr char encode_digit(uint32_t d) {
        if (d < 26) {
            return 'a' + d;
        }
        else {
            assert(d < 36);
            return '0' + (d - 26);
        }
    }

    constexpr bool decode_digit(char c, uint32_t& digit) noexcept {
        if (c >= 'a' && c <= 'z') {
            digit = c - 'a' + 0;
            return true;
        }
        else if (c >= '0' && c <= '9') {
            digit = c + 26 - '0';
            return true;
        }
        return false;
    }

    constexpr bool add_and_check_overflow(uint32_t& out, uint32_t in) {
        uint32_t old_out = out;
        out += in;
        return old_out <= out;
    }

    constexpr bool multiply_and_check_overflow(uint32_t& out, uint32_t in) {
        std::uint64_t res64 = out;
        res64 *= in;
        out *= in;
        return out == res64;
    }
} // namespace

bool idna::needs_punycode_encode(std::string_view input) noexcept {
    const char* p = input.data();
    std::size_t n = input.size();
    while (n > 0) {
        codepoint_t cp = utf8_codecvt::decode_unchecked(p, n);
        if (cp > 127) {
            return true;
        }
    }
    return false;
}

std::size_t idna::punycode_encode(std::string_view input, dynamic_buffer output,
                                  std::error_code& ec) {
    constexpr auto params = punycode_params_t{};

    uint32_t basic_count = 0, non_basic_count = 0;
    if (auto e = count_codepoints(input, basic_count, non_basic_count);
        e != std::errc{}) {
        ec = std::make_error_code(e);
        return 0;
    }

    output.reserve(input.size());
    std::vector<codepoint_t> codepoints;
    codepoints.reserve(basic_count + non_basic_count);
    std::vector<codepoint_t> non_basic_vec;
    non_basic_vec.reserve(non_basic_count);

    fill_codepoints(input, output, non_basic_vec, codepoints);
    std::sort(non_basic_vec.begin(), non_basic_vec.end());
    auto non_basic_view = std::span{non_basic_vec};

    uint32_t n = params.initial_n;
    uint32_t delta = 0;
    uint32_t bias = params.initial_bias;
    const uint32_t b = basic_count;
    uint32_t h = b;

    while (h < codepoints.size()) {
        codepoint_t m = non_basic_view.front();
        non_basic_view = non_basic_view.subspan(1);
        if (m < n) {
            continue;
        }

        delta = delta + (m - n) * (h + 1);
        n = m;
        for (codepoint_t c : codepoints) {
            if (c < n) {
                delta += 1;
            }
            else if (c == n) {
                uint32_t q = delta;
                for (uint32_t k = params.base;; k += params.base) {
                    uint32_t t = 0;
                    if (k <= bias) {
                        t = params.tmin;
                    }
                    else if (k >= bias + params.tmax) {
                        t = params.tmax;
                    }
                    else {
                        t = k - bias;
                    }
                    if (q < t) {
                        break;
                    }
                    uint32_t digit = t + ((q - t) % (params.base - t));
                    output.push_back(encode_digit(digit));
                    q = (q - t) / (params.base - t);
                }
                output.push_back(encode_digit(q));
                bias = adapt(delta, h + 1, h == b, params);
                delta = 0;
                h += 1;
            }
        }

        delta += 1;
        n += 1;
    }

    return input.size();
}

std::size_t idna::punycode_decode(std::string_view input, dynamic_buffer out,
                                  std::error_code& ec) {
    constexpr auto params = punycode_params_t{};
    std::vector<codepoint_t> output;

    if (!consume_basic_codepoints(input, output)) {
        ec = std::make_error_code(std::errc::invalid_argument);
        return 0;
    }
    // output now contains only basic code points (ASCII)
    uint32_t output_bytes_len = static_cast<uint32_t>(output.size());

    uint32_t n = params.initial_n;
    uint32_t i = 0;
    uint32_t bias = params.initial_bias;

    while (!input.empty()) {
        const uint32_t oldi = i;
        uint32_t w = 1;
        for (uint32_t k = params.base;; k += params.base) {
            if (input.empty()) {
                ec = std::make_error_code(std::errc::invalid_argument);
                return 0;
            }
            uint32_t digit = 0;
            if (!decode_digit(input.front(), digit)) {
                ec = std::make_error_code(std::errc::invalid_argument);
                return 0;
            }
            input.remove_prefix(1);
            i = i + digit * w;
            const uint32_t t = k <= bias                 ? params.tmin
                               : k >= bias + params.tmax ? params.tmax
                                                         : k - bias;
            if (digit < t) {
                break;
            }
            // w = w * (params.base - t);
            if (!multiply_and_check_overflow(w, (params.base - t))) {
                ec = std::make_error_code(std::errc::invalid_argument);
                return 0;
            }
        }
        bias = adapt(i - oldi, static_cast<uint32_t>(output.size() + 1),
                     oldi == 0, params);
        // n = n + i / (output.size() + 1);
        if (!add_and_check_overflow(
                n, i / static_cast<uint32_t>(output.size() + 1))) {
            ec = std::make_error_code(std::errc::invalid_argument);
            return 0;
        }
        i = i % (output.size() + 1);
        std::errc e{};
        if (!validate_codepoint(n, e)) {
            ec = std::make_error_code(e);
            return 0;
        }
        output_bytes_len +=
            static_cast<uint32_t>(utf8_codecvt::codepoint_size(n));
        output.insert(output.begin() + i, n);
        i += 1;
    }

    auto out_buff = out.prepare(output_bytes_len).to_span<char>();
    char* out_ptr = out_buff.data();
    std::size_t out_n = out_buff.size();
    for (codepoint_t cp : output) {
        const std::size_t len = utf8_codecvt::encode(cp, out_ptr, out_n);
        if (len == 0) {
            ec = std::make_error_code(std::errc::illegal_byte_sequence);
            return 0;
        }
        out_ptr += len;
        out_n -= len;
    }

    return input.size();
}