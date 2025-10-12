#pragma once
#include <rad/libbase.h>
#include <rad/net/types.h>
#include <rad/string.h>

#include <optional>
#include <system_error>
#include <vector>

namespace rad::net::detail {
    enum class url_record_flags : std::uint32_t {
        none,
        host = 1 << 0,
        username = 1 << 1,
        password = 1 << 2,
        port = 1 << 3,
        query = 1 << 4,
        fragment = 1 << 5,
        invalid_host = 1 << 6,
        ipv4_host = 1 << 7,
        ipv6_host = 1 << 8,
        domain_host = 1 << 9,
        opaque_host = 1 << 10,
        empty_host = 1 << 11,
        opaque_path = 1 << 12,
        special_scheme = 1 << 13,
        empty_first_path_segment = 1 << 14,
    };

    RAD_OVERLOAD_ENUM_OPERATORS(url_record_flags);

    struct url_record {
        url_record_flags flags = url_record_flags::none;
        uint16_t port = 0;
        std::intptr_t password_pos = -1; // in userinfo

        std::string scheme;
        std::string userinfo;
        std::string host;
        std::string path;
        std::string query;
        std::string fragment;

        constexpr bool has_host() const noexcept {
            return flags & url_record_flags::host;
        }

        constexpr bool has_username() const noexcept {
            // if password_pos == 0 this means all of
            // userinfo is password
            return (flags & url_record_flags::username) && !userinfo.empty() &&
                   password_pos != 0;
        }

        constexpr bool has_password() const noexcept {
            return (flags & url_record_flags::password) && !userinfo.empty() &&
                   (password_pos >= 0 &&
                    static_cast<std::size_t>(password_pos) < userinfo.size());
        }

        constexpr bool has_port() const noexcept {
            return flags & url_record_flags::port;
        }

        constexpr bool is_opaque_path() const noexcept {
            return flags & url_record_flags::opaque_path;
        }

        constexpr bool has_query() const noexcept {
            return flags & url_record_flags::query;
        }

        constexpr bool has_fragment() const noexcept {
            return flags & url_record_flags::fragment;
        }

        constexpr bool is_special() const noexcept {
            return flags & url_record_flags::special_scheme;
        }

        constexpr bool is_ipv4_host() const noexcept {
            return flags & url_record_flags::ipv4_host;
        }

        constexpr bool is_ipv6_host() const noexcept {
            return flags & url_record_flags::ipv6_host;
        }

        constexpr bool is_domain_host() const noexcept {
            return flags & url_record_flags::domain_host;
        }

        constexpr bool is_opaque_host() const noexcept {
            return flags & url_record_flags::opaque_host;
        }

        constexpr bool is_empty_host() const noexcept {
            return flags & url_record_flags::empty_host;
        }

        constexpr bool is_invalid_host() const noexcept {
            return flags & url_record_flags::invalid_host;
        }

        constexpr bool is_empty_first_path_segment() const noexcept {
            return flags & url_record_flags::empty_first_path_segment;
        }

        constexpr bool can_has_userinfo_or_port() const noexcept {
            // A URL cannot have a username/password/port if
            // its host is null or the empty string, or its
            // scheme is "file".
            return has_host() && !host.empty() && scheme != "file";
        }

        void set_special() noexcept {
            flags |= url_record_flags::special_scheme;
        }

        void set_has_username() noexcept {
            flags |= url_record_flags::username;
        }

        void set_has_password() noexcept {
            flags |= url_record_flags::password;
        }

        void clear_host_flags() noexcept {
            flags &=
                ~(url_record_flags::empty_host | url_record_flags::ipv4_host |
                  url_record_flags::ipv6_host | url_record_flags::domain_host |
                  url_record_flags::opaque_host);
        }

        void clear_port_flag() noexcept {
            flags &= ~url_record_flags::port;
        }

        void set_host_flag(url_record_flags flag) noexcept {
            clear_host_flags();
            flags |= flag | url_record_flags::host;
        }

