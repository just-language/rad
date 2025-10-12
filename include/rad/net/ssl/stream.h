#pragma once
#include <rad/net/ssl/sslctx.h>
#include <rad/async/io_executor.h>

namespace RAD_LIB_NAMESPACE::net::ssl {
    namespace details = RAD_LIB_NAMESPACE::detail;

    /*!
     * @brief The handler passed to async_handshake() method must satisfy
     * the requirements of this concept. The following expression must be
     * valid: handler(std::error_code{})
     */
    template <class Handler>
    concept HandShakeHandler =
        requires(Handler handler) { handler(std::error_code{}); };

    /*!
     * @brief The handler passed to async_shutdown() method must satisfy the
     * requirements of this concept. The following expression must be valid:
     * handler(std::error_code{})
     */
    template <class Handler>
    concept ShutdownHandler =
        requires(Handler handler) { handler(std::error_code{}); };

    namespace detail {
        template <class Layer, class T>
        concept ReOpenableNextLayer1 = requires(Layer& layer, T&& arg) {
            layer.open(std::forward<T>(arg));
        };

        template <class Layer, class T>
        concept ReOpenableNextLayer2 =
            requires(Layer& layer, T&& arg, std::error_code& ec) {
                layer.open(std::forward<T>(arg), ec);
            };
    } // namespace detail

    /*!
     * @brief An async SSL stream that writes and reads encrypted data.
     * @tparam NextLayer the underlying io stream type that will be used by
     * the ssl stream to send and receive encrypted data.
     */
    template <class NextLayer>
    class stream {
        struct handshake_op_base;
        struct write_op_base;
        struct read_op_base;
        struct shutdown_op_base;

        friend struct handshake_op_base;
        friend struct write_op_base;
        friend struct read_op_base;
        friend struct shutdown_op_base;

        template <class Handler, class Alloc>
        struct handshake_op;
        template <class Handler, class Buffers, class Alloc>
        struct write_op;
        template <class Handler, class Buffers, class Alloc, bool ReadAll>
        struct read_op;
        template <class Handler, class Alloc>
        struct shutdown_op;

        class handshake_awaiter;
        template <class Buffers>
        class write_awaiter;
        template <class Buffers, bool ReadAll>
        class read_awaiter;
        class shutdown_awaiter;

        friend class handshake_awaiter;
        friend class shutdown_awaiter;
        template <typename>
        friend class write_awaiter;
        template <typename, bool>
        friend class read_awaiter;

        struct allocator_types {
            using ops = op_alloc_type;

            using write_buffers_type = std::array<const_buffer, 2>;
            using read_buffers_type = std::array<mutable_buffer, 2>;

            static constexpr ops op_type = ops::write | ops::read;
            static constexpr std::size_t max_handler_size = sizeof(void*);
        };

        static constexpr std::size_t calc_allocator_size() {
            return NextLayer::template max_allocator_size<allocator_types>();
        }

        inline static constexpr std::size_t max_next_layer_alloc_size =
            calc_allocator_size();

    public:
        /*!
         * @brief The type of the next layer io stream used by
         * the ssl stream
         */
        using next_layer_type = NextLayer;
        /*!
         * @brief The type of the lowest layer io stream used by
         * the next layer
         */
        using lowest_layer_type = typename next_layer_type::lowest_layer_type;
        /*!
         * @brief The type of executor used by this ssl stream.
         * This is the executor used by its next layer
         */
        using executor_type = typename next_layer_type::executor_type;

        /*!
         * @brief Construct an async ssl stream and its
         * unerlying io layer
         * @tparam ...Args The type of arguments passed to the
         * next layer
         * @param ctx The ssl context to be used by the stream
         * to create ssl sessions
         * @param ...args Arguments to be passed to the next io
         * layer
         */
        template <class... Args>
            requires std::constructible_from<NextLayer, Args...>
        stream(context_base& ctx, Args&&... args)
            : next_layer_{std::forward<Args>(args)...}, engine_{ctx} {
        }

        /*!
         * @brief Re-create the ssl stream using an ssl context
         * and re-open the unerlying io layer
         * @tparam Arg Type of argument passed to the next layer
         * @param ctx The ssl context to be used by the stream
         * to create ssl sessions
         * @param arg Argument to be passed to the next io layer
         */
        template <class Arg>
            requires(detail::ReOpenableNextLayer1<NextLayer, Arg>)
        void open(context_base& ctx, Arg&& arg) {
            engine new_engine{ctx};
            next_layer().open(std::forward<Arg>(arg));
            engine_ = std::move(new_engine);
        }

        /*!
         * @brief Re-create the ssl stream using the same ssl
         * context associated with the stream and re-open the
         * unerlying io layer. If this function fails the state
         * of the stream is valid but unspecified. Note that the
         * stream must be open (even if in shutdown state) prior
         * to this call to use its ssl context.
         * @tparam Arg Type of argument passed to the next layer
         * @param arg Argument to be passed to the next io layer
         * @param ec Cleared on success, and assigned an error
         * on failure
         */
        template <class Arg>
            requires(detail::ReOpenableNextLayer2<NextLayer, Arg>)
        void reopen(Arg&& arg, std::error_code& ec) {
            ec.clear();
            engine_.reopen(ec);
            if (ec) {
                return;
            }
            next_layer().open(std::forward<Arg>(arg), ec);
        }

        /*!
         * @brief Re-create the ssl stream using the same ssl
         * context associated with the stream and re-open the
         * unerlying io layer. If this function fails the state
         * of the stream is valid but unspecified. Note that the
         * stream must be open (even if in shutdown state) prior
         * to this call to use its ssl context.
         * @tparam Arg Type of argument passed to the next layer
         * @param arg Argument to be passed to the next io layer
         * @throws An exception of type std::system_error is
         * thrown on failure
         */
        template <class Arg>
            requires(detail::ReOpenableNextLayer1<NextLayer, Arg>)
        void reopen(Arg&& arg) {
            engine_.reopen();
            next_layer().open(std::forward<Arg>(arg));
        }

