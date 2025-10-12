#pragma once
#include <rad/dynamic_buffer.h>
#include <rad/libbase.h>

#include <array>
#include <cassert>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace RAD_LIB_NAMESPACE::detail {
    template <class PodType>
    constexpr bool check_pod = std::is_trivially_constructible_v<PodType> &&
                               std::is_trivially_copyable_v<PodType>;
}

namespace RAD_LIB_NAMESPACE {
    /*!
     * @brief Holds a buffer that cannot be modified.
     *
     * The const_buffer class provides a safe representation of a buffer that
     * cannot be modified. It does not own the underlying data, and so is cheap
     * to copy or assign.
     *
     * The contents of a buffer may be accessed using the data(), data_as() and
     * size() member functions.
     *
     * The data() and data_as() methods permit violations of type safety, so
     * uses of them in application code should be carefully considered.
     */
    class const_buffer {
    public:
        /*!
         * @brief The size type used by the buffer.
         */
        using size_type = std::size_t;

        /*!
         * @brief Construct an empty buffer.
         */
        constexpr const_buffer() = default;

        /*!
         * @brief Construct a buffer to represent a given memory range.
         * @param data Pointer to the memory buffer.
         * @param size The count of bytes in the buffer pointed to by @p data.
         */
        constexpr const_buffer(const void* data, size_type size) noexcept
            : data_{data}, size_{size} {
        }

        bool operator==(const const_buffer& other) const noexcept {
            return size_ == other.size_ &&
                   (!size_ ||
                    to_string_view<char>() == other.to_string_view<char>());
        }

        /*!
         * @brief Get a pointer to the beginning of the memory range.
         * @return A pointer to the beginning of the memory range.
         */
        constexpr const void* data() const noexcept {
            return data_;
        }

        /*!
         * @brief Get a pointer to the beginning of the memory range casted to
         * T. This function is very unsafe and should be used with great care.
         * @tparam T The desired type.
         * @return A pointer to the beginning of the memory range casted to T.
         */
        template <class T>
        T* data_as() const noexcept {
            return reinterpret_cast<T*>(const_cast<void*>(data()));
        }

        /*!
         * @brief Get the size of the memory range in bytes.
         * @return he size of the memory range in bytes.
         */
        constexpr size_type size() const noexcept {
            return size_;
        }

        /*!
         * @brief Check if the buffer memory range is empty.
         * @return True if the buffer memory range is empty, otherwise false.
         */
        constexpr bool empty() const noexcept {
            return size() == 0;
        }

        /*!
         * @brief Make a `std::vector` from the memory buffer.
         * This function is very unsafe and should be used with great care.
         * @tparam T The vector value type.
         * @tparam Allocator The vector allocator type.
         * @return The `std::vector` created from the memory buffer.
         */
        template <class T, class Allocator = std::allocator<T>>
        std::vector<T, Allocator> to_vector() const {
            auto first = data_as<const T>();
            auto last = first + (size() / sizeof(T));
            return std::vector<T, Allocator>{first, last};
        }

        /*!
         * @brief Insert the memory buffer into a `std::vector`.
         * This function is very unsafe and should be used with great care.
         * @tparam T The vector value type.
         * @tparam Allocator The vector allocator type.
         * @param The `std::vector` to insert into from the memory buffer.
         */
        template <class T, class Allocator>
        void to_vector(std::vector<T, Allocator>& vec) const {
            auto first = data_as<const T>();
            auto last = first + (size() / sizeof(T));
            vec.insert(vec.end(), first, last);
        }

        /*!
         * @brief Make a `std::basic_string` from the memory buffer.
         * This function is very unsafe and should be used with great care.
         * @tparam CharT The string value type.
         * @tparam Traits The string char traits.
         * @tparam Allocator The allocator type of the string.
         * @return The `std::basic_string` created from the memory buffer.
         */
        template <class CharT, class Traits = std::char_traits<CharT>,
                  class Allocator = std::allocator<CharT>>
        std::basic_string<CharT, Traits, Allocator> to_string() const {
            auto first = data_as<const CharT>();
            auto last = first + (size() / sizeof(CharT));
            return std::basic_string<CharT, Traits, Allocator>{first, last};
        }

        /*!
         * @brief Insert the memory buffer into a `std::basic_string`.
         * This function is very unsafe and should be used with great care.
         * @tparam CharT The string value type.
         * @tparam Traits The string char traits.
         * @tparam Allocator The allocator type of the string.
         * @param str The `std::basic_string` to insert into from the memory
         * buffer.
         */
        template <class CharT, class Traits, class Allocator>
        void to_string(std::basic_string<CharT, Traits, Allocator>& str) const {
            auto first = data_as<const CharT>();
            auto last = first + (size() / sizeof(CharT));
            str.insert(str.end(), first, last);
        }

        /*!
         * @brief Make a `std::string` from the memory buffer.
         * This function is very unsafe and should be used with great care.
         * @return The `std::string` created from the memory buffer.
         */
        std::string to_string() const {
            return to_string<char>();
        }

        /*!
         * @brief Make a `std::basic_string_view` from the memory buffer.
         * This function is very unsafe and should be used with great care.
         * @tparam CharT The string value type.
         * @tparam Traits The string char traits.
         * @return The `std::basic_string_view` created from the memory buffer.
         */
        template <class CharT, class Traits = std::char_traits<CharT>>
        std::basic_string_view<CharT, Traits> to_string_view() const noexcept {
            auto ptr = data_as<const CharT>();
            return {ptr, size() / sizeof(CharT)};
        }

        /*!
         * @brief Assign a `std::basic_string_view` to the memory buffer.
         * This function is very unsafe and should be used with great care.
         * @tparam CharT The string value type.
         * @tparam Traits The string char traits.
         * @param str The `std::basic_string_view` assigned to the memory
         * buffer.
         */
        template <class CharT, class Traits>
        void to_string_view(
            std::basic_string_view<CharT, Traits>& str) const noexcept {
            auto ptr = data_as<const CharT>();
            str = {ptr, size() / sizeof(CharT)};
        }

        /*!
         * @brief Make a `std::string_view` from the memory buffer.
         * This function is very unsafe and should be used with great care.
         * @return The `std::string_view` created from the memory buffer.
         */
        std::string_view to_string_view() const noexcept {
            return to_string_view<char>();
        }

        /*!
         * @brief Make a `std::span` from the memory buffer.
         * This function is very unsafe and should be used with great care.
         * @tparam T The span value type.
         * @return The `std::span` created from the memory buffer.
         */
        template <class T>
        std::span<T> to_span() const noexcept {
            auto ptr = data_as<T>();
            return {ptr, size() / sizeof(T)};
        }

