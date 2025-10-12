#pragma once
#include <rad/buffer.h>
#include <rad/function_view.h>
#include <rad/ring_buffer_consumer.h>
#include <rad/string.h>

#include <algorithm>
#include <charconv>
#include <optional>
#include <span>
#include <variant>
#include <vector>

namespace RAD_LIB_NAMESPACE::net::http::detail {

    RAD_EXPORT_DECL std::span<const std::string_view, 7>
    get_http_verbs() noexcept;

    RAD_EXPORT_DECL std::span<const std::string_view, 2>
    get_http_versions() noexcept;

    RAD_EXPORT_DECL std::span<const std::string_view, 353>
    get_http_fields_names() noexcept;
} // namespace RAD_LIB_NAMESPACE::net::http::detail

namespace RAD_LIB_NAMESPACE::net {
    class url;
}

namespace RAD_LIB_NAMESPACE::net::http {

    /*!
     * @brief HTTP CRLF line ending.
     */
    inline constexpr std::string_view CRLF = "\r\n";

    /*!
     * @brief HTTP methods.
     */
    enum class verb : uint8_t {
        /// Invalid method.
        invalid,
        /// GET
        get,
        /// POST
        post,
        /// PUT
        put,
        /// HEAD
        head,
        /// DELETE
        delete_,
        /// OPTIONS
        options,
        /// CONNECT
        connect,
    };

    /*!
     * @brief HTTP version.
     */
    enum class version : uint8_t {
        /// Invalid version.
        invalid,
        /// HTTP/1.0
        v1_0,
        /// HTTP/1.1
        v1_1,
    };

    /*!
     * @brief HTTP defined field names.
     */
    enum class field : unsigned short {
        unknown = 0,

        a_im,
        accept,
        accept_additions,
        accept_charset,
        accept_datetime,
        accept_encoding,
        accept_features,
        accept_language,
        accept_patch,
        accept_post,
        accept_ranges,
        access_control,
        access_control_allow_credentials,
        access_control_allow_headers,
        access_control_allow_methods,
        access_control_allow_origin,
        access_control_expose_headers,
        access_control_max_age,
        access_control_request_headers,
        access_control_request_method,
        age,
        allow,
        alpn,
        also_control,
        alt_svc,
        alt_used,
        alternate_recipient,
        alternates,
        apparently_to,
        apply_to_redirect_ref,
        approved,
        archive,
        archived_at,
        article_names,
        article_updates,
        authentication_control,
        authentication_info,
        authentication_results,
        authorization,
        auto_submitted,
        autoforwarded,
        autosubmitted,
        base,
        bcc,
        body,
        c_ext,
        c_man,
        c_opt,
        c_pep,
        c_pep_info,
        cache_control,
        caldav_timezones,
        cancel_key,
        cancel_lock,
        cc,
        close,
        comments,
        compliance,
        connection,
        content_alternative,
        content_base,
        content_description,
        content_disposition,
        content_duration,
        content_encoding,
        content_features,
        content_id,
        content_identifier,
        content_language,
        content_length,
        content_location,
        content_md5,
        content_range,
        content_return,
        content_script_type,
        content_style_type,
        content_transfer_encoding,
        content_type,
        content_version,
        control,
        conversion,
        conversion_with_loss,
        cookie,
        cookie2,
        cost,
        dasl,
        date,
        date_received,
        dav,
        default_style,
        deferred_delivery,
        delivery_date,
        delta_base,
        depth,
        derived_from,
        destination,
        differential_id,
        digest,
        discarded_x400_ipms_extensions,
        discarded_x400_mts_extensions,
        disclose_recipients,
        disposition_notification_options,
        disposition_notification_to,
        distribution,
        dkim_signature,
        dl_expansion_history,
        downgraded_bcc,
        downgraded_cc,
        downgraded_disposition_notification_to,
        downgraded_final_recipient,
        downgraded_from,
        downgraded_in_reply_to,
        downgraded_mail_from,
        downgraded_message_id,
        downgraded_original_recipient,
        downgraded_rcpt_to,
        downgraded_references,
        downgraded_reply_to,
        downgraded_resent_bcc,
        downgraded_resent_cc,
        downgraded_resent_from,
        downgraded_resent_reply_to,
        downgraded_resent_sender,
        downgraded_resent_to,
        downgraded_return_path,
        downgraded_sender,
        downgraded_to,
        ediint_features,
        eesst_version,
        encoding,
        encrypted,
        errors_to,
        etag,
        expect,
        expires,
        expiry_date,
        ext,
        followup_to,
        forwarded,
        from,
        generate_delivery_report,
        getprofile,
        hobareg,
        host,
        http2_settings,
        if_,
        if_match,
        if_modified_since,
        if_none_match,
        if_range,
        if_schedule_tag_match,
        if_unmodified_since,
        im,
        importance,
        in_reply_to,
        incomplete_copy,
        injection_date,
        injection_info,
        jabber_id,
        keep_alive,
        keywords,
        label,
        language,
        last_modified,
        latest_delivery_time,
        lines,
        link,
        list_archive,
        list_help,
        list_id,
        list_owner,
        list_post,
        list_subscribe,
        list_unsubscribe,
        list_unsubscribe_post,
        location,
        lock_token,
        man,
        max_forwards,
        memento_datetime,
        message_context,
        message_id,
        message_type,
        meter,
        method_check,
        method_check_expires,
        mime_version,
        mmhs_acp127_message_identifier,
        mmhs_authorizing_users,
        mmhs_codress_message_indicator,
        mmhs_copy_precedence,
        mmhs_exempted_address,
        mmhs_extended_authorisation_info,
        mmhs_handling_instructions,
        mmhs_message_instructions,
        mmhs_message_type,
        mmhs_originator_plad,
        mmhs_originator_reference,
        mmhs_other_recipients_indicator_cc,
        mmhs_other_recipients_indicator_to,
        mmhs_primary_precedence,
        mmhs_subject_indicator_codes,
        mt_priority,
        negotiate,
        newsgroups,
        nntp_posting_date,
        nntp_posting_host,
        non_compliance,
        obsoletes,
        opt,
        optional,
        optional_www_authenticate,
        ordering_type,
        organization,
        origin,
        original_encoded_information_types,
        original_from,
        original_message_id,
        original_recipient,
        original_sender,
        original_subject,
        originator_return_address,
        overwrite,
        p3p,
        path,
        pep,
        pep_info,
        pics_label,
        position,
        posting_version,
        pragma,
        prefer,
        preference_applied,
        prevent_nondelivery_report,
        priority,
        privicon,
        profileobject,
        protocol,
        protocol_info,
        protocol_query,
        protocol_request,
        proxy_authenticate,
        proxy_authentication_info,
        proxy_authorization,
        proxy_connection,
        proxy_features,
        proxy_instruction,
        public_,
        public_key_pins,
        public_key_pins_report_only,
        range,
        received,
        received_spf,
        redirect_ref,
        references,
        referer,
        referer_root,
        relay_version,
        reply_by,
        reply_to,
        require_recipient_valid_since,
        resent_bcc,
        resent_cc,
        resent_date,
        resent_from,
        resent_message_id,
        resent_reply_to,
        resent_sender,
        resent_to,
        resolution_hint,
        resolver_location,
        retry_after,
        return_path,
        safe,
        schedule_reply,
        schedule_tag,
        sec_websocket_accept,
        sec_websocket_extensions,
        sec_websocket_key,
        sec_websocket_protocol,
        sec_websocket_version,
        security_scheme,
        see_also,
        sender,
        sensitivity,
        server,
        set_cookie,
        set_cookie2,
        setprofile,
        sio_label,
        sio_label_history,
        slug,
        soapaction,
        solicitation,
        status_uri,
        strict_transport_security,
        subject,
        subok,
        subst,
        summary,
        supersedes,
        surrogate_capability,
        surrogate_control,
        tcn,
        te,
        timeout,
        title,
        to,
        topic,
        trailer,
        transfer_encoding,
        ttl,
        ua_color,
        ua_media,
        ua_pixels,
        ua_resolution,
        ua_windowpixels,
        upgrade,
        urgency,
        uri,
        user_agent,
        variant_vary,
        vary,
        vbr_info,
        version,
        via,
        want_digest,
        warning,
        www_authenticate,
        x_archived_at,
        x_device_accept,
        x_device_accept_charset,
        x_device_accept_encoding,
        x_device_accept_language,
        x_device_user_agent,
        x_frame_options,
        x_mittente,
        x_pgp_sig,
        x_ricevuta,
        x_riferimento_message_id,
        x_tiporicevuta,
        x_trasporto,
        x_verificasicurezza,
        x400_content_identifier,
        x400_content_return,
        x400_content_type,
        x400_mts_identifier,
        x400_originator,
        x400_received,
        x400_recipients,
        x400_trace,
        xref,
    };

