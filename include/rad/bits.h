#pragma once
#include <rad/libbase.h>

#include <cassert>

namespace RAD_LIB_NAMESPACE::bits {
    /*!
     * @brief Check if the i-th bit is set in an integer
     * @tparam T The type of integral
     * @tparam N The index of the bit to check. Must be in the range [0,
     * sizeof(T) * 8[
     * @param i The integer to check its i-th bit
     * @return true if the i-th bit is set, and false if not set
     */
    template <size_t N, class T>
    constexpr bool check(T i) noexcept {
        static_assert(std::is_integral_v<T>,
                      "bits operations are fore integrals only");
        static_assert(N < sizeof(T) * 8, "out of range bit access");
        return (i & (T{1} << static_cast<T>(N))) != 0;
    }

    /*!
     * @brief Check if the i-th bit is set in an integer
     * @tparam T The type of integral
     * @param i The integer to check its i-th bit
     * @param bitn The index of the bit to check. Must be in the range [0,
     * sizeof(T)
     * * 8[
     * @return true if the i-th bit is set, and false if not set
     */
    template <class T>
    constexpr bool check(T i, std::type_identity_t<T> bitn) noexcept {
        assert(bitn < sizeof(T) * 8 && "out of range bit access");
        return (i & (T{1} << bitn)) != 0;
    }

    /*!
     * @brief Set the i-th bit in an integer
     * @tparam T The type of integral
     * @tparam N The index of the bit to set. Must be in the range [0,
     * sizeof(T) * 8[
     * @param i The integer to set its i-th bit
     */
    template <size_t N, class T>
    constexpr void set(T& i) noexcept {
        static_assert(std::is_integral_v<T>,
                      "bits operations are for integrals only");
        static_assert(N < sizeof(T) * 8, "out of range bit access");
        i |= (T{1} << static_cast<T>(N));
    }

    /*!
     * @brief Set the i-th bit in an integer
     * @tparam T The type of integral
     * @param i The integer to set its i-th bit
     * @param bitn The index of the bit to set. Must be in the range [0,
     * sizeof(T) * 8[
     */
    template <class T>
    constexpr void set(T& i, std::size_t bitn) noexcept {
        assert(bitn < sizeof(T) * 8 && "out of range bit access");
        i |= (T{1} << bitn);
    }

    /*!
     * @brief Clear the i-th bit in an integer
     * @tparam T The type of integral
     * @tparam N The index of the bit to clear. Must be in the range [0,
     * sizeof(T) * 8[
     * @param i The integer to clear its i-th bit
     */
    template <size_t N, class T>
    constexpr void clear(T& i) noexcept {
        static_assert(std::is_integral_v<T>,
                      "bits operations are for integrals only");
        static_assert(N < sizeof(T) * 8, "out of range bit access");
        i &= static_cast<T>(~(T{1} << N));
    }

    /*!
     * @brief Clear the i-th bit in an integer
     * @tparam T The type of integral
     * @param i The integer to clear its i-th bit
     * @param bitn The index of the bit to clear. Must be in the range [0,
     * sizeof(T)
     * * 8[
     */
    template <class T>
    constexpr void clear(T& i, std::size_t bitn) noexcept {
        assert(bitn < sizeof(T) * 8 && "out of range bit access");
        i &= ~(T{1} << bitn);
    }

    /*!
     * @brief Extract the first N bits from an integer
     * @tparam T The type of integral
     * @tparam N The count of first bits to extract.
     * Must be larger than or equal to 1 and less than or equal to the count
     * of bits in the integral.
     * @param i The integer to extract first bits from
     * @return The extracted first N bits
     */
    template <std::size_t N, class T>
    constexpr T first_n_bits(T i) {
        static_assert(std::is_integral_v<T>,
                      "bits operations are for integrals only");
        static_assert(N > 0 && N <= sizeof(T) * 8,
                      "valid bits range from 1 to sizeof(T) * 8");
        const T bit_mask = (T{1} << N) - T{1};
        return i & bit_mask;
    }

    /*!
     * @brief Extract the first N bits from an integer
     * @tparam T The type of integral
     * @param i The integer to extract first bits from
     * @param bits_count The count of first bits to extract.
     * Must be larger than or equal to 1 and less than or equal to the count
     * of bits in the integral.
     * @return The extracted first N bits
     */
    template <class T>
    constexpr T first_n_bits(T i, std::size_t bits_count) {
        static_assert(std::is_integral_v<T>,
                      "bits operations are for integrals only");
        assert(bits_count > 0 && bits_count <= sizeof(T) * 8 &&
               "valid bits range from 1 to sizeof(T) * 8");
        const T bit_mask = (T{1} << static_cast<T>(bits_count)) - T{1};
        return i & bit_mask;
    }

