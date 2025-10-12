#include <rad/bits.h>
#include <rad/net/http2/hpack.h>
#include <rad/net/http2/http2_parser.h>

#include <array>
#include <optional>

#include "http2_debug.h"

using namespace RAD_LIB_NAMESPACE;
using namespace net;
using namespace http2;

namespace {
    using http2_header_t = std::pair<std::string_view, std::string_view>;

    const std::array<http2_header_t, 61> predefined_http2_headers{
        http2_header_t(":authority", ""),
        http2_header_t(":method", "GET"),
        http2_header_t(":method", "POST"),
        http2_header_t(":path", "/"),
        http2_header_t(":path", "/index.html"),
        http2_header_t(":scheme", "http"),
        http2_header_t(":scheme", "https"),
        http2_header_t(":status", "200"),
        http2_header_t(":status", "204"),
        http2_header_t(":status", "206"),
        http2_header_t(":status", "304"),
        http2_header_t(":status", "400"),
        http2_header_t(":status", "404"),
        http2_header_t(":status", "500"),
        http2_header_t("accept-charset", ""),
        http2_header_t("accept-encoding", "gzip, deflate"),
        http2_header_t("accept-language", ""),
        http2_header_t("accept-ranges", ""),
        http2_header_t("accept", ""),
        http2_header_t("access-control-allow-origin", ""),
        http2_header_t("age", ""),
        http2_header_t("allow", ""),
        http2_header_t("authorization", ""),
        http2_header_t("cache-control", ""),
        http2_header_t("content-disposition", ""),
        http2_header_t("content-encoding", ""),
        http2_header_t("content-language", ""),
        http2_header_t("content-length", ""),
        http2_header_t("content-location", ""),
        http2_header_t("content-range", ""),
        http2_header_t("content-type", ""),
        http2_header_t("cookie", ""),
        http2_header_t("date", ""),
        http2_header_t("etag", ""),
        http2_header_t("expect", ""),
        http2_header_t("expires", ""),
        http2_header_t("from", ""),
        http2_header_t("host", ""),
        http2_header_t("if-match", ""),
        http2_header_t("if-modified-since", ""),
        http2_header_t("if-none-match", ""),
        http2_header_t("if-range", ""),
        http2_header_t("if-unmodified-since", ""),
        http2_header_t("last-modified", ""),
        http2_header_t("link", ""),
        http2_header_t("location", ""),
        http2_header_t("max-forwards", ""),
        http2_header_t("proxy-authenticate", ""),
        http2_header_t("proxy-authorization", ""),
        http2_header_t("range", ""),
        http2_header_t("referer", ""),
        http2_header_t("refresh", ""),
        http2_header_t("retry-after", ""),
        http2_header_t("server", ""),
        http2_header_t("set-cookie", ""),
        http2_header_t("strict-transport-security", ""),
        http2_header_t("transfer-encoding", ""),
        http2_header_t("user-agent", ""),
        http2_header_t("vary", ""),
        http2_header_t("via", ""),
        http2_header_t("www-authenticate", "")};

    std::optional<size_t> find_predefined_http2_header(std::string_view name,
                                                       std::string_view value) {
        auto it = std::find_if(predefined_http2_headers.begin(),
                               predefined_http2_headers.end(),
                               [&](const http2_header_t& header) {
                                   return iequal(header.first, name) &&
                                          iequal(header.second, value);
                               });
        if (it == predefined_http2_headers.end()) {
            return std::nullopt;
        }
        return std::distance(predefined_http2_headers.begin(), it) + 1;
    }

    std::optional<size_t> find_predefined_http2_name(std::string_view name) {
        auto it = std::find_if(predefined_http2_headers.begin(),
                               predefined_http2_headers.end(),
                               [&](const http2_header_t& header) {
                                   return iequal(header.first, name);
                               });
        if (it == predefined_http2_headers.end()) {
            return std::nullopt;
        }
        return std::distance(predefined_http2_headers.begin(), it) + 1;
    }

    template <std::size_t N>
    constexpr size_t hpack_integer_encoding_len(uint64_t i) {
        static_assert(N <= 8 && N > 1, "N <= 8 && N > 1");
        constexpr std::size_t two_N_1 = (1 << N) - 1;
        if (i < two_N_1) {
            return 1;
        }
        size_t count = 1;
        i -= two_N_1;
        while (i >= 128) {
            count += 1;
            i /= 128;
        }
        return count + 1;
    }

    template <std::size_t N>
    constexpr size_t hpack_encode_integer(uint64_t i, uint8_t* p) {
        static_assert(N <= 8 && N > 1, "N <= 8 && N > 1");
        constexpr std::size_t two_N_1 = (1 << N) - 1;
        uint8_t* start_p = p;
        if (i < two_N_1) {
            *p = static_cast<uint8_t>(i);
            return 1;
        }
        *p++ = static_cast<uint8_t>(two_N_1);
        i -= two_N_1;
        while (i >= 128) {
            *p++ = static_cast<uint8_t>((i % 128) + 128);
            i = i / 128;
        }
        *p++ = static_cast<uint8_t>(i);
        return static_cast<size_t>(p - start_p);
    }

    template <std::size_t N>
    constexpr size_t hpack_decode_integer(const uint8_t* p, std::size_t n,
                                          uint64_t& i) {
        static_assert(N <= 8 && N > 1, "N <= 8 && N > 1");
        constexpr std::size_t two_N_1 = (1 << N) - 1;
        const uint8_t* start_p = p;
        if (n == 0) {
            return 0;
        }
        i = *p++;
        n -= 1;
        i &= two_N_1;
        if (i < two_N_1) {
            return 1;
        }
        uint8_t next_byte;
        uint8_t m = 0;
        do {
            if (n == 0) {
                return 0;
            }
            next_byte = *p++;
            n -= 1;
            size_t two_M = 1ull << m;
            i += (next_byte & 127) * two_M;
            m += 7;
        } while ((next_byte & 128) == 128);
        return static_cast<size_t>(p - start_p);
    }

    template <std::size_t N>
    constexpr std::pair<std::array<uint8_t, 4>, size_t> encode(uint64_t i) {
        std::array<uint8_t, 4> buff{};
        size_t len = hpack_encode_integer<N>(i, buff.data());
        return std::pair{buff, len};
    }

    template <std::size_t N, std::size_t N2>
    constexpr std::pair<uint64_t, size_t>
    decode(const std::array<uint8_t, N2>& p) {
        uint64_t i = 0;
        size_t len = hpack_decode_integer<N>(p.data(), p.size(), i);
        return std::pair{i, len};
    }

    [[maybe_unused]] void test_integer_encoding_decoding() {
        constexpr auto t1 = encode<5>(10);
        static_assert(t1.second == 1);
        static_assert(t1.first[0] == 0b01010);

        constexpr auto d1 = decode<5>(t1.first);
        static_assert(d1.first == 10);
        static_assert(d1.second == 1);

        constexpr auto t2 = encode<5>(1337);
        static_assert(t2.second == 3);
        static_assert(t2.first[0] == 0b11111);
        static_assert(t2.first[1] == 0b10011010);
        static_assert(t2.first[2] == 0b00001010);

        constexpr auto d2 = decode<5>(t2.first);
        static_assert(d2.first == 1337);
        static_assert(d2.second == 3);

        constexpr auto t3 = encode<8>(42);
        static_assert(t3.second == 1);
        static_assert(t3.first[0] == 42);

        constexpr auto d3 = decode<8>(t3.first);
        static_assert(d3.first == 42);
        static_assert(d3.second == 1);
    }

