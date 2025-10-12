#pragma once
#include <rad/string.h>

#include <array>
#include <memory>
#include <optional>
#include <span>
#include <vector>

namespace RAD_LIB_NAMESPACE::detail::cli {
    struct argv_reader {
        virtual ~argv_reader() = default;

        virtual bool finished() const noexcept = 0;

        virtual std::string_view read() = 0;
    };
} // namespace RAD_LIB_NAMESPACE::detail::cli

namespace RAD_LIB_NAMESPACE::cli {
    template <class T>
    struct argument_parser;

    /*!
     * @brief CLI Parsable types T are types which have a specialization of
     * argument_parser<T> and the specialization provides the following:
     *
     * static constexpr bool many_values;
     *
     * bool parse(std::string_view argument, T& parsed, char separator)
     *
     * void reset(T& value)
     */
    template <class T>
    concept Parsable =
        requires(argument_parser<T>& parser, std::string_view argument, T& v,
                 char separator) {
            requires std::is_default_constructible_v<argument_parser<T>>;
            { parser.parse(argument, v, separator) } -> std::same_as<bool>;
            { parser.reset(v) } -> std::same_as<void>;
        };

    template <std::integral T>
    struct argument_parser<T> {
        static constexpr bool many_values = false;

        bool parse(std::string_view argument, T& parsed,
                   char = 0) const noexcept {
            std::error_code ec;
            T i = to_numeric<T>(argument, 10, ec);
            if (ec) {
                return false;
            }
            parsed = i;
            return true;
        }

        void reset(T& value) noexcept {
            value = 0;
        }
    };

#if __cpp_lib_to_chars >= 201611L
    template <std::floating_point T>
    struct argument_parser<T> {
        static constexpr bool many_values = false;

        bool parse(std::string_view argument, T& parsed,
                   char = 0) const noexcept {
            std::error_code ec;
            T i =
                to_floating_point<T>(argument, std::chars_format::general, ec);
            if (ec) {
                return false;
            }
            parsed = i;
            return true;
        }

        void reset(T& value) noexcept {
            value = 0;
        }
    };
#endif // __cpp_lib_to_chars >= 201611L

    template <>
    struct argument_parser<bool> {
        static constexpr bool many_values = false;

        bool parse(std::string_view argument, bool& parsed,
                   char = 0) const noexcept {
            if (argument == "true") {
                parsed = true;
            }
            else if (argument == "false") {
                parsed = false;
            }
            else {
                return false;
            }
            return true;
        }

        void reset(bool& value) noexcept {
            value = false;
        }
    };

    template <>
    struct argument_parser<std::string> {
        static constexpr bool many_values = false;

        bool parse(std::string_view argument, std::string& parsed,
                   char = 0) const {
            parsed = argument;
            return true;
        }

        void reset(std::string& value) noexcept {
            value.clear();
        }
    };

    template <>
    struct argument_parser<std::wstring> {
        static constexpr bool many_values = false;

        bool parse(std::string_view argument, std::wstring& parsed,
                   char = 0) const {
            parsed.clear();
            to_wstring(argument, parsed);
            return true;
        }

        void reset(std::wstring& value) noexcept {
            value.clear();
        }
    };

    template <>
    struct argument_parser<std::u16string> {
        static constexpr bool many_values = false;

        bool parse(std::string_view argument, std::u16string& parsed,
                   char = 0) const {
            parsed = to_u16string(argument);
            return true;
        }

        void reset(std::string& value) noexcept {
            value.clear();
        }
    };

    template <>
    struct argument_parser<std::u32string> {
        static constexpr bool many_values = false;

        bool parse(std::string_view argument, std::u32string& parsed,
                   char = 0) const {
            parsed = to_u32string(argument);
            return true;
        }

        void reset(std::string& value) noexcept {
            value.clear();
        }
    };

    template <Parsable T>
    struct argument_parser<std::vector<T>> {
        static constexpr bool many_values = true;

        bool parse(std::string_view arguments, std::vector<T>& parsed,
                   char separator) const {
            if (arguments.empty()) {
                return true;
            }
            for (auto arg :
                 arguments | split(std::string_view{&separator, 1})) {
                T parsed_arg{};
                argument_parser<T> parser;
                bool result = parser.parse(arg, parsed_arg, (char)0);
                if (!result) {
                    return false;
                }
                parsed.emplace_back(std::move(parsed_arg));
            }
            return true;
        }

