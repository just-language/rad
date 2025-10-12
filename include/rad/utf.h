#pragma once
#include <rad/big_endian.h>
#include <rad/libbase.h>

#include <array>
#include <bit>
#include <cassert>
#include <limits>
#include <span>
#include <string>
#include <string_view>
#include <system_error>

namespace RAD_LIB_NAMESPACE {
    using codepoint_t = uint32_t;

    namespace detail {
        // utf constants
        inline constexpr codepoint_t max_utf_codepoint = 0x10ffff;
        inline constexpr codepoint_t utf_surrogate_start = 0xd800;
        inline constexpr codepoint_t utf_surrogate_end = 0xdfff;
        inline constexpr codepoint_t replacement_codepoint = 0xfffd;

        // UTF-8 constants

        struct utf8_mask_value {
            uint8_t mask;  // the mask to apply to the byte
            uint8_t value; // the expected out after
                           // applying the mask

            constexpr bool matches(uint8_t b) const noexcept {
                return (b & mask) == value;
            }
        };

        // the length of the matched sequence is the index + 1
        inline constexpr std::array<utf8_mask_value, 4> utf8_masks_values = {{
            {0b10000000, 0},          // 1 byte (0xxxxxxx)
            {0b11100000, 0b11000000}, // 2 bytes (110xxxxx)
            {0b11110000, 0b11100000}, // 3 bytes (1110xxxx)
            {0b11111000, 0b11110000}, // 4 bytes (11110xxx)
        }};

        inline constexpr uint8_t utf8_cont_byte_mask = 0b11000000;
        inline constexpr uint8_t utf8_cont_byte_value = 0b10000000;
        inline constexpr uint8_t utf8_cont_bits_length = 6;

        inline constexpr codepoint_t utf8_max_1 = 0x7f;
        inline constexpr codepoint_t utf8_max_2 = 0x7ff;
        inline constexpr codepoint_t utf8_max_3 = 0xffff;

        // UTF-16 constants

        inline constexpr codepoint_t utf16_bmp_max = 0xffff;
        // high surrogate in first byte and low in second byte
        inline constexpr codepoint_t utf16_surrogate_offset = 0x10000;
        inline constexpr uint16_t utf16_high_surrogate_value = 0xd800;
        inline constexpr uint16_t utf16_low_surrogate_value = 0xdc00;
        inline constexpr uint16_t utf16_surrogate_bits_length = 10;
        inline constexpr uint16_t utf16_surrogate_mask =
            std::numeric_limits<uint16_t>::max() >> 6;
        inline constexpr uint16_t utf16_surrogate_value_mask =
            ~utf16_surrogate_mask;

        template <class T>
        inline constexpr T read_le_value(const T* p) {
            T val;
            std::copy(p, p + 1, std::addressof(val));
            if constexpr (std::endian::native == std::endian::little) {
                return val;
            }
            else {
                return detail::bswap(val);
            }
        }

        template <class T>
        inline constexpr void write_le_value(T val, T* p) {
            if constexpr (std::endian::native == std::endian::little) {
                std::copy(std::addressof(val), std::addressof(val) + 1, p);
            }
            else {
                val = detail::bswap(val);
                std::copy(std::addressof(val), std::addressof(val) + 1, p);
            }
        }

        template <class T>
        inline constexpr T read_be_value(const T* p) {
            T val;
            std::copy(p, p + 1, std::addressof(val));
            if constexpr (std::endian::native == std::endian::big) {
                return val;
            }
            else {
                return detail::bswap(val);
            }
        }

        template <class T>
        inline constexpr void write_be_value(T val, T* p) {
            if constexpr (std::endian::native == std::endian::big) {
                std::copy(std::addressof(val), std::addressof(val) + 1, p);
            }
            else {
                val = detail::bswap(val);
                std::copy(std::addressof(val), std::addressof(val) + 1, p);
            }
        }

        template <class T>
        inline constexpr T read_native_value(const T* p) {
            if constexpr (std::endian::native == std::endian::little) {
                return read_le_value(p);
            }
            else {
                return read_be_value(p);
            }
        }

        template <class T>
        inline constexpr void write_native_value(T val, T* p) {
            if constexpr (std::endian::native == std::endian::little) {
                write_le_value(val, p);
            }
            else {
                write_be_value(val, p);
            }
        }

        RAD_EXPORT_DECL codepoint_t
        map_cp1256_to_unicode(std::uint8_t cp1256_char) noexcept;

        RAD_EXPORT_DECL std::pair<bool, std::uint8_t>
        map_unicode_to_cp1256(codepoint_t cp) noexcept;

        inline bool is_valid_cp1256_codepoint(codepoint_t cp) noexcept {
            return map_unicode_to_cp1256(cp).first;
        }
    } // namespace detail

    /*!
     * @brief Check if code point @p cp is in the surrogate range from
     * 0xd800 to 0xdfff inclusive.
     * @param cp The codepoint to check if it is a surrogate.
     * @return True if @p cp is a surrogate, otherwise false.
     */
    inline constexpr bool is_surrogate_codepoint(codepoint_t cp) {
        using namespace detail;
        return cp >= utf_surrogate_start && cp <= utf_surrogate_end;
    }

    /*!
     * @brief Check if code point @p cp is a valid unicode code point.
     * Valid codepoints are not greater than the maximum codepoint 0x10ffff,
     * and are not in the surrogate range from 0xd800 to 0xdfff inclusive.
     * @param cp The codepoint to validate.
     * @param ec Cleared on valid codepoint,
     * and set to std::errc::illegal_byte_sequence on invalid one.
     * @return True if @p cp is valid, otherwise false.
     */
    inline constexpr bool validate_codepoint(codepoint_t cp,
                                             std::errc& ec) noexcept {
        using namespace detail;
        if (cp > max_utf_codepoint) {
            ec = std::errc::illegal_byte_sequence;
            return false;
        }
        if (is_surrogate_codepoint(cp)) {
            ec = std::errc::illegal_byte_sequence;
            return false;
        }
        ec = {};
        return true;
    }

    /*!
     * @brief Check if code point @p cp is a valid unicode code point.
     * Valid codepoints are not greater than the maximum codepoint 0x10ffff,
     * and are not in the surrogate range from 0xd800 to 0xdfff inclusive.
     * @param cp The codepoint to validate.
     * @return True if @p cp is valid, otherwise false.
     */
    inline constexpr bool is_valid_codepoint(codepoint_t cp) noexcept {
        using namespace detail;
        if (cp > max_utf_codepoint) {
            return false;
        }
        if (is_surrogate_codepoint(cp)) {
            return false;
        }
        return true;
    }

    /*!
     * @brief Check if a byte is a UTF-8 continuation byte.
     * @param b The byte to check.
     * @return True if @p b is a UTF-8 continuation byte, otherwise false.
     */
    inline constexpr bool is_utf8_continuation_byte(uint8_t b) {
        using namespace detail;
        return (b & utf8_cont_byte_mask) == utf8_cont_byte_value;
    }

    /*!
     * @brief Determine the UTF-8 seqeunce length from the leading byte.
     * @param b The leading byte of the UTF-8 sequence.
     * @return The UTF-8 sequenece length which is in the range of 1 to 4
     * inclusive if @p is a valid UTF-8 leading byte, and zero if @p b is
     * invalid.
     */
    inline constexpr size_t utf8_sequence_size(uint8_t b) noexcept {
        using namespace detail;
        for (size_t i = 0; i < utf8_masks_values.size(); ++i) {
            if (utf8_masks_values[i].matches(b)) {
                return i + 1;
            }
        }
        return 0;
    }

    /*!
     * @brief Little-endian reader and writer.
     * It deals with potentially unaligned memory.
     */
    struct le_reader_writer_t {
        inline static constexpr std::endian endian = std::endian::little;