        /*!
         * @brief Re-create the ssl stream using the same ssl
         * context associated with the stream and don't re-open
         * the unerlying io layer. If this function fails the
         * state of the stream is valid but unspecified. Note
         * that the stream must be open (even if in shutdown
         * state) prior to this call to use its ssl context. The
         * unerlying io layer will ususally need to be re-opened
         * by opening or connecting.
         * @param ec Cleared on success, and assigned an error
         * on failure.
         */
        void reopen(std::error_code& ec) noexcept {
            ec.clear();
            engine_.reopen(ec);
        }

        /*!
         * @brief Re-create the ssl stream using the same ssl
         * context associated with the stream and don't re-open
         * the unerlying io layer. If this function fails the
         * state of the stream is valid but unspecified. Note
         * that the stream must be open (even if in shutdown
         * state) prior to this call to use its ssl context. The
         * unerlying io layer will ususally need to be re-opened
         * by opening or connecting.
         * @throws An exception of type std::system_error is
         * thrown on failure
         */
        void reopen() {
            engine_.reopen();
        }

        /*!
         * @brief Closes the ssl engine and the underlying io
         * layer. Close will cancel all pending async operations
         * on the stream. Note that not all operations can be
         * canceled. Operations that have completed and are
         * scheduled for invocation can no longer be canceled.
         */
        void close() noexcept {
            engine_.close();
            next_layer().close();
        }

        /*!
         * @brief Check if the ssl stream and the underlying io
         * layer are open
         * @return true if the ssl stream and the underlying io
         * layer are open, and false otherwise
         */
        bool is_open() const noexcept {
            return engine_.is_valid() && next_layer_.is_open();
        }

        /*!
         * @brief Get a reference to the next layer.
         * @return A reference to the next layer.
         */
        next_layer_type& next_layer() noexcept {
            return next_layer_;
        }

        /*!
         * @brief Get a reference to the next layer.
         * @return A reference to the next layer.
         */
        const next_layer_type& next_layer() const noexcept {
            return next_layer_;
        }

        /*!
         * @brief Get a reference to the lowest layer.
         * @return A reference to the lowest layer.
         */
        lowest_layer_type& lowest_layer() noexcept {
            return next_layer_.lowest_layer();
        }

        /*!
         * @brief Get a reference to the lowest layer.
         * @return A reference to the lowest layer.
         */
        const lowest_layer_type& lowest_layer() const noexcept {
            return next_layer_.lowest_layer();
        }

        /*!
         * @brief Get a reference to the ssl engine used by the
         * stream
         * @return A reference to the ssl engine used by the
         * stream
         */
        engine& ssl_engine() noexcept {
            return engine_;
        }

        /*!
         * @brief Get a reference to the ssl engine used by the
         * stream
         * @return A reference to the ssl engine used by the
         * stream
         */
        const engine& ssl_engine() const noexcept {
            return engine_;
        }

        /*!
         * @brief Get a reference to the executor used by the
         * next layer
         * @return A reference to the executor used by the next
         * layer
         */
        executor_type& executor() noexcept {
            return next_layer().executor();
        }

        /*!
         * @brief Get a reference to the executor used by the
         * next layer
         * @return A reference to the executor used by the next
         * layer
         */
        const executor_type& executor() const noexcept {
            return next_layer().executor();
        }

        /*!
         * @brief Get the ssl version used by this stream if it
         * is open, and an empty string if closed. The returned
         * view string (if not empty) is valid until the stream
         * is closed, destroyed or moved to and from it.
         * @return The ssl version used by this stream.
         */
        std::string_view version() const noexcept {
            if (!engine_.is_valid()) {
                return "";
            }
            return engine_.version();
        }

        /*!
         * @brief Set the hostname (SNI server name indication)
         * on the ssl session. Many hosts need the hostname be
         * set to handshake successfully.
         * @param hostname The domain of the target url
         */
        void set_hostname(std::string_view hostname) noexcept {
            engine_.set_hostname(hostname);
        }

        /*!
         * @brief Perform async ssl handshake. The underlying io
         * layer must be ready for data transport prior to
         * performing the handshake. Note that the async
         * operation will not start until the returned awaitable
         * is awaited.
         * @param type The type of handshaking, either client or
         * server.
         * @param ec If set to no_ec then errors are reported
         * via exceptions, otherwise errors are reported via @p
         * ec.
         * @return An awaitable that is when awaited will start
         * the async operation.
         */
        handshake_awaiter async_handshake(handshake_type type,
                                          std::error_code& ec = no_ec);

        /*!
         * @brief Perform async write on the ssl stream. The
         * handshake must have been done successfully prior to
         * reading and writing. Note that the async operation
         * will not start until the returned awaitable is
         * awaited. The operation will complete when all data in
         * @p buffers is written or an error occurs.
         * @tparam Buffers The type of buffers sequence
         * @param buffers The data to be written. The underlying
         * buffers must be valid until the operation completes.
         * @param ec If set to no_ec then errors are reported
         * via exceptions, otherwise errors are reported via @p
         * ec.
         * @return An awaitable that is when awaited will start
         * the async operation and return the count of bytes
         * transferred.
         */
        template <BufferSequence Buffers>
        write_awaiter<Buffers> async_write(Buffers&& buffers,
                                           std::error_code& ec = no_ec);

        /*!
         * @brief Perform async read on the ssl stream. The
         * handshake must have been done successfully prior to
         * reading and writing. Note that the async operation
         * will not start until the returned awaitable is
         * awaited. The operation will complete when at least
         * one byte or more is read or an error occurs.
         * @tparam Buffers The type of mutable buffers sequence
         * @param buffers The buffers into which the data will
         * be read. The underlying buffers must be valid until
         * the operation completes.
         * @param ec If set to no_ec then errors are reported
         * via exceptions, otherwise errors are reported via @p
         * ec.
         * @return An awaitable that is when awaited will start
         * the async operation and return the count of bytes
         * transferred.
         */
        template <MutableBufferSequence Buffers>
        read_awaiter<Buffers, false>
        async_read_some(Buffers&& buffers, std::error_code& ec = no_ec);

