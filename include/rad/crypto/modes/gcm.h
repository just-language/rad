#pragma once
#include <rad/buffer.h>
#include <rad/crypto/detail/m128.h>
#include <rad/crypto/source_sink.h>
#include <rad/dynamic_buffer.h>
#include <rad/libbase.h>

#include <istream>
#include <ostream>

#ifdef __GNUC__
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wignored-attributes"
#else
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wignored-attributes"
#endif // __clang__
#endif // __GNUC__

namespace RAD_LIB_NAMESPACE::crypto {
    namespace detail {
        struct iv_storage {
            struct beus {
                beus() = default;

                beu32 u3;
                beu32 u2;
                beu32 u1;
                beu32 inc_u;
            };

            union {
                std::array<uint8_t, 16> Yi;
                beus u32parts;
            };

            iv_storage() noexcept : u32parts{} {
            }

            iv_storage(const iv_storage& other) noexcept : Yi{other.Yi} {
            }
        };

        struct unaligned_block_t {
            uint64_t u64s[2];
        };

        static_assert(sizeof(unaligned_block_t) == 16);
    } // namespace detail

    // not thread safe, use for testing only !
    RAD_EXPORT_DECL void disable_pclmulqdq() noexcept;

    // not thread safe, use for testing only !
    RAD_EXPORT_DECL void enable_pclmulqdq() noexcept;

    RAD_EXPORT_DECL const std::error_category& gcm_category() noexcept;

    class ghash {
    public:
        static constexpr std::size_t block_length = 16;
        using block_type = std::array<uint8_t, block_length>;

        RAD_EXPORT_DECL static __m128i step(__m128i H, __m128i X,
                                            const_buffer data) noexcept;

        RAD_EXPORT_DECL static __m128i step(__m128i H, __m128i X,
                                            __m128i block) noexcept;

        RAD_EXPORT_DECL static __m128i merge_lengths(uint64_t a_len,
                                                     uint64_t c_len) noexcept;

        RAD_EXPORT_DECL static __m128i calc(__m128i H, const_buffer Ablocks,
                                            const_buffer Cblocks) noexcept;
    };

    template <class Cipher>
    class alignas(16) gcm_mode {
        using iv_storage = detail::iv_storage;
        using unaligned_block_t = detail::unaligned_block_t;

    public:
        using cipher_type = typename Cipher::encryption_only_cipher;

        static constexpr std::size_t block_length = 16;
        static constexpr std::size_t default_tag_length = 16;
        static constexpr std::size_t default_iv_length = 12;

        using block_type = std::array<uint8_t, block_length>;
        using default_iv_type = std::array<uint8_t, default_iv_length>;
        using tag_type = std::array<uint8_t, block_length>;
        using iv_type = std::array<uint8_t, block_length>;
        using key_type = typename cipher_type::key_type;

        using encryption = gcm_mode;
        using decryption = gcm_mode;

        static_assert(Cipher::block_length == 16,
                      "the underlying cipher block size must be 16 "
                      "bytes (128 bits)");

        gcm_mode() = default;

        gcm_mode(const key_type& key, const_buffer iv) noexcept : cipher_{key} {
            update_H();
            set_iv(iv);
        }

        gcm_mode(const key_type& key, const default_iv_type& iv) noexcept
            : cipher_{key} {
            update_H();
            set_iv(iv);
        }

        gcm_mode(const_buffer key, const default_iv_type& iv) noexcept
            : cipher_{key} {
            update_H();
            set_iv(iv);
        }

        gcm_mode(const_buffer key, const_buffer iv) noexcept : cipher_{key} {
            update_H();
            set_iv(iv);
        }

        /*!
         * @brief Construct a gcm_mode with the provided cipher
         * and if the cipher has a set key call key_updated()
         * after construction the iv is not set and must be set
         * before any encryption or decryption is done
         * @param cipher the cipher to use in gcm mode which
         * must have a block size of 16 bytes
         */
        gcm_mode(cipher_type cipher) noexcept : cipher_{cipher} {
            if (cipher_.has_key()) {
                update_H();
            }
        }