    struct huffman_entry_t {
        uint8_t symbol = 0;
        uint8_t len = 0;
        uint32_t code = 0;
    };

    constexpr std::array<huffman_entry_t, 256> http2_huffman_table = {{
        {0, 13, 0x1ff8},      {1, 23, 0x7fffd8},    {2, 28, 0xfffffe2},
        {3, 28, 0xfffffe3},   {4, 28, 0xfffffe4},   {5, 28, 0xfffffe5},
        {6, 28, 0xfffffe6},   {7, 28, 0xfffffe7},   {8, 28, 0xfffffe8},
        {9, 24, 0xffffea},    {10, 30, 0x3ffffffc}, {11, 28, 0xfffffe9},
        {12, 28, 0xfffffea},  {13, 30, 0x3ffffffd}, {14, 28, 0xfffffeb},
        {15, 28, 0xfffffec},  {16, 28, 0xfffffed},  {17, 28, 0xfffffee},
        {18, 28, 0xfffffef},  {19, 28, 0xffffff0},  {20, 28, 0xffffff1},
        {21, 28, 0xffffff2},  {22, 30, 0x3ffffffe}, {23, 28, 0xffffff3},
        {24, 28, 0xffffff4},  {25, 28, 0xffffff5},  {26, 28, 0xffffff6},
        {27, 28, 0xffffff7},  {28, 28, 0xffffff8},  {29, 28, 0xffffff9},
        {30, 28, 0xffffffa},  {31, 28, 0xffffffb},  {' ', 6, 0x14},
        {'!', 10, 0x3f8},     {'"', 10, 0x3f9},     {'#', 12, 0xffa},
        {'$', 13, 0x1ff9},    {'%', 6, 0x15},       {'&', 8, 0xf8},
        {'\'', 11, 0x7fa},    {'(', 10, 0x3fa},     {')', 10, 0x3fb},
        {'*', 8, 0xf9},       {'+', 11, 0x7fb},     {',', 8, 0xfa},
        {'-', 6, 0x16},       {'.', 6, 0x17},       {'/', 6, 0x18},
        {'0', 5, 0x0},        {'1', 5, 0x1},        {'2', 5, 0x2},
        {'3', 6, 0x19},       {'4', 6, 0x1a},       {'5', 6, 0x1b},
        {'6', 6, 0x1c},       {'7', 6, 0x1d},       {'8', 6, 0x1e},
        {'9', 6, 0x1f},       {':', 7, 0x5c},       {';', 8, 0xfb},
        {'<', 15, 0x7ffc},    {'=', 6, 0x20},       {'>', 12, 0xffb},
        {'?', 10, 0x3fc},     {'@', 13, 0x1ffa},    {'A', 6, 0x21},
        {'B', 7, 0x5d},       {'C', 7, 0x5e},       {'D', 7, 0x5f},
        {'E', 7, 0x60},       {'F', 7, 0x61},       {'G', 7, 0x62},
        {'H', 7, 0x63},       {'I', 7, 0x64},       {'J', 7, 0x65},
        {'K', 7, 0x66},       {'L', 7, 0x67},       {'M', 7, 0x68},
        {'N', 7, 0x69},       {'O', 7, 0x6a},       {'P', 7, 0x6b},
        {'Q', 7, 0x6c},       {'R', 7, 0x6d},       {'S', 7, 0x6e},
        {'T', 7, 0x6f},       {'U', 7, 0x70},       {'V', 7, 0x71},
        {'W', 7, 0x72},       {'X', 8, 0xfc},       {'Y', 7, 0x73},
        {'Z', 8, 0xfd},       {'[', 13, 0x1ffb},    {'\\', 19, 0x7fff0},
        {']', 13, 0x1ffc},    {'^', 14, 0x3ffc},    {'_', 6, 0x22},
        {'`', 15, 0x7ffd},    {'a', 5, 0x3},        {'b', 6, 0x23},
        {'c', 5, 0x4},        {'d', 6, 0x24},       {'e', 5, 0x5},
        {'f', 6, 0x25},       {'g', 6, 0x26},       {'h', 6, 0x27},
        {'i', 5, 0x6},        {'j', 7, 0x74},       {'k', 7, 0x75},
        {'l', 6, 0x28},       {'m', 6, 0x29},       {'n', 6, 0x2a},
        {'o', 5, 0x7},        {'p', 6, 0x2b},       {'q', 7, 0x76},
        {'r', 6, 0x2c},       {'s', 5, 0x8},        {'t', 5, 0x9},
        {'u', 6, 0x2d},       {'v', 7, 0x77},       {'w', 7, 0x78},
        {'x', 7, 0x79},       {'y', 7, 0x7a},       {'z', 7, 0x7b},
        {'{', 15, 0x7ffe},    {'|', 11, 0x7fc},     {'}', 14, 0x3ffd},
        {'~', 13, 0x1ffd},    {127, 28, 0xffffffc}, {128, 20, 0xfffe6},
        {129, 22, 0x3fffd2},  {130, 20, 0xfffe7},   {131, 20, 0xfffe8},
        {132, 22, 0x3fffd3},  {133, 22, 0x3fffd4},  {134, 22, 0x3fffd5},
        {135, 23, 0x7fffd9},  {136, 22, 0x3fffd6},  {137, 23, 0x7fffda},
        {138, 23, 0x7fffdb},  {139, 23, 0x7fffdc},  {140, 23, 0x7fffdd},
        {141, 23, 0x7fffde},  {142, 24, 0xffffeb},  {143, 23, 0x7fffdf},
        {144, 24, 0xffffec},  {145, 24, 0xffffed},  {146, 22, 0x3fffd7},
        {147, 23, 0x7fffe0},  {148, 24, 0xffffee},  {149, 23, 0x7fffe1},
        {150, 23, 0x7fffe2},  {151, 23, 0x7fffe3},  {152, 23, 0x7fffe4},
        {153, 21, 0x1fffdc},  {154, 22, 0x3fffd8},  {155, 23, 0x7fffe5},
        {156, 22, 0x3fffd9},  {157, 23, 0x7fffe6},  {158, 23, 0x7fffe7},
        {159, 24, 0xffffef},  {160, 22, 0x3fffda},  {161, 21, 0x1fffdd},
        {162, 20, 0xfffe9},   {163, 22, 0x3fffdb},  {164, 22, 0x3fffdc},
        {165, 23, 0x7fffe8},  {166, 23, 0x7fffe9},  {167, 21, 0x1fffde},
        {168, 23, 0x7fffea},  {169, 22, 0x3fffdd},  {170, 22, 0x3fffde},
        {171, 24, 0xfffff0},  {172, 21, 0x1fffdf},  {173, 22, 0x3fffdf},
        {174, 23, 0x7fffeb},  {175, 23, 0x7fffec},  {176, 21, 0x1fffe0},
        {177, 21, 0x1fffe1},  {178, 22, 0x3fffe0},  {179, 21, 0x1fffe2},
        {180, 23, 0x7fffed},  {181, 22, 0x3fffe1},  {182, 23, 0x7fffee},
        {183, 23, 0x7fffef},  {184, 20, 0xfffea},   {185, 22, 0x3fffe2},
        {186, 22, 0x3fffe3},  {187, 22, 0x3fffe4},  {188, 23, 0x7ffff0},
        {189, 22, 0x3fffe5},  {190, 22, 0x3fffe6},  {191, 23, 0x7ffff1},
        {192, 26, 0x3ffffe0}, {193, 26, 0x3ffffe1}, {194, 20, 0xfffeb},
        {195, 19, 0x7fff1},   {196, 22, 0x3fffe7},  {197, 23, 0x7ffff2},
        {198, 22, 0x3fffe8},  {199, 25, 0x1ffffec}, {200, 26, 0x3ffffe2},
        {201, 26, 0x3ffffe3}, {202, 26, 0x3ffffe4}, {203, 27, 0x7ffffde},
        {204, 27, 0x7ffffdf}, {205, 26, 0x3ffffe5}, {206, 24, 0xfffff1},
        {207, 25, 0x1ffffed}, {208, 19, 0x7fff2},   {209, 21, 0x1fffe3},
        {210, 26, 0x3ffffe6}, {211, 27, 0x7ffffe0}, {212, 27, 0x7ffffe1},
        {213, 26, 0x3ffffe7}, {214, 27, 0x7ffffe2}, {215, 24, 0xfffff2},
        {216, 21, 0x1fffe4},  {217, 21, 0x1fffe5},  {218, 26, 0x3ffffe8},
        {219, 26, 0x3ffffe9}, {220, 28, 0xffffffd}, {221, 27, 0x7ffffe3},
        {222, 27, 0x7ffffe4}, {223, 27, 0x7ffffe5}, {224, 20, 0xfffec},
        {225, 24, 0xfffff3},  {226, 20, 0xfffed},   {227, 21, 0x1fffe6},
        {228, 22, 0x3fffe9},  {229, 21, 0x1fffe7},  {230, 21, 0x1fffe8},
        {231, 23, 0x7ffff3},  {232, 22, 0x3fffea},  {233, 22, 0x3fffeb},
        {234, 25, 0x1ffffee}, {235, 25, 0x1ffffef}, {236, 24, 0xfffff4},
        {237, 24, 0xfffff5},  {238, 26, 0x3ffffea}, {239, 23, 0x7ffff4},
        {240, 26, 0x3ffffeb}, {241, 27, 0x7ffffe6}, {242, 26, 0x3ffffec},
        {243, 26, 0x3ffffed}, {244, 27, 0x7ffffe7}, {245, 27, 0x7ffffe8},
        {246, 27, 0x7ffffe9}, {247, 27, 0x7ffffea}, {248, 27, 0x7ffffeb},
        {249, 28, 0xffffffe}, {250, 27, 0x7ffffec}, {251, 27, 0x7ffffed},
        {252, 27, 0x7ffffee}, {253, 27, 0x7ffffef}, {254, 27, 0x7fffff0},
        {255, 26, 0x3ffffee},
    }};