    /*!
     * @brief HTTP defined status codes.
     */
    enum class response_status {
        // Informational (100 - 199)
        continue_ = 100,
        switching_protocols = 101,
        processing = 102,
        early_hints = 103,

        // Successful (200 - 299)
        ok = 200,
        created = 201,
        accepted = 202,
        non_authoritative_info = 203,
        no_content = 204,
        reset_content = 205,
        partial_content = 206,
        multi_status = 207,
        already_reported = 208,
        im_used = 209,

        // Redirection (300 - 399)
        multiple_choices = 300,
        moved_permanently = 301,
        found = 302,
        see_other = 303,
        not_modified = 304,
        use_proxy = 305,
        unused = 306,
        temporary_redirect = 307,
        permanent_redirect = 308,

        // Client error (400 - 499)
        bad_request = 400,
        unauthorized = 401,
        payment_required = 402,
        forbidden = 403,
        not_found = 404,
        method_not_allowed = 405,
        not_acceptable = 406,
        proxy_authentication_required = 407,
        request_timeout = 408,
        conflict = 409,
        gone = 410,
        length_required = 411,
        precondition_failed = 412,
        payload_too_large = 413,
        uri_too_long = 414,
        unsupported_media_type = 415,
        range_not_satisfiable = 416,
        expectation_failed = 417,
        im_a_teapot = 418,

        misdirected_request = 421,
        unprocessable_content = 422,
        locked = 423,
        failed_dependency = 424,
        too_early = 425,
        upgrade_required = 426,

        precondition_required = 428,
        too_many_requests = 429,

        request_header_fields_too_large = 431,

        unavailable_for_legal_reasons = 451,

        // Server error (500 - 599)
        internal_server_error = 500,
        not_implemented = 501,
        bad_gateway = 502,
        service_unavailable = 503,
        gateway_timeout = 504,
        http_version_not_supported = 505,
        variant_also_negotiates = 506,
        insufficient_storage = 507,
        loop_detected = 508,

        not_extended = 510,
        network_authentication_required = 511,
    };

    /*!
     * @brief Get a string view representing the HTTP verb in uppercase
     * (GET, POST,
     * ...)
     * @param v The HTTP verb code.
     * @return A string view representing the HTTP verb in uppercase.
     * The returned view is valid for the lifetime of this module (either
     * executable or shared library)
     */
    inline std::string_view verb_to_string(verb v) noexcept {
        size_t v_index = static_cast<size_t>(v) - 1;
        const auto http_verbs = detail::get_http_verbs();
        if (v_index >= http_verbs.size()) {
            return "";
        }
        return http_verbs[v_index];
    }

    /*!
     * @brief Get the HTTP method corresponding to the method string.
     * If the method string doesn't equal any valid HTTP method, an
     * invalid method is returned.
     * @param text A string view representing the HTTP method in uppercase.
     * @return The HTTP method code.
     */
    inline verb string_to_verb(std::string_view text) noexcept {
        uint32_t index = 0;
        const auto http_verbs = detail::get_http_verbs();
        for (auto v : http_verbs) {
            index += 1;
            if (v == text) {
                return static_cast<verb>(index);
            }
        }
        return verb::invalid;
    }

    /*!
     * @brief Get a string view representing the HTTP versions 1.0 or 1.1
     * @param v The HTTP version.
     * @return A string view representing the HTTP versions 1.0 or 1.1.
     * The returned view is valid for the lifetime of this module (either
     * executable or shared library)
     */
    inline std::string_view version_to_string(version v) noexcept {
        size_t index = static_cast<size_t>(v) - 1;
        const auto http_versions = detail::get_http_versions();
        if (index >= http_versions.size()) {
            return "";
        }
        return http_versions[index];
    }

    /*!
     * @brief Get the HTTP version corresponding to the version string.
     * If the version string doesn't equal any valid HTTP version, an
     * invalid version is returned.
     * @param text A string view representing the HTTP HTTP versions 1.0 or 1.1.
     * @return The HTTP version.
     */
    inline version string_to_version(std::string_view text) {
        uint32_t index = 0;
        const auto http_versions = detail::get_http_versions();
        for (auto v : http_versions) {
            index += 1;
            if (v == text) {
                return static_cast<version>(index);
            }
        }
        return version{};
    }

    /*!
     * @brief Get a string view representing the name of a predefined http
     * header.
     * @param f The http header name.
     * @return A string view representing the name of a predefined http
     * header. The returned view is valid for the lifetime of this module
     * (either executable or shared library)
     */
    inline std::string_view field_to_string(field f) noexcept {
        size_t index = static_cast<size_t>(f);
        const auto fields_names = detail::get_http_fields_names();
        if (index >= fields_names.size()) {
            return "";
        }
        return fields_names[index];
    }

    /*!
     * @brief HTTP error codes.
     */
    enum class error {
        no_error,
        end_of_stream,
        partial_message,

        bad_line_ending,
        bad_method,
        bad_target,
        bad_version,
        bad_status,
        bad_reason,
        bad_field,
        bad_value,
        bad_content_length,
        bad_transfer_encoding,
        bad_chunk,
        bad_chunk_extension,

        too_large_message,
        bad_response_code,
        unexpected_body,
        bad_scheme,
        too_many_redirections,
    };

    /*!
     * @brief Get a reference to the error category used by http error
     * codes.
     */
    RAD_EXPORT_DECL const std::error_category& http_category() noexcept;

    /*!
     * @brief Make http `std::error_code` using the passed error code and
     * the http error category.
     * @return An error code using the http error category.
     */
    inline std::error_code make_error(error error) noexcept {
        return std::error_code{static_cast<int>(error), http_category()};
    }

    /*!
     * @brief A list of http headers names and values.
     * The string type used by the list may be owning `std::string` or non
     * owning `std::string_view`. Great care must be taken when using non
     * owning strings.
     * @tparam StrType The type of string.
     */
    template <class StrType>
    class headers_base {
        template <typename>
        friend class headers_base;

    public:
        /*!
         * @brief The iterator type.
         */
        using iterator =
            typename std::vector<std::pair<StrType, StrType>>::iterator;

        /*!
         * @brief The const iterator type.
         */
        using const_iterator =
            typename std::vector<std::pair<StrType, StrType>>::const_iterator;

        /*!
         * @brief Construct an empty headers list.
         */
        headers_base() = default;

