#pragma once
#include <rad/json/value.h>

#include <ostream>

namespace rad::json {

    /*!
     * @brief Serialize options.
     * This structure is used for specifying whether to allow non-standard
     * extensions. Default-constructed options specify that only standard
     * JSON is produced.
     */
    struct serialize_options {
        /*!
         * @brief Output Infinity, -Infinity and NaN for
         * positive infinity, negative infinity, and "not a
         * number" doubles
         */
        bool allow_infinity_and_nan = false;
        /*!
         * @brief Output escaped '\\/' for '/' which may be
         * useful when included in scripts
         */
        bool escape_solidus = false;
    };

    /*!
     * @brief A serializer for JSON.
     * This class traverses an instance of a JSON type (value, object,
     * array, string, ...) and emits serialized JSON text by filling in one
     * or more caller-provided buffers. To use, declare a variable and call
     * reset with a reference to the JSON variable you want to serialize.
     * Then call read over and over until done returns true.
     */
    class serializer {
        struct null_value_entry {
            uint32_t written = 0;
        };

        struct empty_array_entry {
            uint32_t written = 0;
        };

        struct empty_object_entry {
            uint32_t written = 0;
        };

        struct bool_value_entry {
            bool b = false;
            uint32_t written = 0;
        };

        struct int64_value_entry {
            std::string_view text;
        };

        struct uint64_value_entry {
            std::string_view text;
        };

        struct double_value_entry {
            std::string_view text;
        };

        struct string_value_entry {
            std::string_view s;
            uint8_t quotes = 0; // " + " = 2
            uint32_t escape_len = 0;
        };

        struct key_entry {
            std::string_view s;
            uint32_t escape_len = 0;
            uint8_t quotes = 0; // " + " + : + ' ' = 4
        };

        struct array_value_entry {
            array::const_iterator current;
            array::const_iterator end;
            bool first_bracket = false;
        };

        struct comma_space_entry {
            uint32_t written = 0;
        };

        struct object_value_entry {
            object::const_iterator current;
            object::const_iterator end;
            bool first_bracket = false;
        };

        using item_type =
            std::variant<null_value_entry, bool_value_entry, int64_value_entry,
                         uint64_value_entry, double_value_entry,
                         string_value_entry, key_entry, empty_array_entry,
                         array_value_entry, empty_object_entry,
                         object_value_entry, comma_space_entry>;

    public:
        /*!
         * @brief The serializer is constructed with no value to
         * serialize. The value may be set later by calling @ref
         * reset. If serialization is attempted with no value,
         * the output is as if a null value is serialized.
         * @param opts The options for the serializer
         */
        RAD_EXPORT_DECL
        serializer(const serialize_options& opts = {});

        /*!
         * @brief Prepares the serializer to emit a new
         * serialized JSON representing its argument @p v.
         * Ownership is not transferred. If the value is a non
         * empty object, array or string, the caller is
         * responsible for ensuring that the lifetime of the
         * object pointed to by the argument extends until it is
         * no longer needed. Any memory internally allocated for
         * previous uses of this serializer object is preserved
         * and re-used for the new output.
         * @param v A reference to the value to serialize
         */
        RAD_EXPORT_DECL void reset(const value& v);

        /*!
         * @brief Prepares the serializer to emit a new
         * serialized JSON representing its argument @p v.
         * Ownership is not transferred. If the object is not
         * empty, the caller is responsible for ensuring that
         * the lifetime of the object pointed to by the argument
         * extends until it is no longer needed. Any memory
         * internally allocated for previous uses of this
         * serializer object is preserved and re-used for the
         * new output.
         * @param v A reference to the object to serialize
         */
        RAD_EXPORT_DECL void reset(const object& v);

        /*!
         * @brief  Prepares the serializer to emit a new
         * serialized empty JSON object. Any memory internally
         * allocated for previous uses of this serializer object
         * is preserved and re-used for the new output.
         */
        void reset(object_kind_t) {
            object o;
            reset(o);
        }

        /*!
         * @brief Deleted to prevent passing temporary object
         */
        void reset(object&&) = delete;

