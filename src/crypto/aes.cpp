#include <rad/crypto/aes.h>

using namespace RAD_LIB_NAMESPACE;
using namespace crypto;

/*
The software implementation is inspired a lot by:
https://github.com/kokke/tiny-AES-c
*/

namespace {

    const uint8_t sbox[256] = {
        0x63, 0x7c, 0x77, 0x7b, 0xf2, 0x6b, 0x6f, 0xc5, 0x30, 0x01, 0x67, 0x2b,
        0xfe, 0xd7, 0xab, 0x76, 0xca, 0x82, 0xc9, 0x7d, 0xfa, 0x59, 0x47, 0xf0,
        0xad, 0xd4, 0xa2, 0xaf, 0x9c, 0xa4, 0x72, 0xc0, 0xb7, 0xfd, 0x93, 0x26,
        0x36, 0x3f, 0xf7, 0xcc, 0x34, 0xa5, 0xe5, 0xf1, 0x71, 0xd8, 0x31, 0x15,
        0x04, 0xc7, 0x23, 0xc3, 0x18, 0x96, 0x05, 0x9a, 0x07, 0x12, 0x80, 0xe2,
        0xeb, 0x27, 0xb2, 0x75, 0x09, 0x83, 0x2c, 0x1a, 0x1b, 0x6e, 0x5a, 0xa0,
        0x52, 0x3b, 0xd6, 0xb3, 0x29, 0xe3, 0x2f, 0x84, 0x53, 0xd1, 0x00, 0xed,
        0x20, 0xfc, 0xb1, 0x5b, 0x6a, 0xcb, 0xbe, 0x39, 0x4a, 0x4c, 0x58, 0xcf,
        0xd0, 0xef, 0xaa, 0xfb, 0x43, 0x4d, 0x33, 0x85, 0x45, 0xf9, 0x02, 0x7f,
        0x50, 0x3c, 0x9f, 0xa8, 0x51, 0xa3, 0x40, 0x8f, 0x92, 0x9d, 0x38, 0xf5,
        0xbc, 0xb6, 0xda, 0x21, 0x10, 0xff, 0xf3, 0xd2, 0xcd, 0x0c, 0x13, 0xec,
        0x5f, 0x97, 0x44, 0x17, 0xc4, 0xa7, 0x7e, 0x3d, 0x64, 0x5d, 0x19, 0x73,
        0x60, 0x81, 0x4f, 0xdc, 0x22, 0x2a, 0x90, 0x88, 0x46, 0xee, 0xb8, 0x14,
        0xde, 0x5e, 0x0b, 0xdb, 0xe0, 0x32, 0x3a, 0x0a, 0x49, 0x06, 0x24, 0x5c,
        0xc2, 0xd3, 0xac, 0x62, 0x91, 0x95, 0xe4, 0x79, 0xe7, 0xc8, 0x37, 0x6d,
        0x8d, 0xd5, 0x4e, 0xa9, 0x6c, 0x56, 0xf4, 0xea, 0x65, 0x7a, 0xae, 0x08,
        0xba, 0x78, 0x25, 0x2e, 0x1c, 0xa6, 0xb4, 0xc6, 0xe8, 0xdd, 0x74, 0x1f,
        0x4b, 0xbd, 0x8b, 0x8a, 0x70, 0x3e, 0xb5, 0x66, 0x48, 0x03, 0xf6, 0x0e,
        0x61, 0x35, 0x57, 0xb9, 0x86, 0xc1, 0x1d, 0x9e, 0xe1, 0xf8, 0x98, 0x11,
        0x69, 0xd9, 0x8e, 0x94, 0x9b, 0x1e, 0x87, 0xe9, 0xce, 0x55, 0x28, 0xdf,
        0x8c, 0xa1, 0x89, 0x0d, 0xbf, 0xe6, 0x42, 0x68, 0x41, 0x99, 0x2d, 0x0f,
        0xb0, 0x54, 0xbb, 0x16};