        /*!
         * @brief Read a little-endian value from @p p.
         * @tparam T The type of integer to read.
         * @param p The pointer to the memory to read from.
         * @return The result integer in host byte order.
         */
        template <class T>
        static constexpr T read(const T* p) noexcept {
            return detail::read_le_value(p);
        }

        /*!
         * @brief Write a little-endian value to @p p.
         * @tparam T The type of integer to write.
         * @param val The integer to write, in host byte order.
         * @param p The pointer to the memory to write the little-endian value
         * to.
         */
        template <class T>
        static constexpr void write(T val, T* p) noexcept {
            detail::write_le_value(val, p);
        }
    };

    /*!
     * @brief Big-endian reader and writer.
     * It deals with potentially unaligned memory.
     */
    struct be_reader_writer_t {
        inline static constexpr std::endian endian = std::endian::big;

        /*!
         * @brief Read a big-endian value from @p p.
         * @tparam T The type of integer to read.
         * @param p The pointer to the memory to read from.
         * @return The result integer in host byte order.
         */
        template <class T>
        static constexpr T read(const T* p) noexcept {
            return detail::read_be_value(p);
        }

        /*!
         * @brief Write a big-endian value to @p p.
         * @tparam T The type of integer to write.
         * @param val The integer to write, in host byte order.
         * @param p The pointer to the memory to write the big-endian value
         * to.
         */
        template <class T>
        static constexpr void write(T val, T* p) noexcept {
            detail::write_be_value(val, p);
        }
    };

    /*!
     * @brief Read and write integer values in host byte order.
     * It deals with potentially unaligned memory.
     */
    struct native_reader_writer_t {
        inline static constexpr std::endian endian = std::endian::native;

        /*!
         * @brief Read an integer value in host byte order from @p p.
         * @tparam T The type of integer to read.
         * @param p The pointer to the memory to read from.
         * @return The result integer in host byte order.
         */
        template <class T>
        static constexpr T read(const T* p) noexcept {
            return detail::read_native_value(p);
        }

        /*!
         * @brief Write an integer value in host byte order to @p p.
         * @tparam T The type of integer to write.
         * @param val The integer to write, in host byte order.
         * @param p The pointer to the memory to write the integer value in host
         * byte order to.
         */
        template <class T>
        static constexpr void write(T val, T* p) noexcept {
            detail::write_native_value(val, p);
        }
    };

    template <class CodeCvtT>
    concept CodeCvt = requires(typename CodeCvtT::value_type* mp,
                               const typename CodeCvtT::value_type*& cp,
                               codepoint_t c, std::size_t& n, std::errc& ec) {
        { bool{CodeCvtT::supports_all_codepoints} } noexcept;
        // Calculate the number of value_type characters needed to
        // encode a valid unicode code point
        { CodeCvtT::codepoint_size(c) } -> std::same_as<std::size_t>;
        // check if a code point is covered
        { CodeCvtT::is_accepted_codepoint(c) } -> std::same_as<bool>;
        // Attemp to decode a codepoint from buffer { cp, cp + n },
        // on success cp and n are updated, on failure ec is set to
        // error
        { CodeCvtT::decode(cp, n, ec) } noexcept -> std::same_as<codepoint_t>;
        // Attemp to decode a codepoint from an assumed valid
        // encoded buffer { cp, cp
        // + n }, no encoding or size validations are done
        {
            CodeCvtT::decode_unchecked(cp, n)
        } noexcept -> std::same_as<codepoint_t>;
        // Attempt to encode a codepoint in buffer { mp, n }, on
        // success the number of encoded value_type is returned, on
        // failure 0 is returned indicating no buffer space
        {
            CodeCvtT::encode(codepoint_t{}, mp, std::size_t{})
        } noexcept -> std::same_as<std::size_t>;
        // Attempt to encode a codepoint in buffer without size
        // checking
        {
            CodeCvtT::encode_unchecked(c, mp)
        } noexcept -> std::same_as<std::size_t>;
    };

    /*!
     * @brief UTF-8 encoder and decoder.
     */
    struct utf8_codecvt {
        /// The value type of each UTF-8 byte.
        using value_type = uint8_t;

        /// All valid unicode code points are coverd.
        static constexpr bool supports_all_codepoints = true;

        /*!
         * @brief Determine if a type is supported.
         * Supported types are char, char8_t and uint8_t.
         * @tparam T The type to check.
         */
        template <class T>
        inline static constexpr bool is_supported_value =
            std::is_same_v<T, char> || std::is_same_v<T, char8_t> ||
            std::is_same_v<T, uint8_t>;

        /*!
         * @brief The size of UTF-8 BOM mark in bytes.
         */
        inline static constexpr std::size_t bom_size = 3;

        /*!
         * @brief Make a UTF-8 BOM bytes array.
         * @return The UTF-8 BOM bytes array.
         */
        template <class T>
            requires is_supported_value<T>
        static constexpr std::array<T, bom_size> make_bom_array() {
            return std::array<T, bom_size>{
                static_cast<T>(0xef),
                static_cast<T>(0xbb),
                static_cast<T>(0xbf),
            };
        }

        /*!
         * @brief Skip the UTF-8 BOM if it is found at the start of @p p.
         * @param p The pointer to UTF-8 data.
         * If BOM mark is found, this pointer will be advanced by 3 bytes.
         * @param n The count of bytes in buffer pointed to by @p p.
         * If BOM mark is found, this count will be decreased by 3 bytes.
         * @return The endian of UTF-8 data which will always be
         * `std::endian::native` since UTF-8 is endianless.
         */
        template <class T>
            requires is_supported_value<T>
        static constexpr std::endian consume_bom(const T*& p,
                                                 std::size_t& n) noexcept {
            if (n >= 3) {
                constexpr auto bom_mark = make_bom_array<T>();
                std::span<const T, bom_size> in_buff{p, n};
                if (std::equal(in_buff.begin(), in_buff.end(),
                               bom_mark.begin())) {
                    p += bom_size;
                    n -= bom_size;
                }
            }
            return std::endian::native;
        }

        /*!
         * @brief Write UTF-8 BOM mark to buffer pointed to by @p p.
         * @param p The pointer to UTF-8 data.
         * @param n The count of bytes in buffer pointed to by @p p.
         * If @p n is less than 3, no BOM is written and 0 is returned.
         * @return The count of written bytes which will be 3 if n
         * is not less than 3, or 0 if n is less than 3.
         */
        template <class T>
            requires is_supported_value<T>
        static constexpr std::size_t write_bom(T* p, std::size_t n) noexcept {
            if (n < bom_size) {
                return 0;
            }
            constexpr auto bom_mark = make_bom_array<T>();
            std::copy(bom_mark.begin(), bom_mark.end(), p);
            return bom_size;
        }

        /*!
         * @brief Check if a valid code point is accepted.
         * @param cp The code point to check.
         * It must be a valid code point.
         * @return True since all valid unicode code points are coverd.
         */
        static constexpr bool is_accepted_codepoint(codepoint_t cp) noexcept {
            return true;
        }

        /*!
         * @brief Get the size in bytes required to encode a
         * valid code point using UTF-8.
         * @param cp The code point to encode.
         * It must be a valid code point.
         * @return The count of bytes required to encode the
         * code point using UTF-8 which will be from 1 to 4 inclusive.
         */
        static constexpr std::size_t codepoint_size(codepoint_t cp) noexcept {
            assert(is_valid_codepoint(cp));
            if (cp <= detail::utf8_max_1) {
                return 1;
            }
            if (cp <= detail::utf8_max_2) {
                return 2;
            }
            if (cp <= detail::utf8_max_3) {
                return 3;
            }
            assert(cp <= detail::max_utf_codepoint);
            return 4;
        }

