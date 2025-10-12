#pragma once
#include <rad/io/windows/async_file_impl.h>
#include <rad/ipc/pipe_endpoint.h>

namespace RAD_LIB_NAMESPACE::pipe {

    namespace details = RAD_LIB_NAMESPACE::detail;

    /*!
     * @brief The handler passed to async_accept() method must satisfy the
     * requirements of this concept. The following expression must be valid:
     * handler(std::error_code{})
     */
    template <class Handler>
    concept PipeAcceptHandler =
        requires(Handler handler) { handler(std::error_code{}); };

    /*!
     * @brief Async pipe that wraps named pipe (on Windows)
     */
    class async_pipe : public trackable {
        class accept_awaiter;

        template <class Handler, class Alloc>
        struct accept_op;

        friend class accept_awaiter;

        template <typename, typename>
        friend struct accept_op;

        using implementation_type = io::detail::async_file_impl;

    public:
        /*!
         * @brief The type of executor used by the pipe. This is
         * typically io_executor.
         */
        using executor_type = implementation_type::executor_type;
        /*!
         * @brief The type of lowest io layer. This is the type
         * of pipe itself.
         */
        using lowest_layer_type = async_pipe;
        /*!
         * @brief The type of pipe native handle.
         */
        using native_handle_type = implementation_type::native_handle_type;
        /*!
         * @brief The type of pipe native os handle.
         */
        using native_fd_type = implementation_type::native_fd_type;

        template <class AllocatorTypes>
        static constexpr std::size_t accept_allocator_size() noexcept {
            if constexpr (!(AllocatorTypes::op_type &
                            op_alloc_type::accept_pipe)) {
                return 0;
            }
            else {
                using handler = handler_allocator_size_calculator<
                    AllocatorTypes::max_handler_size>;
                return sizeof(accept_op<handler, stateful_null_allocator>);
            }
        }

        template <class AllocatorTypes>
        static constexpr std::size_t max_allocator_size() noexcept {
            constexpr std::size_t sizes[] = {
                accept_allocator_size<AllocatorTypes>(),
                implementation_type::max_allocator_size<AllocatorTypes>(),
            };
            return max_of(sizes);
        }

        /*!
         * @brief Construct an async named pipe and attach it to
         * an executor. The pipe is not yet open after
         * construction.
         * @param ex The io executor to attach the pipe to.
         */
        async_pipe(executor_type& ex) noexcept : impl_{ex} {
        }

        /*!
         * @brief  @brief Construct and open an async named pipe
         * and attach it to an executor.
         * @param ex The io executor to attach the pipe to.
         * @param epoint The endpoint that contains pipe name
         * and options used to open the pipe.
         */
        async_pipe(executor_type& ex, const endpoint& epoint) : impl_{ex} {
            open(epoint);
        }

        async_pipe(async_pipe&&) = default;

        async_pipe& operator=(async_pipe&&) = default;

        /*!
         * @brief Destroy the pipe.
         * Note that is undefined behavior to destroy the pipe
         * while it has an outstanding async operation.
         */
        ~async_pipe() = default;

        /*!
         * @brief Get a reference to the lowset io layer.
         * A reference to the lowset io layer. This is a
         * reference to the pipe itself.
         */
        lowest_layer_type& lowest_layer() noexcept {
            return *this;
        }

        /*!
         * @brief Get a reference to the lowset io layer.
         * @return A reference to the lowset io layer. This is a
         * reference to the pipe itself.
         */
        const lowest_layer_type& lowest_layer() const noexcept {
            return *this;
        }

        /*!
         * @brief Get a reference to the pipe native handle.
         * @return A reference to the pipe native handle.
         */
        native_handle_type& native_handle() noexcept {
            return impl_.native_handle();
        }

        /*!
         * @brief Get a reference to the pipe native handle.
         * @return A reference to the pipe native handle.
         */
        const native_handle_type& native_handle() const noexcept {
            return impl_.native_handle();
        }

        /*!
         * @brief Get a reference to the pipe native os handle.
         * @return A reference to the pipe native os handle.
         */
        native_fd_type native_fd() const noexcept {
            return native_handle().get();
        }