        void set_has_empty_host() noexcept {
            set_host_flag(url_record_flags::empty_host);
        }

        void set_has_ipv4_host() noexcept {
            set_host_flag(url_record_flags::ipv4_host);
        }

        void set_has_ipv6_host() noexcept {
            set_host_flag(url_record_flags::ipv6_host);
        }

        void set_has_domain_host() noexcept {
            set_host_flag(url_record_flags::domain_host);
        }

        void set_has_opaque_host() noexcept {
            set_host_flag(url_record_flags::opaque_host);
        }

        void set_has_invalid_host() noexcept {
            set_host_flag(url_record_flags::invalid_host);
        }

        void set_has_port() noexcept {
            flags |= url_record_flags::port;
        }

        void set_has_opaque_path() noexcept {
            flags |= url_record_flags::opaque_path;
        }

        void set_empty_first_path_segment() noexcept {
            flags |= url_record_flags::empty_first_path_segment;
        }

        void set_has_query() noexcept {
            flags |= url_record_flags::query;
        }

        void set_has_fragment() noexcept {
            flags |= url_record_flags::fragment;
        }

        void clear_has_query() noexcept {
            flags &= ~url_record_flags::query;
        }

        void clear_has_fragment() noexcept {
            flags &= ~url_record_flags::fragment;
        }

        constexpr std::string_view username() const noexcept {
            if (!has_username()) {
                return "";
            }
            if (password_pos < 0 ||
                static_cast<std::size_t>(password_pos) >= userinfo.size()) {
                return userinfo;
            }
            return subview(userinfo, 0, static_cast<std::size_t>(password_pos));
        }

        constexpr std::string_view password() const noexcept {
            if (!has_password()) {
                return "";
            }
            return subview(userinfo, password_pos);
        }

        constexpr std::string_view not_serialized_host() const noexcept {
            if (!has_host()) {
                return {};
            }
            else if (is_ipv6_host()) {
                std::string_view h = host;
                assert(h.size() > 2);
                h.remove_prefix(1);
                h.remove_suffix(1);
                return h;
            }
            else {
                return host;
            }
        }
    };

    struct url_view_parts {
        std::string_view scheme;

        std::optional<std::string_view> username;
        std::optional<std::string_view> password;

        std::optional<std::string_view> host;

        std::optional<uint16_t> port;

        std::string_view path;

        std::optional<std::string_view> query;

        std::optional<std::string_view> fragment;
    };

    RAD_EXPORT_DECL void parse_url_record(std::string_view input,
                                          url_record& new_url,
                                          const url_record* base,
                                          std::error_code& ec);

    RAD_EXPORT_DECL void serialize_url(const url_record& url,
                                       bool exclude_fragment,
                                       std::string& output);

    RAD_EXPORT_DECL std::optional<uint16_t>
    get_default_url_scheme_port(std::string_view scheme);

    bool url_domain_ends_in_number(std::string_view input);

    ipv4 parse_url_ipv4_host(std::string_view input, std::error_code& ec);

    std::string url_serialize_ipv6(const ipv6& address);

    RAD_EXPORT_DECL void url_percent_decode_userinfo(std::string_view userinfo,
                                                     std::string& out,
                                                     std::error_code& ec);

    RAD_EXPORT_DECL void url_percent_decode_host(std::string_view host,
                                                 std::string& out,
                                                 std::error_code& ec);

    RAD_EXPORT_DECL void url_percent_decode_path(std::string_view path,
                                                 std::string& out,
                                                 std::error_code& ec);

    RAD_EXPORT_DECL void url_percent_decode_query(std::string_view query,
                                                  std::string& out,
                                                  std::error_code& ec);

    RAD_EXPORT_DECL void url_percent_decode_fragment(std::string_view fragment,
                                                     std::string& out,
                                                     std::error_code& ec);

} // namespace rad::net::detail

