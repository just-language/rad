#pragma once
#ifdef _WIN32
#include <rad/io/windows/serial_port_impl.h>
#else
#include <rad/io/posix/serial_port_impl.h>
#endif
#include <rad/detail/string_converter.h>

namespace RAD_LIB_NAMESPACE::io {
    /*!
     * @brief Provides serial port functionality.
     *
     * The basic_serial_port class provides a wrapper over serial port
     * functionality.
     */
    class serial_port {
        using implementation_type = io::detail::serial_port_impl;

        using string_converter = rad::detail::string_converter<
            typename implementation_type::native_string_type,
            typename implementation_type::alternative_string_type1,
            typename implementation_type::alternative_string_type2>;

    public:
        /*!
         * @brief The type of the executor associated with the object.
         */
        using executor_type = typename implementation_type::executor_type;
        /*!
         * @brief A `serial_port` is always the lowest layer.
         */
        using lowest_layer_type = serial_port;
        /*!
         * @brief The serial port native handle wrapper.
         */
        using native_handle_type =
            typename implementation_type::native_handle_type;
        /*!
         * @brief The native representation of a serial port.
         */
        using native_fd_type = typename implementation_type::native_fd_type;
        /*!
         * @brief The native os path string type.
         */
        using native_path_type = typename implementation_type::native_path_type;

        template <class AllocatorTypes>
        static constexpr std::size_t max_allocator_size() noexcept {
            return implementation_type::max_allocator_size<AllocatorTypes>();
        }

        /*!
         * @brief Construct a serial port without opening it.
         *
         * This constructor creates a serial port without
         * opening it. The serial port needs to be opened
         * before data can be sent or received on it.
         * @param ex The I/O executor that the serial port
         * will use to dispatch handlers for any asynchronous
         * operations performed on the serial port.
         */
        serial_port(executor_type& ex) noexcept : impl_{ex} {
        }

        /*!
         * @brief Create a serial port object and open it.
         * @param ex The I/O executor that the serial port
         * will use to dispatch handlers for any asynchronous
         * operations performed on the serial port.
         * @param path The device name.
         * @param access The desired access on the serial port.
         */
        template <class StringType>
        serial_port(executor_type& ex, const StringType& path,
                    serial_access access)
            : serial_port(ex) {
            open(path, access);
        }

        /*!
         * @brief Create a serial port object and open it.
         * @param ex The I/O executor that the serial port
         * will use to dispatch handlers for any asynchronous
         * operations performed on the serial port.
         * @param path The device name.
         */
        template <class StringType>
        serial_port(executor_type& ex, const StringType& path)
            : serial_port(ex) {
            open(path);
        }

        /*!
         * @brief Get a reference to the lowest layer.
         * @return A reference to the lowest layer.
         */
        lowest_layer_type& lowest_layer() noexcept {
            return *this;
        }

        /*!
         * @brief Get a const reference to the lowest layer.
         * @return A const reference to the lowest layer.
         */
        const lowest_layer_type& lowest_layer() const noexcept {
            return *this;
        }

        /*!
         * @brief Get a reference to the native handle wrapper.
         * @return A reference to the native handle wrapper.
         */
        native_handle_type& native_handle() noexcept {
            return impl_.native_handle();
        }

        /*!
         * @brief Get a const reference to the native handle wrapper.
         * @return A const reference to the native handle wrapper.
         */
        const native_handle_type& native_handle() const noexcept {
            return impl_.native_handle();
        }

        /*!
         * @brief Get the native serial port handle.
         * @return The native serial port handle.
         */
        native_fd_type native_fd() const noexcept {
            return native_handle().get();
        }

        /*!
         * @brief Get the path of the device this serial port is connected to.
         * If the serial port is not open the returned string will be empty.
         * @return The path of the device this serial port is connected to.
         */
        const native_path_type& path() const noexcept {
            return impl_.path();
        }

        /*!
         * @brief Get the executor associated with the object.
         * @return The executor associated with the object.
         */
        executor_type& executor() noexcept {
            return impl_.executor();
        }

        /*!
         * @brief Get the executor associated with the object.
         * @return The executor associated with the object.
         */
        const executor_type& executor() const noexcept {
            return impl_.executor();
        }

