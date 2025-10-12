#pragma once
#include <rad/net/detail/async_socket_impl.h>

namespace RAD_LIB_NAMESPACE::net {
    /*!
     * @brief Base for both stream and connection-less sockets and
     * acceptors.
     * @tparam Protocol
     */
    template <class Protocol>
    class async_socket_base {
    protected:
        using impl_type = detail::async_socket_impl;

    public:
        template <class AllocatorTypes>
        static constexpr std::size_t max_allocator_size() noexcept {
            constexpr std::size_t sizes[] = {
                impl_type::template write_allocator_size<AllocatorTypes>(),
                impl_type::template read_allocator_size<AllocatorTypes>(),
                impl_type::template sendto_allocator_size<AllocatorTypes>(),
                impl_type::template recvfrom_allocator_size<AllocatorTypes>(),
            };

            return max_of(sizes);
        }

        /// The protocol type.
        using protocol_type = Protocol;
        /// The endpoint type.
        using endpoint_type = typename Protocol::endpoint_type;
        /// The native handle wrapper of a socket.
        using native_handle_type = typename impl_type::native_handle_type;
        /// The native representation of a socket.
        using native_fd_type = typename impl_type::native_fd_type;
        /// The type used for count of bytes transferred
        /// (std::size_t).
        using size_type = typename impl_type::size_type;
        /// The type of executor used by the socke
        /// (io_executor).
        using executor_type = io_executor;
        /// The next layer type (async_socket_base).
        using next_layer_type = async_socket_base;
        /// The lowest layer type (async_socket_base).
        using lowest_layer_type = async_socket_base;

        /*!
         * @brief Construct a closed socket and use the provided
         * executor
         * @param ex the executor to attach the socket to
         */
        explicit async_socket_base(executor_type& ex) noexcept : impl{ex} {
        }

        async_socket_base(executor_type& ex, socket_fd_t sock_fd)
            : impl{ex, sock_fd} {
        }

        async_socket_base(executor_type& ex, native_handle_type& sock_fd)
            : impl{ex, sock_fd} {
        }

        async_socket_base(executor_type& ex, const protocol_type& protocol)
            : async_socket_base(ex) {
            open(protocol);
        }

        async_socket_base(executor_type& ex, const protocol_type& protocol,
                          const endpoint_type& local_address)
            : async_socket_base(ex, protocol) {
            bind(local_address);
        }

        async_socket_base(executor_type& ex, const endpoint_type& local_address)
            : async_socket_base(ex, protocol_type{local_address.family()},
                                local_address) {
        }

        async_socket_base(async_socket_base&&) = default;

        async_socket_base& operator=(async_socket_base&&) = default;

        /*!
         * @brief Check whether the native os socket is used in
         * non blocking mode or not
         * @return true if the socket is non blocking and false
         * otherwise
         */
        constexpr bool non_blocking() noexcept {
#ifdef _WIN32
            return false;
#else
            return true;
#endif // _WIN32
        }

        /*!
         * @brief Get a reference to the native handle wrapper
         * used by this socket class wrapper
         * @return a reference to the native handle wrapper
         */
        native_handle_type& native_handle() noexcept {
            return impl.native_handle();
        }

        /*!
         * @brief Get a reference to the native handle wrapper
         * used by this socket class wrapper
         * @return a reference to the native handle wrapper
         */
        const native_handle_type& native_handle() const noexcept {
            return impl.native_handle();
        }

        /*!
         * @brief Get the native os socket fd which may be
         * closed. The native fd can be passed to other os api
         * functions to add functionality not provided by this
         * library. Don't miss with options that may break this
         * socket wrapper like non blocking mode, ...
         * @return the native os socket fd
         */
        native_fd_type native_fd() const noexcept {
            return impl.native_fd();
        }

        next_layer_type& next_layer() noexcept {
            return *this;
        }

        const next_layer_type& next_layer() const noexcept {
            return *this;
        }

        lowest_layer_type& lowest_layer() noexcept {
            return *this;
        }

        const lowest_layer_type& lowest_layer() const noexcept {
            return *this;
        }

        /*!
         * @brief Get a reference to the io executor used by the
         * socket which may be either an io_loop or a strand
         * over io_loop
         * @return a reference to io the executor used by the
         * socket
         */
        executor_type& executor() noexcept {
            return impl.executor();
        }

        /*!
         * @brief Get a reference to io the executor used by the
         * socket which may be either an io_loop or a strand
         * over io_loop
         * @return a reference to io the executor used by the
         * socket
         */
        const executor_type& executor() const noexcept {
            return impl.executor();
        }

        /*!
         * @brief Check if the socket is open
         * @return true if open and false otherwise
         */
        bool is_open() const noexcept {
            return impl.is_open();
        }

        /*!
         * @brief Close the socket if open and cancel any
         * pending operations on the socket. Canceled operations
         * are passed an error_code that indicates cancelation.
         * Note that not all operations can be canceled.
         * Operations that have completed and are scheduled for
         * invocation can no longer be canceled
         */
        void close() noexcept {
            impl.close();
        }

        /*!
         * @brief Shutdown the send end or the receive end or
         * both using shutdown() system call
         * @param how the direction in which to shutdown the
         * socket
         * @param ec an error code used to report errors from os
         * functions
         */
        void shutdown(socket_shutdown how, std::error_code& ec) noexcept {
            impl.shutdown(how, ec);
        }

        /*!
         * @brief Shutdown the send end or the receive end or
         * both using shutdown() system call
         * @param how the direction in which to shutdown the
         * socket (defaults to both)
         */
        void shutdown(socket_shutdown how = socket_shutdown::both) {
            std::error_code ec;
            shutdown(how, ec);
            check_and_throw(ec, __func__);
        }

        /*!
         * @brief Set an option to socket using setsockopt()
         * system call
         * @tparam SocketOption type of the option
         * @param option the option to set
         * @param ec an error code used to report errors from os
         * functions
         */
        template <class SocketOption>
        void set_option(const SocketOption& option,
                        std::error_code& ec) noexcept {
            static_assert(is_supported_option<SocketOption, Protocol>,
                          "unsupported option for this protocol");
            impl.set_option(option.level(), option.name(), option.data(),
                            option.size(), ec);
        }

        /*!
         * @brief Set an option to socket using setsockopt()
         * system call
         * @tparam SocketOption type of the option
         * @param option the option to set
         */
        template <class SocketOption>
        void set_option(const SocketOption& option) {
            std::error_code ec;
            set_option(option, ec);
            check_and_throw(ec, __func__);
        }

        /*!
         * @brief Get an option of the socket using getsockopt()
         * system call
         * @tparam SocketOption type of the option
         * @param option receives the retrived option on success
         * @param ec an error code used to report errors from os
         * functions
         */
        template <class SocketOption>
        void get_option(SocketOption& option,
                        std::error_code& ec) const noexcept {
            static_assert(is_supported_option<SocketOption, Protocol>,
                          "unsupported option for this protocol");
            socklen_t size = option.size();
            impl.get_option(option.level(), option.name(), option.data(), size,
                            ec);
        }

        /*!
         * @brief Get an option of the socket using getsockopt()
         * system call
         * @tparam SocketOption type of the option
         * @param option receives the retrived option on success
         */
        template <class SocketOption>
        void get_option(SocketOption& option) const {
            std::error_code ec;
            get_option(option, ec);
            check_and_throw(ec, __func__);
        }

        /*!
         * @brief Get an option of the socket using getsockopt()
         * system call
         * @tparam SocketOption type of the option
         * @return the retrived option
         */
        template <class SocketOption>
        SocketOption get_option() const {
            SocketOption opt;
            get_option(opt);
            return opt;
        }

        /*!
         * @brief Get the local address the socket is bound to.
         * Note that a connected socket is also bound either
         * explicitly before connect or implicitly by a local
         * address chosen by the os
         * @param ec an error code used to report errors from os
         * functions
         * @return the local endooint the socket is bound to on
         * success or a default endpoint on failure
         */
        endpoint_type local_endpoint(std::error_code& ec) const noexcept {
            endpoint_type address;
            socklen_t size = endpoint_type::max_size();
            impl.local_endpoint(address.address(), size, ec);
            if (!ec) {
                address.resize(size);
            }
            return address;
        }

        /*!
         * @brief Get the local address the socket is bound to.
         * Note that a connected socket is also bound either
         * explicitly before connect or implicitly by a local
         * address chosen by the os
         * @param ec an error code used to report errors from os
         * functions
         * @return the local endooint the socket is bound to
         */
        endpoint_type local_endpoint() const {
            std::error_code ec;
            auto epoint = local_endpoint(ec);
            check_and_throw(ec, __func__);
            return epoint;
        }

        /*!
         * @brief Get the remote address of the peer the socket
         * is connected to
         * @param ec an error code used to report errors from os
         * functions
         * @return the remote endooint the socket is connected
         * to on success or a default endpoint on failure
         */
        endpoint_type remote_endpoint(std::error_code& ec) const noexcept {
            endpoint_type address;
            socklen_t size = endpoint_type::max_size();
            impl.remote_endpoint(address.address(), size, ec);
            if (!ec) {
                address.resize(size);
            }
            return address;
        }

        /*!
         * @brief Get the remote address of the peer the socket
         * is connected to
         * @return the remote endooint the socket is connected
         * to
         */
        endpoint_type remote_endpoint() const {
            std::error_code ec;
            auto peer = remote_endpoint(ec);
            check_and_throw(ec, __func__);
            return peer;
        }

        /*!
         * @brief Bind the socket to the given local endpoint.
         * The socket must be open.
         *
         * This function binds the socket to the specified
         * endpoint on the local machine.
         * @param local_address An endpoint on the local machine
         * to which the socket will be bound.
         * @param ec Set to indicate what error occurred, if
         * any.
         */
        void bind(const endpoint_type& local_address,
                  std::error_code& ec) noexcept {
            impl.bind(local_address.address(), local_address.size(), ec);
        }

        /*!
         * @brief Bind the socket to the given local endpoint.
         * The socket must be open.
         *
         * This function binds the socket to the specified
         * endpoint on the local machine.
         * @param local_address An endpoint on the local machine
         * to which the socket will be bound.
         */
        void bind(const endpoint_type& local_address) {
            std::error_code ec;
            bind(local_address, ec);
            check_and_throw(ec, __func__);
        }

        /*!
         * @brief Get the max listen backlog which is defined by
         * the os as SOMAXCONN
         * @return the max listen backlog
         */
        static constexpr int max_listen_backlog() noexcept {
            return impl_type::max_listen_backlog();
        }

        /*!
         * @brief Open the socket with the provided protocol. If
         * the socket is open before this call the socket is
         * closed and then reponed with the provided protocol.
         * If this method fails then the socket is not affected.
         * @param protocol the protocol to open the socket with
         * @param ec an error code used to report errors from os
         * functions
         */
        void open(const protocol_type& protocol, std::error_code& ec) noexcept {
            impl.open(protocol.family(), protocol.type(), protocol.protocol(),
                      ec);
        }

        /*!
         * @brief Open the socket with the provided protocol. If
         * the socket is open before this call the socket is
         * closed and then reponed with the provided protocol.
         * If this method fails then the socket is not affected.
         * @param protocol the protocol to open the socket with
         */
        void open(const protocol_type& protocol) {
            std::error_code ec;
            open(protocol, ec);
            check_and_throw(ec, __func__);
        }

        /*!
         * @brief Connect the socket to the specified endpoint.
         *
         * This function is used to connect a socket to the
         * specified remote endpoint. The function call will
         * block until the connection is successfully made or an
         * error occurs. The socket is automatically opened if
         * it is not already open. If the connect fails, and the
         * socket was automatically opened, the socket is not
         * returned to the closed state.
         * @param remote_address The remote endpoint to which
         * the socket will be connected.
         * @param ec Set to indicate what error occurred, if
         * any.
         */
        void connect(const endpoint_type& remote_address,
                     std::error_code& ec) noexcept {
            ec.clear();
            if (!is_open()) {
                open(Protocol{remote_address.family()}, ec);
            }
            if (!ec) {
                impl.connect(remote_address.address(), remote_address.size(),
                             ec);
            }
        }