        /*!
         * @brief Prepares the serializer to emit a new
         * serialized JSON representing its argument @p v.
         * Ownership is not transferred. If the array is not
         * empty, the caller is responsible for ensuring that
         * the lifetime of the array pointed to by the argument
         * extends until it is no longer needed. Any memory
         * internally allocated for previous uses of this
         * serializer object is preserved and re-used for the
         * new output.
         * @param v A reference to the array to serialize
         */
        RAD_EXPORT_DECL void reset(const array& v);

        /*!
         * @brief  Prepares the serializer to emit a new
         * serialized empty JSON array. Any memory internally
         * allocated for previous uses of this serializer object
         * is preserved and re-used for the new output.
         */
        void reset(array_kind_t) {
            array a;
            reset(a);
        }

        /*!
         * @brief Deleted to prevent passing temporary array
         */
        void reset(array&&) = delete;

        /*!
         * @brief Prepares the serializer to emit a new
         * serialized JSON representing the string value @p v.
         * Ownership is not transferred. The caller is
         * responsible for ensuring that the lifetime of the
         * object pointed to by the argument extends until it is
         * no longer needed. Any memory internally allocated for
         * previous uses of this serializer object is preserved
         * and re-used for the new output.
         * @param v The string value to serialize
         */
        RAD_EXPORT_DECL void reset(std::string_view v);

        /*!
         * @brief Prepares the serializer to emit a new
         * serialized JSON representing the string value @p v.
         * Ownership is not transferred. The caller is
         * responsible for ensuring that the lifetime of the
         * object pointed to by the argument extends until it is
         * no longer needed. Any memory internally allocated for
         * previous uses of this serializer object is preserved
         * and re-used for the new output.
         * @param v A pointer to the null terminated string
         * value to serialize
         */
        void reset(const char* v) {
            reset(std::string_view{v});
        }

        /*!
         * @brief Prepares the serializer to emit a new
         * serialized JSON representing the string value @p v.
         * Ownership is not transferred. The caller is
         * responsible for ensuring that the lifetime of the
         * object pointed to by the argument extends until it is
         * no longer needed. Any memory internally allocated for
         * previous uses of this serializer object is preserved
         * and re-used for the new output.
         * @param v A pointer to the string value to serialize
         * @param n Count of bytes in the string buffer pointed
         * to by @p
         * v
         */
        void reset(const char* v, std::size_t n) {
            reset(std::string_view{v, n});
        }

        /*!
         * @brief Prepares the serializer to emit a new
         * serialized JSON representing the string value @p v.
         * Ownership is not transferred. The caller is
         * responsible for ensuring that the lifetime of the
         * object pointed to by the argument extends until it is
         * no longer needed. Any memory internally allocated for
         * previous uses of this serializer object is preserved
         * and re-used for the new output.
         * @param v The string value to serialize
         */
        void reset(const std::string& v) {
            reset(std::string_view{v});
        }

        /*!
         * @brief Deleted to prevent passing temporary
         * std::string
         */
        void reset(std::string&&) = delete;

        /*!
         * @brief Prepares the serializer to emit a new
         * serialized JSON representing null value. Any memory
         * internally allocated for previous uses of this
         * serializer object is preserved and re-used for the
         * new output.
         */
        RAD_EXPORT_DECL void reset(std::nullptr_t);

        /*!
         * @brief Prepares the serializer to emit a new
         * serialized JSON representing the bool value @p v. Any
         * memory internally allocated for previous uses of this
         * serializer object is preserved and re-used for the
         * new output.
         * @param v The bool value to serialize
         */
        RAD_EXPORT_DECL void reset(bool v);

        /*!
         * @brief Prepares the serializer to emit a new
         * serialized JSON representing the double value @p v.
         * Any memory internally allocated for previous uses of
         * this serializer object is preserved and re-used for
         * the new output.
         * @param v The double value to serialize
         */
        RAD_EXPORT_DECL void reset(double v);

        /*!
         * @brief Prepares the serializer to emit a new
         * serialized JSON representing the std::int64_t value
         * @p v. Any memory internally allocated for previous
         * uses of this serializer object is preserved and
         * re-used for the new output.
         * @param v The std::int64_t value to serialize
         */
        RAD_EXPORT_DECL void reset(std::int64_t v);

