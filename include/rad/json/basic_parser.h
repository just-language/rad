#pragma once
#include <rad/json/error.h>
#include <rad/match.h>
#include <rad/utf.h>

#include <cassert>
#include <string>
#include <system_error>
#include <vector>

namespace rad::json {
    namespace detail {
        enum class parsing_array_step {
            may_parse_value,  // spaces + (value or '}')
            must_parse_value, // spaces + value
            parse_sep,        // spaces + ((, + value) or '}')
        };

        enum class parsing_object_step {
            may_parse_key,  // spaces + (key or '}')
            must_parse_key, // spaces + key
            parse_value,    // spaces + value
            parse_sep,      // ' , ' or '}'
        };

        struct parsing_document {
            std::size_t n = 0;

            constexpr bool found_value() const noexcept {
                return n > 0;
            }
        };

        struct parsing_null {
            uint32_t parsed = 1;
        };

        struct parsing_bool {
            bool b = false;
            uint32_t parsed = 1;
        };

        enum class escape_state {
            none,
            want_escape,
            want_4hex_digits,
            want_3hex_digits,
            want_2hex_digits,
            want_1hex_digits,
            want_reverse_solidus,
            want_u,
        };

        struct string_escape_context {
            escape_state esc = escape_state::none;
            char hex_buff[4];
            char hex_buff2[4];
            bool has_hex_buff = false;

            void reset() noexcept {
                esc = escape_state::none;
                has_hex_buff = false;
            }
        };

        struct parsing_key {
            std::size_t n = 0;
            bool parsed_name = false;
        };

        struct parsing_string {
            std::size_t n = 0;
        };

        struct parsing_number {
            uint32_t point_pos = 0;
            uint32_t e_pos = 0;
            bool has_minus = false;
        };

        struct parsing_array {
            std::size_t n = 0;
            parsing_array_step step = parsing_array_step::may_parse_value;
        };

        struct parsing_object {
            std::size_t n = 0;
            parsing_object_step step = parsing_object_step::may_parse_key;
        };

        using basic_parser_item =
            std::variant<parsing_document, parsing_null, parsing_bool,
                         parsing_number, parsing_string, parsing_key,
                         parsing_array, parsing_object>;

        enum class basic_parser_state {
            not_started,
            parsing,
            done,
            failed,
        };

        struct utf8_reader {
            std::array<uint8_t, 4> cached_bytes;
            uint8_t cached_bytes_len = 0;
            uint8_t expected_cached_len = 0;

            codepoint_t decode_cached(const char*& p, std::size_t& n,
                                      std::errc& ec) noexcept {
                assert(expected_cached_len > cached_bytes_len);
                const std::size_t remaining_bytes =
                    expected_cached_len - cached_bytes_len;
                std::size_t consumed_n = std::min(remaining_bytes, n);
                n -= consumed_n;
                for (; cached_bytes_len < expected_cached_len && consumed_n > 0;
                     ++cached_bytes_len, --consumed_n) {
                    if (!is_utf8_continuation_byte(*p)) {
                        ec = std::errc::illegal_byte_sequence;
                        return 0;
                    }
                    cached_bytes[cached_bytes_len] = *p;
                    p += 1;
                }
                if (cached_bytes_len == expected_cached_len) {
                    const uint8_t* cached_p = cached_bytes.data();
                    std::size_t cached_len = cached_bytes_len;
                    cached_bytes_len = expected_cached_len = 0;
                    codepoint_t cp =
                        utf8_codecvt::decode(cached_p, cached_len, ec);
                    assert(ec != std::errc::no_buffer_space);
                    return cp;
                }
                ec = std::errc::no_buffer_space;
                return 0;
            }

#ifdef __GNUC__
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wignored-attributes"
#else
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstringop-overflow"
#endif // __clang__
#endif // __GNUC__