        /*!
         * @brief Construct headers list from initializer list.
         * @param hdrs The headers to insert to header list.
         */
        headers_base(
            std::initializer_list<std::pair<std::string_view, std::string_view>>
                hdrs) {
            for (const auto& [f, v] : hdrs) {
                insert(f, v);
            }
        }

        /*!
         * @brief Construct headers list from initializer list.
         * @param hdrs The headers to insert to header list.
         */
        headers_base(
            std::initializer_list<std::pair<field, std::string_view>> hdrs) {
            for (const auto& [f, v] : hdrs) {
                insert(f, v);
            }
        }

        /*!
         * @brief Get a reference to the headers list.
         * @return A reference to the headers list.
         */
        const std::vector<std::pair<StrType, StrType>>& headers() {
            return headers_;
        }

        /*!
         * @brief Get the begin iterator of the headers list.
         * @return The begin iterator of the headers list.
         */
        iterator begin() noexcept {
            return headers_.begin();
        }

        /*!
         * @brief Get the begin iterator of the headers list.
         * @return The begin iterator of the headers list.
         */
        const_iterator begin() const noexcept {
            return headers_.begin();
        }

        /*!
         * @brief Get the end iterator of the headers list.
         * @return The end iterator of the headers list.
         */
        iterator end() noexcept {
            return headers_.end();
        }

        /*!
         * @brief Get the end iterator of the headers list.
         * @return The end iterator of the headers list.
         */
        const_iterator end() const noexcept {
            return headers_.end();
        }

        /*!
         * @brief Get the count of headers in the list.
         * @return The count of headers in the list.
         */
        std::size_t size() const noexcept {
            return headers_.size();
        }

        /*!
         * @brief Check if the headers list is empty.
         * @return True if the headers list is empty, and false
         * otherwise.
         */
        bool empty() const noexcept {
            return headers_.empty();
        }

        /*!
         * @brief Reserve space in the headers list for @p n
         * headers to be inserted.
         * @param n The count of headers to reserve for.
         */
        void reserve(std::size_t n) {
            headers_.reserve(n);
        }

        /*!
         * @brief Insert a header name and value in the headers
         * list. If a header with the same name exists in the
         * headers list, a new header with @p name and the new
         * @p value will be inserted after it. Note that header
         * name comparison is case insensetive.
         * @param name The name of the header.
         * @param value The value of the header.
         */
        void insert(std::string_view name, std::string_view value) {
            auto it = find(name);
            if (it != headers_.end()) {
                std::advance(it, 1);
            }
            headers_.emplace(it, name, value);
        }

        /*!
         * @brief Insert a predefined header name and value in
         * the headers list. If a header with the same name
         * exists in the headers list, a new header with @p name
         * and the new @p value will be inserted after it. Note
         * that header name comparison is case insensetive.
         * @param name The predefined header name code.
         * @param value The value of the header.
         */
        void insert(field name, std::string_view value) {
            insert(field_to_string(name), value);
        }

        /*!
         * @brief Insert a header name and value in the headers
         * list. If a header with the same name exists in the
         * headers list it will be assigned the new value, and
         * no insertion will happen. Note that header name
         * comparison is case insensetive.
         * @param name The name of the header.
         * @param value The value of the header.
         */
        void set(std::string_view name, std::string_view value) {
            auto it = find(name);
            if (it != headers_.end()) {
                it->second = value;
                return;
            }
            headers_.emplace_back(name, value);
        }

        /*!
         * @brief Insert a predefined header name and value in
         * the headers list. If a header with the same name
         * exists in the headers list it will be assigned the
         * new value, and no insertion will happen. Note that
         * header name comparison is case insensetive.
         * @param name The predefined header name code.
         * @param value The value of the header.
         */
        void set(field name, std::string_view value) {
            set(field_to_string(name), value);
        }

        /*!
         * @brief Get count of the headers with the given @p
         * name in the list. Note that header name comparison is
         * case insensetive.
         * @param name The name of the header.
         * @return Count of the headers with the given @p name.
         */
        std::size_t count(std::string_view name) const noexcept {
            auto it = begin();
            const auto last = end();
            std::size_t n = 0;
            while (it != last) {
                it = std::find_if(
                    it, last, [&](const std::pair<StrType, StrType>& header) {
                        return iequal(header.first, name);
                    });
                if (it != last) {
                    std::advance(it, 1);
                    n += 1;
                }
            }
            return n;
        }

        /*!
         * @brief Check if a header with the given name exists
         * in the headers list. Note that header name comparison
         * is case insensetive.
         * @param name The name of the header.
         * @return True if a header with the given name exists
         * in the headers list, and false if not.
         */
        bool contains(std::string_view name) const noexcept {
            return find(name) != headers_.end();
        }

        /*!
         * @brief Check if a header with the given predefined
         * name exists in the headers list. Note that header
         * name comparison is case insensetive.
         * @param name The predefined header name code.
         * @return True if a header with the given predefined
         * name exists in the headers list, and false if not.
         */
        bool contains(field name) const noexcept {
            return contains(field_to_string(name));
        }

        /*!
         * @brief Search for a header in the list by its name
         * and return an iterator to it. Note that header name
         * comparison is case insensetive.
         * @param name The name of the header.
         * @return The iterator pointing to the header if found,
         * and the end iterator if not fount.
         */
        iterator find(std::string_view name) noexcept {
            return std::find_if(headers_.begin(), headers_.end(),
                                [&](const std::pair<StrType, StrType>& header) {
                                    return iequal(header.first, name);
                                });
        }

        /*!
         * @brief Search for a header in the list by its name
         * and return an iterator to it. Note that header name
         * comparison is case insensetive.
         * @param name The name of the header.
         * @return The iterator pointing to the header if found,
         * and the end iterator if not fount.
         */
        const_iterator find(std::string_view name) const noexcept {
            return std::find_if(headers_.begin(), headers_.end(),
                                [&](const std::pair<StrType, StrType>& header) {
                                    return iequal(header.first, name);
                                });
        }

        /*!
         * @brief Search for a predefined header in the list by
         * its predefined name and return an iterator to it.
         * Note that header name comparison is case insensetive.
         * @param name The predefined header name code.
         * @return The iterator pointing to the header if found,
         * and the end iterator if not fount.
         */
        iterator find(field name) noexcept {
            return find(field_to_string(name));
        }

        /*!
         * @brief Search for a predefined header in the list by
         * its predefined name and return an iterator to it.
         * Note that header name comparison is case insensetive.
         * @param name The predefined header name code.
         * @return The iterator pointing to the header if found,
         * and the end iterator if not fount.
         */
        const_iterator find(field name) const {
            return find(field_to_string(name));
        }

        /*!
         * @brief Return a span to the fields with the specified name.
         * @param name The field name.
         * @return A span to the fields with the specified name.
         */
        std::span<std::pair<StrType, StrType>>
        equal_range(std::string_view name) noexcept {
            auto first = find(name);
            if (first == end()) {
                return {};
            }
            auto last = std::next(first);
            while (last != end() && iequal(last->first, name)) {
                std::advance(last, 1);
            }
            return std::span{first, last};
        }

        /*!
         * @brief Return a span to the fields with the specified name.
         * @param name The field name.
         * @return A span to the fields with the specified name.
         */
        std::span<const std::pair<StrType, StrType>>
        equal_range(std::string_view name) const noexcept {
            auto first = find(name);
            if (first == end()) {
                return {};
            }
            auto last = std::next(first);
            while (last != end() && iequal(last->first, name)) {
                std::advance(last, 1);
            }
            return std::span{first, last};
        }