        /*!
         * @brief Construct a gcm_mode with the provided cipher
         * and if the cipher has a set key call key_updated()
         * after construction the iv is not set and must be set
         * before any encryption or decryption is done
         * @param cipher the cipher to use in gcm mode which
         * must have a block size of 16 bytes
         */
        gcm_mode(const Cipher& cipher) noexcept
            requires(!std::is_same_v<Cipher, cipher_type>)
            : cipher_{cipher.get_encryption_only_cipher()} {
            if (cipher_.has_key()) {
                update_H();
            }
        }

        /*!
         * @brief Construct a gcm_mode with the provided cipher
         * which must have a set key and use the iv for
         * encryption or decryption
         * @param cipher the cipher to use in gcm mode which
         * must have a block size of 16 bytes
         * @param iv the iv to use for encryption and
         * decryption. This is preferred to be 12 bytes long
         */
        gcm_mode(cipher_type cipher, const_buffer iv) noexcept
            : cipher_{cipher} {
            assert(cipher.has_key());
            update_H();
            set_iv(iv);
        }

        /*!
         * @brief Construct a gcm_mode with the provided cipher
         * which must have a set key and use the iv for
         * encryption or decryption
         * @param cipher the cipher to use in gcm mode which
         * must have a block size of 16 bytes
         * @param iv the iv to use for encryption and
         * decryption. This is preferred to be 12 bytes long
         */
        gcm_mode(const Cipher& cipher, const_buffer iv) noexcept
            requires(!std::is_same_v<Cipher, cipher_type>)
            : cipher_{cipher.get_encryption_only_cipher()} {
            assert(cipher.has_key());
            update_H();
            set_iv(iv);
        }

        cipher_type& get_cipher() noexcept {
            return cipher_;
        }

        const cipher_type& get_cipher() const noexcept {
            return cipher_;
        }

        const iv_type& get_iv() const noexcept {
            return iv_counter_.Yi;
        }

        bool has_key() const noexcept {
            return cipher_.has_key();
        }

        /*!
         * @brief calls set_key on the underlying cipher then
         * calls key_updated
         * @param key the key to pass to the underlying cipher
         */
        void set_key(const key_type& key) {
            cipher_.set_key(key);
            key_updated();
        }

        /*!
         * @brief calls set_key on the underlying cipher then
         * calls key_updated
         * @param key_buff the key to pass to the underlying
         * cipher
         */
        void set_key(const_buffer key_buff) {
            cipher_.set_key(key_buff);
            key_updated();
        }

        /*!
         * @brief Inform the gcm object that the cipher key was
         * set or changed so that it updates the values of H and
         * E(K, Y0). These values are per key and needs to be
         * computed only if the key is changed. This method is
         * not required if the gcm set_key method was used to
         * update the key
         */
        void key_updated() noexcept {
            update_H();
            update_EKY0_();
        }

        /*!
         * @brief Set or change the current iv from a 12 bytes
         * (96 bits) iv which is preferrable than other sizes
         * @param iv used to generate the gcm counter
         */
        void set_iv(const default_iv_type& iv) noexcept {
            assert(has_key());
            std::copy(iv.begin(), iv.end(), Y0_.begin());
            *reinterpret_cast<beu32*>(Y0_.data() + iv.size()) = 1;
            iv_counter_.Yi = Y0_;
            update_EKY0_();
        }

        /*!
         * @brief Set or change the current iv
         * @param iv used to generate the gcm counter and is
         * preferred to be 12 bytes long (96 bits)
         */
        void set_iv(const_buffer iv) noexcept {
            assert(has_key());
            Y0_ = {};
            if (iv.size() == default_iv_length) {
                auto iv_span = iv.to_span<const uint8_t, default_iv_length>();
                std::copy(iv_span.begin(), iv_span.end(), Y0_.begin());
                Y0_.back() = 1;
            }
            else {
                // Y0_ is aligned to 16
                m128i_utils::store(ghash::calc(H, buffer(nullptr), iv),
                                   Y0_.data());
            }
            iv_counter_.Yi = Y0_;
            update_EKY0_();
        }

        void set_key_iv(const key_type& key, const default_iv_type& iv) {
            cipher_.set_key(key);
            update_H();
            set_iv(iv);
        }

        void set_key_iv(const_buffer key, const default_iv_type& iv) {
            cipher_.set_key(key);
            update_H();
            set_iv(iv);
        }