        /*!
         * @brief Make a `std::span` from part of the memory buffer.
         * This function is very unsafe and should be used with great care.
         * @tparam T The span value type.
         * @param n The count of span elements to include in the span view.
         * If @p n is greater than buffer size * sizeof(T), it will be
         * truncated.
         * @return The `std::span` created from part of the memory buffer.
         */
        template <class T>
        std::span<T> to_span(size_type n) const noexcept {
            n = std::min(n, size() / sizeof(T));
            auto ptr = data_as<T>();
            return {ptr, n};
        }

        /*!
         * @brief Make a `std::span` from the memory buffer with constant
         * extent. This function is very unsafe and should be used with great
         * care.
         * @tparam T The span value type.
         * @tparam Extent The span extent value.
         * This value must not be greater than buffer size / sizeof(T).
         * @return The `std::span` created from part of the memory buffer.
         */
        template <class T, std::size_t Extent>
        std::span<T, Extent> to_span() const noexcept {
            assert(Extent * sizeof(T) <= size());
            auto ptr = data_as<T>();
            return std::span<T, Extent>{ptr, Extent};
        }

        /*!
         * @brief Move the start of the buffer by the specified number of bytes.
         * @param n The count of bytes to move the start of the buffer by.
         * This count must be less than or equal to the size of the buffer.
         * @return Reference to self.
         */
        const_buffer& operator+=(size_type n) noexcept {
            assert(n <= size());
            data_ = reinterpret_cast<const uint8_t*>(data()) + n;
            size_ -= n;
            return *this;
        }

        /*!
         * @brief Get a copy buffer whose start is moved by the specified number
         * of bytes.
         * @param n The count of bytes to move the start of the buffer by.
         * This count must be less than or equal to the size of the buffer.
         * @return The copied buffer.
         */
        const_buffer operator+(size_type n) noexcept {
            assert(n <= size());
            auto data_ptr = reinterpret_cast<const uint8_t*>(data()) + n;
            auto data_size = size() - n;
            return const_buffer(data_ptr, data_size);
        }

        /*!
         * @brief Move the start of the buffer by 1 byte.
         * The size of the buffer must be greater than or equal to 1 byte.
         * @return Reference to self.
         */
        const_buffer& operator++() noexcept {
            assert(size() >= 1);
            data_ = reinterpret_cast<const uint8_t*>(data()) + 1;
            --size_;
            return *this;
        }

        /*!
         * @brief Move the start of the buffer by 1 byte.
         * The size of the buffer must be greater than or equal to 1 byte.
         * @return Copy of the original buffer before move.
         */
        const_buffer operator++(int) noexcept {
            assert(size() >= 1);
            auto saved(*this);
            ++(*this);
            return saved;
        }

        /*!
         * @brief Decrease the end of the buffer by the specified number of
         * bytes.
         * @param n The count of bytes to decrease the end of the buffer by.
         * This count must be less than or equal to the size of the buffer.
         * @return Reference to self.
         */
        const_buffer& operator-=(size_type n) noexcept {
            assert(n <= size());
            size_ -= n;
            return *this;
        }

        /*!
         * @brief Get a copy buffer whose end is decreased by the specified
         * number of bytes.
         * @param n The count of bytes to decrease the end of the buffer by.
         * This count must be less than or equal to the size of the buffer.
         * @return The copied buffer.
         */
        const_buffer operator-(size_type n) noexcept {
            assert(n <= size());
            auto data_size = size() - n;
            return const_buffer(data(), data_size);
        }

        /*!
         * @brief Decrease the end of the buffer by 1 byte.
         * The size of the buffer must be greater than or equal to 1 byte.
         * @return Reference to self.
         */
        const_buffer& operator--() noexcept {
            assert(size() >= 1);
            --size_;
            return *this;
        }

        /*!
         * @brief Decrease the end of the buffer by 1 byte.
         * The size of the buffer must be greater than or equal to 1 byte.
         * @return Copy of the original buffer before decrease.
         */
        const_buffer operator--(int) noexcept {
            assert(size() >= 1);
            auto saved(*this);
            --(*this);
            return saved;
        }

        /*!
         * @brief Get a sub buffer starting from @p pos to the end of the
         * buffer. The buffer size can't be less than @p pos.
         * @param pos The position of the first byte of the sub buffer.
         * @return The sub buffer.
         */
        const_buffer sub_buffer(size_type pos) const noexcept {
            assert(pos <= size());
            auto len = size() - pos;
            return const_buffer(data_as<const uint8_t>() + pos, len);
        }

        /*!
         * @brief Get a sub buffer starting from @p pos with length @p len.
         * The buffer size can't be less than @p pos.
         * If @p len is greater than size() - @p pos, it will be truncated.
         * @param pos The position of the first byte of the sub buffer.
         * @param len The length of the sub buffer.
         * @return The sub buffer.
         */
        const_buffer sub_buffer(size_type pos, size_type len) const noexcept {
            assert(pos <= size());
            auto len2 = size() - pos;
            if (len > len2) {
                len = len2;
            }
            return const_buffer(data_as<const uint8_t>() + pos, len);
        }

    private:
        const void* data_ = nullptr;
        size_type size_ = 0;
    };

    /*!
     * @brief Holds a buffer that can be modified.
     *
     * The mutable_buffer class provides a safe representation of a buffer that
     * can be modified. It does not own the underlying data, and so is cheap to
     * copy or assign.
     *
     * The contents of a buffer may be accessed using the data(), data_as() and
     * size() member functions.
     *
     * The data() and data_as() methods permit violations of type safety, so
     * uses of them in application code should be carefully considered.
     */
    class mutable_buffer {
    public:
        /*!
         * @brief The size type used by the buffer.
         */
        using size_type = std::size_t;

        /*!
         * @brief Construct an empty buffer.
         */
        constexpr mutable_buffer() = default;

        /*!
         * @brief Construct a buffer to represent a given memory range.
         * @param data Pointer to the memory buffer.
         * @param size The count of bytes in the buffer pointed to by @p data.
         */
        constexpr mutable_buffer(void* data, size_type size) noexcept
            : data_{data}, size_{size} {
        }

        bool operator==(const mutable_buffer& other) const noexcept {
            return size_ == other.size_ &&
                   (!size_ ||
                    to_string_view<char>() == other.to_string_view<char>());
        }

        /*!
         * @brief Get a pointer to the beginning of the memory range.
         * @return A pointer to the beginning of the memory range.
         */
        constexpr void* data() const noexcept {
            return data_;
        }

        /*!
         * @brief Get a pointer to the beginning of the memory range casted to
         * T. This function is very unsafe and should be used with great care.
         * @tparam T The desired type.
         * @return A pointer to the beginning of the memory range casted to T.
         */
        template <class T>
        T* data_as() const noexcept {
            return reinterpret_cast<T*>(const_cast<void*>(data()));
        }

        /*!
         * @brief Get the size of the memory range in bytes.
         * @return he size of the memory range in bytes.
         */
        constexpr size_type size() const noexcept {
            return size_;
        }

