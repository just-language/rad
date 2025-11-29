#pragma once
#include <rad/big_endian.h>
#include <rad/buffer.h>
#include <rad/dynamic_buffer.h>
#include <rad/libbase.h>
#include <rad/trackable.h>
#include <rad/crypto/detail/m128.h>
#ifdef RAD_FAST_CRYPTO
#include <rad/cpu.h>
#include <wmmintrin.h>
#endif // RAD_FAST_CRYPTO

#include <cstring>
#include <random>

#if defined(__GNUC__) || defined(__clang__)
#define RAD_CRYPTO_FN_ATTRIBUTE_AES __attribute__((target("aes")))
#else
#define RAD_CRYPTO_FN_ATTRIBUTE_AES
#endif // defined(__GNUC__) || defined(__clang__)

#ifdef __GNUC__
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wignored-attributes"
#else
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wignored-attributes"
#endif // __clang__
#endif // __GNUC__

#if defined(__ARM_ARCH) || defined(_M_ARM) || defined(__aarch64__) ||          \
    defined(__arm__)
#undef RAD_FAST_CRYPTO
#endif

namespace RAD_LIB_NAMESPACE::crypto::detail {
    // only used for testing so not thread safe
    inline bool use_cpu_aesni_if_available = true;

    inline constexpr uint32_t aes_key_word_size = 4;

    struct aes_128_constants {
        static constexpr uint32_t key_words = 4;
        static constexpr uint32_t key_rounds = 11;
        static constexpr uint32_t cipher_rounds = 10;
        static constexpr uint32_t key_size = 16;
        static constexpr uint32_t key_expand_size = 16 * key_rounds;
    };

    struct aes_192_constants {
        static constexpr uint32_t key_words = 6;
        static constexpr uint32_t key_rounds = 13;
        static constexpr uint32_t cipher_rounds = 12;
        static constexpr uint32_t key_size = 24;
        static constexpr uint32_t key_expand_size = 16 * key_rounds;
    };

    struct aes_256_constants {
        static constexpr uint32_t key_words = 8;
        static constexpr uint32_t key_rounds = 15;
        static constexpr uint32_t cipher_rounds = 14;
        static constexpr uint32_t key_size = 32;
        static constexpr uint32_t key_expand_size = 16 * key_rounds;
    };

    union aes_key_word_type {
        uint32_t u32;
        std::array<uint8_t, 4> bytes;
    };

    using aes_key_matrix_type = std::array<uint8_t, 16>;
    using aes_state_column_type = std::array<uint8_t, 4>;
    using aes_state_matrix_type = aes_key_matrix_type;

    template <class AesConsts>
    struct alignas(16) aes_input_key {
        union {
            std::array<uint8_t, AesConsts::key_size> key_bytes;
            std::array<aes_key_word_type,
                       AesConsts::key_size / aes_key_word_size>
                key_words;
        };
    };

#ifdef RAD_FAST_CRYPTO

    template <class AesConsts, bool EncryptionOnly>
    struct aesni_keys_type;

    template <class AesConsts>
    struct alignas(16) aesni_keys_type<AesConsts, false> {
        union {
            m128i_type first_enc_key;
            m128i_type last_dec_key;
        };
        alignas(16) std::array<m128i_type,
                               (AesConsts::key_expand_size / 16) - 2> enc_keys;
        union {
            m128i_type last_enc_key;
            m128i_type first_dec_key;
        };
        alignas(16) std::array<m128i_type,
                               (AesConsts::key_expand_size / 16) - 2> dec_keys;
    };

    template <class AesConsts>
    struct alignas(16) aesni_keys_type<AesConsts, true> {
        m128i_type first_enc_key;
        alignas(16) std::array<m128i_type,
                               (AesConsts::key_expand_size / 16) - 2> enc_keys;
        m128i_type last_enc_key;
    };

#else
    template <class AesConsts, bool EncryptionOnly>
    struct aesni_keys_type {};
#endif // RAD_FAST_CRYPTO

    template <class AesConsts, bool EncryptionOnly = false>
    struct alignas(16) aes_expanded_key {
        using bytes_type = std::array<uint8_t, AesConsts::key_expand_size>;
        using words_type =
            std::array<aes_key_word_type, AesConsts::key_expand_size / 4>;
        using matrices_type =
            std::array<aes_key_matrix_type, AesConsts::key_expand_size / 16>;