        /*!
         * @brief Connect the socket to the specified endpoint.
         *
         * This function is used to connect a socket to the
         * specified remote endpoint. The function call will
         * block until the connection is successfully made or an
         * error occurs. The socket is automatically opened if
         * it is not already open. If the connect fails, and the
         * socket was automatically opened, the socket is not
         * returned to the closed state.
         * @param remote_address The remote endpoint to which
         * the socket will be connected.
         */
        void connect(const endpoint_type& remote_address) {
            std::error_code ec;
            connect(remote_address, ec);
            check_and_throw(ec, __func__);
        }

        /*!
         * @brief Establishes a socket connection by trying each
         * endpoint in a sequence.
         *
         * This method attempts to connect a socket to one of a
         * sequence of endpoints. It does this by repeated calls
         * to the socket's connect member function, once for
         * each endpoint in the sequence, until a connection is
         * successfully established.
         * @tparam EndpointRange The type of endpoints sequence.
         * @param addresses A sequence of endpoints.
         * @param ec Set to indicate what error occurred, if
         * any.
         */
        template <EndpointSequence<endpoint_type> EndpointRange>
        void connect(const EndpointRange& addresses,
                     std::error_code& ec) noexcept {
            for (const auto& epoint : addresses) {
                ec.clear();
                if (is_open()) {
                    address_family family = local_endpoint(ec).family();
                    if (ec || family != epoint.family()) {
                        ec.clear();
                        close();
                    }
                }
                connect(epoint, ec);
                if (!ec) {
                    break;
                }
            }
        }

        /*!
         * @brief Establishes a socket connection by trying each
         * endpoint in a sequence.
         *
         * This method attempts to connect a socket to one of a
         * sequence of endpoints. It does this by repeated calls
         * to the socket's connect member function, once for
         * each endpoint in the sequence, until a connection is
         * successfully established.
         * @tparam EndpointRange The type of endpoints sequence.
         * @param addresses A sequence of endpoints.
         */
        template <EndpointSequence<endpoint_type> EndpointRange>
        void connect(const EndpointRange& addrs) {
            std::error_code ec;
            connect(addrs, ec);
            check_and_throw(ec, __func__);
        }

        /*!
         * @brief Send all of the supplied data on the socket.
         * The function call will block until the data has been
         * sent successfully or an error occurs.
         * @tparam Buffers The buffers sequence type.
         * @param buffers One or more data buffers to be sent on
         * the socket.
         * @param flags Flags specifying how the send call is to
         * be made.
         * @param ec Set to indicate what error occurred, if
         * any.
         * @return The number of bytes sent.
         * Returns 0 if an error occurred, or size of @p buffers
         * was 0.
         */
        template <BufferSequence Buffers>
        size_type send(const Buffers& buffers, transfer_flags flags,
                       std::error_code& ec) noexcept {
            auto [buffs, n] = extract_buffers<true>(buffers);
            return impl.send(buffs, n, flags, ec);
        }

        /*!
         * @brief Send all of the supplied data on the socket.
         * The function call will block until the data has been
         * sent successfully or an error occurs.
         * @tparam Buffers The buffers sequence type.
         * @param buffers One or more data buffers to be sent on
         * the socket.
         * @param ec Set to indicate what error occurred, if
         * any.
         * @return The number of bytes sent.
         * Returns 0 if an error occurred, or size of @p buffers
         * was 0.
         */
        template <BufferSequence Buffers>
        size_type send(const Buffers& buffers, std::error_code& ec) noexcept {
            return send(buffers, transfer_flags::none, ec);
        }

        /*!
         * @brief Send all of the supplied data on the socket.
         * The function call will block until the data has been
         * sent successfully or an error occurs.
         * @tparam Buffers The buffers sequence type.
         * @param buffers One or more data buffers to be sent on
         * the socket.
         * @param flags Flags specifying how the send call is to
         * be made.
         * @return The number of bytes sent.
         * Returns 0 if size of @p buffers was 0.
         */
        template <BufferSequence Buffers>
        size_type send(const Buffers& buffers,
                       transfer_flags flags = transfer_flags::none) {
            std::error_code ec;
            auto sent = send(buffers, flags, ec);
            check_and_throw(ec, __func__);
            return sent;
        }

        /*!
         * @brief Send all of the supplied data on the socket.
         * The function call will block until the data has been
         * sent successfully or an error occurs.
         * @tparam Buffers The buffers sequence type.
         * @param buffers One or more data buffers to be sent on
         * the socket.
         * @param flags Flags specifying how the send call is to
         * be made.
         * @param ec Set to indicate what error occurred, if
         * any.
         * @return The number of bytes sent.
         * Returns 0 if an error occurred, or size of @p buffers
         * was 0.
         */
        template <BufferSequence Buffers>
        size_type write(const Buffers& buffers, transfer_flags flags,
                        std::error_code& ec) noexcept {
            return send(buffers, flags, ec);
        }

        /*!
         * @brief Send all of the supplied data on the socket.
         * The function call will block until the data has been
         * sent successfully or an error occurs.
         * @tparam Buffers The buffers sequence type.
         * @param buffers One or more data buffers to be sent on
         * the socket.
         * @param ec Set to indicate what error occurred, if
         * any.
         * @return The number of bytes sent.
         * Returns 0 if an error occurred, or size of @p buffers
         * was 0.
         */
        template <BufferSequence Buffers>
        size_type write(const Buffers& buffers, std::error_code& ec) noexcept {
            return send(buffers, ec);
        }

        /*!
         * @brief Send all of the supplied data on the socket.
         * The function call will block until the data has been
         * sent successfully or an error occurs.
         * @tparam Buffers The buffers sequence type.
         * @param buffers One or more data buffers to be sent on
         * the socket.
         * @param flags Flags specifying how the send call is to
         * be made.
         * @return The number of bytes sent.
         * Returns 0 if size of @p buffers was 0.
         */
        template <BufferSequence Buffers>
        size_type write(const Buffers& buffers,
                        transfer_flags flags = transfer_flags::none) {
            return send(buffers, flags);
        }

        /*!
         * @brief Send a datagram to the specified endpoint.
         *
         * This function is used to send a datagram to the
         * specified remote endpoint. The function call will
         * block until the data has been sent successfully or an
         * error occurs.
         * @tparam Buffers The buffers sequence type.
         * @param buffers One or more data buffers to be sent to
         * the remote endpoint.
         * @param receiver The remote endpoint to which the data
         * will be sent.
         * @param flags Flags specifying how the send call is to
         * be made.
         * @param ec Set to indicate what error occurred, if
         * any.
         * @return The number of bytes sent.
         * If the size of @p buffers is 0, the return will be 0.
         */
        template <BufferSequence Buffers>
        size_type send_to(const Buffers& buffers, const endpoint_type& receiver,
                          transfer_flags flags, std::error_code& ec) noexcept {
            auto [buffs, n] = extract_buffers<true>(buffers);
            return impl.send_to(buffs, n, flags, receiver.address(),
                                receiver.size(), ec);
        }

        /*!
         * @brief Send a datagram to the specified endpoint.
         *
         * This function is used to send a datagram to the
         * specified remote endpoint. The function call will
         * block until the data has been sent successfully or an
         * error occurs.
         * @tparam Buffers The buffers sequence type.
         * @param buffers One or more data buffers to be sent to
         * the remote endpoint.
         * @param receiver The remote endpoint to which the data
         * will be sent.
         * @param ec Set to indicate what error occurred, if
         * any.
         * @return The number of bytes sent.
         * If the size of @p buffers is 0, the return will be 0.
         */
        template <BufferSequence Buffers>
        size_type send_to(const Buffers& buffers, const endpoint_type& receiver,
                          std::error_code& ec) noexcept {
            return send_to(buffers, receiver, transfer_flags::none, ec);
        }

        /*!
         * @brief Send a datagram to the specified endpoint.
         *
         * This function is used to send a datagram to the
         * specified remote endpoint. The function call will
         * block until the data has been sent successfully or an
         * error occurs.
         * @tparam Buffers The buffers sequence type.
         * @param buffers One or more data buffers to be sent to
         * the remote endpoint.
         * @param receiver The remote endpoint to which the data
         * will be sent.
         * @param flags Flags specifying how the send call is to
         * be made.
         * @return The number of bytes sent.
         * If the size of @p buffers is 0, the return will be 0.
         */
        template <BufferSequence Buffers>
        size_type send_to(const Buffers& buffers, const endpoint_type& receiver,
                          transfer_flags flags = transfer_flags::none) {
            std::error_code ec;
            auto sent = send_to(buffers, receiver, flags, ec);
            check_and_throw(ec, __func__);
            return sent;
        }

        /*!
         * @brief Receive some data on the socket.
         * This function is used to receive data on the socket.
         * The function call will block until one or more bytes
         * of data has been received successfully, or until an
         * error occurs. In case of datagram socket, a full
         * message is received.
         * @tparam Buffers The buffers sequence type.
         * @param buffers One or more buffers into which the
         * data will be received.
         * @param flags Flags specifying how the receive call is
         * to be made.
         * @param ec Set to indicate what error occurred, if
         * any.
         * @return The number of bytes received.
         * For stream sockets returns 0 if an error occurred.
         * For datagram sockets a return value of 0 indicates a
         * zero length datagram and should not be considered an
         * error.
         */
        template <MutableBufferSequence Buffers>
        size_type receive(const Buffers& buffers, transfer_flags flags,
                          std::error_code& ec) noexcept {
            auto [buffs, n] = extract_buffers<false>(buffers);
            bool not_zero =
                protocol_type::is_stream_protocol && not_empty_buffers(buffers);
            return impl.receive(buffs, n, not_zero, flags, ec);
        }

        /*!
         * @brief Receive some data on the socket.
         * This function is used to receive data on the socket.
         * The function call will block until one or more bytes
         * of data has been received successfully, or until an
         * error occurs. In case of datagram socket, a full
         * message is received.
         * @tparam Buffers The buffers sequence type.
         * @param buffers One or more buffers into which the
         * data will be received.
         * @param ec Set to indicate what error occurred, if
         * any.
         * @return The number of bytes received.
         * For stream sockets returns 0 if an error occurred.
         * For datagram sockets a return value of 0 indicates a
         * zero length datagram and should not be considered an
         * error.
         */
        template <MutableBufferSequence Buffers>
        size_type receive(const Buffers& buffers,
                          std::error_code& ec) noexcept {
            return receive(buffers, transfer_flags::none, ec);
        }

        /*!
         * @brief Receive some data on the socket.
         * This function is used to receive data on the socket.
         * The function call will block until one or more bytes
         * of data has been received successfully, or until an
         * error occurs. In case of datagram socket, a full
         * message is received.
         * @tparam Buffers The buffers sequence type.
         * @param buffers One or more buffers into which the
         * data will be received.
         * @param flags Flags specifying how the receive call is
         * to be made.
         * @return The number of bytes received.
         * For datagram sockets a return value of 0 indicates a
         * zero length datagram and should not be considered an
         * error.
         */
        template <MutableBufferSequence Buffers>
        size_type receive(const Buffers& buffers,
                          transfer_flags flags = transfer_flags::none) {
            std::error_code ec;
            auto recved = receive(buffers, flags, ec);
            check_and_throw(ec, __func__);
            return recved;
        }

        /*!
         * @brief Read some data from the socket.
         * This function is used to read data from the stream
         * socket. The function call will block until one or
         * more bytes of data has been read successfully, or
         * until an error occurs.
         * @tparam Buffers The buffers sequence type.
         * @param buffers One or more buffers into which the
         * data will be read.
         * @param flags Flags specifying how the read call is to
         * be made.
         * @param ec Set to indicate what error occurred, if
         * any.
         * @return The number of bytes read.
         * Returns 0 if an error occurred, or size of @p buffers
         * was 0.
         */
        template <MutableBufferSequence Buffers>
        size_type read_some(const Buffers& buffers, transfer_flags flags,
                            std::error_code& ec) noexcept {
            return receive(buffers, flags, ec);
        }

