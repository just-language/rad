#pragma once
#include <rad/json/basic_parser.h>
#include <rad/json/value.h>

namespace rad::json {
    namespace detail {
        struct dom_handler {
        public:
            static constexpr std::size_t max_array_size =
                std::numeric_limits<std::size_t>::max();

            static constexpr std::size_t max_object_size =
                std::numeric_limits<std::size_t>::max();

            static constexpr std::size_t max_key_size =
                std::numeric_limits<std::size_t>::max();

            static constexpr std::size_t max_string_size =
                std::numeric_limits<std::size_t>::max();

            bool on_document_begin(std::error_code& ec) {
                return true;
            }

            bool on_document_end(std::error_code& ec) {
                return true;
            }

            bool on_array_begin(std::error_code& ec) {
                return true;
            }

            bool on_array_end(std::size_t n, std::error_code& ec) {
                stack_.push_array(n);
                return true;
            }

            bool on_object_begin(std::error_code& ec) {
                return true;
            }

            bool on_object_end(std::size_t n, std::error_code& ec) {
                stack_.push_object(n);
                return true;
            }

            bool on_string_part(std::string_view s, std::size_t n,
                                std::error_code& ec) {
                stack_.push_chars(s);
                return true;
            }

            bool on_string(std::string_view s, std::size_t n,
                           std::error_code& ec) {
                stack_.push_string(s);
                return true;
            }

            bool on_key_part(std::string_view s, std::size_t n,
                             std::error_code& ec) {
                stack_.push_chars(s);
                return true;
            }

            bool on_key(std::string_view s, std::size_t n,
                        std::error_code& ec) {
                stack_.push_key(s);
                return true;
            }

            bool on_number_part(std::string_view s, std::error_code& ec) {
                return true;
            }

            bool on_int64(int64_t i, std::string_view s, std::error_code& ec) {
                stack_.push_int64(i);
                return true;
            }

            bool on_uint64(uint64_t i, std::string_view s,
                           std::error_code& ec) {
                stack_.push_uint64(i);
                return true;
            }

            bool on_double(double d, std::string_view s, std::error_code& ec) {
                stack_.push_double(d);
                return true;
            }

            bool on_bool(bool b, std::error_code& ec) {
                stack_.push_bool(b);
                return true;
            }

            bool on_null(std::error_code& ec) {
                stack_.push_null();
                return true;
            }

            value release() noexcept {
                return stack_.release();
            }

        private:
            value_stack stack_;
        };
    } // namespace detail

    static_assert(ParserHandler<detail::dom_handler>,
                  "The DOM handler didn't satisfy ParserHandler");

    /*!
     * @brief A DOM parser for JSON contained in a single buffer.
     * @details This class is used to parse a JSON text contained in a
     * single character buffer, into a @ref value container. To use the
     * parser first construct it, then call write to parse a character
     * buffer containing a complete JSON text. If the parse is successful,
     * call release to take ownership of the value:
     * @code
     * parser p;                         // construct a parser
     * size_t n = p.write( "[1,2,3]" );  // parse a complete JSON text
     * assert( n == 7 );                 // all characters consumed
     * value jv = p.release();           // take ownership of the value
     * @endcode
     */
    class parser {
    public:
        /*!
         * @brief Return the parsed JSON text as a value.
         * This returns the parsed value, or throws an exception
         * if the parsing is incomplete or failed. It is
         * necessary to call reset after calling this function
         * in order to parse another JSON text.
         * @return The parsed value. Ownership of this value is
         * transferred to the caller.
         */
        value release() noexcept {
            return parser_.handler().release();
        }

        /*!
         * @brief Reset the parser for a new JSON text.
         * This function is used to reset the parser to prepare
         * it for parsing a new complete JSON text. Any previous
         * partial results are destroyed.
         */
        void reset() noexcept {
            parser_.reset();
        }

        /*!
         * @brief Parse a buffer containing a complete JSON
         * text. This function parses a complete JSON text
         * contained in the specified character buffer.
         * Additional characters past the end of the complete
         * JSON text are ignored. The function returns the
         * actual number of characters parsed, which may be less
         * than the size of the input. This allows parsing of a
         * buffer containing multiple individual JSON texts or
         * containing different protocol data.
         * @param data The character string to parse.
         * @param ec Set to the error, if any occurred.
         * @return The number of characters consumed from the
         * buffer.
         */
        std::size_t write_some(std::string_view data, std::error_code& ec) {
            return parser_.write_some(false, data, ec);
        }