        /*!
         * @brief Perform async read on the ssl stream. The
         * handshake must have been done successfully prior to
         * reading and writing. Note that the async operation
         * will not start until the returned awaitable is
         * awaited. The operation will complete when all @p
         * buffers are filled or an error occurs.
         * @tparam Buffers Type of mutable buffers sequence
         * @param buffers The buffers into which the data will
         * be read. The underlying buffers must be valid until
         * the operation completes.
         * @param ec If set to no_ec then errors are reported
         * via exceptions, otherwise errors are reported via @p
         * ec.
         * @return An awaitable that is when awaited will start
         * the async operation and return the count of bytes
         * transferred.
         */
        template <MutableBufferSequence Buffers>
        read_awaiter<Buffers, true> async_read(Buffers&& buffers,
                                               std::error_code& ec = no_ec);

        /*!
         * @brief Perform async shutdown on the ssl stream. The
         * handshake must have been done successfully prior to
         * shutdown. Note that the async operation will not
         * start until the returned awaitable is awaited.
         * @param ec If set to no_ec then errors are reported
         * via exceptions, otherwise errors are reported via @p
         * ec.
         * @return An awaitable that is when awaited will start
         * the async operation.
         */
        shutdown_awaiter async_shutdown(std::error_code& ec = no_ec);

        /*!
         * @brief Perform async ssl handshake. The underlying io
         * layer must be ready for data transport prior to
         * performing the handshake.
         * @tparam Handler The type of handler which must
         * satisfy HandShakeHandler
         * @tparam Alloc The type of allocator
         * @param type The type of handshaking, either client or
         * server.
         * @param handler The handler to invoke when the
         * operation is done and will be passed an error_code
         * that determines whether the operation has succeeded
         * or failed.
         * @param alloc An allocator used to allocate the
         * operation. The size passed to its allocate method is
         * in bytes and the alignment of the allocator must be
         * suitable for std::max_align_t
         */
        template <HandShakeHandler Handler,
                  HandlerAllocator Alloc = default_io_allocator>
        void async_handshake(handshake_type type, Handler&& handler,
                             const Alloc& alloc = Alloc()) {
            if (!is_open()) {
                return post(
                    any_ex(),
                    [handler = std::forward<Handler>(handler)] {
                        handler(std::make_error_code(
                            std::errc::bad_file_descriptor));
                    },
                    alloc);
            }
            if (type == handshake_type::client) {
                engine_.set_client_mode();
            }
            else {
                engine_.set_server_mode();
            }
            start_handshake(std::forward<Handler>(handler), alloc);
        }

        /*!
         * @brief Perform async write on the ssl stream. The
         * handshake must have been done successfully prior to
         * reading and writing. The operation will complete when
         * all data in @p buffers is written or an error occurs.
         * @tparam Buffers The type of buffers sequence which
         * must satisfy BufferSequence
         * @tparam Handler The type of handler which must
         * satisfy WriteHandler
         * @tparam Alloc The type of allocator which must
         * satisfy HandlerAllocator
         * @param buffers The data to be written. The underlying
         * buffers must be valid until the operation completes.
         * @param handler The handler to invoke when the
         * operation is done and will be passed an error_code
         * that determines whether the operation has succeeded
         * or failed, and the count of bytes transferred.
         * @param alloc An allocator used to allocate the
         * operation. The size passed to its allocate method is
         * in bytes and the alignment of the allocator must be
         * suitable for std::max_align_t
         */
        template <BufferSequence Buffers, WriteHandler Handler,
                  HandlerAllocator Alloc = default_io_allocator>
        void async_write(Buffers&& buffers, Handler&& handler,
                         const Alloc& alloc = Alloc()) {
            if (!is_open()) {
                return post(
                    any_ex(),
                    [handler = std::forward<Handler>(handler)] {
                        handler(std::make_error_code(
                                    std::errc::bad_file_descriptor),
                                0);
                    },
                    alloc);
            }
            using op_t =
                write_op<std::remove_reference_t<Handler>, Buffers, Alloc>;
            op_t* op = details::allocate_op<op_t>(
                alloc, *this, std::forward<Buffers>(buffers),
                std::forward<Handler>(handler));
            std::error_code ec;
            bool finished = do_write(*op, ec);
            if (finished) {
                details::post_sync_rw(
                    any_ex(),
                    ec ? async_result::failed(ec)
                       : async_result::success(buffers_size(buffers)),
                    op);
            }
        }

        /*!
         * @brief Perform async read on the ssl stream. The
         * handshake must have been done successfully prior to
         * reading and writing. The operation will complete when
         * at least one byte or more is read or an error occurs.
         * @tparam Buffers The type of buffers sequence which
         * must satisfy MutableBufferSequence
         * @tparam Handler The type of handler which must
         * satisfy ReadHandler
         * @tparam Alloc The type of allocator which must
         * satisfy HandlerAllocator
         * @param buffers The buffers into which the data will
         * be read. The underlying buffers must be valid until
         * the operation completes.
         * @param handler The handler to invoke when the
         * operation is done and will be passed an error_code
         * that determines whether the operation has succeeded
         * or failed, and the count of bytes transferred.
         * @param alloc An allocator used to allocate the
         * operation. The size passed to its allocate method is
         * in bytes and the alignment of the allocator must be
         * suitable for std::max_align_t
         */
        template <MutableBufferSequence Buffers, ReadHandler Handler,
                  HandlerAllocator Alloc = default_io_allocator>
        void async_read_some(Buffers&& buffers, Handler&& handler,
                             const Alloc& alloc = Alloc()) {
            async_read_impl<false>(std::forward<Buffers>(buffers),
                                   std::forward<Handler>(handler), alloc);
        }