        /*!
         * @brief Read some data from the socket.
         * This function is used to read data from the stream
         * socket. The function call will block until one or
         * more bytes of data has been read successfully, or
         * until an error occurs.
         * @tparam Buffers The buffers sequence type.
         * @param buffers One or more buffers into which the
         * data will be read.
         * @param ec Set to indicate what error occurred, if
         * any.
         * @return The number of bytes read.
         * Returns 0 if an error occurred, or size of @p buffers
         * was 0.
         */
        template <MutableBufferSequence Buffers>
        size_type read_some(const Buffers& buffers,
                            std::error_code& ec) noexcept {
            return receive(buffers, ec);
        }

        /*!
         * @brief Read some data from the socket.
         * This function is used to read data from the stream
         * socket. The function call will block until one or
         * more bytes of data has been read successfully, or
         * until an error occurs.
         * @tparam Buffers The buffers sequence type.
         * @param buffers One or more buffers into which the
         * data will be read.
         * @param flags Flags specifying how the read call is to
         * be made.
         * @return The number of bytes read.
         * Returns 0 if size of @p buffers was 0.
         */
        template <MutableBufferSequence Buffers>
        size_type read_some(const Buffers& buffers,
                            transfer_flags flags = transfer_flags::none) {
            return receive(buffers, flags);
        }

        /*!
         * @brief Attempt to read a certain amount of data from
         * a stream before returning.
         * @tparam Buffers The buffers sequence type.
         * @param buffers One or more buffers into which the
         * data will be read. The sum of the buffer sizes
         * indicates the maximum number of bytes to read from
         * the stream.
         * @param flags Flags specifying how the read call is to
         * be made.
         * @param ec Set to indicate what error occurred, if
         * any.
         * @return The number of bytes transferred.
         * Returns 0 if an error occurred, or size of @p buffers
         * was 0.
         */
        template <MutableBufferSequence Buffers>
        size_type read(const Buffers& buffers, transfer_flags flags,
                       std::error_code& ec) noexcept {
            return receive(buffers, flags | transfer_flags::wait_all, ec);
        }

        /*!
         * @brief Attempt to read a certain amount of data from
         * a stream before returning.
         * @tparam Buffers The buffers sequence type.
         * @param buffers One or more buffers into which the
         * data will be read. The sum of the buffer sizes
         * indicates the maximum number of bytes to read from
         * the stream.
         * @param ec Set to indicate what error occurred, if
         * any.
         * @return The number of bytes transferred.
         * Returns 0 if an error occurred, or size of @p buffers
         * was 0.
         */
        template <MutableBufferSequence Buffers>
        size_type read(const Buffers& buffers, std::error_code& ec) noexcept {
            return receive(buffers, transfer_flags::wait_all, ec);
        }

        /*!
         * @brief Attempt to read a certain amount of data from
         * a stream before returning.
         * @tparam Buffers The buffers sequence type.
         * @param buffers One or more buffers into which the
         * data will be read. The sum of the buffer sizes
         * indicates the maximum number of bytes to read from
         * the stream.
         * @param flags Flags specifying how the read call is to
         * be made.
         * @return The number of bytes transferred.
         * Returns 0 if size of @p buffers was 0.
         */
        template <MutableBufferSequence Buffers>
        size_type read(const Buffers& buffers,
                       transfer_flags flags = transfer_flags::wait_all) {
            return receive(buffers, flags | transfer_flags::wait_all);
        }

        /*!
         * @brief Receive a datagram with the endpoint of the
         * sender.
         *
         * This function is used to receive a datagram.
         * The function call will block until data has been
         * received successfully or an error occurs.
         * @tparam Buffers The buffers sequence type.
         * @param buffers One or more buffers into which the
         * data will be received.
         * @param sender An endpoint object that receives the
         * endpoint of the remote sender of the datagram.
         * @param flags Flags specifying how the receive call is
         * to be made.
         * @param ec Set to indicate what error occurred, if
         * any.
         * @return The number of bytes received which may be 0
         * if the received datagram has 0 length.
         */
        template <MutableBufferSequence Buffers>
        size_type receive_from(const Buffers& buffers, endpoint_type& sender,
                               transfer_flags flags,
                               std::error_code& ec) noexcept {
            flags &= ~transfer_flags::wait_all;
            auto [buffs, n] = extract_buffers<false>(buffers);
            bool not_zero =
                protocol_type::is_stream_protocol && not_empty_buffers(buffers);
            socklen_t size = endpoint_type::max_size();
            auto recved = impl.receive_from(buffs, n, not_zero, flags,
                                            sender.address(), size, ec);
            if (!ec) {
                sender.resize(size);
            }
            return recved;
        }

        /*!
         * @brief Receive a datagram with the endpoint of the
         * sender.
         *
         * This function is used to receive a datagram.
         * The function call will block until data has been
         * received successfully or an error occurs.
         * @tparam Buffers The buffers sequence type.
         * @param buffers One or more buffers into which the
         * data will be received.
         * @param sender An endpoint object that receives the
         * endpoint of the remote sender of the datagram.
         * @param ec Set to indicate what error occurred, if
         * any.
         * @return The number of bytes received which may be 0
         * if the received datagram has 0 length.
         */
        template <MutableBufferSequence Buffers>
        size_type receive_from(const Buffers& buffers, endpoint_type& sender,
                               std::error_code& ec) noexcept {
            return receive_from(buffers, sender, transfer_flags::none, ec);
        }

        /*!
         * @brief Receive a datagram with the endpoint of the
         * sender.
         *
         * This function is used to receive a datagram.
         * The function call will block until data has been
         * received successfully or an error occurs.
         * @tparam Buffers The buffers sequence type.
         * @param buffers One or more buffers into which the
         * data will be received.
         * @param sender An endpoint object that receives the
         * endpoint of the remote sender of the datagram.
         * @param flags Flags specifying how the receive call is
         * to be made.
         * @return The number of bytes received which may be 0
         * if the received datagram has 0 length.
         */
        template <MutableBufferSequence Buffers>
        size_type receive_from(const Buffers& buffers, endpoint_type& sender,
                               transfer_flags flags = transfer_flags::none) {
            std::error_code ec;
            auto recved = receive_from(buffers, sender, flags, ec);
            check_and_throw(ec, __func__);
            return recved;
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
            impl.cancel();
        }

        /*!
         * @brief Start an asynchronous write using an
         * awaitable.
         *
         * This function is used to asynchronously write all
         * provided data to the stream socket. It is an
         * initiating function for an asynchronous operation,
         * and returns an awaitable object. To start the write
         * operation and wait for its completion, await the
         * returned awaitable.
         * @tparam Buffers The buffers sequence type.
         * @param buffers One or more data buffers to be written
         * to the socket. Although the buffers object may be
         * copied as necessary, ownership of the underlying
         * memory blocks is retained by the caller, which must
         * guarantee that they remain valid until the completion
         * handler is called.
         * @param flags Flags specifying how the write call is
         * to be made.
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
        template <BufferSequence Buffers>
        auto async_write(Buffers&& buffers,
                         transfer_flags flags = transfer_flags::none,
                         std::error_code& ec = no_ec) {
            return impl.async_write(std::forward<Buffers>(buffers), flags, ec);
        }

        /*!
         * @brief Start an asynchronous write using an
         * awaitable.
         *
         * This function is used to asynchronously write all
         * provided data to the stream socket. It is an
         * initiating function for an asynchronous operation,
         * and returns an awaitable object. To start the write
         * operation and wait for its completion, await the
         * returned awaitable.
         * @tparam Buffers The buffers sequence type.
         * @param buffers One or more data buffers to be written
         * to the socket. Although the buffers object may be
         * copied as necessary, ownership of the underlying
         * memory blocks is retained by the caller, which must
         * guarantee that they remain valid until the completion
         * handler is called.
         * @param ec If this is a reference to no_ec, then it is
         * ignored and errors are reported with exceptions.
         * Otherwise it is set to indicate what error occurred,
         * if any.
         * @return Awaitable object, that is when awaited will
         * start the write operation and wait for its
         * completion. The result of the awaitable is the number
         * of bytes written. On error the number of bytes
         * returned from the awaitable will be 0.
         */
        template <BufferSequence Buffers>
        auto async_write(Buffers&& buffers, std::error_code& ec) {
            return async_write(std::forward<Buffers>(buffers),
                               transfer_flags::none, ec);
        }

        /*!
         * @brief Start an asynchronous send using an awaitable.
         *
         * This function is used to asynchronously send all
         * provided data on the stream socket. It is an
         * initiating function for an asynchronous operation,
         * and returns an awaitable object. To start the send
         * operation and wait for its completion, await the
         * returned awaitable.
         * @tparam Buffers The buffers sequence type.
         * @param buffers One or more data buffers to be sent on
         * the socket. Although the buffers object may be copied
         * as necessary, ownership of the underlying memory
         * blocks is retained by the caller, which must
         * guarantee that they remain valid until the completion
         * handler is called.
         * @param flags Flags specifying how the send call is to
         * be made.
         * @param ec If this is a reference to no_ec, then it is
         * ignored and errors are reported with exceptions.
         * Otherwise it is set to indicate what error occurred,
         * if any.
         * @return Awaitable object, that is when awaited will
         * start the send operation and wait for its completion.
         * The result of the awaitable is the number of bytes
         * sent. On error the number of bytes returned from the
         * awaitable will be 0.
         */
        template <BufferSequence Buffers>
        auto async_send(Buffers&& buffers,
                        transfer_flags flags = transfer_flags::none,
                        std::error_code& ec = no_ec) {
            return async_write(std::forward<Buffers>(buffers), flags, ec);
        }

        /*!
         * @brief Start an asynchronous send using an awaitable.
         *
         * This function is used to asynchronously send all
         * provided data on the stream socket. It is an
         * initiating function for an asynchronous operation,
         * and returns an awaitable object. To start the send
         * operation and wait for its completion, await the
         * returned awaitable.
         * @tparam Buffers The buffers sequence type.
         * @param buffers One or more data buffers to be sent on
         * the socket. Although the buffers object may be copied
         * as necessary, ownership of the underlying memory
         * blocks is retained by the caller, which must
         * guarantee that they remain valid until the completion
         * handler is called.
         * @param ec If this is a reference to no_ec, then it is
         * ignored and errors are reported with exceptions.
         * Otherwise it is set to indicate what error occurred,
         * if any.
         * @return Awaitable object, that is when awaited will
         * start the send operation and wait for its completion.
         * The result of the awaitable is the number of bytes
         * sent. On error the number of bytes returned from the
         * awaitable will be 0.
         */
        template <BufferSequence Buffers>
        auto async_send(Buffers&& buffers, std::error_code& ec) {
            return async_write(std::forward<Buffers>(buffers), ec);
        }