        /*!
         * @brief Return a span to the fields with the specified name.
         * @param name The field name.
         * @return A span to the fields with the specified name.
         */
        std::span<std::pair<StrType, StrType>>
        equal_range(field name) noexcept {
            return equal_range(field_to_string(name));
        }

        /*!
         * @brief Return a span to the fields with the specified name.
         * @param name The field name.
         * @return A span to the fields with the specified name.
         */
        std::span<const std::pair<StrType, StrType>>
        equal_range(field name) const noexcept {
            return equal_range(field_to_string(name));
        }

        /*!
         * @brief Append another headers list to this headers
         * list. If this list already contains a header whose
         * name already exists in the other header the value of
         * this header is replaced with the new one from @p
         * other. Note that header name comparison is case
         * insensetive.
         * @param other The other headers list to append.
         * @return The headers list itself.
         */
        headers_base& append(const headers_base<std::string>& other) {
            if (other.headers_.empty()) {
                return *this;
            }
            headers_.reserve(headers_.size() + other.headers_.size());
            for (const auto& header : other.headers_) {
                insert(header.first, header.second);
            }
            return *this;
        }

        /*!
         * @brief Append another headers list to this headers
         * list. If this list already contains a header whose
         * name already exists in the other header the value of
         * this header is replaced with the new one from @p
         * other. Note that header name comparison is case
         * insensetive.
         * @param other The other headers list to append.
         * @return The headers list itself.
         */
        headers_base& append(const headers_base<std::string_view>& other) {
            if (other.headers_.empty()) {
                return *this;
            }
            headers_.reserve(headers_.size() + other.headers_.size());
            for (const auto& header : other.headers_) {
                insert(header.first, header.second);
            }
            return *this;
        }

        /*!
         * @brief Remove all headers with given @p name from the
         * headers list if it already exists in the headers
         * list. Note that header name comparison is case
         * insensetive.
         * @param name The name of the header.
         */
        void remove(std::string_view name) noexcept {
            auto it = find(name);
            if (it == headers_.end()) {
                return;
            }
            while (1) {
                it = headers_.erase(it);
                if (it == headers_.end() || !iequal(it->first, name)) {
                    return;
                }
            }
        }

        /*!
         * @brief Remove all headers with given @p name from the
         * headers list if it already exists in the headers
         * list. Note that header name comparison is case
         * insensetive.
         * @param name The predefined header name code.
         */
        void remove(field name) noexcept {
            remove(field_to_string(name));
        }

        /*!
         * @brief Clear the headers list.
         */
        void clear() noexcept {
            headers_.clear();
        }

        /*!
         * @brief Get the required size in bytes to serialize
         * the headers in the http message.
         * @return The required size in bytes to serialize the
         * headers in the http message.
         */
        std::size_t serialized_size() const noexcept {
            if (headers_.empty()) {
                return 0;
            }
            // each header has 2 bytes for colon and space
            std::size_t size = (2 + CRLF.size()) * headers_.size();
            for (const auto& [f, v] : headers_) {
                size += f.size() + v.size();
            }
            return size;
        }

        friend bool operator==(const headers_base& lhs,
                               const headers_base& rhs) noexcept {
            return lhs.headers_ == rhs.headers_;
        }

        friend bool operator<(const headers_base& lhs,
                              const headers_base& rhs) noexcept {
            return lhs.headers_ < rhs.headers_;
        }

    private:
        std::vector<std::pair<StrType, StrType>> headers_;
    };

    /*!
     * @brief HTTP headers list that stores headers names and values using
     * owning `std::string`
     */
    using headers = headers_base<std::string>;
    /*!
     * @brief HTTP headers list that stores headers names and values using
     * non owning `std::string_view`
     */
    using headers_view = headers_base<std::string_view>;

    /*!
     * @brief Convert HTTP headers list with owning strings to a one with
     * view strings. The view strings in the returned headers point to the
     * owning strings in the input headers.
     * @param hdrs The HTTP headers with owning strings.
     * @return An HTTP headers with view strings.
     */
    inline headers_view get_headers_view(const headers& hdrs) {
        headers_view hdrs_view;
        hdrs_view.reserve(hdrs.size());
        for (const auto& [f, v] : hdrs) {
            hdrs_view.insert(f, v);
        }
        return hdrs_view;
    }

    /*!
     * @brief Identity function that returns the same reference to the input
     * headers view.
     * @param hdrs The HTTP headers with view strings.
     * @return The same reference to the input headers view.
     */
    inline const headers_view&
    get_headers_view(const headers_view& hdrs) noexcept {
        return hdrs;
    }

    /*!
     * @brief Headers iterator interface to iterate both view and owning
     * headers.
     *
     * The iterator is invalidated by any operation on the headers that
     * invalidates the iterators.
     */
    class headers_iterator {
    public:
        virtual ~headers_iterator() = default;

        /*!
         * @brief Seek the headers iterator to the begin.
         */
        virtual void return_to_begin() noexcept = 0;

        /*!
         * @brief Get the next header and advance the iterator.
         * If the iterator is already at the end, nullopt is returned.
         * @return The next header.
         */
        virtual std::optional<std::pair<std::string_view, std::string_view>>
        next() noexcept = 0;
    };

    /*!
     * @brief The `headers_iterator` implementation for `headers_base`.
     * @tparam StrType The headers string type.
     */
    template <class StrType>
    class headers_view_iterator : public headers_iterator {
    public:
        using headers_type = headers_base<StrType>;

        /*!
         * @brief Construct the iterator with headers.
         * @param headers The headers to iterate.
         */
        headers_view_iterator(const headers_type& headers) noexcept
            : headers_{headers} {
        }

        /*!
         * @brief Seek the headers iterator to the begin.
         */
        void return_to_begin() noexcept override {
            current_it_ = headers_.begin();
        }

        /*!
         * @brief Get the next header and advance the iterator.
         * If the iterator is already at the end, nullopt is returned.
         * @return The next header.
         */
        std::optional<std::pair<std::string_view, std::string_view>>
        next() noexcept override {
            if (current_it_ == headers_.end()) {
                return std::nullopt;
            }
            auto next_it = std::next(current_it_);
            auto ret = std::pair<std::string_view, std::string_view>{
                current_it_->first, current_it_->second};
            current_it_ = next_it;
            return ret;
        }

    private:
        const headers_type& headers_;
        typename headers_type::const_iterator current_it_ = headers_.begin();
    };

    /*!
     * @brief Serialize HTTP 1/1.1 response to string.
     * @param status The response status code.
     * @param ver The HTTP version (1.0 or 1.1).
     * @param reason_phrase The optional reason phrase.
     * @param headers The message headers.
     * @param headers_serialized_size The size in bytes of serialized headers.
     * @param body The optional message body.
     * @param out_buff The output dynamic buffer where serialized message will
     * be appended.
     */
    RAD_EXPORT_DECL void serialize_response(uint32_t status, version ver,
                                            std::string_view reason_phrase,
                                            headers_iterator& headers,
                                            std::size_t headers_serialized_size,
                                            std::string_view body,
                                            dynamic_buffer out_buff);

    /*!
     * @brief An HTTP response container that contains various parts of the
     * http response like: status, version, reason, headers and body. The
     * string type used may be owning `std::string` or non owning
     * `std::string_view`. Great care must be taken when using non owning
     * strings.
     * @tparam StrType The type of string.
     */
    template <class StrType>
    struct response_base {
        /// The response status code.
        uint32_t status = 0;
        /// The optional reason phrase.
        StrType reason;
        /// The message headers.
        headers_base<StrType> headers;
        /// The optional message body.
        StrType body;
        /// The HTTP version.
        http::version version = http::version::invalid;