        /*!
         * @brief Perform async read on the ssl stream. The
         * handshake must have been done successfully prior to
         * reading and writing. The operation will complete when
         * all @p buffers are filled or an error occurs.
         * @tparam Buffers The type of buffers sequence which
         * must satisfy MutableBufferSequence
         * @tparam Handler The type of handler which must
         * satisfy ReadHandler
         * @tparam Alloc The type of allocator which must
         * satisfy HandlerAllocator
         * @param buffers The buffers into which the data will
         * be read. The underlying buffers must be valid until
         * the operation completes.
         * @param handler The handler to invoke when the
         * operation is done and will be passed an error_code
         * that determines whether the operation has succeeded
         * or failed, and the count of bytes transferred.
         * @param alloc An allocator used to allocate the
         * operation. The size passed to its allocate method is
         * in bytes and the alignment of the allocator must be
         * suitable for std::max_align_t
         */
        template <MutableBufferSequence Buffers, ReadHandler Handler,
                  HandlerAllocator Alloc = default_io_allocator>
        void async_read(Buffers&& buffers, Handler&& handler,
                        const Alloc& alloc = Alloc()) {
            async_read_impl<true>(std::forward<Buffers>(buffers),
                                  std::forward<Handler>(handler), alloc);
        }

        /*!
         * @brief Perform async shutdown on the ssl stream. The
         * handshake must have been done successfully prior to
         * shutdown.
         * @tparam Handler The type of handler which must
         * satisfy ShutdownHandler
         * @tparam Alloc The type of allocator which must
         * satisfy HandlerAllocator
         * @param handler The handler to invoke when the
         * operation is done and will be passed an error_code
         * that determines whether the operation has succeeded
         * or failed.
         * @param alloc An allocator used to allocate the
         * operation. The size passed to its allocate method is
         * in bytes and the alignment of the allocator must be
         * suitable for std::max_align_t
         */
        template <ShutdownHandler Handler,
                  HandlerAllocator Alloc = default_io_allocator>
        void async_shutdown(Handler&& handler, const Alloc& alloc = Alloc()) {
            if (!is_open()) {
                return post(
                    any_ex(),
                    [handler = std::forward<Handler>(handler)] {
                        handler(std::make_error_code(
                            std::errc::bad_file_descriptor));
                    },
                    alloc);
            }
            using op_t = shutdown_op<std::remove_reference_t<Handler>, Alloc>;
            op_t* op = details::allocate_op<op_t>(
                alloc, *this, std::forward<Handler>(handler));
            std::error_code ec;
            bool finished = do_shutdown(*op, ec);
            if (finished) {
                details::post_sync_ec(any_ex(),
                                      ec ? async_result::failed(ec)
                                         : async_result::success(0),
                                      op);
            }
        }

    private:
        template <class Handler, class Alloc = default_io_allocator>
        void start_handshake(Handler&& handler, const Alloc& alloc = Alloc()) {
            using op_t = handshake_op<std::remove_reference_t<Handler>, Alloc>;
            op_t* op = details::allocate_op<op_t>(
                alloc, *this, std::forward<Handler>(handler));
            std::error_code ec;
            bool finished = do_handshake(*op, ec);
            if (finished) {
                details::post_sync_ec(any_ex(),
                                      ec ? async_result::failed(ec)
                                         : async_result::success(0),
                                      op);
            }
        }

        template <bool ReadAll, class Buffers, class Handler,
                  class Alloc = default_io_allocator>
        void async_read_impl(Buffers&& buffers, Handler&& handler,
                             const Alloc& alloc = Alloc()) {
            if (!is_open()) {
                return post(
                    any_ex(),
                    [handler = std::forward<Handler>(handler)] {
                        handler(std::make_error_code(
                                    std::errc::bad_file_descriptor),
                                0);
                    },
                    alloc);
            }
            using op_t = read_op<std::remove_reference_t<Handler>, Buffers,
                                 Alloc, ReadAll>;
            op_t* op = details::allocate_op<op_t>(
                alloc, *this, std::forward<Buffers>(buffers),
                std::forward<Handler>(handler));
            std::error_code ec;
            bool finished = do_read(*op, ec);
            if (finished) {
                std::size_t transferred =
                    op->buffers.size() - op->buffers.current_size();
                details::post_sync_rw(any_ex(),
                                      ec ? async_result::failed(ec, transferred)
                                         : async_result::success(transferred),
                                      op);
            }
        }

        // returns true if finished with either success or error
        bool do_handshake(handshake_op_base& op, std::error_code& ec) noexcept;

        void process_handshake(handshake_op_base& op);

        // returns true if finished with either success or error
        bool do_write(write_op_base& op, std::error_code& ec) noexcept;

        void process_write(write_op_base& op, std::size_t transferred);

        // returns true if finished with either success or error
        bool do_read(read_op_base& op, std::error_code& ec) noexcept;

        void process_read(read_op_base& op, std::size_t transferred);

        // returns true if finished with either success or error
        bool do_shutdown(shutdown_op_base& op, std::error_code& ec) noexcept;

        void process_shutdown(shutdown_op_base& op);

        bool transfer_output_buffers(write_op_base& op,
                                     std::error_code& ec) noexcept;

        bool read_into_input_buffers(read_op_base& op,
                                     std::error_code& ec) noexcept;

        any_executor& any_ex() noexcept {
            return executor().as_any_executor();
        }

        void make_error_if_closed(std::error_code& ec, const char* msg) {
            if (is_open()) {
                return;
            }
            if (use_exceptions(ec)) {
                throw std::system_error{ec, msg};
            }
            else {
                ec = std::make_error_code(std::errc::bad_file_descriptor);
            }
        }

        next_layer_type next_layer_;
        engine engine_;
    };

    template <class NextLayer>
    struct stream<NextLayer>::handshake_op_base {
        ref<stream> s_;
        std::array<std::uint8_t, max_next_layer_alloc_size> allocator_buff;

        handshake_op_base(stream& s) noexcept : s_{s} {
        }

        auto make_write_handler() {
            return [this](const std::error_code& ec, std::size_t transferred) {
                if (ec) {
                    complete(ec);
                }
                else {
                    s_->engine_.consume_output_buffers();
                    s_->process_handshake(*this);
                }
            };
        }

        auto make_read_handler() {
            return [this](const std::error_code& ec, std::size_t transferred) {
                if (ec) {
                    complete(ec);
                }
                else {
                    s_->engine_.commit_input_buffers(transferred);
                    s_->process_handshake(*this);
                }
            };
        }

        virtual void complete(const std::error_code& ec) = 0;

    protected:
        ~handshake_op_base() = default;
    };