        /*!
         * @brief Open the serial port using the specified device name.
         * @param path The device name.
         * @param access The desired access on the serial port.
         * @param ec Set to indicate what error occurred, if any.
         */
        template <class StringType>
        void open(const StringType& path, serial_access access,
                  std::error_code& ec) {
            string_converter cv;
            impl_.open(cv(path), access, ec);
        }

        /*!
         * @brief Open the serial port using the specified device name.
         * @param path The device name.
         * @param ec Set to indicate what error occurred, if any.
         */
        template <class StringType>
        void open(const StringType& path, std::error_code& ec) {
            open(path, serial_access::read_write, ec);
        }

        /*!
         * @brief Open the serial port using the specified device name.
         * @param path The device name.
         * @param access The desired access on the serial port.
         */
        template <class StringType>
        void open(const StringType& path, serial_access access) {
            std::error_code ec;
            open(path, access, ec);
            check_and_throw(ec, __func__);
        }

        /*!
         * @brief Open the serial port using the specified device name.
         * @param path The device name.
         */
        template <class StringType>
        void open(const StringType& path) {
            open(path, serial_access::read_write);
        }

        /*!
         * @brief Check if the serial port handle is open.
         * @return True if the serial port handle is open, otherwise false.
         */
        bool is_open() const noexcept {
            return impl_.is_open();
        }

        /*!
         * @brief Close the serial port if it is open.
         */
        void close() noexcept {
            impl_.close();
        }

        /*!
         * @brief Cancel pending async operations.
         * Canceled operations are passed an error_code that
         * indicates cancelation. Note that not all operations
         * can be canceled. Operations that have completed and
         * are scheduled for invocation can no longer be
         * canceled.
         */
        void cancel() noexcept {
            impl_.cancel();
        }

        /*!
         * @brief Start an asynchronous write using an
         * awaitable.
         *
         * This function is used to asynchronously write all
         * provided data to the serial port. It is an
         * initiating function for an asynchronous operation,
         * and returns an awaitable object. To start the write
         * operation and wait for its completion, await the
         * returned awaitable.
         * @param buff One data buffer to be written
         * to the serial port. Although the buffer object may be
         * copied as necessary, ownership of the underlying
         * memory blocks is retained by the caller, which must
         * guarantee that they remain valid until the completion
         * handler is called.
         * @param ec If this is a reference to no_ec (default),
         * then it is ignored and errors are reported with
         * exceptions. Otherwise it is set to indicate what
         * error occurred, if any.
         * @return Awaitable object, that is when awaited will
         * start the write operation and wait for its
         * completion. The result of the awaitable is the number
         * of bytes written. On error the number of bytes
         * returned from the awaitable will be 0.
         */
        auto async_write(const_buffer buff, std::error_code& ec = no_ec) {
            return impl_.async_write(buff, ec);
        }

        /*!
         * @brief Start an asynchronous read using an awaitable.
         *
         * This function is used to asynchronously read data
         * from the serial port. It is an initiating function
         * for an asynchronous operation, and returns an
         * awaitable object. To start the read operation and
         * wait for its completion, await the returned
         * awaitable. The read operation may not read all of the
         * requested number of bytes. Consider using the
         * async_read method if you need to ensure that the
         * requested amount of data is read before the
         * asynchronous operation completes.
         * @param buff One buffer into which the
         * data will be read. Although the buffer object may be
         * copied as necessary, ownership of the underlying
         * memory blocks is retained by the caller, which must
         * guarantee that they remain valid until the completion
         * handler is called.
         * @param ec If this is a reference to no_ec, then it is
         * ignored and errors are reported with exceptions.
         * Otherwise it is set to indicate what error occurred,
         * if any.
         * @return Awaitable object, that is when awaited will
         * start the read operation and wait for its completion.
         * The result of the awaitable is the number of bytes
         * read. On error, or if size of @p buffers is 0, the
         * number of bytes returned from the awaitable will be
         * 0.
         */
        auto async_read_some(mutable_buffer buff, std::error_code& ec = no_ec) {
            return impl_.async_read(buff, ec);
        }