            codepoint_t decode(const char*& p, std::size_t& n,
                               std::errc& ec) noexcept {
                assert(n != 0);
                if (cached_bytes_len > 0) {
                    return decode_cached(p, n, ec);
                }
                codepoint_t cp = utf8_codecvt::decode(p, n, ec);
                if (ec == std::errc::no_buffer_space) {
                    assert(n < 4);
                    expected_cached_len =
                        static_cast<uint8_t>(utf8_sequence_size(*p));
                    assert(expected_cached_len != 0 && expected_cached_len > n);
                    cached_bytes_len = static_cast<uint8_t>(n);
                    std::copy(p, p + n, cached_bytes.data());
                    p += n;
                    n = 0;
                }
                return cp;
            }

#ifdef __GNUC__
#ifdef __clang__
#pragma clang diagnostic pop
#else
#pragma GCC diagnostic pop
#endif // __clang__
#endif // __GNUC__
        };

        RAD_EXPORT_DECL std::string_view
        parse_json_string(utf8_reader& utf8_reader, std::string_view& jtext,
                          string_escape_context& esc_ctx, char& unescaped_char,
                          std::array<char, 4>& utf8_buff, bool& finished,
                          std::error_code& ec);

        RAD_EXPORT_DECL std::variant<int64_t, uint64_t, double>
        parse_json_number(parsing_number& i, std::string_view number_text,
                          std::error_code& ec) noexcept;

        inline constexpr bool is_white_space(codepoint_t cp) {
            return cp == ' ' || cp == '\t' || cp == '\n' || cp == '\r';
        }

        inline void
        ensure_error_on_handler_false(std::error_code& ec) noexcept {
            if (ec) {
                ec = std::make_error_code(std::errc::interrupted);
            }
        }
    } // namespace detail

    template <class Handler>
    concept ParserHandler =
        requires(Handler handler, std::string_view s, std::size_t n,
                 std::int64_t i, std::uint64_t u, double d, bool b,
                 std::error_code& ec) {
            { Handler::max_array_size } -> std::convertible_to<std::size_t>;
            { Handler::max_object_size } -> std::convertible_to<std::size_t>;
            { Handler::max_string_size } -> std::convertible_to<std::size_t>;
            { Handler::max_key_size } -> std::convertible_to<std::size_t>;
            { handler.on_document_begin(ec) } -> std::same_as<bool>;
            { handler.on_document_end(ec) } -> std::same_as<bool>;
            { handler.on_array_begin(ec) } -> std::same_as<bool>;
            { handler.on_array_end(n, ec) } -> std::same_as<bool>;
            { handler.on_object_begin(ec) } -> std::same_as<bool>;
            { handler.on_object_end(n, ec) } -> std::same_as<bool>;
            { handler.on_string_part(s, n, ec) } -> std::same_as<bool>;
            { handler.on_string(s, n, ec) } -> std::same_as<bool>;
            { handler.on_key_part(s, n, ec) } -> std::same_as<bool>;
            { handler.on_key(s, n, ec) } -> std::same_as<bool>;
            { handler.on_number_part(s, ec) } -> std::same_as<bool>;
            { handler.on_int64(i, s, ec) } -> std::same_as<bool>;
            { handler.on_uint64(u, s, ec) } -> std::same_as<bool>;
            { handler.on_double(d, s, ec) } -> std::same_as<bool>;
            { handler.on_bool(b, ec) } -> std::same_as<bool>;
            { handler.on_null(ec) } -> std::same_as<bool>;
        };

    /*!
     * @brief An incremental SAX parser for serialized JSON.
     * This implements a SAX-style parser, invoking a caller-supplied
     * handler with each parsing event. To use, first declare a variable of
     * type basic_parser<T> where T meets the ParserHandler requirements.
     * Then call write_some one or more times with the input, setting more =
     * false on the final buffer. The parsing events are realized through
     * member function calls on the handler, which exists as a data member
     * of the parser. The parser may dynamically allocate intermediate
     * storage as needed to accommodate the nesting level of the input JSON.
     * On subsequent invocations, the parser can cheaply re-use this memory,
     * improving performance. This storage is freed when the parser is
     * destroyed.
     * @tparam Handler The type of handler. Must meet the ParserHandler
     * requirements.
     */
    template <ParserHandler Handler>
    class basic_parser {
    public:
        template <class... Args>
        basic_parser(Args&&... args) : handler_{std::forward<Args>(args)...} {
        }

        /*!
         * @brief Check if a complete JSON text has been parsed.
         * This function returns true when all of these
         * conditions are met: A complete serialized JSON text
         * has been presented to the parser, and No error has
         * occurred since the parser was constructed, or since
         * the last call to @ref reset.
         * @return True if parsing is done and successful, and
         * false otherwise.
         */
        bool done() const noexcept {
            return state_ == detail::basic_parser_state::done;
        }