    constexpr size_t validate_http2_huffman_table() {
        for (size_t i = 0; i < http2_huffman_table.size(); ++i) {
            const huffman_entry_t& entry = http2_huffman_table[i];
            if (entry.symbol != i) {
                return i;
            }
            if (entry.len > 30 || entry.len < 5) {
                return i;
            }
            uint32_t max_code =
                std::numeric_limits<uint32_t>::max() >> (32 - entry.len);
            if (entry.code > max_code) {
                return i;
            }
        }
        return 0;
    }

    constexpr huffman_entry_t find_huffman_code_for_symbol(uint8_t symbol) {
        static_assert(http2_huffman_table.size() ==
                      std::numeric_limits<uint8_t>::max() + 1);
        const huffman_entry_t& entry = http2_huffman_table[symbol];
        assert(entry.symbol == symbol);
        return entry;
    }

    constexpr std::optional<huffman_entry_t>
    find_huffman_code_for_code(uint32_t code, uint8_t len) {
        auto it =
            std::find_if(http2_huffman_table.begin(), http2_huffman_table.end(),
                         [code, len](const huffman_entry_t& entry) {
                             return entry.len == len && entry.code == code;
                         });
        if (it != http2_huffman_table.end()) {
            return *it;
        }
        return std::nullopt;
    }

    constexpr size_t http2_encoded_huffman_len(std::string_view text) {
        size_t len = 0;
        while (!text.empty()) {
            uint8_t ch = text[0];
            text.remove_prefix(1);
            auto hcode = find_huffman_code_for_symbol(ch);
            len += hcode.len;
        }
        return (len / 8) + (len % 8 != 0);
    }

    constexpr uint8_t
    http2_encode_one_huffman_code(const huffman_entry_t& entry,
                                  uint8_t used_bits, uint8_t*& p) {
        uint8_t len = entry.len;
        uint32_t code = entry.code << (32 - len);
        while (len > 0) {
            len -= 1;
            uint8_t bit_i = bits::check<31>(code);
            code <<= 1;
            *p |= bit_i;
            used_bits += 1;
            if (used_bits == 8) {
                used_bits = 0;
                p += 1;
            }
            else {
                *p <<= 1;
            }
        }
        return used_bits;
    }

    constexpr size_t http2_encode_huffman(std::string_view text, uint8_t* p) {
        uint8_t* start_p = p;
        uint8_t used_bits = 0;
        while (!text.empty()) {
            uint8_t ch = text[0];
            text.remove_prefix(1);
            auto hcode = find_huffman_code_for_symbol(ch);
            used_bits = http2_encode_one_huffman_code(hcode, used_bits, p);
        }
        if (used_bits != 0) {
            *p <<= 8 - used_bits - 1;
            const uint8_t ones_mask = 0xff >> used_bits;
            *p |= ones_mask;
        }
        return static_cast<size_t>(p - start_p);
    }

    struct bits_reader_t {
        std::span<const uint8_t> bytes;
        uint8_t byte = 0;
        uint8_t used_bits = 8;

        constexpr bool empty() const noexcept {
            return bytes.empty() && used_bits == 8;
        }

        constexpr uint8_t get_next_bit(std::errc& ec) {
            if (used_bits == 8) {
                used_bits = 0;
                if (bytes.empty()) {
                    ec = std::errc::no_buffer_space;
                    return 0;
                }
                byte = bytes[0];
                bytes = bytes.subspan(1);
            }
            uint8_t bit_i = bits::check<7>(byte);
            used_bits += 1;
            byte <<= 1;
            return bit_i;
        }
    };

    constexpr size_t
    http2_decoded_huffman_len(std::span<const uint8_t> encoded) {
        std::errc ec = {};
        size_t len = 0;
        bits_reader_t bits_reader{encoded};
        while (!bits_reader.empty()) {
            bool found_code = false;
            uint32_t code = 0;
            uint8_t code_len = 0;
            for (uint8_t i = 0; i < 4; ++i) {
                code |= bits_reader.get_next_bit(ec);
                if (ec != std::errc{}) {
                    code >>= 1;
                    if (code == std::numeric_limits<uint32_t>::max() >>
                                    (32 - code_len)) {
                        return len;
                    }
                    return 0;
                }
                code_len += 1;
                code <<= 1;
            }
            while (code_len < 30) {
                code |= bits_reader.get_next_bit(ec);
                if (ec != std::errc{}) {
                    code >>= 1;
                    if (code == std::numeric_limits<uint32_t>::max() >>
                                    (32 - code_len)) {
                        return len;
                    }
                    return 0;
                }
                code_len += 1;
                auto hcode = find_huffman_code_for_code(code, code_len);
                code <<= 1;
                if (!hcode.has_value()) {
                    continue;
                }
                len += 1;
                found_code = true;
                break;
            }
            if (!found_code) {
                ec = std::errc::invalid_argument;
                return 0;
            }
        }
        return len;
    }