        /*!
         * @brief Start an asynchronous read using an awaitable.
         *
         * This function is used to asynchronously read data
         * from the serial port. It is an initiating function
         * for an asynchronous operation, and returns an
         * awaitable object. To start the read operation and
         * wait for its completion, await the returned
         * awaitable. The read operation will not complete
         * before reading all of the requested number of bytes,
         * or an error occurs.
         * @param buff One buffer into which the
         * data will be read. Although the buffer object may be
         * copied as necessary, ownership of the underlying
         * memory blocks is retained by the caller, which must
         * guarantee that they remain valid until the completion
         * handler is called.
         * @param ec If this is a reference to no_ec, then it is
         * ignored and errors are reported with exceptions.
         * Otherwise it is set to indicate what error occurred,
         * if any.
         * @return Awaitable object, that is when awaited will
         * start the read operation and wait for its completion.
         * The result of the awaitable is the number of bytes
         * read. On error, or if size of @p buffers is 0, the
         * number of bytes returned from the awaitable will be
         * 0.
         */
        auto async_read(mutable_buffer buff, std::error_code& ec = no_ec) {
            return impl_.async_read_all(buff, ec);
        }

        /*!
         * @brief Write all of the supplied data on the serial port.
         * The function call will block until the data has been
         * sent successfully or an error occurs.
         * @param buff One buffer to be sent on the serial port.
         * @param ec Set to indicate what error occurred, if
         * any.
         * @return The number of bytes sent.
         * Returns 0 if an error occurred, or size of @p buff
         * was 0.
         */
        std::size_t write(const_buffer buff, std::error_code& ec) noexcept {
            return impl_.write(buff, ec);
        }

        /*!
         * @brief Write all of the supplied data on the serial port.
         * The function call will block until the data has been
         * sent successfully or an error occurs.
         * @param buff One buffer to be sent on the serial port.
         * @return The number of bytes sent.
         * Returns 0 if an error occurred, or size of @p buff
         * was 0.
         */
        std::size_t write(const const_buffer& buff) {
            std::error_code ec;
            auto written = write(buff, ec);
            check_and_throw(ec, __func__);
            return written;
        }

        /*!
         * @brief Start an asynchronous write.
         *
         * This function is used to asynchronously write all
         * provided data to the serial port. It is an
         * initiating function for an asynchronous operation,
         * and always returns immediately.
         * @tparam Handler The type of handler.
         * @tparam Alloc The type of allocator.
         * @param buffer One data buffer to be written
         * to the serial port. Although the buffer object may be
         * copied as necessary, ownership of the underlying
         * memory blocks is retained by the caller, which must
         * guarantee that they remain valid until the completion
         * handler is called.
         * @param handler The handler that will be invoked with
         * the result of the write operation when it is
         * finished. It must be callable with:
         * `handler(std::size_t{}, std::error_code{})`.
         * @param alloc The allocator used to allocate memory
         * for the operation.
         */
        template <WriteHandler Handler,
                  HandlerAllocator Alloc = default_io_allocator>
        void async_write(const_buffer buffer, Handler&& handler,
                         const Alloc& alloc = Alloc()) {
            impl_.async_write(buffer, std::forward<Handler>(handler), alloc);
        }

        /*!
         * @brief Read some data from the serial port.
         * This function is used to read data from the serial port.
         * The function call will block until one or
         * more bytes of data has been read successfully, or
         * until an error occurs.
         * @param buff One buffer into which the data will be read.
         * @param ec Set to indicate what error occurred, if
         * any.
         * @return The number of bytes read.
         * Returns 0 if an error occurred, or size of @p buffers
         * was 0.
         */
        std::size_t read_some(const mutable_buffer& buff,
                              std::error_code& ec) noexcept {
            return impl_.read(buff, ec);
        }

        /*!
         * @brief Read some data from the serial port.
         * This function is used to read data from the serial port.
         * The function call will block until one or
         * more bytes of data has been read successfully, or
         * until an error occurs.
         * @param buff One buffer into which the data will be read.
         * @return The number of bytes read.
         * Returns 0 if an error occurred, or size of @p buffers
         * was 0.
         */
        std::size_t read_some(const mutable_buffer& buff) {
            std::error_code ec;
            auto read_num = read_some(buff, ec);
            check_and_throw(ec, __func__);
            return read_num;
        }

