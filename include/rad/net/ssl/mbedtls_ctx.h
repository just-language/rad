#pragma once
#include <rad/buffer.h>
#include <rad/net/ssl/sslctx.h>
#include <rad/string.h>
#include <rad/trackable.h>

#include <memory>

namespace RAD_LIB_NAMESPACE::net::ssl::mbedtls {

    /*!
     * @brief Get a const reference to the error category used by MbedTLS
     * ssl implementation.
     * @return A const reference to the error category used by MbedTLS ssl
     * implementation.
     */
    RAD_EXPORT_DECL const std::error_category& ssl_category() noexcept;

    class mbedtls_impl;

    /*!
     * @brief MbedTLS context implementation.
     *
     * The implementation wraps wraps `mbedtls_ssl_config` mbedtls context
     * and allows access to it using `native_handle` method.
     *
     * To load trused certificates use `add_verify_certificate`,
     * `add_verify_file` and `add_verify_path` methods.
     *
     * To load the default system trusted certificates use
     * `set_default_verify_paths` method.
     *
     * To load identity certificate and private key use
     * `use_own_certificate_private_key` method.
     */
    class RAD_EXPORT_VTABLE context : public context_base {
        struct context_inner;
        friend class mbedtls_impl;

    public:
        using ssl_impl_type = mbedtls_impl;

        context() = default;

        context(const context&) = delete;

        context(context&&) noexcept = default;

        RAD_EXPORT_DECL context(version v);

        context& operator=(const context& other) = delete;

        context& operator=(context&&) noexcept = default;

        RAD_EXPORT_DECL ~context();

        /*!
         * @brief Create an mbedtls ssl implementation derived
         * from this context. The context must be open prior to
         * this call.
         * @param ec Set to indicate errors occured, if any.
         * @return An mbedtls ssl implementation derived from
         * this context, or null on failure.
         */
        RAD_EXPORT_DECL std::unique_ptr<ssl_impl_base>
        make_generic_impl(std::error_code& ec) noexcept override;

        /*!
         * @brief Make a client context that negotiates the
         * maximum available protocol version.
         * @return A client context that negotiates the maximum
         * available protocol version.
         */
        RAD_EXPORT_DECL static context generic_client();

        /*!
         * @brief Make a server context that negotiates the
         * maximum available protocol version.
         * @return A server context that negotiates the maximum
         * available protocol version.
         */
        RAD_EXPORT_DECL static context generic_server();

        /*!
         * @brief Create a new context with version and
         * direction (client or server)
         * @p v and replace the existing one, if any, by the new
         * created context. On failure, the current context is
         * not changed.
         * @param v The protocol version and transport direction
         * (client or server). To negotiate the maximum
         * available protocol version use `tls_client` or
         * `tls_server`.
         * @param ec Set to indicate errors occured, if any.
         */
        RAD_EXPORT_DECL void open(version v, std::error_code& ec) noexcept;

        /*!
         * @brief Create a new context with version and
         * direction (client or server)
         * @p v and replace the existing one, if any, by the new
         * created context. On failure, the current context is
         * not changed.
         * @param v The protocol version and transport direction
         * (client or server). To negotiate the maximum
         * available protocol version use `tls_client` or
         * `tls_server`.
         */
        void open(version v) {
            std::error_code ec;
            open(v, ec);
            check_and_throw(ec, __func__);
        }

        /*!
         * @brief Close the context.
         * Make sure not to close the context
         * until all derived ssl implementations are closed.
         * After the context is closed it can't be used
         * except to open it, or move assign it.
         */
        RAD_EXPORT_DECL void close() noexcept;

        /*!
         * @brief Check if there is an underlying context.
         * @return True if there is an underlying context,
         * otherwise false.
         */
        bool is_open() const noexcept {
            return ctx_ != nullptr;
        }

        /*!
         * @brief Check if there is an underlying context.
         * @return True if there is an underlying context,
         * otherwise false.
         */
        explicit operator bool() const noexcept {
            return is_open();
        }

        /*!
         * @brief Get the native context pointer.
         * The returned pointer can be casted to
         * `mbedtls_ssl_config*`. If there is no current
         * context, the returned pointer is null.
         * @return A pointer to the native context that can be
         * casted to `mbedtls_ssl_config*`, or null.
         */
        RAD_EXPORT_DECL void* native_handle() const noexcept;