        void reset(std::vector<T>& value) noexcept {
            value.clear();
        }
    };

    template <Parsable T>
    struct argument_parser<std::optional<T>> {
        static constexpr bool many_values = argument_parser<T>::many_values;

        bool parse(std::string_view argument, std::optional<T>& parsed,
                   char separator) const {
            T parsed_arg{};
            argument_parser<T> parser;
            bool result = parser.parse(argument, parsed_arg, separator);
            if (!result) {
                parsed = std::nullopt;
                return false;
            }
            parsed.emplace(std::move(parsed_arg));
            return true;
        }

        void reset(std::optional<T>& value) noexcept {
            value.reset();
        }
    };

    class option;

    class positional;

    class parser;

    /*!
     * @brief Split the command line string into vector of arguments,
     * following the rules of winapi CommandLineToArgvW function.
     * @param cmd_line The command line string to split.
     * @return vector of arguments.
     */
    RAD_EXPORT_DECL std::vector<std::string>
    split_winmain(std::string_view cmd_line);

    /*!
     * @brief Split the command line string into vector of arguments,
     * following the rules of winapi CommandLineToArgvW function.
     * @param cmd_line The command line string to split.
     * @return vector of arguments.
     */
    RAD_EXPORT_DECL std::vector<std::wstring>
    split_winmain(std::wstring_view cmd_line);

    /*!
     * @brief Split the command line string into vector of arguments,
     * following the rules of winapi CommandLineToArgvW function. Then
     * convert the arguments to utf-8 strings.
     * @param cmd_line The command line string to split.
     * @return vector of arguments.
     */
    inline std::vector<std::string>
    split_winmain_utf8(std::wstring_view cmd_line) {
        auto wargv = split_winmain(cmd_line);
        std::vector<std::string> argv;
        argv.reserve(wargv.size());
        for (const auto& arg : wargv) {
            argv.emplace_back(to_string(arg));
        }
        return argv;
    }

    class value_base {
        friend class option;

        friend class positional;

    public:
        virtual ~value_base() = default;

        virtual bool parse(std::string_view text) = 0;

        virtual void reset() = 0;

        virtual bool many_values() const noexcept = 0;
    };

    /*!
     * @brief Wrapper around a parsed value to use the specialization
     * of argument_parser<T> to parse the pointed to value.
     *
     * Specializations of value<T> may be added if the functionality
     * provided by this type is not enough.
     * @tparam T Type of value to parse.
     */
    template <Parsable T>
    class value final : public value_base {
    public:
        /*!
         * @brief Construct a value with reference to the output
         * value and a separator character.
         * @param v Reference to the output value. The
         * referenced value must be valid as long as the wrapper
         * value is used by the parser.
         * @param separator The character to use as a separator
         * for values that support multi values. ( default
         * separator is ';')
         */
        explicit value(T& v, char separator = ';') noexcept
            : value_{&v}, separator_{separator} {
        }

    private:
        bool parse(std::string_view text) override {
            argument_parser<T> parser;
            return parser.parse(text, *value_, separator_);
        }

        void reset() override {
            argument_parser<T> parser;
            parser.reset(*value_);
        }

        bool many_values() const noexcept override {
            return argument_parser<T>::many_values;
        }

        T* value_;
        char separator_;
    };

    /*!
     * @brief Option used by the parser to descripe the options to parse.
     * Unlike positionals, options are always prefixed on the command line
     * by '-' or
     * '/'.
     */
    class option {
        friend class parser;

    public:
        /*!
         * @brief Construct a valueless option with name and
         * whether it is required or not.
         * @param name The name of this option. If the name is
         * single character then the option short and long name
         * will be equal to this character. If the name ends
         * with ',' and a character then the option will have a
         * short name equal to this last character and a long
         * name equal to the part before ',' which must be non
         * empty. Otherwise the option does not have a short
         * name. An exception is thrown for invalid names.
         * @param optional True if this option is not required,
         * otherwise false. Required options will cause the
         * parser to fail if they are not present in the
         * arguments. To check if an option was parsed use
         * parser has_option().
         */
        option(std::string_view name, bool optional = false)
            : optional_{optional} {
            assign_name(name);
        }