        /*!
         * @brief Get a reference to the pipe name.
         * The returned name doesn't have the pipe path prefix
         * '\\\\.\\pipe\\' on Windows.
         * @return A reference to the pipe name.
         */
        std::string& name() noexcept {
            return name_utf8_;
        }

        /*!
         * @brief Get a reference to the pipe name.
         * The returned name doesn't have the pipe path prefix
         * '\\\\.\\pipe\\' on Windows.
         * @return A reference to the pipe name.
         */
        const std::string& name() const noexcept {
            return name_utf8_;
        }

        /*!
         * @brief Get a reference to the io executor used by the
         * pipe.
         * @return A reference to the io executor used by the
         * pipe.
         */
        executor_type& executor() noexcept {
            return impl_.executor();
        }

        /*!
         * @brief Get a reference to the io executor used by the
         * pipe.
         * @return A reference to the io executor used by the
         * pipe.
         */
        const executor_type& executor() const noexcept {
            return impl_.executor();
        }

        /*!
         * @brief Check if the pipe is open.
         * @return True if the pipe is open, and false if
         * closed.
         */
        bool is_open() const noexcept {
            return impl_.is_open();
        }

        /*!
         * @brief Check if the pipe is open.
         * @return True if the pipe is open, and false if
         * closed.
         */
        explicit operator bool() const noexcept {
            return is_open();
        }

        /*!
         * @brief Open (create) a named pipe. After open the
         * pipe is not yet connect and can not be used to read
         * and write.
         * @param epoint The endpoint that contains pipe name
         * and options used to open the pipe.
         * @param ec Cleared on success, and set to error on
         * failure.
         */
        RAD_EXPORT_DECL void open(const endpoint& epoint,
                                  std::error_code& ec) noexcept;

        /*!
         * @brief Open (create) a named pipe. After open the
         * pipe is not yet connect and can not be used to read
         * and write.
         * @param epoint The endpoint that contains pipe name
         * and options used to open the pipe.
         * @throws On failure 'std::system_error' is thrown.
         */
        void open(const endpoint& epoint) {
            std::error_code ec;
            open(epoint, ec);
            check_and_throw(ec, __func__);
        }

        /*!
         * @brief Close the pipe if it is open.
         */
        void close() noexcept {
            impl_.close();
        }

        /*!
         * @brief Cancel any pending io operations on the pipe.
         */
        void cancel() noexcept {
            impl_.cancel();
        }

        /*!
         * @brief Perform async accept on the pipe. After
         * success completion the pipe is connected to a remote
         * end and can be used to read and write data. Note that
         * the async operation will not start until the returned
         * awaitable is awaited.
         * @param ec If set to no_ec then errors are reported
         * via exceptions, otherwise errors are reported via @p
         * ec.
         * @return An awaitable that is when awaited will start
         * the async operation.
         */
        accept_awaiter async_accept(std::error_code& ec = no_ec);

        /*!
         * @brief Perform async write on the pipe. The pipe must
         * have been connected by either accept or connect prior
         * to reading and writing. Note that the async operation
         * will not start until the returned awaitable is
         * awaited. The operation will complete when all data in
         * @p buff is written or an error occurs.
         * @param buff The data to be written. The underlying
         * buffers must be valid until the operation completes.
         * @param ec If set to no_ec then errors are reported
         * via exceptions, otherwise errors are reported via @p
         * ec.
         * @return An awaitable that is when awaited will start
         * the async operation and return the count of bytes
         * transferred.
         */
        auto async_write(const_buffer buff, std::error_code& ec = no_ec) {
            return impl_.async_write(buff, ec);
        }

        /*!
         * @brief Perform async read on the pipe. The pipe must
         * have been connected by either accept or connect prior
         * to reading and writing. Note that the async operation
         * will not start until the returned awaitable is
         * awaited. The operation will complete when at least
         * one byte or more is read or an error occurs.
         * @param buff The buffer into which the data will be
         * read.
         * @param ec If set to no_ec then errors are reported
         * via exceptions, otherwise errors are reported via @p
         * ec.
         * @return An awaitable that is when awaited will start
         * the async operation and return the count of bytes
         * transferred.
         */
        auto async_read_some(mutable_buffer buff, std::error_code& ec = no_ec) {
            return impl_.async_read(buff, ec);
        }