        /*!
         * @brief Check if the parser has failed to parse
         * previous JSON text.
         * @return True if the parser has failed to parse
         * previous JSON text, otherwise false.
         */
        bool failed() const noexcept {
            return static_cast<bool>(last_ec_);
        }

        /*!
         * @brief Indicate a parsing failure.
         * This changes the state of the parser to indicate that
         * the parse has failed. A parser implementation can use
         * this to fail the parser if needed due to external
         * inputs.
         * @param ec The error code to set.
         */
        void fail(std::error_code ec) noexcept {
            if (last_ec_) {
                return;
            }
            if (!ec) {
                ec = make_error(error::incomplete);
            }
            last_ec_ = ec;
            state_ = detail::basic_parser_state::failed;
        }

        /*!
         * @brief Get the last error if the parser has failed.
         * @return The last error if the parser has failed. In
         * case of no failure an empty error is returned.
         */
        std::error_code last_error() const noexcept {
            return last_ec_;
        }

        /*!
         * @brief Return a reference to the handler. This
         * function provides access to the constructed instance
         * of the handler owned by the parser.
         * @return A reference to the handler.
         */
        Handler& handler() noexcept {
            return handler_;
        }

        /*!
         * @brief Return a reference to the handler. This
         * function provides access to the constructed instance
         * of the handler owned by the parser.
         * @return A reference to the handler.
         */
        const Handler& handler() const noexcept {
            return handler_;
        }

        /*!
         * @brief Reset the state, to parse a new document.
         * This function discards the current parsing state, to
         * prepare for parsing a new document. Dynamically
         * allocated temporary memory used by the implementation
         * is not deallocated.
         */
        void reset() noexcept {
            state_ = detail::basic_parser_state::not_started;
            stack_.clear();
            number_text_.clear();
            last_ec_.clear();
        }

        /*!
         * @brief Parse some of input characters as JSON,
         * incrementally. This function parses the JSON text in
         * the specified buffer, calling the handler to emit
         * each SAX parsing event. The parse proceeds from the
         * current state, which is at the beginning of a new
         * JSON or in the middle of the current JSON if any
         * characters were already parsed. The characters in the
         * buffer are processed starting from the beginning,
         * until one of the following conditions is met:
         * - All of the characters in the buffer have been
         * parsed, or
         * - Some of the characters in the buffer have been
         * parsed and the JSON is complete, or
         * - A parsing error occurs.
         * The supplied buffer does not need to contain the
         * entire JSON. Subsequent calls can provide more
         * serialized data, allowing JSON to be processed
         * incrementally. The end of the serialized JSON can be
         * indicated by passing more = false.
         * @param more true if there are possibly more buffers
         * in the current JSON, otherwise false.
         * @param data A pointer to a buffer of size characters
         * to parse.
         * @param n The number of characters pointed to by data.
         * @param ec Set to the error, if any occurred.
         * @return The number of characters successfully parsed,
         * which may be smaller than @p n.
         */
        std::size_t write_some(bool more, const char* data, std::size_t n,
                               std::error_code& ec);

        /*!
         * @brief Parse some of input characters as JSON,
         * incrementally. This function parses the JSON text in
         * the specified buffer, calling the handler to emit
         * each SAX parsing event. The parse proceeds from the
         * current state, which is at the beginning of a new
         * JSON or in the middle of the current JSON if any
         * characters were already parsed. The characters in the
         * buffer are processed starting from the beginning,
         * until one of the following conditions is met:
         *
         * - All of the characters in the buffer have been
         * parsed, or
         *
         * - Some of the characters in the buffer have been
         * parsed and the JSON is complete, or
         *
         * - A parsing error occurs.
         * The supplied buffer does not need to contain the
         * entire JSON. Subsequent calls can provide more
         * serialized data, allowing JSON to be processed
         * incrementally. The end of the serialized JSON can be
         * indicated by passing more = false.
         * @param more true if there are possibly more buffers
         * in the current JSON, otherwise false.
         * @param data The character string to parse.
         * @param ec Set to the error, if any occurred.
         * @return The number of characters successfully parsed,
         * which may be smaller than size of @p data.
         */
        std::size_t write_some(bool more, std::string_view data,
                               std::error_code& ec) {
            return write_some(more, data.data(), data.size(), ec);
        }