        /*!
         * @brief Check if the buffer memory range is empty.
         * @return True if the buffer memory range is empty, otherwise false.
         */
        constexpr bool empty() const noexcept {
            return size() == 0;
        }

        /*!
         * @brief Make a `std::vector` from the memory buffer.
         * This function is very unsafe and should be used with great care.
         * @tparam T The vector value type.
         * @tparam Allocator The vector allocator type.
         * @return The `std::vector` created from the memory buffer.
         */
        template <class T, class Allocator = std::allocator<T>>
        std::vector<T, Allocator> to_vector() const {
            auto first = data_as<const T>();
            auto last = first + (size() / sizeof(T));
            return std::vector<T, Allocator>{first, last};
        }

        /*!
         * @brief Insert the memory buffer into a `std::vector`.
         * This function is very unsafe and should be used with great care.
         * @tparam T The vector value type.
         * @tparam Allocator The vector allocator type.
         * @param The `std::vector` to insert into from the memory buffer.
         */
        template <class T, class Allocator>
        void to_vector(std::vector<T, Allocator>& vec) const {
            auto first = data_as<const T>();
            auto last = first + (size() / sizeof(T));
            vec.insert(vec.end(), first, last);
        }

        /*!
         * @brief Make a `std::basic_string` from the memory buffer.
         * This function is very unsafe and should be used with great care.
         * @tparam CharT The string value type.
         * @tparam Traits The string char traits.
         * @tparam Allocator The allocator type of the string.
         * @return The `std::basic_string` created from the memory buffer.
         */
        template <class CharT, class Traits = std::char_traits<CharT>,
                  class Allocator = std::allocator<CharT>>
        std::basic_string<CharT, Traits, Allocator> to_string() const {
            auto first = data_as<const CharT>();
            auto last = first + (size() / sizeof(CharT));
            return std::basic_string<CharT, Traits, Allocator>{first, last};
        }

        /*!
         * @brief Insert the memory buffer into a `std::basic_string`.
         * This function is very unsafe and should be used with great care.
         * @tparam CharT The string value type.
         * @tparam Traits The string char traits.
         * @tparam Allocator The allocator type of the string.
         * @param str The `std::basic_string` to insert into from the memory
         * buffer.
         */
        template <class CharT, class Traits, class Allocator>
        void to_string(std::basic_string<CharT, Traits, Allocator>& str) const {
            auto first = data_as<const CharT>();
            auto last = first + (size() / sizeof(CharT));
            str.insert(str.end(), first, last);
        }

        /*!
         * @brief Make a `std::string` from the memory buffer.
         * This function is very unsafe and should be used with great care.
         * @return The `std::string` created from the memory buffer.
         */
        std::string to_string() const {
            return to_string<char>();
        }

        /*!
         * @brief Make a `std::basic_string_view` from the memory buffer.
         * This function is very unsafe and should be used with great care.
         * @tparam CharT The string value type.
         * @tparam Traits The string char traits.
         * @return The `std::basic_string_view` created from the memory buffer.
         */
        template <class CharT, class Traits = std::char_traits<CharT>>
        std::basic_string_view<CharT, Traits> to_string_view() const noexcept {
            auto ptr = data_as<const CharT>();
            return {ptr, size() / sizeof(CharT)};
        }

        /*!
         * @brief Assign a `std::basic_string_view` to the memory buffer.
         * This function is very unsafe and should be used with great care.
         * @tparam CharT The string value type.
         * @tparam Traits The string char traits.
         * @param str The `std::basic_string_view` assigned to the memory
         * buffer.
         */
        template <class CharT, class Traits>
        void to_string_view(
            std::basic_string_view<CharT, Traits>& str) const noexcept {
            auto ptr = data_as<const CharT>();
            str = {ptr, (size() / sizeof(CharT))};
        }

        /*!
         * @brief Make a `std::string_view` from the memory buffer.
         * This function is very unsafe and should be used with great care.
         * @return The `std::string_view` created from the memory buffer.
         */
        std::string_view to_string_view() const noexcept {
            return to_string_view<char>();
        }

        /*!
         * @brief Make a `std::span` from the memory buffer.
         * This function is very unsafe and should be used with great care.
         * @tparam T The span value type.
         * @return The `std::span` created from the memory buffer.
         */
        template <class T>
        std::span<T> to_span() const noexcept {
            auto ptr = data_as<T>();
            return {ptr, (size() / sizeof(T))};
        }

        /*!
         * @brief Make a `std::span` from part of the memory buffer.
         * This function is very unsafe and should be used with great care.
         * @tparam T The span value type.
         * @param n The count of span elements to include in the span view.
         * If @p n is greater than buffer size * sizeof(T), it will be
         * truncated.
         * @return The `std::span` created from part of the memory buffer.
         */
        template <class T>
        std::span<T> to_span(size_type n) const noexcept {
            n = std::min(n, size() / sizeof(T));
            auto ptr = data_as<T>();
            return {ptr, n};
        }

        /*!
         * @brief Make a `std::span` from the memory buffer with constant
         * extent. This function is very unsafe and should be used with great
         * care.
         * @tparam T The span value type.
         * @tparam Extent The span extent value.
         * This value must not be greater than buffer size / sizeof(T).
         * @return The `std::span` created from part of the memory buffer.
         */
        template <class T, std::size_t Extent>
        std::span<T, Extent> to_span() const noexcept {
            assert(Extent * sizeof(T) <= size());
            auto ptr = data_as<T>();
            return std::span<T, Extent>{ptr, Extent};
        }

        /*!
         * @brief Convert operator to const_buffer.
         */
        constexpr operator const_buffer() const noexcept {
            return const_buffer{data_, size_};
        }

        /*!
         * @brief Move the start of the buffer by the specified number of bytes.
         * @param n The count of bytes to move the start of the buffer by.
         * This count must be less than or equal to the size of the buffer.
         * @return Reference to self.
         */
        mutable_buffer& operator+=(size_type n) noexcept {
            assert(n <= size());
            data_ = reinterpret_cast<uint8_t*>(data()) + n;
            size_ -= n;
            return *this;
        }

        /*!
         * @brief Get a copy buffer whose start is moved by the specified number
         * of bytes.
         * @param n The count of bytes to move the start of the buffer by.
         * This count must be less than or equal to the size of the buffer.
         * @return The copied buffer.
         */
        mutable_buffer operator+(size_type n) noexcept {
            assert(n <= size());
            auto data_ptr = reinterpret_cast<uint8_t*>(data()) + n;
            auto data_size = size() - n;
            return mutable_buffer(data_ptr, data_size);
        }

        /*!
         * @brief Move the start of the buffer by 1 byte.
         * The size of the buffer must be greater than or equal to 1 byte.
         * @return Reference to self.
         */
        mutable_buffer& operator++() noexcept {
            assert(size() >= 1);
            data_ = reinterpret_cast<uint8_t*>(data()) + 1;
            --size_;
            return *this;
        }