    constexpr size_t http2_decode_huffman(std::span<const uint8_t> encoded,
                                          char* p, std::errc& ec) {
        ec = {};
        char* start_p = p;
        bits_reader_t bits_reader{encoded};
        while (!bits_reader.empty()) {
            bool found_code = false;
            uint32_t code = 0;
            uint8_t code_len = 0;
            for (uint8_t i = 0; i < 4; ++i) {
                code |= bits_reader.get_next_bit(ec);
                if (ec != std::errc{}) {
                    code >>= 1;
                    if (code == std::numeric_limits<uint32_t>::max() >>
                                    (32 - code_len)) {
                        ec = {};
                    }
                    return static_cast<size_t>(p - start_p);
                }
                code_len += 1;
                code <<= 1;
            }
            while (code_len < 30) {
                code |= bits_reader.get_next_bit(ec);
                if (ec != std::errc{}) {
                    code >>= 1;
                    if (code == std::numeric_limits<uint32_t>::max() >>
                                    (32 - code_len)) {
                        ec = {};
                    }
                    return static_cast<size_t>(p - start_p);
                }
                code_len += 1;
                auto hcode = find_huffman_code_for_code(code, code_len);
                code <<= 1;
                if (!hcode.has_value()) {
                    continue;
                }
                *p++ = static_cast<char>(hcode->symbol);
                found_code = true;
                break;
            }
            if (!found_code) {
                ec = std::errc::invalid_argument;
                return static_cast<size_t>(p - start_p);
            }
        }
        return static_cast<size_t>(p - start_p);
    }

    template <std::size_t N>
    constexpr std::array<uint8_t, N> test_encode_h(std::string_view text) {
        std::array<uint8_t, N> buff{};
        http2_encode_huffman(text, buff.data());
        return buff;
    }

    template <std::size_t N>
    struct test_decode_h_result {
        size_t len;
        std::errc ec;
        std::array<char, N> text;
    };

    template <std::size_t N1, std::size_t N2>
    constexpr test_decode_h_result<N1>
    test_decode_h(const std::array<uint8_t, N2>& encoded) {
        test_decode_h_result<N1> res{};
        res.len = http2_decode_huffman(encoded, res.text.data(), res.ec);
        return res;
    }

    template <std::size_t N>
    constexpr bool equal(const std::array<char, N>& in1, std::string_view in2) {
        for (size_t i = 0; i < N; ++i) {
            if (in1[i] != in2[i]) {
                return false;
            }
        }
        return true;
    }