        union {
            bytes_type key_bytes;
            words_type key_words;
            matrices_type key_matrices;
            aesni_keys_type<AesConsts, EncryptionOnly> aesni_keys;
        };
    };

    struct alignas(16) aes_state_type {
        union {
            std::array<aes_state_column_type, 4> columns;
            std::array<uint32_t, 4> u32s;
            std::array<uint64_t, 2> u64s;
            aes_state_matrix_type matrix;
        };
    };

    // aes-128
    RAD_EXPORT_DECL void
    aes_expand_key(aes_expanded_key<aes_128_constants>::words_type& round_key,
                   const aes_input_key<aes_128_constants>& key) noexcept;

    // aes-192
    RAD_EXPORT_DECL void
    aes_expand_key(aes_expanded_key<aes_192_constants>::words_type& round_key,
                   const aes_input_key<aes_192_constants>& key) noexcept;

    // aes-256
    RAD_EXPORT_DECL void
    aes_expand_key(aes_expanded_key<aes_256_constants>::words_type& round_key,
                   const aes_input_key<aes_256_constants>& key) noexcept;

    // aes-128
    RAD_EXPORT_DECL aes_state_type aes_encrypt(
        const aes_expanded_key<aes_128_constants>::matrices_type& round_key,
        aes_state_type state) noexcept;

    // aes-192
    RAD_EXPORT_DECL aes_state_type aes_encrypt(
        const aes_expanded_key<aes_192_constants>::matrices_type& round_key,
        aes_state_type state) noexcept;

    // aes-256
    RAD_EXPORT_DECL aes_state_type aes_encrypt(
        const aes_expanded_key<aes_256_constants>::matrices_type& round_key,
        aes_state_type state) noexcept;

    // aes-128
    RAD_EXPORT_DECL aes_state_type aes_decrypt(
        const aes_expanded_key<aes_128_constants>::matrices_type& round_key,
        aes_state_type state) noexcept;

    // aes-192
    RAD_EXPORT_DECL aes_state_type aes_decrypt(
        const aes_expanded_key<aes_192_constants>::matrices_type& round_key,
        aes_state_type state) noexcept;

    // aes-256
    RAD_EXPORT_DECL aes_state_type aes_decrypt(
        const aes_expanded_key<aes_256_constants>::matrices_type& round_key,
        aes_state_type state) noexcept;

    // input must be aligned to 16 bytes
    inline void aes_ctr_inc(std::array<uint8_t, 16>& iv) noexcept {
        struct u32parts {
            beu32 u3;
            beu32 u2;
            beu32 u1;
            beu32 u0;
        };

        static_assert(sizeof(u32parts) == sizeof(iv),
                      "sizeof u32parts != sizeof iv");

        u32parts& parts = *reinterpret_cast<u32parts*>(&iv);
        ++parts.u0;
        if (parts.u0) {
            return;
        }
        ++parts.u1;
        if (parts.u1) {
            return;
        }
        ++parts.u2;
        if (parts.u2) {
            return;
        }
        ++parts.u3;
        if (parts.u3) {
            return;
        }
    }

#ifdef RAD_FAST_CRYPTO
    template <int Rcon>
    struct aes_rcon_val {
        static constexpr int rcon = Rcon;
    };