        /*!
         * @brief Indicate the end of JSON input.
         * This function is used to indicate that there are no
         * more character buffers in the current JSON text being
         * parsed. If the resulting JSON text is incomplete,
         * assigns the relevant std::error_code to @p ec.
         * @param ec Set to the error, if any occurred.
         */
        void finish(std::error_code& ec);

    private:
        std::size_t parse_document(detail::parsing_document& d,
                                   std::string_view jtext, std::error_code& ec);

        std::size_t parse_const_text(std::string_view desired,
                                     uint32_t& parsed_n, std::string_view jtext,
                                     bool& finished, std::error_code& ec);

        std::size_t parse_null(detail::parsing_null& p, std::string_view jtext,
                               std::error_code& ec) {
            bool finished = false;
            const std::size_t n =
                parse_const_text("null", p.parsed, jtext, finished, ec);
            if (finished && !ec) {
                if (!handler_.on_null(ec)) {
                    detail::ensure_error_on_handler_false(ec);
                }
            }
            return n;
        }

        std::size_t parse_bool(detail::parsing_bool& p, std::string_view jtext,
                               std::error_code& ec) {
            bool finished = false;
            const bool bval = p.b;
            const std::size_t n = parse_const_text(
                p.b ? "true" : "false", p.parsed, jtext, finished, ec);
            if (finished && !ec) {
                if (!handler_.on_bool(bval, ec)) {
                    detail::ensure_error_on_handler_false(ec);
                }
            }
            return n;
        }

        std::size_t parse_number(detail::parsing_number& i,
                                 std::string_view jtext, std::error_code& ec);

        std::size_t parse_string(detail::parsing_string& s,
                                 std::string_view jtext, std::error_code& ec);

        std::size_t parse_key(detail::parsing_key& k, std::string_view jtext,
                              std::error_code& ec);

        std::size_t parse_array(detail::parsing_array& a,
                                std::string_view jtext, std::error_code& ec);

        std::size_t parse_object(detail::parsing_object& o,
                                 std::string_view jtext, std::error_code& ec);

        void validate_and_report_number(detail::parsing_number& i,
                                        std::error_code& ec);

        void validate_and_report_double(detail::parsing_number& i,
                                        std::error_code& ec);

        bool try_parse_value(codepoint_t cp, std::error_code& ec,
                             std::size_t& n);

        Handler handler_;
        std::vector<detail::basic_parser_item> stack_;
        detail::basic_parser_state state_ =
            detail::basic_parser_state::not_started;
        std::string number_text_;
        detail::string_escape_context esc_ctx_;
        detail::utf8_reader utf8_reader_;
        std::error_code last_ec_;
    };

    template <ParserHandler Handler>
    std::size_t basic_parser<Handler>::write_some(bool more, const char* data,
                                                  std::size_t n,
                                                  std::error_code& ec) {
        using namespace detail;
        ec.clear();
        if (last_ec_) {
            ec = last_ec_;
            return 0;
        }
        assert(!failed());
        if (done()) {
            return 0;
        }
        if (state_ == basic_parser_state::not_started) {
            const bool res = handler_.on_document_begin(ec);
            if (!res) {
                ensure_error_on_handler_false(ec);
                state_ = basic_parser_state::failed;
                return 0;
            }
            state_ = basic_parser_state::parsing;
            stack_.emplace_back(parsing_document{});
        }
        else if (stack_.empty()) {
            state_ = basic_parser_state::failed;
            ensure_error_on_handler_false(ec);
            return 0;
        }

        std::string_view jtext{data, n};
        while (!jtext.empty() && !done() && !failed() && !ec) {
            const std::size_t count = match(
                stack_.back(),
                [&](parsing_document& d) {
                    return parse_document(d, jtext, ec);
                },
                [&](parsing_null& n) { return parse_null(n, jtext, ec); },
                [&](parsing_bool& b) { return parse_bool(b, jtext, ec); },
                [&](parsing_number& n) { return parse_number(n, jtext, ec); },
                [&](parsing_string& s) { return parse_string(s, jtext, ec); },
                [&](parsing_key& k) { return parse_key(k, jtext, ec); },
                [&](parsing_array& a) { return parse_array(a, jtext, ec); },
                [&](parsing_object& o) { return parse_object(o, jtext, ec); });
            jtext.remove_prefix(count);
        }

        if (jtext.empty() &&
            ec == std::make_error_code(std::errc::no_buffer_space) &&
            utf8_reader_.cached_bytes_len > 0) {
            if (!more) {
                ec = make_error(error::incomplete);
            }
            else {
                ec.clear();
            }
        }

        if (!ec && !more && !done() && !failed()) {
            finish(ec);
        }

        if (ec) {
            state_ = basic_parser_state::failed;
            last_ec_ = ec;
        }
        else {
            // consume remaining leading white space
            // so write(data) doesn't fail because of remaining
            // white space
            while (!jtext.empty() && is_white_space(jtext.front())) {
                jtext.remove_prefix(1);
            }
        }

        return n - jtext.size();
    }