    [[maybe_unused]] void test_huffman_encoding_decoding() {
        using namespace std::string_view_literals;

        constexpr size_t ret = validate_http2_huffman_table();
        static_assert(ret == 0);

        constexpr auto text1 = "www.example.com"sv;
        constexpr auto test1 =
            std::array<uint8_t, 12>{0xf1, 0xe3, 0xc2, 0xe5, 0xf2, 0x3a,
                                    0x6b, 0xa0, 0xab, 0x90, 0xf4, 0xff};
        static_assert(http2_encoded_huffman_len(text1) == test1.size());
        static_assert(http2_decoded_huffman_len(test1) == text1.size());
        constexpr auto res1 = test_encode_h<test1.size()>(text1);
        static_assert(test1 == res1);
        constexpr auto dec1 = test_decode_h<text1.size()>(res1);
        static_assert(equal(dec1.text, text1));
        static_assert(dec1.ec == std::errc{});
        static_assert(dec1.len == text1.size());

        constexpr auto text2 = "no-cache"sv;
        constexpr auto test2 =
            std::array<uint8_t, 6>{0xa8, 0xeb, 0x10, 0x64, 0x9c, 0xbf};
        static_assert(http2_encoded_huffman_len(text2) == test2.size());
        static_assert(http2_decoded_huffman_len(test2) == text2.size());
        constexpr auto res2 = test_encode_h<test2.size()>(text2);
        static_assert(test2 == res2);
        constexpr auto dec2 = test_decode_h<text2.size()>(res2);
        static_assert(equal(dec2.text, text2));
        static_assert(dec2.ec == std::errc{});
        static_assert(dec2.len == text2.size());

        constexpr auto text3 = "custom-key"sv;
        constexpr auto test3 = std::array<uint8_t, 8>{0x25, 0xa8, 0x49, 0xe9,
                                                      0x5b, 0xa9, 0x7d, 0x7f};
        static_assert(http2_encoded_huffman_len(text3) == test3.size());
        static_assert(http2_decoded_huffman_len(test3) == text3.size());
        constexpr auto res3 = test_encode_h<8>(text3);
        static_assert(test3 == res3);
        constexpr auto dec3 = test_decode_h<text3.size()>(res3);
        static_assert(equal(dec3.text, text3));
        static_assert(dec3.ec == std::errc{});
        static_assert(dec3.len == text3.size());

        constexpr auto text4 = "custom-value"sv;
        constexpr auto test4 = std::array<uint8_t, 9>{
            0x25, 0xa8, 0x49, 0xe9, 0x5b, 0xb8, 0xe8, 0xb4, 0xbf};
        static_assert(http2_encoded_huffman_len(text4) == test4.size());
        static_assert(http2_decoded_huffman_len(test4) == text4.size());
        constexpr auto res4 = test_encode_h<test4.size()>(text4);
        static_assert(test4 == res4);
        constexpr auto dec4 = test_decode_h<text4.size()>(res4);
        static_assert(equal(dec4.text, text4));
        static_assert(dec4.ec == std::errc{});
        static_assert(dec4.len == text4.size());

        constexpr auto text5 = "302"sv;
        constexpr auto test5 = std::array<uint8_t, 2>{0x64, 0x02};
        static_assert(http2_encoded_huffman_len(text5) == test5.size());
        static_assert(http2_decoded_huffman_len(test5) == text5.size());
        constexpr auto res5 = test_encode_h<test5.size()>(text5);
        static_assert(test5 == res5);
        constexpr auto dec5 = test_decode_h<text5.size()>(res5);
        static_assert(equal(dec5.text, text5));
        static_assert(dec5.ec == std::errc{});
        static_assert(dec5.len == text5.size());

        constexpr auto text6 = "private"sv;
        constexpr auto test6 =
            std::array<uint8_t, 5>{0xae, 0xc3, 0x77, 0x1a, 0x4b};
        static_assert(http2_encoded_huffman_len(text6) == test6.size());
        static_assert(http2_decoded_huffman_len(test6) == text6.size());
        constexpr auto res6 = test_encode_h<test6.size()>(text6);
        static_assert(test6 == res6);
        constexpr auto dec6 = test_decode_h<text6.size()>(res6);
        static_assert(equal(dec6.text, text6));
        static_assert(dec6.ec == std::errc{});
        static_assert(dec6.len == text6.size());

        constexpr auto text7 = "Mon, 21 Oct 2013 20:13:21 GMT"sv;
        constexpr auto test7 = std::array<uint8_t, 22>{
            0xd0, 0x7a, 0xbe, 0x94, 0x10, 0x54, 0xd4, 0x44, 0xa8, 0x20, 0x05,
            0x95, 0x04, 0x0b, 0x81, 0x66, 0xe0, 0x82, 0xa6, 0x2d, 0x1b, 0xff};
        static_assert(http2_encoded_huffman_len(text7) == test7.size());
        static_assert(http2_decoded_huffman_len(test7) == text7.size());
        constexpr auto res7 = test_encode_h<test7.size()>(text7);
        static_assert(test7 == res7);
        constexpr auto dec7 = test_decode_h<text7.size()>(res7);
        static_assert(equal(dec7.text, text7));
        static_assert(dec7.ec == std::errc{});
        static_assert(dec7.len == text7.size());

        constexpr auto text8 = "307"sv;
        constexpr auto test8 = std::array<uint8_t, 3>{0x64, 0x0e, 0xff};
        static_assert(http2_encoded_huffman_len(text8) == test8.size());
        static_assert(http2_decoded_huffman_len(test8) == text8.size());
        constexpr auto res8 = test_encode_h<test8.size()>(text8);
        static_assert(test8 == res8);
        constexpr auto dec8 = test_decode_h<text8.size()>(res8);
        static_assert(equal(dec8.text, text8));
        static_assert(dec8.ec == std::errc{});
        static_assert(dec8.len == text8.size());

        constexpr auto text9 = "Mon, 21 Oct 2013 20:13:22 GMT"sv;
        constexpr auto test9 = std::array<uint8_t, 22>{
            0xd0, 0x7a, 0xbe, 0x94, 0x10, 0x54, 0xd4, 0x44, 0xa8, 0x20, 0x05,
            0x95, 0x04, 0x0b, 0x81, 0x66, 0xe0, 0x84, 0xa6, 0x2d, 0x1b, 0xff};
        static_assert(http2_encoded_huffman_len(text9) == test9.size());
        static_assert(http2_decoded_huffman_len(test9) == text9.size());
        constexpr auto res9 = test_encode_h<test9.size()>(text9);
        static_assert(test9 == res9);
        constexpr auto dec9 = test_decode_h<text9.size()>(res9);
        static_assert(equal(dec9.text, text9));
        static_assert(dec9.ec == std::errc{});
        static_assert(dec9.len == text9.size());

        constexpr auto text10 = "gzip"sv;
        constexpr auto test10 = std::array<uint8_t, 3>{0x9b, 0xd9, 0xab};
        static_assert(http2_encoded_huffman_len(text10) == test10.size());
        static_assert(http2_decoded_huffman_len(test10) == text10.size());
        constexpr auto res10 = test_encode_h<test10.size()>(text10);
        static_assert(test10 == res10);
        constexpr auto dec10 = test_decode_h<text10.size()>(res10);
        static_assert(equal(dec10.text, text10));
        static_assert(dec10.ec == std::errc{});
        static_assert(dec10.len == text10.size());

        constexpr auto text11 =
            "foo=ASDJKHQKBZXOQWEOPIUAXQWEOIU; max-age=3600; version=1"sv;
        constexpr auto test11 = std::array<uint8_t, 45>{
            0x94, 0xe7, 0x82, 0x1d, 0xd7, 0xf2, 0xe6, 0xc7, 0xb3,
            0x35, 0xdf, 0xdf, 0xcd, 0x5b, 0x39, 0x60, 0xd5, 0xaf,
            0x27, 0x08, 0x7f, 0x36, 0x72, 0xc1, 0xab, 0x27, 0x0f,
            0xb5, 0x29, 0x1f, 0x95, 0x87, 0x31, 0x60, 0x65, 0xc0,
            0x03, 0xed, 0x4e, 0xe5, 0xb1, 0x06, 0x3d, 0x50, 0x07};
        static_assert(http2_encoded_huffman_len(text11) == test11.size());
        static_assert(http2_decoded_huffman_len(test11) == text11.size());
        constexpr auto res11 = test_encode_h<test11.size()>(text11);
        static_assert(test11 == res11);
        constexpr auto dec11 = test_decode_h<text11.size()>(res11);
        static_assert(equal(dec11.text, text11));
        static_assert(dec11.ec == std::errc{});
        static_assert(dec11.len == text11.size());
    }
} // namespace

namespace {
    void validate_request_pseudo_headers(const http::request& req,
                                         std::string_view scheme,
                                         bool has_method, bool has_authority,
                                         bool has_path, bool has_scheme,
                                         std::error_code& ec) noexcept {
        if (!has_method) {
            http2_printf("(hpack) decoded request didn't contain :method "
                         "header at first !!!\n");
            ec = http::make_error(http::error::bad_method);
            return;
        }
        if (req.method == http::verb::connect) {
            if (has_path || has_scheme) {
                http2_printf("(hpack) decoded CONNECT request contains "
                             ":path or :scheme "
                             "header !!!\n");
                ec = http::make_error(http::error::bad_target);
                return;
            }
            if (!has_authority) {
                http2_printf("(hpack) decoded CONNECT request didn't "
                             "contain :authority "
                             "header !!!\n");
                ec = http::make_error(http::error::bad_target);
                return;
            }
        }
        else if (!has_path || !has_scheme) {
            http2_printf("(hpack) decoded request didn't contain :path or "
                         ":scheme "
                         "header at first !!!\n");
            ec = http::make_error(http::error::bad_target);
            return;
        }
        if ((scheme == "http" || scheme == "https") && req.target.empty()) {
            http2_printf("(hpack) decoded request contain empty :path "
                         "header !!!\n");
            ec = http::make_error(http::error::bad_target);
            return;
        }
    }
} // namespace

std::size_t http2::encoded_huffman_len(std::string_view text) noexcept {
    return http2_encoded_huffman_len(text);
}

std::size_t http2::decoded_huffman_len(std::span<const uint8_t> buff) noexcept {
    return http2_decoded_huffman_len(buff);
}

std::size_t http2::encode_huffman(std::string_view text,
                                  uint8_t* out) noexcept {
    return http2_encode_huffman(text, out);
}

size_t http2::decode_huffman(std::span<const uint8_t> buff, uint8_t* out,
                             std::errc& ec) {
    return http2_decode_huffman(buff, reinterpret_cast<char*>(out), ec);
}

void dynamic_table::set_max_size(std::size_t n) noexcept {
    if (n == 0) {
        headers_.clear();
        max_size_ = 0;
        headers_size_ = 0;
        return;
    }
    max_size_ = n;
    if (n < headers_size_) {
        const std::size_t excess_size = headers_size_ - n;
        remove_size(excess_size);
    }
}

void dynamic_table::add(std::string_view name, std::string_view value) {
    const std::size_t header_size = 32 + name.size() + value.size();
    if (header_size > max_size_) {
        headers_.clear();
        headers_size_ = 0;
        return;
    }
    if (headers_size_ + header_size > max_size_) {
        remove_size(headers_size_ + header_size - max_size_);
    }
    if (headers_.full()) {
        headers_.set_capacity(std::max(headers_.capacity() * 2, size_t{1}));
    }
    headers_.emplace_back(name, value);
    headers_size_ += header_size;
}