        /*!
         * @brief Construct a option with value, name and
         * whether it is required or not.
         * @param name The name of this option. If the name is
         * single character then the option short and long name
         * will be equal to this character. If the name ends
         * with ',' and a character then the option will have a
         * short name equal to this last character and a long
         * name equal to the part before ',' which must be non
         * empty. Otherwise the option does not have a short
         * name. An exception is thrown for invalid names.
         * @param val The output value to bind to this option.
         * The referenced internal value by value<T> must be
         * valid as long as this option is used by the parser.
         * @param optional True if this option is not required,
         * otherwise false. Required options will cause the
         * parser to fail if they are not present in the
         * arguments. To check if an option was parsed use
         * parser has_option().
         */
        template <Parsable T>
        option(std::string_view name, const value<T>& val,
               bool optional = false)
            : option(name, "", val, optional) {
        }

        /*!
         * @brief Construct a valueless option with name,
         * description and whether it is required or not.
         * @param name The name of this option. If the name is
         * single character then the option short and long name
         * will be equal to this character. If the name ends
         * with ',' and a character then the option will have a
         * short name equal to this last character and a long
         * name equal to the part before ',' which must be non
         * empty. Otherwise the option does not have a short
         * name. An exception is thrown for invalid names.
         * @param description The description of this option.
         * @param optional True if this option is not required,
         * otherwise false. Required options will cause the
         * parser to fail if they are not present in the
         * arguments. To check if an option was parsed use
         * parser has_option().
         */
        option(std::string_view name, std::string_view description,
               bool optional = false)
            : description_{description}, optional_{optional} {
            assign_name(name);
        }

        /*!
         * @brief Construct a option with value, name,
         * description and whether it is required or not.
         * @param name The name of this option. If the name is
         * single character then the option short and long name
         * will be equal to this character. If the name ends
         * with ',' and a character then the option will have a
         * short name equal to this last character and a long
         * name equal to the part before ',' which must be non
         * empty. Otherwise the option does not have a short
         * name. An exception is thrown for invalid names.
         * @param val The output value to bind to this option.
         * The referenced internal value by value<T> must be
         * valid as long as this option is used by the parser.
         * @param description The description of this option.
         * @param optional True if this option is not required,
         * otherwise false. Required options will cause the
         * parser to fail if they are not present in the
         * arguments. To check if an option was parsed use
         * parser has_option().
         */
        template <class T>
        option(std::string_view name, std::string_view description,
               const value<T>& val, bool optional = false)
            : description_{description}, optional_{optional} {
            value_ = std::make_unique<value<T>>(val);
            assign_name(name);
        }

        /*!
         * @brief Get the description of this option.
         * @return The description of this option.
         */
        const std::string& description() const noexcept {
            return description_;
        }

        /*!
         * @brief Get the long name of this option.
         * @return The long name of this option.
         */
        const std::string& long_name() const noexcept {
            return long_name_;
        }

        /*!
         * @brief Get the short name of this option.
         * @return The short name of this option.
         */
        std::optional<char> short_name() const noexcept {
            return short_name_;
        }

        /*!
         * @brief Check if this option is optional (not
         * required)
         * @return True if this option is optional (not
         * required), otherwise false.
         */
        constexpr bool is_optional() const noexcept {
            return optional_;
        }

    private:
        void reset() noexcept {
            parsed_ = false;
        }

        RAD_EXPORT_DECL void parse(std::string_view text, bool use_short);

        value_base* get_value() {
            return value_.get();
        }

        // for error reporting
        std::string_view get_name(bool use_short) const noexcept {
            if (!use_short || !short_name_.has_value()) {
                return long_name();
            }
            else {
                return std::string_view(&*short_name_, 1);
            }
        }

        RAD_EXPORT_DECL void assign_name(std::string_view name);

        std::unique_ptr<value_base> value_;
        std::string long_name_;
        std::string description_;
        std::optional<char> short_name_ = -1;
        bool optional_ = false;
        bool parsed_ = false;
    };

    /*!
     * @brief Positional used by the parser to descripe the positionals to
     * parse. Unlike options, positionals must not be prefixed on the
     * command line by '-' or '/'.
     */
    class positional {
        friend class parser;