    template <ParserHandler Handler>
    std::size_t
    basic_parser<Handler>::parse_document(detail::parsing_document& d,
                                          std::string_view jtext,
                                          std::error_code& ec) {
        if (d.found_value()) {
            stack_.pop_back();
            state_ = detail::basic_parser_state::done;
            if (!handler_.on_document_end(ec)) {
                state_ = detail::basic_parser_state::failed;
                detail::ensure_error_on_handler_false(ec);
            }
            return 0;
        }

        const char* data = jtext.data();
        std::size_t n = jtext.size();
        const std::size_t total_size = jtext.size();
        while (n > 0) {
            std::errc e{};
            const std::size_t saved_n = n;
            codepoint_t cp = utf8_codecvt::decode(data, n, e);
            if (e != std::errc{}) {
                ec = make_error(error::input_error);
                return total_size - n;
            }
            if (detail::is_white_space(cp)) {
                continue;
            }
            // d may be dangling after value insertion
            d.n += 1;
            if (try_parse_value(cp, ec, d.n)) {
                // d may be dangling!
                return total_size - n;
            }
            else {
                ec = make_error(error::syntax);
                return total_size - saved_n;
            }
        }
        return total_size - n;
    }

    template <ParserHandler Handler>
    std::size_t basic_parser<Handler>::parse_string(detail::parsing_string& s,
                                                    std::string_view jtext,
                                                    std::error_code& ec) {
        const std::size_t total_size = jtext.size();
        bool finished = false;
        while (!jtext.empty() && !ec && !finished) {
            char unescaped_char = 0;
            std::array<char, 4> utf8_buff;
            auto part =
                parse_json_string(utf8_reader_, jtext, esc_ctx_, unescaped_char,
                                  utf8_buff, finished, ec);
            if (ec) {
                return total_size - jtext.size();
            }
            s.n += part.size();
            if (s.n > Handler::max_string_size) {
                ec = make_error(error::string_too_large);
                return total_size - jtext.size();
            }
            if (finished) {
                const std::size_t n = s.n;
                stack_.pop_back();
                if (!handler_.on_string(part, n, ec)) {
                    detail::ensure_error_on_handler_false(ec);
                }
                return total_size - jtext.size();
            }
            else if (!part.empty()) {
                if (!handler_.on_string_part(part, s.n, ec)) {
                    detail::ensure_error_on_handler_false(ec);
                }
            }
        }
        return total_size - jtext.size();
    }