std::optional<size_t>
dynamic_table::find(std::string_view name,
                    std::string_view value) const noexcept {
    auto it = headers_.find_if(
        [&](const std::pair<std::string, std::string>& header) {
            return iequal(name, header.first) && value == header.second;
        });
    if (it == headers_.end()) {
        return std::nullopt;
    }
    return (headers_.size() - std::distance(headers_.begin(), it)) + 61;
}

std::optional<size_t>
dynamic_table::find(std::string_view name) const noexcept {
    auto it = headers_.find_if(
        [&](const std::pair<std::string, std::string>& header) {
            return iequal(name, header.first);
        });
    if (it == headers_.end()) {
        return std::nullopt;
    }
    return (headers_.size() - std::distance(headers_.begin(), it)) + 61;
}

void dynamic_table::remove_size(size_t n) noexcept {
    assert(n <= headers_size_);
    while (n > 0) {
        assert(!headers_.empty());
        if (headers_.empty()) {
            break;
        }
        std::pair<std::string, std::string> header;
        headers_.pop_front(header);
        size_t header_size = 32 + header.first.size() + header.second.size();
        assert(headers_size_ >= header_size);
        n -= std::min(n, header_size);
        ;
        headers_size_ -= std::min(headers_size_, header_size);
    }
}

void hpack_encoder::encode(const headers& hdrs, dynamic_buffer out,
                           bool indexed, bool huffman, bool never_indexed) {
    for (const auto& hdr : hdrs) {
        encode_header(hdr.first, hdr.second, out, huffman, indexed,
                      never_indexed);
    }
}

void hpack_encoder::encode(http::verb method, std::string_view path,
                           const headers& hdrs, dynamic_buffer out,
                           bool indexed, bool huffman, bool never_indexed) {
    encode_header(":method", http::verb_to_string(method), out, huffman,
                  indexed, never_indexed);
    if (method != http::verb::connect) {
        encode_header(":scheme", scheme_, out, huffman, indexed, never_indexed);
        encode_header(":path", path, out, huffman, indexed, never_indexed);
    }
    else {
        encode_header(":authority", path, out, huffman, indexed, never_indexed);
    }
    std::string_view host_header_name =
        http::field_to_string(http::field::host);
    auto auth_it = hdrs.find(":authority");
    if (auth_it == hdrs.end()) {
        auth_it = hdrs.find(host_header_name);
    }
    if (auth_it != hdrs.end() && method != http::verb::connect) {
        encode_header(":authority", auth_it->second, out, huffman, indexed,
                      never_indexed);
    }
    else if (!host_.empty() && method != http::verb::connect) {
        encode_header(":authority", host_, out, huffman, indexed,
                      never_indexed);
    }

    std::string_view cookie_header_name =
        http::field_to_string(http::field::cookie);
    // note: host header is allowed to exist with :authority
    // connect requests don't contain host in http2
    for (auto it = hdrs.begin(); it != hdrs.end(); ++it) {
        if (it == auth_it) {
            continue;
        }
        if (method == http::verb::connect &&
            iequal(it->first, host_header_name)) {
            continue;
        }
        if (iequal(it->first, cookie_header_name)) {
            // Therefore, the following two lists of Cookie header
            // fields are semantically equivalent. cookie: a=b; c=d;
            // e=f cookie: a=b cookie: c=d cookie: e=f
            for (auto cookie_value : it->second | split(";")) {
                while (!cookie_value.empty() && cookie_value.front() == ' ') {
                    cookie_value.remove_prefix(1);
                }
                while (!cookie_value.empty() && cookie_value.back() == ' ') {
                    cookie_value.remove_suffix(1);
                }
                if (cookie_value.empty()) {
                    continue;
                }
                encode_header(cookie_header_name, cookie_value, out, huffman,
                              indexed, never_indexed);
            }
            continue;
        }
        encode_header(it->first, it->second, out, huffman, indexed,
                      never_indexed);
    }
}

void hpack_encoder::encode(uint32_t res_status, const headers& hdrs,
                           dynamic_buffer out, bool indexed, bool huffman,
                           bool never_indexed) {
    encode_header(":status", std::to_string(res_status), out, huffman, indexed,
                  false);
    for (const auto& h : hdrs) {
        encode_header(h.first, h.second, out, huffman, indexed, never_indexed);
    }
}

void hpack_encoder::encode_length_string(std::string_view text, bool huffman,
                                         dynamic_buffer out) {
    /*
    +---+---+-----------------------+
    | H |     Name Length (7+)      |
    +---+---------------------------+
    |  Name String (Length octets)  |
    +---+---------------------------+
    */
    if (text.empty()) {
        out.push_back('\0');
        return;
    }
    std::string encoded_text;
    if (huffman) {
        encoded_text.resize(encoded_huffman_len(text));
        encode_huffman(text, reinterpret_cast<uint8_t*>(encoded_text.data()));
        text = encoded_text;
    }
    // encode length
    {
        size_t len = hpack_integer_encoding_len<7>(text.size());
        auto buff = out.prepare(len);
        hpack_encode_integer<7>(text.size(), buff.data_as<uint8_t>());
        bits::assign<7>(buff.data_as<uint8_t>()[0], huffman);
    }
    // insert name string
    out.insert(text.data(), text.size());
}

void hpack_encoder::encode_header(std::string_view name, std::string_view value,
                                  dynamic_buffer out, bool huffman,
                                  bool indexed, bool never_indexed) {
    auto existing_index = never_indexed
                              ? std::nullopt
                              : find_predefined_http2_header(name, value);
    if (!never_indexed && !existing_index.has_value()) {
        existing_index = dynamic_table_.find(name, value);
    }
    if (existing_index.has_value()) {
        /*
          0   1   2   3   4   5   6   7
        +---+---+---+---+---+---+---+---+
        | 1 |        Index (7+)         |
        +---+---------------------------+
        */
        size_t len = hpack_integer_encoding_len<7>(*existing_index);
        auto buff = out.prepare(len);
        hpack_encode_integer<7>(*existing_index, buff.data_as<uint8_t>());
        bits::set<7>(buff.data_as<uint8_t>()[0]);
        return;
    }

    existing_index = find_predefined_http2_name(name);
    if (!existing_index.has_value()) {
        existing_index = dynamic_table_.find(name);
    }
    encode_name_index(indexed, never_indexed, existing_index.value_or(0), out);

    if (!existing_index.has_value()) {
        encode_length_string(to_lower(name), huffman, out);
    }
    encode_length_string(value, huffman, out);

    if (indexed) {
        dynamic_table_.add(name, value);
    }
}

void hpack_encoder::encode_name_index(bool indexed, bool never_indexed,
                                      size_t index, dynamic_buffer out) {
    if (never_indexed || !indexed) {
        /*
        not indexed:
         0   1   2   3   4   5   6   7
        +---+---+---+---+---+---+---+---+
        | 0 | 0 | 0 | 0 |  Index (4+)   |
        +---+---+-----------------------+
        never indexed:
         0   1   2   3   4   5   6   7
        +---+---+---+---+---+---+---+---+
        | 0 | 0 | 0 | 1 |  Index (4+)   |
        +---+---+-----------------------+
        */
        size_t len = hpack_integer_encoding_len<4>(index);
        auto buff = out.prepare(len);
        hpack_encode_integer<4>(index, buff.data_as<uint8_t>());
        buff.data_as<uint8_t>()[0] &= 0xff >> 4;
        bits::assign<4>(buff.data_as<uint8_t>()[0], never_indexed);
    }
    else {
        /*
        indexed:
         0   1   2   3   4   5   6   7
        +---+---+---+---+---+---+---+---+
        | 0 | 1 |      Index (6+)       |
        +---+---+-----------------------+
        */
        size_t len = hpack_integer_encoding_len<6>(index);
        auto buff = out.prepare(len);
        hpack_encode_integer<6>(index, buff.data_as<uint8_t>());
        bits::clear<7>(buff.data_as<uint8_t>()[0]);
        bits::set<6>(buff.data_as<uint8_t>()[0]);
    }
}

