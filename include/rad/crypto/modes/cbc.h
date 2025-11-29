#pragma once
#include <rad/buffer.h>
#include <rad/crypto/detail/m128.h>
#include <rad/crypto/padding.h>
#include <rad/dynamic_buffer.h>

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
    template <class Cipher, class Padding = PKCS7>
    class alignas(16) cbc_mode {
        using btype =
            std::conditional_t<Cipher::block_length == sizeof(m128i_type),
                               m128i_type, typename Cipher::block_type>;

    public:
        using cipher_type = Cipher;
        using padding_type = Padding;
        using block_type = typename cipher_type::block_type;
        using iv_type = typename cipher_type::block_type;

        static constexpr std::size_t block_length = cipher_type::block_length;
        static constexpr std::size_t block_size = block_length;
        static constexpr std::size_t iv_size = block_length;
        static constexpr std::size_t max_padding_size = block_length;

        using max_padding_buffer = std::array<uint8_t, max_padding_size>;

        cbc_mode() = default;

        cbc_mode(const cipher_type& cipher) noexcept : cipher_{cipher} {
        }

        cbc_mode(const cipher_type& cipher, const iv_type& nonce) noexcept
            : cipher_{cipher}, iv_{nonce} {
        }

        cbc_mode(const cipher_type& cipher, const_buffer nonce) noexcept
            : cipher_{cipher} {
            set_iv(nonce);
        }

        cbc_mode(const cipher_type& cipher, const_buffer nonce,
                 std::random_device& rd) noexcept
            : cipher_{cipher} {
            set_iv(nonce, rd);
        }

        cbc_mode(const cipher_type& cipher, const_buffer nonce,
                 std::default_random_engine& rng) noexcept
            : cipher_{cipher} {
            set_iv(nonce, rng);
        }

        const cipher_type& get_cipher() const noexcept {
            return cipher_;
        }

        iv_type get_iv() const noexcept {
            if constexpr (std::is_same_v<btype, iv_type>) {
                return iv_;
            }
            else {
                iv_type nonce;
                m128i_utils::store(iv_, nonce.data());
                return nonce;
            }
        }

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
                if constexpr (std::is_same_v<btype, m128i_type>) {
                    iv_ = m128i_utils::load(nonce_span.data());
                }
                else {
                    std::copy(nonce_span.begin(),
                              nonce_span.begin() + iv_.size(), iv_.data());
                }
            }
            else {
                std::uniform_int_distribution<uint16_t> gen(0, 255);
                alignas(16) block_type iv;
                std::copy(nonce_span.begin(), nonce_span.end(), iv.data());
                for (auto i : range(nonce_span.size(), iv.size())) {
                    iv.data()[i] = static_cast<uint8_t>(gen(rng));
                }
                std::memcpy(&iv_, &iv, sizeof(iv));
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

        std::size_t encrypt(mutable_buffer input_output,
                            mutable_buffer padding_buffer,
                            std::error_code& ec) noexcept {
            ec.clear();
            if (input_output.empty()) {
                return 0;
            }
            std::size_t padding_size =
                padding_type::template get_required_padding_size<block_length>(
                    input_output.size());
            if (padding_buffer.size() < padding_size) {
                using namespace detail;
                make_padding_error_code(
                    padding_error_code::small_padding_buffer, ec);
                return 0;
            }
            std::size_t aligned_size =
                input_output.size() - input_output.size() % block_length;
            encrypt(input_output.sub_buffer(0, aligned_size));
            block_type last_block;
            padding_type::template add_padding<block_length>(input_output,
                                                             last_block);
            xor_iv_plaintext(last_block);
            cipher_.encrypt(last_block.data());
            store_iv(iv_, last_block);

            auto last_cipher_block =
                (input_output + aligned_size).to_span<uint8_t>();
            std::copy(last_block.begin(),
                      last_block.begin() + last_cipher_block.size(),
                      last_cipher_block.begin());
            std::copy(last_block.begin() + last_cipher_block.size(),
                      last_block.end(), padding_buffer.data_as<uint8_t>());

            return padding_size;
        }

        std::size_t encrypt(mutable_buffer input_output,
                            mutable_buffer padding_buffer) {
            std::error_code ec;
            encrypt(input_output, padding_buffer, ec);
            if (ec) {
                throw std::runtime_error(ec.message());
            }
        }

        std::size_t encrypt(mutable_buffer input_output,
                            max_padding_buffer& padding_buffer) noexcept {
            std::error_code ec;
            return encrypt(input_output, buffer(padding_buffer), ec);
        }

        void encrypt(std::in_place_t, mutable_buffer input_output,
                     dynamic_buffer padding_buffer) {
            if (input_output.empty()) {
                return;
            }
            std::size_t padding_size =
                padding_type::template get_required_padding_size<block_length>(
                    input_output.size());
            auto padding_buff = buffer(
                padding_buffer.increase_size(padding_size), padding_size);
            encrypt(input_output, padding_buff);
        }

        void encrypt(const_buffer plaintext, dynamic_buffer ciphertext) {
            if (plaintext.empty()) {
                return;
            }
            std::size_t padding_size =
                padding_type::template get_required_padding_size<block_length>(
                    plaintext.size());
            std::size_t output_size = padding_size + plaintext.size();
            auto output =
                buffer(ciphertext.increase_size(output_size), output_size);
            std::memcpy(output.data(), plaintext.data(), plaintext.size());
            padding_type::template put_padding<block_length>(output,
                                                             plaintext.size());
            encrypt(output);
        }

        std::size_t decrypt(mutable_buffer input_output,
                            std::error_code& ec) noexcept {
            ec.clear();
            if (input_output.empty()) {
                return 0;
            }

            if (input_output.size() % block_length != 0) {
                using namespace detail;
                make_padding_error_code(padding_error_code::not_aligned, ec);
                return 0;
            }

            decrypt_blocks(input_output);

            std::size_t existing_padding =
                padding_type::template get_existing_padding_size<block_length>(
                    input_output, ec);
            if (ec) {
                return 0;
            }
            return input_output.size() - existing_padding;
        }

        std::size_t decrypt(mutable_buffer input_output) {
            std::error_code ec;
            std::size_t plaintext_size = decrypt(input_output, ec);
            if (ec) {
                throw std::runtime_error(ec.message());
            }
            return plaintext_size;
        }

        void decrypt(dynamic_buffer input_output, std::error_code& ec) {
            ec.clear();
            std::size_t plaintext_size = decrypt(buffer(input_output), ec);
            if (ec) {
                return;
            }
            input_output.resize(plaintext_size);
        }

        void decrypt(dynamic_buffer input_output) {
            std::size_t plaintext_size = decrypt(buffer(input_output));
            input_output.resize(plaintext_size);
        }

        void decrypt(const_buffer ciphertext, dynamic_buffer plaintext,
                     std::error_code& ec) {
            ec.clear();
            std::size_t old_size = plaintext.size();
            auto output_buff = buffer(
                plaintext.increase_size(ciphertext.size()), ciphertext.size());
            std::memcpy(output_buff.data(), ciphertext.data(),
                        ciphertext.size());
            std::size_t plaintext_size = decrypt(output_buff, ec);
            if (ec) {
                plaintext.resize(old_size);
                return;
            }
            plaintext.resize(old_size + plaintext_size);
        }

        void decrypt(const_buffer ciphertext, dynamic_buffer plaintext) {
            std::size_t old_size = plaintext.size();
            auto output_buff = buffer(
                plaintext.increase_size(ciphertext.size()), ciphertext.size());
            std::memcpy(output_buff.data(), ciphertext.data(),
                        ciphertext.size());
            std::size_t plaintext_size = decrypt(output_buff);
            plaintext.resize(old_size + plaintext_size);
        }

    private:
        static void store_iv(btype& iv, block_type& block) {
            if constexpr (std::is_same_v<btype, m128i_type>) {
                iv = m128i_utils::load(block.data());
            }
            else {
                iv = block;
            }
        }

        void xor_iv_plaintext(block_type& plaintext) const noexcept {
            if constexpr (std::is_same_v<btype, m128i_type>) {
                m128i_type aligned_block = m128i_utils::load(plaintext.data());
                m128i_utils::store(m128i_utils::xor128(aligned_block, iv_),
                                   plaintext.data());
            }
            else {
                for (auto i : range(plaintext.size())) {
                    plaintext[i] ^= iv_[i];
                }
            }
        }

        void encrypt(mutable_buffer input_output) noexcept {
            auto plaintext = input_output.to_span<block_type>().subspan(
                0, input_output.size() / block_length);
            for (auto& block : plaintext) {
                alignas(16) block_type aligned_block = block;
                xor_iv_plaintext(aligned_block);
                cipher_.encrypt(aligned_block.data());
                store_iv(iv_, aligned_block);
                block = aligned_block;
            }
        }

        void decrypt_blocks(mutable_buffer input_output) noexcept {
            auto ciphertext = input_output.to_span<block_type>().subspan(
                0, input_output.size() / block_length);
            for (auto& block : ciphertext) {
                alignas(16) block_type aligned_block = block;
                auto next_iv = block;
                cipher_.decrypt(aligned_block.data());
                xor_iv_plaintext(aligned_block);
                store_iv(iv_, next_iv);
                block = aligned_block;
            }
        }

        alignas(16) cipher_type cipher_;
        alignas(16) btype iv_;
    };
} // namespace RAD_LIB_NAMESPACE::crypto

#ifdef __GNUC__
#ifdef __clang__
#pragma clang diagnostic pop
#else
#pragma GCC diagnostic pop
#endif // __clang__
#endif // __GNUC__