        /*!
         * @brief Add certificates for performing verification.
         * This function is used to add one or more trusted
         * certificates from a memory buffer. Existing trusted
         * certificates are not removed.
         * @param cert_buffer The buffer containing the
         * certification authority certificate. The certificate
         * can use the PEM or ASN.1 format.
         * @param format The certificate format (ASN.1 or PEM).
         * @param ec Set to indicate what error occurred, if
         * any.
         */
        RAD_EXPORT_DECL void
        add_verify_certificate(const_buffer cert_buffer, file_format format,
                               std::error_code& ec) noexcept;

        /*!
         * @brief Add certificates for performing verification.
         * This function is used to add one or more trusted
         * certificates from a memory buffer. Existing trusted
         * certificates are not removed.
         * @param cert_buffer The buffer containing the
         * certification authority certificate. The certificate
         * can use the PEM or ASN.1 format.
         * @param format The certificate format (ASN.1 or PEM).
         */
        void add_verify_certificate(const_buffer cert_buffer,
                                    file_format format) {
            std::error_code ec;
            add_verify_certificate(cert_buffer, format, ec);
            check_and_throw(ec, __func__);
        }

        /*!
         * @brief Load a certification authority file for
         * performing verification. This function is used to
         * load one or more trusted certification authorities
         * from a file. Existing trusted certificates are not
         * removed.
         * @param file The name of a file containing
         * certification authority certificates in PEM format.
         * @param ec Set to indicate what error occurred, if
         * any.
         */
        RAD_EXPORT_DECL void add_verify_file(const std::string& file,
                                             std::error_code& ec) noexcept;

        /*!
         * @brief Load a certification authority file for
         * performing verification. This function is used to
         * load one or more trusted certification authorities
         * from a file. Existing trusted certificates are not
         * removed.
         * @param file The name of a file containing
         * certification authority certificates in PEM format.
         */
        void add_verify_file(const std::string& file) {
            std::error_code ec;
            add_verify_file(file, ec);
            check_and_throw(ec, __func__);
        }

        /*!
         * @brief Add a directory containing certificate
         * authority files to be used for performing
         * verification.
         *
         * This function is used to specify the name of a
         * directory containing certification authority
         * certificates. Each file in the directory will be
         * retried for parsing, and on success parsed
         * certificates will be added.
         *
         * Existing trusted certificates are not removed.
         * @param path The name of a directory containing the
         * certificates.
         * @param ec Set to indicate what error occurred, if
         * any.
         */
        RAD_EXPORT_DECL std::size_t add_verify_path(const std::string& path,
                                                    std::error_code& ec);

        /*!
         * @brief Add a directory containing certificate
         * authority files to be used for performing
         * verification.
         *
         * This function is used to specify the name of a
         * directory containing certification authority
         * certificates. Each file in the directory will be
         * retried for parsing, and on success parsed
         * certificates will be added.
         *
         * Existing trusted certificates are not removed.
         * @param path The name of a directory containing the
         * certificates.
         */
        void add_verify_path(const std::string& path) {
            std::error_code ec;
            add_verify_path(path, ec);
            check_and_throw(ec, __func__);
        }

        /*!
         * @brief Configures the context to use the default
         * directories for finding certification authority
         * certificates.
         *
         * This function specifies that the context should use
         * the default, system-dependent directories for
         * locating certification authority certificates.
         * @param ec Set to indicate what error occurred, if
         * any.
         */
        RAD_EXPORT_DECL void set_default_verify_paths(std::error_code& ec);

        /*!
         * @brief Configures the context to use the default
         * directories for finding certification authority
         * certificates.
         *
         * This function specifies that the context should use
         * the default, system-dependent directories for
         * locating certification authority certificates.
         */
        void set_default_verify_paths() {
            std::error_code ec;
            set_default_verify_paths(ec);
            check_and_throw(ec, __func__);
        }

