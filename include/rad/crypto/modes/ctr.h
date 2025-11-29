#pragma once
#include <rad/buffer.h>
#include <rad/crypto/detail/m128.h>
#include <rad/dynamic_buffer.h>
#include <rad/libbase.h>

#include <istream>
#include <ostream>
#include <random>

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
    template <class Cipher>
    class alignas(16) ctr_mode {
        using btype =
            std::conditional_t<Cipher::block_length == sizeof(m128i_type),
                               m128i_type, typename Cipher::block_type>;

    public:
        using cipher_type = typename Cipher::encryption_only_cipher;
        static constexpr std::size_t block_length = cipher_type::block_length;
        static constexpr std::size_t block_size = block_length;
        static constexpr std::size_t iv_size = block_length;
        static constexpr std::size_t nonce_size = block_length;
        using iv_type = std::array<uint8_t, iv_size>;

        ctr_mode() = default;

        ctr_mode(cipher_type cipher) noexcept : cipher_{cipher} {
        }

        ctr_mode(const Cipher& cipher) noexcept
            requires(!std::is_same_v<Cipher, cipher_type>)
            : cipher_{cipher.get_encryption_only_cipher()} {
        }

        ctr_mode(cipher_type cipher, const iv_type& iv) noexcept
            : iv_{iv}, cipher_{cipher} {
        }

        ctr_mode(const Cipher& cipher, const iv_type& iv) noexcept
            requires(!std::is_same_v<Cipher, cipher_type>)
            : iv_{iv}, cipher_{cipher.get_encryption_only_cipher()} {
        }

        ctr_mode(cipher_type cipher, const_buffer nonce) noexcept
            : cipher_{cipher} {
            set_iv(nonce);
        }

        ctr_mode(const Cipher& cipher, const_buffer nonce) noexcept
            requires(!std::is_same_v<Cipher, cipher_type>)
            : cipher_{cipher.get_encryption_only_cipher()} {
            set_iv(nonce);
        }

        ctr_mode(cipher_type cipher, const_buffer nonce,
                 std::random_device& rd) noexcept
            : cipher_{cipher} {
            set_iv(nonce, rd);
        }

        ctr_mode(const Cipher& cipher, const_buffer nonce,
                 std::random_device& rd) noexcept
            requires(!std::is_same_v<Cipher, cipher_type>)
            : cipher_{cipher.get_encryption_only_cipher()} {
            set_iv(nonce, rd);
        }

        ctr_mode(cipher_type cipher, const_buffer nonce,
                 std::default_random_engine& rng) noexcept
            : cipher_{cipher} {
            set_iv(nonce, rng);
        }

        ctr_mode(const Cipher& cipher, const_buffer nonce,
                 std::default_random_engine& rng) noexcept
            requires(!std::is_same_v<Cipher, cipher_type>)
            : cipher_{cipher.get_encryption_only_cipher()} {
            set_iv(nonce, rng);
        }

        const cipher_type& get_cipher() const noexcept {
            return cipher_;
        }

        const iv_type& get_iv() const noexcept {
            return iv_;
        }

        /*!
         * @brief Set the stored iv to nonce
         * @param nonce an initialization vector consisting of
         * random bytes
         */
        void set_iv(const iv_type& nonce) noexcept {
            iv_ = nonce;
        }

        /*!
         * @brief Set the stored iv to @p nonce. If @p nonce
         * size equals iv_size then the new iv is the nonce. If
         * @p nonce size is greater than iv_size then the nonce
         * is truncated to iv_size. If @p nonce size is less
         * than iv_size then the nonce is copied to the start of
         * the iv and the seeded random numbers generator @p rng
         * is used to generate the rest of the iv
         * @param nonce an initialization vector consisting of
         * random bytes
         * @param rng a seeded random numbers generator to be
         * used if @p nonce size is less than iv_size
         */
        void set_iv(const_buffer nonce,
                    std::default_random_engine& rng) noexcept {
            auto nonce_span = nonce.to_span<uint8_t>();

            if (nonce.size() >= iv_size) {
                std::copy(nonce_span.begin(), nonce_span.begin() + iv_.size(),
                          iv_.data());
            }
            else {
                std::uniform_int_distribution<uint32_t> gen(0, 255);
                std::copy(nonce_span.begin(), nonce_span.end(), iv_.data());
                for (auto i : range(nonce_span.size(), iv_.size())) {
                    iv_.data()[i] = static_cast<uint8_t>(gen(rng));
                }
            }
        }

        /*!
         * @brief Set the stored iv to @p nonce. If @p nonce
         * size equals iv_size then the new iv is the nonce. If
         * @p nonce size is greater than iv_size then the nonce
         * is truncated to iv_size. If @p nonce size is less
         * than iv_size then the nonce is copied to the start of
         * the iv and the random device @p rd is used to seed a
         * random numbers generator to generate the rest of the
         * iv
         * @param nonce an initialization vector consisting of
         * random bytes
         * @param rd random device to seed a random numbers
         * generator to be used if @p nonce size is less than
         * iv_size
         */
        void set_iv(const_buffer nonce, std::random_device& rd) noexcept {
            std::default_random_engine rng;
            if (nonce.size() < iv_size) {
                rng.seed(rd());
            }
            set_iv(nonce, rng);
        }

        /*!
         * @brief Set the stored iv to @p nonce. If @p nonce
         * size equals iv_size then the new iv is the nonce. If
         * @p nonce size is greater than iv_size then the nonce
         * is truncated to iv_size. If @p nonce size is less
         * than iv_size then the nonce is copied to the start of
         * the iv and a random numbers generator is used to
         * generate the rest of the iv
         * @param nonce an initialization vector consisting of
         * random bytes
         */
        void set_iv(const_buffer nonce) noexcept {
            std::random_device rd;
            set_iv(nonce, rd);
        }

        /*!
         * @brief Encrypts input plaintext in place using the
         * cipher encrypt method and stored iv in CTR mode, the
         * stored iv will be incremented by the number of blocks
         * in plaintext
         * @param[in,out] input_output on input, the plaintext
         * buffer to encrypt, on output, the encrypted
         * ciphertext
         */
        void encrypt(mutable_buffer input_output) noexcept {
            encrypt_blocks(input_output);
        }

        /*!
         * @brief encrypts input plaintext using the cipher
         * encrypt method and stored iv in CTR mode, and append
         * the encrypted ciphertext to output. the stored iv
         * will be incremented by the number of blocks in
         * plaintext
         * @param[in] input the buffer containing plaintext to
         * encrypt
         * @param[out] output the buffer to append the encrypted
         * ciphertext to
         */
        void encrypt(const_buffer input, dynamic_buffer output) {
            if (input.empty()) {
                return;
            }
            size_t old_size = output.size();
            output.insert(input.data(), input.size());
            encrypt(buffer(output) + old_size);
        }

        void encrypt(std::istream& is, std::ostream& os,
                     mutable_buffer read_buffer) {
            std::array<char, block_length> stack_buffer;
            std::size_t read_size = read_buffer.size();
            char* read_ptr = read_buffer.data_as<char>();

            if (read_size < block_length) {
                read_size = block_length;
                read_ptr = stack_buffer.data();
            }
            else if (read_size % block_length) {
                read_size -= read_size % block_length;
            }

            while (!is.eof()) {
                is.read(read_ptr, read_size);
                std::size_t n = static_cast<std::size_t>(is.gcount());
                if (!n) {
                    break;
                }

                auto ciphertext = buffer(read_ptr, n);
                encrypt(ciphertext);
                os.write(read_ptr, n);

                if (n < read_size) {
                    break;
                }
            }
        }

        void encrypt(std::istream& is, std::ostream& os,
                     std::size_t read_size = 0) {
            constexpr std::size_t default_size = 16 * 1024;
            if (!read_size) {
                read_size = default_size;
            }
            else if (read_size < block_length) {
                read_size = block_length;
            }
            else if (read_size % block_length) {
                read_size += block_length - read_size % block_length;
            }

            std::vector<uint8_t> read_buffer(read_size);
            encrypt(is, os, buffer(read_buffer));
        }

        template <std::size_t N>
        void encrypt(std::istream& is, std::ostream& os) {
            constexpr std::size_t default_size = 16 * 1024;
            constexpr std::size_t aligned_size =
                !N ? default_size
                   : (N < block_length     ? block_length
                      : (N % block_length) ? N + block_length - N % block_length
                                           : N);

            std::array<uint8_t, aligned_size> read_buffer;
            encrypt(is, os, buffer(read_buffer));
        }

        /*!
         * @brief decrypts ciphertext using the cipher encrypt
         * method and stored iv in CTR mode, the stored iv will
         * be incremented by the number of blocks in plaintext
         * @param[in,out] ciphertext on input, the buffer to
         * decrypt, on output, the decrypted plaintext
         * @note CTR decrypt is the same as encrypt
         */
        void decrypt(mutable_buffer ciphertext) {
            encrypt(ciphertext);
        }

        /*!
         * @brief Decrypts input plaintext using the cipher
         * encrypt method and stored iv in CTR mode, and append
         * the encrypted ciphertext to output. the stored iv
         * will be incremented by the number of blocks in
         * plaintext
         * @param[in] input the buffer containing plaintext to
         * encrypt
         * @param[out] output the buffer to append the encrypted
         * ciphertext to
         */
        void decrypt(const_buffer input, dynamic_buffer output) {
            encrypt(input, output);
        }

        void decrypt(std::istream& is, std::ostream& os,
                     mutable_buffer read_buffer) {
            encrypt(is, os, read_buffer);
        }

        void decrypt(std::istream& is, std::ostream& os,
                     std::size_t read_size = 0) {
            encrypt(is, os, read_size);
        }

        template <std::size_t N>
        void decrypt(std::istream& is, std::ostream& os) {
            encrypt<N>(is, os);
        }

    private:
        btype encrypt_counter(const iv_type& iv) {
            if constexpr (std::is_same_v<btype, m128i_type>) {
                btype enc = m128i_utils::load(iv.data());
                cipher_.enrypt_m128i(enc);
                return enc;
            }
            else {
                alignas(16) auto ebuff = iv;
                cipher_.encrypt(ebuff);
                return m128i_utils::load(ebuff.data());
            }
        }

        void encrypt_blocks(mutable_buffer input_output) noexcept {
            if (input_output.empty()) {
                return;
            }

            if (last_block_size_) {
                std::size_t block_size =
                    min(block_length - last_block_size_, input_output.size());
                auto block =
                    input_output.sub_buffer(0, block_size).to_span<uint8_t>();

                alignas(16) auto enc_buff = iv_;
                cipher_.encrypt(enc_buff);
                auto keystream = (buffer(enc_buff) + last_block_size_)
                                     .template to_span<uint8_t>();
                for (auto i : range(block.size())) {
                    block[i] ^= keystream[i];
                }

                last_block_size_ -= block_size;
                input_output += block_size;

                if (!last_block_size_) {
                    cipher_type::ctr_increment(iv_);
                }
            }

            if (input_output.empty()) {
                return;
            }

            const bool is_aligned =
                reinterpret_cast<uintptr_t>(input_output.data()) % 16 == 0;
            if (!std::is_same_v<btype, m128i_type> || !is_aligned) {
                auto blocks = input_output.to_span<iv_type>().subspan(
                    0, input_output.size() / block_length);
                encrypt_unaligned_blocks(blocks);
            }
            else {
                auto blocks = input_output.to_span<btype>().subspan(
                    0, input_output.size() / block_length);
                encrypt_aligned_blocks(blocks);
            }

            if (input_output.size() % block_length == 0) {
                return;
            }
            auto last_block =
                input_output
                    .sub_buffer(input_output.size() -
                                input_output.size() % block_length)
                    .to_span<uint8_t>();

            alignas(16) auto ebuff = iv_;
            cipher_.encrypt(ebuff);
            xor_last_block(last_block, ebuff);

            last_block_size_ = last_block.size();
        }

        void encrypt_aligned_blocks(std::span<btype> blocks)
            requires(std::is_same_v<btype, m128i_type>)
        {
            for (auto& block : blocks) {
                block = m128i_utils::xor128(block, encrypt_counter(iv_));
                cipher_type::ctr_increment(iv_);
            }
        }

        void encrypt_unaligned_blocks(std::span<iv_type> blocks)
            requires(std::is_same_v<btype, m128i_type>)
        {
            for (auto& block : blocks) {
                m128i_type aligned_block = m128i_utils::load(block.data());
                aligned_block =
                    m128i_utils::xor128(aligned_block, encrypt_counter(iv_));
                m128i_utils::store(aligned_block, block.data());
                cipher_type::ctr_increment(iv_);
            }
        }

        void encrypt_unaligned_blocks(std::span<iv_type> blocks)
            requires(!std::is_same_v<btype, m128i_type>)
        {
            for (auto& block : blocks) {
                xor_bytes(block, encrypt_counter(iv_));
                cipher_type::ctr_increment(iv_);
            }
        }

        static void xor_bytes(iv_type& out, const iv_type& with) noexcept {
            for (auto i : range(out.size())) {
                out[i] ^= with[i];
            }
        }

        template <std::size_t Extent>
        void xor_last_block(std::span<uint8_t, Extent> block,
                            const iv_type& counter) {
            for (auto i : range(block.size())) {
                block[i] ^= counter[i];
            }
        }

        alignas(16) iv_type iv_; // the counter which is incremented after
                                 // each block encryption or decryption
        alignas(16) cipher_type cipher_;
        std::size_t last_block_size_ =
            0; // bytes consumed from the last partial block
    };
} // namespace RAD_LIB_NAMESPACE::crypto

#ifdef __GNUC__
#ifdef __clang__
#pragma clang diagnostic pop
#else
#pragma GCC diagnostic pop
#endif // __clang__
#endif // __GNUC__