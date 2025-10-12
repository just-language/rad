#pragma once
#include <rad/bits.h>
#include <rad/libbase.h>

namespace RAD_LIB_NAMESPACE {
    namespace detail {
#ifdef _WIN32
        inline uint16_t bswap16(uint16_t val) noexcept {
            return _byteswap_ushort(val);
        }

        inline uint32_t bswap32(uint32_t val) noexcept {
            return _byteswap_ulong(val);
        }

        inline uint64_t bswap64(uint64_t val) noexcept {
            return _byteswap_uint64(val);
        }
#else
        inline uint16_t bswap16(uint16_t val) noexcept {
            return __builtin_bswap16(val);
        }

        inline uint32_t bswap32(uint32_t val) noexcept {
            return __builtin_bswap32(val);
        }

        inline uint64_t bswap64(uint64_t val) noexcept {
            return __builtin_bswap64(val);
        }
#endif // _WIN32

        template <class T>
        inline T bswap(T val) noexcept {
            if constexpr (std::is_same_v<T, uint16_t> ||
                          std::is_same_v<T, int16_t>) {
                return bswap16(static_cast<uint16_t>(val));
            }
            else if constexpr (std::is_same_v<T, uint32_t> ||
                               std::is_same_v<T, int32_t>) {
                return bswap32(static_cast<uint32_t>(val));
            }
            else if constexpr (std::is_same_v<T, uint64_t> ||
                               std::is_same_v<T, int64_t>) {
                return bswap64(static_cast<uint64_t>(val));
            }
            else if constexpr (sizeof(val) == 1) {
                return val;
            }
        }
    } // namespace detail

    /*!
     * @brief a class that stores an unsigned integer in big endian and
     * allows to retrieve it in little endian
     * @tparam UnsignedIntegerType an unsgined integer type, either
     * uint16_t, uint32_t or uint64_T example:
     * @code
     * uint32_t le_val = 0x516512;
     * beu_base<uint32_t> be_val = le_val;
     * uint32_t le_val2 = be_val;
     * assert(le_val == le_val2)
     * @endocde
     */
    template <std::unsigned_integral UnsignedIntegerType>
    class beu_base {
    public:
        using integer_type = UnsignedIntegerType;

        /*!
         * @brief construct a big endian unsigned integer and
         * set underlying integer to 0
         */
        constexpr beu_base() = default;

        /*!
         * @brief construct a big endian unsigned integer and
         * set underlying big endian integer to val
         * @param val in little endain order
         */
        beu_base(integer_type val)
            : big_endian_val(detail::bswap<integer_type>(val)) {
        }

        /*!
         * @brief implicit conversion to liitle endian unsigned
         * integer
         * @return little endain unsigned integer
         */
        operator UnsignedIntegerType() const noexcept {
            return get_le_val();
        }

        /*!
         * @brief check if the the underlying big endian
         * unsigned integer isn't equal to 0
         * @return true if not zero and false if zero
         */
        explicit constexpr operator bool() const noexcept {
            return big_endian_val != 0;
        }

        // since beu_base is implicitly convertible to its
        // underlying integral type the built in operator will
        // be used bool operator==(const beu_base&) const
        // noexcept = default;

        // since beu_base is implicitly convertible to its
        // underlying integral type the built in operator will
        // be used std::strong_ordering operator<=>(const
        // beu_base& other) const noexcept = default;

        /*!
         * @brief pre increment the underying big endian
         * unsigned number by 1, increment is done in little
         * endian then result is stored in big endian
         * @return the object itself
         */
        beu_base& operator++() noexcept {
            store_be_val(get_le_val() + 1);
            return *this;
        }

        /*!
         * @brief post increment operator
         * @return the old object before increment
         */
        beu_base operator++(int) noexcept {
            auto old_val = big_endian_val;
            ++(*this);
            return old_val;
        }

        /*!
         * @brief increment the underlying big endian unsigned
         * integer by val, increment is done in little endian
         * then result is stored in big endian
         * @param val in little endain order
         * @return the object itself
         */
        beu_base& operator+=(integer_type val) noexcept {
            store_be_val(get_le_val() + val);
        }

        /*!
         * @brief increment the underlying big endian unsigned
         * integer by val, and return an object storing the
         * result increment is done in little endian then result
         * is stored in big endian
         * @param val in littel endian order
         * @return a new object which holds the increment result
         */
        beu_base operator+(integer_type val) const noexcept {
            return get_le_val() + val;
        }

        /*!
         * @brief pre decrement the underying big endian
         * unsigned number by 1, decrement is done in little
         * endian then result is stored in big endian
         * @return the object itself
         */
        beu_base& operator--() noexcept {
            store_be_val(get_le_val() - 1);
            return *this;
        }

        /*!
         * @brief post decrement operator
         * @return the old object before decrement
         */
        beu_base operator--(int) noexcept {
            auto old_val = big_endian_val;
            --(*this);
            return old_val;
        }

        /*!
         * @brief decrement the underlying big endian unsigned
         * integer by val, decrement is done in little endian
         * then result is stored in big endian
         * @param val in little endain order
         * @return the object itself
         */
        beu_base& operator-=(integer_type val) noexcept {
            store_be_val(get_le_val() - val);
        }

        /*!
         * @brief decrement the underlying big endian unsigned
         * integer by val, and return an object storing the
         * result decrement is done in little endian then result
         * is stored in big endian
         * @param val in littel endian order
         * @return a new object which holds the increment result
         */
        beu_base operator-(integer_type val) const noexcept {
            return get_le_val() - val;
        }

        beu_base& operator|=(integer_type val) noexcept {
            store_be_val(get_le_val() | val);
            return *this;
        }

        beu_base operator|(integer_type val) const noexcept {
            return get_le_val() | val;
        }

        beu_base& operator&=(integer_type val) noexcept {
            store_be_val(get_le_val() & val);
            return *this;
        }

        beu_base operator&(integer_type val) const noexcept {
            return get_le_val() & val;
        }

        beu_base& operator^=(integer_type val) noexcept {
            store_be_val(get_le_val() ^ val);
            return *this;
        }

        beu_base operator^(integer_type val) const noexcept {
            return get_le_val() ^ val;
        }

        constexpr bool operator[](integer_type bitn) const noexcept {
            return bits::check(big_endian_val, bitn);
        }

        void set_bit(integer_type bitn) noexcept {
            bits::set(big_endian_val, bitn);
        }

    private:
        constexpr void store_be_val(integer_type val) noexcept {
            big_endian_val = detail::bswap<integer_type>(val);
        }

        constexpr integer_type get_le_val() const noexcept {
            return detail::bswap<integer_type>(big_endian_val);
        }

        integer_type big_endian_val = 0;
    };

    using beu16 = beu_base<uint16_t>;
    using beu32 = beu_base<uint32_t>;
    using beu64 = beu_base<uint64_t>;

} // namespace RAD_LIB_NAMESPACE