        /*!
         * @brief Clear status, reason phrase, headers and optionally the body.
         * @param clear_body If true the body will be cleared.
         */
        void clear(bool clear_body = true) noexcept {
            status = 0;
            version = http::version::invalid;
            reason = {};
            headers.clear();
            if (clear_body) {
                body = {};
            }
        }

        /*!
         * @brief Get the status code as a `response_status` enum.
         * @return The status code as a `response_status` enum.
         */
        constexpr response_status status_code() const noexcept {
            return static_cast<response_status>(status);
        }

        /*!
         * @brief Set the status code.
         * @param code The status code to set.
         */
        constexpr void status_code(response_status code) noexcept {
            status = static_cast<uint32_t>(code);
        }

        /*!
         * @brief Check if the response is informational.
         * @return True if the response is informational, otherwise false.
         */
        constexpr bool is_informational() const noexcept {
            return status >= 100 && status < 200;
        }

        /*!
         * @brief Check if the response is success.
         * @return True if the response is success, otherwise false.
         */
        constexpr bool is_success() const noexcept {
            return status >= 200 && status < 300;
        }

        /*!
         * @brief Check if the response is redirect.
         * @return True if the response is redirect, otherwise false.
         */
        constexpr bool is_redirect() const noexcept {
            return status >= 300 && status < 400;
        }

        /*!
         * @brief Check if the response is client error.
         * @return True if the response is client error, otherwise false.
         */
        constexpr bool is_client_error() const noexcept {
            return status >= 400 && status < 500;
        }

        /*!
         * @brief Check if the response is server error.
         * @return True if the response is server error, otherwise false.
         */
        constexpr bool is_server_error() const noexcept {
            return status >= 500 && status < 600;
        }

        /*!
         * @brief Check if the response is error.
         * @return True if the response is error, otherwise false.
         */
        constexpr bool is_error() const noexcept {
            return status >= 400 && status < 600;
        }

        /*!
         * @brief Check if the response is OK.
         * @return True if the response is OK, otherwise false.
         */
        constexpr bool is_ok() const noexcept {
            return status == 200;
        }

        /*!
         * @brief Check if the response is allowed to have a
         * body. 204 No Content and 304 Not Modified responses
         * have no body
         * @return True if the response can have a body,
         * otherwise false.
         */
        constexpr bool may_have_body() const noexcept {
            return status != 204 && status != 304;
        }

        /*!
         * @brief Check if the response can't have a body.
         * 204 No Content and 304 Not Modified responses have no
         * body
         * @return True if the response can't have a body,
         * otherwise false.
         */
        constexpr bool cannot_have_body() const noexcept {
            return status == 204 || status == 304;
        }

        /*!
         * @brief If a header with the given name exists, then return the
         * first found header value. Otherwise, return an empty string view.
         * @param f The header name.
         * @return The header value or empty string.
         */
        std::string_view try_get_header(field f) const noexcept {
            auto it = headers.find(f);
            return it == headers.end() ? std::string_view{}
                                       : std::string_view{it->second};
        }

        /*!
         * @brief If a header with the given name exists, then return the
         * first found header value. Otherwise, return an empty string view.
         * @param f The header name.
         * @return The header value or empty string.
         */
        std::string_view try_get_header(std::string_view f) const noexcept {
            auto it = headers.find(f);
            return it == headers.end() ? std::string_view{}
                                       : std::string_view{it->second};
        }

        /*!
         * @brief If a Content-Length header exists, then return its value.
         * Otherwise, return an empty string view.
         * @return The Content-Length header value or empty string.
         */
        std::string_view get_content_length() const noexcept {
            return try_get_header(field::content_length);
        }

        /*!
         * @brief If a Transfer-Encoding header exists, then return its value.
         * Otherwise, return an empty string view.
         * @return The Transfer-Encoding header value or empty string.
         */
        std::string_view get_transfer_encoding() const noexcept {
            return try_get_header(field::transfer_encoding);
        }

        /*!
         * @brief Serialize this HTTP response to string.
         * @param out_buff The output dynamic buffer where serialized message
         * will be appended.
         * @param include_body Whether to include the body in serialization or
         * not.
         */
        void serialize(dynamic_buffer out_buff,
                       bool include_body = true) const {
            const std::size_t headers_serialized_size =
                headers.serialized_size();
            headers_view_iterator<StrType> headers_iter{headers};
            serialize_response(
                status, version, reason, headers_iter, headers_serialized_size,
                include_body ? std::string_view{body} : std::string_view{},
                out_buff);
        }

        /*!
         * @brief Serialize this HTTP response to string.
         * @param include_body Whether to include the body in serialization or
         * not.
         * @return The output dynamic buffer where serialized message
         * will be appended.
         */
        std::string serialize(bool include_body = true) const {
            std::string buff;
            serialize(dynamic_buffer(buff), include_body);
            return buff;
        }

        friend bool operator==(const response_base& lhs,
                               const response_base& rhs) noexcept {
            return lhs.status == rhs.status && lhs.reason == rhs.reason &&
                   lhs.version == rhs.version && lhs.headers == rhs.headers &&
                   lhs.body == rhs.body;
        }
    };

    /*!
     * @brief HTTP response that stores strings using owning `std::string`
     */
    using response = response_base<std::string>;

    /*!
     * @brief HTTP response that stores strings using non owning
     * `std::string_view`
     */
    using response_view = response_base<std::string_view>;

    /*!
     * @brief Get the new method for redirect request.
     * @param method The method of the first request.
     * @param status The status of the redirect response.
     * @return The new request method.
     */
    inline constexpr verb
    change_method_on_redirect(verb method, response_status status) noexcept {
        if (status == response_status::moved_permanently) {
            // GET methods unchanged. Others may or may not be
            // changed to GET
            return verb::get;
        }
        if (status == response_status::permanent_redirect) {
            // Method and body not changed.
            return method;
        }
        if (status == response_status::found) {
            // GET methods unchanged. Others may or may not be
            // changed to GET
            return verb::get;
        }
        if (status == response_status::see_other) {
            // GET methods unchanged. Others changed to GET (body
            // lost)
            return verb::get;
        }
        if (status == response_status::temporary_redirect) {
            // Method and body not changed
            return method;
        }

        return method;
    }

    /*!
     * @brief Parse the value of Transfer-Encoding header.
     * @param transfer_encoding The value of Transfer-Encoding header.
     * @param encodings The result encodings will be appended to this list.
     */
    RAD_EXPORT_DECL void
    parse_transfer_encoding(std::string_view transfer_encoding,
                            std::vector<std::string_view>& encodings);

    /*!
     * @brief Make the request target based on the request method, request URL
     * and request receiver.
     * @param req_url The request URL.
     * @param method The request method.
     * @param to_proxy Whether this request is sent to a proxy or not.
     * @param target The output target will be stored here.
     * Initial content is cleared.
     * @param ec Set to indicate error occured, if any.
     */
    RAD_EXPORT_DECL void make_request_target(const url& req_url, verb method,
                                             bool to_proxy, std::string& target,
                                             std::error_code& ec);

    /*!
     * @brief Unknown length body.
     * Read until EOF.
     */
    struct body_until_eof {};

    /*!
     * @brief Unknown length body.
     * The body is divided into chunks with the last
     * empty chunk indicating the end of the body.
     */
    struct chunked_body {};

    /*!
     * @brief The body size and read method couldn't be
     * determined.
     */
    struct bad_message_body {};

    using message_body_size = std::variant<std::uint64_t, body_until_eof,
                                           chunked_body, bad_message_body>;