namespace rad::net {
    /*!
     * @brief URL errors.
     */
    enum class url_error_code {
        none,
        invalid_url_unit,
        special_scheme_missing_following_solidus,
        missing_scheme_non_relative_url,
        host_missing,
        host_invalid_code_point,
        domain_invalid_code_point,
        ipv6_unclosed,
        ipv6_invalid,
        ipv4_too_many_parts,
        ipv4_non_numeric_part,
        ipv4_out_of_range,
        domain_to_ascii,
        port_invalid,
        port_out_of_range,
    };

    /*!
     * @brief Get a const reference to the URL error category.
     * @return A const reference to the URL error category.
     */
    RAD_EXPORT_DECL const std::error_category& url_category() noexcept;

    /*!
     * @brief Make `std::error_code` with URL error code and
     * URL error category.
     * @param e The URL error code.
     * @return A  `std::error_code` with URL error code and
     * URL error category.
     */
    inline std::error_code make_error(url_error_code e) noexcept {
        return std::error_code{static_cast<int>(e), url_category()};
    }

    /*!
     * @brief A URL is a struct that represents a universal identifier.
     * To build a URL construct or call parse() with a valid URL string.
     * To serialize a URL call href().
     */
    class url {
    public:
        /*!
         * @brief Construct an empty url.
         * The constructed url is invalid.
         */
        constexpr url() = default;

        /*!
         * @brief Construct a url and parse UTF-8 encoded url
         * string @p input. If parsing fails an exception is
         * thrown.
         * @param input The url UTF-8 encoded string to parse.
         */
        url(std::string_view input) {
            parse(input);
        }

        /*!
         * @brief Construct a url and parse UTF-8 encoded url
         * string @p input. If parsing fails an exception is
         * thrown.
         * @param base The base url.
         * @param input The url UTF-8 encoded string to parse.
         */
        url(const url& base, std::string_view input) {
            parse(base, input);
        }

        /*!
         * @brief Parse UTF-8 encoded url string @p input, and
         * if parsing succeeds replace the record of this url
         * with the new parsed url. All views pointing to
         * internal owned strings are invalidated. On error @p
         * ec is set to the corresponding error.
         * @param base Optional pointer to base url.
         * @param input The url UTF-8 encoded string to parse.
         * @param ec Set to error on failure, and cleared on
         * success.
         */
        void parse(const url* base, std::string_view input,
                   std::error_code& ec) {
            ec.clear();
            detail::url_record record;
            detail::parse_url_record(
                input, record, base == nullptr ? nullptr : &base->record_, ec);
            if (!ec) {
                record_ = std::move(record);
            }
        }

        /*!
         * @brief Parse UTF-8 encoded url string @p input, and
         * if parsing succeeds replace the record of this url
         * with the new parsed url. All views pointing to
         * internal owned strings are invalidated. On error @p
         * ec is set to the corresponding error.
         * @param base Optional pointer to base url.
         * @param input The url UTF-8 encoded string to parse.
         */
        void parse(const url* base, std::string_view input) {
            std::error_code ec;
            parse(base, input, ec);
            if (ec) {
                throw std::system_error{ec};
            }
        }

        /*!
         * @brief Parse UTF-8 encoded url string @p input, and
         * if parsing succeeds replace the record of this url
         * with the new parsed url. All views pointing to
         * internal owned strings are invalidated. On error @p
         * ec is set to the corresponding error.
         * @param input The url UTF-8 encoded string to parse.
         * @param ec Set to error on failure, and cleared on
         * success.
         */
        void parse(std::string_view input, std::error_code& ec) {
            parse(nullptr, input, ec);
        }

        /*!
         * @brief Parse UTF-8 encoded url string @p input, and
         * if parsing succeeds replace the record of this url
         * with the new parsed url. All views pointing to
         * internal owned strings are invalidated. On error @p
         * ec is set to the corresponding error.
         * @param base The base url.
         * @param input The url UTF-8 encoded string to parse.
         * @param ec Set to error on failure, and cleared on
         * success.
         */
        void parse(const url& base, std::string_view input,
                   std::error_code& ec) {
            parse(&base, input, ec);
        }

