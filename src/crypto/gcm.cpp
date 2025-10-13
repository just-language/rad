#include <rad/cpu.h>
#include <rad/crypto/modes/gcm.h>
#ifdef __unix__
#include <tmmintrin.h>
#endif // __unix__

#if defined(__ARM_ARCH) || defined(_M_ARM) || defined(__aarch64__) ||          \
    defined(__arm__)
#undef RAD_FAST_CRYPTO
#endif

using namespace RAD_LIB_NAMESPACE;
namespace details = RAD_LIB_NAMESPACE::detail;
using namespace crypto;
using namespace m128i_utils;

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wignored-attributes"
#endif // defined(__GNUC__) && !defined(__clang__)

#if defined(__GNUC__) || defined(__clang__)
#define FN_ATTRIBUTE_SSE41 __attribute__((target("sse4.1")))
#define FN_ATTRIBUTE_PCLMUL __attribute__((target("pclmul")))
#else
#define FN_ATTRIBUTE_SSE41
#define FN_ATTRIBUTE_PCLMUL
#endif // defined(__GNUC__) || defined(__clang__)

namespace {
    inline bool use_pclmulqdq_if_available = true;

    inline bool use_pclmulqdq() noexcept {
        return use_pclmulqdq_if_available && cpu::pclmulqdq();
    }

    struct gcm_category_type : public std::error_category {
        virtual const char* name() const noexcept override {
            return "gcm";
        }

        virtual std::string message(int) const override {
            return "The computed authentication tag did "
                   "not match "
                   "the "
                   "input "
                   "authentication tag";
        }
    };

    const gcm_category_type gcm_category_inst;

    inline __m128i g256_mul(__m128i X, __m128i Y) noexcept {
        __m128i Z = zero128(), V = X;
        for (auto i : range(uint8_t{128})) {
            if (check_bit(Y, i)) {
                Z = xor128(Z, V);
            }
            // check 127th bit before shift!
            bool is_127bit_set = check_bit(V, 127);
            V = shift_one_bit_right(V);
            if (is_127bit_set) {
                *reinterpret_cast<uint8_t*>(&V) ^= 0xe1;
            }
        }
        return Z;
    }

    template <bool use_cpu>
    inline __m128i do_g256_mul(__m128i X, __m128i Y) noexcept {
        return g256_mul(X, Y);
    }

    template <bool use_cpu>
    inline __m128i shuffle_m128i(__m128i i) noexcept {
        return i;
    }

    template <bool use_cpu>
    inline void shuffle_m128(__m128i&) noexcept {
    }

#ifdef RAD_FAST_CRYPTO
    inline FN_ATTRIBUTE_PCLMUL __m128i pclmulqdq_g256_mul(__m128i a,
                                                          __m128i b) noexcept {
        __m128i tmp2, tmp3, tmp4, tmp5, tmp6, tmp7, tmp8, tmp9;
        tmp3 = _mm_clmulepi64_si128(a, b, 0x00);
        tmp4 = _mm_clmulepi64_si128(a, b, 0x10);
        tmp5 = _mm_clmulepi64_si128(a, b, 0x01);
        tmp6 = _mm_clmulepi64_si128(a, b, 0x11);
        tmp4 = _mm_xor_si128(tmp4, tmp5);
        tmp5 = _mm_slli_si128(tmp4, 8);
        tmp4 = _mm_srli_si128(tmp4, 8);
        tmp3 = _mm_xor_si128(tmp3, tmp5);
        tmp6 = _mm_xor_si128(tmp6, tmp4);
        tmp7 = _mm_srli_epi32(tmp3, 31);
        tmp8 = _mm_srli_epi32(tmp6, 31);
        tmp3 = _mm_slli_epi32(tmp3, 1);
        tmp6 = _mm_slli_epi32(tmp6, 1);
        tmp9 = _mm_srli_si128(tmp7, 12);
        tmp8 = _mm_slli_si128(tmp8, 4);
        tmp7 = _mm_slli_si128(tmp7, 4);
        tmp3 = _mm_or_si128(tmp3, tmp7);
        tmp6 = _mm_or_si128(tmp6, tmp8);
        tmp6 = _mm_or_si128(tmp6, tmp9);
        tmp7 = _mm_slli_epi32(tmp3, 31);
        tmp8 = _mm_slli_epi32(tmp3, 30);
        tmp9 = _mm_slli_epi32(tmp3, 25);
        tmp7 = _mm_xor_si128(tmp7, tmp8);
        tmp7 = _mm_xor_si128(tmp7, tmp9);
        tmp8 = _mm_srli_si128(tmp7, 4);
        tmp7 = _mm_slli_si128(tmp7, 12);
        tmp3 = _mm_xor_si128(tmp3, tmp7);
        tmp2 = _mm_srli_epi32(tmp3, 1);
        tmp4 = _mm_srli_epi32(tmp3, 2);
        tmp5 = _mm_srli_epi32(tmp3, 7);
        tmp2 = _mm_xor_si128(tmp2, tmp4);
        tmp2 = _mm_xor_si128(tmp2, tmp5);
        tmp2 = _mm_xor_si128(tmp2, tmp8);
        tmp3 = _mm_xor_si128(tmp3, tmp2);
        tmp6 = _mm_xor_si128(tmp6, tmp3);
        return tmp6;
    }