    template <class NextLayer>
    struct stream<NextLayer>::write_op_base {
        ref<stream> s_;
        std::array<std::uint8_t, max_next_layer_alloc_size> allocator_buff;

        write_op_base(stream& s) noexcept : s_{s} {
        }

        auto make_handler() {
            return [this](const std::error_code& ec, std::size_t transferred) {
                if (ec) {
                    complete(ec);
                }
                else {
                    s_->process_write(*this, transferred);
                }
            };
        }

        void redo_write() {
            std::error_code ec;
            bool finished = s_->do_write(*this, ec);
            if (finished) {
                complete(ec);
            }
        }

        virtual const_buffer get_buffer() noexcept = 0;

        virtual const_buffer advance_buffers(std::size_t n) noexcept = 0;

        virtual void complete(const std::error_code& ec) = 0;

    protected:
        ~write_op_base() = default;
    };

    template <class NextLayer>
    struct stream<NextLayer>::read_op_base {
        ref<stream> s_;
        std::array<std::uint8_t, max_next_layer_alloc_size> allocator_buff;

        read_op_base(stream& s) noexcept : s_{s} {
        }

        auto make_handler() {
            return [this](const std::error_code& ec, std::size_t transferred) {
                if (ec) {
                    complete(ec);
                }
                else {
                    s_->process_read(*this, transferred);
                }
            };
        }

        void redo_read() {
            std::error_code ec;
            bool finished = s_->do_read(*this, ec);
            if (finished) {
                complete(ec);
            }
        }

        virtual mutable_buffer get_buffer() noexcept = 0;

        virtual mutable_buffer advance_buffers(std::size_t n) noexcept = 0;

        virtual bool finished() const noexcept = 0;

        virtual void complete(const std::error_code& ec) = 0;

    protected:
        ~read_op_base() = default;
    };

    template <class NextLayer>
    struct stream<NextLayer>::shutdown_op_base {
        ref<stream> s_;
        std::array<std::uint8_t, max_next_layer_alloc_size> allocator_buff;

        shutdown_op_base(stream& s) noexcept : s_{s} {
        }

        auto make_write_handler() {
            return [this](const std::error_code& ec, std::size_t transferred) {
                if (ec) {
                    complete(ec);
                }
                else {
                    s_->engine_.consume_output_buffers();
                    s_->process_shutdown(*this);
                }
            };
        }

        auto make_read_handler() {
            return [this](const std::error_code& ec, std::size_t transferred) {
                if (ec) {
                    complete(ec);
                }
                else {
                    s_->engine_.commit_input_buffers(transferred);
                    s_->process_shutdown(*this);
                }
            };
        }

        virtual void complete(const std::error_code& ec) = 0;

    protected:
        ~shutdown_op_base() = default;
    };

    template <class NextLayer>
    class [[nodiscard]] stream<NextLayer>::handshake_awaiter final
        : public stream<NextLayer>::handshake_op_base,
          error_storage {
    public:
        handshake_awaiter(stream& s, std::error_code& ec) noexcept
            : stream<NextLayer>::handshake_op_base(s), error_storage(ec) {
        }

        bool await_ready() const noexcept {
            return has_error();
        }

        bool await_suspend(std::coroutine_handle<> coro) {
            waiter_ = coro;
            std::error_code ec;
            bool finished =
                stream<NextLayer>::handshake_op_base::s_->do_handshake(*this,
                                                                       ec);
            if (finished) {
                store(ec);
                raise("handshake");
            }
            return !finished;
        }

        void await_resume() {
            raise("handshake");
        }

        void complete(const std::error_code& ec) override {
            store(ec);
            waiter_.resume();
        }

    private:
        std::coroutine_handle<> waiter_;
    };

    template <class NextLayer>
    template <class Buffers>
    class [[nodiscard]] stream<NextLayer>::write_awaiter final
        : public stream<NextLayer>::write_op_base,
          error_storage {
        using buffers_t = std::remove_cvref_t<Buffers>;

    public:
        template <class B>
        write_awaiter(stream& s, B&& buffers, std::error_code& ec) noexcept
            : stream<NextLayer>::write_op_base(s), error_storage(ec),
              buffers_{std::forward<B>(buffers)} {
        }

        bool await_ready() const noexcept {
            return has_error();
        }

        bool await_suspend(std::coroutine_handle<> coro) {
            waiter_ = coro;
            std::error_code ec;
            bool finished =
                stream<NextLayer>::write_op_base::s_->do_write(*this, ec);
            if (finished) {
                store(ec);
                raise("write");
            }
            assert(!finished || has_error()); // the write will finish
                                              // in sync mode only if it
                                              // failed for now
            return !finished;
        }

        std::size_t await_resume() {
            raise("write");
            return has_error() ? 0 : buffers_.size();
        }

        void complete(const std::error_code& ec) override {
            store(ec);
            waiter_.resume();
        }

        virtual const_buffer get_buffer() noexcept override {
            return buffers_.finished() ? buffer(nullptr)
                                       : *buffers_.current_buffer();
        }

        virtual const_buffer advance_buffers(std::size_t n) noexcept override {
            buffers_.advance(n);
            return buffers_.finished() ? buffer(nullptr)
                                       : *buffers_.current_buffer();
        }

    private:
        std::coroutine_handle<> waiter_;
        buffers_range_adapter<buffers_t> buffers_;
    };