    public:
        /*!
         * @brief Construct a positional with value, name,
         * repeat count and whether it is required or not.
         * @param name The name of this positional. If @p name
         * is empty, an exception is thrown.
         * @param val The output value to bind to this
         * positional. The referenced internal value by value<T>
         * must be valid as long as this positional is used by
         * the parser.
         * @param repeat The maximum times this positional may
         * appear in the arguments. If @p repeat is greater than
         * 1 and the argument_parser<T> specialization has
         * many_values = false, an exception is thrown. If
         * repeat is 0 it is set to 1.
         * @param optional True if this positional is not
         * required, otherwise false. Required positionals will
         * cause the parser to fail if they are not present in
         * the arguments. To check if a positional was parsed
         * use parser has_positional().
         */
        template <Parsable T>
        positional(std::string_view name, const value<T>& val,
                   uint32_t repeat = 1, bool optional = false)
            : name_{name}, repeat_{repeat}, optional_{optional} {
            if (repeat_ < 0) {
                repeat_ = 1;
            }
            value_ = std::make_unique<value<T>>(val);
            validate_name_repeat();
        }

        /*!
         * @brief Construct a positional with value, name,
         * repeat count set to one and whether it is required or
         * not.
         * @param name The name of this positional. If @p name
         * is empty, an exception is thrown.
         * @param val The output value to bind to this
         * positional. The referenced internal value by value<T>
         * must be valid as long as this positional is used by
         * the parser.
         * @param optional True if this positional is not
         * required, otherwise false. Required positionals will
         * cause the parser to fail if they are not present in
         * the arguments. To check if a positional was parsed
         * use parser has_positional().
         */
        template <class T>
        positional(std::string_view name, const value<T>& val, bool optional)
            : positional(name, val, 1, optional) {
        }

        /*!
         * @brief Get the name of this positional.
         * @return The name of this positional.
         */
        const std::string& name() const noexcept {
            return name_;
        }

        /*!
         * @brief Get the repeat count of this positional.
         * @return The repeat count of this positional.
         */
        std::uint32_t repeat() const noexcept {
            return repeat_;
        }

        /*!
         * @brief Check if this positional is optional (not
         * required)
         * @return True if this positional is optional (not
         * required), otherwise false.
         */
        constexpr bool is_optional() const noexcept {
            return optional_;
        }

    private:
        void reset() noexcept {
            parsed_ = 0;
        }

        bool need_value() const noexcept {
            return parsed_ < repeat_;
        }

        RAD_EXPORT_DECL void parse(std::string_view text);

        value_base& get_value() {
            return *value_;
        }

        RAD_EXPORT_DECL void validate_name_repeat();

        std::string name_;
        uint32_t repeat_ = 0;
        uint32_t parsed_ = 0;
        std::unique_ptr<value_base> value_;
        bool optional_ = false;
    };

    /*!
     * @brief Command line parser.
     */
    class parser {
        class adder {
        public:
            adder(parser& p) noexcept : p{p} {
            }

            adder& operator()(option opt) {
                p.add_option(std::move(opt));
                return *this;
            }

            adder& operator()(std::string_view name, bool optional = false) {
                p.add_option(name, optional);
                return *this;
            }

            template <class T>
            adder& operator()(std::string_view name, const value<T>& val,
                              bool optional = false) {
                p.add_option(name, val, optional);
                return *this;
            }

            adder& operator()(std::string_view name,
                              std::string_view description,
                              bool optional = false) {
                p.add_option(name, description, optional);
                return *this;
            }

            template <class T>
            adder& operator()(std::string_view name,
                              std::string_view description, const value<T>& val,
                              bool optional = false) {
                p.add_option(name, description, val, optional);
                return *this;
            }

        private:
            parser& p;
        };

        class pos_adder {
        public:
            pos_adder(parser& p) noexcept : p{p} {
            }

            pos_adder& operator()(positional pos) {
                p.add_positional(std::move(pos));
                return *this;
            }

            template <class T>
            pos_adder& operator()(std::string_view name, const value<T>& val,
                                  uint32_t repeat = 1, bool optional = false) {
                p.add_positional(name, val, repeat, optional);
                return *this;
            }