    template <>
    inline __m128i do_g256_mul<true>(__m128i X, __m128i Y) noexcept {
        return pclmulqdq_g256_mul(X, Y);
    }

    template <>
    inline FN_ATTRIBUTE_SSE41 __m128i shuffle_m128i<true>(__m128i i) noexcept {
        const __m128i BSWAP_MASK =
            _mm_set_epi8(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15);
        return _mm_shuffle_epi8(i, BSWAP_MASK);
    }

    template <>
    inline FN_ATTRIBUTE_SSE41 void shuffle_m128<true>(__m128i& i) noexcept {
        const __m128i BSWAP_MASK =
            _mm_set_epi8(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15);
        i = _mm_shuffle_epi8(i, BSWAP_MASK);
    }
#endif // RAD_FAST_CRYPTO

    template <bool use_cpu>
    inline __m128i
    ghash_blocks(__m128i X, __m128i H, std::span<const __m128i> blocks,
                 __m128i last_block, bool has_last_block) noexcept {
        const bool is_aligned =
            reinterpret_cast<uintptr_t>(blocks.data()) % 16 == 0;

        if (is_aligned) {
            for (const auto& block : blocks) {
                X = do_g256_mul<use_cpu>(
                    xor128(X, shuffle_m128i<use_cpu>(block)), H);
            }
        }
        else {
            for (const auto& block : blocks) {
                __m128i aligned_block = align128(&block);
                shuffle_m128<use_cpu>(aligned_block);
                X = do_g256_mul<use_cpu>(xor128(X, aligned_block), H);
            }
        }

        if (has_last_block) {
            shuffle_m128<use_cpu>(last_block);
            X = do_g256_mul<use_cpu>(xor128(X, last_block), H);
        }

        return X;
    }

    template <bool use_cpu>
    inline __m128i
    do_ghash_step(__m128i H, __m128i X, std::span<const __m128i> blocks,
                  __m128i last_block, bool has_last_block) noexcept {
        shuffle_m128<use_cpu>(H);
        shuffle_m128<use_cpu>(X);
        X = ghash_blocks<use_cpu>(X, H, blocks, last_block, has_last_block);
        shuffle_m128<use_cpu>(X);
        return X;
    }

    template <bool use_cpu>
    inline __m128i do_ghash(__m128i H, std::span<const __m128i> A, __m128i As,
                            std::span<const __m128i> C, __m128i Cs,
                            __m128i lens_bits, bool a_last_block,
                            bool c_last_block) noexcept {
        shuffle_m128<use_cpu>(H);
        // the first iteration is Xi_1 = 0
        __m128i Xi_1 = zero128();
        Xi_1 = ghash_blocks<use_cpu>(Xi_1, H, A, As, a_last_block);
        Xi_1 = ghash_blocks<use_cpu>(Xi_1, H, C, Cs, c_last_block);
        Xi_1 = do_g256_mul<use_cpu>(
            xor128(Xi_1, shuffle_m128i<use_cpu>(lens_bits)), H);
        shuffle_m128<use_cpu>(Xi_1);
        return Xi_1;
    }