        void set_key_iv(const key_type& key, const_buffer iv) {
            cipher_.set_key(key);
            update_H();
            set_iv(iv);
        }

        void set_key_iv(const_buffer key, const_buffer iv) {
            cipher_.set_key(key);
            update_H();
            set_iv(iv);
        }

        /*!
         * @brief Encrypt data in place using the underlying
         * cipher in gcm mode and use additional data for
         * authentication and produce an authentication tag. The
         * stored iv is incremented by the number of blocks
         * processed
         * @param[in,out] input_output on input the plaintext
         * data to encrypt, on output the encrypted ciphertext
         * @param[in] additional_data data to be authenticated,
         * this data is not encrypted but used to generate the
         * authentication tag and must be passed to the
         * decryption routine to verify the tag
         * @param[out] tag on output the generated
         * authentication tag truncated to the min of 16 bytes
         * and provided tag buffer size
         */
        void encrypt(mutable_buffer input_output, const_buffer additional_data,
                     mutable_buffer tag) noexcept {
            assert(has_key());
            encrypt_blocks(input_output);
            auto C = input_output;
            auto A = additional_data;
            __m128i T = m128i_utils::xor128(ghash::calc(H, A, C), EKY0_);

            m128i_utils::copy_to(tag.data(), &T,
                                 std::min(tag.size(), sizeof(T)));
        }

        /*!
         * @brief Encrypt data in place using the underlying
         * cipher in gcm mode and produce an authentication tag.
         * The stored iv is incremented by the number of blocks
         * processed
         * @param[in,out] input_output on input the plaintext
         * data to encrypt, on output the encrypted ciphertext
         * @param[out] tag on output the generated
         * authentication tag truncated to the min of 16 bytes
         * and provided tag buffer size
         */
        void encrypt(mutable_buffer input_output, mutable_buffer tag) noexcept {
            encrypt(input_output, buffer(nullptr), tag);
        }

        /*!
         * @brief Encrypt data using the underlying cipher in
         * gcm mode and use additional data for authentication
         * and produce an authentication tag. The stored iv is
         * incremented by the number of blocks processed
         * @param[in] plaintext the plaintext data to encrypt
         * @param[out] ciphertext the encrypted ciphertext will
         * be appended to it
         * @param[in] add_data data to be authenticated, this
         * data is not encrypted but used to generate the
         * authentication tag and must be passed to the
         * decryption routine to verify the tag
         * @param[out] tag on output the generated
         * authentication tag truncated to the min of 16 bytes
         * and provided tag buffer size
         */
        void encrypt(const_buffer plaintext, dynamic_buffer ciphertext,
                     const_buffer add_data, mutable_buffer tag) {
            auto output = buffer(ciphertext.increase_size(plaintext.size()),
                                 plaintext.size());
            std::memcpy(output.data(), plaintext.data(), plaintext.size());
            encrypt(output, add_data, tag);
        }

        /*!
         * @brief Encrypt data using the underlying cipher in
         * gcm mode and produce an authentication tag. The
         * stored iv is incremented by the number of blocks
         * processed
         * @param[in] plaintext the plaintext data to encrypt
         * @param[out] ciphertext the encrypted ciphertext will
         * be appended to it
         * @param[out] tag on output the generated
         * authentication tag truncated to the min of 16 bytes
         * and provided tag buffer size
         */
        void encrypt(const_buffer plaintext, dynamic_buffer ciphertext,
                     mutable_buffer tag) {
            auto output = buffer(ciphertext.increase_size(plaintext.size()),
                                 plaintext.size());
            std::memcpy(output.data(), plaintext.data(), plaintext.size());
            encrypt(output, tag);
        }