void hpack_decoder::decode(const_buffer input, headers& hdrs,
                           std::error_code& ec) {
    ec.clear();
    std::errc ecode{};
    std::string name;
    std::string value;
    while (!input.empty()) {
        input += decode_header(input, name, value, ecode);
        if (ecode != std::errc{}) {
            ec = std::make_error_code(ecode);
            return;
        }
        hdrs.insert(name, value);
    }
}

void hpack_decoder::decode(const_buffer input, request& req,
                           std::error_code& ec) {
    ec.clear();
    req.version = http::version::v1_1;
    req.method = verb::invalid;
    req.target.clear();
    req.headers.clear();

    std::errc ecode{};
    std::string name;
    std::string value;
    std::string scheme;
    std::string authority;
    bool has_method = false;
    bool has_authority = false;
    bool has_path = false;
    bool has_scheme = false;
    bool passed_pseudo = false;

    while (!input.empty()) {
        input += decode_header(input, name, value, ecode);
        if (ecode != std::errc{}) {
            ec = std::make_error_code(ecode);
            return;
        }
        if (name.empty()) {
            ec = http::make_error(http::error::bad_field);
            return;
        }
        if (name[0] == ':') {
            if (passed_pseudo) {
                http2_printf("(hpack) decoded request contains "
                             "pseudo header after non "
                             "pseudo !!!\n");
                ec = http::make_error(http::error::bad_field);
                return;
            }
            if (name == ":method") {
                if (has_method) {
                    http2_printf("(hpack) decoded request contains "
                                 "two :method headers !!!\n");
                    ec = http::make_error(http::error::bad_field);
                    return;
                }
                has_method = true;
                req.method = http::string_to_verb(value);
                if (req.method == verb::invalid) {
                    ec = http::make_error(http::error::bad_method);
                    return;
                }
            }
            else if (name == ":path") {
                if (has_path) {
                    http2_printf("(hpack) duplicate :path "
                                 "header: '%s' !!!\n",
                                 value.c_str());
                    ec = http::make_error(http::error::bad_field);
                    return;
                }
                has_path = true;
                req.target = value;
            }
            else if (name == ":authority") {
                if (has_authority) {
                    http2_printf("(hpack) duplicate :authority "
                                 "header: '%s' !!!\n",
                                 value.c_str());
                    ec = http::make_error(http::error::bad_field);
                    return;
                }
                has_authority = true;
                authority = value;
            }
            else if (name == ":scheme") {
                if (has_scheme) {
                    http2_printf("(hpack) duplicate :scheme header: "
                                 "'%s' !!!\n",
                                 value.c_str());
                    ec = http::make_error(http::error::bad_field);
                    return;
                }
                has_scheme = true;
                scheme = value;
                req.headers.insert(name, value);
            }
            else {
                http2_printf("(hpack) invalid request pseudo "
                             "header: (%s: %s) !\n",
                             name.c_str(), value.c_str());
                ec = http::make_error(http::error::bad_field);
                return;
            }
        }
        else {
            if (!passed_pseudo) {
                validate_request_pseudo_headers(req, scheme, has_method,
                                                has_authority, has_path,
                                                has_scheme, ec);
                if (ec) {
                    return;
                }
                if (req.method == verb::connect) {
                    assert(req.target.empty());
                    req.target = std::move(authority);
                }
                else {
                    if (!scheme_.empty() && scheme != scheme) {
                        ec = http::make_error(http::error::bad_value);
                        return;
                    }
                    req.headers.insert(http::field::host, authority);
                }
            }
            passed_pseudo = true;

            if (name == "cookie") {
                auto it = req.headers.find(name);
                if (it != req.headers.end()) {
                    it->second += "; ";
                    it->second += value;
                }
                else {
                    req.headers.insert(name, value);
                }
            }
            else {
                req.headers.insert(name, value);
            }
        }
    }
}

void hpack_decoder::decode(const_buffer input, response& res,
                           std::error_code& ec) {
    ec.clear();
    res.clear(false);
    res.version = http::version::v1_1;

    bool got_status = false;
    bool passed_pseudo = false;
    std::errc ecode{};
    std::string name;
    std::string value;

    res.version = http::version::v1_1;
    while (!input.empty()) {
        input += decode_header(input, name, value, ecode);
        if (ecode != std::errc{}) {
            ec = std::make_error_code(ecode);
            return;
        }
        if (name.empty()) {
            ec = http::make_error(http::error::bad_field);
            return;
        }
        if (name[0] == ':') {
            if (passed_pseudo) {
                http2_printf("(hpack) decoded response "
                             "contains pseudo header after "
                             "non pseudo !!!\n");
                ec = http::make_error(http::error::bad_field);
                return;
            }
            if (name == ":status") {
                if (got_status) {
                    http2_printf("(hpack) decoded response contains "
                                 "two :status headers !!!\n");
                    ec = http::make_error(http::error::bad_field);
                    return;
                }
                res.status = to_uint32(value, 10, ec);
                if (ec) {
                    http2_printf("(hpack) decoded response :status "
                                 "value is invalid! (%s)\n",
                                 value.c_str());
                    ec = http::make_error(http::error::bad_status);
                    return;
                }
                got_status = true;
            }
            else {
                http2_printf("(hpack) invalid response pseudo "
                             "header: (%s: %s) !\n",
                             name.c_str(), value.c_str());
                ec = http::make_error(http::error::bad_field);
                return;
            }
        }
        else {
            passed_pseudo = true;
            if (!got_status) {
                http2_printf("(hpack) decoded response didn't "
                             "contain :status header "
                             "at first !!!\n");
                ec = http::make_error(http::error::bad_field);
                return;
            }

            if (name == "cookie") {
                auto it = res.headers.find(name);
                if (it != res.headers.end()) {
                    it->second.reserve(it->second.size() + value.size() + 3);
                    it->second += "; ";
                    it->second += value;
                }
                else {
                    res.headers.insert(name, value);
                }
            }
            else {
                res.headers.insert(name, value);
            }
        }
    }
}