        /*!
         * @brief Parse a buffer containing a complete JSON
         * text. This function parses a complete JSON text
         * contained in the specified character buffer.
         * Additional characters past the end of the complete
         * JSON text are ignored. The function returns the
         * actual number of characters parsed, which may be less
         * than the size of the input. This allows parsing of a
         * buffer containing multiple individual JSON texts or
         * containing different protocol data.
         * @param data The character string to parse.
         * @return The number of characters consumed from the
         * buffer.
         */
        std::size_t write_some(std::string_view data) {
            std::error_code ec;
            const std::size_t n = write_some(data, ec);
            if (ec) {
                throw std::system_error{ec};
            }
            return n;
        }

        /*!
         * @brief Parse a buffer containing a complete JSON
         * text. This function parses a complete JSON text
         * contained in the specified character buffer.
         * Additional characters past the end of the complete
         * JSON text are ignored. The function returns the
         * actual number of characters parsed, which may be less
         * than the size of the input. This allows parsing of a
         * buffer containing multiple individual JSON texts or
         * containing different protocol data.
         * @param data A pointer to a buffer of size characters
         * to parse.
         * @param n The number of characters pointed to by data.
         * @param ec Set to the error, if any occurred.
         * @return The number of characters consumed from the
         * buffer.
         */
        std::size_t write_some(const char* data, std::size_t n,
                               std::error_code& ec) {
            return write_some(std::string_view{data, n}, ec);
        }

        /*!
         * @brief Parse a buffer containing a complete JSON
         * text. This function parses a complete JSON text
         * contained in the specified character buffer.
         * Additional characters past the end of the complete
         * JSON text are ignored. The function returns the
         * actual number of characters parsed, which may be less
         * than the size of the input. This allows parsing of a
         * buffer containing multiple individual JSON texts or
         * containing different protocol data.
         * @param data A pointer to a buffer of size characters
         * to parse.
         * @param n The number of characters pointed to by data.
         * @return The number of characters consumed from the
         * buffer.
         */
        std::size_t write_some(const char* data, std::size_t n) {
            return write_some(std::string_view{data, n});
        }

        /*!
         * @brief Parse a buffer containing a complete JSON
         * text. This function parses a complete JSON text
         * contained in the specified character buffer. The
         * entire buffer must be consumed; if there are
         * additional characters past the end of the complete
         * JSON text, the parse fails and an error is returned.
         * @param data The character string to parse.
         * @param ec Set to the error, if any occurred.
         * @return The number of characters consumed from the
         * buffer.
         */
        std::size_t write(std::string_view data, std::error_code& ec) {
            std::size_t n = parser_.write_some(false, data, ec);
            if (n != data.size() && !ec) {
                ec = make_error(error::extra_data);
            }
            return n;
        }

        /*!
         * @brief Parse a buffer containing a complete JSON
         * text. This function parses a complete JSON text
         * contained in the specified character buffer. The
         * entire buffer must be consumed; if there are
         * additional characters past the end of the complete
         * JSON text, the parse fails and an error is returned.
         * @param data The character string to parse.
         * @return The number of characters consumed from the
         * buffer.
         */
        std::size_t write(std::string_view data) {
            std::error_code ec;
            const std::size_t n = write(data, ec);
            if (ec) {
                throw std::system_error{ec};
            }
            return n;
        }

        /*!
         * @brief Parse a buffer containing a complete JSON
         * text. This function parses a complete JSON text
         * contained in the specified character buffer. The
         * entire buffer must be consumed; if there are
         * additional characters past the end of the complete
         * JSON text, the parse fails and an error is returned.
         * @param data A pointer to a buffer of size characters
         * to parse.
         * @param n The number of characters pointed to by data.
         * @param ec Set to the error, if any occurred.
         * @return The number of characters consumed from the
         * buffer.
         */
        std::size_t write(const char* data, std::size_t n,
                          std::error_code& ec) {
            return write(std::string_view{data, n}, ec);
        }

        /*!
         * @brief Parse a buffer containing a complete JSON
         * text. This function parses a complete JSON text
         * contained in the specified character buffer. The
         * entire buffer must be consumed; if there are
         * additional characters past the end of the complete
         * JSON text, the parse fails and an error is returned.
         * @param data A pointer to a buffer of size characters
         * to parse.
         * @param n The number of characters pointed to by data.
         * @return The number of characters consumed from the
         * buffer.
         */
        std::size_t write(const char* data, std::size_t n) {
            return write(std::string_view{data, n});
        }

