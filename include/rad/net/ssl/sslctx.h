#pragma once
#include <rad/libbase.h>
#include <rad/ring_buffer_consumer.h>
#include <rad/trackable.h>

#include <memory>
#include <optional>

namespace RAD_LIB_NAMESPACE::net::ssl {
    /*!
     * @brief The TLS version and mode (client or server) to use.
     * To negotiate the max available with the peer, use tls_client and
     * tls_server.
     */
    enum class version : std::uint32_t {
        tls_client,
        tls_server,

        tlsv1_client,
        tlsv1_server,

        tlsv11_client,
        tlsv11_server,

        tlsv12_client,
        tlsv12_server,

        tlsv13_client,
        tlsv13_server,
    };

    /*!
     * @brief Check if version @p v is a client version.
     * @param v The version to check.
     * @return True if version @p v is a client version, otherwise false.
     */
    inline constexpr bool is_client_version(version v) noexcept {
        return static_cast<std::uint32_t>(v) % 2 == 0;
    }

    /*!
     * @brief Check if version @p v is a server version.
     * @param v The version to check.
     * @return True if version @p v is a server version, otherwise false.
     */
    inline constexpr bool is_server_version(version v) noexcept {
        return static_cast<std::uint32_t>(v) % 2 != 0;
    }

    /*!
     * @brief The type of handshake the stream should perform.
     */
    enum class handshake_type {
        client,
        server,
    };

    /*!
     * @brief The peer certificate verfication mode.
     * For servers the default is none.
     * For clients the default is peer.
     */
    enum class verify_mode {
        required,
        optional,
        none,
    };

    /*!
     * @brief The purpose of calling password callback.
     */
    enum class password_purpose : int {
        reading = 0,
        decrypting = 0,
        writing = 1,
        encrypting = 1,
    };

    /*!
     * @brief The format of private keys and certificates.
     */
    enum class file_format : int {
        pem = 1,
        asn1,
    };

    RAD_OVERLOAD_ENUM_OPERATORS(verify_mode);

    /*!
     * @brief Base of X509 certificates verify callbacks.
     */
    struct verify_callback_base {
        virtual ~verify_callback_base() = default;

        /*!
         * @brief Verify the X509 certificate during handshake.
         * @param preverify_ok If true, the certificate was
         * verified before calling this method, otherwise the
         * certificate verfication failed and the certificate
         * was considered unverified.
         * @param x509_ctx The X509 certificate pointer.
         * For openssl this pointer can be casted to
         * `X509_STORE_CTX*`. For wolfssl this pointer can be
         * casted to `WOLFSSL_X509_STORE_CTX*`. For mbedtls this
         * pointer can be casted to `mbedtls_x509_crt*`. Other
         * implementations may have other types of certificates.
         * @return True if the certificate was decided to be
         * valid, otherwise false to indicate unverified
         * certificate. The return overrides the previous
         * verification result indicated by @p preverify_ok.
         */
        virtual bool verify(bool preverify_ok, void* x509_ctx) = 0;
    };

    /*!
     * @brief Base of password callbacks.
     * Password callbacks provide password on demand.
     */
    struct password_callback_base {
        virtual ~password_callback_base() = default;

        /*!
         * @brief Get the password for encryting or decrypting.
         * @param password_buff The buffer where the method will
         * write the password to.
         * @param purpose The purpose of calling this method for
         * password. Either decrpting (read), or encrypting
         * (write)
         * @return The count of written bytes in password buffer
         * on success, and a null optional on failure. The count
         * of written bytes must not be greater than passed
         * buffer size.
         */
        virtual std::optional<std::size_t>
        get_password(mutable_buffer password_buff,
                     password_purpose purpose) = 0;
    };

    template <class T>
    concept VerifyCallback = requires(T t, bool ok, void* cert) {
        { t(ok, cert) } -> std::same_as<bool>;
    };

    template <class T>
    concept PasswordCallback =
        requires(T t, mutable_buffer b, password_purpose p) {
            { t(b, p) } -> std::same_as<std::optional<std::size_t>>;
        };