        /*!
         * @brief Start an asynchronous read using an awaitable.
         *
         * This function is used to asynchronously read data
         * from the stream socket. It is an initiating function
         * for an asynchronous operation, and returns an
         * awaitable object. To start the read operation and
         * wait for its completion, await the returned
         * awaitable. The read operation may not read all of the
         * requested number of bytes. Consider using the
         * async_read method if you need to ensure that the
         * requested amount of data is read before the
         * asynchronous operation completes.
         * @tparam Buffers The buffers sequence type.
         * @param buffers One or more buffers into which the
         * data will be read. Although the buffers object may be
         * copied as necessary, ownership of the underlying
         * memory blocks is retained by the caller, which must
         * guarantee that they remain valid until the completion
         * handler is called.
         * @param flags Flags specifying how the read call is to
         * be made.
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
        template <MutableBufferSequence Buffers>
        auto async_read_some(Buffers&& buffers,
                             transfer_flags flags = transfer_flags::none,
                             std::error_code& ec = no_ec) {
            bool not_zero =
                protocol_type::is_stream_protocol && not_empty_buffers(buffers);
            return impl.async_read(std::forward<Buffers>(buffers), not_zero,
                                   flags, ec);
        }

        /*!
         * @brief Start an asynchronous read using an awaitable.
         *
         * This function is used to asynchronously read data
         * from the stream socket. It is an initiating function
         * for an asynchronous operation, and returns an
         * awaitable object. To start the read operation and
         * wait for its completion, await the returned
         * awaitable. The read operation may not read all of the
         * requested number of bytes. Consider using the
         * async_read method if you need to ensure that the
         * requested amount of data is read before the
         * asynchronous operation completes.
         * @tparam Buffers The buffers sequence type.
         * @param buffers One or more buffers into which the
         * data will be read. Although the buffers object may be
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
        template <MutableBufferSequence Buffers>
        auto async_read_some(Buffers&& buffers, std::error_code& ec) {
            return async_read_some(std::forward<Buffers>(buffers),
                                   transfer_flags::none, ec);
        }

        /*!
         * @brief Start an asynchronous receive using an
         * awaitable.
         *
         * This function is used to asynchronously receive data
         * from the stream socket. It is an initiating function
         * for an asynchronous operation, and returns an
         * awaitable object. To start the receive operation and
         * wait for its completion, await the returned
         * awaitable. The receive operation may not receive all
         * of the requested number of bytes. Consider using the
         * async_read method if you need to ensure that the
         * requested amount of data is read before the
         * asynchronous operation completes.
         * @tparam Buffers The buffers sequence type.
         * @param buffers One or more buffers into which the
         * data will be received. Although the buffers object
         * may be copied as necessary, ownership of the
         * underlying memory blocks is retained by the caller,
         * which must guarantee that they remain valid until the
         * completion handler is called.
         * @param flags Flags specifying how the receive call is
         * to be made.
         * @param ec If this is a reference to no_ec, then it is
         * ignored and errors are reported with exceptions.
         * Otherwise it is set to indicate what error occurred,
         * if any.
         * @return Awaitable object, that is when awaited will
         * start the receive operation and wait for its
         * completion. The result of the awaitable is the number
         * of bytes received. On error, or if size of @p buffers
         * is 0, the number of bytes returned from the awaitable
         * will be 0.
         */
        template <MutableBufferSequence Buffers>
        auto async_receive(Buffers&& buffers,
                           transfer_flags flags = transfer_flags::none,
                           std::error_code& ec = no_ec) {
            return async_read_some(std::forward<Buffers>(buffers), flags, ec);
        }

        /*!
         * @brief Start an asynchronous receive using an
         * awaitable.
         *
         * This function is used to asynchronously receive data
         * from the stream socket. It is an initiating function
         * for an asynchronous operation, and returns an
         * awaitable object. To start the receive operation and
         * wait for its completion, await the returned
         * awaitable. The receive operation may not receive all
         * of the requested number of bytes. Consider using the
         * async_read method if you need to ensure that the
         * requested amount of data is read before the
         * asynchronous operation completes.
         * @tparam Buffers The buffers sequence type.
         * @param buffers One or more buffers into which the
         * data will be received. Although the buffers object
         * may be copied as necessary, ownership of the
         * underlying memory blocks is retained by the caller,
         * which must guarantee that they remain valid until the
         * completion handler is called.
         * @param ec If this is a reference to no_ec, then it is
         * ignored and errors are reported with exceptions.
         * Otherwise it is set to indicate what error occurred,
         * if any.
         * @return Awaitable object, that is when awaited will
         * start the receive operation and wait for its
         * completion. The result of the awaitable is the number
         * of bytes received. On error, or if size of @p buffers
         * is 0, the number of bytes returned from the awaitable
         * will be 0.
         */
        template <MutableBufferSequence Buffers>
        auto async_receive(Buffers&& buffers, std::error_code& ec) {
            return async_read_some(std::forward<Buffers>(buffers), ec);
        }

        /*!
         * @brief Start an asynchronous read using an awaitable.
         *
         * This function is used to asynchronously read data
         * from the stream socket. It is an initiating function
         * for an asynchronous operation, and returns an
         * awaitable object. To start the read operation and
         * wait for its completion, await the returned
         * awaitable. The read operation will not complete
         * before reading all of the requested number of bytes,
         * or an error occurs.
         * @tparam Buffers The buffers sequence type.
         * @param buffers One or more buffers into which the
         * data will be read. Although the buffers object may be
         * copied as necessary, ownership of the underlying
         * memory blocks is retained by the caller, which must
         * guarantee that they remain valid until the completion
         * handler is called.
         * @param flags Flags specifying how the read call is to
         * be made.
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
        template <MutableBufferSequence Buffers>
        auto async_read(Buffers&& buffers,
                        transfer_flags flags = transfer_flags::wait_all,
                        std::error_code& ec = no_ec) {
            return async_read_some(std::forward<Buffers>(buffers),
                                   flags | transfer_flags::wait_all, ec);
        }

        /*!
         * @brief Start an asynchronous read using an awaitable.
         *
         * This function is used to asynchronously read data
         * from the stream socket. It is an initiating function
         * for an asynchronous operation, and returns an
         * awaitable object. To start the read operation and
         * wait for its completion, await the returned
         * awaitable. The read operation will not complete
         * before reading all of the requested number of bytes,
         * or an error occurs.
         * @tparam Buffers The buffers sequence type.
         * @param buffers One or more buffers into which the
         * data will be read. Although the buffers object may be
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
        template <MutableBufferSequence Buffers>
        auto async_read(Buffers&& buffers, std::error_code& ec) {
            return async_read_some(std::forward<Buffers>(buffers),
                                   transfer_flags::wait_all, ec);
        }

        /*!
         * @brief Start an asynchronous send using an awaitable.
         *
         * This function is used to asynchronously send a
         * datagram to the specified remote endpoint. It is an
         * initiating function for an asynchronous operation,
         * and returns an awaitable object. To start the send
         * operation and wait for its completion, await the
         * returned awaitable.
         * @tparam Buffers The buffers sequence type.
         * @param buffers One or more data buffers to be sent on
         * the socket. Although the buffers object may be copied
         * as necessary, ownership of the underlying memory
         * blocks is retained by the caller, which must
         * guarantee that they remain valid until the completion
         * handler is called.
         * @param receiver The remote endpoint to which the data
         * will be sent. Copies will be made of the endpoint as
         * required.
         * @param flags Flags specifying how the send call is to
         * be made.
         * @param ec If this is a reference to no_ec, then it is
         * ignored and errors are reported with exceptions.
         * Otherwise it is set to indicate what error occurred,
         * if any.
         * @return Awaitable object, that is when awaited will
         * start the send operation and wait for its completion.
         * The result of the awaitable is the number of bytes
         * sent. On error the number of bytes returned from the
         * awaitable will be 0.
         */
        template <BufferSequence Buffers>
        auto async_send_to(Buffers&& buffers, const endpoint_type& receiver,
                           transfer_flags flags = transfer_flags::none,
                           std::error_code& ec = no_ec) {
            return impl.async_send_to(std::forward<Buffers>(buffers), flags,
                                      receiver, ec);
        }

        /*!
         * @brief Start an asynchronous send using an awaitable.
         *
         * This function is used to asynchronously send a
         * datagram to the specified remote endpoint. It is an
         * initiating function for an asynchronous operation,
         * and returns an awaitable object. To start the send
         * operation and wait for its completion, await the
         * returned awaitable.
         * @tparam Buffers The buffers sequence type.
         * @param buffers One or more data buffers to be sent on
         * the socket. Although the buffers object may be copied
         * as necessary, ownership of the underlying memory
         * blocks is retained by the caller, which must
         * guarantee that they remain valid until the completion
         * handler is called.
         * @param receiver The remote endpoint to which the data
         * will be sent. Copies will be made of the endpoint as
         * required.
         * @param ec If this is a reference to no_ec, then it is
         * ignored and errors are reported with exceptions.
         * Otherwise it is set to indicate what error occurred,
         * if any.
         * @return Awaitable object, that is when awaited will
         * start the send operation and wait for its completion.
         * The result of the awaitable is the number of bytes
         * sent. On error the number of bytes returned from the
         * awaitable will be 0.
         */
        template <BufferSequence Buffers>
        auto async_send_to(Buffers&& buffers, const endpoint_type& receiver,
                           std::error_code& ec) {
            return async_send_to(std::forward<Buffers>(buffers), receiver,
                                 transfer_flags::none, ec);
        }

        /*!
         * @brief Start an asynchronous receive using an
         * awaitable.
         *
         * This function is used to asynchronously receive a
         * datagram. It is an initiating function for an
         * asynchronous operation, and returns an awaitable
         * object. To start the receive operation and wait for
         * its completion, await the returned awaitable.
         * @tparam Buffers The buffers sequence type.
         * @param buffers One or more buffers into which the
         * data will be received. Although the buffers object
         * may be copied as necessary, ownership of the
         * underlying memory blocks is retained by the caller,
         * which must guarantee that they remain valid until the
         * completion handler is called.
         * @param sender An endpoint object that receives the
         * endpoint of the remote sender of the datagram.
         * Ownership of the sender_endpoint object is retained
         * by the caller, which must guarantee that it is valid
         * until the handler is called.
         * @param flags Flags specifying how the receive call is
         * to be made.
         * @param ec If this is a reference to no_ec, then it is
         * ignored and errors are reported with exceptions.
         * Otherwise it is set to indicate what error occurred,
         * if any.
         * @return Awaitable object, that is when awaited will
         * start the receive operation and wait for its
         * completion. The result of the awaitable is the number
         * of bytes received. On error, or if size of @p buffers
         * is 0, the number of bytes returned from the awaitable
         * will be 0.
         */
        template <MutableBufferSequence Buffers>
        auto async_receive_from(Buffers&& buffers, endpoint_type& sender,
                                transfer_flags flags = transfer_flags::none,
                                std::error_code& ec = no_ec) {
            flags &= ~transfer_flags::wait_all;
            return impl.async_receive_from(std::forward<Buffers>(buffers),
                                           flags, sender, ec);
        }

        /*!
         * @brief Start an asynchronous receive using an
         * awaitable.
         *
         * This function is used to asynchronously receive a
         * datagram. It is an initiating function for an
         * asynchronous operation, and returns an awaitable
         * object. To start the receive operation and wait for
         * its completion, await the returned awaitable.
         * @tparam Buffers The buffers sequence type.
         * @param buffers One or more buffers into which the
         * data will be received. Although the buffers object
         * may be copied as necessary, ownership of the
         * underlying memory blocks is retained by the caller,
         * which must guarantee that they remain valid until the
         * completion handler is called.
         * @param sender An endpoint object that receives the
         * endpoint of the remote sender of the datagram.
         * Ownership of the sender_endpoint object is retained
         * by the caller, which must guarantee that it is valid
         * until the handler is called.
         * @param ec If this is a reference to no_ec, then it is
         * ignored and errors are reported with exceptions.
         * Otherwise it is set to indicate what error occurred,
         * if any.
         * @return Awaitable object, that is when awaited will
         * start the receive operation and wait for its
         * completion. The result of the awaitable is the number
         * of bytes received. On error, or if size of @p buffers
         * is 0, the number of bytes returned from the awaitable
         * will be 0.
         */
        template <MutableBufferSequence Buffers>
        auto async_receive_from(Buffers&& buffers, endpoint_type& sender,
                                std::error_code& ec) {
            return async_receive_from(std::forward<Buffers>(buffers), sender,
                                      transfer_flags::none, ec);
        }

        /*!
         * @brief Start an asynchronous write.
         *
         * This function is used to asynchronously write all
         * provided data to the stream socket. It is an
         * initiating function for an asynchronous operation,
         * and always returns immediately.
         * @tparam Buffers The buffers sequence type.
         * @tparam Handler The type of handler.
         * @tparam Alloc The type of allocator.
         * @param buffers One or more data buffers to be written
         * to the socket. Although the buffers object may be
         * copied as necessary, ownership of the underlying
         * memory blocks is retained by the caller, which must
         * guarantee that they remain valid until the completion
         * handler is called.
         * @param handler The handler that will be invoked with
         * the result of the write operation when it is
         * finished. It must be callable with:
         * `handler(std::size_t{}, std::error_code{})`.
         * @param flags Flags specifying how the write call is
         * to be made.
         * @param alloc The allocator used to allocate memory
         * for the operation.
         */
        template <BufferSequence Buffers, WriteHandler Handler,
                  HandlerAllocator Alloc = default_io_allocator>
        void async_write(Buffers&& buffers, Handler&& handler,
                         transfer_flags flags, const Alloc& alloc = Alloc()) {
            impl.async_write(std::forward<Buffers>(buffers),
                             std::forward<Handler>(handler), flags, alloc);
        }