    union alignas(16) m128i_bytes {
        __m128i i = zero128();
        std::array<uint8_t, 16> bytes;
    };
} // namespace

void crypto::disable_pclmulqdq() noexcept {
    use_pclmulqdq_if_available = false;
}

void crypto::enable_pclmulqdq() noexcept {
    use_pclmulqdq_if_available = true;
}

const std::error_category& crypto::gcm_category() noexcept {
    return gcm_category_inst;
}

__m128i ghash::step(__m128i H, __m128i X, const_buffer data) noexcept {
    const size_t blocks_n = data.size() / block_length;
    const size_t remainig = data.size() % block_length;

    auto blocks = data.to_span<__m128i>().subspan(0, blocks_n);
    std::span<const uint8_t> last_block_data;
    if (remainig > 0) {
        last_block_data =
            data.sub_buffer(data.size() - remainig).to_span<uint8_t>();
    }
    m128i_bytes last_block;
    std::copy(last_block_data.begin(), last_block_data.end(),
              last_block.bytes.begin());
#ifdef RAD_FAST_CRYPTO
    if (use_pclmulqdq()) {
        return do_ghash_step<true>(H, X, blocks, last_block.i,
                                   !last_block_data.empty());
    }
    else
#endif // RAD_FAST_CRYPTO
        return do_ghash_step<false>(H, X, blocks, last_block.i,
                                    !last_block_data.empty());
}

__m128i ghash::step(__m128i H, __m128i X, __m128i block) noexcept {
#ifdef RAD_FAST_CRYPTO
    if (use_pclmulqdq()) {
        return shuffle_m128i<true>(do_g256_mul<true>(
            xor128(shuffle_m128i<true>(X), shuffle_m128i<true>(block)),
            shuffle_m128i<true>(H)));
    }
    else
#endif // RAD_FAST_CRYPTO
        return do_g256_mul<false>(xor128(X, block), H);
}

__m128i ghash::merge_lengths(uint64_t a_len, uint64_t c_len) noexcept {
    return _mm_set_epi64x(details::bswap(c_len), details::bswap(a_len));
}

__m128i ghash::calc(__m128i H, const_buffer Abuff,
                    const_buffer Cbuff) noexcept {
    auto A =
        Abuff.to_span<const __m128i>().subspan(0, Abuff.size() / block_length);
    auto C =
        Cbuff.to_span<const __m128i>().subspan(0, Cbuff.size() / block_length);

    // A* and C* (star) are the last non complete blocks in A and C padded
    // with 0 bits to complete 128 bits
    std::span<const uint8_t> AsData, CsData;
    if (Abuff.size() % block_length > 0) {
        AsData = Abuff.sub_buffer(Abuff.size() - (Abuff.size() % block_length))
                     .to_span<const uint8_t>();
    }
    if (Cbuff.size() % block_length > 0) {
        CsData = Cbuff.sub_buffer(Cbuff.size() - (Cbuff.size() % block_length))
                     .to_span<const uint8_t>();
    }

    m128i_bytes AsBytes, CsBytes;
    std::copy(AsData.begin(), AsData.end(), AsBytes.bytes.begin());
    std::copy(CsData.begin(), CsData.end(), CsBytes.bytes.begin());

    __m128i lens_bits = merge_lengths(Abuff.size() * 8, Cbuff.size() * 8);
#ifdef RAD_FAST_CRYPTO
    if (use_pclmulqdq()) {
        return do_ghash<true>(H, A, AsBytes.i, C, CsBytes.i, lens_bits,
                              !AsData.empty(), !CsData.empty());
    }
#endif // RAD_FAST_CRYPTO
    return do_ghash<false>(H, A, AsBytes.i, C, CsBytes.i, lens_bits,
                           !AsData.empty(), !CsData.empty());
}

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic pop
#endif // defined(__GNUC__) && !defined(__clang__)