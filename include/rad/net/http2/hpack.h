#pragma once
#include <rad/libbase.h>
#include <rad/net/http/http_parser.h>
#include <rad/ring_buffer.h>

#include <optional>

namespace RAD_LIB_NAMESPACE::net::http2 {
    /*!
     * @brief Get the required buffer length to encode @p text using HPACK
     * huffman encoding.
     * @param text The text to get its encoded length.
     * @return The buffer length required to encode @p text.
     */
    RAD_EXPORT_DECL std::size_t
    encoded_huffman_len(std::string_view text) noexcept;

    /*!
     * @brief Get the required buffer length to decode @p buff which is
     * encoded using HPACK huffman encoding.
     * @param buff The encoded buffer to get its decoded length.
     * @return The buffer length required to decode @p buff, or 0 if the
     * buffer is empty or an error occurs.
     */
    RAD_EXPORT_DECL std::size_t
    decoded_huffman_len(std::span<const uint8_t> buff) noexcept;

    /*!
     * @brief Encode @p text using HPACK huffman encoding.
     * @param text The text to encode using HPACK huffman encoding.
     * @param out The buffer pointer where the encoded data will be written.
     * This buffer must be large enough to hold the entire encoded data.
     * To get the required buffer length use @ref encoded_huffman_len().
     * @return The count of encoded bytes written to @p out.
     */
    RAD_EXPORT_DECL std::size_t encode_huffman(std::string_view text,
                                               uint8_t* out) noexcept;

    /*!
     * @brief Decode @p buff which is encoded using HPACK huffman encoding.
     * @param buff The encoded buffer which is encoded using HPACK huffman
     * encoding.
     * @param out The buffer pointer where the decoded data will be written.
     * This buffer must be large enough to hold the entire decoded data.
     * To get the required buffer length use @ref decoded_huffman_len().
     * @param ec Cleared on success, and set to error on failure.
     * @return The count of decoded bytes written to @p out.
     */
    RAD_EXPORT_DECL size_t decode_huffman(std::span<const uint8_t> buff,
                                          uint8_t* out, std::errc& ec);

    /*!
     * @brief HTTP 2 methods (verbs) are the same as HTTP 1/1.1
     */
    using verb = http::verb;

    /*!
     * @brief HTTP 2 headers (verbs) are the same as HTTP 1/1.1
     */
    using headers = http::headers;

    /*!
     * @brief HTTP 2 headers (verbs) are the same as HTTP 1/1.1
     */
    using headers_view = http::headers_view;

    /*!
     * @brief HTTP 2 request (verbs) is the same as HTTP 1/1.1
     */
    using request = http::request;

    /*!
     * @brief HTTP 2 response (verbs) is the same as HTTP 1/1.1
     */
    using response = http::response;

    /*!
     * @brief The dynamic table used by HPACK encoder and decoder.
     */
    class dynamic_table {
    public:
        /*!
         * @brief Get the maximum allowed size of the dynamic
         * table in bytes.
         *
         * The size of the dynamic table is the sum of the size
         * of its entries. The size of an entry is the sum of
         * its name's length in octets, its value's length in
         * octets, and 32. The size of an entry is calculated
         * using the length of its name and value without any
         * Huffman encoding applied.
         * @return The maximum allowed size of the dynamic table
         * in bytes.
         */
        size_t max_size() const noexcept {
            return max_size_;
        }

        /*!
         * @brief Set the maximum allowed size of the dynamic
         * table in bytes.
         *
         * Whenever the maximum size for the dynamic table is
         * reduced, entries are evicted from the end of the
         * dynamic table until the size of the dynamic table is
         * less than or equal to the maximum size.
         *
         * The size of the dynamic table is the sum of the size
         * of its entries. The size of an entry is the sum of
         * its name's length in octets, its value's length in
         * octets, and 32. The size of an entry is calculated
         * using the length of its name and value without any
         * Huffman encoding applied.
         * @param n The maximum allowed size of the dynamic
         * table in bytes.
         */
        RAD_EXPORT_DECL void set_max_size(std::size_t n) noexcept;

        /*!
         * @brief Add a new header entry to the table.
         *
         * Before a new entry is added to the dynamic table,
         * entries are evicted from the end of the dynamic table
         * until the size of the dynamic table is less than or
         * equal to (maximum size - new entry size) or until the
         * table is empty.
         *
         * If the size of the new entry is less than or equal to
         * the maximum size, that entry is added to the table.
         * It is not an error to attempt to add an entry that is
         * larger than the maximum size; an attempt to add an
         * entry larger than the maximum size causes the table
         * to be emptied of all existing entries and results in
         * an empty table.
         *
         * @param name The header name.
         * @param value The header value.
         */
        RAD_EXPORT_DECL void add(std::string_view name, std::string_view value);