            template <class T>
            pos_adder& operator()(std::string_view name, const value<T>& val,
                                  bool optional) {
                p.add_positional(name, val, optional);
                return *this;
            }

        private:
            parser& p;
        };

        using argv_reader = detail::cli::argv_reader;

    public:
        /*!
         * @brief Construct a parser with empty options and
         * positional, and with undefined options disallowed
         */
        constexpr parser() = default;

        /*!
         * @brief Move construct a parser. The constructed
         * parser takes the options and positional of the moved
         * from parser and its undefined options allow. The
         * moved from parser retains its undefined options
         * allow, and has no options and positional.
         */
        constexpr parser(parser&&) noexcept = default;

        /*!
         * @brief Copy construct a parser. The constructed
         * parser copies the options and positional of the
         * copied from parser and its undefined options allow.
         */
        parser(const parser&) = default;

        /*!
         * @brief Move assign a parser. The moved to parser
         * takes the options and positional of the moved from
         * parser and its undefined options allow. The moved
         * from parser retains its undefined options allow, and
         * has no options and positional.
         * @return the parser itslef
         */
        constexpr parser& operator=(parser&&) noexcept = default;

        /*!
         * @brief Copy assign a parser. The constructed copied
         * to parser copies the options and positional of the
         * copied from parser and its undefined options allow.
         * @return the parser itslef
         */
        parser& operator=(const parser&) = default;

        /*!
         * @brief Resets the parser to its initial state. Clears
         * all options and positionals and disallows undefined
         * options.
         */
        void reset() noexcept {
            opts.clear();
            poses.clear();
            allow_undefined = false;
        }

        /*!
         * @brief Allows undefined options. If undefined options
         * are not allowed and one is encountered during parsing
         * an exception is thrown. By default undefined options
         * are not allowed.
         */
        void allow_undefined_options() noexcept {
            allow_undefined = true;
        }

        /*!
         * @brief Disallows undefined options. If undefined
         * options are not allowed and one is encountered during
         * parsing an exception is thrown. By default undefined
         * options are not allowed.
         */
        void disallow_undefined_options() noexcept {
            allow_undefined = false;
        }

        /*!
         * @brief Parses the arguments in @p argv array whose
         * elements count is given by @p argc. Bound values of
         * options that are not parsed retain their previous
         * value. Text arguments are parsed into bound values
         * using argument_parser trait specializations. To check
         * if an option or a positional was parsed or not use
         * has_option() and has_positional(). For
         * `std::optional<T>` bound values if the initial value
         * was `std::nullopt` then check if the optional has a
         * value.
         * @param argc count of arguments array
         * @param argv the arguments array
         */
        RAD_EXPORT_DECL void parse(int argc, char** argv);

        /*!
         * @brief Parses the arguments in @p argv array whose
         * elements count is given by @p argc. Bound values of
         * options that are not parsed retain their previous
         * value. Text arguments are parsed into bound values
         * using argument_parser trait specializations. To check
         * if an option or a positional was parsed or not use
         * has_option() and has_positional(). For
         * `std::optional<T>` bound values if the initial value
         * was `std::nullopt` then check if the optional has a
         * value.
         * @param argc count of arguments array
         * @param argv the arguments array
         */
        RAD_EXPORT_DECL void parse(int argc, wchar_t** argv);

        /*!
         * @brief Parses the arguments in @p argv span.
         * Bound values of options that are not parsed retain
         * their previous value. Text arguments are parsed into
         * bound values using argument_parser trait
         * specializations. To check if an option or a
         * positional was parsed or not use has_option() and
         * has_positional(). For `std::optional<T>` bound values
         * if the initial value was `std::nullopt` then check if
         * the optional has a value.
         * @param argv the arguments span
         */
        RAD_EXPORT_DECL void parse(std::span<const std::string> argv);

        /*!
         * @brief Parses the arguments in @p argv span.
         * Bound values of options that are not parsed retain
         * their previous value. Text arguments are parsed into
         * bound values using argument_parser trait
         * specializations. To check if an option or a
         * positional was parsed or not use has_option() and
         * has_positional(). For `std::optional<T>` bound values
         * if the initial value was `std::nullopt` then check if
         * the optional has a value.
         * @param argv the arguments span
         */
        RAD_EXPORT_DECL void parse(std::span<const std::string_view> argv);