    template <ParserHandler Handler>
    std::size_t basic_parser<Handler>::parse_key(detail::parsing_key& k,
                                                 std::string_view jtext,
                                                 std::error_code& ec) {
        using namespace detail;
        const char* data = jtext.data();
        const std::size_t total_size = jtext.size();

        if (k.parsed_name) {
            std::size_t n = jtext.size();
            while (n > 0) {
                std::errc e{};
                const std::size_t saved_n = n;
                codepoint_t cp = utf8_reader_.decode(data, n, e);
                if (e != std::errc{}) {
                    ec = make_error(error::input_error);
                    return total_size - n;
                }
                if (is_white_space(cp)) {
                    continue;
                }
                else if (cp == ':') {
                    stack_.pop_back();
                    return total_size - n;
                }
                else {
                    ec = make_error(error::syntax);
                    return total_size - saved_n;
                }
            }
            return total_size - n;
        }

        bool finished = false;
        while (!jtext.empty() && !ec && !finished) {
            char unescaped_char = 0;
            std::array<char, 4> utf8_buff;
            auto part =
                parse_json_string(utf8_reader_, jtext, esc_ctx_, unescaped_char,
                                  utf8_buff, finished, ec);
            if (ec) {
                return total_size - jtext.size();
            }
            k.n += part.size();
            if (k.n > Handler::max_key_size) {
                ec = make_error(error::key_too_large);
                return total_size - jtext.size();
            }
            if (finished) {
                k.parsed_name = true;
                if (!handler_.on_key(part, k.n, ec)) {
                    detail::ensure_error_on_handler_false(ec);
                }
                return total_size - jtext.size();
            }
            else if (!part.empty()) {
                if (!handler_.on_key_part(part, k.n, ec)) {
                    detail::ensure_error_on_handler_false(ec);
                }
            }
        }
        return total_size - jtext.size();
    }

    template <ParserHandler Handler>
    std::size_t basic_parser<Handler>::parse_const_text(
        std::string_view desired, uint32_t& parsed_n, std::string_view jtext,
        bool& finished, std::error_code& ec) {
        assert(parsed_n < desired.size());
        assert(!jtext.empty() && !desired.empty());
        finished = false;
        const std::size_t part_size =
            std::min(desired.size() - parsed_n, jtext.size());
        const std::string_view desired_part =
            desired.substr(parsed_n, part_size);
        const std::string_view jtext_part = jtext.substr(0, part_size);
        assert(desired_part.size() == jtext_part.size());
        for (std::size_t i = 0; i < desired_part.size(); ++i) {
            if (desired_part[i] != jtext_part[i]) {
                ec = make_error(error::syntax);
                return i;
            }
        }
        parsed_n += static_cast<uint32_t>(part_size);
        assert(parsed_n <= desired.size());
        if (parsed_n == desired.size()) {
            finished = true;
            stack_.pop_back();
        }
        return part_size;
    }

    template <ParserHandler Handler>
    std::size_t basic_parser<Handler>::parse_number(detail::parsing_number& i,
                                                    std::string_view jtext,
                                                    std::error_code& ec) {
        const char* data = jtext.data();
        std::size_t n = jtext.size();
        const std::size_t total_size = n;
        const char* saved_data = data;

        while (n > 0) {
            std::errc e{};
            const std::size_t saved_n = n;
            codepoint_t cp = utf8_reader_.decode(data, n, e);
            if (e != std::errc{}) {
                // numbers contain only ascii codes
                ec = make_error(error::input_error);
                return total_size - n;
            }
            if (cp == '-') {
                const bool leading_minus = number_text_.empty();
                const bool exp_minus =
                    !number_text_.empty() &&
                    (number_text_.back() == 'e' || number_text_.back() == 'E');
                if (!leading_minus && !exp_minus) {
                    ec = make_error(error::syntax);
                    return total_size - saved_n;
                }
                number_text_ += '-';
                i.has_minus = leading_minus;
            }
            else if (cp == '+') {
                const bool exp_plus =
                    !number_text_.empty() &&
                    (number_text_.back() == 'e' || number_text_.back() == 'E');
                if (!exp_plus) {
                    ec = make_error(error::syntax);
                    return total_size - saved_n;
                }
                number_text_ += '+';
            }
            else if (cp == '.') {
                if (number_text_.empty() || i.point_pos != 0 || i.e_pos != 0) {
                    ec = make_error(error::syntax);
                    return total_size - saved_n;
                }
                i.point_pos = static_cast<uint32_t>(number_text_.size());
                number_text_ += '.';
            }
            else if (cp == 'e' || cp == 'E') {
                if (number_text_.empty() || i.e_pos != 0 ||
                    number_text_.back() == '.' || number_text_.back() == '-') {
                    ec = make_error(error::syntax);
                    return total_size - saved_n;
                }
                i.e_pos = static_cast<uint32_t>(number_text_.size());
                number_text_ += static_cast<char>(cp);
            }
            else if (cp >= '0' && cp <= '9') {
                number_text_ += static_cast<char>(cp);
            }
            else {
                if (number_text_.empty() || !(number_text_.back() >= '0' &&
                                              number_text_.back() <= '9')) {
                    ec = make_error(error::syntax);
                    return total_size - saved_n;
                }
                validate_and_report_number(i, ec);
                // don't consume the non digit character
                return total_size - saved_n;
            }
        }

        assert(n == 0);
        if (!handler_.on_number_part(
                std::string_view{saved_data, total_size - n}, ec)) {
            detail::ensure_error_on_handler_false(ec);
        }
        return total_size - n;
    }