        /*!
         * @brief Move the start of the buffer by 1 byte.
         * The size of the buffer must be greater than or equal to 1 byte.
         * @return Copy of the original buffer before move.
         */
        mutable_buffer operator++(int) noexcept {
            assert(size() >= 1);
            auto saved(*this);
            ++(*this);
            return saved;
        }

        /*!
         * @brief Decrease the end of the buffer by the specified number of
         * bytes.
         * @param n The count of bytes to decrease the end of the buffer by.
         * This count must be less than or equal to the size of the buffer.
         * @return Reference to self.
         */
        mutable_buffer& operator-=(size_type n) noexcept {
            assert(n <= size());
            size_ -= n;
            return *this;
        }

        /*!
         * @brief Get a copy buffer whose end is decreased by the specified
         * number of bytes.
         * @param n The count of bytes to decrease the end of the buffer by.
         * This count must be less than or equal to the size of the buffer.
         * @return The copied buffer.
         */
        mutable_buffer operator-(size_type n) noexcept {
            assert(n <= size());
            auto data_size = size() - n;
            return mutable_buffer(data(), data_size);
        }

        /*!
         * @brief Decrease the end of the buffer by 1 byte.
         * The size of the buffer must be greater than or equal to 1 byte.
         * @return Reference to self.
         */
        mutable_buffer& operator--() noexcept {
            assert(size() >= 1);
            --size_;
            return *this;
        }

        /*!
         * @brief Decrease the end of the buffer by 1 byte.
         * The size of the buffer must be greater than or equal to 1 byte.
         * @return Copy of the original buffer before decrease.
         */
        mutable_buffer operator--(int) noexcept {
            assert(size() >= 1);
            auto saved(*this);
            --(*this);
            return saved;
        }

        /*!
         * @brief Get a sub buffer starting from @p pos to the end of the
         * buffer. The buffer size can't be less than @p pos.
         * @param pos The position of the first byte of the sub buffer.
         * @return The sub buffer.
         */
        mutable_buffer sub_buffer(size_type pos) const noexcept {
            assert(pos <= size());
            auto len = size() - pos;
            return mutable_buffer(data_as<uint8_t>() + pos, len);
        }

        /*!
         * @brief Get a sub buffer starting from @p pos with length @p len.
         * The buffer size can't be less than @p pos.
         * If @p len is greater than size() - @p pos, it will be truncated.
         * @param pos The position of the first byte of the sub buffer.
         * @param len The length of the sub buffer.
         * @return The sub buffer.
         */
        mutable_buffer sub_buffer(size_type pos, size_type len) const noexcept {
            assert(pos <= size());
            auto len2 = size() - pos;
            if (len > len2) {
                len = len2;
            }
            return mutable_buffer(data_as<uint8_t>() + pos, len);
        }

    private:
        void* data_ = nullptr;
        size_type size_ = 0;
    };

    // from raw buffers

    /*!
     * @brief Create a new modifiable buffer that represents the given
     * memory range.
     * @param data Pointer to the memory buffer.
     * @param size The count of bytes in the buffer pointed to by @p data.
     * @return mutable_buffer(data, size).
     */
    inline constexpr mutable_buffer buffer(void* data,
                                           std::size_t size) noexcept {
        return mutable_buffer{data, size};
    }

    /*!
     * @brief Create a new non-modifiable buffer that represents the given
     * memory range.
     * @param data Pointer to the memory buffer.
     * @param size The count of bytes in the buffer pointed to by @p data.
     * @return const_buffer(data, size).
     */
    inline constexpr const_buffer buffer(const void* data,
                                         std::size_t size) noexcept {
        return const_buffer{data, size};
    }

    /*!
     * @brief Create an empty modifiable buffer.
     * @return An empty modifiable buffer.
     */
    inline constexpr mutable_buffer buffer(std::nullptr_t,
                                           std::size_t) noexcept {
        return mutable_buffer{nullptr, 0};
    }

    /*!
     * @brief Create an empty modifiable buffer.
     * @return An empty modifiable buffer.
     */
    inline constexpr mutable_buffer buffer(std::nullptr_t) noexcept {
        return mutable_buffer{nullptr, 0};
    }

    /*!
     * @brief Create a new non-modifiable buffer from an existing buffer.
     * @param buff The existing non-modifiable buffer.
     * @return The new non-modifiable buffer copy.
     */
    inline constexpr const_buffer buffer(const const_buffer& buff) noexcept {
        return buff;
    }

    /*!
     * @brief Create a new modifiable buffer from an existing buffer.
     * @param buff The existing modifiable buffer.
     * @return The new modifiable buffer copy.
     */
    inline constexpr mutable_buffer
    buffer(const mutable_buffer& buff) noexcept {
        return buff;
    }

    // from c-array

    /*!
     * @brief Create a new modifiable buffer that represents the given POD
     * array.
     * @tparam PodType The array item type.
     * @tparam N The count of array items.
     * @param arr The array.
     * @return The new modifiable buffer that represents the given POD array.
     * The size of the returned buffer is `N * sizeof(PodType)`.
     */
    template <class PodType, std::size_t N>
    inline constexpr mutable_buffer buffer(PodType (&arr)[N]) noexcept {
        static_assert(detail::check_pod<PodType>,
                      "the array must be of pod type");
        return mutable_buffer{arr, N * sizeof(PodType)};
    }

    /*!
     * @brief Create a new modifiable buffer that represents the given POD
     * array.
     * @tparam PodType The array item type.
     * @tparam N The count of array items.
     * @param arr The array.
     * @param max_n The max count of array items to include in the buffer.
     * If @p max_n is greater than N, it will be truncated.
     * @return The new modifiable buffer that represents the given POD array.
     * The size of the returned buffer is `min(N, max_n) * sizeof(PodType)`.
     */
    template <class PodType, std::size_t N>
    inline constexpr mutable_buffer buffer(PodType (&arr)[N],
                                           std::size_t max_n) noexcept {
        static_assert(detail::check_pod<PodType>,
                      "the array must be of pod type");
        max_n = std::min(max_n, N);
        return mutable_buffer{arr, max_n * sizeof(PodType)};
    }

    /*!
     * @brief Create a new non-modifiable buffer that represents the given POD
     * array.
     * @tparam PodType The array item type.
     * @tparam N The count of array items.
     * @param arr The array.
     * @return The new non-modifiable buffer that represents the given POD
     * array. The size of the returned buffer is `N * sizeof(PodType)`.
     */
    template <class PodType, std::size_t N>
    inline constexpr const_buffer buffer(const PodType (&arr)[N]) noexcept {
        static_assert(detail::check_pod<PodType>,
                      "the array must be of pod type");
        return const_buffer{arr, N * sizeof(PodType)};
    }