        /*!
         * @brief Parses the arguments in @p cmd_line. Splitting
         * the command line to a list of arguments is done using
         * the rules of `GetCommandLineW` which is available on
         * Windows. Bound values of options that are not parsed
         * retain their previous value. Text arguments are
         * parsed into bound values using argument_parser trait
         * specializations. To check if an option or a
         * positional was parsed or not use has_option() and
         * has_positional(). For `std::optional<T>` bound values
         * if the initial value was `std::nullopt` then check if
         * the optional has a value.
         * @param cmd_line the arguments command line
         */
        RAD_EXPORT_DECL void parse(std::string_view cmd_line);

        /*!
         * @brief Parses the arguments in @p cmd_line. Splitting
         * the command line to a list of arguments is done using
         * the rules of `GetCommandLineW` which is available on
         * Windows. Bound values of options that are not parsed
         * retain their previous value. Text arguments are
         * parsed into bound values using argument_parser trait
         * specializations. To check if an option or a
         * positional was parsed or not use has_option() and
         * has_positional(). For `std::optional<T>` bound values
         * if the initial value was `std::nullopt` then check if
         * the optional has a value.
         * @param cmd_line the arguments command line
         */
        RAD_EXPORT_DECL void parse(std::wstring_view cmd_line);

        /*!
         * @brief Get the count of parsed options having the
         * given long name
         * @param long_name the long name of the option
         * @return the count of parsed options having the given
         * long name
         */
        RAD_EXPORT_DECL std::size_t
        count_options(std::string_view long_name) const noexcept;

        /*!
         * @brief Get the count of parsed options having the
         * given short name
         * @param short_name the short name of the option
         * @return the count of parsed options having the given
         * short name
         */
        RAD_EXPORT_DECL std::size_t
        count_options(char short_name) const noexcept;

        /*!
         * @brief Get the count of parsed positionals having the
         * given name
         * @param name the name of the positional
         * @return the count of parsed positionals having the
         * given name
         */
        RAD_EXPORT_DECL std::size_t
        count_positionals(std::string_view name) const noexcept;

        /*!
         * @brief Get the count of parsed options having the
         * given long name and parsed positionals having the
         * given name.
         * @param name the long name of the option or name of
         * the positional
         * @return count of parsed options and positionals
         * having the given name
         */
        RAD_EXPORT_DECL std::size_t count(std::string_view name) const noexcept;

        /*!
         * @brief Check if an option having the given long name
         * was parsed.
         * @param long_name the long name of the option
         * @return true if an option having the given long name
         * was parsed, and false otherwise
         */
        RAD_EXPORT_DECL bool
        has_option(std::string_view long_name) const noexcept;

        /*!
         * @brief Check if an option having the given short name
         * was parsed.
         * @param short_name the short name of the option
         * @return true if an option having the given short name
         * was parsed, and false otherwise
         */
        RAD_EXPORT_DECL bool has_option(char short_name) const noexcept;

        /*!
         * @brief Check if a positional having the given name
         * was parsed.
         * @param name the name of the positional
         * @return true if a positional having the given name
         * was parsed, and false otherwise
         */
        RAD_EXPORT_DECL bool
        has_positional(std::string_view name) const noexcept;

        /*!
         * @brief Check if an option having the given long name
         * was parsed, or a positional having the given name was
         * parsed.
         * @param name the long name of the option or name of
         * the positional
         * @return true if an option or a positional having the
         * given name was parsed, and false otherwise
         */
        RAD_EXPORT_DECL bool has(std::string_view name) const noexcept;

        /*!
         * @brief Add an option to the parser
         * @param opt the option to add to the parser
         * @return the parser itself
         */
        parser& add_option(option opt) {
            opts.emplace_back(std::move(opt));
            return *this;
        }

        /*!
         * @brief Add a valueless option to the parser using its
         * long name and specifiy whether it is an optional or a
         * required option
         * @param name the long name of the option
         * @param optional true if this option is required and
         * false otherwise (default is false)
         * @return the parser itself
         */
        parser& add_option(std::string_view name, bool optional = false) {
            opts.emplace_back(name, optional);
            return *this;
        }