        /*!
         * @brief Get the index of the header with @p name and
         * @p value.
         *
         * Returned indexed are incremented by 61 which is the
         * count of predefined http 2 headers.
         * @param name The header name.
         * @param value The value name.
         * @return The index + 61 of the header with @p name and
         * @p value if found, otherwise nullopt is returned.
         */
        RAD_EXPORT_DECL std::optional<size_t>
        find(std::string_view name, std::string_view value) const noexcept;

        /*!
         * @brief Get the index of the header with @p name.
         *
         * Returned indexed are incremented by 61 which is the
         * count of predefined http 2 headers.
         * @param name The header name.
         * @return The index + 61 of the header with @p name if
         * found, otherwise nullopt is returned.
         */
        RAD_EXPORT_DECL std::optional<size_t>
        find(std::string_view name) const noexcept;

        /*!
         * @brief Get the count of headers entries in the table.
         * @return The count of headers entries in the table.
         */
        std::size_t count() const noexcept {
            return headers_.size();
        }

        /*!
         * @brief Get a header from the table at index @p i.
         * The last (newest) added header is at index 0, while
         * the first (oldest) added header is at index count()
         * - 1. If @p i is greater than or equal to the count of
         * entries in the table, a null pointer is returned.
         * @param i The 0 based index of the header entry to
         * get.
         * @return A pointer to the header at index @p i, or
         * null.
         */
        const std::pair<std::string, std::string>* get_header(size_t i) {
            if (i >= headers_.size()) {
                return nullptr;
            }
            return &headers_[headers_.size() - i - 1];
        }

        const ring_buffer<std::pair<std::string, std::string>>&
        headers() const {
            return headers_;
        }

    private:
        RAD_EXPORT_DECL void remove_size(size_t n) noexcept;

        ring_buffer<std::pair<std::string, std::string>> headers_;
        size_t max_size_ = 0;
        size_t headers_size_ = 0;
    };

    /*!
     * @brief HPACK encoder.
     */
    class hpack_encoder {
    public:
        /*!
         * @brief Set the maximum allowed size of the dynamic
         * table in bytes.
         *
         * Whenever the maximum size for the dynamic table is
         * reduced, entries are evicted from the end of the
         * dynamic table until the size of the dynamic table is
         * less than or equal to the maximum size.
         *
         * The size of the dynamic table is the sum of the size
         * of its entries. The size of an entry is the sum of
         * its name's length in octets, its value's length in
         * octets, and 32. The size of an entry is calculated
         * using the length of its name and value without any
         * Huffman encoding applied.
         * @param n The maximum allowed size of the dynamic
         * table in bytes.
         */
        void set_max_table_size(size_t n) {
            dynamic_table_.set_max_size(n);
        }

        /*!
         * @brief Encode headers @p hdrs and write the
         * encoded buffer result into dynamic buffer @p out.
         *
         * Headers are encoded without any conversion for
         * compatiblity between HTTP 1.1 and HTTP 2.
         *
         * The use of this method is discouraged and it is
         * provided mainly for testing.
         *
         * Instead, use the encode methods that takes a request
         * or a response that is compatible with HTTP 1.1
         * requests and responses.
         * @param hdrs The headers to encode.
         * @param out The dynamic output buffer where encoded
         * data will be appended.
         * @param indexed Whether to append each header to the
         * dynamic table or not.
         * @param huffman Whether to use huffman encoding to
         * compress the headers or not.
         * @param never_indexed Whether to use indexed headers
         * if available or not.
         */
        RAD_EXPORT_DECL void encode(const headers& hdrs, dynamic_buffer out,
                                    bool indexed = true, bool huffman = false,
                                    bool never_indexed = false);

        /*!
         * @brief Encode an HTTP 1.1 compatible request and
         * write the encoded buffer result into dynamic buffer
         * @p out.
         *
         * The stored scheme will be encoded for non CONNECT
         * requests as the pseudo :scheme header.
         *
         * The stored host will be encoded for non CONNECT
         * requests as the pseudo :authority header, if no host
         * header is provided in the headers list.
         * @param method The request method. It will be encoded
         * as the pseudo :method header.
         * @param path The request target. For CONNECT requests
         * this will be encoded as the pseudo :authority header.
         * Otherwise, it will be encoded as the pseudo :path
         * header.
         * @param hdrs The request headers.
         * For CONNECT requests, the host header is skipped.
         * For other requests, the host header will be encoded
         * as the pseudo :authority header.
         * @param out The dynamic output buffer where encoded
         * data will be appended.
         * @param indexed Whether to append each header to the
         * dynamic table or not.
         * @param huffman Whether to use huffman encoding to
         * compress the headers or not.
         * @param never_indexed Whether to use indexed headers
         * if available or not.
         * @return
         */
        RAD_EXPORT_DECL void encode(http::verb method, std::string_view path,
                                    const headers& hdrs, dynamic_buffer out,
                                    bool indexed = true, bool huffman = false,
                                    bool never_indexed = false);