    template <class NextLayer>
    template <class Buffers, bool ReadAll>
    class [[nodiscard]] stream<NextLayer>::read_awaiter final
        : public stream<NextLayer>::read_op_base,
          error_storage {
        using buffers_t = std::remove_cvref_t<Buffers>;

    public:
        template <class B>
        read_awaiter(stream& s, B&& buffers, std::error_code& ec) noexcept
            : stream<NextLayer>::read_op_base(s), error_storage(ec),
              buffers_(std::forward<B>(buffers)) {
        }

        bool await_ready() const noexcept {
            return has_error();
        }

        bool await_suspend(std::coroutine_handle<> coro) {
            waiter_ = coro;
            std::error_code ec;
            bool finished =
                stream<NextLayer>::read_op_base::s_->do_read(*this, ec);
            if (finished) {
                store(ec);
                raise("read");
            }
            return !finished;
        }

        std::size_t await_resume() {
            raise("read");
            return buffers_.size() - buffers_.current_size();
        }

        virtual mutable_buffer get_buffer() noexcept override {
            return buffers_.finished() ? buffer(nullptr)
                                       : *buffers_.current_buffer();
        }

        virtual mutable_buffer
        advance_buffers(std::size_t n) noexcept override {
            buffers_.advance(n);
            return buffers_.finished() ? buffer(nullptr)
                                       : *buffers_.current_buffer();
        }

        virtual bool finished() const noexcept override {
            if constexpr (ReadAll) {
                return !buffers_.current_size();
            }
            else {
                return buffers_.current_size() != buffers_.size();
            }
        }

        virtual void complete(const std::error_code& ec) override {
            store(ec);
            waiter_.resume();
        }

    private:
        std::coroutine_handle<> waiter_;
        buffers_range_adapter<buffers_t> buffers_;
    };

    template <class NextLayer>
    class [[nodiscard]] stream<NextLayer>::shutdown_awaiter final
        : public stream<NextLayer>::shutdown_op_base,
          error_storage {
    public:
        shutdown_awaiter(stream& s, std::error_code& ec) noexcept
            : stream<NextLayer>::shutdown_op_base(s), error_storage(ec) {
        }

        bool await_ready() const noexcept {
            return has_error();
        }

        bool await_suspend(std::coroutine_handle<> coro) {
            waiter_ = coro;
            std::error_code ec;
            bool finished =
                stream<NextLayer>::shutdown_op_base::s_->do_shutdown(*this, ec);
            if (finished) {
                store(ec);
                raise("shutdown");
            }
            return !finished;
        }

        void await_resume() {
            raise("shutdown");
        }

        void complete(const std::error_code& ec) override {
            store(ec);
            waiter_.resume();
        }

    private:
        std::coroutine_handle<> waiter_;
    };

    template <class NextLayer>
    template <class Handler, class Alloc>
    struct stream<NextLayer>::handshake_op final
        : public stream<NextLayer>::handshake_op_base,
          public allocator_storage<Alloc> {
        using alloc_base = allocator_storage<Alloc>;

        Handler handler;

        template <class H>
        handshake_op(stream& s, H&& handler, const Alloc& alloc)
            : stream<NextLayer>::handshake_op_base(s), alloc_base(alloc),
              handler{std::forward<H>(handler)} {
        }

        void complete(const std::error_code& ec) override {
            details::invoke_handler(this, ec);
        }
    };

    template <class NextLayer>
    template <class Handler, class Buffers, class Alloc>
    struct stream<NextLayer>::write_op final
        : public stream<NextLayer>::write_op_base,
          public allocator_storage<Alloc> {
        using alloc_base = allocator_storage<Alloc>;
        using buffers_t = std::remove_cvref_t<Buffers>;

        Handler handler;
        buffers_range_adapter<buffers_t> buffers;

        template <class B, class H>
        write_op(stream& s, B&& buffers, H&& handler,
                 const Alloc& alloc) noexcept
            : stream<NextLayer>::write_op_base(s), alloc_base(alloc),
              handler{std::forward<H>(handler)},
              buffers{std::forward<B>(buffers)} {
        }

        virtual const_buffer get_buffer() noexcept override {
            return buffers.finished() ? buffer(nullptr)
                                      : *buffers.current_buffer();
        }

        virtual const_buffer advance_buffers(std::size_t n) noexcept override {
            buffers.advance(n);
            return buffers.finished() ? buffer(nullptr)
                                      : *buffers.current_buffer();
        }

        virtual void complete(const std::error_code& ec) override {
            details::invoke_handler(this, ec, ec ? 0 : buffers.size());
        }
    };

    template <class NextLayer>
    template <class Handler, class Buffers, class Alloc, bool ReadAll>
    struct stream<NextLayer>::read_op final
        : public stream<NextLayer>::read_op_base,
          public allocator_storage<Alloc> {
        using alloc_base = allocator_storage<Alloc>;
        using buffers_t = std::remove_cvref_t<Buffers>;

        Handler handler;
        buffers_range_adapter<buffers_t> buffers;

        template <class B, class H>
        read_op(stream& s, B&& buffers, H&& handler, const Alloc& alloc)
            : stream<NextLayer>::read_op_base(s), alloc_base(alloc),
              handler{std::forward<H>(handler)},
              buffers{std::forward<B>(buffers)} {
        }

        virtual mutable_buffer get_buffer() noexcept override {
            return buffers.finished() ? buffer(nullptr)
                                      : *buffers.current_buffer();
        }

        virtual mutable_buffer
        advance_buffers(std::size_t n) noexcept override {
            buffers.advance(n);
            return buffers.finished() ? buffer(nullptr)
                                      : *buffers.current_buffer();
        }

        virtual bool finished() const noexcept override {
            std::size_t current_size = buffers.current_size();
            std::size_t total_size = buffers.size();
            if constexpr (ReadAll) {
                return !current_size;
            }
            else {
                return current_size < total_size;
            }
        }

        virtual void complete(const std::error_code& ec) override {
            std::size_t total_transferred =
                buffers.size() - buffers.current_size();
            details::invoke_handler(this, ec, total_transferred);
        }
    };

    template <class NextLayer>
    template <class Handler, class Alloc>
    struct stream<NextLayer>::shutdown_op final
        : public stream<NextLayer>::shutdown_op_base,
          public allocator_storage<Alloc> {
        using alloc_base = allocator_storage<Alloc>;

        Handler handler;

        template <class H>
        shutdown_op(stream& s, H&& handler, const Alloc& alloc)
            : stream<NextLayer>::shutdown_op_base(s), alloc_base(alloc),
              handler{std::forward<H>(handler)} {
        }

        void complete(const std::error_code& ec) override {
            details::invoke_handler(this, ec);
        }
    };