        /*!
         * @brief Decode a code point from UTF-8 data at @p p.
         * @param p The pointer to UTF-8 data.
         * On successful decoding, it will be advanced by the amount
         * of bytes consumed.
         * @param n The count of bytes in buffer pointed to by @p p.
         * On successful decoding, this count will be decreased by the amount of
         * bytes consumed.
         * @param ec On sucess, it will be cleared.
         * On invalid UTF-8 sequence, it will be set to
         * `std::errc::illegal_byte_sequence`. Otherwise, it will be set
         * `std::errc::no_buffer_space` if @p n is less than the determined
         * sequence size.
         * @return The decoded and validated code point.
         */
        template <class T>
            requires is_supported_value<T>
        static constexpr codepoint_t decode(const T*& p, std::size_t& n,
                                            std::errc& ec) noexcept {
            if (n == 0) {
                ec = std::errc::no_buffer_space;
                return 0;
            }
            assert(p != nullptr);
            ec = {};

            const size_t seq_len = utf8_sequence_size(static_cast<uint8_t>(*p));
            assert(seq_len <= detail::utf8_masks_values.size());
            if (seq_len == 0) {
                ec = std::errc::illegal_byte_sequence;
                return 0;
            }
            if (n < seq_len) {
                const T* read_p = p + 1; // consume the first byte
                for (size_t i = 0; i < n - 1; ++i) {
                    const uint8_t cont_byte = static_cast<uint8_t>(*read_p);
                    if (!is_utf8_continuation_byte(cont_byte)) {
                        ec = std::errc::illegal_byte_sequence;
                        return 0;
                    }
                    read_p += 1;
                }
                ec = std::errc::no_buffer_space;
                return 0;
            }

            codepoint_t cp = static_cast<uint8_t>(p[0]) &
                             ~detail::utf8_masks_values[seq_len - 1].mask;
            const T* read_p = p + 1; // consume the first byte
            std::size_t read_n = n - 1;
            for (size_t i = 0; i < seq_len - 1; ++i) {
                const uint8_t cont_byte = static_cast<uint8_t>(*read_p);
                if (!is_utf8_continuation_byte(cont_byte)) {
                    ec = std::errc::illegal_byte_sequence;
                    return 0;
                }
                cp <<= detail::utf8_cont_bits_length;
                cp |= cont_byte & ~detail::utf8_cont_byte_mask;
                ++read_p; // consume continuation byte
                --read_n;
            }

            if (!validate_codepoint(cp, ec)) {
                return 0;
            }
            if (codepoint_size(cp) != seq_len) {
                ec = std::errc::illegal_byte_sequence;
                n = read_n;
                return 0;
            }
            p += seq_len;
            n -= seq_len;
            return cp;
        }

        /*!
         * @brief Decode a code point from a previously validated UTF-8 data at
         * @p p.
         * @param p The pointer to UTF-8 data.
         * After decoding, it will be advanced by the amount
         * of bytes consumed.
         * @param n The count of bytes in buffer pointed to by @p p.
         * It can't be less than the determined sequence length.
         * @return The decoded codepoint without validation.
         */
        template <class T>
            requires is_supported_value<T>
        static constexpr codepoint_t decode_unchecked(const T*& p,
                                                      std::size_t& n) noexcept {
            const size_t seq_len = utf8_sequence_size(static_cast<uint8_t>(*p));
            codepoint_t cp = static_cast<uint8_t>(static_cast<uint8_t>(*p)) &
                             ~detail::utf8_masks_values[seq_len - 1].mask;
            ++p; // consume the first byte
            --n;
            for (size_t i = 0; i < seq_len - 1; ++i) {
                const uint8_t cont_byte = static_cast<uint8_t>(*p);
                cp <<= detail::utf8_cont_bits_length;
                cp |= cont_byte & ~detail::utf8_cont_byte_mask;
                ++p; // consume continuation byte
                --n;
            }
            return cp;
        }
        /*!
         * @brief Encode a valid unicode code point using UTF-8.
         * @param cp The code point to encode.
         * It must be a valid code point.
         * @param p The pointer to UTF-8 data.
         * @param n The count of bytes in buffer pointed to by @p p.
         * @return The count of written bytes if @p is not less than
         * the determined sequence size (from 1 to 4 inclusive), or 0
         * if @p n is less than the determined sequence size.
         */
        template <class T>
            requires is_supported_value<T>
        static constexpr std::size_t encode(codepoint_t cp, T* p,
                                            std::size_t n) noexcept {
            assert(is_valid_codepoint(cp));
            if (codepoint_size(cp) > n) {
                return 0;
            }
            return encode_unchecked(cp, p);
        }

        /*!
         * @brief Encode a valid unicode code point using UTF-8.
         * This function is used when the required size for UTF-8
         * encoding is known before the call.
         * @param cp The code point to encode.
         * It must be a valid code point.
         * @param p The pointer to UTF-8 data.
         * The size in bytes of buffer pointed to by @p p must be
         * at least the determined sequence size, or the behavior is undefined.
         * @return The count of written bytes.
         */
        template <class T>
            requires is_supported_value<T>
        static constexpr std::size_t encode_unchecked(codepoint_t cp,
                                                      T* p) noexcept {
            using namespace detail;
            assert(is_valid_codepoint(cp));
            if (cp <= utf8_max_1) {
                *p = static_cast<T>(cp);
                return 1;
            }
            else if (cp <= utf8_max_2) {
                std::array<value_type, 2> encode_buff;
                // 110xxxxx | 10xxxxxx
                encode_buff[1] =
                    static_cast<value_type>(cp & ~utf8_cont_byte_mask);
                encode_buff[1] |= utf8_cont_byte_value;
                cp >>= utf8_cont_bits_length;
                encode_buff[0] =
                    static_cast<value_type>(cp & ~utf8_masks_values[1].mask);
                encode_buff[0] |= utf8_masks_values[1].value;
                p[0] = static_cast<T>(encode_buff[0]);
                p[1] = static_cast<T>(encode_buff[1]);
                return 2;
            }
            else if (cp <= utf8_max_3) {
                std::array<value_type, 3> encode_buff;
                // 1110xxxx | 10xxxxxx | 10xxxxxx
                encode_buff[2] = cp & ~utf8_cont_byte_mask;
                encode_buff[2] |= utf8_cont_byte_value;
                cp >>= utf8_cont_bits_length;
                encode_buff[1] = cp & ~utf8_cont_byte_mask;
                encode_buff[1] |= utf8_cont_byte_value;
                cp >>= utf8_cont_bits_length;
                encode_buff[0] = cp & ~utf8_masks_values[2].mask;
                encode_buff[0] |= utf8_masks_values[2].value;
                p[0] = static_cast<T>(encode_buff[0]);
                p[1] = static_cast<T>(encode_buff[1]);
                p[2] = static_cast<T>(encode_buff[2]);
                return 3;
            }
            else if (cp <= max_utf_codepoint) {
                std::array<value_type, 4> encode_buff;
                // 11110xxx | 10xxxxxx | 10xxxxxx |
                // 10xxxxxx
                encode_buff[3] = cp & ~utf8_cont_byte_mask;
                encode_buff[3] |= utf8_cont_byte_value;
                cp >>= utf8_cont_bits_length;
                encode_buff[2] = cp & ~utf8_cont_byte_mask;
                encode_buff[2] |= utf8_cont_byte_value;
                cp >>= utf8_cont_bits_length;
                encode_buff[1] = cp & ~utf8_cont_byte_mask;
                encode_buff[1] |= utf8_cont_byte_value;
                cp >>= utf8_cont_bits_length;
                encode_buff[0] = cp & ~utf8_masks_values[3].mask;
                encode_buff[0] |= utf8_masks_values[3].value;
                p[0] = static_cast<T>(encode_buff[0]);
                p[1] = static_cast<T>(encode_buff[1]);
                p[2] = static_cast<T>(encode_buff[2]);
                p[3] = static_cast<T>(encode_buff[3]);
                return 4;
            }
            assert(false && "invalid codepoint passed to utf8 "
                            "encode_unchecked");
            return 0;
        }
    };