    /*!
     * @brief Create a new non-modifiable buffer that represents the given POD
     * array.
     * @tparam PodType The array item type.
     * @tparam N The count of array items.
     * @param arr The array.
     * @param max_n The max count of array items to include in the buffer.
     * If @p max_n is greater than N, it will be truncated.
     * @return The new non-modifiable buffer that represents the given POD
     * array. The size of the returned buffer is `min(N, max_n) *
     * sizeof(PodType)`.
     */
    template <class PodType, std::size_t N>
    inline constexpr const_buffer buffer(const PodType (&arr)[N],
                                         std::size_t max_n) noexcept {
        static_assert(detail::check_pod<PodType>,
                      "the array must be of pod type");
        max_n = std::min(max_n, N);
        return const_buffer{arr, max_n * sizeof(PodType)};
    }

    // from std::array

    /*!
     * @brief Create a new modifiable buffer that represents the given POD
     * array.
     * @tparam PodType The array item type.
     * @tparam N The count of array items.
     * @param arr The array.
     * @return The new modifiable buffer that represents the given POD array.
     * The size of the returned buffer is `N * sizeof(PodType)`.
     */
    template <class PodType, std::size_t N>
    inline constexpr mutable_buffer
    buffer(std::array<PodType, N>& arr) noexcept {
        static_assert(detail::check_pod<PodType>,
                      "the array must be of pod type");
        return mutable_buffer{arr.data(), arr.size() * sizeof(PodType)};
    }

    /*!
     * @brief Create a new modifiable buffer that represents the given POD
     * array.
     * @tparam PodType The array item type.
     * @tparam N The count of array items.
     * @param arr The array.
     * @param max_n The max count of array items to include in the buffer.
     * If @p max_n is greater than N, it will be truncated.
     * @return The new modifiable buffer that represents the given POD array.
     * The size of the returned buffer is `min(N, max_n) * sizeof(PodType)`.
     */
    template <class PodType, std::size_t N>
    inline constexpr mutable_buffer buffer(std::array<PodType, N>& arr,
                                           std::size_t max_n) noexcept {
        static_assert(detail::check_pod<PodType>,
                      "the array must be of pod type");
        max_n = std::min(max_n, N);
        return mutable_buffer{arr.data(), max_n * sizeof(PodType)};
    }

    /*!
     * @brief Create a new non-modifiable buffer that represents the given POD
     * array.
     * @tparam PodType The array item type.
     * @tparam N The count of array items.
     * @param arr The array.
     * @return The new non-modifiable buffer that represents the given POD
     * array. The size of the returned buffer is `N * sizeof(PodType)`.
     */
    template <class PodType, std::size_t N>
    inline constexpr const_buffer
    buffer(const std::array<PodType, N>& arr) noexcept {
        static_assert(detail::check_pod<PodType>,
                      "the array must be of pod type");
        return const_buffer{arr.data(), arr.size() * sizeof(PodType)};
    }

    /*!
     * @brief Create a new non-modifiable buffer that represents the given POD
     * array.
     * @tparam PodType The array item type.
     * @tparam N The count of array items.
     * @param arr The array.
     * @param max_n The max count of array items to include in the buffer.
     * If @p max_n is greater than N, it will be truncated.
     * @return The new non-modifiable buffer that represents the given POD
     * array. The size of the returned buffer is `min(N, max_n) *
     * sizeof(PodType)`.
     */
    template <class PodType, std::size_t N>
    inline constexpr const_buffer buffer(const std::array<PodType, N>& arr,
                                         std::size_t max_n) noexcept {
        static_assert(detail::check_pod<PodType>,
                      "the array must be of pod type");
        max_n = std::min(max_n, N);
        return const_buffer{arr.data(), max_n * sizeof(PodType)};
    }

    // from vectors of pod types

    /*!
     * @brief Create a new modifiable buffer that represents the given POD
     * vector.
     * @tparam PodType The vector value type.
     * @tparam Alloc The vector allocator type.
     * @param vec The vector.
     * @return The new modifiable buffer that represents the given POD vector.
     * The size of the returned buffer is `vec.size() * sizeof(PodType)`.
     */
    template <class PodType, class Alloc>
    inline constexpr mutable_buffer
    buffer(std::vector<PodType, Alloc>& vec) noexcept {
        static_assert(detail::check_pod<PodType>,
                      "the vector must be of pod type");
        const std::size_t size = vec.size();
        return mutable_buffer(size ? vec.data() : nullptr,
                              size * sizeof(PodType));
    }

    /*!
     * @brief Create a new modifiable buffer that represents the given POD
     * vector.
     * @tparam PodType The vector value type.
     * @tparam Alloc The vector allocator type.
     * @param vec The vector.
     * @param max_n The max count of vector items to include in the buffer.
     * If @p max_n is greater than @p vec size, it will be truncated.
     * @return The new modifiable buffer that represents the given POD vector.
     * The size of the returned buffer is `min(vec.size(), max_n) *
     * sizeof(PodType)`.
     */
    template <class PodType, class Alloc>
    inline constexpr mutable_buffer buffer(std::vector<PodType, Alloc>& vec,
                                           std::size_t max_n) noexcept {
        static_assert(detail::check_pod<PodType>,
                      "the vector must be of pod type");
        max_n = std::min(max_n, vec.size());
        return mutable_buffer(max_n ? vec.data() : nullptr,
                              max_n * sizeof(PodType));
    }

    /*!
     * @brief Create a new non-modifiable buffer that represents the given POD
     * vector.
     * @tparam PodType The vector value type.
     * @tparam Alloc The vector allocator type.
     * @param vec The vector.
     * @return The new non-modifiable buffer that represents the given POD
     * vector. The size of the returned buffer is `vec.size() *
     * sizeof(PodType)`.
     */
    template <class PodType, class Alloc>
    inline constexpr const_buffer
    buffer(const std::vector<PodType, Alloc>& vec) noexcept {
        static_assert(detail::check_pod<PodType>,
                      "the vector must be of pod type");
        std::size_t size = vec.size();
        return const_buffer(size ? vec.data() : nullptr,
                            size * sizeof(PodType));
    }

    /*!
     * @brief Create a new non-modifiable buffer that represents the given POD
     * vector.
     * @tparam PodType The vector value type.
     * @tparam Alloc The vector allocator type.
     * @param vec The vector.
     * @param max_n The max count of vector items to include in the buffer.
     * If @p max_n is greater than @p vec size, it will be truncated.
     * @return The new non-modifiable buffer that represents the given POD
     * vector. The size of the returned buffer is `min(vec.size(), max_n) *
     * sizeof(PodType)`.
     */
    template <class PodType, class Alloc>
    inline constexpr const_buffer buffer(const std::vector<PodType, Alloc>& vec,
                                         std::size_t max_n) noexcept {
        static_assert(detail::check_pod<PodType>,
                      "the vector must be of pod type");
        max_n = std::min(max_n, vec.size());
        return const_buffer(max_n ? vec.data() : nullptr,
                            max_n * sizeof(PodType));
    }