    /*!
     * @brief Determine the HTTP 1/1.1 message body size and how to read it.
     * @param req_method The request method.
     *
     * Client should pass the request method it used to make the request
     * it wants its response size.
     *
     * Server should pass the received client request method.
     * @param status The response status code.
     *
     * Client should pass the received response status code.
     *
     * Server should pass nullopt since it sends not receives responses.
     * @param ver The HTTP message version.
     *
     * Client should pass the received response version.
     *
     * Server should pass the received request version.
     * @param content_length The value of Content-Length header which may be
     * empty if no Content-Length header was received.
     * @param transfer_encoding The value of Transfer-Encoding header which may
     * be empty if no Transfer-Encoding header was received.
     */
    RAD_EXPORT_DECL message_body_size determine_message_body_length(
        verb req_method, std::optional<response_status> status, version ver,
        std::string_view content_length, std::string_view transfer_encoding);

    /*!
     * @brief Serialize HTTP 1/1.1 request to string.
     * @param method The HTTP request method.
     * @param target The HTTP request target.
     * @param ver The HTTP version (1.0 or 1.1).
     * @param headers The message headers.
     * @param headers_serialized_size The size in bytes of serialized headers.
     * @param body The optional message body.
     * @param out_buff The output dynamic buffer where serialized message will
     * be appended.
     */
    RAD_EXPORT_DECL void serialize_request(verb method, std::string_view target,
                                           version ver,
                                           headers_iterator& headers,
                                           std::size_t headers_serialized_size,
                                           std::string_view body,
                                           dynamic_buffer out_buff);

    /*!
     * @brief An HTTP request container that contains various parts of the
     * http request like: method, path, version, headers and body. The
     * string type used may be owning `std::string` or non owning
     * `std::string_view`. Great care must be taken when using non owning
     * strings.
     * @tparam StrType The type of string.
     */
    template <class StrType>
    struct request_base {
        verb method = {};
        StrType target;
        http::version version = http::version::invalid;
        headers_base<StrType> headers;
        StrType body;

        /*!
         * @brief Serialize this HTTP request to string.
         * @param out_buff The output dynamic buffer to append
         * the serialized request to.
         * @param include_body If true and there is a body, the
         * body will be appended to the serialized message,
         * otherwise the body will not be appended.
         */
        void serialize(dynamic_buffer out_buff,
                       bool include_body = true) const {
            const std::size_t headers_serialized_size =
                headers.serialized_size();
            headers_view_iterator<StrType> headers_iter{headers};
            serialize_request(
                method, target, version, headers_iter, headers_serialized_size,
                include_body ? body : std::string_view{}, out_buff);
        }

        /*!
         * @brief Serialize this HTTP request to string.
         * @param include_body If true and there is a body, the
         * body will be appended to the serialized message,
         * otherwise the body will not be appended.
         * @return The string containing the serialized request.
         */
        std::string serialize(bool include_body = true) const {
            std::string str;
            serialize(dynamic_buffer(str), include_body);
            return str;
        }

        friend bool operator==(const request_base& lhs,
                               const request_base& rhs) noexcept {
            return lhs.method == rhs.method && lhs.target == rhs.target &&
                   lhs.version == rhs.version && lhs.headers == rhs.headers &&
                   lhs.body == rhs.body;
        }
    };

    /*!
     * @brief HTTP request that stores strings using owning `std::string`
     */
    using request = request_base<std::string>;

    /*!
     * @brief HTTP request that stores strings using non owning
     * `std::string_view`
     */
    using request_view = request_base<std::string_view>;

    /*!
     * @brief Context used when parsing request line.
     */
    struct parse_request_line_context {
        enum class parse_stage {
            method,
            sp,
            target_start,
            target,
            http_slash,
            version,
            cr,
            lf,
            done,
            error,
        };

        verb method = verb::invalid;
        http::version version = http::version::v1_0;
        std::string target;
        parse_stage stage = parse_stage::method;
        parse_stage next_stage = parse_stage::method;
        http::error last_ec = http::error::no_error;

        bool done() const noexcept {
            return stage == parse_stage::done;
        }

        bool error() const noexcept {
            return stage == parse_stage::error;
        }

        bool need_more() const noexcept {
            return !done() && !error();
        }
    };

    /*!
     * @brief Context used when parsing status line.
     */
    struct parse_status_line_context {
        enum class parse_stage {
            http_slash,
            version,
            sp,
            status,
            reason_start_or_cr,
            reason,
            lf,
            done,
            error,
        };

        enum version version = version::v1_0;
        uint32_t status = 0;
        std::string reason;
        parse_stage stage = parse_stage::http_slash;
        parse_stage next_stage = parse_stage::http_slash;
        http::error last_ec = http::error::no_error;

        bool done() const noexcept {
            return stage == parse_stage::done;
        }

        bool error() const noexcept {
            return stage == parse_stage::error;
        }

        bool need_more() const noexcept {
            return !done() && !error();
        }
    };

    /*!
     * @brief Context used when parsing headers.
     */
    struct parse_headers_context {
        enum class parse_stage {
            name_and_colon,
            ows,
            value,
            cr,
            lf,
            terminating_lf,
            done,
            error,
        };

        std::string name;
        std::string value;
        std::optional<std::uint64_t> first_content_length;
        bool got_host = false;
        bool got_transfer_encoding = false;
        bool got_connection = false;
        bool got_upgrade = false;
        parse_stage stage = parse_stage::name_and_colon;
        parse_stage next_stage = parse_stage::name_and_colon;
        http::error last_ec = http::error::no_error;

        bool done() const noexcept {
            return stage == parse_stage::done;
        }

        bool error() const noexcept {
            return stage == parse_stage::error;
        }

        bool need_more() const noexcept {
            return !done() && !error();
        }
    };

    /*!
     * @brief Parse HTTP request line incrementally.
     * @param input The http request buffer.
     * @param ctx The context used during parsing. If multiple calls to
     * this function is needed, the passed context must be the same each
     * time.
     *
     * After successful parsing, the caller must check if ctx is done
     * and if it is not, the parsing is not complete and additional buffers
     * are needed.
     * @param ec Set to indicate error occured, if any.
     * @return Count of consumed bytes from @p input.
     */
    RAD_EXPORT_DECL std::size_t
    parse_request_line(ring_consumer& input, parse_request_line_context& ctx,
                       std::error_code& ec) noexcept;

    /*!
     * @brief Parse HTTP status line incrementally.
     * @param input The http response buffer.
     * @param ctx The context used during parsing. If multiple calls to
     * this function is needed, the passed context must be the same each
     * time.
     *
     * After successful parsing, the caller must check if ctx is done
     * and if it is not, the parsing is not complete and additional buffers
     * are needed.
     * @param ec Set to indicate error occured, if any.
     * @return Count of consumed bytes from @p input.
     */
    RAD_EXPORT_DECL std::size_t
    parse_status_line(ring_consumer& input, parse_status_line_context& ctx,
                      std::error_code& ec) noexcept;

    /*!
     * @brief Callback called with each parsed field and passed
     * the name and value, which may be empty, of this field.
     * The name can't be empty.
     */
    using header_callback =
        function_view<void(std::string_view, std::string_view)>;