        /*!
         * @brief Parse UTF-8 encoded url string @p input, and
         * if parsing succeeds replace the record of this url
         * with the new parsed url. All views pointing to
         * internal owned strings are invalidated. On error an
         * exception is thrown.
         * @param input The url UTF-8 encoded string to parse.
         */
        void parse(std::string_view input) {
            parse(nullptr, input);
        }

        /*!
         * @brief Parse UTF-8 encoded url string @p input, and
         * if parsing succeeds replace the record of this url
         * with the new parsed url. All views pointing to
         * internal owned strings are invalidated. On error @p
         * ec is set to the corresponding error.
         * @param base The base url.
         * @param input The url UTF-8 encoded string to parse.
         * @param ec Set to error on failure, and cleared on
         * success.
         */
        void parse(const url& base, std::string_view input) {
            parse(&base, input);
        }

        /*!
         * @brief Check if this url is empty (invalid).
         * @return True if this url is empty (invalid),
         * otherwise false.
         */
        constexpr bool empty() const noexcept {
            // a valid url must have a scheme
            return record_.scheme.empty();
        }

        /*!
         * @brief Check if this url is special which means it
         * has a special scheme. Special schemes are: http,
         * https, ws, wss, ftp and file.
         * @return True if this url is special, otherwise false.
         */
        constexpr bool is_special() const noexcept {
            return record_.is_special();
        }

        /*!
         * @brief Check if this url has a username.
         * @return True if this url has a username, otherwise
         * false.
         */
        constexpr bool has_username() const noexcept {
            return record_.has_username();
        }

        /*!
         * @brief Check if this url has a password.
         * @return True if this url has a password, otherwise
         * false.
         */
        constexpr bool has_password() const noexcept {
            return record_.has_password();
        }

        /*!
         * @brief Check if this url has a host.
         * @return True if this url has a host, otherwise false.
         */
        constexpr bool has_host() const noexcept {
            return record_.has_host();
        }

        /*!
         * @brief Check if this url has a port.
         * @return True if this url has a port, otherwise false.
         */
        constexpr bool has_port() const noexcept {
            return record_.has_port();
        }

        /*!
         * @brief Check if the path of this url is opaque.
         * @return True if the path of this url is opaque,
         * otherwise port.
         */
        constexpr bool has_opaque_path() const noexcept {
            return record_.is_opaque_path();
        }

        /*!
         * @brief Check if this url has a query which may be
         * empty.
         * @return True if this url has a query, otherwise
         * false.
         */
        constexpr bool has_query() const noexcept {
            return record_.has_query();
        }

        /*!
         * @brief Check if this url has a query which may be
         * empty.
         * @return True if this url has a query, otherwise
         * false.
         */
        constexpr bool has_search() const noexcept {
            return has_query();
        }

        /*!
         * @brief Check if this url has a fragment which may be
         * empty.
         * @return True if this url has a fragment, otherwise
         * false.
         */
        constexpr bool has_fragment() const noexcept {
            return record_.has_fragment();
        }

        /*!
         * @brief Check if this url has a fragment which may be
         * empty.
         * @return True if this url has a fragment, otherwise
         * false.
         */
        constexpr bool has_hash() const noexcept {
            return has_fragment();
        }

        /*!
         * @brief Check if this url is allowed to have a user
         * name.
         * @return True if this url is allowed to have a user
         * name, otherwise false.
         */
        constexpr bool can_has_username() const noexcept {
            return record_.can_has_userinfo_or_port();
        }

        /*!
         * @brief Check if this url is allowed to have a
         * password.
         * @return True if this url is allowed to have a
         * password, otherwise false.
         */
        constexpr bool can_has_password() const noexcept {
            return record_.can_has_userinfo_or_port();
        }