    template <bool EncryptionOnly>
    inline RAD_CRYPTO_FN_ATTRIBUTE_AES void aesni_expand_enc_key(
        aes_expanded_key<aes_256_constants, EncryptionOnly>& round_keys,
        const aes_input_key<aes_256_constants>& key) noexcept {
        m128i_type X1 = _mm_setzero_si128(), X3 = _mm_setzero_si128();
        m128i_type X0 =
            *reinterpret_cast<const m128i_type*>(key.key_bytes.data());
        m128i_type X2 = *reinterpret_cast<const m128i_type*>(
            &key.key_bytes[sizeof(m128i_type)]);

        auto& rkeys = round_keys.aesni_keys;
        rkeys.first_enc_key = X0;
        rkeys.enc_keys[0] = X2;

        auto expand_key1 = [&]<class Rcon>(m128i_type& k,
                                           Rcon) RAD_CRYPTO_FN_ATTRIBUTE_AES {
            X1 = _mm_shuffle_epi32(_mm_aeskeygenassist_si128(X2, Rcon::rcon),
                                   0xff);
            X3 = _mm_castps_si128(_mm_shuffle_ps(_mm_castsi128_ps(X3),
                                                 _mm_castsi128_ps(X0), 0x10));
            X0 = _mm_xor_si128(X0, X3);
            X3 = _mm_castps_si128(_mm_shuffle_ps(_mm_castsi128_ps(X3),
                                                 _mm_castsi128_ps(X0), 0x8c));
            X0 = _mm_xor_si128(_mm_xor_si128(X0, X3), X1);
            k = X0;
        };

        auto expand_key2 = [&]<class Rcon>(m128i_type& k,
                                           Rcon) RAD_CRYPTO_FN_ATTRIBUTE_AES {
            X1 = _mm_shuffle_epi32(_mm_aeskeygenassist_si128(X0, Rcon::rcon),
                                   0xaa);
            X3 = _mm_castps_si128(_mm_shuffle_ps(_mm_castsi128_ps(X3),
                                                 _mm_castsi128_ps(X2), 0x10));
            X2 = _mm_xor_si128(X2, X3);
            X3 = _mm_castps_si128(_mm_shuffle_ps(_mm_castsi128_ps(X3),
                                                 _mm_castsi128_ps(X2), 0x8c));
            X2 = _mm_xor_si128(_mm_xor_si128(X2, X3), X1);
            k = X2;
        };

        expand_key1(rkeys.enc_keys[1], aes_rcon_val<0x1>{});
        expand_key2(rkeys.enc_keys[2], aes_rcon_val<0x1>{});

        expand_key1(rkeys.enc_keys[3], aes_rcon_val<0x2>{});
        expand_key2(rkeys.enc_keys[4], aes_rcon_val<0x2>{});

        expand_key1(rkeys.enc_keys[5], aes_rcon_val<0x4>{});
        expand_key2(rkeys.enc_keys[6], aes_rcon_val<0x4>{});

        expand_key1(rkeys.enc_keys[7], aes_rcon_val<0x8>{});
        expand_key2(rkeys.enc_keys[8], aes_rcon_val<0x8>{});

        expand_key1(rkeys.enc_keys[9], aes_rcon_val<0x10>{});
        expand_key2(rkeys.enc_keys[10], aes_rcon_val<0x10>{});

        expand_key1(rkeys.enc_keys[11], aes_rcon_val<0x20>{});
        expand_key2(rkeys.enc_keys[12], aes_rcon_val<0x20>{});

        expand_key1(rkeys.last_enc_key, aes_rcon_val<0x40>{});
    }

    template <bool EncryptionOnly>
    inline RAD_CRYPTO_FN_ATTRIBUTE_AES void aesni_expand_enc_key(
        aes_expanded_key<aes_192_constants, EncryptionOnly>& round_keys,
        const aes_input_key<aes_192_constants>& key) noexcept {
        aes_expand_key(round_keys.key_words, key);
    }