        /*!
         * @brief Reads plaintext from @p is input stream into
         * @p read_buffer, encrypt plaintext using the
         * underlying cipher in gcm mode and write the
         * ciphertext into @p os output stream. This is done in
         * a loop until
         * @p is reaches eof
         * @param is the input stream to read plaintext data
         * from
         * @param os the output stream to write ciphertext data
         * to
         * @param aad_data data to be authenticated, this data
         * is not encrypted but used to generate the
         * authentication tag and must be passed to the
         * decryption routine to verify the tag
         * @param tag on output the generated authentication tag
         * truncated to the min of 16 bytes and provided tag
         * buffer size
         * @param read_buffer The plaintext is fetched from the
         * input stream into this buffer, encrypted then written
         * back to output stream
         */
        void encrypt(std::istream& is, std::ostream& os, const_buffer aad_data,
                     mutable_buffer tag, mutable_buffer read_buffer) {
            std::array<char, block_length> stack_buffer;

            char* read_ptr = read_buffer.data_as<char>();
            std::size_t read_size = read_buffer.size();

            if (read_size < block_length) {
                read_ptr = stack_buffer.data();
                read_size = block_length;
            }
            else if (read_size % block_length) {
                read_size -= read_size % block_length;
            }

            __m128i X = m128i_utils::zero128();
            X = ghash::step(H, X, aad_data);

            uint64_t cipher_size = 0;

            while (!is.eof()) {
                is.read(read_ptr, read_size);
                std::size_t n = static_cast<std::size_t>(is.gcount());
                if (!n) {
                    break;
                }

                auto ciphertext = buffer(read_ptr, n);
                encrypt_no_auth(ciphertext);
                X = ghash::step(H, X, ciphertext);
                cipher_size += n;
                os.write(read_ptr, n);

                if (n < read_size) {
                    break;
                }
            }

            X = ghash::step(
                H, X,
                ghash::merge_lengths(aad_data.size() * 8, cipher_size * 8));
            __m128i T = m128i_utils::xor128(X, EKY0_);
            m128i_utils::copy_to(tag.data(), &T,
                                 std::min(tag.size(), sizeof(T)));
        }

        /*!
         * @brief Reads plaintext from @p is input stream into
         * @p read_buffer, encrypt plaintext using the
         * underlying cipher in gcm mode and write the
         * ciphertext into @p os output stream. This is done in
         * a loop until
         * @p is reaches eof
         * @param is the input stream to read plaintext data
         * from
         * @param os the output stream to write ciphertext data
         * to
         * @param tag on output the generated authentication tag
         * truncated to the min of 16 bytes and provided tag
         * buffer size
         * @param read_buffer The plaintext is fetched from the
         * input stream into this buffer, encrypted then written
         * back to output stream
         */
        void encrypt(std::istream& is, std::ostream& os, mutable_buffer tag,
                     mutable_buffer read_buffer) {
            encrypt(is, os, buffer(nullptr), tag, read_buffer);
        }

        /*!
         * @brief Reads plaintext from @p is input stream into a
         * heap allocated buffer, encrypt plaintext using the
         * underlying cipher in gcm mode and write the
         * ciphertext into @p os output stream. This is done in
         * a loop until @p is reaches eof
         * @param is the input stream to read plaintext data
         * from
         * @param os the output stream to write ciphertext data
         * to
         * @param aad_data data to be authenticated, this data
         * is not encrypted but used to generate the
         * authentication tag and must be passed to the
         * decryption routine to verify the tag
         * @param tag on output the generated authentication tag
         * truncated to the min of 16 bytes and provided tag
         * buffer size
         * @param read_size The size of heap buffer to be used.
         * If this is 0 then the default size 16 KB is used
         */
        void encrypt(std::istream& is, std::ostream& os, const_buffer aad_data,
                     mutable_buffer tag, std::size_t read_size = 0) {
            constexpr std::size_t default_read_size = 16 * 1024;
            if (read_size) {
                size_t rem = read_size % block_length;
                if (rem) {
                    read_size += block_length - rem;
                }
            }
            else {
                read_size = default_read_size;
            }

            std::vector<char> read_buffer(read_size);
            encrypt(is, os, aad_data, tag, buffer(read_buffer));
        }

        /*!
         * @brief Reads plaintext from @p is input stream into a
         * heap allocated buffer, encrypt plaintext using the
         * underlying cipher in gcm mode and write the
         * ciphertext into @p os output stream. This is done in
         * a loop until @p is reaches eof
         * @param is the input stream to read plaintext data
         * from
         * @param os the output stream to write ciphertext data
         * to
         * @param tag on output the generated authentication tag
         * truncated to the min of 16 bytes and provided tag
         * buffer size
         * @param read_size The size of heap buffer to be used.
         * If this is 0 then the default size 16 KB is used
         */
        void encrypt(std::istream& is, std::ostream& os, mutable_buffer tag,
                     std::size_t read_size = 0) {
            encrypt(is, os, buffer(nullptr), tag, read_size);
        }