        /*!
         * @brief Prepares the serializer to emit a new
         * serialized JSON representing the std::uint64_t value
         * @p v. Any memory internally allocated for previous
         * uses of this serializer object is preserved and
         * re-used for the new output.
         * @param v The std::uint64_t value to serialize
         */
        RAD_EXPORT_DECL void reset(std::uint64_t v);

        /*!
         * @brief Prepares the serializer to emit a new
         * serialized JSON representing the std::int8_t value @p
         * v. Any memory internally allocated for previous uses
         * of this serializer object is preserved and re-used
         * for the new output.
         * @param v The std::int8_t value to serialize
         */
        void reset(std::int8_t v) {
            reset(static_cast<std::int64_t>(v));
        }

        /*!
         * @brief Prepares the serializer to emit a new
         * serialized JSON representing the std::int16_t value
         * @p v. Any memory internally allocated for previous
         * uses of this serializer object is preserved and
         * re-used for the new output.
         * @param v The std::int16_t value to serialize
         */
        void reset(std::int16_t v) {
            reset(static_cast<std::int64_t>(v));
        }

        /*!
         * @brief Prepares the serializer to emit a new
         * serialized JSON representing the std::int32_t value
         * @p v. Any memory internally allocated for previous
         * uses of this serializer object is preserved and
         * re-used for the new output.
         * @param v The std::int32_t value to serialize
         */
        void reset(std::int32_t v) {
            reset(static_cast<std::int64_t>(v));
        }

        /*!
         * @brief Prepares the serializer to emit a new
         * serialized JSON representing the std::uint8_t value
         * @p v. Any memory internally allocated for previous
         * uses of this serializer object is preserved and
         * re-used for the new output.
         * @param v The std::uint8_t value to serialize
         */
        void reset(std::uint8_t v) {
            reset(static_cast<std::uint64_t>(v));
        }

        /*!
         * @brief Prepares the serializer to emit a new
         * serialized JSON representing the std::uint16_t value
         * @p v. Any memory internally allocated for previous
         * uses of this serializer object is preserved and
         * re-used for the new output.
         * @param v The std::uint16_t value to serialize
         */
        void reset(std::uint16_t v) {
            reset(static_cast<std::uint64_t>(v));
        }

        /*!
         * @brief Prepares the serializer to emit a new
         * serialized JSON representing the std::uint32_t value
         * @p v. Any memory internally allocated for previous
         * uses of this serializer object is preserved and
         * re-used for the new output.
         * @param v The std::uint32_t value to serialize
         */
        void reset(std::uint32_t v) {
            reset(static_cast<std::uint64_t>(v));
        }

        /*!
         * @brief Returns true when all of the characters in the
         * serialized representation of the value have been
         * read.
         * @return True if the serialization is complete and
         * false otherwise.
         */
        bool done() const noexcept {
            return stack_.empty();
        }

        /*!
         * @brief Attempts to fill the caller provided buffer
         * starting at @p buff with up to @p n characters of the
         * serialized JSON that represents the value. If the
         * buffer is not large enough, multiple calls may be
         * required. If serialization completes during this
         * call; that is, that all of the characters belonging
         * to the serialized value have been written to
         * caller-provided buffers, the function done will
         * return true.
         * @param buff A pointer to storage to write into.
         * @param n The maximum number of characters to write to
         * the memory pointed to by @p buff.
         * @return A std::string_view containing the characters
         * written, which may be less than @p n.
         */
        std::string_view read(char* buff, std::size_t n) {
            return std::string_view{buff, read_into(buff, n)};
        }

    private:
        RAD_EXPORT_DECL std::size_t read_into(char* buff, std::size_t n);

        int64_value_entry make_int64_value_entry(std::int64_t i);

        uint64_value_entry make_uint64_value_entry(std::uint64_t u);

        double_value_entry make_double_value_entry(double d);

        item_type make_item_type_from_value(const value& v);

        std::size_t serialize_object(char* buff, std::size_t n,
                                     object_value_entry& e);

        std::size_t serialize_array(char* buff, std::size_t n,
                                    array_value_entry& e);

        std::size_t write_piece_of_string(char* buff, std::size_t n,
                                          uint32_t& written,
                                          std::string_view text,
                                          bool pop_stack = true);