        /*!
         * @brief Encode an HTTP 1.1 compatible request and
         * write the encoded buffer result into dynamic buffer
         * @p out.
         *
         * The stored scheme will be encoded for non CONNECT
         * requests as the pseudo :scheme header.
         *
         * The stored host will be encoded for non CONNECT
         * requests as the pseudo :authority header, if no host
         * header is provided in the headers list.
         * @param req The request to encode.
         *
         * The request method will be encoded as the pseudo
         * :method header.
         *
         * The request target, for CONNECT requests will be
         * encoded as the pseudo :authority header. Otherwise,
         * it will be encoded as the pseudo :path header.
         *
         * For CONNECT requests, the host header is skipped.
         *
         * For other requests, the host header will be encoded
         * as the pseudo :authority header.
         *
         * @param out The dynamic output buffer where encoded
         * data will be appended.
         * @param indexed Whether to append each header to the
         * dynamic table or not.
         * @param huffman Whether to use huffman encoding to
         * compress the headers or not.
         * @param never_indexed Whether to use indexed headers
         * if available or not.
         */
        void encode(const request& req, dynamic_buffer out, bool indexed = true,
                    bool huffman = false, bool never_indexed = false) {
            encode(req.method, req.target, req.headers, out, indexed, huffman,
                   never_indexed);
        }

        /*!
         * @brief Encode an HTTP 1.1 compatible response and
         * write the encoded buffer result into dynamic buffer
         * @p out.
         * @param res_status The response status. It will be
         * encoded as the pseudo :status header.
         * @param hdrs The headers to encode.
         * @param out The dynamic output buffer where encoded
         * data will be appended.
         * @param indexed Whether to append each header to the
         * dynamic table or not.
         * @param huffman Whether to use huffman encoding to
         * compress the headers or not.
         * @param never_indexed Whether to use indexed headers
         * if available or not.
         */
        RAD_EXPORT_DECL void encode(uint32_t res_status, const headers& hdrs,
                                    dynamic_buffer out, bool indexed = true,
                                    bool huffman = false,
                                    bool never_indexed = false);

        /*!
         * @brief Encode an HTTP 1.1 compatible response and
         * write the encoded buffer result into dynamic buffer
         * @p out.
         * @param res The response to encode.
         *
         * The response status will be encoded as the pseudo
         * :status header.
         * @param out The dynamic output buffer where encoded
         * data will be appended.
         * @param indexed Whether to append each header to the
         * dynamic table or not.
         * @param huffman Whether to use huffman encoding to
         * compress the headers or not.
         * @param never_indexed Whether to use indexed headers
         * if available or not.
         */
        void encode(const response& res, dynamic_buffer out,
                    bool indexed = true, bool huffman = false,
                    bool never_indexed = false) {
            encode(res.status, res.headers, out, indexed, huffman,
                   never_indexed);
        }

        /*!
         * @brief Set the host name which is used to encode
         * :authority pseudo header if not given in the headers
         * list.
         * @param host The host name.
         */
        void set_host(std::string_view host) {
            host_ = host;
        }

        /*!
         * @brief Get the host name which is used to encode
         * :authority pseudo header if not given in the headers
         * list.
         * @return The host name.
         */
        std::string_view host() const noexcept {
            return host_;
        }

        /*!
         * @brief Set the scheme which is used to encode :scheme
         * pseudo header if not given in the headers list.
         * @param host The scheme.
         */
        void set_scheme(std::string_view scheme) {
            scheme_ = scheme;
        }

        /*!
         * @brief Get the scheme which is used to encode :scheme
         * pseudo header if not given in the headers list.
         * @return host The scheme.
         */
        std::string_view scheme() const noexcept {
            return scheme_;
        }

        /*!
         * @brief Get a const reference to the dynamic table.
         * @return A const reference to the dynamic table.
         */
        const dynamic_table& table() const {
            return dynamic_table_;
        }