        /*!
         * @brief Perform async read on the pipe. The pipe must
         * have been connected by either accept or connect prior
         * to reading and writing. Note that the async operation
         * will not start until the returned awaitable is
         * awaited. The operation will complete when all
         * supplied buffers are filled or an error occurs.
         * @param buff The buffer into which the data will be
         * read.
         * @param ec If set to no_ec then errors are reported
         * via exceptions, otherwise errors are reported via @p
         * ec.
         * @return An awaitable that is when awaited will start
         * the async operation and return the count of bytes
         * transferred.
         */
        auto async_read(mutable_buffer buff, std::error_code& ec = no_ec) {
            return impl_.async_read_all(buff, ec);
        }

        /*!
         * @brief Connect the pipe to a remote endpoint. Connect
         * is done in blocking mode. After connect the pipe can
         * be used to read and write data.
         * @param epoint The endpoint that contains remote end
         * name and options used to connect the pipe.
         * @param ec Cleared on success, and set to error on
         * failure.
         */
        RAD_EXPORT_DECL void connect(const endpoint& epoint,
                                     std::error_code& ec) noexcept;

        /*!
         * @brief Connect the pipe to a remote endpoint. Connect
         * is done in blocking mode. After connect the pipe can
         * be used to read and write data.
         * @param epoint The endpoint that contains remote end
         * name and options used to connect the pipe.
         * @throws On failure 'std::system_error' is thrown.
         */
        void connect(const endpoint& epoint) {
            std::error_code ec;
            connect(epoint, ec);
            check_and_throw(ec, __func__);
        }

        /*!
         * @brief Disconnect the pipe from its remote endpoint.
         * @param ec Cleared on success, and set to error on
         * failure.
         */
        RAD_EXPORT_DECL void disconnect(std::error_code& ec) noexcept;

        /*!
         * @brief Disconnect the pipe from its remote endpoint.
         * @throws On failure 'std::system_error' is thrown.
         */
        void disconnect() {
            std::error_code ec;
            disconnect(ec);
            check_and_throw(ec, __func__);
        }

        /*!
         * @brief Perform blocking accept on the pipe. After
         * success completion the pipe is connected to a remote
         * end and can be used to read and write data.
         * @param ec Cleared on success, and set to error on
         * failure.
         */
        RAD_EXPORT_DECL void accept(std::error_code& ec) noexcept;

        /*!
         * @brief Perform blocking accept on the pipe. After
         * success completion the pipe is connected to a remote
         * end and can be used to read and write data.
         * @throws On failure 'std::system_error' is thrown.
         */
        void accept() {
            std::error_code ec;
            accept(ec);
            check_and_throw(ec, __func__);
        }

        /*!
         * @brief Perform async accept on the pipe. After
         * success completion the pipe is connected to a remote
         * end and can be used to read and write data.
         * @tparam Handler The type of handler which must
         * satisfy PipeAcceptHandler
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
        template <PipeAcceptHandler Handler,
                  HandlerAllocator Alloc = default_io_allocator>
        void async_accept(Handler&& handler, const Alloc& alloc = Alloc()) {
            using op_t = accept_op<Handler, Alloc>;
            op_t* op = details::allocate_op<op_t>(
                alloc, *this, std::forward<Handler>(handler));
            auto result = do_async_accept(*op);
            if (!result.is_pending()) {
                details::post_sync_ec(get_any_exuector(), result, op);
            }
        }

        /*!
         * @brief Write a buffer of data to the pipe. Write is
         * done in blocking mode. The method does not return
         * until all supplied buffer is written or an error
         * occurs.
         * @param buff The data buffer to write.
         * @param ec Cleared on success, and set to error on
         * failure.
         * @return The count of written bytes.
         */
        std::size_t write(const_buffer buff, std::error_code& ec) noexcept {
            return impl_.write(buff, ec);
        }