        std::size_t write_piece_of_string(char* buff, std::size_t n,
                                          std::string_view& text,
                                          bool pop_stack = true);

        std::size_t serialize_i64(char* buff, std::size_t n, uint32_t& written,
                                  int64_t i);

        std::size_t serialize_u64(char* buff, std::size_t n, uint32_t& written,
                                  uint64_t i);

        std::size_t write_escaped_string(char* buff, std::size_t n,
                                         std::string_view& text,
                                         uint32_t& escape_len);

        std::size_t write_quoted_string(char* buff, std::size_t n,
                                        string_value_entry& e);

        std::size_t write_quoted_key(char* buff, std::size_t n, key_entry& e);

        std::vector<item_type> stack_;
        serialize_options opts_;
        std::string temp_text_;
    };

    /*!
     * @brief This function serializes a JSON value and returns it as a
     * std::string.
     * @param v A reference to the value to serialize
     * @param opts The options for the serializer
     * @return The serialized string
     */
    RAD_EXPORT_DECL std::string serialize(const value& v,
                                          const serialize_options& opts = {});

    /*!
     * @brief This function serializes a JSON object and returns it as a
     * std::string.
     * @param o A reference to the object to serialize
     * @param opts The options for the serializer
     * @return The serialized string
     */
    RAD_EXPORT_DECL std::string serialize(const object& o,
                                          const serialize_options& opts = {});

    /*!
     * @brief This function serializes a JSON array and returns it as a
     * std::string.
     * @param a A reference to the array to serialize
     * @param opts The options for the serializer
     * @return The serialized string
     */
    RAD_EXPORT_DECL std::string serialize(const array& a,
                                          const serialize_options& opts = {});

    /*!
     * @brief This function serializes a JSON string and returns it as a
     * std::string.
     * @param s A reference to the string to serialize
     * @param opts The options for the serializer
     * @return The serialized string
     */
    RAD_EXPORT_DECL std::string serialize(const std::string& s,
                                          const serialize_options& opts = {});

    /*!
     * @brief This function serializes a JSON string and returns it as a
     * std::string.
     * @param s A view to the string to serialize
     * @param opts The options for the serializer
     * @return The serialized string
     */
    RAD_EXPORT_DECL std::string serialize(std::string_view s,
                                          const serialize_options& opts = {});

    /*!
     * @brief This function serializes a JSON string and returns it as a
     * std::string.
     * @param s A pointer to the string to serialize.
     * @param n Count of bytes in the string @p s.
     * @param opts The options for the serializer.
     * @return The serialized string.
     */
    RAD_EXPORT_DECL std::string serialize(const char* s, std::size_t n,
                                          const serialize_options& opts = {});

    /*!
     * @brief This function serializes a JSON unsigned integer and returns
     * it as a std::string.
     * @param u An unsigned integer to serialize.
     * @param opts The options for the serializer.
     * @return The serialized string.
     */
    RAD_EXPORT_DECL std::string serialize(std::uint64_t u,
                                          const serialize_options& opts = {});

    /*!
     * @brief This function serializes a JSON signed integer and returns it
     * as a std::string.
     * @param i An signed integer to serialize.
     * @param opts The options for the serializer.
     * @return The serialized string.
     */
    RAD_EXPORT_DECL std::string serialize(std::int64_t i,
                                          const serialize_options& opts = {});

    /*!
     * @brief This function serializes a JSON double and returns it as a
     * std::string.
     * @param d A double to serialize.
     * @param opts The options for the serializer.
     * @return The serialized string.
     */
    RAD_EXPORT_DECL std::string serialize(double d,
                                          const serialize_options& opts = {});

    /*!
     * @brief This function serializes a JSON boolean and returns it as a
     * std::string.
     * @param b A boolean to serialize.
     * @param opts The options for the serializer.
     * @return The serialized string.
     */
    RAD_EXPORT_DECL std::string serialize(bool b,
                                          const serialize_options& opts = {});

    /*!
     * @brief This function serializes a JSON null and returns it as a
     * std::string.
     * @param opts The options for the serializer.
     * @return The serialized string.
     */
    RAD_EXPORT_DECL std::string serialize(std::nullptr_t,
                                          const serialize_options& opts = {});
} // namespace rad::json