    /*!
     * @brief UTF-16 encoder and decoder.
     * @tparam ReaderWriter The type of reader and writer
     * which will decide whether to use little or big endian.
     */
    template <class ReaderWriter>
    struct utf16_codecvt_base {
        /// The reader and writer.
        using reader_writer_type = ReaderWriter;
        /// The value type of each UTF-16 unit.
        using value_type = uint16_t;
        /// All valid unicode code points are coverd.
        static constexpr bool supports_all_codepoints = true;

        /*!
         * @brief Determine if a type is supported.
         * Supported types are uint16_t, char16_t and wchar_t.
         * In case of sizeof wchar_t == 4, each wchar_t is
        // treated as if it was char16_t.
         * @tparam T The type to check.
         */
        template <class T>
        inline static constexpr bool is_supported_value =
            std::is_same_v<T, char16_t> || std::is_same_v<T, uint16_t> ||
            std::is_same_v<T, wchar_t>;

        /*!
         * @brief The size of UTF-16 BOM mark in UTF-16 units.
         * It is 1 UTF-16 code unit.
         */
        inline static constexpr std::size_t bom_size = 1;

        /*!
         * @brief The endian used by the encoder and decoder.
         * It decided from the reader and writer template type.
         */
        inline static constexpr std::endian endian = reader_writer_type::endian;

        /*!
         * @brief Skip the UTF-16 BOM if it is found at the start of @p p.
         * @param p The pointer to UTF-16 data.
         * If BOM mark is found, this pointer will be advanced by 1 UTF-16 unit.
         * @param n The count of UTF-16 units in buffer pointed to by @p p.
         * If BOM mark is found, this count will be decreased by 1 UTF-16 unit.
         * @return The determined endian of UTF-16.
         * If n is less than 1, or if no BOM mark is found, the returned
         * endian will be `std::endian::big`.
         */
        static constexpr std::endian consume_bom(const value_type*& p,
                                                 std::size_t& n) noexcept {
            if (n >= 1) {
                const value_type bom = reader_writer_type::read(p);
                constexpr std::array<uint8_t, 2> utf16_be_bom = {0xfe, 0xff};
                constexpr std::array<uint8_t, 2> utf16_le_bom = {0xff, 0xfe};
                std::span<const uint8_t, 2> bom_buff{
                    reinterpret_cast<const uint8_t*>(&bom), 2};
                if (std::equal(bom_buff.begin(), bom_buff.end(),
                               utf16_be_bom.begin())) {
                    p += 1;
                    n -= 1;
                    return std::endian::big;
                }
                else if (std::equal(bom_buff.begin(), bom_buff.end(),
                                    utf16_le_bom.begin())) {
                    p += 1;
                    n -= 1;
                    return std::endian::little;
                }
            }
            return std::endian::big;
        }

        /*!
         * @brief Write UTF-16 BOM mark to buffer pointed to by @p p.
         * The written BOM mark will use the endain of current encoder and
         * decoder.
         * @param p The pointer to UTF-16 data.
         * @param n The count of UTF-16 units in buffer pointed to by @p p.
         * If @p n is less than 1, no BOM is written and 0 is returned.
         * @return The count of written UTF-16 units which will be 1 if n
         * is not less than 1, or 0 if n is less than 1.
         */
        static std::size_t write_bom(value_type* p, std::size_t n) noexcept {
            if (n < bom_size) {
                return 0;
            }
            uint8_t* pu8 = reinterpret_cast<uint8_t*>(p);
            constexpr std::array<uint8_t, 2> utf16_be_bom = {0xfe, 0xff};
            constexpr std::array<uint8_t, 2> utf16_le_bom = {0xff, 0xfe};
            if constexpr (endian == std::endian::little) {
                std::copy(utf16_be_bom.begin(), utf16_be_bom.end(), pu8);
                return bom_size;
            }
            else if constexpr (endian == std::endian::big) {
                std::copy(utf16_le_bom.begin(), utf16_le_bom.end(), pu8);
                return bom_size;
            }
            return 0;
        }

        /*!
         * @brief Check if a valid code point is accepted.
         * @param cp The code point to check.
         * It must be a valid code point.
         * @return True since all valid unicode code points are coverd.
         */
        static constexpr bool is_accepted_codepoint(codepoint_t cp) noexcept {
            return true;
        }

        /*!
         * @brief Get the size in UTF-16 units required to encode a
         * valid code point using UTF-16.
         * @param cp The code point to encode.
         * It must be a valid code point.
         * @return The count of UTF-16 units required to encode the
         * code point using UTF-16 which will be either 1 or 2.
         */
        static constexpr std::size_t codepoint_size(codepoint_t cp) noexcept {
            assert(is_valid_codepoint(cp));
            if (cp <= detail::utf16_bmp_max) {
                return 1;
            }
            assert(cp <= detail::max_utf_codepoint);
            return 2;
        }

        /*!
         * @brief Decode a code point from UTF-16 data at @p p.
         * @param p The pointer to UTF-16 data.
         * On successful decoding, it will be advanced by the amount
         * of UTF-16 units consumed.
         * @param n The count of UTF-16 units in buffer pointed to by @p p.
         * On successful decoding, this count will be decreased by the amount of
         * UTF-16 units consumed.
         * @param ec On sucess, it will be cleared.
         * On invalid UTF-16 sequence, it will be set to
         * `std::errc::illegal_byte_sequence`. Otherwise, it will be set
         * `std::errc::no_buffer_space` if @p n is less than the determined
         * sequence size.
         * @return The decoded and validated code point.
         */
        template <class T>
            requires is_supported_value<T>
        static constexpr codepoint_t decode(const T*& p, std::size_t& n,
                                            std::errc& ec) noexcept {
            using namespace detail;
            if (n < 1) {
                ec = std::errc::no_buffer_space;
                return 0;
            }
            // either w1 has no surrogate or it has high
            // surrogate value, otherwise the codepoint is
            // invalid
            uint16_t w1_16 =
                static_cast<std::uint16_t>(reader_writer_type::read(p));
            // if it start with low surrogate value it is
            // invalid
            if ((w1_16 & utf16_surrogate_value_mask) ==
                utf16_low_surrogate_value) {
                ec = std::errc::illegal_byte_sequence;
                return 0;
            }
            // if it does not start with high or low
            // surrogate value then it is a BMP codepoint
            if ((w1_16 & utf16_surrogate_value_mask) !=
                utf16_high_surrogate_value) {
                ++p;
                --n;
                return w1_16;
            }
            // look for the low surrogate
            if (n < 2) {
                ec = std::errc::no_buffer_space;
                return 0;
            }
            uint16_t w2_16 =
                static_cast<std::uint16_t>(reader_writer_type::read(p + 1));
            if ((w2_16 & utf16_surrogate_value_mask) !=
                utf16_low_surrogate_value) {
                ec = std::errc::illegal_byte_sequence;
                return 0;
            }
            uint32_t w1 = w1_16 & utf16_surrogate_mask;
            uint32_t w2 = w2_16 & utf16_surrogate_mask;
            codepoint_t cp = (w1 << utf16_surrogate_bits_length) | w2;
            cp += utf16_surrogate_offset;
            if (!validate_codepoint(cp, ec)) {
                return 0;
            }
            p += 2;
            n -= 2;
            return cp;
        }