        /*!
         * @brief Add a value bound option to the parser using
         * its long name and specifiy whether it is an optional
         * or a required option
         * @tparam T the type of bound value
         * @param name the long name of the option
         * @param val the value bound to the option. When the
         * option is parsed the result of parsing is stored in
         * this value, so this value must be valid when parse()
         * is called
         * @param optional true if this option is required and
         * false otherwise (default is false)
         * @return the parser itself
         */
        template <Parsable T>
        parser& add_option(std::string_view name, const value<T>& val,
                           bool optional = false) {
            opts.emplace_back(name, val, optional);
            return *this;
        }

        /*!
         * @brief Add a valueless option to the parser using its
         * long name and description and specifiy whether it is
         * an optional or a required option
         * @param name the long name of the option
         * @param description the description of the option
         * @param optional true if this option is required and
         * false otherwise (default is false)
         * @return the parser itself
         */
        parser& add_option(std::string_view name, std::string_view description,
                           bool optional = false) {
            opts.emplace_back(name, description, optional);
            return *this;
        }

        /*!
         * @brief Add a value bound option to the parser using
         * its long name and description and specifiy whether it
         * is an optional or a required option
         * @tparam T the type of bound value
         * @param name the long name of the option
         * @param description the description of the option
         * @param val the value bound to the option. When the
         * option is parsed the result of parsing is stored in
         * this value, so this value must be valid when parse()
         * is called
         * @param optional true if this option is required and
         * false otherwise (default is false)
         * @return the parser itself
         */
        template <Parsable T>
        parser& add_option(std::string_view name, std::string_view description,
                           const value<T>& val, bool optional = false) {
            opts.emplace_back(name, description, val, optional);
            return *this;
        }

        /*!
         * @brief Add a positional to the parser
         * @param pos the positional to add the parser
         * @return the parser itself
         */
        parser& add_positional(positional pos) {
            poses.emplace_back(std::move(pos));
            return *this;
        }

        /*!
         * @brief Add a value bound positional to the parser
         * using its name and specifiy the repeat count and
         * whether it is an optional or a required positional
         * @tparam T the type of bound value
         * @param name the name of the positional
         * @param val the value bound to the option. When the
         * option is parsed the result of parsing is stored in
         * this value, so this value must be valid when parse()
         * is called
         * @param repeat the max allowed repeat count of the
         * positional
         * @param optional true if this positional is required
         * and false otherwise (default is false)
         * @return the parser itself
         */
        template <Parsable T>
        parser& add_positional(std::string_view name, const value<T>& val,
                               uint32_t repeat = 1, bool optional = false) {
            poses.emplace_back(name, val, repeat, optional);
            return *this;
        }

        /*!
         * @brief Add a value bound positional to the parser
         * using its name and specifiy whether it is an optional
         * or a required positional
         * @tparam T the type of bound value
         * @param name the name of the positional
         * @param val the value bound to the option. When the
         * option is parsed the result of parsing is stored in
         * this value, so this value must be valid when parse()
         * is called
         * @param optional true if this positional is required
         * and false otherwise (default is not required)
         * @return the parser itself
         */
        template <Parsable T>
        parser& add_positional(std::string_view name, const value<T>& val,
                               bool optional) {
            poses.emplace_back(name, val, optional);
            return *this;
        }

        /*!
         * @brief Starts adding options to the parser.
         * @return An object used to add options to the parser
         * using its call operator method
         */
        adder add_options() noexcept {
            return {*this};
        }

        /*!
         * @brief Starts adding positionals to the parser.
         * @return An object used to add positionals to the
         * parser using its call operator method
         */
        pos_adder add_positionals() noexcept {
            return {*this};
        }

    private:
        void do_parse(argv_reader& reader);

        void parse_positional(std::string_view arg);

        void parse_short(char arg, argv_reader& reader, bool may_have_param);

        void parse_multi_short(std::string_view args, argv_reader& reader);

        void parse_long(std::string_view arg, argv_reader& reader);

        void parse_long_eq(std::string_view arg, size_t eq_pos);

        std::vector<option> opts;
        std::vector<positional> poses;
        bool allow_undefined = false;
    };
} // namespace RAD_LIB_NAMESPACE::cli