        /*!
         * @brief Check if this url is allowed to have a port.
         * @return True if this url is allowed to have a port,
         * otherwise false.
         */
        constexpr bool can_has_port() const noexcept {
            return record_.can_has_userinfo_or_port();
        }

        /*!
         * @brief Check if the host of this url is an ipv4
         * address.
         * @return True if the host of this url is an ipv4
         * address, otherwise false.
         */
        constexpr bool is_host_ipv4() const noexcept {
            return record_.is_ipv4_host();
        }

        /*!
         * @brief Check if the host of this url is an ipv6
         * address.
         * @return True if the host of this url is an ipv6
         * address, otherwise false.
         */
        constexpr bool is_host_ipv6() const noexcept {
            return record_.is_ipv6_host();
        }

        /*!
         * @brief Check if the host of this url is a domain.
         * @return True if the host of this url is a domain,
         * otherwise false.
         */
        constexpr bool is_host_domain() const noexcept {
            return record_.is_domain_host();
        }

        /*!
         * @brief Check if the host of this url is opaque.
         * @return True if the host of this url is opaque,
         * otherwise false.
         */
        constexpr bool is_host_opaque() const noexcept {
            return record_.is_opaque_host();
        }

        /*!
         * @brief Check if the host of this url is empty.
         * @return True if the host of this url is empty,
         * otherwise false.
         */
        constexpr bool is_host_empty() const noexcept {
            return record_.is_empty_host();
        }

        /*!
         * @brief Return the serialization of this url.
         * @return The serialization of this url.
         */
        std::string href() const {
            std::string output;
            detail::serialize_url(record_, false, output);
            return output;
        }

        /*!
         * @brief Return the scheme of this url.
         * The returned string view points to the internal owned
         * string which may be invalidated by assigning the url,
         * destroying it or calling to parse().
         * @return The scheme of this url.
         */
        constexpr std::string_view scheme() const noexcept {
            return record_.scheme;
        }

        /*!
         * @brief Return the scheme of this url, followed by
         * (:). For example: (https:).
         * @return The scheme of this url, followed by (:).
         */
        std::string protocol() const {
            return record_.scheme + ':';
        }

        /*!
         * @brief Return the username of this url, if there is
         * one. The returned string view points to the internal
         * owned string which may be invalidated by assigning
         * the url, destroying it or calling to parse().
         * @return The username of this url.
         */
        constexpr std::string_view username() const noexcept {
            return record_.username();
        }

        /*!
         * @brief Return the password of this url, if there is
         * one. The returned string view points to the internal
         * owned string which may be invalidated by assigning
         * the url, destroying it or calling to parse().
         * @return The password of this url.
         */
        constexpr std::string_view password() const noexcept {
            return record_.password();
        }

        /*!
         * @brief Return the host of this url, followed by (:)
         * and the serialization of the port of this url, if
         * there is a port. For example: (example.org:123)
         * @return The host of this url, followed by (:) and the
         * serialization of the port of this url.
         */
        std::string host() const {
            // 1 Let url be this's URL.
            // 2 If url's host is null, then return the
            // empty string.
            if (!has_host()) {
                return "";
            }
            // 3 If url's port is null, return url's host,
            // serialized.
            if (!has_port()) {
                return std::string{hostname()};
            }
            // 4 Return url's host, serialized, followed by
            // U+003A
            // (:) and url's port, serialized.
            else {
                std::string out;
                out.reserve(hostname().size() + 1 + 4);
                out += hostname();
                out += ':';
                out += std::to_string(port());
                return out;
            }
        }

        /*!
         * @brief Return the host of this url without the port.
         * If the host is ipv6 then it is prefixed by '[' and
         * suffixed by ']' For example: (example.org)
         * @return The host of this url without the port.
         */
        std::string_view hostname() const {
            // 1 If this's URL's host is null, then return
            // the empty string. 2 Return this's URL's host,
            // serialized.
            return record_.host;
        }