    template <ParserHandler Handler>
    void
    basic_parser<Handler>::validate_and_report_number(detail::parsing_number& i,
                                                      std::error_code& ec) {
        auto on_exit = scope_exit([&] {
            number_text_.clear();
            stack_.pop_back();
        });
        auto parsed = parse_json_number(i, number_text_, ec);
        if (ec) {
            return;
        }
        const bool res = match(
            parsed,
            [&](std::int64_t i) {
                return handler_.on_int64(i, number_text_, ec);
            },
            [&](std::uint64_t u) {
                return handler_.on_uint64(u, number_text_, ec);
            },
            [&](double d) { return handler_.on_double(d, number_text_, ec); });
        if (!res) {
            detail::ensure_error_on_handler_false(ec);
        }
        return;
    }

    template <ParserHandler Handler>
    std::size_t basic_parser<Handler>::parse_array(detail::parsing_array& a,
                                                   std::string_view jtext,
                                                   std::error_code& ec) {
        using namespace detail;
        const char* data = jtext.data();
        std::size_t n = jtext.size();
        const std::size_t total_size = n;
        while (n > 0) {
            std::errc e{};
            const std::size_t saved_n = n;
            codepoint_t cp = utf8_reader_.decode(data, n, e);
            if (e != std::errc{}) {
                // only ascii codes are accepted here: [ ] ,
                ec = make_error(error::input_error);
                return total_size - n;
            }
            if (cp == ']') {
                if (a.step == parsing_array_step::must_parse_value) {
                    ec = make_error(error::syntax);
                    return total_size - saved_n;
                }
                const std::size_t items_count = a.n;
                stack_.pop_back();
                if (!handler_.on_array_end(items_count, ec)) {
                    detail::ensure_error_on_handler_false(ec);
                }
                return total_size - n;
            }
            if (is_white_space(cp)) {
                continue;
            }
            if (cp == ',') {
                if (a.step == parsing_array_step::parse_sep) {
                    a.step = parsing_array_step::must_parse_value;
                    continue;
                }
                else {
                    ec = make_error(error::syntax);
                    return total_size - saved_n;
                }
            }
            else {
                if (a.step == parsing_array_step::parse_sep) {
                    ec = make_error(error::syntax);
                    return total_size - saved_n;
                }
                a.step = parsing_array_step::parse_sep;
                a.n += 1;
                if (a.n > Handler::max_array_size) {
                    ec = make_error(error::array_too_large);
                }
                if (!try_parse_value(cp, ec, a.n)) {
                    return total_size - saved_n;
                }
                // a may be dangling!
                return total_size - n;
            }
        }
        return total_size - n;
    }