        /*!
         * @brief Use an identity certificate from a memory
         * buffer, and a private key from a memory buffer.
         *
         * If there is an existing identity certificate,
         * it will be removed on success.
         *
         * The identity certificate and private key must match.
         *
         * If the private key is encrypted and there is a
         * registered password callback, it will be called to
         * provide decryption password. If there is no
         * registered callback, the function fails on encrypted
         * keys.
         * @param cert_buffer The buffer containing the
         * certificate.
         * @param key_buffer The buffer containing the private
         * key.
         * @param ec Set to indicate what error occurred, if
         * any.
         */
        RAD_EXPORT_DECL void
        use_own_certificate_private_key(const_buffer cert_buffer,
                                        const_buffer key_buffer,
                                        std::error_code& ec);

        /*!
         * @brief Use an identity certificate from a memory
         * buffer, and a private key from a memory buffer.
         *
         * If there is an existing identity certificate,
         * it will be removed on success.
         *
         * The identity certificate and private key must match.
         *
         * If the private key is encrypted and there is a
         * registered password callback, it will be called to
         * provide decryption password. If there is no
         * registered callback, the function fails on encrypted
         * keys.
         * @param cert_buffer The buffer containing the
         * certificate.
         * @param key_buffer The buffer containing the private
         * key.
         */
        void use_own_certificate_private_key(const_buffer cert_buffer,
                                             const_buffer key_buffer) {
            std::error_code ec;
            use_own_certificate_private_key(cert_buffer, key_buffer, ec);
            check_and_throw(ec, __func__);
        }

        /*!
         * @brief Use an identity certificate from a file,
         * and a private key from a memory buffer.
         *
         * If there is an existing identity certificate,
         * it will be removed on success.
         *
         * The identity certificate and private key must match.
         *
         * If the private key is encrypted and there is a
         * registered password callback, it will be called to
         * provide decryption password. If there is no
         * registered callback, the function fails on encrypted
         * keys.
         * @param cert_file The name of the file containing the
         * certificate.
         * @param key_buffer The buffer containing the private
         * key.
         * @param ec Set to indicate what error occurred, if
         * any.
         */
        RAD_EXPORT_DECL void
        use_own_certificate_private_key(const std::string& cert_file,
                                        const_buffer key_buffer,
                                        std::error_code& ec);

        /*!
         * @brief Use an identity certificate from a file,
         * and a private key from a memory buffer.
         *
         * If there is an existing identity certificate,
         * it will be removed on success.
         *
         * The identity certificate and private key must match.
         *
         * If the private key is encrypted and there is a
         * registered password callback, it will be called to
         * provide decryption password. If there is no
         * registered callback, the function fails on encrypted
         * keys.
         * @param cert_file The name of the file containing the
         * certificate.
         * @param key_buffer The buffer containing the private
         * key.
         */
        void use_own_certificate_private_key(const std::string& cert_file,
                                             const_buffer key_buffer) {
            std::error_code ec;
            use_own_certificate_private_key(cert_file, key_buffer, ec);
            check_and_throw(ec, __func__);
        }

        /*!
         * @brief Use an identity certificate from a memory
         * buffer, and a private key from a file.
         *
         * If there is an existing identity certificate,
         * it will be removed on success.
         *
         * The identity certificate and private key must match.
         *
         * If the private key is encrypted and there is a
         * registered password callback, it will be called to
         * provide decryption password. If there is no
         * registered callback, the function fails on encrypted
         * keys.
         * @param cert_file The name of the file containing the
         * certificate.
         * @param cert_buffer The buffer containing the
         * certificate.
         * @param key_file The name of the file containing the
         * private key.
         * @param ec Set to indicate what error occurred, if
         * any.
         */
        RAD_EXPORT_DECL void
        use_own_certificate_private_key(const_buffer cert_buffer,
                                        const std::string& key_file,
                                        std::error_code& ec);

        /*!
         * @brief Use an identity certificate from a memory
         * buffer, and a private key from a file.
         *
         * If there is an existing identity certificate,
         * it will be removed on success.
         *
         * The identity certificate and private key must match.
         *
         * If the private key is encrypted and there is a
         * registered password callback, it will be called to
         * provide decryption password. If there is no
         * registered callback, the function fails on encrypted
         * keys.
         * @param cert_file The name of the file containing the
         * certificate.
         * @param cert_buffer The buffer containing the
         * certificate.
         * @param key_file The name of the file containing the
         * private key.
         */
        void use_own_certificate_private_key(const_buffer cert_buffer,
                                             const std::string& key_file) {
            std::error_code ec;
            use_own_certificate_private_key(cert_buffer, key_file, ec);
            check_and_throw(ec, __func__);
        }