        /*!
         * @brief Start an asynchronous write.
         *
         * This function is used to asynchronously write all
         * provided data to the stream socket. It is an
         * initiating function for an asynchronous operation,
         * and always returns immediately.
         * @tparam Buffers The buffers sequence type.
         * @tparam Handler The type of handler.
         * @tparam Alloc The type of allocator.
         * @param buffers One or more data buffers to be written
         * to the socket. Although the buffers object may be
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
        template <BufferSequence Buffers, WriteHandler Handler,
                  HandlerAllocator Alloc = default_io_allocator>
        void async_write(Buffers&& buffers, Handler&& handler,
                         const Alloc& alloc = Alloc()) {
            async_write(std::forward<Buffers>(buffers),
                        std::forward<Handler>(handler), transfer_flags::none,
                        alloc);
        }

        /*!
         * @brief Start an asynchronous send.
         *
         * This function is used to asynchronously send a
         * datagram to the specified remote endpoint. It is an
         * initiating function for an asynchronous operation,
         * and always returns immediately.
         * @tparam Buffers The buffers sequence type.
         * @tparam Handler The type of handler.
         * @tparam Alloc The type of allocator.
         * @param buffers One or more data buffers to be sent to
         * the remote endpoint. Although the buffers object may
         * be copied as necessary, ownership of the underlying
         * memory blocks is retained by the caller, which must
         * guarantee that they remain valid until the completion
         * handler is called.
         * @param receiver The remote endpoint to which the data
         * will be sent. Copies will be made of the endpoint as
         * required.
         * @param flags Flags specifying how the send call is to
         * be made.
         * @param handler The handler that will be invoked with
         * the result of the send operation when it is finished.
         * It must be callable with: `handler(std::size_t{},
         * std::error_code{})`.
         * @param alloc The allocator used to allocate memory
         * for the operation.
         */
        template <BufferSequence Buffers, WriteHandler Handler,
                  HandlerAllocator Alloc = default_io_allocator>
        void async_send_to(Buffers&& buffers, const endpoint_type& receiver,
                           transfer_flags flags, Handler&& handler,
                           const Alloc& alloc = Alloc()) {
            impl.async_send_to(std::forward<Buffers>(buffers),
                               std::forward<Handler>(handler), receiver, flags,
                               alloc);
        }

        /*!
         * @brief Start an asynchronous send.
         *
         * This function is used to asynchronously send a
         * datagram to the specified remote endpoint. It is an
         * initiating function for an asynchronous operation,
         * and always returns immediately.
         * @tparam Buffers The buffers sequence type.
         * @tparam Handler The type of handler.
         * @tparam Alloc The type of allocator.
         * @param buffers One or more data buffers to be sent to
         * the remote endpoint. Although the buffers object may
         * be copied as necessary, ownership of the underlying
         * memory blocks is retained by the caller, which must
         * guarantee that they remain valid until the completion
         * handler is called.
         * @param receiver The remote endpoint to which the data
         * will be sent. Copies will be made of the endpoint as
         * required.
         * @param handler The handler that will be invoked with
         * the result of the send operation when it is finished.
         * It must be callable with: `handler(std::size_t{},
         * std::error_code{})`.
         * @param alloc The allocator used to allocate memory
         * for the operation.
         */
        template <BufferSequence Buffers, WriteHandler Handler,
                  HandlerAllocator Alloc = default_io_allocator>
        void async_send_to(Buffers&& buffers, const endpoint_type& receiver,
                           Handler&& handler, const Alloc& alloc = Alloc()) {
            async_send_to(std::forward<Buffers>(buffers), receiver,
                          transfer_flags::none, std::forward<Handler>(handler),
                          alloc);
        }

        /*!
         * @brief Start an asynchronous read.
         *
         * This function is used to asynchronously read data
         * from the stream socket. It is an initiating
         * function for an asynchronous operation, and always
         * returns immediately. The read operation may not read
         * all of the requested number of bytes. Consider using
         * the async_read method if you need to ensure that the
         * requested amount of data is read before the
         * asynchronous operation completes.
         * @tparam Buffers The buffers sequence type.
         * @tparam Handler The type of handler.
         * @tparam Alloc The type of allocator.
         * @param buffers One or more buffers into which the
         * data will be read. Although the buffers object may be
         * copied as necessary, ownership of the underlying
         * memory blocks is retained by the caller, which must
         * guarantee that they remain valid until the completion
         * handler is called.
         * @param handler The handler that will be invoked with
         * the result of the read operation when it is finished.
         * It must be callable with: `handler(std::size_t{},
         * std::error_code{})`.
         * @param flags Flags specifying how the read call is to
         * be made.
         * @param alloc The allocator used to allocate memory
         * for the operation.
         */
        template <MutableBufferSequence Buffers, ReadHandler Handler,
                  HandlerAllocator Alloc = default_io_allocator>
        void async_read_some(Buffers&& buffers, Handler&& handler,
                             transfer_flags flags,
                             const Alloc& alloc = Alloc()) {
            bool not_zero =
                protocol_type::is_stream_protocol && not_empty_buffers(buffers);
            impl.async_read(std::forward<Buffers>(buffers),
                            std::forward<Handler>(handler), not_zero, flags,
                            alloc);
        }

        /*!
         * @brief Start an asynchronous read.
         *
         * This function is used to asynchronously read data
         * from the stream socket. It is an initiating
         * function for an asynchronous operation, and always
         * returns immediately. The read operation may not read
         * all of the requested number of bytes. Consider using
         * the async_read method if you need to ensure that the
         * requested amount of data is read before the
         * asynchronous operation completes.
         * @tparam Buffers The buffers sequence type.
         * @tparam Handler The type of handler.
         * @tparam Alloc The type of allocator.
         * @param buffers One or more buffers into which the
         * data will be read. Although the buffers object may be
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
        template <MutableBufferSequence Buffers, ReadHandler Handler,
                  HandlerAllocator Alloc = default_io_allocator>
        void async_read_some(Buffers&& buffers, Handler&& handler,
                             const Alloc& alloc = Alloc()) {
            async_read_some(std::forward<Buffers>(buffers),
                            std::forward<Handler>(handler),
                            transfer_flags::none, alloc);
        }

        /*!
         * @brief Start an asynchronous read.
         *
         * This function is used to asynchronously read data
         * from the stream socket. It is an initiating
         * function for an asynchronous operation, and always
         * returns immediately. The read operation may not read
         * all of the requested number of bytes. Consider using
         * the async_read method if you need to ensure that the
         * requested amount of data is read before the
         * asynchronous operation completes.
         * @tparam Buffers The buffers sequence type.
         * @tparam Handler The type of handler.
         * @tparam Alloc The type of allocator.
         * @param buffers One or more buffers into which the
         * data will be read. Although the buffers object may be
         * copied as necessary, ownership of the underlying
         * memory blocks is retained by the caller, which must
         * guarantee that they remain valid until the completion
         * handler is called.
         * @param handler The handler that will be invoked with
         * the result of the read operation when it is finished.
         * It must be callable with: `handler(std::size_t{},
         * std::error_code{})`.
         * @param flags Flags specifying how the read call is to
         * be made.
         * @param alloc The allocator used to allocate memory
         * for the operation.
         */
        template <MutableBufferSequence Buffers, ReadHandler Handler,
                  HandlerAllocator Alloc = default_io_allocator>
        void async_receive(Buffers&& buffers, Handler&& handler,
                           transfer_flags flags, const Alloc& alloc = Alloc()) {
            async_read(std::forward<Buffers>(buffers),
                       std::forward<Handler>(handler), flags, alloc);
        }

        /*!
         * @brief Start an asynchronous read.
         *
         * This function is used to asynchronously read data
         * from the stream socket. It is an initiating
         * function for an asynchronous operation, and always
         * returns immediately. The read operation may not read
         * all of the requested number of bytes. Consider using
         * the async_read method if you need to ensure that the
         * requested amount of data is read before the
         * asynchronous operation completes.
         * @tparam Buffers The buffers sequence type.
         * @tparam Handler The type of handler.
         * @tparam Alloc The type of allocator.
         * @param buffers One or more buffers into which the
         * data will be read. Although the buffers object may be
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
        template <MutableBufferSequence Buffers, ReadHandler Handler,
                  HandlerAllocator Alloc = default_io_allocator>
        void async_receive(Buffers&& buffers, Handler&& handler,
                           const Alloc& alloc = Alloc()) {
            async_read(std::forward<Buffers>(buffers),
                       std::forward<Handler>(handler), transfer_flags::none,
                       alloc);
        }

        /*!
         * @brief Start an asynchronous read.
         *
         * This function is used to asynchronously read data
         * from the stream socket. It is an initiating
         * function for an asynchronous operation, and always
         * returns immediately. The read operation will not
         * complete before reading all of the requested number
         * of bytes, or an error occurs.
         * @tparam Buffers The buffers sequence type.
         * @tparam Handler The type of handler.
         * @tparam Alloc The type of allocator.
         * @param buffers One or more buffers into which the
         * data will be read. Although the buffers object may be
         * copied as necessary, ownership of the underlying
         * memory blocks is retained by the caller, which must
         * guarantee that they remain valid until the completion
         * handler is called.
         * @param handler The handler that will be invoked with
         * the result of the read operation when it is finished.
         * It must be callable with: `handler(std::size_t{},
         * std::error_code{})`.
         * @param flags Flags specifying how the read call is to
         * be made.
         * @param alloc The allocator used to allocate memory
         * for the operation.
         */
        template <MutableBufferSequence Buffers, ReadHandler Handler,
                  HandlerAllocator Alloc = default_io_allocator>
        void async_read(Buffers&& buffers, Handler&& handler,
                        transfer_flags flags, const Alloc& alloc = Alloc()) {
            async_read_some(std::forward<Buffers>(buffers),
                            std::forward<Handler>(handler),
                            flags | transfer_flags::wait_all, alloc);
        }

        /*!
         * @brief Start an asynchronous read.
         *
         * This function is used to asynchronously read data
         * from the stream socket. It is an initiating
         * function for an asynchronous operation, and always
         * returns immediately. The read operation will not
         * complete before reading all of the requested number
         * of bytes, or an error occurs.
         * @tparam Buffers The buffers sequence type.
         * @tparam Handler The type of handler.
         * @tparam Alloc The type of allocator.
         * @param buffers One or more buffers into which the
         * data will be read. Although the buffers object may be
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
        template <MutableBufferSequence Buffers, ReadHandler Handler,
                  HandlerAllocator Alloc = default_io_allocator>
        void async_read(Buffers&& buffers, Handler&& handler,
                        const Alloc& alloc = Alloc()) {
            async_read_some(std::forward<Buffers>(buffers),
                            std::forward<Handler>(handler),
                            transfer_flags::wait_all, alloc);
        }

        /*!
         * @brief Start an asynchronous receive.
         *
         * This function is used to asynchronously receive a
         * datagram. It is an initiating function for an
         * asynchronous operation, and always returns
         * immediately.
         * @tparam Buffers The buffers sequence type.
         * @tparam Handler The type of handler.
         * @tparam Alloc The type of allocator.
         * @param buffers One or more buffers into which the
         * data will be received. Although the buffers object
         * may be copied as necessary, ownership of the
         * underlying memory blocks is retained by the caller,
         * which must guarantee that they remain valid until the
         * completion handler is called.
         * @param peer An endpoint object that receives the
         * endpoint of the remote sender of the datagram.
         * Ownership of the sender_endpoint object is retained
         * by the caller, which must guarantee that it is valid
         * until the handler is called.
         * @param flags Flags specifying how the receive call is
         * to be made.
         * @param handler The handler that will be invoked with
         * the result of the receive operation when it is
         * finished. It must be callable with:
         * `handler(std::size_t{}, std::error_code{})`.
         * @param alloc The allocator used to allocate memory
         * for the operation.
         */
        template <MutableBufferSequence Buffers, ReadHandler Handler,
                  HandlerAllocator Alloc = default_io_allocator>
        void async_receive_from(Buffers&& buffers, endpoint_type& peer,
                                transfer_flags flags, Handler&& handler,
                                const Alloc& alloc = Alloc()) {
            flags &= ~transfer_flags::wait_all;
            impl.async_receive_from(std::forward<Buffers>(buffers),
                                    std::forward<Handler>(handler), peer, flags,
                                    alloc);
        }