    template <bool EncryptionOnly>
    inline RAD_CRYPTO_FN_ATTRIBUTE_AES void aesni_expand_enc_key(
        aes_expanded_key<aes_128_constants, EncryptionOnly>& round_keys,
        const aes_input_key<aes_128_constants>& key) noexcept {
        auto& rkeys = round_keys.aesni_keys;
        rkeys.first_enc_key =
            *reinterpret_cast<const m128i_type*>(key.key_bytes.data());

        auto aes_expand_key = []<class Rcon>(m128i_type prev_key,
                                             Rcon) RAD_CRYPTO_FN_ATTRIBUTE_AES {
            m128i_type keygened =
                _mm_aeskeygenassist_si128(prev_key, Rcon::rcon);
            keygened = _mm_shuffle_epi32(keygened, _MM_SHUFFLE(3, 3, 3, 3));
            prev_key = _mm_xor_si128(prev_key, _mm_slli_si128(prev_key, 4));
            prev_key = _mm_xor_si128(prev_key, _mm_slli_si128(prev_key, 4));
            prev_key = _mm_xor_si128(prev_key, _mm_slli_si128(prev_key, 4));
            return _mm_xor_si128(prev_key, keygened);
        };

        rkeys.enc_keys[0] =
            aes_expand_key(rkeys.first_enc_key, aes_rcon_val<0x1>{});
        rkeys.enc_keys[1] =
            aes_expand_key(rkeys.enc_keys[0], aes_rcon_val<0x2>{});
        rkeys.enc_keys[2] =
            aes_expand_key(rkeys.enc_keys[1], aes_rcon_val<0x4>{});
        rkeys.enc_keys[3] =
            aes_expand_key(rkeys.enc_keys[2], aes_rcon_val<0x8>{});
        rkeys.enc_keys[4] =
            aes_expand_key(rkeys.enc_keys[3], aes_rcon_val<0x10>{});
        rkeys.enc_keys[5] =
            aes_expand_key(rkeys.enc_keys[4], aes_rcon_val<0x20>{});
        rkeys.enc_keys[6] =
            aes_expand_key(rkeys.enc_keys[5], aes_rcon_val<0x40>{});
        rkeys.enc_keys[7] =
            aes_expand_key(rkeys.enc_keys[6], aes_rcon_val<0x80>{});
        rkeys.enc_keys[8] =
            aes_expand_key(rkeys.enc_keys[7], aes_rcon_val<0x1b>{});

        rkeys.last_enc_key =
            aes_expand_key(rkeys.enc_keys.back(), aes_rcon_val<0x36>{});
    }

    template <class AesConsts, bool EncryptionOnly>
    inline void RAD_CRYPTO_FN_ATTRIBUTE_AES
    aesni_expand_key(aes_expanded_key<AesConsts, EncryptionOnly>& round_keys,
                     const aes_input_key<AesConsts>& key) noexcept {
        aesni_expand_enc_key(round_keys, key);
        if constexpr (!EncryptionOnly) {
            auto& rkeys = round_keys.aesni_keys;
            for (size_t i = 0; i < rkeys.dec_keys.size(); ++i) {
                rkeys.dec_keys[i] = _mm_aesimc_si128(
                    rkeys.enc_keys[rkeys.enc_keys.size() - 1 - i]);
            }
        }
    }

    template <class AesConsts, bool EncryptionOnly>
    inline RAD_CRYPTO_FN_ATTRIBUTE_AES m128i_type
    aesni_enc_block(m128i_type state,
                    const aesni_keys_type<AesConsts, EncryptionOnly>& keys) {
        state = _mm_xor_si128(state, keys.first_enc_key);
        for (size_t i = 0; i < keys.enc_keys.size(); ++i) {
            state = _mm_aesenc_si128(state, keys.enc_keys[i]);
        }
        return _mm_aesenclast_si128(state, keys.last_enc_key);
    }

    template <class AesConsts>
    inline RAD_CRYPTO_FN_ATTRIBUTE_AES m128i_type aesni_dec_block(
        m128i_type state, const aesni_keys_type<AesConsts, false>& keys) {
        state = _mm_xor_si128(state, keys.first_dec_key);
        for (size_t i = 0; i < keys.dec_keys.size(); ++i) {
            state = _mm_aesdec_si128(state, keys.dec_keys[i]);
        }
        return _mm_aesdeclast_si128(state, keys.last_dec_key);
    }
#endif // RAD_FAST_CRYPTO
} // namespace RAD_LIB_NAMESPACE::crypto::detail

namespace RAD_LIB_NAMESPACE::crypto {

    /*!
     * @brief Check if CPU AES-NI instructions are used to
     * perform aes encryption and decryption.
     * The CPU AES-NI instructions are used only if the cpu supports them
     * and they are not disabled using disable_aesni().
     * @return True if CPU AES-NI instructions are used, otherwise false.
     */
    inline bool using_aesni() noexcept {
#ifdef RAD_FAST_CRYPTO
        return detail::use_cpu_aesni_if_available && cpu::aes();
#else
        return false;
#endif // RAD_FAST_CRYPTO
    }