    const uint8_t rsbox[256] = {
        0x52, 0x09, 0x6a, 0xd5, 0x30, 0x36, 0xa5, 0x38, 0xbf, 0x40, 0xa3, 0x9e,
        0x81, 0xf3, 0xd7, 0xfb, 0x7c, 0xe3, 0x39, 0x82, 0x9b, 0x2f, 0xff, 0x87,
        0x34, 0x8e, 0x43, 0x44, 0xc4, 0xde, 0xe9, 0xcb, 0x54, 0x7b, 0x94, 0x32,
        0xa6, 0xc2, 0x23, 0x3d, 0xee, 0x4c, 0x95, 0x0b, 0x42, 0xfa, 0xc3, 0x4e,
        0x08, 0x2e, 0xa1, 0x66, 0x28, 0xd9, 0x24, 0xb2, 0x76, 0x5b, 0xa2, 0x49,
        0x6d, 0x8b, 0xd1, 0x25, 0x72, 0xf8, 0xf6, 0x64, 0x86, 0x68, 0x98, 0x16,
        0xd4, 0xa4, 0x5c, 0xcc, 0x5d, 0x65, 0xb6, 0x92, 0x6c, 0x70, 0x48, 0x50,
        0xfd, 0xed, 0xb9, 0xda, 0x5e, 0x15, 0x46, 0x57, 0xa7, 0x8d, 0x9d, 0x84,
        0x90, 0xd8, 0xab, 0x00, 0x8c, 0xbc, 0xd3, 0x0a, 0xf7, 0xe4, 0x58, 0x05,
        0xb8, 0xb3, 0x45, 0x06, 0xd0, 0x2c, 0x1e, 0x8f, 0xca, 0x3f, 0x0f, 0x02,
        0xc1, 0xaf, 0xbd, 0x03, 0x01, 0x13, 0x8a, 0x6b, 0x3a, 0x91, 0x11, 0x41,
        0x4f, 0x67, 0xdc, 0xea, 0x97, 0xf2, 0xcf, 0xce, 0xf0, 0xb4, 0xe6, 0x73,
        0x96, 0xac, 0x74, 0x22, 0xe7, 0xad, 0x35, 0x85, 0xe2, 0xf9, 0x37, 0xe8,
        0x1c, 0x75, 0xdf, 0x6e, 0x47, 0xf1, 0x1a, 0x71, 0x1d, 0x29, 0xc5, 0x89,
        0x6f, 0xb7, 0x62, 0x0e, 0xaa, 0x18, 0xbe, 0x1b, 0xfc, 0x56, 0x3e, 0x4b,
        0xc6, 0xd2, 0x79, 0x20, 0x9a, 0xdb, 0xc0, 0xfe, 0x78, 0xcd, 0x5a, 0xf4,
        0x1f, 0xdd, 0xa8, 0x33, 0x88, 0x07, 0xc7, 0x31, 0xb1, 0x12, 0x10, 0x59,
        0x27, 0x80, 0xec, 0x5f, 0x60, 0x51, 0x7f, 0xa9, 0x19, 0xb5, 0x4a, 0x0d,
        0x2d, 0xe5, 0x7a, 0x9f, 0x93, 0xc9, 0x9c, 0xef, 0xa0, 0xe0, 0x3b, 0x4d,
        0xae, 0x2a, 0xf5, 0xb0, 0xc8, 0xeb, 0xbb, 0x3c, 0x83, 0x53, 0x99, 0x61,
        0x17, 0x2b, 0x04, 0x7e, 0xba, 0x77, 0xd6, 0x26, 0xe1, 0x69, 0x14, 0x63,
        0x55, 0x21, 0x0c, 0x7d};

    inline uint8_t sbox_ith(uint8_t i) noexcept {
        return sbox[i];
    }

    inline uint8_t rsbox_ith(uint8_t i) noexcept {
        return rsbox[i];
    }
} // namespace

using crypto::detail::aes_key_matrix_type;
using crypto::detail::aes_key_word_size;
using crypto::detail::aes_key_word_type;
using crypto::detail::aes_state_column_type;
using crypto::detail::aes_state_type;

namespace {
    inline constexpr uint32_t rot_u32(uint32_t in) {
        return (in >> 8) | ((in & 0xff) << 24);
    }