    private:
        basic_parser<detail::dom_handler> parser_;
    };

    /*!
     * @brief This function parses input in one step to produce a complete
     * JSON value. If the input does not contain a complete serialized JSON,
     * an error occurs.
     * @param data The string to parse.
     * @param ec Set to the error, if any occurred.
     * @return A value representing the parsed JSON. On error a null value
     * is returned.
     */
    inline value parse(std::string_view data, std::error_code& ec) {
        parser p;
        p.write(data, ec);
        if (!ec) {
            return p.release();
        }
        return {};
    }

    /*!
     * @brief This function parses input in one step to produce a complete
     * JSON value. If the input does not contain a complete serialized JSON,
     * an exception is thrown.
     * @param data The string to parse.
     * @return A value representing the parsed JSON.
     */
    inline value parse(std::string_view data) {
        std::error_code ec;
        auto val = parse(data, ec);
        if (ec) {
            throw std::system_error{ec};
        }
        return val;
    }

    /*!
     * @brief A DOM parser for JSON text contained in multiple buffers.
     * This class is used to parse a JSON text contained in a series of
     * one or more character buffers, into a value container.
     */
    class stream_parser {
    public:
        /*!
         * @brief Return the parsed JSON as a value.
         * This returns the parsed value, or throws an exception
         * if the parsing is incomplete or failed. If
         * !this->done(), calls finish() first. It is necessary
         * to call reset after calling this function in order to
         * parse another JSON text.
         * @return The parsed value. Ownership of this value is
         * transferred to the caller.
         */
        value release() noexcept {
            return parser_.handler().release();
        }

        /*!
         * @brief Reset the parser for a new JSON text.
         * This function is used to reset the parser to prepare
         * it for parsing a new complete JSON text. Any previous
         * partial results are destroyed.
         */
        void reset() noexcept {
            parser_.reset();
        }

        /*!
         * @brief Check if a complete JSON text has been parsed.
         * This function returns true when all of these
         * conditions are met: A complete serialized JSON text
         * has been presented to the parser, and No error has
         * occurred since the parser was constructed, or since
         * the last call to @ref reset
         * @return True if parsing is done and successful, and
         * false otherwise.
         */
        bool done() const noexcept {
            return parser_.done();
        }

        /*!
         * @brief Indicate the end of JSON input.
         * This function is used to indicate that there are no
         * more character buffers in the current JSON text being
         * parsed. If the resulting JSON text is incomplete,
         * assigns the relevant std::error_code to @p ec.
         * @param ec Set to the error, if any occurred.
         */
        void finish(std::error_code& ec) noexcept {
            parser_.finish(ec);
        }

        /*!
         * @brief Indicate the end of JSON input.
         * This function is used to indicate that there are no
         * more character buffers in the current JSON text being
         * parsed. If the resulting JSON text is incomplete,
         * throws an exception.
         */
        void finish() {
            std::error_code ec;
            parser_.finish(ec);
            if (ec) {
                throw std::system_error{ec};
            }
        }

        /*!
         * @brief Parse a buffer containing all or part of a
         * complete JSON text. This function parses JSON text
         * contained in the specified character buffer. If
         * parsing completes, any additional characters past the
         * end of the complete JSON text are ignored. The
         * function returns the actual number of characters
         * parsed, which may be less than the size of the input.
         * This allows parsing of a buffer containing multiple
         * individual JSON texts or containing different
         * protocol data.
         * @param data The character string to parse.
         * @param ec Set to the error, if any occurred.
         * @return The number of characters consumed from the
         * buffer.
         */
        std::size_t write_some(std::string_view data, std::error_code& ec) {
            return parser_.write_some(true, data, ec);
        }

        /*!
         * @brief Parse a buffer containing all or part of a
         * complete JSON text. This function parses JSON text
         * contained in the specified character buffer. If
         * parsing completes, any additional characters past the
         * end of the complete JSON text are ignored. The
         * function returns the actual number of characters
         * parsed, which may be less than the size of the input.
         * This allows parsing of a buffer containing multiple
         * individual JSON texts or containing different
         * protocol data.
         * @param data The character string to parse.
         * @return The number of characters consumed from the
         * buffer.
         */
        std::size_t write_some(std::string_view data) {
            std::error_code ec;
            const std::size_t n = parser_.write_some(true, data, ec);
            if (ec) {
                throw std::system_error{ec};
            }
            return n;
        }