        /*!
         * @brief Write a buffer of data to the pipe. Write is
         * done in blocking mode. The method does not return
         * until all supplied buffer is written or an error
         * occurs.
         * @param buff The data buffer to write.
         * @param ec Cleared on success, and set to error on
         * failure.
         * @throws On failure `std::system_error` is thrown.
         */
        std::size_t write(const const_buffer& buff) {
            std::error_code ec;
            auto written = write(buff, ec);
            check_and_throw(ec, __func__);
            return written;
        }

        /*!
         * @brief Perform async write on the pipe. The pipe must
         * have been connected by either accept or connect prior
         * to reading and writing. The operation will complete
         * when all data in @p buffers is written or an error
         * occurs.
         * @tparam Handler The type of handler which must
         * satisfy WriteHandler
         * @tparam Alloc The type of allocator which must
         * satisfy HandlerAllocator
         * @param buffer The data to be written. The underlying
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
        template <WriteHandler Handler,
                  HandlerAllocator Alloc = default_io_allocator>
        void async_write(const_buffer buffer, Handler&& handler,
                         const Alloc& alloc = Alloc()) {
            impl_.async_write(buffer, std::forward<Handler>(handler), alloc);
        }

        /*!
         * @brief Read from the pipe into a buffer. Read is done
         * in blocking mode. The method will return if at least
         * one byte is read, or an error occurs.
         * @param buff The buffer to read into.
         * @param ec Cleared on success, and set to error on
         * failure.
         * @return The count of read bytes.
         */
        std::size_t read_some(const mutable_buffer& buff,
                              std::error_code& ec) noexcept {
            return impl_.read(buff, ec);
        }

        /*!
         * @brief Read from the pipe into a buffer. Read is done
         * in blocking mode. The method will return if at least
         * one byte is read, or an error occurs.
         * @param buff The buffer to read into.
         * @param ec Cleared on success, and set to error on
         * failure.
         * @throws On failure `std::system_error` is thrown.
         */
        std::size_t read_some(const mutable_buffer& buff) {
            std::error_code ec;
            auto read_num = read_some(buff, ec);
            check_and_throw(ec, __func__);
            return read_num;
        }

        /*!
         * @brief Read from the pipe into a buffer. Read is done
         * in blocking mode. The method does not return until
         * all supplied buffer is filled or an error occurs.
         * @param buff The buffer to read into.
         * @param ec Cleared on success, and set to error on
         * failure.
         * @return The count of read bytes.
         */
        void read(mutable_buffer buff, std::error_code& ec) noexcept {
            do {
                buff += read_some(buff, ec);
            } while (!buff.empty() && !ec);
        }

        /*!
         * @brief Read from the pipe into a buffer. Read is done
         * in blocking mode. The method does not return until
         * all supplied buffer is filled or an error occurs.
         * @param buff The buffer to read into.
         * @param ec Cleared on success, and set to error on
         * failure.
         * @throws On failure `std::system_error` is thrown.
         */
        void read(const mutable_buffer& buff) {
            std::error_code ec;
            read(buff, ec);
            check_and_throw(ec, __func__);
        }

        /*!
         * @brief Perform async read on the pipe.
         * The pipe must have been connected by either accept or
         * connect prior to reading and writing. The operation
         * will complete when at least one byte or more is read
         * or an error occurs.
         * @tparam Handler The type of handler which must
         * satisfy ReadHandler
         * @tparam Alloc The type of allocator which must
         * satisfy HandlerAllocator
         * @param buffer The buffers into which the data will be
         * read. The underlying buffers must be valid until the
         * operation completes.
         * @param handler The handler to invoke when the
         * operation is done and will be passed an error_code
         * that determines whether the operation has succeeded
         * or failed, and the count of bytes transferred.
         * @param alloc An allocator used to allocate the
         * operation. The size passed to its allocate method is
         * in bytes and the alignment of the allocator must be
         * suitable for std::max_align_t
         */
        template <ReadHandler Handler,
                  HandlerAllocator Alloc = default_io_allocator>
        void async_read_some(mutable_buffer buffer, Handler&& handler,
                             const Alloc& alloc = Alloc()) {
            impl_.async_read(buffer, std::forward<Handler>(handler), alloc);
        }