    inline aes_key_word_type rot_word(aes_key_word_type in) {
        return aes_key_word_type{.u32 = rot_u32(in.u32)};
    }

    inline aes_key_word_type sub_word(aes_key_word_type in) {
        const auto bytes = in.bytes;
        return aes_key_word_type{
            .bytes = {sbox_ith(bytes[0]), sbox_ith(bytes[1]),
                      sbox_ith(bytes[2]), sbox_ith(bytes[3])}};
    }

    inline aes_key_word_type xor_word(aes_key_word_type in1,
                                      aes_key_word_type in2) {
        return aes_key_word_type{.u32 = in1.u32 ^ in2.u32};
    }

    template <class AesConsts>
    void do_aes_expand_key(
        std::array<aes_key_word_type, AesConsts::key_expand_size / 4>&
            round_key,
        const std::array<aes_key_word_type,
                         AesConsts::key_size / aes_key_word_size>& key) {
        constexpr auto N = AesConsts::key_words;
        constexpr auto R = AesConsts::key_rounds;

        constexpr uint8_t rcon_values[11] = {0x8d, 0x01, 0x02, 0x04, 0x08, 0x10,
                                             0x20, 0x40, 0x80, 0x1b, 0x36};

        std::copy(key.begin(), key.end(), round_key.begin());

        for (uint32_t i = N; i < 4 * R; ++i) {
            auto Wi_1 = round_key[i - 1];

            if (i % N == 0) {
                // const std::array<uint8_t, 4> xval{
                // rcon_values[i / N], 0, 0, 0 };
                Wi_1 = xor_word(sub_word(rot_word(Wi_1)),
                                aes_key_word_type{.u32 = rcon_values[i / N]});
            }
            else if (N > 6 && i % N == 4) {
                Wi_1 = sub_word(round_key[i - 1]);
            }
            round_key[i] = xor_word(round_key[i - N], Wi_1);
        }
    }

    inline void add_round_key(aes_state_type& state,
                              const aes_key_matrix_type& key_bytes) {
        for (size_t i = 0; i < key_bytes.size(); ++i) {
            state.matrix[i] ^= key_bytes[i];
        }
    }

    inline void sub_bytes(aes_state_type& state) noexcept {
        for (auto& byte : state.matrix) {
            byte = sbox_ith(byte);
        }
    }

    inline void inv_sub_bytes(aes_state_type& state) {
        for (auto& byte : state.matrix) {
            byte = rsbox_ith(byte);
        }
    }

    inline constexpr uint32_t make_u32_from_parts(uint32_t b0, uint32_t b1,
                                                  uint32_t b2,
                                                  uint32_t b3) noexcept {
        return (b0 & (0xff << (8 * 0))) | (b1 & (0xff << (8 * 1))) |
               (b2 & (0xff << (8 * 2))) | (b3 & (0xff << (8 * 3)));
    }

    inline constexpr aes_state_type shift_rows_u32s(aes_state_type in) {
        /*
        matrix is column major ordered
        shift to left

        0  4  8   12         0  4  8   12
        1  5  9   13  ==>    5  9  13   1
        2  6  10  14         10 14  2   6
        3  7  11  15         15 3   7  11
        */

        aes_state_type out{.u32s = {}};
        out.u32s[0] =
            make_u32_from_parts(in.u32s[0], in.u32s[1], in.u32s[2], in.u32s[3]);
        out.u32s[1] =
            make_u32_from_parts(in.u32s[1], in.u32s[2], in.u32s[3], in.u32s[0]);
        out.u32s[2] =
            make_u32_from_parts(in.u32s[2], in.u32s[3], in.u32s[0], in.u32s[1]);
        out.u32s[3] =
            make_u32_from_parts(in.u32s[3], in.u32s[0], in.u32s[1], in.u32s[2]);
        return out;
    }

    inline constexpr void shift_rows(aes_state_type& state) noexcept {
        state = shift_rows_u32s(state);
    }

