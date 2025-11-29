#pragma once
#include <rad/big_endian.h>
#include <rad/libbase.h>
#ifdef RAD_CPU_ARM_ARCH
#include <arm_neon.h>
#else
#include <wmmintrin.h>
#ifdef __unix__
#include <tmmintrin.h>
#endif // __unix__
#endif // RAD_CPU_ARM_ARCH

#if defined(__GNUC__) || defined(__clang__)
#ifdef RAD_CPU_ARM_ARCH
#define RAD_FN_ATTRIBUTE_M128_SHUFFLE __attribute__((target("simd")))
#define RAD_FN_ATTRIBUTE_CLMUL __attribute__((target("crypto")))
#else
#define RAD_FN_ATTRIBUTE_M128_SHUFFLE __attribute__((target("sse4.1")))
#define RAD_FN_ATTRIBUTE_CLMUL __attribute__((target("pclmul")))
#endif // RAD_CPU_ARM_ARCH
#else
#define RAD_FN_ATTRIBUTE_M128_SHUFFLE
#define RAD_FN_ATTRIBUTE_CLMUL
#endif // defined(__GNUC__) || defined(__clang__)

namespace RAD_LIB_NAMESPACE::crypto {
#ifdef RAD_CPU_ARM_ARCH
    using m128i_type = uint8x16_t;
#else
    using m128i_type = __m128i;
#endif // RAD_CPU_ARM_ARCH

    namespace m128i_utils {
        inline m128i_type zero128() noexcept {
#ifdef RAD_CPU_ARM_ARCH
            return vdupq_n_u8(0);
#else
            return _mm_setzero_si128();
#endif // RAD_CPU_ARM_ARCH
        }

        // load m128i from unaligned memory
        inline m128i_type align128(const m128i_type* i) noexcept {
#ifdef RAD_CPU_ARM_ARCH
            return vld1q_u8(reinterpret_cast<const uint8_t*>(i));
#else
            return _mm_loadu_si128(i);
#endif // RAD_CPU_ARM_ARCH
        }

        // load m128i from unaligned memory
        inline m128i_type load(const void* src) noexcept {
            return align128(static_cast<const m128i_type*>(src));
        }

        // store m128i to unaligned memory
        inline void store(m128i_type i, void* dst) noexcept {
#ifdef RAD_CPU_ARM_ARCH
            vst1q_u8(static_cast<uint8_t*>(dst), i);
#else
            _mm_storeu_si128(static_cast<m128i_type*>(dst), i);
#endif // RAD_CPU_ARM_ARCH
        }

        inline void copy_from(m128i_type* dst, const uint8_t* src,
                              std::size_t n) noexcept {
            std::memcpy(dst, src, n);
        }

        inline void copy_to(void* dst, const m128i_type* src,
                            std::size_t n) noexcept {
            std::memcpy(dst, src, n);
        }

        inline m128i_type or128(m128i_type lhs, m128i_type rhs) noexcept {
#ifdef RAD_CPU_ARM_ARCH
            return vorrq_u8(lhs, rhs);
#else
            return _mm_or_si128(lhs, rhs);
#endif // RAD_CPU_ARM_ARCH
        }

        inline m128i_type xor128(m128i_type lhs, m128i_type rhs) noexcept {
#ifdef RAD_CPU_ARM_ARCH
            return veorq_u8(lhs, rhs);
#else
            return _mm_xor_si128(lhs, rhs);
#endif // RAD_CPU_ARM_ARCH
        }

        template <int N>
        inline m128i_type shift_right_by_bytes(m128i_type a) {
#ifdef RAD_CPU_ARM_ARCH
            return vextq_u8(zero128(), a, 16 - N);
#else
            return _mm_srli_si128(a, N);
#endif // RAD_CPU_ARM_ARCH
        }

        template <int N>
        inline m128i_type shift_left_by_bytes(m128i_type a) {
#ifdef RAD_CPU_ARM_ARCH
            return vextq_u8(a, zero128(), N);
#else
            return _mm_slli_si128(a, N);
#endif // RAD_CPU_ARM_ARCH
        }

        inline m128i_type shift_right_32bits(m128i_type a, int n) {
#ifdef RAD_CPU_ARM_ARCH
            return vreinterpretq_u8_u32(
                vshlq_u32(vreinterpretq_u32_u8(a), vdupq_n_s32(-n)));
#else
            return _mm_srli_epi32(a, n);
#endif // RAD_CPU_ARM_ARCH
        }

        inline m128i_type shift_left_32bits(m128i_type a, int n) {
#ifdef RAD_CPU_ARM_ARCH
            return vreinterpretq_u8_u32(
                vshlq_u32(vreinterpretq_u32_u8(a), vdupq_n_s32(n)));
#else
            return _mm_slli_epi32(a, n);
#endif // RAD_CPU_ARM_ARCH
        }

        inline RAD_FN_ATTRIBUTE_M128_SHUFFLE m128i_type
        shuffle_packed_8bits(m128i_type i, m128i_type mask) {
#ifdef RAD_CPU_ARM64_ARCH
            return vqtbl1q_u8(i, mask);
#elif defined(RAD_CPU_ARM_ARCH)
            uint8x8x2_t i2 = {vget_low_u8(i), vget_high_u8(i)};
            uint8x8_t lo = vtbl2_u8(i2, vget_low_u8(mask));
            uint8x8_t hi = vtbl2_u8(i2, vget_high_u8(mask));
            return vcombine_u8(lo, hi);
#else
            return _mm_shuffle_epi8(i, mask);
#endif // RAD_CPU_ARM_ARCH
        }

        template <int imm8>
        inline RAD_FN_ATTRIBUTE_CLMUL m128i_type clmul(m128i_type a,
                                                       m128i_type b) noexcept {
#ifdef RAD_CPU_ARM64_ARCH
            poly64x1_t a_poly, b_poly;
            // Extract the appropriate 64-bit halves based on imm8
            switch (imm8 & 0x11) {
            case 0x00: // Multiply low 64 bits of both
                a_poly = vget_low_p64(vreinterpretq_p64_u8(a));
                b_poly = vget_low_p64(vreinterpretq_p64_u8(b));
                break;
            case 0x01: // Multiply high 64 bits of a, low 64 bits of b
                a_poly = vget_high_p64(vreinterpretq_p64_u8(a));
                b_poly = vget_low_p64(vreinterpretq_p64_u8(b));
                break;
            case 0x10: // Multiply low 64 bits of a, high 64 bits of b
                a_poly = vget_low_p64(vreinterpretq_p64_u8(a));
                b_poly = vget_high_p64(vreinterpretq_p64_u8(b));
                break;
            case 0x11: // Multiply high 64 bits of both
                a_poly = vget_high_p64(vreinterpretq_p64_u8(a));
                b_poly = vget_high_p64(vreinterpretq_p64_u8(b));
                break;
            }
            poly128_t result_poly =
                vmull_p64(vget_lane_p64(a_poly, 0), vget_lane_p64(b_poly, 0));
            return vreinterpretq_u8_p128(result_poly);
#elif defined(RAD_CPU_ARM_ARCH)
            return zero128();
#else
            return _mm_clmulepi64_si128(a, b, imm8);
#endif // RAD_CPU_ARM_ARCH
        }

        inline bool check_bit(m128i_type i128, uint8_t bitn) noexcept {
            auto& u8s = *reinterpret_cast<std::array<uint8_t, 16>*>(&i128);
            // bytes are little endian, bits are big endain
            return bits::check(u8s[bitn / 8], 7 - bitn % 8);
        }

        inline m128i_type shift_one_bit_right(m128i_type i) noexcept {
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