    // from basic_string

    /*!
     * @brief Create a new modifiable buffer that represents the given
     * string.
     * @tparam CharT The string value type.
     * @tparam Traits The string char traits.
     * @tparam Alloc The string allocator type.
     * @param str The string.
     * @return The new modifiable buffer that represents the given
     * string. The size of the returned buffer is `str.size() *
     * sizeof(CharT)`.
     */
    template <class CharT, class Traits, class Alloc>
    inline constexpr mutable_buffer
    buffer(std::basic_string<CharT, Traits, Alloc>& str) noexcept {
        static_assert(detail::check_pod<CharT>,
                      "the string must be of pod type");
        const std::size_t size = str.size();
        return mutable_buffer(size ? str.data() : nullptr,
                              size * sizeof(CharT));
    }

    /*!
     * @brief Create a new modifiable buffer that represents the given
     * string.
     * @tparam CharT The string value type.
     * @tparam Traits The string char traits.
     * @tparam Alloc The string allocator type.
     * @param str The string.
     * @param max_n The max count of string items to include in the buffer.
     * If @p max_n is greater than @p str size, it will be truncated.
     * @return The new modifiable buffer that represents the given
     * string. The size of the returned buffer is `min(str.size(), max_n) *
     * sizeof(CharT)`.
     */
    template <class CharT, class Traits, class Alloc>
    inline constexpr mutable_buffer
    buffer(std::basic_string<CharT, Traits, Alloc>& str,
           std::size_t max_n) noexcept {
        static_assert(detail::check_pod<CharT>,
                      "the string must be of pod type");
        max_n = std::min(max_n, str.size());
        return mutable_buffer(max_n ? str.data() : nullptr,
                              max_n * sizeof(CharT));
    }

    /*!
     * @brief Create a new non-modifiable buffer that represents the given
     * string.
     * @tparam CharT The string value type.
     * @tparam Traits The string char traits.
     * @tparam Alloc The string allocator type.
     * @param str The string.
     * @return The new non-modifiable buffer that represents the given
     * string. The size of the returned buffer is `str.size() *
     * sizeof(CharT)`.
     */
    template <class CharT, class Traits, class Alloc>
    inline constexpr const_buffer
    buffer(const std::basic_string<CharT, Traits, Alloc>& str) noexcept {
        static_assert(detail::check_pod<CharT>,
                      "the string must be of pod type");
        const std::size_t size = str.size();
        return const_buffer(size ? str.data() : nullptr, size * sizeof(CharT));
    }

    /*!
     * @brief Create a new non-modifiable buffer that represents the given
     * string.
     * @tparam CharT The string value type.
     * @tparam Traits The string char traits.
     * @tparam Alloc The string allocator type.
     * @param str The string.
     * @param max_n The max count of string items to include in the buffer.
     * If @p max_n is greater than @p str size, it will be truncated.
     * @return The new non-modifiable buffer that represents the given
     * string. The size of the returned buffer is `min(str.size(), max_n) *
     * sizeof(CharT)`.
     */
    template <class CharT, class Traits, class Alloc>
    inline constexpr const_buffer
    buffer(const std::basic_string<CharT, Traits, Alloc>& str,
           std::size_t max_n) noexcept {
        static_assert(detail::check_pod<CharT>,
                      "the string must be of pod type");
        max_n = std::min(max_n, str.size());
        return const_buffer(max_n ? str.data() : nullptr,
                            max_n * sizeof(CharT));
    }

    // from basic_string_view

    /*!
     * @brief Create a new non-modifiable buffer that represents the given
     * string_view.
     * @tparam CharT The string value type.
     * @tparam Traits The string char traits.
     * @param str The string view.
     * @return The new non-modifiable buffer that represents the given
     * string_view. The size of the returned buffer is `str.size() *
     * sizeof(CharT)`.
     */
    template <class CharT, class Traits>
    inline constexpr const_buffer
    buffer(const std::basic_string_view<CharT, Traits>& str) noexcept {
        static_assert(detail::check_pod<CharT>,
                      "the string view must be of pod type");
        const std::size_t size = str.size();
        return const_buffer(size ? str.data() : nullptr, size * sizeof(CharT));
    }

    /*!
     * @brief Create a new non-modifiable buffer that represents the given
     * string_view.
     * @tparam CharT The string value type.
     * @tparam Traits The string char traits.
     * @param str The string view.
     * @param max_n The max count of string items to include in the buffer.
     * If @p max_n is greater than @p str size, it will be truncated.
     * @return The new non-modifiable buffer that represents the given POD
     * array. The size of the returned buffer is `min(str.size(), max_n) *
     * sizeof(CharT)`.
     */
    template <class CharT, class Traits>
    inline constexpr const_buffer
    buffer(const std::basic_string_view<CharT, Traits>& str,
           std::size_t max_n) noexcept {
        static_assert(detail::check_pod<CharT>,
                      "the string view must be of pod type");
        max_n = std::min(max_n, str.size());
        return const_buffer(max_n ? str.data() : nullptr,
                            max_n * sizeof(CharT));
    }

    /*!
     * @brief Create a new modifiable buffer that represents the given POD
     * span.
     * @tparam PodType The span value type.
     * @tparam Extent The span extent.
     * @param s The span.
     * @return The new modifiable buffer that represents the given POD span.
     * The size of the returned buffer is `s.size() * sizeof(PodType)`.
     */
    template <class PodType, std::size_t Extent>
    inline constexpr mutable_buffer
    buffer(std::span<PodType, Extent> s) noexcept {
        static_assert(detail::check_pod<PodType>,
                      "the span must be of pod type");
        return {s.empty() ? nullptr : s.data(), s.size_bytes()};
    }

    /*!
     * @brief Create a new modifiable buffer that represents the given POD
     * span.
     * @tparam PodType The span value type.
     * @tparam Extent The span extent.
     * @param s The span.
     * @param max_n The max count of span items to include in the buffer.
     * If @p max_n is greater than @p s size, it will be truncated.
     * @return The new modifiable buffer that represents the given POD
     * span. The size of the returned buffer is `min(s.size(), max_n) *
     * sizeof(PodType)`.
     */
    template <class PodType, std::size_t Extent>
    inline constexpr mutable_buffer buffer(std::span<PodType, Extent> s,
                                           std::size_t max_n) noexcept {
        static_assert(detail::check_pod<PodType>,
                      "the span must be of pod type");
        max_n = std::min(max_n, s.size());
        return {max_n ? s.data() : nullptr, max_n * sizeof(PodType)};
    }