    inline void inv_shift_rows(aes_state_type& state) noexcept {
        /*
        shift to right

        0  4  8   12         0   4   8   12
        1  5  9   13  ==>    13  1   5   9
        2  6  10  14         10  14  2   6
        3  7  11  15         7   11  15  3
        */

        // matrix is column major ordered
        auto& matrix = state.matrix;
        matrix = {matrix[0],  matrix[13], matrix[10], matrix[7],
                  matrix[4],  matrix[1],  matrix[14], matrix[11],
                  matrix[8],  matrix[5],  matrix[2],  matrix[15],
                  matrix[12], matrix[9],  matrix[6],  matrix[3]};
    }

    inline constexpr uint8_t xtime(uint8_t x) {
        return (uint8_t)(((x << 1) ^ (((x >> 7) & 1) * 0x1b)));
    }

    inline constexpr uint8_t gmul8(uint8_t x, uint8_t y) {
        return (((y & 1) * x) ^ ((y >> 1 & 1) * xtime(x)) ^
                ((y >> 2 & 1) * xtime(xtime(x))) ^
                ((y >> 3 & 1) * xtime(xtime(xtime(x)))) ^
                ((y >> 4 & 1) *
                 xtime(xtime(xtime(xtime(
                     x)))))); /* this last call to xtime() can be omitted */
    }

    inline constexpr uint8_t gmul_8_by_2(uint8_t a) {
        // h is 0xff if the high bit of r[c] is set, 0 otherwise
        uint8_t h =
            (uint8_t)((signed char)a >> 7); // arithmetic right shift, thus
                                            // shifting in either zeros or ones
        uint8_t b = a << 1; // implicitly removes high bit because b[c]
                            // is an 8-bit char, so we xor by 0x1b and
                            // not 0x11b in the next line
        b ^= 0x1b & h;      // Rijndael's Galois field
        return b;
    }

    inline constexpr aes_state_column_type
    mix_one_col(aes_state_column_type column) noexcept {
        const auto a = column;
        aes_state_column_type b;
        for (std::size_t i = 0; i < a.size(); ++i) {
            b[i] = gmul_8_by_2(a[i]);
        }
        column[0] =
            b[0] ^ a[3] ^ a[2] ^ b[1] ^ a[1]; // 2 * a0 + a3 + a2 + 3 * a1
        column[1] =
            b[1] ^ a[0] ^ a[3] ^ b[2] ^ a[2]; // 2 * a1 + a0 + a3 + 3 * a2
        column[2] =
            b[2] ^ a[1] ^ a[0] ^ b[3] ^ a[3]; // 2 * a2 + a1 + a0 + 3 * a3
        column[3] =
            b[3] ^ a[2] ^ a[1] ^ b[0] ^ a[0]; // 2 * a3 + a2 + a1 + 3 * a0
        return column;
    }

    static_assert(mix_one_col(aes_state_column_type{219, 19, 83, 69}) ==
                  aes_state_column_type{142, 77, 161, 188});
    static_assert(mix_one_col(aes_state_column_type{242, 10, 34, 92}) ==
                  aes_state_column_type{159, 220, 88, 157});
    static_assert(mix_one_col(aes_state_column_type{1, 1, 1, 1}) ==
                  aes_state_column_type{1, 1, 1, 1});
    static_assert(mix_one_col(aes_state_column_type{198, 198, 198, 198}) ==
                  aes_state_column_type{198, 198, 198, 198});
    static_assert(mix_one_col(aes_state_column_type{212, 212, 212, 213}) ==
                  aes_state_column_type{213, 213, 215, 214});
    static_assert(mix_one_col(aes_state_column_type{45, 38, 49, 76}) ==
                  aes_state_column_type{77, 126, 189, 248});

    inline void mix_columns(aes_state_type& state) noexcept {
        for (auto& column : state.columns) {
            column = mix_one_col(column);
        }
    }

    inline void inv_mix_one_column(aes_state_column_type& column) noexcept {
        auto [d0, d1, d2, d3] = column;
        auto& [b0, b1, b2, b3] = column;

        b0 = gmul8(d0, 14) ^ gmul8(d1, 11) ^ gmul8(d2, 13) ^ gmul8(d3, 9);
        b1 = gmul8(d0, 9) ^ gmul8(d1, 14) ^ gmul8(d2, 11) ^ gmul8(d3, 13);
        b2 = gmul8(d0, 13) ^ gmul8(d1, 9) ^ gmul8(d2, 14) ^ gmul8(d3, 11);
        b3 = gmul8(d0, 11) ^ gmul8(d1, 13) ^ gmul8(d2, 9) ^ gmul8(d3, 14);
    }