        /*!
         * @brief Use an identity certificate from a file,
         * and a private key from a file.
         *
         * If there is an existing identity certificate,
         * it will be removed on success.
         *
         * The identity certificate and private key must match.
         *
         * If the private key is encrypted and there is a
         * registered password callback, it will be called to
         * provide decryption password. If there is no
         * registered callback, the function fails on encrypted
         * keys.
         * @param cert_file The name of the file containing the
         * certificate.
         * @param key_file The name of the file containing the
         * private key.
         * @param ec Set to indicate what error occurred, if
         * any.
         */
        RAD_EXPORT_DECL void
        use_own_certificate_private_key(const std::string& cert_file,
                                        const std::string& key_file,
                                        std::error_code& ec);

        /*!
         * @brief Use an identity certificate from a file,
         * and a private key from a file.
         *
         * If there is an existing identity certificate,
         * it will be removed on success.
         *
         * The identity certificate and private key must match.
         *
         * If the private key is encrypted and there is a
         * registered password callback, it will be called to
         * provide decryption password. If there is no
         * registered callback, the function fails on encrypted
         * keys.
         * @param cert_file The name of the file containing the
         * certificate.
         * @param key_file The name of the file containing the
         * private key.
         */
        void use_own_certificate_private_key(const std::string& cert_file,
                                             const std::string& key_file) {
            std::error_code ec;
            use_own_certificate_private_key(cert_file, key_file, ec);
            check_and_throw(ec, __func__);
        }

        /*!
         * @brief Use an identity certificate from a memory
         * buffer.
         *
         * If there is an existing identity certificate,
         * it will be removed on success.
         * @param cert_buffer The buffer containing the
         * certificate.
         * @param format The certificate format (ASN.1 or PEM).
         * @param ec Set to indicate what error occurred, if
         * any.
         */
        RAD_EXPORT_DECL void use_own_certificate(const_buffer cert_buffer,
                                                 file_format format,
                                                 std::error_code& ec) noexcept;

        /*!
         * @brief Use an identity certificate from a memory
         * buffer.
         *
         * If there is an existing identity certificate,
         * it will be removed on success.
         * @param cert_buffer The buffer containing the
         * certificate.
         * @param format The certificate format (ASN.1 or PEM).
         */
        void use_own_certificate(const_buffer cert_buffer, file_format format) {
            std::error_code ec;
            use_own_certificate(cert_buffer, format, ec);
            check_and_throw(ec, __func__);
        }

        /*!
         * @brief Use an identity certificate from a file.
         *
         * If there is an existing identity certificate,
         * it will be removed on success.
         * @param filename The name of the file containing the
         * certificate.
         * @param format The certificate format (ASN.1 or PEM).
         * @param ec Set to indicate what error occurred, if
         * any.
         */
        RAD_EXPORT_DECL void
        use_own_certificate_file(const std::string& filename,
                                 file_format format,
                                 std::error_code& ec) noexcept;

        /*!
         * @brief Use an identity certificate from a file.
         *
         * If there is an existing identity certificate,
         * it will be removed on success.
         * @param filename The name of the file containing the
         * certificate.
         * @param format The certificate format (ASN.1 or PEM).
         */
        void use_own_certificate_file(const std::string& filename,
                                      file_format format) {
            std::error_code ec;
            use_own_certificate_file(filename, format, ec);
            check_and_throw(ec, __func__);
        }

        /*!
         * @brief Use a private key from a memory buffer.
         *
         * The identity certificate and private key must match.
         *
         * If the private key is encrypted and there is a
         * registered password callback, it will be called to
         * provide decryption password. If there is no
         * registered callback, the function fails on encrypted
         * keys.
         * @param key_buffer The buffer containing the private
         * key.
         * @param format The private key format (ASN.1 or PEM).
         * @param ec Set to indicate what error occurred, if
         * any.
         */
        RAD_EXPORT_DECL void use_private_key(const_buffer key_buffer,
                                             file_format format,
                                             std::error_code& ec) noexcept;