    /*!
     * @brief Disable use of CPU AES-NI instructions even if the cpu
     * supports them. This will result in slower aes operations. This
     * function is provided for testing only. Also, this function is not
     * thread safe.
     */
    inline void disable_aesni() noexcept {
        detail::use_cpu_aesni_if_available = false;
    }

    /*!
     * @brief Enable use of CPU AES-NI instructions if the cpu supports
     * them. This will result in faster aes operations. This function is
     * provided for testing only. Also, this function is not thread safe.
     */
    inline void enable_aesni() noexcept {
        detail::use_cpu_aesni_if_available = true;
    }

    /*!
     * @brief AES key for encryption and decryption.
     * Usually this is not used directly but the specialized types:
     * aes, aes128, aes192 and aes256.
     * @tparam AesConsts The aes constants to use.
     * @tparam EncryptionOnly Whether only encryption operations are to be
     * supported. Setting this to true may result in smaller size of the
     * key.
     */
    template <class AesConsts, bool EncryptionOnly = false>
    class alignas(16) aes_base : public trackable {
        static bool is_ptr_aligned_16(const uint8_t* p) {
            return reinterpret_cast<uintptr_t>(p) % 16 == 0;
        }

        static bool is_ptr_aligned_16(const m128i_type* p) {
            return reinterpret_cast<uintptr_t>(p) % 16 == 0;
        }

    public:
        /*!
         * @brief The block size in bytes of AES cipher.
         * Typically 16.
         */
        static constexpr std::size_t block_length = 16;
        /*!
         * @brief The key size in bytes of this variant of AES
         * cipher.
         */
        static constexpr std::size_t key_length = AesConsts::key_size;
        /*!
         * @brief The key type as array of bytes of length
         * key_length.
         */
        using key_type = std::array<uint8_t, key_length>;
        /*!
         * @brief The block type as array of bytes of length
         * block_length.
         */
        using block_type = std::array<uint8_t, block_length>;
        /*!
         * @brief The type of the encryption only cipher for
         * this variant of AES.
         */
        using encryption_only_cipher = aes_base<AesConsts, true>;

        /*!
         * @brief Default constructed aes key is invalid.
         * has_key() will return false.
         */
        aes_base() = default;

        /*!
         * @brief Construct aes key with array of bytes key.
         * @param key The aes key in bytes format.
         */
        aes_base(const key_type& key) noexcept {
            set_key(key);
        }

        /*!
         * @brief Construct aes key with buffer of key data.
         * If size of @p key_buff is not the same as key_length,
         * an exception is thrown.
         * @param key_buff The buffer containing the aes key.
         */
        aes_base(const_buffer key_buff) noexcept {
            set_key(key_buff);
        }

        /*!
         * @brief Get the encryption only cipher for this
         * variant of AES.
         * @return The encryption only cipher for this variant
         * of AES.
         */
        std::conditional_t<EncryptionOnly, const encryption_only_cipher&,
                           encryption_only_cipher>
        get_encryption_only_cipher() const noexcept {
            if constexpr (EncryptionOnly) {
                return *this;
            }
            else {
                return encryption_only_cipher{input_key_.key_bytes};
            }
        }

        /*!
         * @brief Set aes key with array of bytes key.
         * @param key The aes key in bytes format.
         */
        void set_key(const key_type& key) noexcept {
            input_key_.key_bytes = key;
#ifdef RAD_FAST_CRYPTO
            if (using_aesni()) {
                detail::aesni_expand_key(expanded_key_, input_key_);
            }
            else
#endif // RAD_FAST_CRYPTO
                detail::aes_expand_key(expanded_key_.key_words, input_key_);
            has_key_ = true;
        }

        /*!
         * @brief Set aes key with buffer of key data.
         * If size of @p key_buff is not the same as key_length,
         * an exception is thrown.
         * @param key_buff The buffer containing the aes key.
         */
        void set_key(const_buffer key_buff) {
            if (key_buff.size() != key_length) {
                throw std::system_error(
                    std::make_error_code(std::errc::invalid_argument));
            }
            auto key_span = key_buff.to_span<const uint8_t, key_length>();
            std::copy(key_span.begin(), key_span.end(),
                      input_key_.key_bytes.begin());
#ifdef RAD_FAST_CRYPTO
            if (using_aesni()) {
                detail::aesni_expand_key(expanded_key_, input_key_);
            }
            else
#endif // RAD_FAST_CRYPTO
                detail::aes_expand_key(expanded_key_.key_words, input_key_);
            has_key_ = true;
        }