    template <PasswordCallback Callback>
    std::unique_ptr<password_callback_base>
    make_password_callback(Callback cb, std::error_code& ec) noexcept {
        struct passwd_cb : public password_callback_base {
            Callback cb;

            passwd_cb(Callback&& cb) : cb{std::move(cb)} {
            }

            std::optional<std::size_t>
            get_password(mutable_buffer passwd_buff,
                         password_purpose purpose) override {
                return cb(passwd_buff, purpose);
            }
        };

        ec.clear();
        passwd_cb* callback = new (std::nothrow) passwd_cb{std::move(cb)};
        if (callback == nullptr) {
            ec = std::make_error_code(std::errc::not_enough_memory);
            return nullptr;
        }
        return std::unique_ptr<passwd_cb>{callback};
    }

    template <VerifyCallback Callback>
    std::unique_ptr<verify_callback_base>
    make_verify_callback(Callback cb, std::error_code& ec) noexcept {
        struct verify_cb : public verify_callback_base {
            Callback cb;

            verify_cb(Callback&& cb) : cb{std::move(cb)} {
            }

            bool verify(bool preverify_ok, void* x509_ctx) override {
                return cb(preverify_ok, x509_ctx);
            }
        };

        ec.clear();
        verify_cb* callback = new (std::nothrow) verify_cb{std::move(cb)};
        if (callback == nullptr) {
            ec = std::make_error_code(std::errc::not_enough_memory);
            return nullptr;
        }
        return std::unique_ptr<verify_cb>{callback};
    }

    /*!
     * @brief A ring buffer view used to exchange input and output
     * with ssl implementations.
     */
    using ring_bio = ring_consumer_producer;

    struct ssl_impl_base;

    /*!
     * @brief Base interface for ssl context.
     */
    struct context_base : public trackable {
        virtual ~context_base() = default;

        /*!
         * @brief Make an ssl implementation derived from
         * `ssl_impl_base`.
         * @param ec Set to indicate errors, if any.
         * @return The ssl implementation associated with this
         * context on sucess. On failure null is returned. The
         * returned ssl implementation depends on this context
         * so this context must remain valid as long as any ssl
         * implementation is using it.
         */
        virtual std::unique_ptr<ssl_impl_base>
        make_generic_impl(std::error_code& ec) noexcept = 0;

        /*!
         * @brief Make an ssl implementation derived from
         * `ssl_impl_base`.
         * @return The ssl implementation associated with this
         * context. The returned ssl implementation depends on
         * this context so this context must remain valid as
         * long as any ssl implementation is using it.
         */
        std::unique_ptr<ssl_impl_base> make_generic_impl() {
            std::error_code ec;
            auto impl = make_generic_impl(ec);
            if (ec) {
                throw std::system_error{ec};
            }
            return impl;
        }
    };

    /*!
     * @brief SSL engine state.
     */
    enum class engine_state {
        done,
        want_write,
        want_read,
        error,
    };

    /*!
     * @brief Base interface for ssl implementations.
     *
     * The implementation provides access to its native type pointer
     * using native_handle().
     */
    struct ssl_impl_base : public trackable {
        virtual ~ssl_impl_base() = default;

        /*!
         * @brief Get a pointer to the native ssl
         * implementation.
         *
         * For OpenSSL implementation the returned pointer can
         * be casted to SSL*.
         *
         * For WolfSSL implementation the returned pointer can
         * be casted to WOLFSSL*.
         *
         * For MbedTLS implementation the returned pointer can
         * be casted to mbedtls_ssl_context*.
         * @return A pointer to the native ssl implementation.
         */
        virtual void* native_handle() const noexcept = 0;

        /*!
         * @brief Create an implementation using @p ctx and
         * replace the existing one, if any, by the new created
         * implementation. On failure, the current
         * implementation is not changed.
         * @param ctx The context the implementation will be
         * associated to. It must be a context compatible with
         * this implementation, otherwise the behavior is
         * undefined.
         * @param ec Set to indicate errors, if any.
         */
        virtual void open(context_base& ctx, std::error_code& ec) noexcept = 0;

        /*!
         * @brief Create a new implementation using the current
         * context and replace the existing one, if any, by the
         * new created implementation. If there is not current
         * implementation, the function fails. On failure, the
         * current implementation is not changed.
         * @param ec Set to indicate errors, if any.
         */
        virtual void reopen(std::error_code& ec) noexcept = 0;

        /*!
         * @brief Set the read and write ring buffer BIOs.
         * If there is not current implementation, the behavior
         * is undefined.
         * @param rbio The read ring buffer BIO.
         * @param wbio The write ring buffer BIO.
         */
        virtual void set_bios(ring_bio& rbio, ring_bio& wbio) noexcept = 0;