        /*!
         * @brief Perform async read on the pipe.
         * The pipe must have been connected by either accept or
         * connect prior to reading and writing. The operation
         * will complete when all @p buffers are filled or an
         * error occurs.
         * @tparam Handler The type of handler which must
         * satisfy ReadHandler
         * @tparam Alloc The type of allocator which must
         * satisfy HandlerAllocator
         * @param buffer The buffers into which the data will be
         * read. The underlying buffers must be valid until the
         * operation completes.
         * @param handler The handler to invoke when the
         * operation is done and will be passed an error_code
         * that determines whether the operation has succeeded
         * or failed, and the count of bytes transferred.
         * @param alloc An allocator used to allocate the
         * operation. The size passed to its allocate method is
         * in bytes and the alignment of the allocator must be
         * suitable for std::max_align_t
         */
        template <ReadHandler Handler,
                  HandlerAllocator Alloc = default_io_allocator>
        void async_read(mutable_buffer buffer, Handler&& handler,
                        const Alloc& alloc = Alloc()) {
            impl_.async_read_all(buffer, std::forward<Handler>(handler), alloc);
        }

        /*!
         * @brief Flush pending data on the pipe.
         * @param ec Cleared on success, and set to error on
         * failure.
         */
        RAD_EXPORT_DECL void flush(std::error_code& ec) noexcept;

        /*!
         * @brief Flush pending data on the pipe.
         * @throws On failure 'std::system_error' is thrown.
         */
        void flush() {
            std::error_code ec;
            flush(ec);
            check_and_throw(ec, __func__);
        }

    private:
        RAD_EXPORT_DECL async_result
        do_async_accept(io::detail::io_op& op) noexcept;

        RAD_EXPORT_DECL std::error_code
        get_accept_result(io::detail::io_op& op) noexcept;

        any_executor& get_any_exuector() noexcept {
            return impl_.get_any_exuector();
        }

        io::detail::async_file_impl impl_;
        std::string name_utf8_;
    };

    class [[nodiscard]] async_pipe::accept_awaiter final : noncopyable,
                                                           io::detail::io_op,
                                                           error_storage {
        ref<async_pipe> pipe;
        std::coroutine_handle<> waiter;

    public:
        accept_awaiter(async_pipe& pipe, std::error_code& ec) noexcept
            : io::detail::io_op(details::async_op_type::accept),
              error_storage(ec), pipe{pipe} {
        }

        bool await_ready() const noexcept {
            return has_error();
        }

        RAD_EXPORT_DECL bool await_suspend(std::coroutine_handle<> coro);

        RAD_EXPORT_DECL void await_resume() const;

        RAD_EXPORT_DECL void invoke_operation() override;

        RAD_EXPORT_DECL any_executor&
        associated_executor() const noexcept override;
    };

    template <class Handler, class Alloc>
    struct async_pipe::accept_op final : io::detail::io_op,
                                         allocator_storage<Alloc> {
        using alloc_base = allocator_storage<Alloc>;
        using handler_t = std::remove_cvref_t<Handler>;

        ref<async_pipe> pipe;
        handler_t handler;

        template <class H>
        accept_op(async_pipe& pipe, H&& handler, const Alloc& alloc)
            : io::detail::io_op(details::async_op_type::accept),
              alloc_base(alloc), pipe{pipe}, handler{std::forward<H>(handler)} {
        }

        void invoke_operation() override {
            auto ec = pipe->get_accept_result(*this);
            details::invoke_handler(this, ec);
        }

        any_executor& associated_executor() const noexcept override {
            return pipe->executor().as_any_executor();
        }
    };

    inline auto async_pipe::async_accept(std::error_code& ec)
        -> accept_awaiter {
        return {*this, ec};
    }
} // namespace RAD_LIB_NAMESPACE::pipe