        /*!
         * @brief Check if this aes key is valid and has a key.
         * @return True if this aes key is valid and has a key,
         * otherwise false.
         */
        bool has_key() const noexcept {
            return has_key_;
        }

        /*!
         * @brief Return the aes key in bytes format.
         * @return The aes key in bytes format.
         */
        const key_type& key() const noexcept {
            assert(has_key_);
            return input_key_.key_bytes;
        }

        /*!
         * @brief Encrypt up to 16 bytes (block_length) of
         * buffer @p buff using the current aes key. If @p buff
         * is not aligned to 16 bytes, the behavior is
         * undefined. If @p buff points to buff with length less
         * than 16, the behavior is undefined. If no aes key is
         * set, the behavior is undefined.
         * @param buff Pointer to buffer of at least 16 bytes to
         * encrypt up to 16 bytes (block_length) of it.
         */
        void encrypt(uint8_t* buff) const noexcept {
            assert(is_ptr_aligned_16(buff));
            assert(has_key());
#ifdef RAD_FAST_CRYPTO
            if (using_aesni()) {
                m128i_type state = *reinterpret_cast<const m128i_type*>(buff);
                state = detail::aesni_enc_block<AesConsts>(
                    state, expanded_key_.aesni_keys);
                *reinterpret_cast<m128i_type*>(buff) = state;
                return;
            }
#endif // RAD_FAST_CRYPTO
            *reinterpret_cast<detail::aes_state_type*>(buff) =
                detail::aes_encrypt(
                    expanded_key_.key_matrices,
                    *reinterpret_cast<detail::aes_state_type*>(buff));
        }

        /*!
         * @brief Encrypt @p block using the current aes key.
         * If @p block is not aligned to 16 bytes, the behavior
         * is undefined. If no aes key is set, the behavior is
         * undefined.
         * @param block Block buffer to encrypt.
         */
        void encrypt(block_type& block) const noexcept {
            assert(is_ptr_aligned_16(block.data()));
            assert(has_key());
#ifdef RAD_FAST_CRYPTO
            if (using_aesni()) {
                m128i_type state =
                    *reinterpret_cast<const m128i_type*>(block.data());
                state = detail::aesni_enc_block<AesConsts>(
                    state, expanded_key_.aesni_keys);
                *reinterpret_cast<m128i_type*>(block.data()) = state;
                return;
            }
#endif // RAD_FAST_CRYPTO
            *reinterpret_cast<detail::aes_state_type*>(block.data()) =
                detail::aes_encrypt(
                    expanded_key_.key_matrices,
                    *reinterpret_cast<detail::aes_state_type*>(block.data()));
        }

        /*!
         * @brief Encrypt 128-bit integer @p block using the
         * current aes key. If @p block is not aligned to 16
         * bytes, the behavior is undefined. If no aes key is
         * set, the behavior is undefined.
         * @param block 128-bit integer to encrypt.
         */
        void enrypt(m128i_type& block) const noexcept {
            enrypt_m128i(block);
        }

        /*!
         * @brief Encrypt 128-bit integer @p block using the
         * current aes key. If @p block is not aligned to 16
         * bytes, the behavior is undefined. If no aes key is
         * set, the behavior is undefined.
         * @param block 128-bit integer to encrypt.
         */
        void enrypt_m128i(m128i_type& block) const noexcept {
            assert(is_ptr_aligned_16(&block));
            assert(has_key());
#ifdef RAD_FAST_CRYPTO
            if (using_aesni()) {
                block = detail::aesni_enc_block<AesConsts>(
                    block, expanded_key_.aesni_keys);
                return;
            }
#endif // RAD_FAST_CRYPTO
            *reinterpret_cast<detail::aes_state_type*>(&block) =
                detail::aes_encrypt(
                    expanded_key_.key_matrices,
                    *reinterpret_cast<detail::aes_state_type*>(&block));
        }