    template <class NextLayer>
    auto stream<NextLayer>::async_handshake(handshake_type type,
                                            std::error_code& ec)
        -> handshake_awaiter {
        make_error_if_closed(ec, "handshake");
        if (is_open()) {
            if (type == handshake_type::client) {
                engine_.set_client_mode();
            }
            else {
                engine_.set_server_mode();
            }
        }
        return handshake_awaiter{*this, ec};
    }

    template <class NextLayer>
    template <BufferSequence Buffers>
    auto stream<NextLayer>::async_write(Buffers&& buffers, std::error_code& ec)
        -> write_awaiter<Buffers> {
        make_error_if_closed(ec, "write");
        return write_awaiter<Buffers>{*this, std::forward<Buffers>(buffers),
                                      ec};
    }

    template <class NextLayer>
    template <MutableBufferSequence Buffers>
    auto stream<NextLayer>::async_read_some(Buffers&& buffers,
                                            std::error_code& ec)
        -> read_awaiter<Buffers, false> {
        make_error_if_closed(ec, "read");
        return read_awaiter<Buffers, false>{*this,
                                            std::forward<Buffers>(buffers), ec};
    }

    template <class NextLayer>
    template <MutableBufferSequence Buffers>
    auto stream<NextLayer>::async_read(Buffers&& buffers, std::error_code& ec)
        -> read_awaiter<Buffers, true> {
        make_error_if_closed(ec, "read");
        return read_awaiter<Buffers, true>{*this,
                                           std::forward<Buffers>(buffers), ec};
    }

    template <class NextLayer>
    auto stream<NextLayer>::async_shutdown(std::error_code& ec)
        -> shutdown_awaiter {
        make_error_if_closed(ec, "shutdown");
        return shutdown_awaiter{*this, ec};
    }

    namespace detail {
        inline bool should_retry(engine_state s) noexcept {
            return s == engine_state::want_read ||
                   s == engine_state::want_write;
        }
    } // namespace detail

    template <class NextLayer>
    bool stream<NextLayer>::do_handshake(handshake_op_base& op,
                                         std::error_code& ec) noexcept {
        // check if the ssl stream was closed
        if (!is_open()) {
            ec = std::make_error_code(std::errc::operation_canceled);
            return true;
        }
        engine_state state = engine_state::done;
        engine_.handshake(state, ec);
        // If process completes with no error, there may be data in the
        // output buffer to send
        auto out_buffs = engine_.available_output_buffers();
        bool has_out_buffs = !out_buffs[0].empty() || !out_buffs[1].empty();
        // if completed with no error and the output buffers are empty
        // or failed with non io error, then abort
        if ((!ec && !has_out_buffs) || !detail::should_retry(state)) {
            return true;
        }
        // clear the ec if it indicates want read or write
        ec.clear();
        // the engine may put some data in the output buffers and
        // request to read data but the remote peer will not send any
        // data unless it receives the data in engine's out buffer so
        // check if there is any data in out buffers and transfer it
        // before retrying
        if (has_out_buffs) {
            next_layer().async_write(
                out_buffs, op.make_write_handler(),
                static_buffer_allocator(op.allocator_buff));
        }
        else if (state == engine_state::want_read) {
            auto in_buffs = engine_.available_input_buffers();
            bool both_empty = in_buffs[0].empty() && in_buffs[1].empty();
            assert(!both_empty && "the ssl engine requested to read data "
                                  "but the read buffer is full");
            if (both_empty) {
                ec = std::make_error_code(std::errc::protocol_error);
                return true;
            }
            next_layer().async_read_some(
                in_buffs, op.make_read_handler(),
                static_buffer_allocator(op.allocator_buff));
        }
        else if (state == engine_state::done) {
            return true;
        }
        return false;
    }

    template <class NextLayer>
    void stream<NextLayer>::process_handshake(handshake_op_base& op) {
        std::error_code ec;
        bool finished = do_handshake(op, ec);
        if (finished) {
            op.complete(ec);
        }
    }

    template <class NextLayer>
    bool
    stream<NextLayer>::transfer_output_buffers(write_op_base& op,
                                               std::error_code& ec) noexcept {
        ec.clear();
        auto out_buffs = engine_.available_output_buffers();
        bool both_empty = out_buffs[0].empty() && out_buffs[1].empty();
        assert(!both_empty && "the ssl engine requested to transfer the output "
                              "buffers but they are empty");
        if (both_empty) {
            ec = std::make_error_code(std::errc::protocol_error);
            return true;
        }
        next_layer().async_write(out_buffs, op.make_handler(),
                                 static_buffer_allocator(op.allocator_buff));
        return false;
    }

    template <class NextLayer>
    bool
    stream<NextLayer>::read_into_input_buffers(read_op_base& op,
                                               std::error_code& ec) noexcept {
        ec.clear();
        auto in_buffs = engine_.available_input_buffers();
        bool both_empty = in_buffs[0].empty() && in_buffs[1].empty();
        assert(!both_empty && "the ssl engine requested to read into input "
                              "buffers but they are full");
        if (both_empty) {
            ec = std::make_error_code(std::errc::protocol_error);
            return true;
        }
        next_layer().async_read_some(
            in_buffs, op.make_handler(),
            static_buffer_allocator(op.allocator_buff));
        return false;
    }