        /*!
         * @brief Return a view of the host of this url without
         * additions.
         * @return View of the host of this url without
         * additions.
         */
        constexpr std::string_view host_view() const noexcept {
            return record_.not_serialized_host();
        }

        /*!
         * @brief Convert the host of this url to IPv4 and
         * return it. On conversion failure an exception is
         * thrown.
         * @return The IPv4 resulted from converting the host of
         * this url.
         */
        ipv4 host_ipv4() const {
            return ipv4{record_.host};
        }

        /*!
         * @brief Convert the host of this url to IPv6 and
         * return it. On conversion failure an exception is
         * thrown.
         * @return The IPv6 resulted from converting the host of
         * this url.
         */
        ipv6 host_ipv6() const {
            return ipv6{record_.host};
        }

        /*!
         * @brief Convert the host of this url to IPv4 and
         * return an address from it and the port of this url.
         * If there is no port zero is used. On conversion
         * failure an exception is thrown.
         * @return The IPv4 address resulted from converting the
         * host of this url and port.
         */
        ipv4_endpoint make_ipv4_endpoint() const {
            return ipv4_endpoint{host_ipv4(), port()};
        }

        /*!
         * @brief Convert the host of this url to IPv6 and
         * return an address from it and the port of this url.
         * If there is no port zero is used. On conversion
         * failure an exception is thrown.
         * @return The IPv6 address resulted from converting the
         * host of this url and port.
         */
        endpoint make_ipv6_endpoint() const {
            return endpoint{host_ipv6(), port()};
        }

        /*!
         * @brief Convert the host of this url to IPv4 or IPv6
         * and return an address from it and the port of this
         * url. If there is no port zero is used. On conversion
         * failure an exception is thrown.
         * @return The IPv4 or IPv6 address resulted from
         * converting the host of this url and port.
         */
        endpoint make_endpoint() const {
            if (is_host_ipv4()) {
                return endpoint{make_ipv4_endpoint()};
            }
            else {
                return endpoint{make_ipv6_endpoint()};
            }
        }

        /*!
         * @brief Return the numeric port of this url, if there
         * is a port. Otherwise if the scheme of this url has a
         * default port return it. Otherwise return zero.
         *
         * Default scheme ports are as follows:
         *
         * http, ws have default port 80
         *
         * https, wss have default port 443
         *
         * ftp has default port 21
         * @return Return the numeric port of this url.
         */
        uint16_t port() const noexcept {
            if (has_port()) {
                return record_.port;
            }
            return detail::get_default_url_scheme_port(record_.scheme)
                .value_or(0);
        }

        /*!
         * @brief Return the serialization of the port of this
         * url, if there is a port. Otherwise return an empty
         * string.
         * @return The serialization of the port of this url.
         */
        std::string port_string() const {
            if (has_port()) {
                return std::to_string(port());
            }
            return "";
        }

        /*!
         * @brief Return the serialized path of this url.
         * The returned string view points to the internal owned
         * string which may be invalidated by assigning the url,
         * destroying it or calling to parse().
         * @return The serialized path of this url.
         */
        constexpr std::string_view pathname() const noexcept {
            return record_.path;
        }

        /*!
         * @brief Return the query part of this url prefixed by
         * (?), or empty string if there is no query.
         * @return The query part of this url prefixed by (?).
         */
        std::string search() const {
            if (!has_query() || record_.query.empty()) {
                return "";
            }
            return "?" + record_.query;
        }

        /*!
         * @brief Return the query part of this url without (?),
         * or empty string if there is no query.
         * The returned string view points to the internal owned
         * string which may be invalidated by assigning the url,
         * destroying it or calling to parse().
         * @return The query part of this url without (?).
         */
        constexpr std::string_view query() const noexcept {
            return has_query() ? std::string_view{record_.query} : "";
        }

        /*!
         * @brief Return the fragment part of this url prefixed
         * by (#), or empty string if there is no fragment.
         * @return The fragment part of this url prefixed by
         * (#).
         */
        std::string hash() const {
            if (!has_fragment() || record_.fragment.empty()) {
                return "";
            }
            return "#" + record_.fragment;
        }