    inline void inv_mix_columns(aes_state_type& state) noexcept {
        for (auto& column : state.columns) {
            inv_mix_one_column(column);
        }
    }

    template <class AesConsts>
    void do_cipher(
        aes_state_type& state,
        const std::array<aes_key_matrix_type, AesConsts::key_expand_size / 16>&
            key_matrices) noexcept {
        add_round_key(state, key_matrices[0]);
        for (uint32_t i = 1; i < AesConsts::cipher_rounds; ++i) {
            sub_bytes(state);
            shift_rows(state);
            mix_columns(state);
            add_round_key(state, key_matrices[i]);
        }
        sub_bytes(state);
        shift_rows(state);
        add_round_key(state, key_matrices[AesConsts::cipher_rounds]);
    }

    template <class AesConsts>
    void inv_cipher(
        aes_state_type& state,
        const std::array<aes_key_matrix_type, AesConsts::key_expand_size / 16>&
            key_matrices) noexcept {
        add_round_key(state, key_matrices[AesConsts::cipher_rounds]);
        for (uint32_t i = AesConsts::cipher_rounds - 1; i > 0; --i) {
            inv_shift_rows(state);
            inv_sub_bytes(state);
            add_round_key(state, key_matrices[i]);
            inv_mix_columns(state);
        }
        inv_shift_rows(state);
        inv_sub_bytes(state);
        add_round_key(state, key_matrices[0]);
    }
}; // namespace

void crypto::detail::aes_expand_key(
    aes_expanded_key<aes_128_constants>::words_type& round_key,
    const aes_input_key<aes_128_constants>& key) noexcept {
    do_aes_expand_key<aes_128_constants>(round_key, key.key_words);
}

void crypto::detail::aes_expand_key(
    aes_expanded_key<aes_192_constants>::words_type& round_key,
    const aes_input_key<aes_192_constants>& key) noexcept {
    do_aes_expand_key<aes_192_constants>(round_key, key.key_words);
}

void crypto::detail::aes_expand_key(
    aes_expanded_key<aes_256_constants>::words_type& round_key,
    const aes_input_key<aes_256_constants>& key) noexcept {
    do_aes_expand_key<aes_256_constants>(round_key, key.key_words);
}

aes_state_type crypto::detail::aes_encrypt(
    const aes_expanded_key<aes_128_constants>::matrices_type& round_keys,
    aes_state_type state) noexcept {
    do_cipher<aes_128_constants>(state, round_keys);
    return state;
}

aes_state_type crypto::detail::aes_encrypt(
    const aes_expanded_key<aes_192_constants>::matrices_type& round_keys,
    aes_state_type state) noexcept {
    do_cipher<aes_192_constants>(state, round_keys);
    return state;
}

aes_state_type crypto::detail::aes_encrypt(
    const aes_expanded_key<aes_256_constants>::matrices_type& round_keys,
    aes_state_type state) noexcept {
    do_cipher<aes_256_constants>(state, round_keys);
    return state;
}

aes_state_type crypto::detail::aes_decrypt(
    const aes_expanded_key<aes_128_constants>::matrices_type& round_keys,
    aes_state_type state) noexcept {
    inv_cipher<aes_128_constants>(state, round_keys);
    return state;
}

aes_state_type crypto::detail::aes_decrypt(
    const aes_expanded_key<aes_192_constants>::matrices_type& round_keys,
    aes_state_type state) noexcept {
    inv_cipher<aes_192_constants>(state, round_keys);
    return state;
}

aes_state_type crypto::detail::aes_decrypt(
    const aes_expanded_key<aes_256_constants>::matrices_type& round_keys,
    aes_state_type state) noexcept {
    inv_cipher<aes_256_constants>(state, round_keys);
    return state;
}