        /*!
         * @brief Reads plaintext from @p is input stream into a
         * stack allocated buffer, encrypt plaintext using the
         * underlying cipher in gcm mode and write the
         * ciphertext into @p os output stream. This is done in
         * a loop until @p is reaches eof. The stack buffer size
         * is N aligned to multiple of block size or the default
         * size 16 KB if N is 0
         * @param is the input stream to read plaintext data
         * from
         * @param os the output stream to write ciphertext data
         * to
         * @param aad_data data to be authenticated, this data
         * is not encrypted but used to generate the
         * authentication tag and must be passed to the
         * decryption routine to verify the tag
         * @param tag on output the generated authentication tag
         * truncated to the min of 16 bytes and provided tag
         * buffer size
         */
        template <std::size_t N>
        void encrypt(std::istream& is, std::ostream& os, const_buffer aad_data,
                     mutable_buffer tag) {
            constexpr std::size_t default_read_size = 16 * 1024;
            constexpr std::size_t aligned_size =
                !N ? default_read_size
                   : (!(N % block_length)
                          ? N
                          : N + (block_length - N % block_length));
            std::array<uint8_t, aligned_size> read_buffer;
            encrypt(is, os, aad_data, tag, buffer(read_buffer));
        }

        /*!
         * @brief Reads plaintext from @p is input stream into a
         * stack allocated buffer, encrypt plaintext using the
         * underlying cipher in gcm mode and write the
         * ciphertext into @p os output stream. This is done in
         * a loop until @p is reaches eof. The stack buffer size
         * is N aligned to multiple of block size or the default
         * size 16 KB if N is 0
         * @param is the input stream to read plaintext data
         * from
         * @param os the output stream to write ciphertext data
         * to
         * @param tag on output the generated authentication tag
         * truncated to the min of 16 bytes and provided tag
         * buffer size
         */
        template <std::size_t N>
        void encrypt(std::istream& is, std::ostream& os, mutable_buffer tag) {
            encrypt<N>(is, os, buffer(nullptr), tag);
        }

        void encrypt_no_auth(mutable_buffer input_output) {
            encrypt_blocks(input_output);
        }

        void encrypt_no_auth(const_buffer plaintext,
                             dynamic_buffer ciphertext) {
            auto output = buffer(ciphertext.increase_size(plaintext.size()),
                                 plaintext.size());
            std::memcpy(output.data(), plaintext.data(), plaintext.size());
            encrypt_no_auth(output);
        }

        /*!
         * @brief Decrypts data in place using the underlying
         * cipher in gcm mode after verifying the data integirty
         * using authentication tag and additional data. The
         * stored iv is incremented by the number of blocks
         * processed
         * @param input_output on input the encrypted ciphertext
         * to decrypt, on output the decrypted plaintext data
         * @param aad_data additional authentication data used
         * to compute the authentication tag
         * @param tag authentication tag to match with the
         * computed tag
         * @param ec will be set if authentication fails,
         * otherwise leaved untouched
         */
        void decrypt(mutable_buffer input_output, const_buffer aad_data,
                     const_buffer tag, std::error_code& ec) noexcept {
            auto C = input_output;
            auto A = aad_data;
            auto T = m128i_utils::xor128(ghash::calc(H, A, C), EKY0_);

            tag_type computed_buff;
            m128i_utils::copy_to(computed_buff.data(), &T,
                                 computed_buff.size());
            if (std::memcmp(computed_buff.data(), tag.data(),
                            std::min(tag.size(), computed_buff.size())) != 0) {
                return make_gcm_auth_error_code(ec);
            }

            encrypt_blocks(input_output);
        }

        /*!
         * @brief Decrypts data in place using the underlying
         * cipher in gcm mode after verifying the data integirty
         * using authentication tag and additional data. The
         * stored iv is incremented by the number of blocks
         * processed
         * @param input_output on input the encrypted ciphertext
         * to decrypt, on output the decrypted plaintext data
         * @param tag authentication tag to match with the
         * computed tag
         * @param ec will be set if authentication fails,
         * otherwise leaved untouched
         */
        void decrypt(mutable_buffer input_output, const_buffer tag,
                     std::error_code& ec) noexcept {
            decrypt(input_output, buffer(nullptr), tag, ec);
        }