        /*!
         * @brief Return the fragment part of this url without
         * (#), or empty string if there is no fragment. The
         * returned string view points to the internal owned
         * string which may be invalidated by assigning the url,
         * destroying it or calling to parse().
         * @return The fragment part of this url without (#).
         */
        constexpr std::string_view fragment() const noexcept {
            return has_fragment() ? std::string_view{record_.fragment} : "";
        }

        /*!
         * @brief Get the percent decoded username, if any.
         * @param out The output string where percent decoded result will be
         * appended.
         * @param ec Set to indicate error occured, if any.
         */
        void decoded_username(std::string& out, std::error_code& ec) const {
            detail::url_percent_decode_userinfo(username(), out, ec);
        }

        /*!
         * @brief Get the percent decoded username, if any.
         * @param out The output string where percent decoded result will be
         * appended.
         */
        void decoded_username(std::string& out) const {
            std::error_code ec;
            decoded_username(out, ec);
            if (ec) {
                throw std::system_error{ec};
            }
        }

        /*!
         * @brief Get the percent decoded username, if any.
         * @param ec Set to indicate error occured, if any.
         * @return The percent decoded string.
         */
        std::string decoded_username(std::error_code& ec) const {
            std::string out;
            decoded_username(out, ec);
            return out;
        }

        /*!
         * @brief Get the percent decoded username, if any.
         * @return The percent decoded string.
         */
        std::string decoded_username() const {
            std::string out;
            decoded_username(out);
            return out;
        }

        /*!
         * @brief Get the percent decoded password, if any.
         * @param out The output string where percent decoded result will be
         * appended.
         * @param ec Set to indicate error occured, if any.
         */
        void decoded_password(std::string& out, std::error_code& ec) const {
            detail::url_percent_decode_userinfo(password(), out, ec);
        }

        /*!
         * @brief Get the percent decoded password, if any.
         * @param out The output string where percent decoded result will be
         * appended.
         */
        void decoded_password(std::string& out) const {
            std::error_code ec;
            decoded_password(out, ec);
            if (ec) {
                throw std::system_error{ec};
            }
        }

        /*!
         * @brief Get the percent decoded password, if any.
         * @param ec Set to indicate error occured, if any.
         * @return The percent decoded string.
         */
        std::string decoded_password(std::error_code& ec) const {
            std::string out;
            decoded_password(out, ec);
            return out;
        }

        /*!
         * @brief Get the percent decoded password, if any.
         * @return The percent decoded string.
         */
        std::string decoded_password() const {
            std::string out;
            decoded_password(out);
            return out;
        }

        /*!
         * @brief Get the percent decoded host name, if any.
         * The returned host does not include the port or the prefix '[' and
         * suffix ']' for IPv6.
         * @param out The output string where percent decoded result will be
         * appended.
         * @param ec Set to indicate error occured, if any.
         */
        void decoded_hostname(std::string& out, std::error_code& ec) const {
            detail::url_percent_decode_host(host_view(), out, ec);
        }

        /*!
         * @brief Get the percent decoded host name, if any.
         * The returned host does not include the port or the prefix '[' and
         * suffix ']' for IPv6.
         * @param out The output string where percent decoded result will be
         * appended.
         */
        void decoded_hostname(std::string& out) const {
            std::error_code ec;
            decoded_hostname(out, ec);
            if (ec) {
                throw std::system_error{ec};
            }
        }

        /*!
         * @brief Get the percent decoded host name, if any.
         * The returned host does not include the port or the prefix '[' and
         * suffix ']' for IPv6.
         * @param ec Set to indicate error occured, if any.
         * @return The percent decoded string.
         */
        std::string decoded_hostname(std::error_code& ec) const {
            std::string out;
            decoded_hostname(out, ec);
            return out;
        }