        /*!
         * @brief Decode a code point from a previously validated UTF-16 data at
         * @p p.
         * @param p The pointer to UTF-16 data.
         * After decoding, it will be advanced by the amount
         * of UTF-16 units consumed.
         * @param n The count of UTF-16 units in buffer pointed to by @p p.
         * It can't be less than the determined sequence length.
         * @return The decoded codepoint without validation.
         */
        template <class T>
            requires is_supported_value<T>
        static constexpr codepoint_t decode_unchecked(const T*& p,
                                                      std::size_t& n) noexcept {
            using namespace detail;
            uint16_t w1_16 =
                static_cast<std::uint16_t>(reader_writer_type::read(p));
            if ((w1_16 & ~utf16_surrogate_mask) != utf16_high_surrogate_value) {
                ++p;
                --n;
                return w1_16;
            }
            uint16_t w2_16 =
                static_cast<std::uint16_t>(reader_writer_type::read(p + 1));
            p += 2;
            n -= 2;
            uint32_t w1 = w1_16 & utf16_surrogate_mask;
            uint32_t w2 = w2_16 & utf16_surrogate_mask;
            codepoint_t cp = (w1 << utf16_surrogate_bits_length) | w2;
            cp += utf16_surrogate_offset;
            return cp;
        }

        /*!
         * @brief Encode a valid unicode code point using UTF-16.
         * @param cp The code point to encode.
         * It must be a valid code point.
         * @param p The pointer to UTF-16 data.
         * @param n The count of UTF-16 units in buffer pointed to by @p p.
         * @return The count of written UTF-16 units if @p is not less than
         * the determined sequence size (1 or 2), or 0
         * if @p n is less than the determined sequence size.
         */
        template <class T>
            requires is_supported_value<T>
        static constexpr std::size_t encode(codepoint_t cp, T* p,
                                            std::size_t n) noexcept {
            assert(is_valid_codepoint(cp));
            const std::size_t seq_len = codepoint_size(cp);
            assert(seq_len == 1 || seq_len == 2);
            if (seq_len > n) {
                return 0;
            }
            encode_unchecked(cp, p);
            return seq_len;
        }

        /*!
         * @brief Encode a valid unicode code point using UTF-16.
         * This function is used when the required size for UTF-16
         * encoding is known before the call.
         * @param cp The code point to encode.
         * It must be a valid code point.
         * @param p The pointer to UTF-16 data.
         * The size in UTF-16 units of buffer pointed to by @p p must be
         * at least the determined sequence size, or the behavior is undefined.
         * @return The count of written UTF-16 units.
         */
        template <class T>
            requires is_supported_value<T>
        static constexpr std::size_t encode_unchecked(codepoint_t cp,
                                                      T* p) noexcept {
            using namespace detail;
            assert(is_valid_codepoint(cp));
            if (cp <= utf16_bmp_max) {
                reader_writer_type::write(static_cast<T>(cp), p);
                return 1;
            }
            cp -= utf16_surrogate_offset;
            // if cp is valid it must be now no more than 20
            // bits so no need to mask with
            // utf16_surrogate_bits_length
            assert(cp <= std::numeric_limits<uint32_t>::max() >> 12);
            uint16_t w1 = utf16_high_surrogate_value +
                          (cp >> utf16_surrogate_bits_length);
            uint16_t w2 =
                utf16_low_surrogate_value + (cp & utf16_surrogate_mask);
            reader_writer_type::write(static_cast<T>(w1), p);
            reader_writer_type::write(static_cast<T>(w2), p + 1);
            return 2;
        }
    };

    /*!
     * @brief UTF-32 encoder and decoder.
     * @tparam ReaderWriter The type of reader and writer
     * which will decide whether to use little or big endian.
     */
    template <class ReaderWriter>
    struct utf32_codecvt_base {
        /// The reader and writer.
        using reader_writer_type = ReaderWriter;
        /// The value type of each UTF-16 unit.
        using value_type = std::uint32_t;
        /// All valid unicode code points are coverd.
        static constexpr bool supports_all_codepoints = true;

        /*!
         * @brief Determine if a type is supported.
         * Supported types are uint32_t, char32_t and wchar_t.
         * wchar_t is supported only if sizeof wchar_t == 4.
         * @tparam T The type to check.
         */
        template <class T>
        inline static constexpr bool is_supported_value =
            std::is_same_v<T, char32_t> || std::is_same_v<T, std::uint32_t> ||
            (std::is_same_v<T, wchar_t> && sizeof(wchar_t) == sizeof(char32_t));

        /*!
         * @brief The size of UTF-32 BOM mark in UTF-32 units.
         * It is 1 UTF-32 code unit.
         */
        inline static constexpr std::size_t bom_size = 1;

        /*!
         * @brief The endian used by the encoder and decoder.
         * It decided from the reader and writer template type.
         */
        inline static constexpr std::endian endian = reader_writer_type::endian;

        /*!
         * @brief Skip the UTF-32 BOM if it is found at the start of @p p.
         * @param p The pointer to UTF-32 data.
         * If BOM mark is found, this pointer will be advanced by 1 UTF-32 unit.
         * @param n The count of UTF-32 units in buffer pointed to by @p p.
         * If BOM mark is found, this count will be decreased by 1 UTF-32 unit.
         * @return The determined endian of UTF-32.
         * If n is less than 1, or if no BOM mark is found, the returned
         * endian will be `std::endian::big`.
         */
        static constexpr std::endian consume_bom(const value_type*& p,
                                                 std::size_t& n) noexcept {
            if (n >= 1) {
                const value_type bom = reader_writer_type::read(p);
                constexpr std::array<uint8_t, 4> utf32_be_bom = {0, 0, 0xfe,
                                                                 0xff};
                constexpr std::array<uint8_t, 4> utf32_le_bom = {0xff, 0xfe, 0,
                                                                 0};
                std::span<const uint8_t, 4> bom_buff{
                    reinterpret_cast<const uint8_t*>(&bom), 4};
                if (std::equal(bom_buff.begin(), bom_buff.end(),
                               utf32_be_bom.begin())) {
                    p += 1;
                    n -= 1;
                    return std::endian::big;
                }
                else if (std::equal(bom_buff.begin(), bom_buff.end(),
                                    utf32_le_bom.begin())) {
                    p += 1;
                    n -= 1;
                    return std::endian::little;
                }
            }
            return std::endian::big;
        }

        /*!
         * @brief Write UTF-32 BOM mark to buffer pointed to by @p p.
         * The written BOM mark will use the endain of current encoder and
         * decoder.
         * @param p The pointer to UTF-32 data.
         * @param n The count of UTF-32 units in buffer pointed to by @p p.
         * If @p n is less than 1, no BOM is written and 0 is returned.
         * @return The count of written UTF-32 units which will be 1 if n
         * is not less than 1, or 0 if n is less than 1.
         */
        static constexpr std::size_t write_bom(value_type* p,
                                               std::size_t n) noexcept {
            if (n < bom_size) {
                return 0;
            }
            uint8_t* pu8 = reinterpret_cast<uint8_t*>(p);
            constexpr std::array<uint8_t, 4> utf32_be_bom = {0, 0, 0xfe, 0xff};
            constexpr std::array<uint8_t, 4> utf32_le_bom = {0xff, 0xfe, 0, 0};
            if constexpr (endian == std::endian::little) {
                std::copy(utf32_be_bom.begin(), utf32_be_bom.end(), pu8);
                return bom_size;
            }
            else if constexpr (endian == std::endian::big) {
                std::copy(utf32_le_bom.begin(), utf32_le_bom.end(), pu8);
                return bom_size;
            }
            return 0;
        }

        /*!
         * @brief Check if a valid code point is accepted.
         * @param cp The code point to check.
         * It must be a valid code point.
         * @return True since all valid unicode code points are coverd.
         */
        static constexpr bool is_accepted_codepoint(codepoint_t cp) noexcept {
            return true;
        }