        /*!
         * @brief Start an asynchronous receive.
         *
         * This function is used to asynchronously receive a
         * datagram. It is an initiating function for an
         * asynchronous operation, and always returns
         * immediately.
         * @tparam Buffers The buffers sequence type.
         * @tparam Handler The type of handler.
         * @tparam Alloc The type of allocator.
         * @param buffers One or more buffers into which the
         * data will be received. Although the buffers object
         * may be copied as necessary, ownership of the
         * underlying memory blocks is retained by the caller,
         * which must guarantee that they remain valid until the
         * completion handler is called.
         * @param peer An endpoint object that receives the
         * endpoint of the remote sender of the datagram.
         * Ownership of the sender_endpoint object is retained
         * by the caller, which must guarantee that it is valid
         * until the handler is called.
         * @param handler The handler that will be invoked with
         * the result of the receive operation when it is
         * finished. It must be callable with:
         * `handler(std::size_t{}, std::error_code{})`.
         * @param alloc The allocator used to allocate memory
         * for the operation.
         */
        template <MutableBufferSequence Buffers, ReadHandler Handler,
                  HandlerAllocator Alloc = default_io_allocator>
        void async_receive_from(Buffers&& buffers, endpoint_type& peer,
                                Handler&& handler,
                                const Alloc& alloc = Alloc()) {
            async_receive_from(std::forward<Buffers>(buffers), peer,
                               transfer_flags::none,
                               std::forward<Handler>(handler), alloc);
        }

    protected:
        impl_type impl;
    };

    /*!
     * @brief Provides stream-oriented socket functionality.
     * @tparam Protocol The protocol type which must be stream protocol like
     * tcp.
     */
    template <class Protocol>
    class async_stream_socket : async_socket_base<Protocol> {
        using base = async_socket_base<Protocol>;

        using typename base::impl_type;

        using base::impl;

        template <typename, typename>
        friend struct rad::rebind_executor_helper;

        void rebind_to_executor(io_executor& ex) noexcept {
            impl.rebind_to_executor(ex);
        }

    public:
        /// The next layer type (async_stream_socket).
        using next_layer_type = async_stream_socket;
        /// The lowest layer type (async_stream_socket).
        using lowest_layer_type = async_stream_socket;

        using typename base::endpoint_type;
        using typename base::executor_type;
        using typename base::native_fd_type;
        using typename base::native_handle_type;
        using typename base::size_type;

        // msvc bug:
        // https://developercommunity.visualstudio.com/t/C-concepts-cant-access-inherited-nest/10965243

        /// The protocol type.
        using protocol_type = typename base::protocol_type;

        using base::async_read;
        using base::async_read_some;
        using base::async_receive;
        using base::async_send;
        using base::async_write;
        using base::bind;
        using base::cancel;
        using base::close;
        using base::connect;
        using base::executor;
        using base::get_option;
        using base::is_open;
        using base::local_endpoint;
        using base::native_fd;
        using base::native_handle;
        using base::non_blocking;
        using base::open;
        using base::read;
        using base::read_some;
        using base::receive;
        using base::remote_endpoint;
        using base::send;
        using base::set_option;
        using base::shutdown;
        using base::write;

        /*!
         * @brief Construct an async_stream_socket without
         * opening it.
         *
         * This constructor creates a stream socket without
         * opening it. The socket needs to be opened and then
         * connected or accepted before data can be sent or
         * received on it.
         * @param ex The I/O executor that the stream socket
         * will use to dispatch handlers for any asynchronous
         * operations performed on the socket.
         */
        explicit async_stream_socket(executor_type& ex) noexcept : base(ex) {
        }

        /*!
         * @brief Construct an async_stream_socket on an
         * existing native socket. The native socket must be
         * open and not have been attached to any executors.
         *
         * This constructor creates a stream socket object to
         * hold an existing native socket.
         * @param ex The I/O executor that the stream socket
         * will use to dispatch handlers for any asynchronous
         * operations performed on the socket.
         * @param sock_fd The new underlying socket
         * implementation.
         */
        async_stream_socket(executor_type& ex, socket_fd_t sock_fd)
            : base(ex, sock_fd) {
        }

        /*!
         * @brief Construct an async_stream_socket on an
         * existing native socket. The native socket must be
         * open and not have been attached to any executors.
         *
         * This constructor creates a stream socket object to
         * hold an existing native socket.
         * @param ex The I/O executor that the stream socket
         * will use to dispatch handlers for any asynchronous
         * operations performed on the socket.
         * @param sock_fd The new underlying socket
         * implementation.
         */
        async_stream_socket(executor_type& ex, native_handle_type& sock_fd)
            : base(ex, sock_fd) {
        }

        /*!
         * @brief Construct and open an async_stream_socket.
         *
         * This constructor creates and opens a stream socket.
         * The socket needs to be connected or accepted before
         * data can be sent or received on it.
         * @param ex The I/O executor that the stream socket
         * will use to dispatch handlers for any asynchronous
         * operations performed on the socket.
         * @param protocol An object specifying protocol
         * parameters to be used.
         */
        async_stream_socket(executor_type& ex, const protocol_type& protocol)
            : base(ex, protocol) {
        }

        /*!
         * @brief Construct an async_stream_socket, opening it
         * and binding it to the given local endpoint.
         *
         * This constructor creates a stream socket and
         * automatically opens it bound to the specified
         * endpoint on the local machine. The protocol used is
         * the
         * @p protocol passed to the constructor.
         * @param ex The I/O executor that the stream socket
         * will use to dispatch handlers for any asynchronous
         * operations performed on the socket.
         * @param protocol An object specifying protocol
         * parameters to be used.
         * @param bind_addr An endpoint on the local machine to
         * which the stream socket will be bound.
         */
        async_stream_socket(executor_type& ex, const protocol_type& protocol,
                            const endpoint_type& bind_addr)
            : base(ex, protocol, bind_addr) {
        }

        /*!
         * @brief Construct an async_stream_socket, opening it
         * and binding it to the given local endpoint.
         *
         * This constructor creates a stream socket and
         * automatically opens it bound to the specified
         * endpoint on the local machine. The protocol used is
         * the protocol associated with the given endpoint.
         * @param ex The I/O executor that the stream socket
         * will use to dispatch handlers for any asynchronous
         * operations performed on the socket.
         * @param bind_addr An endpoint on the local machine to
         * which the stream socket will be bound.
         */
        async_stream_socket(executor_type& ex, const endpoint_type& bind_addr)
            : base(ex, protocol_type{bind_addr.family()}, bind_addr) {
        }

        template <class AllocatorTypes>
        static constexpr std::size_t max_allocator_size() noexcept {
            constexpr std::size_t sizes[] = {
                base::template max_allocator_size<AllocatorTypes>(),
                impl_type::template connect_allocator_size<AllocatorTypes>(),
                impl_type::template connect_range_allocator_size<
                    AllocatorTypes>()};
            return max_of(sizes);
        }

        /*!
         * @brief Get a reference to the next layer.
         *
         * This function returns a reference to the next layer
         * in a stack of layers. Since an async_stream_socket
         * cannot contain any further layers, it simply returns
         * a reference to itself.
         * @return A reference to the next layer in the stack of
         * layers. Ownership is not transferred to the caller.
         */
        next_layer_type& next_layer() noexcept {
            return *this;
        }

        /*!
         * @brief Get a const reference to the next layer.
         *
         * This function returns a reference to the next layer
         * in a stack of layers. Since an async_stream_socket
         * cannot contain any further layers, it simply returns
         * a reference to itself.
         * @return A const reference to the next layer in the
         * stack of layers. Ownership is not transferred to the
         * caller.
         */
        const next_layer_type& next_layer() const noexcept {
            return *this;
        }

        /*!
         * @brief Get a reference to the lowest layer.
         *
         * This function returns a reference to the lowest layer
         * in a stack of layers. Since an async_stream_socket
         * cannot contain any further layers, it simply returns
         * a reference to itself.
         * @return A reference to the lowest layer in the stack
         * of layers. Ownership is not transferred to the
         * caller.
         */
        lowest_layer_type& lowest_layer() noexcept {
            return *this;
        }

        /*!
         * @brief Get a const reference to the lowest layer.
         *
         * This function returns a reference to the lowest layer
         * in a stack of layers. Since an async_stream_socket
         * cannot contain any further layers, it simply returns
         * a reference to itself.
         * @return A const reference to the lowest layer in the
         * stack of layers. Ownership is not transferred to the
         * caller.
         */
        const lowest_layer_type& lowest_layer() const noexcept {
            return *this;
        }

        /*!
         * @brief Async connect the socket to a remote endpoint.
         * Note the connect operation will not start until the
         * returned awaiter is awaited. Also note that the
         * coroutine may be suspened and resumed later on the
         * socket executor if the operation requires waiting.
         * But if the operation completes immediately the
         * coroutine will not suspend
         * @param remote_address the remote endpoint to connect
         * the socket to
         * @param ec if set to no_ec then errors are reported
         * via exceptions, otherwise errors are reported via ec
         * @return an awaiter that is when awaited will start
         * the connect operation
         */
        auto async_connect(const endpoint_type& remote_address,
                           std::error_code& ec = no_ec) {
            return impl.template async_connect<Protocol>(remote_address, ec);
        }

        /*!
         * @brief Async connect the socket to a range of remote
         * endpoints. Note the connect operation will not start
         * until the returned awaiter is awaited. Also note that
         * the coroutine may be suspened and resumed later on
         * the socket executor if the operation requires
         * waiting. But if the operation completes immediately
         * the coroutine will not suspend
         * @param endpoints a range of remote endpoints to try
         * to connect the socket to in order one by one
         * @param ec if set to no_ec then errors are reported
         * via exceptions, otherwise errors are reported via ec
         * @return an awaiter that is when awaited will start
         * the connect operation and result in the endpoint the
         * socket was connected to. If errors are reported via
         * ec and the operation fails then the returned endpoint
         * is default constructed
         */
        template <EndpointSequence<endpoint_type> EndpointRange>
        auto async_connect(const EndpointRange& endpoints,
                           std::error_code& ec = no_ec) {
            return impl.template async_connect<Protocol, EndpointRange>(
                endpoints, ec);
        }

        /*!
         * @brief Async connect the socket to a remote endpoint
         * and call the handler when the operation is done
         * @param peer the remote endpoint to connect the socket
         * to
         * @param handler a handler to invoke when the operation
         * is done and will be passed an error_code that
         * determines whether the operation has succeeded or
         * failed. The handler must be either copyable or
         * movable and the following expression must be valid:
         * handler(std::error_code{})
         * @param alloc An allocator used to allocate the
         * operation. The size passed to its allocate method is
         * in bytes and the alignment of the allocator must be
         * suitable for std::max_align_t
         */
        template <ConnectHandler Handler,
                  HandlerAllocator Alloc = default_io_allocator>
        void async_connect(const endpoint_type& peer, Handler&& handler,
                           const Alloc& alloc = Alloc()) {
            impl.template async_connect<Protocol>(
                peer, std::forward<Handler>(handler), alloc);
        }

        /*!
         * @brief Async connect the socket to a range of remotes
         * endpoint and call the handler when the operation is
         * done
         * @param addrs the remote endpoints range to try to
         * connect the socket to in order one by one
         * @param handler a handler to invoke when the operation
         * is done and will be passed an error_code that
         * determines whether the operation has succeeded or
         * failed and the connected to endpoint on success or a
         * default constructed one on failure. The handler must
         * be either copyable or movable and the following
         * expression must be valid: handler(std::error_code{},
         * endpoint_type{})
         * @param alloc An allocator used to allocate the
         * operation. The size passed to its allocate method is
         * in bytes and the alignment of the allocator must be
         * suitable for std::max_align_t
         */
        template <ConnectRangeHandler<endpoint_type> Handler,
                  EndpointSequence<endpoint_type> EndpointRange,
                  HandlerAllocator Alloc = default_io_allocator>
        void async_connect(EndpointRange&& addrs, Handler&& handler,
                           const Alloc& alloc = Alloc()) {
            impl.template async_connect_r<Protocol, EndpointRange>(
                std::forward<EndpointRange>(addrs),
                std::forward<Handler>(handler), alloc);
        }
    };

