#pragma once
#include <rad/buffer.h>
#include <rad/crypto/detail/m128.h>
#include <rad/crypto/padding.h>
#include <rad/dynamic_buffer.h>

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
    class alignas(16) ecb_mode {
        using btype =
            std::conditional_t<Cipher::block_length == sizeof(__m128i), __m128i,
                               typename Cipher::block_type>;

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

        ecb_mode() = default;

        ecb_mode(const cipher_type& cipher) noexcept : cipher_{cipher} {
        }

        const cipher_type& get_cipher() const noexcept {
            return cipher_;
        }

        std::size_t encrypt(mutable_buffer input_output,
                            mutable_buffer padding_buffer,
                            std::error_code& ec) noexcept {
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

            cipher_.encrypt(last_block.data());

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
            std::size_t padding_size =
                padding_type::template get_required_padding_size<block_length>(
                    plaintext.size());
            std::size_t output_size = padding_size + plaintext.size();
            auto output =
                buffer(ciphertext.increase_size(output_size), output_size);
            memcpy(output.data(), plaintext.data(), plaintext.size());
            padding_type::template put_padding<block_length>(output,
                                                             plaintext.size());
            encrypt(output);
        }

        std::size_t decrypt(mutable_buffer input_output,
                            std::error_code& ec) noexcept {
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
            std::size_t plaintext_size = decrypt(buffer(input_output), ec);
            if (ec) {
                return;
            }
            assert(plaintext_size <= input_output.size());
            input_output.resize(plaintext_size);
        }

        void decrypt(dynamic_buffer input_output) {
            std::size_t plaintext_size = decrypt(buffer(input_output));
            assert(plaintext_size <= input_output.size());
            input_output.resize(plaintext_size);
        }

        void decrypt(const_buffer ciphertext, dynamic_buffer plaintext,
                     std::error_code& ec) {
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
        void encrypt(mutable_buffer input_output) {
            const bool is_aligned =
                reinterpret_cast<uintptr_t>(input_output.data()) %
                    sizeof(btype) ==
                0;
            if (is_aligned) {
                encrypt_decrypt_aligned<true>(input_output);
            }
            else {
                encrypt_decrypt_unaligned<true>(input_output);
            }
        }

        void decrypt_blocks(mutable_buffer input_output) {
            const bool is_aligned =
                reinterpret_cast<uintptr_t>(input_output.data()) %
                    sizeof(btype) ==
                0;
            if (is_aligned) {
                encrypt_decrypt_aligned<false>(input_output);
            }
            else {
                encrypt_decrypt_unaligned<false>(input_output);
            }
        }

        template <bool Encrypt>
        void encrypt_decrypt_aligned(mutable_buffer input_output)
            requires std::same_as<btype, __m128i>
        {
            auto plaintext = input_output.to_span<btype>().subspan(
                0, input_output.size() / block_length);
            for (auto& block : plaintext) {
                Encrypt ? cipher_.enrypt(block) : cipher_.decrypt(block);
            }
        }

        template <bool Encrypt>
        void encrypt_decrypt_aligned(mutable_buffer input_output)
            requires(!std::same_as<btype, __m128i>)
        {
            auto plaintext = input_output.to_span<btype>().subspan(
                0, input_output.size() / block_length);
            for (auto& block : plaintext) {
                Encrypt ? cipher_.encrypt(block.data())
                        : cipher_.decrypt(block.data());
            }
        }

        template <bool Encrypt>
        void encrypt_decrypt_unaligned(mutable_buffer input_output)
            requires std::same_as<btype, __m128i>
        {
            using unaligned_block_t = std::array<uint8_t, 16>;
            static_assert(sizeof(unaligned_block_t) == 16);
            auto plaintext = input_output.to_span<unaligned_block_t>().subspan(
                0, input_output.size() / block_length);
            for (auto& block : plaintext) {
                __m128i aligned_block = m128i_utils::load(&block);
                Encrypt ? cipher_.enrypt(aligned_block)
                        : cipher_.decrypt(aligned_block);
                m128i_utils::store(aligned_block, &block);
            }
        }

        template <bool Encrypt>
        void encrypt_decrypt_unaligned(mutable_buffer input_output)
            requires(!std::same_as<btype, __m128i>)
        {
            auto plaintext = input_output.to_span<btype>().subspan(
                0, input_output.size() / block_length);
            for (auto& block : plaintext) {
                alignas(16) btype aligned_block;
                std::memcpy(&aligned_block, &block, sizeof(btype));
                Encrypt ? cipher_.enrypt(aligned_block)
                        : cipher_.decrypt(aligned_block);
                std::memcpy(&block, &aligned_block, sizeof(btype));
            }
        }

        alignas(16) cipher_type cipher_;
    };
} // namespace RAD_LIB_NAMESPACE::crypto

#ifdef __GNUC__
#ifdef __clang__
#pragma clang diagnostic pop
#else
#pragma GCC diagnostic pop
#endif // __clang__
#endif // __GNUC__