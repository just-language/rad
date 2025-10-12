#pragma once
#include <rad/buffer.h>
#include <rad/dynamic_buffer.h>

namespace RAD_LIB_NAMESPACE::crypto::detail {
    enum class padding_error_code {
        no_error,
        not_aligned,
        small_padding_buffer,
        invalid_padding,
    };

    struct padding_category_type : public std::error_category {
        virtual const char* name() const noexcept override {
            return "padding";
        }

        virtual std::string message(int code) const override {
            constexpr std::string_view messages[] = {
                "No error",
                "Input buffer is not a multiple of cipher "
                "block "
                "size",
                "Insufficient buffer to store padding",
                "Padding is invalid",
            };

            return std::string(messages[code]);
        }
    };

    inline const padding_category_type padding_category_inst;

    inline void make_padding_error_code(padding_error_code code,
                                        std::error_code& ec) noexcept {
        ec = {static_cast<int>(code), padding_category_inst};
    }
} // namespace RAD_LIB_NAMESPACE::crypto::detail

namespace RAD_LIB_NAMESPACE::crypto {
    struct PKCS7 {
        template <size_t block_length>
        static std::size_t get_required_padding_size(size_t size) noexcept {
            if (size < block_length) {
                return block_length - size;
            }

            if (size == block_length) {
                return block_length;
            }

            auto rem = size % block_length;
            if (!rem) {
                return block_length;
            }

            return block_length - rem;
        }

        /*!
         * @brief add padding to input_output, allocation is
         * made to insert padding as needed
         * @param input_output buffer to add padding to
         */
        template <size_t block_length>
        static void add_padding(dynamic_buffer input_output) {
            size_t old_size = input_output.size();
            size_t padding_size =
                get_required_padding_size<block_length>(old_size);
            input_output.resize(old_size + padding_size,
                                static_cast<uint8_t>(padding_size));
        }

        /*!
         * @brief generate padding for @p input_output and store
         * the last not aligned block (if it is) and padding in
         * @p last_block
         * @param input_output the buffer to generate padding
         * for
         * @param last_block after return it contains the last
         * block in
         * @p input_output if its size isn't equal to
         * block_length, followed by the generated padding
         * @return number of padding bytes written in @p
         * last_block, the number of data before the padding is
         * equal to block_length - padding length, which may be
         * 0 in case of the last block size is equal to
         * block_length
         */
        template <size_t block_length>
        static std::size_t
        add_padding(mutable_buffer input_output,
                    std::array<uint8_t, block_length>& last_block) noexcept {
            size_t padding_size =
                get_required_padding_size<block_length>(input_output.size());
            size_t data_in_last_block = last_block.size() - padding_size;
            if (data_in_last_block) {
                auto last =
                    input_output.data_as<const uint8_t>() + input_output.size();
                auto first = last - data_in_last_block;
                std::copy(first, last, last_block.data());
            }
            std::fill(last_block.begin() + data_in_last_block, last_block.end(),
                      static_cast<uint8_t>(padding_size));
            return padding_size;
        }

        template <size_t block_length>
        static std::size_t put_padding(mutable_buffer input_output,
                                       std::size_t data_size) noexcept {
            assert(input_output.size() > data_size &&
                   (input_output.size() - data_size) <= block_length);
            uint8_t* ptr = input_output.data_as<uint8_t>() + data_size;
            uint8_t padding_value =
                static_cast<uint8_t>(input_output.size() - data_size);
            std::fill(ptr, ptr + padding_value, padding_value);
            return padding_value;
        }

        template <size_t block_length>
        static std::size_t
        get_existing_padding_size(const_buffer input,
                                  std::error_code& ec) noexcept {
            using namespace detail;

            assert(!input.empty());
            uint8_t padding_value =
                *(input.data_as<const uint8_t>() + input.size() - 1);
            if (padding_value > block_length) {
                make_padding_error_code(padding_error_code::invalid_padding,
                                        ec);
                return 0;
            }
            return padding_value;
        }
    };
} // namespace RAD_LIB_NAMESPACE::crypto