    private:
        void encode_header(std::string_view name, std::string_view value,
                           dynamic_buffer out, bool huffman, bool indexed,
                           bool never_indexed);

        void encode_name_index(bool indexed, bool never_indexed, size_t index,
                               dynamic_buffer out);

        void encode_length_string(std::string_view text, bool huffman,
                                  dynamic_buffer out);

        dynamic_table dynamic_table_;
        std::string host_;
        std::string scheme_;
    };

    /*!
     * @brief HPACK decoder.
     */
    class hpack_decoder {
    public:
        /*!
         * @brief Set the maximum allowed size of the dynamic
         * table in bytes.
         *
         * Whenever the maximum size for the dynamic table is
         * reduced, entries are evicted from the end of the
         * dynamic table until the size of the dynamic table is
         * less than or equal to the maximum size.
         *
         * The size of the dynamic table is the sum of the size
         * of its entries. The size of an entry is the sum of
         * its name's length in octets, its value's length in
         * octets, and 32. The size of an entry is calculated
         * using the length of its name and value without any
         * Huffman encoding applied.
         * @param n The maximum allowed size of the dynamic
         * table in bytes.
         */
        void set_max_table_size(size_t n) {
            dynamic_table_.set_max_size(n);
        }

        /*!
         * @brief Decode headers block buffer @p input and put
         * the result into headers @p hdrs. The result headers
         * are not compatible with HTTP 1.1 headers if it
         * contains any pseudo HTTP 2 headers.
         *
         * This method is typically used for testing and to
         * decode the trailers.
         *
         * Instead, for decoding responses use the decode
         * methods that outputs a request or a response that is
         * compatible with HTTP 1.1 requests and responses.
         * @param input The headers block buffer.
         * @param hdrs The output headers.
         * @param ec Set to indicate error occured, if any.
         */
        RAD_EXPORT_DECL void decode(const_buffer input, headers& hdrs,
                                    std::error_code& ec);

        /*!
         * @brief Decode headers block buffer @p input and put
         * the result into request @p req. Pseudo headers are
         * handled as follows:
         *
         * Pseudo :method header is converted to verb and stored
         * in @p req method member.
         *
         * Pseudo :path is stored in @p req target member.
         *
         * Pseudo :authority is converted to host header for
         * compatibility with HTTP 1.1. If the request method is
         * CONNECT, the :authority value will be the request
         * target instead.
         *
         * Pseudo :scheme is compared to the stored scheme if
         * not empty, and if they are not equal the decoding
         * fails.
         *
         * If any required pseudo header is not found in the
         * headers, an error is returned. If a prohibited pseudo
         * header is found in the headers, an error is returned.
         * @param input The headers block buffer.
         * @param req The output request.
         * @param ec Set to indicate error occured, if any.
         */
        RAD_EXPORT_DECL void decode(const_buffer input, request& req,
                                    std::error_code& ec);

        /*!
         * @brief Decode headers block buffer @p input and put
         * the result into response @p res. Pseudo headers are
         * handled as follows:
         *
         * Pseudo :status is converted to number and stored in
         * @p res status member.
         *
         * If :status pseudo header is not found in the headers,
         * an error is returned. If a pseudo header other than
         * :status is found in the headers, an error is
         * returned.
         * @param input The headers block buffer.
         * @param res The output response.
         * @param ec Set to indicate error occured, if any.
         */
        RAD_EXPORT_DECL void decode(const_buffer input, response& res,
                                    std::error_code& ec);

        /*!
         * @brief Set the scheme which is used to validate
         * :scheme pseudo header if given in the headers list.
         * @param host The scheme.
         */
        void set_scheme(std::string_view scheme) {
            scheme_ = scheme;
        }

        /*!
         * @brief Get the scheme which is used to validate
         * :scheme pseudo header if given in the headers list.
         * @return host The scheme.
         */
        std::string_view scheme() const noexcept {
            return scheme_;
        }

        /*!
         * @brief Get a const reference to the dynamic table.
         * @return A const reference to the dynamic table.
         */
        const dynamic_table& table() const {
            return dynamic_table_;
        }

    private:
        size_t decode_header(const_buffer input, std::string& name,
                             std::string& value, std::errc& ec);

        size_t decode_header_name(const_buffer input, std::string& name,
                                  bool& indexed, bool& never_indexed,
                                  std::errc& ec);

        size_t decode_length_string(const_buffer input, std::string& text,
                                    std::errc& ec);

        dynamic_table dynamic_table_;
        std::string scheme_;
    };
} // namespace RAD_LIB_NAMESPACE::net::http2