        /*!
         * @brief Decrypt up to 16 bytes (block_length) of
         * buffer @p buff using the current aes key. If @p buff
         * is not aligned to 16 bytes, the behavior is
         * undefined. If @p buff points to buff with length less
         * than 16, the behavior is undefined. If no aes key is
         * set, the behavior is undefined.
         * @param buff Pointer to buffer of at least 16 bytes to
         * decrypt up to 16 bytes (block_length) of it.
         */
        void decrypt(uint8_t* buff) const noexcept
            requires(!EncryptionOnly)
        {
            assert(is_ptr_aligned_16(buff));
            assert(has_key());
#ifdef RAD_FAST_CRYPTO
            if (using_aesni()) {
                m128i_type state = *reinterpret_cast<const m128i_type*>(buff);
                state = detail::aesni_dec_block<AesConsts>(
                    state, expanded_key_.aesni_keys);
                *reinterpret_cast<m128i_type*>(buff) = state;
                return;
            }
#endif // RAD_FAST_CRYPTO
            *reinterpret_cast<detail::aes_state_type*>(buff) =
                detail::aes_decrypt(
                    expanded_key_.key_matrices,
                    *reinterpret_cast<detail::aes_state_type*>(buff));
        }

        /*!
         * @brief Decrypt @p block using the current aes key.
         * If @p block is not aligned to 16 bytes, the behavior
         * is undefined. If no aes key is set, the behavior is
         * undefined.
         * @param block Block buffer to decrypt.
         */
        void decrypt(block_type& block) const noexcept
            requires(!EncryptionOnly)
        {
            assert(is_ptr_aligned_16(block.data()));
            assert(has_key());
#ifdef RAD_FAST_CRYPTO
            if (using_aesni()) {
                m128i_type state =
                    *reinterpret_cast<const m128i_type*>(block.data());
                state = detail::aesni_dec_block<AesConsts>(
                    state, expanded_key_.aesni_keys);
                *reinterpret_cast<m128i_type*>(block.data()) = state;
                return;
            }
#endif // RAD_FAST_CRYPTO
            *reinterpret_cast<detail::aes_state_type*>(&block) =
                detail::aes_decrypt(
                    expanded_key_.key_matrices,
                    *reinterpret_cast<detail::aes_state_type*>(&block));
        }

        /*!
         * @brief Decrypt 128-bit integer @p block using the
         * current aes key. If @p block is not aligned to 16
         * bytes, the behavior is undefined. If no aes key is
         * set, the behavior is undefined.
         * @param block 128-bit integer to decrypt.
         */
        void decrypt(m128i_type& block) {
            assert(is_ptr_aligned_16(&block));
            assert(has_key());
#ifdef RAD_FAST_CRYPTO
            if (using_aesni()) {
                block = detail::aesni_dec_block<AesConsts>(
                    block, expanded_key_.aesni_keys);
                return;
            }
#endif // RAD_FAST_CRYPTO
            *reinterpret_cast<detail::aes_state_type*>(&block) =
                detail::aes_decrypt(
                    expanded_key_.key_matrices,
                    *reinterpret_cast<detail::aes_state_type*>(&block));
        }

        /*!
         * @brief Perform CTR increment operation on @p block.
         * If @p block is not aligned to 16 bytes, the behavior
         * is undefined.
         * @param block The block to increment.
         */
        static void ctr_increment(block_type& block) noexcept {
            assert(is_ptr_aligned_16(block.data()));
            detail::aes_ctr_inc(block);
        }

    private:
        detail::aes_input_key<AesConsts> input_key_;
        detail::aes_expanded_key<AesConsts, EncryptionOnly> expanded_key_;
        bool has_key_ = false;
    };

    using aes = aes_base<detail::aes_128_constants>;
    using aes128 = aes_base<detail::aes_128_constants>;
    using aes192 = aes_base<detail::aes_192_constants>;
    using aes256 = aes_base<detail::aes_256_constants>;
}; // namespace RAD_LIB_NAMESPACE::crypto

#ifdef __GNUC__
#ifdef __clang__
#pragma clang diagnostic pop
#else
#pragma GCC diagnostic pop
#endif // __clang__
#endif // __GNUC__