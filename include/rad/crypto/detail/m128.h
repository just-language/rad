#pragma once
#include <rad/big_endian.h>
#include <rad/libbase.h>
#include <wmmintrin.h>

namespace RAD_LIB_NAMESPACE::crypto {
    namespace m128i_utils {
        inline __m128i zero128() noexcept {
            return _mm_setzero_si128();
        }

        // load m128i from unaligned memory
        inline __m128i align128(const __m128i* i) noexcept {
            return _mm_loadu_si128(i);
        }

        // load m128i from unaligned memory
        inline __m128i load(const void* src) noexcept {
            return _mm_loadu_si128(static_cast<const __m128i*>(src));
        }

        // store m128i to unaligned memory
        inline void store(__m128i i, void* dst) noexcept {
            _mm_storeu_si128(static_cast<__m128i*>(dst), i);
        }

        inline void copy_from(__m128i* dst, const uint8_t* src,
                              std::size_t n) noexcept {
            std::memcpy(dst, src, n);
        }

        inline void copy_to(void* dst, const __m128i* src,
                            std::size_t n) noexcept {
            std::memcpy(dst, src, n);
        }

        inline __m128i xor128(__m128i lhs, __m128i rhs) noexcept {
            return _mm_xor_si128(lhs, rhs);
        }

        inline bool check_bit(__m128i i128, uint8_t bitn) noexcept {
            auto& u8s = *reinterpret_cast<std::array<uint8_t, 16>*>(&i128);
            // bytes are little endian, bits are big endain
            return bits::check(u8s[bitn / 8], 7 - bitn % 8);
        }

        inline __m128i shift_one_bit_right(__m128i i) noexcept {
            auto& beu64s = *reinterpret_cast<std::array<beu64, 2>*>(&i);

            uint64_t u64_0 = beu64s[0];
            uint64_t u64_1 = beu64s[1];

            u64_1 >>= 1;
            bits::assign<63>(u64_1, bits::check<0>(u64_0));
            u64_0 >>= 1;

            beu64s[0] = u64_0;
            beu64s[1] = u64_1;

            return i;
        }
    } // namespace m128i_utils
} // namespace RAD_LIB_NAMESPACE::crypto