        /*!
         * @brief Get the size in UTF-32 units required to encode a
         * valid code point using UTF-32.
         * @param cp The code point to encode.
         * It must be a valid code point.
         * @return The count of UTF-32 units required to encode the code point
         * using UTF-32 which will be always 1.
         */
        static constexpr std::size_t codepoint_size(codepoint_t cp) {
            assert(is_valid_codepoint(cp));
            return 1;
        }

        /*!
         * @brief Decode a code point from UTF-32 data at @p p.
         * @param p The pointer to UTF-32 data.
         * On successful decoding, it will be advanced by the amount
         * of UTF-32 units consumed.
         * @param n The count of UTF-32 units in buffer pointed to by @p p.
         * On successful decoding, this count will be decreased by the amount of
         * UTF-32 units consumed.
         * @param ec On sucess, it will be cleared.
         * On invalid UTF-32 sequence, it will be set to
         * `std::errc::illegal_byte_sequence`. Otherwise, it will be set
         * `std::errc::no_buffer_space` if @p n is less than 1.
         * @return The decoded and validated code point.
         */
        template <class T>
            requires is_supported_value<T>
        static constexpr codepoint_t decode(const T*& p, std::size_t& n,
                                            std::errc& ec) noexcept {
            if (n < 1) {
                ec = std::errc::no_buffer_space;
                return 0;
            }
            codepoint_t cp = reader_writer_type::read(p);
            if (!validate_codepoint(cp, ec)) {
                return 0;
            }
            ++p;
            --n;
            return cp;
        }

        /*!
         * @brief Decode a code point from a previously validated UTF-32 data at
         * @p p.
         * @param p The pointer to UTF-32 data.
         * After decoding, it will be advanced by the amount
         * of UTF-32 units consumed.
         * @param n The count of UTF-32 units in buffer pointed to by @p p.
         * It can't be less than 1.
         * @return The decoded codepoint without validation.
         */
        template <class T>
            requires is_supported_value<T>
        static constexpr codepoint_t decode_unchecked(const T*& p,
                                                      std::size_t& n) noexcept {
            codepoint_t cp = reader_writer_type::read(p);
            ++p;
            --n;
            return cp;
        }

        /*!
         * @brief Encode a valid unicode code point using UTF-32.
         * @param cp The code point to encode.
         * It must be a valid code point.
         * @param p The pointer to UTF-32 data.
         * @param n The count of UTF-32 units in buffer pointed to by @p p.
         * @return The count of written UTF-32 units if @p is not less than 1,
         * or 0 if @p n is less than 1.
         */
        template <class T>
            requires is_supported_value<T>
        static constexpr std::size_t encode(codepoint_t cp, T* p,
                                            std::size_t n) noexcept {
            assert(is_valid_codepoint(cp));
            if (n < 1) {
                return 0;
            }
            encode_unchecked(cp, p);
            return 1;
        }

        /*!
         * @brief Encode a valid unicode code point using UTF-32.
         * This function is used when the required size for UTF-32
         * encoding is known before the call.
         * @param cp The code point to encode.
         * It must be a valid code point.
         * @param p The pointer to UTF-32 data.
         * The size in UTF-32 units of buffer pointed to by @p p must be
         * at least the determined sequence size, or the behavior is undefined.
         * @return The count of written UTF-32 units.
         */
        template <class T>
            requires is_supported_value<T>
        static constexpr std::size_t encode_unchecked(codepoint_t cp,
                                                      T* p) noexcept {
            assert(is_valid_codepoint(cp));
            reader_writer_type::write(static_cast<T>(cp), p);
            return 1;
        }
    };

    struct cp1256_codecvt {
        using value_type = uint8_t;

        static constexpr bool supports_all_codepoints = false;

        template <class T>
        inline static constexpr bool is_supported_value =
            std::is_same_v<T, char> || std::is_same_v<T, char8_t> ||
            std::is_same_v<T, uint8_t>;

        template <class T>
            requires is_supported_value<T>
        static value_type* cast_pointer(T* p) {
            return reinterpret_cast<value_type*>(p);
        }

        template <class T>
            requires is_supported_value<T>
        static const value_type* cast_pointer(const T* p) {
            return reinterpret_cast<const value_type*>(p);
        }

        static bool is_accepted_codepoint(codepoint_t cp) noexcept {
            return detail::is_valid_cp1256_codepoint(cp);
        }

        static std::size_t codepoint_size(codepoint_t cp) noexcept {
            if (detail::is_valid_cp1256_codepoint(cp)) {
                return 1;
            }
            return 0;
        }

        template <class T>
            requires is_supported_value<T>
        static codepoint_t decode(const T*& p, std::size_t& n,
                                  std::errc& ec) noexcept {
            if (n == 0) {
                ec = std::errc::no_buffer_space;
                return 0;
            }
            assert(p != nullptr);
            ec = {};
            codepoint_t cp =
                detail::map_cp1256_to_unicode(static_cast<uint8_t>(*p));
            n -= 1;
            p += 1;
            return cp;
        }

        template <class T>
            requires is_supported_value<T>
        static codepoint_t decode_unchecked(const T*& p,
                                            std::size_t& n) noexcept {
            assert(n > 0);
            assert(p != nullptr);
            codepoint_t cp =
                detail::map_cp1256_to_unicode(static_cast<uint8_t>(*p));
            n -= 1;
            p += 1;
            return cp;
        }

        template <class T>
            requires is_supported_value<T>
        static std::size_t encode(codepoint_t cp, T* p,
                                  std::size_t n) noexcept {
            assert(is_valid_codepoint(cp));
            if (!detail::is_valid_cp1256_codepoint(cp)) {
                return 0;
            }
            if (codepoint_size(cp) > n) {
                return 0;
            }
            return encode_unchecked(cp, p);
        }

        template <class T>
            requires is_supported_value<T>
        static std::size_t encode_unchecked(codepoint_t cp, T* p) noexcept {
            auto [valid, ch] = detail::map_unicode_to_cp1256(cp);
            assert(valid);
            *p = static_cast<T>(ch);
            return 1;
        }
    };

    /*!
     * @brief UTF-16 encoder and decoder that uses the host byte order.
     */
    using utf16_codecvt = utf16_codecvt_base<native_reader_writer_t>;
    /*!
     * @brief UTF-16 encoder and decoder using little endian.
     */
    using utf16_le_codecvt = utf16_codecvt_base<le_reader_writer_t>;
    /*!
     * @brief UTF-16 encoder and decoder using big endian.
     */
    using utf16_be_codecvt = utf16_codecvt_base<be_reader_writer_t>;
    /*!
     * @brief UTF-32 encoder and decoder that uses the host byte order.
     */
    using utf32_codecvt = utf32_codecvt_base<native_reader_writer_t>;
    /*!
     * @brief UTF-32 encoder and decoder using little endian.
     */
    using utf32_le_codecvt = utf32_codecvt_base<le_reader_writer_t>;
    /*!
     * @brief UTF-32 encoder and decoder using big endian.
     */
    using utf32_be_codecvt = utf32_codecvt_base<be_reader_writer_t>;
    /*!
     * @brief CodeCvt used for `wchar_t`.
     * It is `utf32_codecvt` if `wchar_t` size if 4,
     * otherwise, it is `utf16_codecvt`.
     */
    using wchar_codecvt =
        std::conditional_t<sizeof(wchar_t) == sizeof(uint32_t), utf32_codecvt,
                           utf16_codecvt>;

    static_assert(CodeCvt<utf8_codecvt>);
    static_assert(CodeCvt<utf16_codecvt>);
    static_assert(CodeCvt<utf16_le_codecvt>);
    static_assert(CodeCvt<utf16_be_codecvt>);
    static_assert(CodeCvt<utf32_codecvt>);
    static_assert(CodeCvt<utf32_le_codecvt>);
    static_assert(CodeCvt<utf32_be_codecvt>);
    static_assert(CodeCvt<cp1256_codecvt>);