    /*!
     * @brief Provides datagram-oriented socket functionality.
     * @tparam Protocol The protocol type which must be datagram protocol
     * like udp.
     */
    template <class Protocol>
    class async_datagram_socket : async_socket_base<Protocol> {
        using base = async_socket_base<Protocol>;

        template <typename, typename>
        friend struct rad::rebind_executor_helper;

        using base::impl;

        void rebind_to_executor(io_executor& ex) noexcept {
            base::impl.rebind_to_executor(ex);
        }

    public:
        /// The next layer type (async_datagram_socket).
        using next_layer_type = async_datagram_socket;
        /// The lowest layer type (async_datagram_socket).
        using lowest_layer_type = async_datagram_socket;

        using typename base::endpoint_type;
        using typename base::executor_type;
        using typename base::native_fd_type;
        using typename base::native_handle_type;
        using typename base::size_type;

        // msvc bug:
        // https://developercommunity.visualstudio.com/t/C-concepts-cant-access-inherited-nest/10965243

        /// The protocol type.
        using protocol_type = typename base::protocol_type;

        using base::async_receive;
        using base::async_receive_from;
        using base::async_send;
        using base::async_send_to;
        using base::bind;
        using base::cancel;
        using base::close;
        using base::connect;
        using base::executor;
        using base::get_option;
        using base::is_open;
        using base::local_endpoint;
        using base::max_allocator_size;
        using base::native_fd;
        using base::native_handle;
        using base::non_blocking;
        using base::open;
        using base::receive;
        using base::receive_from;
        using base::remote_endpoint;
        using base::send;
        using base::send_to;
        using base::set_option;
        using base::shutdown;

        /*!
         * @brief Construct a closed udp socket and use the
         * provided executor to dispatch async operations
         * @param ex the executor to dispatch async operations
         * onto
         */
        explicit async_datagram_socket(executor_type& ex) noexcept : base(ex) {
        }

        /*!
         * @brief Construct an async_datagram_socket on an
         * existing native socket. The native socket must be
         * open and not have been attached to any executors.
         *
         * This constructor creates a datagram socket object to
         * hold an existing native socket.
         * @param ex The I/O executor that the datagram socket
         * will use to dispatch handlers for any asynchronous
         * operations performed on the socket.
         * @param sock_fd The new underlying socket
         * implementation.
         */
        async_datagram_socket(executor_type& ex, socket_fd_t sock_fd)
            : base(ex, sock_fd) {
        }

        /*!
         * @brief Construct an async_datagram_socket on an
         * existing native socket. The native socket must be
         * open and not have been attached to any executors.
         *
         * This constructor creates a datagram socket object to
         * hold an existing native socket.
         * @param ex The I/O executor that the datagram socket
         * will use to dispatch handlers for any asynchronous
         * operations performed on the socket.
         * @param sock_fd The new underlying socket
         * implementation.
         */
        async_datagram_socket(executor_type& ex, native_handle_type& sock_fd)
            : base(ex, sock_fd) {
        }

        /*!
         * @brief Construct udp socket and open it with the
         * provided protocol
         * @param ex the executor to dispatch async operations
         * onto
         * @param protocol the protocol to open the udp socket
         * with
         */
        async_datagram_socket(executor_type& ex, const protocol_type& protocol)
            : base(ex, protocol) {
        }

        /*!
         * @brief Construct udp socket, open it with the
         * provided protocol and bind it to the provided remote
         * address. Binding the udp socket to remote address
         * allows the socket to receive messages only from this
         * address
         * @param ex the executor to dispatch async operations
         * onto
         * @param protocol the protocol to open the udp socket
         * with
         * @param src_address the remote address to recieve
         * packets from
         */
        async_datagram_socket(executor_type& ex, const protocol_type& protocol,
                              const endpoint_type& src_address)
            : base(ex, protocol, src_address) {
        }

        /*!
         * @brief Construct udp socket, open it with a protocol
         * whose family is the same as the provided address and
         * bind it to the provided remote address. Binding the
         * udp socket to remote address allows the socket to
         * receive messages only from this address
         * @param ex the executor to dispatch async operations
         * onto
         * @param protocol the protocol to open the udp socket
         * with
         * @param src_address the remote address to recieve
         * packets from
         */
        async_datagram_socket(executor_type& ex,
                              const endpoint_type& src_address)
            : base(ex, protocol_type{src_address.family()}, src_address) {
        }

        /*!
         * @brief Get a reference to the next layer.
         *
         * This function returns a reference to the next layer
         * in a stack of layers. Since an async_datagram_socket
         * cannot contain any further layers, it simply returns
         * a reference to itself.
         * @return A reference to the next layer in the stack of
         * layers. Ownership is not transferred to the caller.
         */
        next_layer_type& next_layer() noexcept {
            return *this;
        }

        /*!
         * @brief Get a const reference to the next layer.
         *
         * This function returns a reference to the next layer
         * in a stack of layers. Since an async_datagram_socket
         * cannot contain any further layers, it simply returns
         * a reference to itself.
         * @return A const reference to the next layer in the
         * stack of layers. Ownership is not transferred to the
         * caller.
         */
        const next_layer_type& next_layer() const noexcept {
            return *this;
        }

        /*!
         * @brief Get a reference to the lowest layer.
         *
         * This function returns a reference to the lowest layer
         * in a stack of layers. Since an async_datagram_socket
         * cannot contain any further layers, it simply returns
         * a reference to itself.
         * @return A reference to the lowest layer in the stack
         * of layers. Ownership is not transferred to the
         * caller.
         */
        lowest_layer_type& lowest_layer() noexcept {
            return *this;
        }

        /*!
         * @brief Get a const reference to the lowest layer.
         *
         * This function returns a reference to the lowest layer
         * in a stack of layers. Since an async_datagram_socket
         * cannot contain any further layers, it simply returns
         * a reference to itself.
         * @return A const reference to the lowest layer in the
         * stack of layers. Ownership is not transferred to the
         * caller.
         */
        const lowest_layer_type& lowest_layer() const noexcept {
            return *this;
        }

        /*!
         * @brief set the default remote endpoint the udp socket
         * will send to when using send() without specifying a
         * receiver address. This is equivalent to connecting
         * the udp socket
         * @param dest the default remote endpoint the udp
         * socket will send to
         * @param ec an error code used to report errors from os
         * functions
         */
        void set_destination(const endpoint_type& dest,
                             std::error_code& ec) noexcept {
            base::connect(dest, ec);
        }

        /*!
         * @brief set the default remote endpoint the udp socket
         * will send to when using send() without specifying a
         * receiver address. This is equivalent to connecting
         * the udp socket
         * @param dest the default remote endpoint the udp
         * socket will send to
         */
        void set_destination(const endpoint_type& dest) {
            base::connect(dest);
        }

        /*!
         * @brief set the remote endpoint the udp socket will be
         * allowed to recevie from only. This is equivalent to
         * binding the udp socket.
         * @param src the remote endpoint the udp socket will be
         * allowed to recevie from only
         * @param ec an error code used to report errors from os
         * functions
         */
        void set_source(const endpoint_type& src,
                        std::error_code& ec) noexcept {
            base::bind(src, ec);
        }

        /*!
         * @brief set the remote endpoint the udp socket will be
         * allowed to recevie from only. This is equivalent to
         * binding the udp socket.
         * @param src the remote endpoint the udp socket will be
         * allowed to recevie from only
         */
        void set_source(const endpoint_type& src) {
            base::bind(src);
        }
    };

    /*!
     * @brief Provides the ability to accept new connections.
     * @tparam Protocol The protocol type.
     */
    template <class Protocol>
    class async_acceptor : async_socket_base<Protocol> {
        using base = async_socket_base<Protocol>;
        using base::impl;

        template <typename, typename>
        friend struct rad::rebind_executor_helper;

        void rebind_to_executor(io_executor& ex) noexcept {
            impl.rebind_to_executor(ex);
        }

    public:
        using typename base::endpoint_type;
        using typename base::executor_type;
        using typename base::native_fd_type;
        using typename base::native_handle_type;

        // msvc bug:
        // https://developercommunity.visualstudio.com/t/C-concepts-cant-access-inherited-nest/10965243

        /// The protocol type.
        using protocol_type = typename base::protocol_type;

        /*!
         * @brief Construct a closed acceptor and use the
         * provided executor to dispatch async operations
         * @param ex the executor to dispatch async operations
         * onto
         */
        explicit async_acceptor(executor_type& ex) noexcept : base(ex) {
        }

        /*!
         * @brief Construct acceptor and open its socket with
         * the provided protocol
         * @param ex the executor to dispatch async operations
         * onto
         * @param protocol the protocol to open the acceptor
         * socket with
         */
        async_acceptor(executor_type& ex, const protocol_type& protocol)
            : base(ex, protocol), protocol_{protocol} {
        }

        /*!
         * @brief Construct acceptor, open its socket with the
         * provided protocol and bind it to the provided local
         * address
         * @param ex the executor to dispatch async operations
         * onto
         * @param protocol the protocol to open the acceptor
         * socket with
         * @param local_address the local address to bind the
         * acceptor socket to
         */
        async_acceptor(executor_type& ex, const protocol_type& protocol,
                       const endpoint_type& local_address)
            : base(ex, protocol, local_address), protocol_{protocol} {
        }

        /*!
         * @brief Construct acceptor, open its socket with a
         * protocol whose family is the same as the provided
         * address and bind it to the provided local address
         * @param ex the executor to dispatch async operations
         * onto
         * @param local_addr the local address to bind the
         * acceptor socket to
         */
        async_acceptor(executor_type& ex, const endpoint_type& local_addr)
            : base(ex, protocol_type{local_addr.family()}, local_addr),
              protocol_{local_addr.family()} {
        }

        /*!
         * @brief Gets the protocol used by the acceptor socket
         * which may not be open yet in which case the returned
         * value is meaningless.
         * @return the protocol used by the acceptor socket.
         */
        protocol_type protocol() const noexcept {
            return protocol_;
        }

        using base::native_fd;

        using base::native_handle;

        using base::executor;

        using base::is_open;

        using base::cancel;

        using base::close;

        using base::local_endpoint;

        using base::set_option;

        using base::get_option;

        using base::max_listen_backlog;

        /*!
         * @brief Open the acceptor socket with the provided
         * protocol. If the acceptor socket is open before this
         * call the socket is closed and then reponed with the
         * provided protocol. If this method fails then the
         * acceptor socket is not affected.
         * @param protocol the protocol to open the acceptor
         * socket with
         * @param ec an error code used to report errors from os
         * functions
         */
        void open(const protocol_type& protocol, std::error_code& ec) noexcept {
            base::open(protocol, ec);
            if (!ec) {
                protocol_ = protocol;
            }
        }

        /*!
         * @brief Open the acceptor socket with the provided
         * protocol. If the acceptor socket is open before this
         * call the socket is closed and then reponed with the
         * provided protocol. If this method fails then the
         * acceptor socket is not affected.
         * @param protocol the protocol to open the acceptor
         * socket with
         */
        void open(const protocol_type& protocol) {
            base::open(protocol);
            protocol_ = protocol;
        }

        /*!
         * @brief Bind the acceptor socket to a local address
         * which clients may connect to using bind() system
         * call. Must be called before listen(). If the socket
         * is not open prior to this call the socket is opened
         * with a protocol whose family is the same as the
         * family of provided address.
         * @param local_address the local address to bind to
         * @param ec an error code used to report errors from os
         * functions
         */
        void bind(const endpoint_type& local_address,
                  std::error_code& ec) noexcept {
            ec.clear();
            if (!is_open()) {
                async_acceptor new_acceptor{executor()};
                new_acceptor.open(protocol_type{local_address.family()}, ec);
                if (!ec) {
                    new_acceptor.bind(local_address, ec);
                    if (!ec) {
                        *this = std::move(new_acceptor);
                    }
                }
            }
            else {
                base::bind(local_address, ec);
                if (!ec) {
                    protocol_ = protocol_type{local_address.family()};
                }
            }
        }