        /*!
         * @brief Use a private key from a memory buffer.
         *
         * The identity certificate and private key must match.
         *
         * If the private key is encrypted and there is a
         * registered password callback, it will be called to
         * provide decryption password. If there is no
         * registered callback, the function fails on encrypted
         * keys.
         * @param key_buffer The buffer containing the private
         * key.
         * @param format The private key format (ASN.1 or PEM).
         */
        void use_private_key(const_buffer key_buffer, file_format format) {
            std::error_code ec;
            use_private_key(key_buffer, format, ec);
            check_and_throw(ec, __func__);
        }

        /*!
         * @brief Use a private key from a file.
         *
         * The identity certificate and private key must match.
         *
         * If the private key is encrypted and there is a
         * registered password callback, it will be called to
         * provide decryption password. If there is no
         * registered callback, the function fails on encrypted
         * keys.
         * @param filename The name of the file containing the
         * private key.
         * @param format The file format (ASN.1 or PEM).
         * @param ec Set to indicate what error occurred, if
         * any.
         */
        RAD_EXPORT_DECL void use_private_key_file(const std::string& filename,
                                                  file_format format,
                                                  std::error_code& ec) noexcept;

        /*!
         * @brief Use a private key from a file.
         *
         * The identity certificate and private key must match.
         *
         * If the private key is encrypted and there is a
         * registered password callback, it will be called to
         * provide decryption password. If there is no
         * registered callback, the function fails on encrypted
         * keys.
         * @param filename The name of the file containing the
         * private key.
         * @param format The file format (ASN.1 or PEM).
         */
        void use_private_key_file(const std::string& filename,
                                  file_format format) {
            std::error_code ec;
            use_private_key_file(filename, format, ec);
            check_and_throw(ec, __func__);
        }

        /*!
         * @brief Set the password callback.
         *
         * This function is used to specify a callback function
         * to obtain password information about an encrypted key
         * in PEM format.
         * @tparam Callback The type of the callback.
         * @param cb The function object to be used for
         * obtaining the password.
         *
         * It must have this signature: `std::optional<std::size_t>
         * callback(mutable_buffer b, password_purpose p)`.
         *
         * The callback write the password to buffer b and
         * returns the count of written bytes.
         * @param ec Set to indicate what error occurred, if
         * any.
         */
        template <PasswordCallback Callback>
        void set_password_callback(Callback cb, std::error_code& ec) noexcept {
            if (!is_open()) {
                ec = std::make_error_code(std::errc::invalid_argument);
                return;
            }
            auto callback = make_password_callback(std::move(cb), ec);
            if (callback == nullptr) {
                return;
            }
            set_passwd_cb(std::move(callback));
        }

        /*!
         * @brief Set the password callback.
         *
         * This function is used to specify a callback function
         * to obtain password information about an encrypted key
         * in PEM format.
         * @tparam Callback The type of the callback.
         * @param cb The function object to be used for
         * obtaining the password.
         *
         * It must have this signature: `std::optional<std::size_t>
         * callback(mutable_buffer b, password_purpose p)`.
         *
         * The callback write the password to buffer b and
         * returns the count of written bytes.
         */
        template <PasswordCallback Callback>
        void set_password_callback(Callback cb) {
            std::error_code ec;
            set_password_callback(std::move(cb), ec);
            check_and_throw(ec, __func__);
        }

        /*!
         * @brief Remove the set password callback, if any.
         */
        void remove_password_callback() noexcept {
            if (!is_open()) {
                return;
            }
            remove_passwd_cb();
        }

        /*!
         * @brief Set the verification mode and callback used to
         * verify peer certificates. Previously set verify
         * callback, if any, will be removed on success.
         *
         * This function is used to specify the verification
         * mode and a callback function that will be called by
         * the implementation when it needs to verify a peer
         * certificate.
         * @tparam Callback The type of the callback.
         * @param mode Peer verification mode.
         * @param cb The function object to be used for
         * verifying a certificate.
         *
         * It must have this signature: `bool callback(bool
         * preverified, void* cert)`.
         *
         * Parameter preverified is true if the certificate
         * passed pre-verification. Parameter cert points to a
         * `X509_STORE_CTX` certificate. The callback returns
         * true if it accepts the certificate, or false to
         * reject it.
         * @param ec Set to indicate what error occurred, if
         * any.
         */
        template <VerifyCallback Callback>
        void set_verify_mode_callback(verify_mode mode, Callback cb,
                                      std::error_code& ec) {
            if (!is_open()) {
                ec = std::make_error_code(std::errc::invalid_argument);
                return;
            }
            auto callback = make_verify_callback(std::move(cb), ec);
            if (callback == nullptr) {
                return;
            }
            set_verify_cb(&mode, std::move(callback));
        }

