#include <rad/crypto/modes/gcm.h>
#ifdef RAD_FAST_CRYPTO
#ifdef RAD_CPU_ARM64_ARCH
#ifdef __linux__
#include <sys/auxv.h>
#include <asm/hwcap.h>
#elif defined(_WIN32)
#include <Windows.h>
#endif // __linux__
#elif defined RAD_CPU_X86_ARCH
#include <rad/cpu.h>
#endif // RAD_CPU_ARM64_ARCH
#endif // RAD_FAST_CRYPTO

#if defined(RAD_CPU_ARM_ARCH) && !defined(RAD_CPU_ARM64_ARCH)
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

namespace {
    inline bool has_pmull_if_available = true;

    inline bool has_pmull() noexcept {
#ifdef RAD_FAST_CRYPTO
        if (!has_pmull_if_available) {
            return false;
        }
#ifdef RAD_CPU_X86_ARCH
        return cpu::pclmulqdq();
#elif defined RAD_CPU_ARM64_ARCH

#ifdef __linux__
#if defined(AT_HWCAP) && defined(HWCAP_PMULL)
        unsigned long hwcap = getauxval(AT_HWCAP);
        return (hwcap & HWCAP_PMULL) != 0;
#else
        return false;
#endif // defined(AT_HWCAP) && defined(HWCAP_PMULL)
#elif defined(_WIN32)
#ifdef PF_ARM_V8_CRYPTO_INSTRUCTIONS_AVAILABLE
        return IsProcessorFeaturePresent(
            PF_ARM_V8_CRYPTO_INSTRUCTIONS_AVAILABLE);
#else
        return false;
#endif // PF_ARM_V8_CRYPTO_INSTRUCTIONS_AVAILABLE
#else
        return false;
#endif // __linux__
#else
        return false;
#endif // RAD_CPU_X86_ARCH
#else
        return false;
#endif // RAD_FAST_CRYPTO
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

    inline m128i_type g256_mul(m128i_type X, m128i_type Y) noexcept {
        m128i_type Z = zero128(), V = X;
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
    inline m128i_type do_g256_mul(m128i_type X, m128i_type Y) noexcept {
        return g256_mul(X, Y);
    }

    template <bool use_cpu>
    inline m128i_type shuffle_m128i(m128i_type i) noexcept {
        return i;
    }

    template <bool use_cpu>
    inline void shuffle_m128(m128i_type&) noexcept {
    }

#ifdef RAD_FAST_CRYPTO
    inline m128i_type fast_g256_mul(m128i_type a, m128i_type b) noexcept {
        m128i_type tmp2, tmp3, tmp4, tmp5, tmp6, tmp7, tmp8, tmp9;
        tmp3 = clmul<0x00>(a, b);
        tmp4 = clmul<0x10>(a, b);
        tmp5 = clmul<0x01>(a, b);
        tmp6 = clmul<0x11>(a, b);
        tmp4 = xor128(tmp4, tmp5);
        tmp5 = shift_left_by_bytes<8>(tmp4);
        tmp4 = shift_right_by_bytes<8>(tmp4);
        tmp3 = xor128(tmp3, tmp5);
        tmp6 = xor128(tmp6, tmp4);
        tmp7 = shift_right_32bits(tmp3, 31);
        tmp8 = shift_right_32bits(tmp6, 31);
        tmp3 = shift_left_32bits(tmp3, 1);
        tmp6 = shift_left_32bits(tmp6, 1);
        tmp9 = shift_right_by_bytes<12>(tmp7);
        tmp8 = shift_left_by_bytes<4>(tmp8);
        tmp7 = shift_left_by_bytes<4>(tmp7);
        tmp3 = or128(tmp3, tmp7);
        tmp6 = or128(tmp6, tmp8);
        tmp6 = or128(tmp6, tmp9);
        tmp7 = shift_left_32bits(tmp3, 31);
        tmp8 = shift_left_32bits(tmp3, 30);
        tmp9 = shift_left_32bits(tmp3, 25);
        tmp7 = xor128(tmp7, tmp8);
        tmp7 = xor128(tmp7, tmp9);
        tmp8 = shift_right_by_bytes<4>(tmp7);
        tmp7 = shift_left_by_bytes<12>(tmp7);
        tmp3 = xor128(tmp3, tmp7);
        tmp2 = shift_right_32bits(tmp3, 1);
        tmp4 = shift_right_32bits(tmp3, 2);
        tmp5 = shift_right_32bits(tmp3, 7);
        tmp2 = xor128(tmp2, tmp4);
        tmp2 = xor128(tmp2, tmp5);
        tmp2 = xor128(tmp2, tmp8);
        tmp3 = xor128(tmp3, tmp2);
        tmp6 = xor128(tmp6, tmp3);
        return tmp6;
    }

    template <>
    inline m128i_type do_g256_mul<true>(m128i_type X, m128i_type Y) noexcept {
        return fast_g256_mul(X, Y);
    }

    template <>
    inline m128i_type shuffle_m128i<true>(m128i_type i) noexcept {
#ifdef RAD_CPU_ARM_ARCH
        const m128i_type BSWAP_MASK = {15, 14, 13, 12, 11, 10, 9, 8,
                                       7,  6,  5,  4,  3,  2,  1, 0};
#else
        const m128i_type BSWAP_MASK =
            _mm_set_epi8(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15);
#endif // RAD_CPU_ARM_ARCH
        return shuffle_packed_8bits(i, BSWAP_MASK);
    }

    template <>
    inline void shuffle_m128<true>(m128i_type& i) noexcept {
#ifdef RAD_CPU_ARM_ARCH
        const m128i_type BSWAP_MASK = {15, 14, 13, 12, 11, 10, 9, 8,
                                       7,  6,  5,  4,  3,  2,  1, 0};
#else
        const m128i_type BSWAP_MASK =
            _mm_set_epi8(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15);
#endif // RAD_CPU_ARM_ARCH
        i = shuffle_packed_8bits(i, BSWAP_MASK);
    }
#endif // RAD_FAST_CRYPTO

    template <bool use_cpu>
    inline m128i_type
    ghash_blocks(m128i_type X, m128i_type H, std::span<const m128i_type> blocks,
                 m128i_type last_block, bool has_last_block) noexcept {
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
                m128i_type aligned_block = align128(&block);
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
    inline m128i_type do_ghash_step(m128i_type H, m128i_type X,
                                    std::span<const m128i_type> blocks,
                                    m128i_type last_block,
                                    bool has_last_block) noexcept {
        shuffle_m128<use_cpu>(H);
        shuffle_m128<use_cpu>(X);
        X = ghash_blocks<use_cpu>(X, H, blocks, last_block, has_last_block);
        shuffle_m128<use_cpu>(X);
        return X;
    }

    template <bool use_cpu>
    inline m128i_type do_ghash(m128i_type H, std::span<const m128i_type> A,
                               m128i_type As, std::span<const m128i_type> C,
                               m128i_type Cs, m128i_type lens_bits,
                               bool a_last_block, bool c_last_block) noexcept {
        shuffle_m128<use_cpu>(H);
        // the first iteration is Xi_1 = 0
        m128i_type Xi_1 = zero128();
        Xi_1 = ghash_blocks<use_cpu>(Xi_1, H, A, As, a_last_block);
        Xi_1 = ghash_blocks<use_cpu>(Xi_1, H, C, Cs, c_last_block);
        Xi_1 = do_g256_mul<use_cpu>(
            xor128(Xi_1, shuffle_m128i<use_cpu>(lens_bits)), H);
        shuffle_m128<use_cpu>(Xi_1);
        return Xi_1;
    }

    union alignas(16) m128i_bytes {
        m128i_type i = zero128();
        std::array<uint8_t, 16> bytes;
    };
} // namespace

void crypto::disable_pclmulqdq() noexcept {
    has_pmull_if_available = false;
}

void crypto::enable_pclmulqdq() noexcept {
    has_pmull_if_available = true;
}

const std::error_category& crypto::gcm_category() noexcept {
    return gcm_category_inst;
}

m128i_type ghash::step(m128i_type H, m128i_type X, const_buffer data) noexcept {
    const size_t blocks_n = data.size() / block_length;
    const size_t remainig = data.size() % block_length;

    auto blocks = data.to_span<m128i_type>().subspan(0, blocks_n);
    std::span<const uint8_t> last_block_data;
    if (remainig > 0) {
        last_block_data =
            data.sub_buffer(data.size() - remainig).to_span<uint8_t>();
    }
    m128i_bytes last_block;
    std::copy(last_block_data.begin(), last_block_data.end(),
              last_block.bytes.begin());
#ifdef RAD_FAST_CRYPTO
    if (has_pmull()) {
        return do_ghash_step<true>(H, X, blocks, last_block.i,
                                   !last_block_data.empty());
    }
    else
#endif // RAD_FAST_CRYPTO
        return do_ghash_step<false>(H, X, blocks, last_block.i,
                                    !last_block_data.empty());
}

m128i_type ghash::step(m128i_type H, m128i_type X, m128i_type block) noexcept {
#ifdef RAD_FAST_CRYPTO
    if (has_pmull()) {
        return shuffle_m128i<true>(do_g256_mul<true>(
            xor128(shuffle_m128i<true>(X), shuffle_m128i<true>(block)),
            shuffle_m128i<true>(H)));
    }
    else
#endif // RAD_FAST_CRYPTO
        return do_g256_mul<false>(xor128(X, block), H);
}

m128i_type ghash::merge_lengths(uint64_t a_len, uint64_t c_len) noexcept {
#ifdef RAD_CPU_ARM_ARCH
    return vreinterpretq_u8_u64(
        vcombine_u64(vcreate_u64(details::bswap(a_len)),
                     vcreate_u64(details::bswap(c_len))));
#else
    return _mm_set_epi64x(details::bswap(c_len), details::bswap(a_len));
#endif // RAD_CPU_ARM_ARCH
}

m128i_type ghash::calc(m128i_type H, const_buffer Abuff,
                       const_buffer Cbuff) noexcept {
    auto A = Abuff.to_span<const m128i_type>().subspan(0, Abuff.size() /
                                                              block_length);
    auto C = Cbuff.to_span<const m128i_type>().subspan(0, Cbuff.size() /
                                                              block_length);

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

    m128i_type lens_bits = merge_lengths(Abuff.size() * 8, Cbuff.size() * 8);
#ifdef RAD_FAST_CRYPTO
    if (has_pmull()) {
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