        /*!
         * @brief Bind the acceptor socket to a local address
         * which clients may connect to using bind() system
         * call. Must be called before listen(). If the socket
         * is not open prior to this call the socket is opened
         * with a protocol whose family is the same as the
         * family of provided address
         * @param local_address the local address to bind to
         */
        void bind(const endpoint_type& local_address) {
            std::error_code ec;
            bind(local_address, ec);
            check_and_throw(ec, __func__);
        }

        /*!
         * @brief Start listening for incoming connections using
         * listen() system call. Must be called after bind() and
         * before accept() or async_accept()
         * @param backlog the number of connections to reserve
         * in the listen queue. Pass 0 to use the default
         * backlog (SOMAXCONN)
         * @param ec an error code used to report errors from os
         * functions
         */
        void listen(uint32_t backlog, std::error_code& ec) noexcept {
            impl.listen(backlog, ec);
        }

        /*!
         * @brief Start listening for incoming connections using
         * listen() system call. Must be called after bind() and
         * before accept() or async_accept()
         * @param backlog the number of connections to reserve
         * in the listen queue. Pass 0 to use the default
         * backlog (SOMAXCONN)
         */
        void listen(uint32_t backlog = 0) {
            std::error_code ec;
            listen(backlog, ec);
            check_and_throw(ec, __func__);
        }

        /*!
         * @brief Accept a new coming connection asynchronously.
         * Note the accept will not start until the returned
         * awaiter is awaited. Also note that the coroutine may
         * be suspened and resumed later on the socket executor
         * if the operation requires waiting. But if the
         * operation completes immediately the coroutine will
         * not suspend
         * @param ec if set to no_ec then errors are reported
         * via exceptions, otherwise errors are reported via ec
         * @return an awaiter that is when awaited will result
         * in a pair of accepted stream socket and remote
         * endpoint of accepted socket
         */
        auto async_accept(std::error_code& ec = no_ec) {
            return impl.template async_accept<Protocol>(ec);
        }

        /*!
         * @brief Accept a new coming connection asynchronously.
         * Note the accept will not start until the returned
         * awaiter is awaited. Also note that the coroutine may
         * be suspened and resumed later on the socket executor
         * if the operation requires waiting. But if the
         * operation completes immediately the coroutine will
         * not suspend
         * @param s the socket to receive the accepted socket on
         * success
         * @param ec if set to no_ec then errors are reported
         * via exceptions, otherwise errors are reported via ec
         * @return an awaiter that is when awaited will result
         * in the remote endpoint of accepted socket
         */
        template <StreamSocketType Stream>
        auto async_accept(Stream& s, std::error_code& ec = no_ec) {
            return impl.template async_accept_s<endpoint_type>(s, ec);
        }

        /*!
         * @brief Accept a new coming connection asynchronously.
         * Note the accept will not start until the returned
         * awaiter is awaited. Also note that the coroutine may
         * be suspened and resumed later on the socket executor
         * if the operation requires waiting. But if the
         * operation completes immediately the coroutine will
         * not suspend
         * @param peer the endpoint to receive the remote
         * endpoint of accepted socket on success
         * @param ec if set to no_ec then errors are reported
         * via exceptions, otherwise errors are reported via ec
         * @return an awaiter that is when awaited will result
         * in the accepted socket
         */
        auto async_accept(endpoint_type& peer, std::error_code& ec = no_ec) {
            return impl.template async_accept_e<typename protocol_type::socket>(
                peer, ec);
        }

        /*!
         * @brief Accept a new coming connection asynchronously.
         * Note the accept will not start until the returned
         * awaiter is awaited. Also note that the coroutine may
         * be suspened and resumed later on the socket executor
         * if the operation requires waiting. But if the
         * operation completes immediately the coroutine will
         * not suspend
         * @param s the socket to receive the accepted socket on
         * success
         * @param peer the endpoint to receive the remote
         * endpoint of accepted socket on success
         * @param ec if set to no_ec then errors are reported
         * via exceptions, otherwise errors are reported via ec
         * @return an awaiter that is when awaited will start
         * the accept operation
         */
        template <StreamSocketType Stream>
        auto async_accept(Stream& s, endpoint_type& peer,
                          std::error_code& ec = no_ec) {
            return impl.async_accept_se(s, peer, ec);
        }

        /*!
         * @brief Accept a new connection from a peer into the
         * given socket, and additionally provide the endpoint
         * of the remote peer. The function call will block
         * until a new connection has been accepted successfully
         * or an error occurs.
         * @tparam Stream The stream socket type.
         * @param peer_sock The socket into which the new
         * connection will be accepted.
         * @param peer_endpoint An endpoint object which will
         * receive the endpoint of the remote peer.
         * @param ec Set to indicate what error occurred, if
         * any.
         */
        template <StreamSocketType Stream>
        void accept(Stream& peer_sock, endpoint_type& peer_endpoint,
                    std::error_code& ec) noexcept {
            socklen_t size = peer_endpoint.size();
            auto new_sock = impl.accept(peer_endpoint.address(), size, ec);
            if (!ec) {
                peer_endpoint.resize(size);
                peer_sock = Stream{base::executor(), new_sock};
            }
        }

        /*!
         * @brief Accept a new connection from a peer into the
         * given socket, and additionally provide the endpoint
         * of the remote peer. The function call will block
         * until a new connection has been accepted successfully
         * or an error occurs.
         * @tparam Stream The stream socket type.
         * @param peer_sock The socket into which the new
         * connection will be accepted.
         * @param peer_endpoint An endpoint object which will
         * receive the endpoint of the remote peer.
         */
        template <StreamSocketType Stream>
        void accept(Stream& peer_sock, endpoint_type& peer_endpoint) {
            std::error_code ec;
            accept(peer_sock, peer_endpoint, ec);
            check_and_throw(ec, __func__);
        }

        /*!
         * @brief Accept a new connection from a peer into the
         * given socket. The function call will block until a
         * new connection has been accepted successfully or an
         * error occurs.
         * @tparam Stream The stream socket type.
         * @param peer_sock The socket into which the new
         * connection will be accepted.
         * @ec Set to indicate what error occurred, if any.
         */
        template <StreamSocketType Stream>
        void accept(Stream& peer_sock, std::error_code& ec) noexcept {
            endpoint_type peer_endpoint;
            accept(peer_sock, peer_endpoint, ec);
        }

        /*!
         * @brief Accept a new connection from a peer into the
         * given socket. The function call will block until a
         * new connection has been accepted successfully or an
         * error occurs.
         * @tparam Stream The stream socket type.
         * @param peer_sock The socket into which the new
         * connection will be accepted.
         */
        template <StreamSocketType Stream>
        void accept(Stream& peer_sock) {
            endpoint_type peer_endpoint;
            accept(peer_sock, peer_endpoint);
        }

        /*!
         * @brief Accept a new connection from a peer into the
         * given socket. The function call will block until a
         * new connection has been accepted successfully or an
         * error occurs.
         * @param loop The I/O executor to be used for the newly
         * accepted socket.
         * @param peer_endpoint An endpoint object into which
         * the endpoint of the remote peer will be written.
         * @param ec Set to indicate what error occurred, if
         * any.
         * @return On success, a socket object representing the
         * newly accepted connection. On error, a socket object
         * where is_open() is false.
         */
        typename protocol_type::socket accept(executor_type& loop,
                                              endpoint_type& peer_endpoint,
                                              std::error_code& ec) {
            typename protocol_type::socket new_sock{loop};
            accept(new_sock, peer_endpoint, ec);
            return new_sock;
        }

        /*!
         * @brief Accept a new connection from a peer into the
         * given socket. The function call will block until a
         * new connection has been accepted successfully or an
         * error occurs.
         * @param loop The I/O executor to be used for the newly
         * accepted socket.
         * @param peer_endpoint An endpoint object into which
         * the endpoint of the remote peer will be written.
         * @return A socket object representing the newly
         * accepted connection.
         */
        typename protocol_type::socket accept(executor_type& loop,
                                              endpoint_type& peer_endpoint) {
            std::error_code ec;
            auto new_sock = accept(loop, peer_endpoint, ec);
            check_and_throw(ec, __func__);
            return new_sock;
        }

        /*!
         * @brief Accept a new connection from a peer.
         * The function call will block until a new connection
         * has been accepted successfully or an error occurs.
         * @param peer_endpoint An endpoint object into which
         * the endpoint of the remote peer will be written.
         * @param ec Set to indicate what error occurred, if
         * any.
         * @return On success, a socket object representing the
         * newly accepted connection. On error, a socket object
         * where is_open() is false.
         */
        typename protocol_type::socket accept(endpoint_type& peer_endpoint,
                                              std::error_code& ec) {
            return accept(base::executor(), peer_endpoint, ec);
        }

        /*!
         * @brief Accept a new connection from a peer.
         * The function call will block until a new connection
         * has been accepted successfully or an error occurs.
         * @param peer_endpoint An endpoint object into which
         * the endpoint of the remote peer will be written.
         * @return A socket object representing the newly
         * accepted connection.
         */
        typename protocol_type::socket accept(endpoint_type& peer_endpoint) {
            std::error_code ec;
            auto new_sock = accept(peer_endpoint, ec);
            check_and_throw(ec, __func__);
            return new_sock;
        }

        /*!
         * @brief Accept a new connection from a peer.
         * The function call will block until a new connection
         * has been accepted successfully or an error occurs.
         * @param loop The I/O executor to be used for the newly
         * accepted socket.
         * @param ec Set to indicate what error occurred, if
         * any.
         * @return On success, a socket object representing the
         * newly accepted connection. On error, a socket object
         * where is_open() is false.
         */
        typename protocol_type::socket accept(executor_type& loop,
                                              std::error_code& ec) {
            endpoint_type peer_endpoint;
            auto new_sock = accept(loop, peer_endpoint, ec);
            check_and_throw(ec, __func__);
            return new_sock;
        }

        /*!
         * @brief Accept a new connection from a peer.
         * The function call will block until a new connection
         * has been accepted successfully or an error occurs.
         * @param loop The I/O executor to be used for the newly
         * accepted socket.
         * @return A socket object representing the newly
         * accepted connection.
         */
        typename protocol_type::socket accept(executor_type& loop) {
            std::error_code ec;
            auto new_sock = accept(base::executor(), ec);
            check_and_throw(ec, __func__);
            return new_sock;
        }

        /*!
         * @brief Accept a new connection from a peer.
         * The function call will block until a new connection
         * has been accepted successfully or an error occurs.
         * @param ec Set to indicate what error occurred, if
         * any.
         * @return On success, a socket object representing the
         * newly accepted connection. On error, a socket object
         * where is_open() is false.
         */
        typename protocol_type::socket accept(std::error_code& ec) {
            return accept(base::executor(), ec);
        }

        /*!
         * @brief Accept a new connection from a peer.
         * The function call will block until a new connection
         * has been accepted successfully or an error occurs.
         * @return A socket object representing the newly
         * accepted connection.
         */
        typename protocol_type::socket accept() {
            return accept(base::executor());
        }

    private:
        protocol_type protocol_;
    };
} // namespace RAD_LIB_NAMESPACE::net

namespace RAD_LIB_NAMESPACE {
    template <ProxyIoExecutor Exec, class Protocol>
    struct rebind_executor_helper<Exec, net::async_stream_socket<Protocol>> {
        using socket_type = net::async_stream_socket<Protocol>;
        static socket_type rebind(Exec& ex, socket_type&& s) noexcept {
            socket_type new_s{std::move(s)};
            new_s.rebind_to_executor(ex);
            return new_s;
        }
    };

    template <ProxyIoExecutor Exec, class Protocol>
    struct rebind_executor_helper<Exec, net::async_datagram_socket<Protocol>> {
        using socket_type = net::async_datagram_socket<Protocol>;
        static socket_type rebind(Exec& ex, socket_type&& s) noexcept {
            socket_type new_s{std::move(s)};
            new_s.rebind_to_executor(ex);
            return new_s;
        }
    };

    template <ProxyIoExecutor Exec, class Protocol>
    struct rebind_executor_helper<Exec, net::async_acceptor<Protocol>> {
        using socket_type = net::async_acceptor<Protocol>;
        static socket_type rebind(Exec& ex, socket_type&& s) noexcept {
            socket_type new_s{std::move(s)};
            new_s.rebind_to_executor(ex);
            return new_s;
        }
    };
} // namespace RAD_LIB_NAMESPACE