        /*!
         * @brief Set the verification mode and callback used to
         * verify peer certificates. Previously set verify
         * callback, if any, will be removed on success.
         *
         * This function is used to specify the verification
         * mode and a callback function that will be called by
         * the implementation when it needs to verify a peer
         * certificate.
         * @tparam Callback The type of the callback.
         * @param mode Peer verification mode.
         * @param cb The function object to be used for
         * verifying a certificate.
         *
         * It must have this signature: `bool callback(bool
         * preverified, void* cert)`.
         *
         * Parameter preverified is true if the certificate
         * passed pre-verification. Parameter cert points to a
         * `X509_STORE_CTX` certificate. The callback returns
         * true if it accepts the certificate, or false to
         * reject it.
         */
        template <VerifyCallback Callback>
        void set_verify_mode_callback(verify_mode mode, Callback cb) {
            std::error_code ec;
            set_verify_mode_callback(mode, std::move(cb), ec);
            check_and_throw(ec, __func__);
        }

        /*!
         * @brief Set the callback used to verify peer
         * certificates. Previously set verify callback, if any,
         * will be removed on success.
         *
         * This function is used to specify a callback function
         * that will be called by the implementation when it
         * needs to verify a peer certificate.
         * @tparam Callback The type of the callback.
         * @param cb The function object to be used for
         * verifying a certificate.
         *
         * It must have this signature: `bool callback(bool
         * preverified, void* cert)`.
         *
         * Parameter preverified is true if the certificate
         * passed pre-verification. Parameter cert points to a
         * `X509_STORE_CTX` certificate. The callback returns
         * true if it accepts the certificate, or false to
         * reject it.
         * @param ec Set to indicate what error occurred, if
         * any.
         */
        template <VerifyCallback Callback>
        void set_verify_callback(Callback cb, std::error_code& ec) {
            if (!is_open()) {
                ec = std::make_error_code(std::errc::invalid_argument);
                return;
            }
            auto callback = make_verify_callback(std::move(cb), ec);
            if (callback == nullptr) {
                return;
            }
            set_verify_cb(nullptr, std::move(callback));
        }

        /*!
         * @brief Set the callback used to verify peer
         * certificates. Previously set verify callback, if any,
         * will be removed on success.
         *
         * This function is used to specify a callback function
         * that will be called by the implementation when it
         * needs to verify a peer certificate.
         * @tparam Callback The type of the callback.
         * @param cb The function object to be used for
         * verifying a certificate.
         *
         * It must have this signature: `bool callback(bool
         * preverified, void* cert)`.
         *
         * Parameter preverified is true if the certificate
         * passed pre-verification. Parameter cert points to a
         * `X509_STORE_CTX` certificate. The callback returns
         * true if it accepts the certificate, or false to
         * reject it.
         */
        template <VerifyCallback Callback>
        void set_verify_callback(Callback cb) {
            std::error_code ec;
            set_verify_callback(std::move(cb), ec);
            check_and_throw(ec, __func__);
        }

        /*!
         * @brief Set the peer verification mode.
         * @param mode Peer verification mode.
         */
        void set_verify_mode(verify_mode mode) noexcept {
            set_verify_cb(&mode, nullptr);
        }

        /*!
         * @brief Remove the set verify callback, if any.
         */
        void remove_verify_callback() noexcept {
            remove_verify_cb();
        }

        /*!
         * @brief Set the peer verification depth.
         * @param depth Maximum depth for the certificate chain
         * verification that shall be allowed.
         */
        RAD_EXPORT_DECL void set_verify_depth(std::size_t depth) noexcept;

    private:
        RAD_EXPORT_DECL void
        set_verify_cb(const verify_mode* mode,
                      std::unique_ptr<verify_callback_base> cb) noexcept;