    /*!
     * @brief Create a new non-modifiable buffer that represents the given POD
     * span.
     * @tparam PodType The span value type.
     * @tparam Extent The span extent.
     * @param s The span.
     * @return The new non-modifiable buffer that represents the given POD span.
     * The size of the returned buffer is `s.size() * sizeof(PodType)`.
     */
    template <class PodType, std::size_t Extent>
    inline constexpr const_buffer
    buffer(std::span<const PodType, Extent> s) noexcept {
        static_assert(detail::check_pod<PodType>,
                      "the span must be of pod type");
        return {s.empty() ? nullptr : s.data(), s.size_bytes()};
    }

    /*!
     * @brief Create a new non-modifiable buffer that represents the given POD
     * span.
     * @tparam PodType The span value type.
     * @tparam Extent The span extent.
     * @param s The span.
     * @param max_n The max count of span items to include in the buffer.
     * If @p max_n is greater than @p s size, it will be truncated.
     * @return The new non-modifiable buffer that represents the given POD
     * span. The size of the returned buffer is `min(s.size(), max_n) *
     * sizeof(PodType)`.
     */
    template <class PodType, std::size_t Extent>
    inline constexpr const_buffer buffer(std::span<const PodType, Extent> s,
                                         std::size_t max_n) noexcept {
        static_assert(detail::check_pod<PodType>,
                      "the span must be of pod type");
        max_n = std::min(max_n, s.size());
        return {max_n ? s.data() : nullptr, max_n * sizeof(PodType)};
    }

    /*!
     * @brief Create a new modifiable buffer that represents the given dynamic
     * buffer.
     * @param dyn_buff The dynamic buffer.
     * @return The new modifiable buffer that represents the given dynamic
     * buffer.
     * The size of the returned buffer is the same as the dynamic buffer.
     */
    inline mutable_buffer buffer(dynamic_buffer dyn_buff) noexcept {
        const std::size_t size = dyn_buff.size();
        return mutable_buffer{size ? dyn_buff.data() : nullptr, size};
    }

    /*!
     * @brief Create a new modifiable buffer that represents the given dynamic
     * buffer.
     * @param dyn_buff The dynamic buffer.
     * @param max_n The max count of bytes to include in the buffer.
     * If @p max_n is greater than @p dyn_buff size, it will be truncated.
     * @return The new modifiable buffer that represents the given dynamic
     * buffer.
     * The size of the returned buffer is `min(dyn_buff.size(), max_n)`.
     */
    inline mutable_buffer buffer(dynamic_buffer dyn_buff,
                                 std::size_t max_n) noexcept {
        max_n = std::min(max_n, dyn_buff.size());
        return mutable_buffer{max_n ? dyn_buff.data() : nullptr, max_n};
    }

    inline mutable_buffer dynamic_buffer::prepare(size_t size) {
        return {impl_.increase_size(size), size};
    }

} // namespace RAD_LIB_NAMESPACE

// some buffers concepts

namespace RAD_LIB_NAMESPACE {

    template <class Buffer>
    concept OneConstBuffer =
        std::is_same_v<std::remove_cvref_t<Buffer>, const_buffer>;

    template <class Buffer>
    concept OneMutableBuffer =
        std::is_same_v<std::remove_cvref_t<Buffer>, mutable_buffer>;

    template <class Buffer>
    concept OneBuffer = OneConstBuffer<Buffer> || OneMutableBuffer<Buffer>;

    template <class Buffers>
    concept MultiMutableBuffers = requires(const Buffers& buffers) {
        { std::data(buffers) } -> std::convertible_to<const mutable_buffer*>;
        { std::size(buffers) } -> std::convertible_to<std::size_t>;
    };

    template <class Buffers>
    concept MultiConstBuffers = requires(const Buffers& buffers) {
        { std::data(buffers) } -> std::convertible_to<const const_buffer*>;
        { std::size(buffers) } -> std::convertible_to<std::size_t>;
    };

    template <class Buffers>
    concept MultiBuffers =
        MultiMutableBuffers<Buffers> || MultiConstBuffers<Buffers>;

    template <class Buffers>
    concept BufferSequence = OneBuffer<Buffers> || MultiBuffers<Buffers>;

    template <class Buffers>
    concept ConstBufferSequence =
        OneConstBuffer<Buffers> || MultiConstBuffers<Buffers>;

    template <class Buffers>
    concept MutableBufferSequence =
        OneMutableBuffer<Buffers> || MultiMutableBuffers<Buffers>;

} // namespace RAD_LIB_NAMESPACE

namespace RAD_LIB_NAMESPACE::detail {
    template <class Buffers>
    using buffers_seq = std::pair<
        std::add_pointer_t<std::add_const_t<std::remove_pointer_t<Buffers>>>,
        std::size_t>;

    template <bool add_const, class Buffer>
    auto extract_buffer(const Buffer& buffer) {
        if constexpr (std::is_same_v<Buffer, const_buffer>) {
            return std::addressof(buffer);
        }
        else if constexpr (std::is_same_v<Buffer, mutable_buffer> &&
                           add_const) {
            return reinterpret_cast<const const_buffer*>(
                std::addressof(buffer));
        }
        else if constexpr (std::is_same_v<Buffer, mutable_buffer>) {
            return std::addressof(buffer);
        }
    }

    template <bool add_const, class Buffer, class Buffers>
    auto get_buffers_ptr(const Buffers& buffers) {
        auto ptr = std::data(buffers);

        if constexpr (std::is_same_v<Buffer, const_buffer> ||
                      (std::is_same_v<Buffer, mutable_buffer> && !add_const)) {
            return ptr;
        }
        if constexpr (std::is_same_v<Buffer, mutable_buffer> && add_const) {
            return reinterpret_cast<const const_buffer*>(ptr);
        }
    }

} // namespace RAD_LIB_NAMESPACE::detail

namespace RAD_LIB_NAMESPACE {
    template <bool add_const, class Buffers>
    auto extract_buffers(const Buffers& buffers) {
        if constexpr (std::is_same_v<Buffers, const_buffer> ||
                      std::is_same_v<Buffers, mutable_buffer>) {
            auto ptr = detail::extract_buffer<add_const>(buffers);
            return detail::buffers_seq<std::decay_t<decltype(ptr)>>{ptr, 1};
        }
        else {
            using buffer_type =
                std::remove_cvref_t<decltype(*std::data(buffers))>;
            if constexpr (std::is_same_v<buffer_type, const_buffer> ||
                          std::is_same_v<buffer_type, mutable_buffer>) {
                auto ptr =
                    detail::get_buffers_ptr<add_const, buffer_type>(buffers);
                std::size_t count =
                    static_cast<std::size_t>(std::size(buffers));
                return detail::buffers_seq<std::decay_t<decltype(ptr)>>{ptr,
                                                                        count};
            }
            else {
                static_assert(always_false<Buffers>, "invalid buffers type");
            }
        }
    }

