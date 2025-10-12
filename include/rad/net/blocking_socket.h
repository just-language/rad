#pragma once
#include <rad/buffer.h>
#include <rad/net/types.h>
#include <rad/os_types.h>

namespace RAD_LIB_NAMESPACE::net::detail {
    namespace socket_fns {
        RAD_EXPORT_DECL socket_handle open(address_family af, socket_type type,
                                           protocol_type proto,
                                           std::error_code& ec) noexcept;

        RAD_EXPORT_DECL void shutdown(socket_fd_t s, socket_shutdown how,
                                      std::error_code& ec) noexcept;

        RAD_EXPORT_DECL void setopt(socket_fd_t s, socket_option_level level,
                                    socket_option_name optname,
                                    const void* optdata, socklen_t optlen,
                                    std::error_code& ec) noexcept;

        RAD_EXPORT_DECL void getopt(socket_fd_t s, socket_option_level level,
                                    socket_option_name optname, void* optdata,
                                    socklen_t* optlen,
                                    std::error_code& ec) noexcept;

        RAD_EXPORT_DECL void bind(socket_fd_t s, const void* addr,
                                  socklen_t addr_len,
                                  std::error_code& ec) noexcept;

        RAD_EXPORT_DECL void bind_if_not(socket_fd_t s, address_family af,
                                         std::error_code& ec) noexcept;

        RAD_EXPORT_DECL int max_listen_backlog() noexcept;

        RAD_EXPORT_DECL void listen(socket_fd_t s, uint32_t backlog,
                                    std::error_code& ec) noexcept;

        RAD_EXPORT_DECL socket_handle accept(socket_fd_t s, void* addr,
                                             socklen_t& addr_size,
                                             std::error_code& ec) noexcept;

        RAD_EXPORT_DECL std::size_t
        send(socket_fd_t s, const const_buffer* buffers, std::size_t count,
             transfer_flags flags, std::error_code& ec) noexcept;

        RAD_EXPORT_DECL std::size_t
        recv(socket_fd_t s, const mutable_buffer* buffers, std::size_t count,
             bool not_zero, transfer_flags flags, std::error_code& ec) noexcept;

        RAD_EXPORT_DECL std::size_t
        sendto(socket_fd_t s, const const_buffer* buffers, std::size_t count,
               transfer_flags flags, const void* addr, socklen_t addr_size,
               std::error_code& ec) noexcept;

        RAD_EXPORT_DECL std::size_t
        recvfrom(socket_fd_t s, const mutable_buffer* buffers,
                 std::size_t count, transfer_flags flags, void* addr,
                 socklen_t* addr_size, bool not_zero,
                 std::error_code& ec) noexcept;

        RAD_EXPORT_DECL void local_endpoint(socket_fd_t s, void* addr,
                                            socklen_t& addr_len,
                                            std::error_code& ec) noexcept;

        RAD_EXPORT_DECL void remote_endpoint(socket_fd_t s, void* addr,
                                             socklen_t& addr_len,
                                             std::error_code& ec) noexcept;

        RAD_EXPORT_DECL void connect(socket_fd_t s, const void* addr,
                                     socklen_t addr_len,
                                     std::error_code& ec) noexcept;
    }; // namespace socket_fns
} // namespace RAD_LIB_NAMESPACE::net::detail