    /*!
     * @brief Parse HTTP headers incrementally.
     * @param input The HTTP headers buffer.
     * @param ctx The context used during parsing. If multiple calls to
     * this function is needed, the passed context must be the same each
     * time.
     *
     * After successful parsing, the caller must check if ctx is done
     * and if it is not, the parsing is not complete and additional buffers
     * are needed.
     * @param on_header Handler function called on each parsed field name and
     * value. The views passed to the handler are not guaranteed to be valid
     * after the handler returns. So, a copy must be made if they are
     * required after handler return.
     * @param ec Set to indicate error occured, if any.
     * @return Count of consumed bytes from @p input.
     */
    RAD_EXPORT_DECL std::size_t parse_headers(ring_consumer& input,
                                              parse_headers_context& ctx,
                                              header_callback on_header,
                                              std::error_code& ec) noexcept;

    /*!
     * @brief HTTP 1/1.1 request incremental parser.
     *
     * To parse an HTTP request construct the parser with a reference
     * to the output request then call parse() multiple times until done()
     * returns true.
     *
     * If a call to parse() results in error, then the parser has failed to
     * parse the HTTP message and no more parse attempts may be done.
     */
    class request_incremental_parser {
        struct done_stage {};
        struct error_stage {
            std::error_code ec;
        };

    public:
        /*!
         * @brief Construct the parser with the output request.
         * @param out The output request.
         */
        request_incremental_parser(request& out) noexcept : out_{out} {
        }

        /*!
         * @brief Check if the parser hasn't completed parsing
         * and hasn't failed.
         * @return True if more data is needed, and false
         * otherwise.
         */
        bool need_more() const noexcept {
            return !done() && !has_error();
        }

        /*!
         * @brief Check if parsing has failed.
         * @return True if parsing has failed, and false
         * otherwise.
         */
        bool has_error() const noexcept {
            return std::holds_alternative<error_stage>(stage_);
        }

        /*!
         * @brief Check if parsing is done and the parser
         * consumed the last http header and the trailing CRLF
         * terminating the message headers.
         * @return True if parsing is done and all the headers
         * were parsed.
         */
        bool done() const noexcept {
            return std::holds_alternative<done_stage>(stage_);
        }

        /*!
         * @brief Get the last error occured while parsing, or
         * empty error if there is no error.
         * @return The last error occured while parsing, or
         * empty error if there is no error.
         */
        std::error_code last_error() const noexcept {
            if (auto error = std::get_if<error_stage>(&stage_)) {
                return error->ec;
            }
            return {};
        }

        /*!
         * @brief Parse HTTP request incrementally.
         *
         * If the call to parse() results in error, then the parser has failed
         * to parse the HTTP message and no more parse attempts may be done.
         *
         * If the parser done() returns false after successful parsing, then
         * the parsing is not complete and additional buffers are required.
         * @param input The HTTP message buffer.
         * @param ec Set to indicate error occured, if any.
         * @return Count of consumed bytes from @p input.
         */
        RAD_EXPORT_DECL std::size_t parse(ring_consumer& input,
                                          std::error_code& ec) noexcept;

        /*!
         * @brief Parse HTTP request incrementally.
         *
         * If the call to parse() results in error, then the parser has failed
         * to parse the HTTP message and no more parse attempts may be done.
         *
         * If the parser done() returns false after successful parsing, then
         * the parsing is not complete and additional buffers are required.
         * @param input The HTTP message buffer.
         * @return Count of consumed bytes from @p input.
         */
        std::size_t parse(ring_consumer& input) {
            std::error_code ec;
            std::size_t n = parse(input, ec);
            if (ec) {
                throw std::system_error{ec};
            }
            return n;
        }

    private:
        request& out_;
        std::variant<parse_request_line_context, parse_headers_context,
                     done_stage, error_stage>
            stage_;
    };

    /*!
     * @brief HTTP 1/1.1 response incremental parser.
     *
     * To parse an HTTP response construct the parser with a reference
     * to the output response then call parse() multiple times until done()
     * returns true.
     *
     * If a call to parse() results in error, then the parser has failed to
     * parse the HTTP message and no more parse attempts may be done.
     */
    class response_incremental_parser {
        struct done_stage {};
        struct error_stage {
            std::error_code ec;
        };

    public:
        /*!
         * @brief Construct the parser with the output response.
         * @param out The output response.
         */
        response_incremental_parser(response& out) noexcept : out_{out} {
        }

        /*!
         * @brief Check if the parser hasn't completed parsing
         * and hasn't failed.
         * @return True if more data is needed, and false
         * otherwise.
         */
        bool need_more() const noexcept {
            return !done() && !has_error();
        }

        /*!
         * @brief Check if parsing has failed.
         * @return True if parsing has failed, and false
         * otherwise.
         */
        bool has_error() const noexcept {
            return std::holds_alternative<error_stage>(stage_);
        }

        /*!
         * @brief Check if parsing is done and the parser
         * consumed the last http header and the trailing CRLF
         * terminating the message headers.
         * @return True if parsing is done and all the headers
         * were parsed.
         */
        bool done() const noexcept {
            return std::holds_alternative<done_stage>(stage_);
        }

        /*!
         * @brief Get the last error occured while parsing, or
         * empty error if there is no error.
         * @return The last error occured while parsing, or
         * empty error if there is no error.
         */
        std::error_code last_error() const noexcept {
            if (auto error = std::get_if<error_stage>(&stage_)) {
                return error->ec;
            }
            return {};
        }

        /*!
         * @brief Parse HTTP response incrementally.
         *
         * If the call to parse() results in error, then the parser has failed
         * to parse the HTTP message and no more parse attempts may be done.
         *
         * If the parser done() returns false after successful parsing, then
         * the parsing is not complete and additional buffers are required.
         * @param input The HTTP message buffer.
         * @param ec Set to indicate error occured, if any.
         * @return Count of consumed bytes from @p input.
         */
        RAD_EXPORT_DECL std::size_t parse(ring_consumer& input,
                                          std::error_code& ec) noexcept;

        /*!
         * @brief Parse HTTP response incrementally.
         *
         * If the call to parse() results in error, then the parser has failed
         * to parse the HTTP message and no more parse attempts may be done.
         *
         * If the parser done() returns false after successful parsing, then
         * the parsing is not complete and additional buffers are required.
         * @param input The HTTP message buffer.
         * @return Count of consumed bytes from @p input.
         */
        std::size_t parse(ring_consumer& input) {
            std::error_code ec;
            std::size_t n = parse(input, ec);
            if (ec) {
                throw std::system_error{ec};
            }
            return n;
        }

    private:
        response& out_;
        std::variant<parse_status_line_context, parse_headers_context,
                     done_stage, error_stage>
            stage_;
    };

    /*!
     * @brief Context used when parsing chunk size.
     */
    struct parse_chunk_size_context {
        enum class parse_stage {
            size,
            done,
            error,
        };

        // 8 bytes = 16 hex char since each byte is 2 hex chars
        std::array<char, 16> hex_buff;
        std::size_t chunk_size = 0;
        std::uint32_t hex_len = 0;
        parse_stage stage = parse_stage::size;

        bool done() const noexcept {
            return stage == parse_stage::done;
        }

        bool error() const noexcept {
            return stage == parse_stage::error;
        }

        bool need_more() const noexcept {
            return !done() && !error();
        }
    };

    /*!
     * @brief Parse HTTP chunk size incrementally.
     * @param input The HTTP chunk buffer.
     * @param ctx The context used during parsing. If multiple calls to
     * this function is needed, the passed context must be the same each
     * time.
     *
     * After successful parsing, the caller must check if ctx is done
     * and if it is not, the parsing is not complete and additional buffers
     * are needed.
     * @param ec Set to indicate error occured, if any.
     * @return Count of consumed bytes from @p input.
     */
    RAD_EXPORT_DECL std::size_t parse_chunk_size(ring_consumer& input,
                                                 parse_chunk_size_context& ctx,
                                                 std::error_code& ec) noexcept;