size_t hpack_decoder::decode_header(const_buffer input, std::string& name,
                                    std::string& value, std::errc& ec) {
    name.clear();
    value.clear();
    if (input.empty()) {
        ec = std::errc::no_buffer_space;
        return 0;
    }
    size_t start_n = input.size();
    uint8_t first_byte = input.data_as<const uint8_t>()[0];
    if ((first_byte >> 5) == 1) {
        /*
        dynamic table size update:
         0   1   2   3   4   5   6   7
        +---+---+---+---+---+---+---+---+
        | 0 | 0 | 1 |   Max size (5+)   |
        +---+---------------------------+
        */
        uint64_t new_size = 0;
        size_t len = hpack_decode_integer<5>(input.data_as<const uint8_t>(),
                                             input.size(), new_size);
        if (len == 0) {
            http2_printf("(hpack) hpack_decode_integer<5>() failed !\n");
            ec = std::errc::no_buffer_space;
            return 0;
        }
        http2_printf("(hpack) received dynamic table size update = %d\n",
                     (int)new_size);
        if (new_size > dynamic_table_.max_size()) {
            http2_printf("(hpack) the new dynamic table size is "
                         "greater than the "
                         "current max size (%d) !\n",
                         (int)dynamic_table_.max_size());
            ec = std::errc::value_too_large;
            return 0;
        }
        dynamic_table_.set_max_size(static_cast<std::size_t>(new_size));
        return len;
    }
    if (bits::check<7>(first_byte)) {
        /*
        index:
          0   1   2   3   4   5   6   7
        +---+---+---+---+---+---+---+---+
        | 1 |        Index (7+)         |
        +---+---------------------------+
        */
        uint64_t index64 = 0;
        size_t len = hpack_decode_integer<7>(input.data_as<const uint8_t>(),
                                             input.size(), index64);
        if (len == 0) {
            http2_printf("(hpack) hpack_decode_integer<7>() failed !\n");
            ec = std::errc::no_buffer_space;
            return 0;
        }
        std::size_t index = static_cast<std::size_t>(index64);
        if (index == 0) {
            http2_printf("(hpack) hpack_decode_integer<7>() "
                         "resulted in zero index !!!\n");
            ec = std::errc::no_buffer_space;
            return 0;
        }
        index -= 1;
        if (index < predefined_http2_headers.size()) {
            name = predefined_http2_headers[index].first;
            value = predefined_http2_headers[index].second;
        }
        else if (auto header = dynamic_table_.get_header(
                     index - predefined_http2_headers.size())) {
            name = to_lower(header->first);
            value = header->second;
        }
        else {
            http2_printf("(hpack) index %zu was not found in "
                         "static and dynamic tables !\n",
                         index + 1);
            ec = std::errc::invalid_argument;
            return 0;
        }
        return len;
    }
    bool indexed = false;
    bool never_indexed = false;
    input += decode_header_name(input, name, indexed, never_indexed, ec);
    if (ec != std::errc{}) {
        return start_n - input.size();
    }
    input += decode_length_string(input, value, ec);
    if (ec != std::errc{}) {
        return start_n - input.size();
    }
    for (char ch : name) {
        if (!std::islower(ch)) {
            // http2_printf("!!! recevied header name: '%s' is not
            // in lower case
            // !!!\n", name.c_str()); ec =
            // std::errc::invalid_argument; return start_n
            // - input.size();
        }
    }
    if (indexed && !never_indexed) {
        dynamic_table_.add(name, value);
    }
    return start_n - input.size();
}

size_t hpack_decoder::decode_header_name(const_buffer input, std::string& name,
                                         bool& indexed, bool& never_indexed,
                                         std::errc& ec) {
    size_t start_n = input.size();
    if (input.empty()) {
        ec = std::errc::no_buffer_space;
        return 0;
    }
    uint8_t first_byte = input.data_as<const uint8_t>()[0];
    uint64_t index64 = 0;
    indexed = false;
    never_indexed = false;
    if (bits::check<6>(first_byte)) {
        /*
        indexed:
         0   1   2   3   4   5   6   7
        +---+---+---+---+---+---+---+---+
        | 0 | 1 |      Index (6+)       |
        +---+---+-----------------------+
        */
        indexed = true;
        size_t len = hpack_decode_integer<6>(input.data_as<const uint8_t>(),
                                             input.size(), index64);
        if (len == 0) {
            http2_printf("(hpack) hpack_decode_integer<6>() failed !!!\n");
            ec = std::errc::no_buffer_space;
            return 0;
        }
        input += len;
    }
    else if ((first_byte >> 4) == 0 || (first_byte >> 4) == 1) {
        /*
        not indexed:
         0   1   2   3   4   5   6   7
        +---+---+---+---+---+---+---+---+
        | 0 | 0 | 0 | 0 |  Index (4+)   |
        +---+---+-----------------------+
        never indexed:
         0   1   2   3   4   5   6   7
        +---+---+---+---+---+---+---+---+
        | 0 | 0 | 0 | 1 |  Index (4+)   |
        +---+---+-----------------------+
        */
        first_byte >>= 4;
        never_indexed = first_byte == 1;
        size_t len = hpack_decode_integer<4>(input.data_as<const uint8_t>(),
                                             input.size(), index64);
        if (len == 0) {
            http2_printf("(hpack) hpack_decode_integer<4>() failed !!!\n");
            ec = std::errc::no_buffer_space;
            return 0;
        }
        input += len;
    }
    else {
        http2_printf("(hpack) invalid header index prefix !\n");
        ec = std::errc::invalid_argument;
        return 0;
    }
    std::size_t index = static_cast<std::size_t>(index64);
    if (index != 0) {
        index -= 1;
        if (index < predefined_http2_headers.size()) {
            name = predefined_http2_headers[index].first;
        }
        else if (auto header = dynamic_table_.get_header(
                     index - predefined_http2_headers.size())) {
            name = to_lower(header->first);
        }
        else {
            http2_printf("(hpack) index %zu was not found in "
                         "static and dynamic tables !!!\n",
                         index + 1);
            ec = std::errc::invalid_argument;
            return 0;
        }
    }
    else {
        input += decode_length_string(input, name, ec);
    }
    return start_n - input.size();
}

size_t hpack_decoder::decode_length_string(const_buffer input,
                                           std::string& text, std::errc& ec) {
    if (input.empty()) {
        ec = std::errc::no_buffer_space;
        return 0;
    }
    size_t start_n = input.size();
    uint8_t first_byte = input.data_as<const uint8_t>()[0];
    bool huffman = bits::check<7>(first_byte);
    uint64_t length64 = 0;
    {
        size_t len = hpack_decode_integer<7>(input.data_as<const uint8_t>(),
                                             input.size(), length64);
        if (len == 0) {
            http2_printf("(hpack) hpack_decode_integer<7>() failed !!!\n");
            ec = std::errc::no_buffer_space;
            return 0;
        }
        input += len;
        if (length64 == 0) {
            return len;
        }
    }
    std::size_t length = static_cast<std::size_t>(length64);
    if (input.size() < length) {
        http2_printf("(hpack) input.size() < length !!!\n");
        ec = std::errc::no_buffer_space;
        return 0;
    }
    auto text_data = input.to_span<uint8_t>(length);
    if (!huffman) {
        input += length;
        text.append(reinterpret_cast<const char*>(text_data.data()),
                    text_data.size());
        return start_n - input.size();
    }
    size_t decode_len = decoded_huffman_len(text_data);
    if (decode_len == 0) {
        http2_printf("(hpack) decoded_huffman_len() failed !!!\n");
        ec = std::errc::invalid_argument;
        return start_n - input.size();
    }
    text.resize(decode_len);
    decode_huffman(text_data, reinterpret_cast<uint8_t*>(text.data()), ec);
    if (ec != std::errc{}) {
        http2_printf("(hpack) decode_huffman() failed !!!\n");
        return start_n - input.size();
    }
    input += text_data.size();
    return start_n - input.size();
}