    /*!
     * @brief Converter that converts from string encoded
     * in one form of UTF to another.
     * @tparam From The input UTF encoder and decoder.
     * @tparam To The output UTF encoder and decoder.
     */
    template <CodeCvt From, CodeCvt To>
    struct utf_converter {
        /// The input UTF encoder and decoder.
        using from_codecvt = From;
        /// The output UTF encoder and decoder.
        using to_codecvt = To;

        /*!
         * @brief Calculate the length in output units required
         * to store all the encoded code points in input.
         * @param from_str The input.
         * @param ec Set to indicate error occured while decoding input, if any.
         * If this function returned with no error, the input can be assumed
         * to be a valid UTF.
         * @return The count of output units required to store all the encoded
         * code points in input.
         */
        template <class T>
            requires(from_codecvt::template is_supported_value<T>)
        static constexpr std::size_t
        calc_length(std::basic_string_view<T> from_str,
                    std::error_code& ec) noexcept {
            ec.clear();
            if (from_str.empty()) {
                return 0;
            }
            const T* p = from_str.data();
            std::size_t n = from_str.size();
            std::size_t len = 0;
            while (n != 0) {
                std::errc ecode{};
                codepoint_t cp = from_codecvt::decode(p, n, ecode);
                if (ecode != std::errc{}) {
                    ec = std::make_error_code(ecode);
                    return 0;
                }
                len += to_codecvt::codepoint_size(cp);
            }
            return len;
        }

        template <class T>
            requires(from_codecvt::template is_supported_value<T>)
        static constexpr std::size_t
        calc_length_replace(std::basic_string_view<T> from_str) {
            if (from_str.empty()) {
                return 0;
            }
            const T* p = from_str.data();
            std::size_t n = from_str.size();
            std::size_t len = 0;
            while (n != 0) {
                std::errc ecode{};
                codepoint_t cp = from_codecvt::decode(p, n, ecode);
                if (ecode != std::errc{}) {
                    cp = detail::replacement_codepoint;
                    constexpr std::size_t replace_n =
                        from_codecvt::codepoint_size(
                            detail::replacement_codepoint);
                    p += replace_n;
                    n -= std::min(replace_n, n);
                }
                len += to_codecvt::codepoint_size(cp);
            }
            return len;
        }

        template <class T1, class T2>
            requires(from_codecvt::template is_supported_value<T1> &&
                     to_codecvt::template is_supported_value<T2>)
        static constexpr std::size_t
        convert_replace(std::basic_string_view<T1> from_str, std::span<T2> to,
                        std::error_code& ec) noexcept {
            assert(!from_str.empty());
            const T1* from_p = from_str.data();
            std::size_t from_n = from_str.size();
            T2* to_p = to.data();
            while (from_n != 0) {
                std::errc ecode;
                codepoint_t cp = from_codecvt::decode(from_p, from_n, ecode);
                if (ecode != std::errc{}) {
                    cp = detail::replacement_codepoint;
                    constexpr std::size_t replace_n =
                        from_codecvt::codepoint_size(
                            detail::replacement_codepoint);
                    from_p += replace_n;
                    from_n -= std::min(replace_n, from_n);
                }
                if (!to_codecvt::is_accepted_codepoint(cp)) {
                    ec = std::make_error_code(std::errc::illegal_byte_sequence);
                    return 0;
                }
                const std::size_t n = to_codecvt::encode_unchecked(cp, to_p);
                assert(n > 0);
                to_p += n;
            }
            return static_cast<std::size_t>(reinterpret_cast<T2*>(to_p) -
                                            to.data());
        }

        template <class T1, class T2>
            requires(from_codecvt::template is_supported_value<T1> &&
                     to_codecvt::template is_supported_value<T2> &&
                     to_codecvt::supports_all_codepoints)
        static constexpr std::size_t
        convert_replace(std::basic_string_view<T1> from_str,
                        std::span<T2> to) noexcept {
            assert(!from_str.empty());
            const T1* from_p = from_str.data();
            std::size_t from_n = from_str.size();
            T2* to_p = to.data();
            while (from_n != 0) {
                std::errc ecode;
                codepoint_t cp = from_codecvt::decode(from_p, from_n, ecode);
                if (ecode != std::errc{}) {
                    cp = detail::replacement_codepoint;
                    constexpr std::size_t replace_n =
                        from_codecvt::codepoint_size(
                            detail::replacement_codepoint);
                    from_p += replace_n;
                    from_n -= std::min(replace_n, from_n);
                }
                const std::size_t n = to_codecvt::encode_unchecked(cp, to_p);
                assert(n > 0);
                to_p += n;
            }
            return static_cast<std::size_t>(reinterpret_cast<T2*>(to_p) -
                                            to.data());
        }

        /*!
         * @brief Convert from string encoded in one form of UTF to another.
         * @param from_str The input.
         * @param to The output.
         * @param ec Set to indicate error occured while decoding input, if any.
         * If output is not large enough to encode all the code points,
         * then @p ec will be set to `std::errc::no_buffer_space`.
         */
        template <class T1, class T2>
            requires(from_codecvt::template is_supported_value<T1>)
        static constexpr void convert(std::basic_string_view<T1> from_str,
                                      std::span<T2> to,
                                      std::error_code& ec) noexcept {
            ec.clear();
            if (from_str.empty()) {
                return;
            }
            const T1* from_p = from_str.data();
            std::size_t from_n = from_str.size();
            T2* to_p = to.data();
            std::size_t to_n = to.size();
            while (from_n != 0) {
                std::errc ecode{};
                codepoint_t cp = from_codecvt::decode(from_p, from_n, ecode);
                if (ecode != std::errc{}) {
                    ec = std::make_error_code(ecode);
                    return;
                }
                if (!to_codecvt::is_accepted_codepoint(cp)) {
                    ec = std::make_error_code(std::errc::illegal_byte_sequence);
                    return;
                }
                std::size_t n = to_codecvt::encode(cp, to_p, to_n);
                if (!n) {
                    ec = std::make_error_code(std::errc::no_buffer_space);
                    return;
                }
                to_p += n;
                to_n -= n;
            }
        }

        /*!
         * @brief Convert from string encoded in one form of UTF to another.
         * @param from_str The input.
         * @param to The output. It must be large enough to encode all the input
         * code points, or the behavior is undefined.
         */
        template <class T1, class T2>
            requires(from_codecvt::template is_supported_value<T1> &&
                     to_codecvt::template is_supported_value<T2> &&
                     to_codecvt::supports_all_codepoints)
        static constexpr std::size_t
        convert(std::basic_string_view<T1> from_str,
                std::span<T2> to) noexcept {
            assert(!from_str.empty());
            const T1* from_p = from_str.data();
            std::size_t from_n = from_str.size();
            T2* to_p = to.data();
            while (from_n != 0) {
                const codepoint_t cp =
                    from_codecvt::decode_unchecked(from_p, from_n);
                const std::size_t n = to_codecvt::encode_unchecked(cp, to_p);
                assert(n > 0);
                to_p += n;
            }
            return static_cast<std::size_t>(reinterpret_cast<T2*>(to_p) -
                                            to.data());
        }