        /*!
         * @brief Parse a buffer containing all or part of a
         * complete JSON text. This function parses JSON text
         * contained in the specified character buffer. If
         * parsing completes, any additional characters past the
         * end of the complete JSON text are ignored. The
         * function returns the actual number of characters
         * parsed, which may be less than the size of the input.
         * This allows parsing of a buffer containing multiple
         * individual JSON texts or containing different
         * protocol data.
         * @param data A pointer to a buffer of size characters
         * to parse.
         * @param n The number of characters pointed to by data.
         * @param ec Set to the error, if any occurred.
         * @return The number of characters consumed from the
         * buffer.
         */
        std::size_t write_some(const char* data, std::size_t n,
                               std::error_code& ec) {
            return write_some(std::string_view{data, n}, ec);
        }

        /*!
         * @brief Parse a buffer containing all or part of a
         * complete JSON text. This function parses JSON text
         * contained in the specified character buffer. If
         * parsing completes, any additional characters past the
         * end of the complete JSON text are ignored. The
         * function returns the actual number of characters
         * parsed, which may be less than the size of the input.
         * This allows parsing of a buffer containing multiple
         * individual JSON texts or containing different
         * protocol data.
         * @param data A pointer to a buffer of size characters
         * to parse.
         * @param n The number of characters pointed to by data.
         * @return The number of characters consumed from the
         * buffer.
         */
        std::size_t write_some(const char* data, std::size_t n) {
            return write_some(std::string_view{data, n});
        }

        /*!
         * @brief Parse a buffer containing all or part of a
         * complete JSON text. This function parses all or part
         * of a JSON text contained in the specified character
         * buffer. The entire buffer must be consumed; if there
         * are additional characters past the end of the
         * complete JSON text, the parse fails and an error is
         * returned.
         * @param data The character string to parse.
         * @param ec Set to the error, if any occurred.
         * @return The number of characters consumed from the
         * buffer.
         */
        std::size_t write(std::string_view data, std::error_code& ec) {
            const std::size_t n = parser_.write_some(true, data, ec);
            if (n != data.size() && !ec) {
                ec = make_error(error::extra_data);
            }
            return n;
        }

        /*!
         * @brief Parse a buffer containing all or part of a
         * complete JSON text. This function parses all or part
         * of a JSON text contained in the specified character
         * buffer. The entire buffer must be consumed; if there
         * are additional characters past the end of the
         * complete JSON text, the parse fails and an error is
         * returned.
         * @param data The character string to parse.
         * @return The number of characters consumed from the
         * buffer.
         */
        std::size_t write(std::string_view data) {
            std::error_code ec;
            const std::size_t n = write(data, ec);
            if (ec) {
                throw std::system_error{ec};
            }
            return n;
        }

        /*!
         * @brief Parse a buffer containing all or part of a
         * complete JSON text. This function parses all or part
         * of a JSON text contained in the specified character
         * buffer. The entire buffer must be consumed; if there
         * are additional characters past the end of the
         * complete JSON text, the parse fails and an error is
         * returned.
         * @param data A pointer to a buffer of size characters
         * to parse.
         * @param n The number of characters pointed to by @p
         * data.
         * @param ec Set to the error, if any occurred.
         * @return The number of characters consumed from the
         * buffer.
         */
        std::size_t write(const char* data, std::size_t n,
                          std::error_code& ec) {
            return write(std::string_view{data, n}, ec);
        }

        /*!
         * @brief Parse a buffer containing all or part of a
         * complete JSON text. This function parses all or part
         * of a JSON text contained in the specified character
         * buffer. The entire buffer must be consumed; if there
         * are additional characters past the end of the
         * complete JSON text, the parse fails and an error is
         * returned.
         * @param data A pointer to a buffer of size characters
         * to parse.
         * @param n The number of characters pointed to by @p
         * data.
         * @return The number of characters consumed from the
         * buffer.
         */
        std::size_t write(const char* data, std::size_t n) {
            return write(std::string_view{data, n});
        }

    private:
        basic_parser<detail::dom_handler> parser_;
    };
} // namespace rad::json