    template <class Buffers>
    std::size_t buffers_size(const Buffers& buffers) noexcept {
        if constexpr (std::is_convertible_v<Buffers, const_buffer>) {
            return static_cast<const_buffer>(buffers).size();
        }
        else {
            std::size_t total_size = 0;
            for (const auto& buff : buffers) {
                total_size += buff.size();
            }
            return total_size;
        }
    }

    template <class Buffers>
    bool not_empty_buffers(const Buffers& buffers) noexcept {
        if constexpr (std::is_convertible_v<Buffers, const_buffer>) {
            return buffers.size();
        }
        else {
            for (const auto& buff : buffers) {
                if (buff.size()) {
                    return true;
                }
            }
            return false;
        }
    }

    template <class Buffers>
    using raw_array_to_array =
        std::array<std::decay_t<std::remove_all_extents_t<Buffers>>,
                   sizeof(Buffers) /
                       sizeof(std::remove_all_extents_t<Buffers>)>;

    template <BufferSequence Buffers>
    using buffers_range_t = std::conditional_t<
        std::is_same_v<std::remove_cvref_t<Buffers>, const_buffer> ||
            std::is_same_v<std::remove_cvref_t<Buffers>, mutable_buffer>,
        std::array<std::decay_t<Buffers>, 1>,
        std::conditional_t<!std::is_array_v<std::remove_cvref_t<Buffers>>,
                           std::remove_cvref_t<Buffers>,
                           raw_array_to_array<Buffers>>>;

    template <BufferSequence Buffers>
    class buffers_range_adapter {
        template <class T, std::size_t N>
        static std::array<T, N> copy_array(const T (&raw_arr)[N]) {
            std::array<T, N> arr;
            std::copy(std::begin(raw_arr), std::end(raw_arr), std::begin(arr));
            return arr;
        }

        template <class T, std::size_t N>
        static std::array<T, N> move_array(const T (&raw_arr)[N]) {
            return copy_array(raw_arr);
        }

        static const Buffers& copy_buffers(const Buffers& buffs) {
            return buffs;
        }

        static Buffers&& move_buffers(Buffers&& buffs) {
            return std::move(buffs);
        }

    public:
        using range_type = buffers_range_t<Buffers>;
        using pointer_type = std::conditional_t<MutableBufferSequence<Buffers>,
                                                mutable_buffer*, const_buffer*>;
        using const_pointer_type =
            std::conditional_t<MutableBufferSequence<Buffers>,
                               const mutable_buffer*, const const_buffer*>;

        buffers_range_adapter(const Buffers& buffs)
            requires(!std::is_array_v<Buffers>)
            : buffers_{buffs} {
        }

        buffers_range_adapter(Buffers&& buffs)
            requires(!std::is_array_v<Buffers>)
            : buffers_{std::move(buffs)} {
        }

        buffers_range_adapter(const Buffers& buffs)
            requires(std::is_array_v<Buffers>)
            : buffers_{copy_array(buffs)} {
        }

        buffers_range_adapter(Buffers&& buffs)
            requires(std::is_array_v<Buffers>)
            : buffers_{move_array(std::move(buffs))} {
        }

        buffers_range_adapter(buffers_range_adapter&& other) noexcept
            : total_size_{std::exchange(other.total_size_, 0)} {
            if (std::empty(other.buffers_)) {
                return;
            }
            // if the buffers are stored in an array the
            // current buffer pointer must be adjusted
            pointer_type other_ptr = std::data(other.buffers_);
            buffers_ = std::move(other.buffers_);
            pointer_type this_ptr = std::data(buffers_);
            current_buff_ = other.current_buff_ + (this_ptr - other_ptr);
            other.current_buff_ = other_ptr;
        }

        buffers_range_adapter(const buffers_range_adapter& other)
            : total_size_{other.total_size_} {
            if (std::empty(other.buffers_)) {
                return;
            }
            // if the buffers are stored in an array the
            // current buffer pointer must be adjusted
            pointer_type other_ptr = std::data(other.buffers_);
            buffers_ = other.buffers_;
            pointer_type this_ptr = std::data(buffers_);
            current_buff_ = other.current_buff_ + (this_ptr - other_ptr);
        }

        std::size_t size() const noexcept {
            return total_size_;
        }

        std::size_t count() const noexcept {
            return std::size(buffers_);
        }

        bool empty() const noexcept {
            return !total_size_;
        }

        bool finished() const noexcept {
            return current_buff_ == last_buffer();
        }

        const range_type& buffers() const noexcept {
            return buffers_;
        }

        pointer_type current_buffer() const noexcept {
            return current_buff_;
        }

        std::size_t buffers_count() const noexcept {
            return static_cast<std::size_t>(std::data(buffers_) -
                                            current_buff_) +
                   std::size(buffers_);
        }

        std::size_t current_size() const noexcept {
            std::size_t n = 0;
            for (const_pointer_type first = current_buff_, last = last_buffer();
                 first != last; ++first) {
                n += first->size();
            }
            return n;
        }

        void advance(std::size_t n) noexcept {
            auto last = last_buffer();

            while (n) {
                assert(current_buff_ != last);

                if (current_buff_->size() > n) {
                    *current_buff_ += n;
                    break;
                }
                n -= current_buff_->size();
                ++current_buff_;
            }

            while (current_buff_ != last && current_buff_->empty()) {
                ++current_buff_;
            }
        }

        /*!
        @brief Set the current buffer pointer to a pointer to
        buffer in the underlying range
        @param cur the current buffer pointer which must be int
        the range [first, last].
        * Note that setting current buffer to the not
        derefrencable last pointer is valid and means
        * that the buffer is totally consumed
        */
        void set_current_buffer(pointer_type cur) noexcept {
            assert(cur >= std::data(buffers_) &&
                   cur <= std::data(buffers_) + std::size(buffers_));
            current_buff_ = cur;
        }

        /*!
        @brief Set the current buffer pointer to a pointer to
        buffer in the underlying range
        @param cur the current buffer pointer which must be int
        the range [first, last].
        * Note that setting current buffer to the not
        derefrencable last pointer is valid and means
        * that the buffer is totally consumed
        */
        void set_current_buffer(const_pointer_type cur) noexcept {
            assert(cur >= std::data(buffers_) &&
                   cur <= std::data(buffers_) + std::size(buffers_));
            current_buff_ = const_cast<pointer_type>(cur);
        }

    private:
        pointer_type last_buffer() noexcept {
            return std::data(buffers_) + std::size(buffers_);
        }

        const_pointer_type last_buffer() const noexcept {
            return std::data(buffers_) + std::size(buffers_);
        }

        range_type buffers_;
        pointer_type current_buff_ = std::data(buffers_);
        std::size_t total_size_ = buffers_size(buffers_);
    };
} // namespace RAD_LIB_NAMESPACE