    template <class NextLayer>
    bool stream<NextLayer>::do_write(write_op_base& op,
                                     std::error_code& ec) noexcept {
        // check if the ssl stream was closed
        if (!is_open()) {
            ec = std::make_error_code(std::errc::operation_canceled);
            return true;
        }
        auto buff = op.get_buffer();
        if (buff.empty()) {
            return true;
        }

        ec.clear();
        engine_state state = engine_state::done;
        while (!buff.empty()) {
            std::size_t n = std::min(buff.size(), engine::max_tls_record_size);
            buff = buff.sub_buffer(0, n);
            n = engine_.put_output(buff, state, ec);
            if (n) {
                buff = op.advance_buffers(n);
            }
            else {
                assert(!!ec);
            }
            if (!n || ec) {
                break;
            }
        }

        // if failed with error other than want_read and want_write then
        // fail the operation even if the buffer was fully encrypted and
        // written
        if (ec && !detail::should_retry(state)) {
            return true;
        }

        // if the buffer was fully encrypted and written and there is no
        // error or error is want_read or want_write then ignore the
        // error for now because the current operation is considered
        // complete now transfer the output buffers
        if (buff.empty()) {
            return transfer_output_buffers(op, ec);
        }

        assert(ec && detail::should_retry(state) &&
               "the ssl engine didn't write the whole buffer and "
               "didn't "
               "request "
               "more data");

        if (!ec || !detail::should_retry(state)) {
            ec = std::make_error_code(std::errc::protocol_error);
            return true;
        }

        // if the engine wants to read in order to complete write then
        // it has encountered a renegotiation should this be handled as
        // error since renegotiation should have been disabled?
        if (state == engine_state::want_read) {
            ec.clear();
            start_handshake([&op = op](const std::error_code& ec) {
                if (!ec) {
                    op.redo_write();
                }
                else {
                    op.complete(ec);
                }
            });
            return false;
        }
        // consume the output buffers to free space for more data to
        // write
        assert(state == engine_state::want_write ||
               state == engine_state::done);
        return transfer_output_buffers(op, ec);
    }

    template <class NextLayer>
    void stream<NextLayer>::process_write(write_op_base& op,
                                          std::size_t transferred) {
        std::ignore = transferred;
        engine_.consume_output_buffers();
        std::error_code ec;
        if (do_write(op, ec)) {
            op.complete(ec);
        }
    }

    template <class NextLayer>
    bool stream<NextLayer>::do_read(read_op_base& op,
                                    std::error_code& ec) noexcept {
        // check if the ssl stream was closed
        if (!is_open()) {
            ec = std::make_error_code(std::errc::operation_canceled);
            return true;
        }
        auto buff = op.get_buffer();
        if (buff.empty()) {
            return true;
        }

        ec.clear();
        engine_state state = engine_state::done;
        while (!buff.empty()) {
            std::size_t n = engine_.get_input(buff, state, ec);
            if (n) {
                buff = op.advance_buffers(n);
            }
            else {
                assert(!!ec);
            }
            if (!n || ec) {
                break;
            }
        }

        const auto eof_ec = io::detail::make_eof_error_code();

        // if failed with error other than want_read and want_write then
        // fail the operation even if the buffer was fully filled
        // if eof is reached and some data was read and not all buffers
        // are required to be filled then assume success and return
        // the eof the next read
        if (ec && ec != eof_ec && !detail::should_retry(state)) {
            return true;
        }

        // if the operation is statisfied and there is no error or error
        // is want_read or want_write then ignore the error for now
        // because the current operation is considered complete the
        // operation is considered statisfied if all buffers have been
        // filled or the operation is not a read all and at least 1 byte
        // was transferred into the buffer
        if (op.finished()) {
            ec.clear();
            return true;
        }

        // unexpected eof since op is not finished yet
        if (ec == eof_ec) {
            return true;
        }

        // if the engine consumed all data but the buffers are not
        // filled entirely the engine may report state as done ?
        if (state == engine_state::done) {
            state = engine_state::want_read;
        }

        assert(detail::should_retry(state) &&
               "the ssl engine didn't fill the buffer and didn't "
               "request more "
               "data");

        if (!detail::should_retry(state)) {
            ec = std::make_error_code(std::errc::protocol_error);
            return true;
        }

        // if the engine wants to write in order to complete read then
        // it has encountered a renegotiation should this be handled as
        // error since renegotiation should have been disabled?
        if (state == engine_state::want_write) {
            ec.clear();
            start_handshake([&op = op](const std::error_code& ec) {
                if (!ec) {
                    op.redo_read();
                }
                else {
                    op.complete(ec);
                }
            });
            return false;
        }

        // read more data into the input buffers to try to decrypt and
        // read later
        assert(state == engine_state::want_read);
        return read_into_input_buffers(op, ec);
    }

    template <class NextLayer>
    void stream<NextLayer>::process_read(read_op_base& op,
                                         std::size_t transferred) {
        engine_.commit_input_buffers(transferred);
        std::error_code ec;
        if (do_read(op, ec)) {
            op.complete(ec);
        }
    }

    template <class NextLayer>
    bool stream<NextLayer>::do_shutdown(shutdown_op_base& op,
                                        std::error_code& ec) noexcept {
        // check if the ssl stream was closed
        if (!is_open()) {
            ec = std::make_error_code(std::errc::operation_canceled);
            return true;
        }
        engine_state state = engine_state::done;
        engine_.shutdown(state, ec);
        // If process completes with no error, there may be data in the
        // output buffer to send
        auto out_buffs = engine_.available_output_buffers();
        bool has_out_buffs = !out_buffs[0].empty() || !out_buffs[1].empty();
        // if completed with no error and the output buffers are empty
        // or failed with non io error, then abort
        if ((!ec && !has_out_buffs) || !detail::should_retry(state)) {
            return true;
        }
        // clear the ec if it indicates want read or write
        ec.clear();
        // the engine may put some data in the output buffers and
        // request to read data but the remote peer will not send any
        // data unless it receives the data in engine's out buffer so
        // check if there is any data in out buffers and transfer it
        // before retrying
        if (has_out_buffs) {
            next_layer().async_write(
                out_buffs, op.make_write_handler(),
                static_buffer_allocator(op.allocator_buff));
        }
        else if (state == engine_state::want_read) {
            auto in_buffs = engine_.available_input_buffers();
            bool both_empty = in_buffs[0].empty() && in_buffs[1].empty();
            assert(!both_empty && "the ssl engine requested to read data "
                                  "but the read buffer is full");
            if (both_empty) {
                ec = std::make_error_code(std::errc::protocol_error);
                return true;
            }
            next_layer().async_read(in_buffs, op.make_read_handler(),
                                    static_buffer_allocator(op.allocator_buff));
        }
        else if (state == engine_state::done) {
            return true;
        }
        return false;
    }

    template <class NextLayer>
    void stream<NextLayer>::process_shutdown(shutdown_op_base& op) {
        std::error_code ec;
        bool finished = do_shutdown(op, ec);
        if (finished) {
            op.complete(ec);
        }
    }
}; // namespace RAD_LIB_NAMESPACE::net::ssl