        /*!
         * @brief Attempt to read a certain amount of data from
         * a serial port before returning.
         * @param buff One buffer into which the
         * data will be read. The buffer size
         * indicates the maximum number of bytes to read from
         * the serial port.
         * @param ec Set to indicate what error occurred, if
         * any.
         * @return The number of bytes transferred.
         * Returns 0 if an error occurred, or size of @p buffers
         * was 0.
         */
        void read(mutable_buffer buff, std::error_code& ec) noexcept {
            do {
                buff += read_some(buff, ec);
            } while (!buff.empty() && !ec);
        }

        /*!
         * @brief Attempt to read a certain amount of data from
         * a serial port before returning.
         * @param buff One buffer into which the
         * data will be read. The buffer size
         * indicates the maximum number of bytes to read from
         * the serial port.
         * @return The number of bytes transferred.
         * Returns 0 if an error occurred, or size of @p buffers
         * was 0.
         */
        void read(const mutable_buffer& buff) {
            std::error_code ec;
            read(buff, ec);
            check_and_throw(ec, __func__);
        }

        /*!
         * @brief Start an asynchronous read.
         *
         * This function is used to asynchronously read data
         * from the serial port. It is an initiating
         * function for an asynchronous operation, and always
         * returns immediately. The read operation may not read
         * all of the requested number of bytes. Consider using
         * the async_read method if you need to ensure that the
         * requested amount of data is read before the
         * asynchronous operation completes.
         * @tparam Handler The type of handler.
         * @tparam Alloc The type of allocator.
         * @param buffer One data buffer into which the
         * data will be read. Although the buffer object may be
         * copied as necessary, ownership of the underlying
         * memory blocks is retained by the caller, which must
         * guarantee that they remain valid until the completion
         * handler is called.
         * @param handler The handler that will be invoked with
         * the result of the read operation when it is finished.
         * It must be callable with: `handler(std::size_t{},
         * std::error_code{})`.
         * @param alloc The allocator used to allocate memory
         * for the operation.
         */
        template <ReadHandler Handler,
                  HandlerAllocator Alloc = default_io_allocator>
        void async_read_some(mutable_buffer buffer, Handler&& handler,
                             const Alloc& alloc = Alloc()) {
            impl_.async_read(buffer, std::forward<Handler>(handler), alloc);
        }

        /*!
         * @brief Start an asynchronous read.
         *
         * This function is used to asynchronously read data
         * from the serial port. It is an initiating
         * function for an asynchronous operation, and always
         * returns immediately. The read operation will not
         * complete before reading all of the requested number
         * of bytes, or an error occurs.
         * @tparam Handler The type of handler.
         * @tparam Alloc The type of allocator.
         * @param buffer One buffer into which the
         * data will be read. Although the buffer object may be
         * copied as necessary, ownership of the underlying
         * memory blocks is retained by the caller, which must
         * guarantee that they remain valid until the completion
         * handler is called.
         * @param handler The handler that will be invoked with
         * the result of the read operation when it is finished.
         * It must be callable with: `handler(std::size_t{},
         * std::error_code{})`.
         * @param alloc The allocator used to allocate memory
         * for the operation.
         */
        template <ReadHandler Handler,
                  HandlerAllocator Alloc = default_io_allocator>
        void async_read(mutable_buffer buffer, Handler&& handler,
                        const Alloc& alloc = Alloc()) {
            impl_.async_read_all(buffer, std::forward<Handler>(handler), alloc);
        }

        /*!
         * @brief Get the options of the serial port.
         * @param ec Set to indicate error occured, if any.
         * @return The options of the serial port.
         */
        serial_options get_options(std::error_code& ec) noexcept {
            return impl_.get_options(ec);
        }

        /*!
         * @brief Get the options of the serial port.
         * @return The options of the serial port.
         */
        serial_options get_options() {
            std::error_code ec;
            auto opts = get_options(ec);
            check_and_throw(ec, __func__);
            return opts;
        }

        /*!
         * @brief Set the options of the serial port.
         * @param opts The options of the serial port.
         * @param ec Set to indicate error occured, if any.
         */
        void set_options(const serial_options& opts,
                         std::error_code& ec) noexcept {
            impl_.set_options(opts, ec);
        }

        /*!
         * @brief Set the options of the serial port.
         * @param opts The options of the serial port.
         */
        void set_options(const serial_options& opts) {
            std::error_code ec;
            impl_.set_options(opts, ec);
            check_and_throw(ec, __func__);
        }

    private:
        implementation_type impl_;
    };
} // namespace RAD_LIB_NAMESPACE::io