        /*!
         * @brief Decrypts data in place using the underlying
         * cipher in gcm mode after verifying the data integirty
         * using authentication tag and additional data. The
         * stored iv is incremented by the number of blocks
         * processed
         * @param input_output on input the encrypted ciphertext
         * to decrypt, on output the decrypted plaintext data
         * @param aad_data additional authentication data used
         * to compute the authentication tag
         * @param tag authentication tag to match with the
         * computed tag
         * @throws if authentication fails an instance of
         * std::runtime_error is thrown
         */
        void decrypt(mutable_buffer input_output, const_buffer aad_data,
                     const_buffer tag) {
            std::error_code ec;
            decrypt(input_output, aad_data, tag, ec);
            check_and_throw_gcm_error(ec);
        }

        /*!
         * @brief Decrypts data in place using the underlying
         * cipher in gcm mode after verifying the data integirty
         * using authentication tag and additional data. The
         * stored iv is incremented by the number of blocks
         * processed
         * @param input_output on input the encrypted ciphertext
         * to decrypt, on output the decrypted plaintext data
         * @param tag authentication tag to match with the
         * computed tag
         * @throws if authentication fails an instance of
         * std::runtime_error is thrown
         */
        void decrypt(mutable_buffer input_output, const_buffer tag) {
            std::error_code ec;
            decrypt(input_output, tag, ec);
            check_and_throw_gcm_error(ec);
        }

        /*!
         * @brief
         * @param plaintext
         * @param ciphertext
         * @param aad
         * @param tag
         * @param ec
         */
        void decrypt(const_buffer plaintext, dynamic_buffer ciphertext,
                     const_buffer aad, const_buffer tag, std::error_code& ec) {
            auto output_buffer = buffer(
                ciphertext.increase_size(plaintext.size()), plaintext.size());
            std::memcpy(output_buffer.data(), plaintext.data(),
                        plaintext.size());
            decrypt(output_buffer, aad, tag, ec);
        }

        void decrypt(const_buffer plaintext, dynamic_buffer ciphertext,
                     const_buffer aad, const_buffer tag) {
            std::error_code ec;
            decrypt(plaintext, ciphertext, aad, tag, ec);
            check_and_throw_gcm_error(ec);
        }

        void decrypt(const_buffer plaintext, dynamic_buffer ciphertext,
                     const_buffer tag, std::error_code& ec) {
            decrypt(plaintext, ciphertext, buffer(nullptr), tag, ec);
        }

        void decrypt(const_buffer plaintext, dynamic_buffer ciphertext,
                     const_buffer tag) {
            decrypt(plaintext, ciphertext, buffer(nullptr), tag);
        }

        void decrypt_no_auth(mutable_buffer input_output) noexcept {
            size_t u = input_output.size() % block_length;

            encrypt_blocks(input_output);

            if (u) {
                incr(iv_counter_);
                alignas(16) auto EKYn = iv_counter_.Yi;
                cipher_.encrypt(EKYn.data());
                auto Ps = input_output.sub_buffer(input_output.size() - u)
                              .to_span<uint8_t>();
                for (auto i : range(Ps.size())) {
                    Ps[i] ^= EKYn[i];
                }
            }
        }

        void decrypt_no_auth(const_buffer plaintext,
                             dynamic_buffer ciphertext) {
            auto p = ciphertext.increase_size(plaintext.size());
            std::copy(plaintext.data_as<const uint8_t>(),
                      plaintext.data_as<const uint8_t>() + plaintext.size(), p);
            decrypt_no_auth(buffer(p, plaintext.size()));
        }

    private:
        void update_H() {
            H = compute_H();
        }

        void update_EKY0_() {
            EKY0_ = E(Y0_);
        }

        __m128i compute_H() {
            alignas(16) block_type block0s{};
            cipher_.encrypt(block0s);
            return m128i_utils::load(block0s.data());
        }

        static void incr(iv_storage& counter) noexcept {
            ++counter.u32parts.inc_u;
        }