        /*!
         * @brief Convert from string encoded in one form of UTF to another.
         * @param from_str The input.
         * @param to The output. It will be resized to append
         * the encoded code points as needed.
         * @param ec Set to indicate error occured while decoding input, if any.
         * @return The count of output code units written to the output.
         */
        template <class T1, class Traits1, class T2, class Traits2, class Alloc>
            requires(from_codecvt::template is_supported_value<T1> &&
                     to_codecvt::template is_supported_value<T2>)
        static std::size_t convert(std::basic_string_view<T1, Traits1> from_str,
                                   std::basic_string<T2, Traits2, Alloc>& to,
                                   std::error_code& ec) {
            ec.clear();
            if (from_str.empty()) {
                return 0;
            }
            std::size_t to_required_len = calc_length(from_str, ec);
            if (ec) {
                return 0;
            }
            const std::size_t index = to.size();
            to.resize(index + to_required_len);
            std::size_t encoded_len = 0;
            if constexpr (to_codecvt::supports_all_codepoints) {
                encoded_len = convert(from_str, std::span{to}.subspan(index));
            }
            else {
                encoded_len =
                    convert(from_str, std::span{to}.subspan(index), ec);
                if (ec) {
                    to.resize(index);
                    return 0;
                }
            }
            assert(encoded_len == to_required_len);
            return encoded_len;
        }

        template <class T1, class Traits1, class T2, class Traits2, class Alloc>
            requires(from_codecvt::template is_supported_value<T1> &&
                     to_codecvt::template is_supported_value<T2>)
        static std::size_t
        convert_replace(std::basic_string_view<T1, Traits1> from_str,
                        std::basic_string<T2, Traits2, Alloc>& to,
                        std::error_code& ec) {
            if (from_str.empty()) {
                return 0;
            }
            std::size_t to_required_len = calc_length_replace(from_str);
            assert(to_required_len != 0);
            if (!to_required_len) {
                return 0;
            }
            const std::size_t index = to.size();
            to.resize(index + to_required_len);
            std::size_t encoded_len = 0;
            if constexpr (to_codecvt::supports_all_codepoints) {
                encoded_len =
                    convert_replace(from_str, std::span{to}.subspan(index));
            }
            else {
                std::error_code ec;
                encoded_len =
                    convert_replace(from_str, std::span{to}.subspan(index), ec);
                if (ec) {
                    to.resize(index);
                    return 0;
                }
            }
            assert(encoded_len == to_required_len);
            return encoded_len;
        }
    };

    /*!
     * @brief Converts UTF-8 string to wide string. Depending on the size of
     * wchar_t this may be a UTF-16 or UTF-32 to conversion. The result
     * UTF-16 or UTF-32 string is in the host native byte order
     * @param from string encoded in UTF-8 to convert. This string is
     * validated before conversion happens
     * @param to wide string which the result UTF-16 or UTF-32 will be
     * appended to.
     */
    RAD_EXPORT_DECL void string_to_wstring(std::string_view from,
                                           std::wstring& to);

    /*!
     * @brief Converts wide string to UTF-8 string. Depending on the size of
     * wchar_t this may be a UTF-16 or UTF-32 from conversion. The input
     * UTF-16 or UTF-32 string is in the host native byte order
     * @param from wide string encoded in UTF-16 or UTF-32 to convert.
     * This string is validated before conversion happens
     * @param to string which the result UTF-8 will be appended to.
     */
    RAD_EXPORT_DECL void wstring_to_string(std::wstring_view from,
                                           std::string& to);

    /*!
     * @brief Converts UTF-8 string to UTF-16 string. The result UTF-16
     * string is in the host native byte order
     * @param from string encoded in UTF-8 to convert. This string is
     * validated before conversion happens
     * @param to string which the result UTF-16 will be appended to.
     */
    RAD_EXPORT_DECL void string_to_u16string(std::string_view from,
                                             std::u16string& to);

    /*!
     * @brief Converts UTF-16 string to UTF-8 string. The input UTF-16
     * string is assumed to be in the host native byte order
     * @param from string encoded in UTF-16 to convert. This string is
     * validated before conversion happens
     * @param to string which the result UTF-8 will be appended to.
     */
    RAD_EXPORT_DECL void u16string_to_string(std::u16string_view from,
                                             std::string& to);

    /*!
     * @brief Converts UTF-8 string to UTF-32 string. The result UTF-32
     * string is in the host native byte order
     * @param from string encoded in UTF-8 to convert. This string is
     * validated before conversion happens
     * @param to string which the result UTF-32 will be appended to.
     */
    RAD_EXPORT_DECL void string_to_u32string(std::string_view from,
                                             std::u32string& to);

    /*!
     * @brief Converts UTF-32 string to UTF-8 string. The input UTF-32
     * string is assumed to be in the host native byte order.
     * @param from string encoded in UTF-32 to convert. This string is
     * validated before conversion happens.
     * @param to string which the result UTF-8 will be appended to.
     */
    RAD_EXPORT_DECL void u32string_to_string(std::u32string_view from,
                                             std::string& to);

    /*!
     * @brief Converts UTF-16 string to UTF-32 string. The input UTF-16
     * and output UTF-32 strings are assumed to be in the host native byte
     * order.
     * @param from string encoded in UTF-16 to convert. This string is
     * validated before conversion happens.
     * @param to string which the result UTF-32 will be appended to.
     */
    RAD_EXPORT_DECL void u16string_to_u32string(std::u16string_view from,
                                                std::u32string& to);

    /*!
     * @brief Converts UTF-32 string to UTF-16 string. The input UTF-32
     * and output UTF-16 strings are assumed to be in the host native byte
     * order.
     * @param from string encoded in UTF-32 to convert. This string is
     * validated before conversion happens.
     * @param to string which the result UTF-16 will be appended to.
     */
    RAD_EXPORT_DECL void u32string_to_u16string(std::u32string_view from,
                                                std::u16string& to);

    /*!
     * @brief Converts wide string to UTF-16 string. Depending on the size
     * of wchar_t this may be a UTF-16 or UTF-32 from conversion. The input
     * UTF-16 or UTF-32 string is in the host native byte order
     * @param from wide string encoded in UTF-16 or UTF-32 to convert.
     * This string is validated before conversion happens
     * @param to string which the result UTF-16 will be appended to.
     */
    RAD_EXPORT_DECL void wstring_to_u16string(std::wstring_view from,
                                              std::u16string& to);

    /*!
     * @brief Converts UTF-16 string to wide string. The input UTF-16 string
     * is assumed to be in the host native byte order. The result UTF-16 or
     * UTF-32 string is in the host native byte order
     * @param from string encoded in UTF-16 to convert. This string is
     * validated before conversion happens
     * @param to string which the result wide string will be appended to.
     */
    RAD_EXPORT_DECL void u16string_to_wstring(std::u16string_view from,
                                              std::wstring& to);

    /*!
     * @brief Converts wide string to UTF-32 string. Depending on the size
     * of wchar_t this may be a UTF-16 or UTF-32 from conversion. The input
     * UTF-16 or UTF-32 string is in the host native byte order
     * @param from wide string encoded in UTF-16 or UTF-32 to convert.
     * This string is validated before conversion happens
     * @param to string which the result UTF-32 will be appended to.
     */
    RAD_EXPORT_DECL void wstring_to_u32string(std::wstring_view from,
                                              std::u32string& to);

    /*!
     * @brief Converts UTF-32 string to wide string. The input UTF-32 string
     * is assumed to be in the host native byte order. The result UTF-16 or
     * UTF-32 string is in the host native byte order
     * @param from string encoded in UTF-32 to convert. This string is
     * validated before conversion happens
     * @param to string which the result wide string will be appended to.
     */
    RAD_EXPORT_DECL void u32string_to_wstring(std::u32string_view from,
                                              std::wstring& to);

    /*!
     * @brief Convert windows-1256 encoded string @p from, to UTF-8 encoded
     * string.
     * @param from The input windows-1256 encoded string.
     * @param to The output UTF-8 encoded string.
     */
    RAD_EXPORT_DECL void cp1256_to_utf8(std::string_view from, std::string& to);
} // namespace RAD_LIB_NAMESPACE