namespace RAD_LIB_NAMESPACE::net {
    /*
    template <class Protocol>
    class socket_base {
    public:
        using protocol_type = Protocol;
        using endpoint_type = typename protocol_type::endpoint_type;
        using lowest_layer_type = socket_base;
        using native_handle_type = socket_handle;
        using native_fd_type = socket_fd_t;
        using size_type = std::size_t;

        socket_base() = default;

        socket_base(socket_base&&) = default;

        socket_base(socket_fd_t sock_fd) noexcept : sock{sock_fd} {
        }

        socket_base(native_handle_type sock_fd) noexcept
            : sock{std::move(sock_fd)} {
        }

        socket_base(const protocol_type& protocol) {
            open(protocol);
        }

        socket_base(const protocol_type& protocol, const endpoint_type& addr)
            : socket_base(protocol) {
            bind(addr);
        }

        socket_base& operator=(socket_base&&) = default;

        ~socket_base() = default;

        lowest_layer_type& lowest_layer() noexcept {
            return *this;
        }

        const lowest_layer_type& lowest_layer() const noexcept {
            return *this;
        }

        native_handle_type& native_handle() noexcept {
            return sock;
        }

        [[nodiscard]] const native_handle_type& native_handle() const noexcept {
            return sock;
        }

        native_fd_type native_fd() const noexcept {
            return native_handle().get();
        }

        void open(const protocol_type& protocol, std::error_code& ec) noexcept {
            auto new_sock = detail::socket_fns::open(
                protocol.family(), protocol.type(), protocol.protocol(), ec);
            if (!ec) {
                sock = std::move(new_sock);
            }
        }

        void open(const protocol_type& protocol = protocol_type{}) {
            std::error_code ec;
            open(protocol, ec);
            check_and_throw(ec, __func__);
        }

        [[nodiscard]] bool is_open() const noexcept {
            return static_cast<bool>(sock);
        }

        void close() noexcept {
            sock.reset();
        }

        void shutdown(socket_shutdown how, std::error_code& ec) noexcept {
            detail::socket_fns::shutdown(sock.get(), how, ec);
        }

        void shutdown(socket_shutdown how = socket_shutdown::both) {
            std::error_code ec;
            shutdown(how, ec);
            check_and_throw(ec, __func__);
        }

        template <class SocketOption>
        void set_option(const SocketOption& option,
                        std::error_code& ec) noexcept {
            static_assert(is_supported_option<SocketOption, Protocol>,
                          "unsupported option for this protocol");
            detail::socket_fns::setopt(sock.get(), option.level(),
                                       option.name(), option.data(),
                                       option.size(), ec);
        }

        template <class SocketOption>
        void set_option(const SocketOption& option) {
            std::error_code ec;
            set_option(option, ec);
            check_and_throw(ec, __func__);
        }

        template <class SocketOption>
        void get_option(SocketOption& option,
                        std::error_code& ec) const noexcept {
            static_assert(is_supported_option<SocketOption, Protocol>,
                          "unsupported option for this protocol");
            socklen_t opt_size = option.size();
            detail::socket_fns::getopt(sock.get(), option.level(),
                                       option.name(), option.data(), &opt_size,
                                       ec);
        }

        template <class SocketOption>
        void get_option(SocketOption& option) const {
            std::error_code ec;
            get_option(option, ec);
            check_and_throw(ec, __func__);
        }

        template <class SocketOption>
        SocketOption get_option() const {
            SocketOption opt;
            get_option(opt);
            return opt;
        }

        endpoint_type local_endpoint(std::error_code& ec) const noexcept {
            endpoint_type epoint;
            socklen_t size = epoint.max_size();
            detail::socket_fns::local_endpoint(sock.get(), epoint.address(),
                                               size, ec);
            if (!ec) {
                epoint.resize(size);
            }
            return epoint;
        }

        endpoint_type local_endpoint() const {
            std::error_code ec;
            auto epoint = local_endpoint(ec);
            check_and_throw(ec, __func__);
            return epoint;
        }

        endpoint_type remote_endpoint(std::error_code& ec) const noexcept {
            endpoint_type epoint;
            socklen_t size = epoint.max_size();
            detail::socket_fns::remote_endpoint(sock.get(), epoint.address(),
                                                size, ec);
            if (!ec) {
                epoint.resize(size);
            }
            return epoint;
        }

        endpoint_type remote_endpoint() const {
            std::error_code ec;
            auto peer = remote_endpoint(ec);
            check_and_throw(ec, __func__);
            return peer;
        }

        void bind(const endpoint_type& addr, std::error_code& ec) noexcept {
            detail::socket_fns::bind(sock.get(), addr.address(), addr.size(),
                                     ec);
        }

        void bind(const endpoint_type& addr) {
            std::error_code ec;
            bind(addr, ec);
            check_and_throw(ec, __func__);
        }

        static int max_listen_backlog() noexcept {
            return detail::socket_fns::max_listen_backlog();
        }

    public:

        void connect(const endpoint_type& addr, std::error_code& ec) noexcept {
            detail::socket_fns::connect(sock.get(), addr.address(), addr.size(),
                                        ec);
        }

        void connect(const endpoint_type& addr) {
            std::error_code ec;
            connect(addr, ec);
            check_and_throw(ec, __func__);
        }

        template <EndpointSequence<endpoint_type> EndpointsRange>
        void connect(const EndpointsRange& addrs,
                     std::error_code& ec) noexcept {
            for (auto& ip : addrs) {
                ec.clear();
                connect(ip, ec);
                if (!ec) {
                    break;
                }
            }
        }

        template <EndpointSequence<endpoint_type> EndpointsRange>
        void connect(const EndpointsRange& addrs) {
            std::error_code ec;
            connect(addrs, ec);
            check_and_throw(ec, __func__);
        }

        template <BufferSequence Buffers>
        size_type send(const Buffers& buffers, transfer_flags flags,
                       std::error_code& ec) noexcept {
            auto [buffs, count] = extract_buffers<true>(buffers);
            return detail::socket_fns::send(sock.get(), buffs, count, flags,
                                            ec);
        }

        template <BufferSequence Buffers>
        size_type send(const Buffers& buffers, std::error_code& ec) noexcept {
            return send(buffers, transfer_flags::none, ec);
        }

        template <BufferSequence Buffers>
        size_type send(const Buffers& buffers,
                       transfer_flags flags = transfer_flags::none) {
            std::error_code ec;
            std::size_t sent = send(buffers, flags, ec);
            check_and_throw(ec, __func__);
            return sent;
        }

        template <BufferSequence Buffers>
        size_type write(const Buffers& buffers, transfer_flags flags,
                        std::error_code& ec) noexcept {
            return send(buffers, flags, ec);
        }

        template <BufferSequence Buffers>
        size_type write(const Buffers& buffers, std::error_code& ec) noexcept {
            return send(buffers, ec);
        }

        template <BufferSequence Buffers>
        size_type write(const Buffers& buffers,
                        transfer_flags flags = transfer_flags::none) {
            return send(buffers, flags);
        }

        template <BufferSequence Buffers>
        size_type send_to(const Buffers& buffers, const endpoint_type& peer,
                          transfer_flags flags, std::error_code& ec) noexcept {
            auto [buffs, count] = extract_buffers<true>(buffers);
            return detail::socket_fns::sendto(sock.get(), buffs, count, flags,
                                              peer.address(), peer.size(), ec);
        }

        template <BufferSequence Buffers>
        size_type send_to(const Buffers& buffers, const endpoint_type& peer,
                          std::error_code& ec) noexcept {
            return send_to(buffers, peer, transfer_flags::none, ec);
        }

        template <BufferSequence Buffers>
        size_type send_to(const Buffers& buffers, const endpoint_type& peer,
                          transfer_flags flags = transfer_flags::none) {
            std::error_code ec;
            std::size_t sent = send_to(buffers, peer, flags, ec);
            check_and_throw(ec, __func__);
            return sent;
        }

        template <MutableBufferSequence Buffers>
        size_type receive(const Buffers& buffers, transfer_flags flags,
                          std::error_code& ec) noexcept {
            auto [buffs, count] = extract_buffers<false>(buffers);
            bool not_zero =
                protocol_type::is_stream_protocol && not_empty_buffers(buffers);
            return detail::socket_fns::recv(sock.get(), buffs, count, not_zero,
                                            flags, ec);
        }

        template <MutableBufferSequence Buffers>
        size_type receive(const Buffers& buffers,
                          std::error_code& ec) noexcept {
            return receive(buffers, transfer_flags::none, ec);
        }

        template <MutableBufferSequence Buffers>
        size_type receive(const Buffers& buffers,
                          transfer_flags flags = transfer_flags::none) {
            std::error_code ec;
            std::size_t recved = receive(buffers, flags, ec);
            check_and_throw(ec, __func__);
            return recved;
        }

        template <MutableBufferSequence Buffers>
        size_type read(const Buffers& buffers, transfer_flags flags,
                       std::error_code& ec) noexcept {
            return receive(buffers, flags, ec);
        }

        template <MutableBufferSequence Buffers>
        size_type read(const Buffers& buffers, std::error_code& ec) noexcept {
            return receive(buffers, ec);
        }

        template <MutableBufferSequence Buffers>
        size_type read(const Buffers& buffers,
                       transfer_flags flags = transfer_flags::none) {
            return receive(buffers, flags);
        }

        template <MutableBufferSequence Buffers>
        size_type receive_all(const Buffers& buffers, transfer_flags flags,
                              std::error_code& ec) noexcept {
            return receive(buffers, flags | transfer_flags::wait_all, ec);
        }

        template <MutableBufferSequence Buffers>
        size_type receive_all(const Buffers& buffers,
                              std::error_code& ec) noexcept {
            return receive(buffers, transfer_flags::wait_all, ec);
        }

        template <MutableBufferSequence Buffers>
        void receive_all(const Buffers& buffers,
                         transfer_flags flags = transfer_flags::wait_all) {
            receive(buffers, flags | transfer_flags::wait_all);
        }

        template <MutableBufferSequence Buffers>
        size_type read_all(const Buffers& buffers, transfer_flags flags,
                           std::error_code& ec) noexcept {
            return receive(buffers, flags | transfer_flags::wait_all, ec);
        }

        template <MutableBufferSequence Buffers>
        size_type read_all(const Buffers& buffers,
                           std::error_code& ec) noexcept {
            return receive(buffers, transfer_flags::wait_all, ec);
        }

        template <MutableBufferSequence Buffers>
        void read_all(const Buffers& buffers,
                      transfer_flags flags = transfer_flags::wait_all) {
            receive(buffers, flags | transfer_flags::wait_all);
        }

        template <MutableBufferSequence Buffers>
        size_type receive_from(const Buffers& buffers, endpoint_type& peer,
                               transfer_flags flags,
                               std::error_code& ec) noexcept {
            auto [buffs, count] = extract_buffers<false>(buffers);
            auto size = peer.max_size();
            bool not_zero =
                protocol_type::is_stream_protocol && not_empty_buffers(buffers);
            auto transferred = detail::socket_fns::recvfrom(
                sock.get(), buffs, count, flags, peer.address(), &size,
                not_zero, ec);
            if (!ec) {
                peer.resize(size);
            }
            return transferred;
        }

        template <MutableBufferSequence Buffers>
        size_type receive_from(const Buffers& buffers, endpoint_type& peer,
                               std::error_code& ec) noexcept {
            return receive_from(buffers, peer, transfer_flags::none, ec);
        }

        template <MutableBufferSequence Buffers>
        size_type
        receive_from(const Buffers& buffers, endpoint_type& peer,
                     transfer_flags flags = transfer_flags::none) {
            std::error_code ec;
            std::size_t recved = receive_from(buffers, peer, flags, ec);
            check_and_throw(ec, __func__);
            return recved;
        }

    private:
        socket_handle sock;
    };

    template <class Protocol>
    class stream_socket : public socket_base<Protocol> {
        using base = socket_base<Protocol>;

    public:
        using endpoint_type = typename base::endpoint_type;

        using base::base;

        stream_socket() = default;

        stream_socket(stream_socket&&) = default;

        stream_socket(base&& other) noexcept : base(std::move(other)) {
        }

        stream_socket& operator=(stream_socket&&) = default;

        void listen(int backlog, std::error_code& ec) noexcept {
            detail::socket_fns::listen(base::native_handle().get(), backlog,
                                       ec);
        }

        void listen(int backlog = base::max_listen_backlog()) {
            std::error_code ec;
            listen(backlog, ec);
            check_and_throw(ec, __func__);
        }

        template <class Proto1>
        void accept(socket_base<Proto1>& peer_sock, endpoint_type& addr,
                    std::error_code& ec) noexcept {
            socklen_t size = addr.max_size();
            auto new_sock = detail::socket_fns::accept(
                base::native_handle().get(), addr.address(), size, ec);
            if (!ec) {
                peer_sock.native_handle() = std::move(new_sock);
            }
        }

        template <class Proto1>
        void accept(socket_base<Proto1>& peer_sock, endpoint_type& addr) {
            std::error_code ec;
            accept(peer_sock, addr, ec);
            check_and_throw(ec, __func__);
        }

        template <class Proto1>
        void accept(socket_base<Proto1>& peer_sock,
                    std::error_code& ec) noexcept {
            endpoint_type addr;
            accept(peer_sock, addr, ec);
        }

        template <class Proto1>
        void accept(socket_base<Proto1>& peer_sock) {
            endpoint_type addr;
            accept(peer_sock, addr);
        }

        stream_socket accept(endpoint_type& peer_addr,
                             std::error_code& ec) noexcept {
            stream_socket new_sock;
            accept(new_sock, peer_addr, ec);
            return new_sock;
        }

        stream_socket accept(endpoint_type& peer_addr) {
            std::error_code ec;
            auto new_sock = accept(peer_addr, ec);
            check_and_throw(ec, __func__);
            return new_sock;
        }

        stream_socket accept(std::error_code& ec) noexcept {
            stream_socket new_sock;
            endpoint_type addr;
            accept(new_sock, addr, ec);
            return new_sock;
        }

        stream_socket accept() {
            std::error_code ec;
            stream_socket new_sock = accept(ec);
            check_and_throw(ec, __func__);
            return new_sock;
        }
    };

    template <class Protocol>
    class datagram_socket : public socket_base<Protocol> {
        using base = socket_base<Protocol>;

    public:
        using protocol_type = typename base::protocol_type;
        using endpoint_type = typename base::endpoint_type;

        using base::base;

        datagram_socket() = default;

        datagram_socket(datagram_socket&&) = default;

        datagram_socket(base&& other) noexcept : base(std::move(other)) {
        }

        datagram_socket& operator=(datagram_socket&&) = default;

        void set_destination(const endpoint_type& dest,
                             std::error_code& ec) noexcept {
            base::connect(dest, ec);
        }

        void set_destination(const endpoint_type& dest) {
            base::connect(dest);
        }

        void set_source(const endpoint_type& src,
                        std::error_code& ec) noexcept {
            base::bind(src, ec);
        }

        void set_source(const endpoint_type& src) {
            base::bind(src);
        }
    };
    */
}; // namespace RAD_LIB_NAMESPACE::net