        /*!
         * @brief Set the hostname (SNI server name indication).
         * Many hosts need the hostname be set to handshake
         * successfully. If there is not current implementation,
         * the behavior is undefined.
         * @param hostname The domain of the target url
         */
        virtual void set_hostname(std::string_view hostname) noexcept = 0;

        /*!
         * @brief Set whether to act as a client or server.
         * If there is not current implementation, the behavior
         * is undefined.
         * @param is_client If true, the implementation will be
         * a client one, otherwise it will be server.
         */
        virtual void set_mode(bool is_client) noexcept = 0;

        /*!
         * @brief Get the version of the current protocol used.
         * If there is not current implementation, the returned
         * string will be empty.
         * @return The version of the current protocol used.
         */
        virtual std::string_view version() const noexcept = 0;

        /*!
         * @brief Get the selected ALPN protocol during
         * handshake. If there is not current implementation or
         * there is no selected ALPN protocol, the returned
         * string will be empty.
         * @return The selected ALPN protocol during handshake.
         */
        virtual std::string_view get_alpn_protocol() const noexcept = 0;

        /*!
         * @brief Set the ALPN protocol list.
         * If there is not current implementation, the behavior
         * is undefined. For MbedTLS implementation, the
         * protocols are set on the associated context and any
         * MbedTLS implementation associated with this context
         * will use the same protocol list.
         * @param protos The ALPN protocol list.
         */
        virtual void set_alpn_protocols(
            std::span<const std::string_view> protos) noexcept = 0;

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
        virtual void handshake(engine_state& state,
                               std::error_code& ec) noexcept = 0;

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
        virtual std::size_t write(const_buffer buff, engine_state& state,
                                  std::error_code& ec) noexcept = 0;

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
        virtual std::size_t read(mutable_buffer buff, engine_state& state,
                                 std::error_code& ec) noexcept = 0;

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
        virtual void shutdown(engine_state& state,
                              std::error_code& ec) noexcept = 0;
    };

    /*!
     * @brief SSL engine constains an ssl implementation, read and write
     * BIOs buffers and structures. The ssl stream should use this engine
     * instead of using an ssl implementation directly.
     */
    class engine {
    public:
        // from openssl crypto/bio/bss_bio.c : b->size = 17 *
        // 1024; if the ring buffer space is not large enough to
        // hold the largest tls record then receiving and
        // decrypting such record will be impossible

        /*!
         * @brief The maximim TLS record size including
         * metadata.
         */
        static constexpr std::size_t max_tls_record_size = 17 * 1024;

        /*!
         * @brief The size of buffer used for read and write
         * BIOs. This buffer is allocated on the heap.
         */
        static constexpr std::size_t tls_in_out_buffer_size =
            2 * max_tls_record_size;

        /*!
         * @brief Create an ssl implementation associated with
         * @p ctx, allocate the read and write BIOs buffer and
         * attach the BIOs to the implementation. The engine is
         * ready to use after construction.
         * @param ctx The context the implementation will be
         * associated to.
         */
        engine(context_base& ctx)
            : in_out_buffers_(tls_in_out_buffer_size),
              out_bio_{ring_bio{buffer(in_out_buffers_) + max_tls_record_size}},
              in_bio_{ring_bio{buffer(in_out_buffers_, max_tls_record_size)}},
              impl_{ctx.make_generic_impl()} {
            impl_->set_bios(in_bio_, out_bio_);
        }

        /*!
         * @brief Move construct the engine. The implementation
         * and read and write BIOs buffer are taken from @p
         * other. After move, the valid operations to perform on
         * @p other is to open it again using open(), move
         * assign it or destroy it.
         * @param other The engine to take its resources.
         */
        engine(engine&& other) noexcept
            : in_out_buffers_{std::move(other.in_out_buffers_)},
              out_bio_{other.out_bio_}, in_bio_{other.in_bio_},
              impl_{std::move(other.impl_)} {
            if (impl_ != nullptr) {
                impl_->set_bios(in_bio_, out_bio_);
            }
        }