        RAD_EXPORT_DECL void
        set_passwd_cb(std::unique_ptr<password_callback_base> cb) noexcept;

        RAD_EXPORT_DECL void remove_verify_cb() noexcept;

        RAD_EXPORT_DECL void remove_passwd_cb() noexcept;

        std::unique_ptr<context_inner> ctx_;
    };

    /*!
     * @brief MbedTLS ssl implementation.
     *
     * The implementation wraps wraps `mbedtls_ssl_context` mbedtls ssl
     * context and allows access to it using `native_handle` method.
     */
    class mbedtls_impl : public ssl_impl_base {
        struct inner;

    public:
        using context_type = context;
        using bio_type = ring_bio;

        mbedtls_impl() = default;

        mbedtls_impl(mbedtls_impl&&) noexcept = default;

        mbedtls_impl& operator=(mbedtls_impl&&) noexcept = default;

        RAD_EXPORT_DECL mbedtls_impl(context& ctx);

        RAD_EXPORT_DECL ~mbedtls_impl();

        /*!
         * @brief Get a pointer to the native ssl
         * implementation. For this MbedTLS implementation the
         * returned pointer can be casted to
         * mbedtls_ssl_context*. If the implementation is
         * closed, the returned pointer is null.
         * @return A pointer to the native ssl implementation
         * which can be casted to mbedtls_ssl_context*.
         */
        RAD_EXPORT_DECL void* native_handle() const noexcept override;

        /*!
         * @brief Create an implementation using @p ctx and
         * replace the existing one, if any, by the new created
         * implementation. On failure, the current
         * implementation is not changed.
         * @param ctx The context the implementation will be
         * associated to. It must be an mbedtls::context,
         * otherwise the behavior is undefined.
         * @param ec Set to indicate errors, if any.
         */
        void open(context_base& ctx, std::error_code& ec) noexcept override {
            open(static_cast<context_type&>(ctx), ec);
        }

        /*!
         * @brief Create a new implementation using the current
         * context and replace the existing one, if any, by the
         * new created implementation. If there is not current
         * implementation, the function fails. On failure, the
         * current implementation is not changed.
         * @param ec Set to indicate errors, if any.
         */
        RAD_EXPORT_DECL void reopen(std::error_code& ec) noexcept override;

        /*!
         * @brief Set the read and write ring buffer BIOs.
         * If there is not current implementation, the behavior
         * is undefined.
         * @param rbio The read ring buffer BIO.
         * @param wbio The write ring buffer BIO.
         */
        RAD_EXPORT_DECL void set_bios(ring_bio& rbio,
                                      ring_bio& wbio) noexcept override;

        /*!
         * @brief Set the hostname (SNI server name indication).
         * Many hosts need the hostname be set to handshake
         * successfully. If there is not current implementation,
         * the behavior is undefined.
         * @param hostname The domain of the target url.
         */
        RAD_EXPORT_DECL void
        set_hostname(std::string_view host) noexcept override;

        /*!
         * @brief Set whether to act as a client or server.
         * If there is not current implementation, the behavior
         * is undefined.
         * @param is_client If true, the implementation will be
         * a client one, otherwise it will be server.
         */
        RAD_EXPORT_DECL void set_mode(bool is_client) noexcept override;

        /*!
         * @brief Get the version of the current protocol used.
         * If there is not current implementation, the returned
         * string will be empty.
         * @return The version of the current protocol used.
         */
        RAD_EXPORT_DECL std::string_view version() const noexcept override;

        /*!
         * @brief Get the selected ALPN protocol during
         * handshake. If there is not current implementation or
         * there is no selected ALPN protocol, the returned
         * string will be empty.
         * @return The selected ALPN protocol during handshake.
         */
        RAD_EXPORT_DECL std::string_view
        get_alpn_protocol() const noexcept override;

        /*!
         * @brief Set the ALPN protocol list.
         * @param protos The ALPN protocol list.
         */
        RAD_EXPORT_DECL void set_alpn_protocols(
            std::span<const std::string_view> protos) noexcept override;

        /*!
         * @brief Do handshake in non blocking mode and update
         * the engine state accordingly. If there is not current
         * implementation, the behavior is undefined. The
         * implementation must be configured as either client or
         * server before doing handshake, and the read and write
         * BIOs must have been attached.
         * @param state The ssl engine state.
         * @param ec Set to indicate errors, if any.
         */
        RAD_EXPORT_DECL void handshake(engine_state& state,
                                       std::error_code& ec) noexcept override;