        __m128i E(const iv_type& Y0) {
            alignas(16) auto Ebuff = Y0_;
            cipher_.encrypt(Ebuff);
            return m128i_utils::load(Ebuff.data());
        }

        __m128i E(const iv_storage& counter) {
            alignas(16) auto Ebuff = counter.Yi;
            cipher_.encrypt(Ebuff);
            return m128i_utils::load(Ebuff.data());
        }

        std::size_t encrypt_with_remaining_iv_counter(
            mutable_buffer unaligned_input_output) noexcept {
            if (remaining_iv_size_ == 0) {
                return 0;
            }
            const std::size_t consume_size =
                std::min(remaining_iv_size_, unaligned_input_output.size());
            auto P = unaligned_input_output.to_span<std::uint8_t>().subspan(
                0, consume_size);
            uint8_t* enc_iv_ptr =
                &remaining_iv_encrypted_[block_length - remaining_iv_size_];
            for (std::size_t i = 0; i < consume_size; ++i) {
                P[i] = P[i] ^ enc_iv_ptr[i];
            }
            remaining_iv_size_ -= consume_size;
            return consume_size;
        }

        void encrypt_aligned(mutable_buffer blocks, size_t n) noexcept {
            auto P = blocks.to_span<__m128i>();
            for (auto i : range(n)) {
                incr(iv_counter_);
                auto EKYi = E(iv_counter_);
                P[i] = m128i_utils::xor128(P[i], EKYi);
            }
        }

        void encrypt_unaligned(mutable_buffer unaligned_blocks,
                               size_t n) noexcept {
            auto P = unaligned_blocks.to_span<unaligned_block_t>();
            for (auto i : range(n)) {
                incr(iv_counter_);
                __m128i EKYi = E(iv_counter_);
                __m128i pi = m128i_utils::load(&P[i]);
                pi = m128i_utils::xor128(pi, EKYi);
                m128i_utils::store(pi, &P[i]);
            }
        }

        void encrypt_blocks(mutable_buffer input_output) noexcept {
            input_output += encrypt_with_remaining_iv_counter(input_output);
            if (input_output.empty()) {
                return;
            }
            const size_t n = input_output.size() / block_length;
            const size_t u = input_output.size() % block_length;
            bool input_is_aligned =
                reinterpret_cast<uintptr_t>(input_output.data()) % 16 == 0;
            if (input_is_aligned) {
                encrypt_aligned(input_output, n);
            }
            else {
                encrypt_unaligned(input_output, n);
            }
            if (u) {
                incr(iv_counter_);
                remaining_iv_encrypted_ = iv_counter_.Yi;
                cipher_.encrypt(remaining_iv_encrypted_.data());
                auto Ps = input_output.sub_buffer(input_output.size() - u)
                              .to_span<uint8_t>();
                for (auto i : range(Ps.size())) {
                    Ps[i] ^= remaining_iv_encrypted_[i];
                }
                remaining_iv_size_ = block_length - u;
            }
        }

        static void make_gcm_auth_error_code(std::error_code& ec) noexcept {
            ec = {1, gcm_category()};
        }

        static void check_and_throw_gcm_error(const std::error_code& ec) {
            if (ec) {
                throw std::system_error(ec);
            }
        }

        alignas(16) __m128i H =
            m128i_utils::zero128(); // the result of E(K, 0128)
        alignas(16) __m128i EKY0_ =
            m128i_utils::zero128();      // the result of E(K, Y0)
        alignas(16) cipher_type cipher_; // the underlying cipher used for
                                         // encryption and decryption
                                         // through its encrypt method
        alignas(16) iv_type Y0_;         // the first iv counter, used
                                         // only for tag generation.
        alignas(16)
            iv_storage iv_counter_; // the iv counter used for encryption
                                    // and decryption, starts from Y0_ + 1
        alignas(16) iv_type remaining_iv_encrypted_;
        std::size_t remaining_iv_size_ = 0;
    };
} // namespace RAD_LIB_NAMESPACE::crypto

#ifdef __GNUC__
#ifdef __clang__
#pragma clang diagnostic pop
#else
#pragma GCC diagnostic pop
#endif // __clang__
#endif // __GNUC__