        /*!
         * @brief Move assign the engine. The implementation and
         * read and write BIOs buffer are taken from @p other.
         * After move, the valid operations to perform on @p
         * other is to open it again using open(), move assign
         * it or destroy it.
         * @param other The engine to take its resources.
         * @return Reference to self.
         */
        engine& operator=(engine&& other) noexcept {
            if (this == std::addressof(other)) {
                return *this;
            }
            in_out_buffers_ = std::move(other.in_out_buffers_);
            out_bio_ = other.out_bio_;
            in_bio_ = other.in_bio_;
            impl_ = std::move(other.impl_);
            if (impl_ != nullptr) {
                impl_->set_bios(in_bio_, out_bio_);
            }
            return *this;
        }

        /*!
         * @brief Check if this engine has an implementation.
         * @return True if this engine has an implementation,
         * otherwise false.
         */
        bool is_valid() const noexcept {
            return impl_ != nullptr;
        }

        /*!
         * @brief Create a new implementation for this engine.
         * On success the current implementation, if there is
         * one, is replaced with the new implementation, and on
         * failure the current implementation is not changed.
         * The read and wrtie BIOs buffer is not reallocated,
         * and it will be reused with the new implementation.
         * @param ctx The context the implementation will be
         * associated to.
         * @param ec Set to indicate errors, if any.
         */
        void open(context_base& ctx, std::error_code& ec) noexcept {
            ec.clear();
            if (impl_ == nullptr) {
                impl_ = ctx.make_generic_impl(ec);
            }
            else {
                impl_->open(ctx, ec);
            }
            if (ec) {
                return;
            }
            in_bio_ = ring_bio{buffer(in_out_buffers_, max_tls_record_size)};
            out_bio_ = ring_bio{buffer(in_out_buffers_) + max_tls_record_size};
            impl_->set_bios(in_bio_, out_bio_);
        }

        /*!
         * @brief Create a new implementation for this engine.
         * On success the current implementation, if there is
         * one, is replaced with the new implementation, and on
         * failure the current implementation is not changed.
         * The read and wrtie BIOs buffer is not reallocated,
         * and it will be reused with the new implementation.
         * @param ctx The context the implementation will be
         * associated to.
         */
        void open(context_base& ctx) {
            std::error_code ec;
            open(ctx, ec);
            check_and_throw(ec, "open");
        }