    /*!
     * @brief Context used when parsing chunk extensions.
     */
    struct parse_chunk_extensions_context {
        enum class parse_stage {
            check,
            bws,
            colon,
            name,
            eq_or_colon,
            value_start,
            token_value,
            quoted_value,
            report_name_value,
            done,
            error,
        };

        std::string name;
        std::string value;
        parse_stage stage = parse_stage::check;
        parse_stage next_stage = parse_stage::check;

        bool done() const noexcept {
            return stage == parse_stage::done;
        }

        bool error() const noexcept {
            return stage == parse_stage::error;
        }

        bool need_more() const noexcept {
            return !done() && !error();
        }
    };

    /*!
     * @brief Callback called with each parsed chunk extension and passed
     * the name and value, which may be empty, of this extention.
     * If the callback return value is false the parsing is stopped with
     * error.
     */
    using chunk_extension_callback =
        function_view<bool(std::string_view, std::string_view)>;

    /*!
     * @brief Parse HTTP chunk extensions incrementally.
     * @param input The HTTP chunk buffer.
     * @param ctx The context used during parsing. If multiple calls to
     * this function is needed, the passed context must be the same each
     * time.
     *
     * After successful parsing, the caller must check if ctx is done
     * and if it is not, the parsing is not complete and additional buffers
     * are needed.
     * @param callback Handler function called on each parsed extension with
     * name and value. The views passed to the handler are not guaranteed to be
     * valid after the handler returns. So, a copy must be made if they are
     * required after handler return.
     * @param ec Set to indicate error occured, if any.
     * @return Count of consumed bytes from @p input.
     */
    RAD_EXPORT_DECL std::size_t parse_chunk_extensions(
        ring_consumer& input, parse_chunk_extensions_context& ctx,
        chunk_extension_callback callback, std::error_code& ec);

    /*!
     * @brief Context used when parsing chunk data.
     */
    struct parse_chunk_data_context {
        std::size_t size = 0;

        bool done() const noexcept {
            return size == 0;
        }
    };

    /*!
     * @brief Parse HTTP chunk data incrementally.
     * @param input The HTTP chunk buffer.
     * @param ctx The context used during parsing. If multiple calls to
     * this function is needed, the passed context must be the same each
     * time.
     *
     * After successful parsing, the caller must check if ctx is done
     * and if it is not, the parsing is not complete and additional buffers
     * are needed.
     * @param out The output dynamic buffer where chunk data will be appended.
     * @return Count of consumed bytes from @p input.
     */
    RAD_EXPORT_DECL std::size_t parse_chunk_data(ring_consumer& input,
                                                 parse_chunk_data_context& ctx,
                                                 dynamic_buffer out);

    /*!
     * @brief Context used when parsing chunk trailing CRLF.
     */
    struct parse_chunk_trailing_crlf_ctx {
        char expected_char = '\r';
        bool is_done = false;
        bool is_error = false;

        bool done() const noexcept {
            return is_done;
        }

        bool error() const noexcept {
            return is_error;
        }

        bool need_more() const noexcept {
            return !done() && !error();
        }
    };

    /*!
     * @brief Parse HTTP chunk trailing CRLF incrementally.
     * @param input The HTTP chunk buffer.
     * @param ctx The context used during parsing. If multiple calls to
     * this function is needed, the passed context must be the same each
     * time.
     *
     * After successful parsing, the caller must check if ctx is done
     * and if it is not, the parsing is not complete and additional buffers
     * are needed.
     * @param ec Set to indicate error occured, if any.
     * @return Count of consumed bytes from @p input.
     */
    RAD_EXPORT_DECL std::size_t
    parse_chunk_trailing_crlf(ring_consumer& input,
                              parse_chunk_trailing_crlf_ctx& ctx,
                              std::error_code& ec) noexcept;

    /*!
     * @brief HTTP 1/1.1 chunks incremental parser.
     *
     * To parse an HTTP body chunks construct the parser
     * then call parse() multiple times until done()
     * returns true.
     *
     * If a call to parse() results in error, then the parser has failed to
     * parse the HTTP message and no more parse attempts may be done.
     */
    class chunks_incremental_parser {
        struct done_stage {};

        struct error_stage {
            std::error_code ec;
        };

        struct parse_crlf_after_size_ctx : parse_chunk_trailing_crlf_ctx {};

    public:
        /*!
         * @brief Check if the parser hasn't completed parsing
         * and hasn't failed.
         * @return True if more data is needed, and false
         * otherwise.
         */
        bool need_more() const noexcept {
            return !done() && !has_error();
        }

        /*!
         * @brief Check if parsing has failed.
         * @return True if parsing has failed, and false
         * otherwise.
         */
        bool has_error() const noexcept {
            return std::holds_alternative<error_stage>(stage_);
        }

        /*!
         * @brief Check if parsing is done and the parser
         * consumed the last http header and the trailing CRLF
         * terminating the message headers.
         * @return True if parsing is done and all the headers
         * were parsed.
         */
        bool done() const noexcept {
            return std::holds_alternative<done_stage>(stage_);
        }

        /*!
         * @brief Get the last error occured while parsing, or
         * empty error if there is no error.
         * @return The last error occured while parsing, or
         * empty error if there is no error.
         */
        std::error_code last_error() const noexcept {
            if (auto error = std::get_if<error_stage>(&stage_)) {
                return error->ec;
            }
            return {};
        }

        /*!
         * @brief Parse HTTP body chunks.
         * @param input The HTTP chunks buffer.
         * @param output The output dynamic buffer where chunk data will be
         * appended.
         * @param extensions_cb Handler function called on each parsed extension
         * with name and value. The views passed to the handler are not
         * guaranteed to be valid after the handler returns. So, a copy must be
         * made if they are required after handler return.
         * @param trailers_callback Handler function called on each parsed
         * trailers field name and value. The views passed to the handler are
         * not guaranteed to be valid after the handler returns. So, a copy must
         * be made if they are required after handler return.
         * @param ec Set to indicate error occured, if any.
         * @return Count of consumed bytes from @p input.
         */
        RAD_EXPORT_DECL std::size_t
        parse(ring_consumer& input, dynamic_buffer output,
              chunk_extension_callback extensions_cb,
              header_callback trailers_callback, std::error_code& ec);

        /*!
         * @brief Parse HTTP body chunks.
         * @param input The HTTP chunks buffer.
         * @param output The output dynamic buffer where chunk data will be
         * appended.
         * @param extensions_cb Handler function called on each parsed extension
         * with name and value. The views passed to the handler are not
         * guaranteed to be valid after the handler returns. So, a copy must be
         * made if they are required after handler return.
         * @param trailers_callback Handler function called on each parsed
         * trailers field name and value. The views passed to the handler are
         * not guaranteed to be valid after the handler returns. So, a copy must
         * be made if they are required after handler return.
         * @return Count of consumed bytes from @p input.
         */
        std::size_t parse(ring_consumer& input, dynamic_buffer output,
                          chunk_extension_callback extensions_cb,
                          header_callback trailers_callback) {
            std::error_code ec;
            const std::size_t n =
                parse(input, output, extensions_cb, trailers_callback, ec);
            if (ec) {
                throw std::system_error{ec};
            }
            return n;
        }

    private:
        std::size_t chunk_size_ = 0;
        std::variant<parse_chunk_size_context, parse_chunk_extensions_context,
                     parse_crlf_after_size_ctx, parse_chunk_data_context,
                     parse_chunk_trailing_crlf_ctx, parse_headers_context,
                     done_stage, error_stage>
            stage_;
    };
}; // namespace RAD_LIB_NAMESPACE::net::http