    template <ParserHandler Handler>
    std::size_t basic_parser<Handler>::parse_object(detail::parsing_object& o,
                                                    std::string_view jtext,
                                                    std::error_code& ec) {
        using namespace detail;
        const char* data = jtext.data();
        std::size_t n = jtext.size();
        const std::size_t total_size = n;
        while (n > 0) {
            std::errc e{};
            const std::size_t saved_n = n;
            codepoint_t cp = utf8_reader_.decode(data, n, e);
            if (e != std::errc{}) {
                // only ascii codes are accepted here: { } ,
                ec = make_error(error::input_error);
                return total_size - n;
            }
            if (cp == '}') {
                if (o.step == parsing_object_step::parse_value ||
                    o.step == parsing_object_step::must_parse_key) {
                    ec = make_error(error::syntax);
                    return total_size - saved_n;
                }
                const std::size_t items_count = o.n;
                stack_.pop_back();
                if (!handler_.on_object_end(items_count, ec)) {
                    detail::ensure_error_on_handler_false(ec);
                }
                return total_size - n;
            }
            if (is_white_space(cp)) {
                continue;
            }
            if (cp == ',') {
                if (o.step == parsing_object_step::parse_sep) {
                    o.step = parsing_object_step::must_parse_key;
                    continue;
                }
                else {
                    ec = make_error(error::syntax);
                    return total_size - saved_n;
                }
            }
            else if (cp == '"') {
                if (o.step == parsing_object_step::parse_sep) {
                    ec = make_error(error::syntax);
                    return total_size - saved_n;
                }
                if (o.step == parsing_object_step::may_parse_key ||
                    o.step == parsing_object_step::must_parse_key) {
                    o.n += 1;
                    o.step = parsing_object_step::parse_value;
                    if (o.n > Handler::max_object_size) {
                        ec = make_error(error::object_too_large);
                        return total_size - saved_n;
                    }
                    stack_.emplace_back(parsing_key{});
                    return total_size - n;
                }
                else if (o.step == parsing_object_step::parse_value) {
                    o.step = parsing_object_step::parse_sep;
                    stack_.emplace_back(parsing_string{});
                    return total_size - n;
                }
            }
            else {
                if (o.step != parsing_object_step::parse_value) {
                    ec = make_error(error::syntax);
                    return total_size - saved_n;
                }
                o.step = parsing_object_step::parse_sep;
                if (!try_parse_value(cp, ec, o.n)) {
                    return total_size - saved_n;
                }
                // o may be dangling!
                return total_size - n;
            }
        }
        return total_size - n;
    }

    template <ParserHandler Handler>
    bool basic_parser<Handler>::try_parse_value(codepoint_t cp,
                                                std::error_code& ec,
                                                std::size_t& n) {
        using namespace detail;
        ec.clear();
        if (cp == 'n') {
            stack_.emplace_back(parsing_null{});
        }
        else if (cp == 't' || cp == 'f') {
            stack_.emplace_back(parsing_bool{cp == 't'});
        }
        else if (cp == '"') {
            esc_ctx_.reset();
            stack_.emplace_back(parsing_string{});
        }
        else if (cp == '-' || (cp >= '0' && cp <= '9')) {
            parsing_number num;
            num.has_minus = cp == '-';
            number_text_ += static_cast<char>(cp);
            stack_.emplace_back(num);
        }
        else if (cp == '[') {
            if (!handler_.on_array_begin(ec)) {
                detail::ensure_error_on_handler_false(ec);
                n -= 1;
                return false;
            }
            stack_.emplace_back(parsing_array{});
        }
        else if (cp == '{') {
            if (!handler_.on_object_begin(ec)) {
                detail::ensure_error_on_handler_false(ec);
                n -= 1;
                return false;
            }
            stack_.emplace_back(parsing_object{});
        }
        else {
            n -= 1;
            ec = make_error(error::syntax);
            return false;
        }
        return true;
    }

    template <ParserHandler Handler>
    void basic_parser<Handler>::finish(std::error_code& ec) {
        using namespace detail;
        ec.clear();
        if (last_ec_) {
            ec = last_ec_;
            return;
        }
        if (done()) {
            return;
        }

        auto make_incomplete_error = [&] {
            ec = make_error(error::incomplete);
            last_ec_ = make_error(error::incomplete);
            state_ = basic_parser_state::failed;
        };

        if (utf8_reader_.cached_bytes_len != 0 || stack_.empty() ||
            stack_.size() > 2) {
            return make_incomplete_error();
        }

        if (stack_.size() == 1) {
            auto& doc = std::get<parsing_document>(stack_.front());
            if (!doc.found_value()) {
                return make_incomplete_error();
            }
            parse_document(doc, "", ec);
        }
        else if (parsing_number* n = std::get_if<parsing_number>(&stack_[1])) {
            if (!number_text_.empty() && number_text_.back() >= '0' &&
                number_text_.back() <= '9') {
                validate_and_report_number(*n, ec);
                if (!ec) {
                    stack_.pop_back();
                    auto& doc = std::get<parsing_document>(stack_.front());
                    assert(doc.found_value());
                    if (!doc.found_value()) {
                        return make_incomplete_error();
                    }
                    parse_document(doc, "", ec);
                }
            }
        }
        else {
            return make_incomplete_error();
        }

        if (ec) {
            last_ec_ = ec;
            state_ = basic_parser_state::failed;
        }
        else {
            state_ = basic_parser_state::done;
        }
    }
} // namespace rad::json