        /*!
         * @brief Encrypt and write from input buffer @p buff to
         * the write BIO buffer and update the engine state. The
         * operation is non blocking. If there is not current
         * implementation, the behavior is undefined. The
         * implementation must be configured as either client or
         * server before doing write, and the read and write
         * BIOs must have been attached.
         * @param buff The input non encrypted buffer.
         * @param state The ssl engine state.
         * @param ec Set to indicate errors, if any.
         * @return Count of bytes written from @p buff.
         */
        RAD_EXPORT_DECL std::size_t
        write(const_buffer buff, engine_state& state,
              std::error_code& ec) noexcept override;

        /*!
         * @brief Decrypt and read from read BIO buffer to the
         * output buffer @p buff and update the engine state.
         * The operation is non blocking. If there is not
         * current implementation, the behavior is undefined.
         * The implementation must be configured as either
         * client or server before doing write, and the read and
         * write BIOs must have been attached.
         * @param buff The output decrypted buffer.
         * @param state The ssl engine state.
         * @param ec Set to indicate errors, if any.
         * @return Count of bytes written to @p buff.
         */
        RAD_EXPORT_DECL std::size_t read(mutable_buffer buff,
                                         engine_state& state,
                                         std::error_code& ec) noexcept override;

        /*!
         * @brief Shutdown the session and update the engine
         * state. The operation is non blocking. This does not
         * shutdown or close the underlying transport. If there
         * is not current implementation, the behavior is
         * undefined. The implementation must be configured as
         * either client or server before doing write, and the
         * read and write BIOs must have been attached.
         * @param state The ssl engine state.
         * @param ec Set to indicate errors, if any.
         */
        RAD_EXPORT_DECL void shutdown(engine_state& state,
                                      std::error_code& ec) noexcept override;

        /*!
         * @brief Check if there is an underlying
         * implementation.
         * @return True if there is an underlying
         * implementation, otherwise false.
         */
        bool is_open() const noexcept {
            return impl_ != nullptr;
        }

        /*!
         * @brief Check if there is an underlying
         * implementation.
         * @return True if there is an underlying
         * implementation, otherwise false.
         */
        explicit operator bool() const noexcept {
            return is_open();
        }

        /*!
         * @brief Create an implementation using
         * mbedtls::context @p ctx and replace the existing one,
         * if any, by the new created implementation. On
         * failure, the current implementation is not changed.
         * @param ctx The context the implementation will be
         * associated to.
         * @param ec Set to indicate errors, if any.
         */
        RAD_EXPORT_DECL void open(context_type& ctx,
                                  std::error_code& ec) noexcept;

        /*!
         * @brief Create an implementation using
         * mbedtls::context @p ctx and replace the existing one,
         * if any, by the new created implementation. On
         * failure, the current implementation is not changed.
         * @param ctx The context the implementation will be
         * associated to.
         */
        void open(context_type& ctx) {
            std::error_code ec;
            open(ctx, ec);
            check_and_throw(ec, __func__);
        }

        /*!
         * @brief Create a new implementation using the current
         * context and replace the existing one, if any, by the
         * new created implementation. If there is not current
         * implementation, the function fails. On failure, the
         * current implementation is not changed.
         */
        void reopen() {
            std::error_code ec;
            reopen(ec);
            check_and_throw(ec, "open");
        }

        /*!
         * @brief Close and free the current implementation, if
         * any. This will not send a TLS shutdown or close the
         * transport layer used to transfer the TLS data.
         */
        RAD_EXPORT_DECL void close() noexcept;

        RAD_EXPORT_DECL bool is_handshake_done() const noexcept;

    private:
        static int ring_io_send(void* ctx, const unsigned char* buf,
                                size_t len) noexcept;

        static int ring_io_recv(void* ctx, unsigned char* buf,
                                size_t len) noexcept;

        std::size_t flush_output(engine_state& state,
                                 std::error_code& ec) noexcept;

        std::unique_ptr<inner> impl_;
    };
} // namespace RAD_LIB_NAMESPACE::net::ssl::mbedtls