    /*!
     * @brief Extract a range of bits from an integer
     * @tparam T The type of integral
     * @tparam From The first bit index to extract. This bit is included in
     * the result.
     * @tparam To The last bit index to extract. This bit is included in the
     * result. This must be larger than or equal to From and lesst than the
     * count of bits in the integral.
     * @param i The integer to extract bits from
     * @return The extracted bits range
     */
    template <size_t From, size_t To, class T>
    constexpr T extract(T i) noexcept {
        static_assert(std::is_integral_v<T>,
                      "bits operations are for integrals only");
        static_assert(From < sizeof(T) * 8 && To < sizeof(T) * 8 && From <= To,
                      "out of range bit access");
        return first_n_bits<To - From + 1>(i >> From);
    }

    /*!
     * @brief Extract a range of bits from an integer
     * @tparam T The type of integral
     * @param i The integer to extract bits from
     * @param from The first bit index to extract. This bit is included in
     * the result.
     * @param to The last bit index to extract. This bit is included in the
     * result. This must be larger than or equal to From and lesst than the
     * count of bits in the integral.
     * @return The extracted bits range
     */
    template <class T>
    constexpr T extract(T i, std::size_t from, std::size_t to) noexcept {
        assert(from < sizeof(T) * 8 && to < sizeof(T) * 8 && from <= to &&
               "out of range bit access");
        return first_n_bits(i >> from, to - from + 1);
    }

    /*!
     * @brief Set or clear the i-th bit in an integer
     * @tparam T The type of integral
     * @tparam N The index of the bit to set or clear. Must be in the range
     * [0, sizeof(T) * 8[
     * @param i The integer to set or clear its i-th bit
     * @param on_or_off True to set the i-th bit, and false to clear the
     * i-th bit
     */
    template <size_t N, class T>
    constexpr void assign(T& i, bool on_or_off) {
        static_assert(N < sizeof(T) * 8, "out of range bit access");
        i |= static_cast<T>(on_or_off) << N;
    }

    /*!
     * @brief Set or clear the i-th bit in an integer
     * @tparam T The type of integral
     * @param i The integer to set or clear its i-th bit
     * @param bitn The index of the bit to set or clear. Must be in the
     * range [0, sizeof(T) * 8[
     * @param on_or_off True to set the i-th bit, and false to clear the
     * i-th bit
     */
    template <class T>
    constexpr void assign(T& i, std::size_t bitn, bool on_or_off) {
        assert(bitn < sizeof(T) * 8 && "out of range bit access");
        i |= static_cast<T>(on_or_off) << bitn;
    }

    template <class T>
    class bits_base {
    public:
        static constexpr size_t bytes_count = sizeof(T);
        static constexpr size_t bits_count = sizeof(T) * 8;

        bits_base(T i) {
            assign(i);
        }

        T get() const noexcept {
            return val();
        }

        void assign(T i) noexcept {
            memcpy(buff, &i, sizeof(T));
        }

        bits_base& operator=(T i) noexcept {
            assign(i);
            return *this;
        }

        uint8_t* buffer() noexcept {
            return buff;
        }

        const uint8_t* buffer() const noexcept {
            return buff;
        }

        bits_base& operator|=(T i) noexcept {
            assign(val() | i);
            return *this;
        }

        template <size_t n>
        constexpr bool check() const noexcept {
            return bits::check<n>(val());
        }

        constexpr bool check(size_t bitn) const noexcept {
            return bits::check(val(), bitn);
        }

        template <size_t n>
        void set() noexcept {
            T i = val();
            bits::set<n>(i);
            assign(i);
        }

        void set(std::size_t bitn) noexcept {
            T i = val();
            bits::set(i, bitn);
            assign(i);
        }

        template <size_t n>
        void clear() noexcept {
            T i = val();
            bits::clear<n>(i);
            assign(i);
        }

        void clear(std::size_t bitn) noexcept {
            T i = val();
            bits::clear(i, bitn);
            assign(i);
        }

        template <size_t From, size_t To>
        constexpr T extract() const noexcept {
            return bits::extract<From, To>(val());
        }

        constexpr T extract(std::size_t from, std::size_t to) const noexcept {
            return bits::extract(val(), from, to);
        }

    private:
        T val() const noexcept {
            T ret;
            memcpy(&ret, buff, sizeof(T));
            return ret;
        }

        alignas(T) uint8_t buff[bytes_count];
    };

    using u8bits = bits_base<uint8_t>;
    using u16bits = bits_base<uint16_t>;
    using u32bits = bits_base<uint32_t>;
    using u64bits = bits_base<uint64_t>;

    static_assert(check<2>(0b00000100), "bits::check not working correctly !");
    static_assert(check<5>(0b00100100), "bits::check not working correctly !");
    static_assert(extract<1, 3>(0b11010) == 0b101,
                  "bits::extract not working correctly !");
} // namespace RAD_LIB_NAMESPACE::bits