        /*!
         * @brief Create a new implementation using the current
         * context and replace the existing one, if any, by the
         * new created implementation. If there is not current
         * implementation, the function fails. On failure, the
         * current implementation is not changed.
         * @param ec Set to indicate errors, if any.
         */
        void reopen(std::error_code& ec) noexcept {
            ec.clear();
            if (!is_valid()) {
                ec = std::make_error_code(std::errc::bad_file_descriptor);
                return;
            }
            impl_->reopen(ec);
            if (ec) {
                return;
            }
            in_bio_ = ring_bio{buffer(in_out_buffers_, max_tls_record_size)};
            out_bio_ = ring_bio{buffer(in_out_buffers_) + max_tls_record_size};
            impl_->set_bios(in_bio_, out_bio_);
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
         * @brief Close the current implementation, if there is
         * one. After close, the valid operations to perform on
         * this engine is to open it again using open(), move
         * assign it or destroy it.
         */
        void close() noexcept {
            impl_ = nullptr;
        }

        /*!
         * @brief Get the version of the current protocol used.
         * If there is not current implementation, the returned
         * string will be empty.
         * @return The version of the current protocol used.
         */
        std::string_view version() const noexcept {
            return impl_->version();
        }

        /*!
         * @brief Set the list of protocols available to be
         * negotiated. The @p protos must be in protocol-list
         * format.
         * @param protos The protocols array.
         */
        void set_alpn_protocols(std::span<const std::string_view> protos) {
            impl_->set_alpn_protocols(protos);
        }

        /*!
         * @brief Set the list of protocols available to be
         * negotiated. The @p protos must be in protocol-list
         * format.
         * @param protos The protocols array.
         */
        void set_alpn_protocols(std::span<const std::string> protos) {
            std::vector<std::string_view> protos_view;
            for (const auto& proto : protos) {
                protos_view.emplace_back(proto);
            }
            impl_->set_alpn_protocols(protos_view);
        }

        /*!
         * @brief Get the selected ALPN protocol during
         * handshake.
         * @return The selected ALPN protocol during handshake.
         */
        std::string_view get_alpn_protocol() const noexcept {
            return impl_->get_alpn_protocol();
        }

        /*!
         * @brief Set the hostname (SNI server name indication).
         * Many hosts need the hostname be set to handshake
         * successfully. If there is not current implementation,
         * the behavior is undefined.
         * @param hostname The domain of the target url
         */
        void set_hostname(std::string_view hostname) noexcept {
            impl_->set_hostname(hostname);
        }

        /*!
         * @brief Set the current implementation to act as a
         * client. If there is not current implementation, the
         * behavior is undefined.
         */
        void set_client_mode() noexcept {
            impl_->set_mode(true);
        }

        /*!
         * @brief Set the current implementation to act as a
         * server. If there is not current implementation, the
         * behavior is undefined.
         */
        void set_server_mode() noexcept {
            impl_->set_mode(false);
        }

        /*!
         * @brief Do handshake in non blocking mode and update
         * the engine state accordingly. If there is not current
         * implementation, the behavior is undefined. The
         * implementation must be configured as either client or
         * server before doing handshake, and have read and
         * write BIOs must have been attached.
         * @param state The ssl engine state.
         * @param ec Set to indicate errors, if any.
         */
        void handshake(engine_state& state, std::error_code& ec) noexcept {
            impl_->handshake(state, ec);
        }

        /*!
         * @brief Shutdown the session and update the engine
         * state. The operation is non blocking. This does not
         * shutdown or close the underlying transport. If there
         * is not current implementation, the behavior is
         * undefined. The implementation must be configured as
         * either client or server before doing write, and have
         * read and write BIOs must have been attached.
         * @param state The ssl engine state.
         * @param ec Set to indicate errors, if any.
         */
        void shutdown(engine_state& state, std::error_code& ec) noexcept {
            impl_->shutdown(state, ec);
        }

        /*!
         * @brief Decrypt and read from the available encrypted
         * input buffers into @p buff. This method will not
         * return until the buff is fully filled or an error
         * occurs.
         * @param buff the buffer to write decrypted data into
         * @param state the state of the engine after operation
         * (done if no error, want read or write if no
         * sufficient buffer and error if another error has
         * occured)
         * @param ec set to empty error_code if no error, and
         * set to error otherwise.
         * @return the number of bytes written into buff even if
         * an error has occured.
         */
        std::size_t get_input(mutable_buffer buff, engine_state& state,
                              std::error_code& ec) noexcept {
            return impl_->read(buff, state, ec);
        }

        /*!
         * @brief get available input buffers to read encrypted
         * data into. After reading into these buffers use
         * commit_input_buffers with the number of bytes read
         * @return two buffers to be filled in order. Any or
         * both of them may be empty
         */
        std::array<mutable_buffer, 2> available_input_buffers() noexcept {
            return in_bio_.available_space();
        }

        /*!
         * @brief inform the engine that n encrypted bytes were
         * transferred into avaialble input buffers
         * @param n number of bytes read
         */
        void commit_input_buffers(std::size_t n) noexcept {
            in_bio_.commit(n);
        }

        /*!
         * @brief encrypt and write buff into available output
         * buffers. This method will not return until the buff
         * is fully written or an error occurs. After writing
         * use available_output_buffers to get the written
         * encrypted data and transfer it
         * @param buff the buffer to encrypt and write into
         * available output buffers
         * @param state the state of the engine after operation
         * (done if no error, want read or write if no
         * sufficient buffer and error if another error has
         * occured)
         * @param ec set to empty error_code if no error and set
         * to error error otherwise
         * @return the number of bytes written from buff into
         * available output buffers even if an error has occured
         */
        std::size_t put_output(const_buffer buff, engine_state& state,
                               std::error_code& ec) noexcept {
            return impl_->write(buff, state, ec);
        }

        /*!
         * @brief Get available encrypted output buffers ready
         * to be transfered. After sending these buffers use
         * consume_output_buffers
         * @return two buffers to be sent in order. Any or both
         * of them may be empty
         */
        std::array<const_buffer, 2> available_output_buffers() const noexcept {
            return out_bio_.available_buffers();
        }

        /*!
         * @brief inform the engine that all available encrypted
         * output buffers were transferred and make them
         * available to use.
         */
        void consume_output_buffers() noexcept {
            out_bio_.clear();
        }

    private:
        std::vector<uint8_t> in_out_buffers_;
        ring_bio out_bio_;
        ring_bio in_bio_;
        std::unique_ptr<ssl_impl_base> impl_;
    };
} // namespace RAD_LIB_NAMESPACE::net::ssl