        /*!
         * @brief Get the percent decoded host name, if any.
         * The returned host does not include the port or the prefix '[' and
         * suffix ']' for IPv6.
         * @return The percent decoded string.
         */
        std::string decoded_hostname() const {
            std::string out;
            decoded_hostname(out);
            return out;
        }

        /*!
         * @brief Get the percent decoded path, if any.
         * @param out The output string where percent decoded result will be
         * appended.
         * @param ec Set to indicate error occured, if any.
         */
        void decoded_path(std::string& out, std::error_code& ec) const {
            detail::url_percent_decode_path(pathname(), out, ec);
        }

        /*!
         * @brief Get the percent decoded path, if any.
         * @param out The output string where percent decoded result will be
         * appended.
         */
        void decoded_path(std::string& out) const {
            std::error_code ec;
            decoded_path(out, ec);
            if (ec) {
                throw std::system_error{ec};
            }
        }

        /*!
         * @brief Get the percent decoded path, if any.
         * @param ec Set to indicate error occured, if any.
         * @return The percent decoded string.
         */
        std::string decoded_path(std::error_code& ec) const {
            std::string out;
            decoded_path(out, ec);
            return out;
        }

        /*!
         * @brief Get the percent decoded path, if any.
         * @return The percent decoded string.
         */
        std::string decoded_path() const {
            std::string out;
            decoded_path(out);
            return out;
        }

        /*!
         * @brief Get the percent decoded query, if any.
         * The returned string does not include the prefix '?'.
         * @param out The output string where percent decoded result will be
         * appended.
         * @param ec Set to indicate error occured, if any.
         */
        void decoded_query(std::string& out, std::error_code& ec) const {
            detail::url_percent_decode_query(query(), out, ec);
        }

        /*!
         * @brief Get the percent decoded query, if any.
         * The returned string does not include the prefix '?'.
         * @param out The output string where percent decoded result will be
         * appended.
         */
        void decoded_query(std::string& out) const {
            std::error_code ec;
            decoded_query(out, ec);
            if (ec) {
                throw std::system_error{ec};
            }
        }

        /*!
         * @brief Get the percent decoded query, if any.
         * The returned string does not include the prefix '?'.
         * @param ec Set to indicate error occured, if any.
         * @return The percent decoded string.
         */
        std::string decoded_query(std::error_code& ec) const {
            std::string out;
            decoded_query(out, ec);
            return out;
        }

        /*!
         * @brief Get the percent decoded query, if any.
         * The returned string does not include the prefix '?'.
         * @return The percent decoded string.
         */
        std::string decoded_query() const {
            std::string out;
            decoded_query(out);
            return out;
        }

        /*!
         * @brief Get the percent decoded fragment, if any.
         * The returned string does not include the prefix '#'.
         * @param out The output string where percent decoded result will be
         * appended.
         * @param ec Set to indicate error occured, if any.
         */
        void decoded_fragment(std::string& out, std::error_code& ec) const {
            detail::url_percent_decode_fragment(fragment(), out, ec);
        }

        /*!
         * @brief Get the percent decoded fragment, if any.
         * The returned string does not include the prefix '#'.
         * @param out The output string where percent decoded result will be
         * appended.
         */
        void decoded_fragment(std::string& out) const {
            std::error_code ec;
            decoded_fragment(out, ec);
            if (ec) {
                throw std::system_error{ec};
            }
        }

        /*!
         * @brief Get the percent decoded fragment, if any.
         * The returned string does not include the prefix '#'.
         * @param ec Set to indicate error occured, if any.
         * @return The percent decoded string.
         */
        std::string decoded_fragment(std::error_code& ec) const {
            std::string out;
            decoded_fragment(out, ec);
            return out;
        }

        /*!
         * @brief Get the percent decoded fragment, if any.
         * The returned string does not include the prefix '#'.
         * @return The percent decoded string.
         */
        std::string decoded_fragment() const {
            std::string out;
            decoded_fragment(out);
            return out;
        }

    private:
        detail::url_record record_;
    };
} // namespace rad::net