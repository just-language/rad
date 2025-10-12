#pragma once
#include <rad/big_endian.h>
#include <rad/os_types.h>
#include <rad/string.h>

#include <array>
#include <cassert>

namespace RAD_LIB_NAMESPACE::net::detail {
    RAD_EXPORT_DECL int close_socket(socket_fd_t fd) noexcept;
}

namespace RAD_LIB_NAMESPACE::net {
    /*!
     * @brief Socket type.
     */
    enum class socket_type : int {
        /*!
         * @brief SOCK_STREAM Provides sequenced, reliable, two-way,
         * connection-based byte streams.
         */
        tcp_stream = 1,
        /*!
         * @brief SOCK_DGRAM Supports datagrams (connectionless, unreliable
         * messages of a fixed maximum length)
         */
        udp_dgram = 2,
        /// RAW Provides raw network protocol access.
        raw = 3,
        /*!
         * @brief RDM Provides a reliable datagram layer that does not guarantee
         * ordering.
         */
        rdm = 4,
        /*!
         * @brief SOCK_SEQPACKET a sequenced-packet socket that is
         * connection-oriented, preserves message boundaries, and delivers
         * messages in the order that they were sent.
         */
        seq_packet = 5,
    };

    /*!
     * @brief The socket protocol.
     */
    enum class protocol_type : int {
        /*!
         * @brief Use the default protocol for the socket type.
         */
        auto_protocol = 0,
        /*!
         * @brief IPPROTO_ICMP The Internet Control Message Protocol (ICMP).
         */
        icmp = 1,
        /*!
         * @brief IPPROTO_IGMP The Internet Group Management Protocol (IGMP).
         */
        igmp = 2,
        /*!
         * @brief BTHPROTO_RFCOMM The Bluetooth Radio Frequency Communications
         * (Bluetooth RFCOMM) protocol.
         */
        bluetooth_rfcomm = 3,

        seq_packet = 5,
        /*!
         * @brief IPPROTO_TCP The Transmission Control Protocol (TCP).
         */
        tcp = 6,
        /*!
         * @brief IPPROTO_UDP The User Datagram Protocol (UDP).
         */
        udp = 17,
        /*!
         * @brief IPPROTO_ICMPV6 The Internet Control Message Protocol Version 6
         * (ICMPv6).
         */
        icmpv6 = 58,
        pgm = 113,
    };

    /*!
     * @brief Socket shutdown type.
     */
    enum class socket_shutdown {
        /// shutdown receive end.
        receive,
        /// shutdown receive end.
        read = receive,
        /// shutdown send end.
        send,
        /// shutdown send end.
        write = send,
        /// shutdown both ends.
        both,
    };

#ifdef _WIN32
    /*!
     * @brief Socket creation flags.
     */
    enum class socket_creation_flags : unsigned long {
        /// Overlapped socket support async operations with IOCP.
        overlapped = 0x01,
        /// Prevent child processes from inheriting this socket handle.
        no_inherit = 0x80,
    };

    /*!
     * @brief Address family.
     */
    enum class address_family : uint16_t {
        /// Unspecified address family.
        unspecified = 0,
        /*!
         * @brief AF_LOCAL or AF_UNIX Local communication.
         * Used for unix domain sockets.
         */
        local = 1,
        /// AF_INET IPv4 Internet protocols.
        ipv4 = 2,
        /// AF_INET6 IPv6 Internet protocols.
        ipv6 = 23,
        /// AF_IRDA The Infrared Data Association (IrDA) address family.
        infrared = 26,
        /// AF_BLUETOOTH Bluetooth low-level socket protocol.
        bluetooth = 32,
        /// AF_NETBIOS The NetBIOS address family.
        net_bios = 17,
        /// AF_APPLETALK AppleTalk.
        apple_talk = 16,
        /// AF_IPX IPX - Novell protocols.
        ipx = 6,
    };

    /*!
     * @brief Socket transfer flags using with send and receive.
     */
    enum class transfer_flags : int {
        /// No flags.
        none = 0,
        /*!
         * @brief MSG_DONTROUTE Don't use a gateway to send out the packet, send
         * to hosts only on directly connected networks.
         */
        dont_route = 0x4,
        /*!
         * @brief MSG_OOB Sends out-of-band data on sockets that support this
         * notion.
         */
        oob = 0x1,
        /*!
         * @brief MSG_PEEK This flag causes the receive operation to return data
         * from the beginning of the receive queue without removing that data
         * from the queue.  Thus, a subsequent receive call will return the same
         * data.
         */
        peek = 0x2,
        /*!
         * @brief MSG_WAITALL This flag requests that the operation don't
         * complete until the full request is satisfied.
         */
        wait_all = 0x8,
        /*!
         * @brief MSG_PUSH_IMMEDIATE This flag is for stream-oriented sockets
         * only. This flag allows an application that uses stream sockets to
         * tell the transport provider not to delay completion of partially
         * filled pending receive requests.
         */
        push_immediate = 0x20,
        /*!
         * @brief MSG_DONTWAIT Enables nonblocking operation.
         */
        dont_wait = 0x40,
    };

    /*!
     * @brief Socket option level.
     */
    enum class socket_option_level : int {
        /// SOL_SOCKET
        socket = 0xffff,
        /// IPPROTO_TCP
        proto_tcp = 6,
        /// IPPROTO_UDP
        proto_udp = 17,
        /// IPv4 level.
        proto_ipv4 = 0,
        /// IPv64 level.
        proto_ipv6 = 41,
    };

    /*!
     * @brief Socket option name.
     */
    enum class socket_option_name : int {
        /*!
         * @brief SO_ERROR Retrieves error status and clear.
         */
        error = 0x1007,
        /*!
         * @brief SO_REUSEADDR Allows the socket to be bound to an address that
         * is already in use.
         */
        reuse_addr = 0x0004,
        /*!
         * @brief SO_EXCLUSIVEADDRUSE Enables a socket to be bound for exclusive
         * access.
         */
        exclusive_addr = ~reuse_addr,
        /*!
         * @brief SO_RCVBUF Specifies the total per-socket buffer space reserved
         * for receives.
         */
        recv_buff = 0x1002,
        /*!
         * @brief SO_SNDBUF Specifies the total per-socket buffer space reserved
         * for sends.
         */
        send_buff = 0x1001,
        /*!
         * @brief SO_RCVTIMEO Sets the timeout, in milliseconds, for blocking
         * receive calls.
         */
        recv_timeout = 0x1006,
        /*!
         * @brief SO_SNDTIMEO The timeout, in milliseconds, for blocking send
         * calls.
         */
        send_timeout = 0x1005,
        /*!
         * @brief SO_KEEPALIVE Enables sending keep-alive packets for a socket
         * connection. Not supported on ATM sockets (results in an error).
         */
        keep_alive = 0x0008,
        /*!
         * @brief SO_DONTROUTE Sets whether outgoing data should be sent on
         * interface the socket is bound to and not a routed on some other
         * interface.
         */
        dont_route = 0x0010,
        /// SO_BROADCAST Configures a socket for sending broadcast data.
        broadcast = 0x0020,
        /// SO_LINGER Lingers on close if unsent data is present.
        linger = 0x80,
        /*!
         * @brief SO_UPDATE_ACCEPT_CONTEXT used with AcceptEx.
         */
        update_accept_context = 0x700B,
        /*!
         * @brief SO_UPDATE_CONNECT_CONTEXT used with ConnectEx.
         */
        update_connect_context = 0x7010,

        /*!
         * @brief TCP_NODELAY Enables or disables the Nagle algorithm for TCP
         * sockets.
         */
        tcp_no_delay = 0x0001,
        /*!
         * @brief TCP_KEEPIDLE Gets or sets the number of seconds a TCP
         * connection will remain idle before keepalive probes are sent to the
         * remote.
         */
        tcp_keep_idle = 3,
        /*!
         * @brief TCP_MAXRT If this value is non-negative, it represents the
         * desired connection timeout in seconds. If it is -1, it represents a
         * request to disable connection timeout (i.e. the connection will
         * retransmit forever).
         */
        tcp_max_rt = 5,
        /*!
         * @brief TCP_KEEPCNT Gets or sets the number of TCP keep alive probes
         * that will be sent before the connection is terminated. It is illegal
         * to set TCP_KEEPCNT to a value greater than 255.
         */
        tcp_keep_count = 16,
        /*!
         * @brief TCP_KEEPINTVL Gets or sets the number of seconds a TCP
         * connection will wait for a keepalive response before sending another
         * keepalive probe.
         */
        tcp_keep_alive_interval = 17,

        /*!
         * @brief IP_MULTICAST_IF Gets or sets the outgoing interface for
         * sending IPv4 multicast traffic.
         */
        multi_cast_outbound_interface = 9,
        /*!
         * @brief IP_MULTICAST_TTL Sets/gets the TTL value associated with IP
         * multicast traffic on the socket.
         */
        multi_cast_hops = 10,
        /*!
         * @brief IP_MULTICAST_LOOP For a socket that is joined to one or more
         * multicast groups, this controls whether it will receive a copy of
         * outgoing packets sent to those multicast groups via the selected
         * multicast interface.
         */
        multi_cast_enable_loop_back = 11,
        /*!
         * @brief IP_ADD_MEMBERSHIP Join the socket to the supplied multicast
         * group on the specified interface.
         */
        multi_cast_join_group = 12,
        /*!
         * @brief IP_DROP_MEMBERSHIP Leaves the specified multicast group from
         * the specified interface.
         */
        multi_cast_leave_group = 13,
    };

    /*!
     * @brief Resolver flags used by getaddrinfo.
     */
    enum class resolver_flags : int {
        /// No flags.
        none = 0x0,
        /// AI_PASSIVE
        passive = 0x01,
        /// AI_CANONNAME
        canon_name = 0x02,
        /// AI_NUMERICHOST
        numeric_host = 0x04,
        /// AI_NUMERICSERV
        numeric_service = 0x08,
        /// AI_ALL
        all = 0x0100,
        /// AI_ADDRCONFIG
        addr_config = 0x0400,
        /// AI_V4MAPPED
        ipv4_mapped = 0x0800,
        /// AI_NON_AUTHORITATIVE
        non_authoritative = 0x04000,
        /// AI_SECURE
        secure = 0x08000,
    };
#elif __linux__
    /*!
     * @brief Socket creation flags.
     */
    enum class socket_creation_flags : int {
        /// close inherited socket on child exec.
        close_exec = 02000000,
    };

    /*!
     * @brief Address family.
     */
    enum class address_family : uint16_t {
        /// Unspecified address family.
        unspecified = 0,
        /*!
         * @brief AF_LOCAL or AF_UNIX Local communication.
         * Used for unix domain sockets.
         */
        local = 1,
        /// AF_INET IPv4 Internet protocols.
        ipv4 = 2,
        /// AF_INET6 IPv6 Internet protocols.
        ipv6 = 10,
        /// AF_IRDA The Infrared Data Association (IrDA) address family.
        infrared = 23,
        /// AF_BLUETOOTH Bluetooth low-level socket protocol.
        bluetooth = 32,
        /// AF_NETBIOS The NetBIOS address family.
        net_bios = 17,
        /// AF_APPLETALK AppleTalk.
        apple_talk = 5,
        /// AF_IPX IPX - Novell protocols.
        ipx = 4,
    };

    enum class transfer_flags : int {
        /// No flags.
        none = 0,
        /*!
         * @brief MSG_DONTROUTE Don't use a gateway to send out the packet, send
         * to hosts only on directly connected networks.
         */
        dont_route = 0x4,
        /*!
         * @brief MSG_OOB Sends out-of-band data on sockets that support this
         * notion.
         */
        oob = 0x1,
        /*!
         * @brief MSG_PEEK This flag causes the receive operation to return data
         * from the beginning of the receive queue without removing that data
         * from the queue.  Thus, a subsequent receive call will return the same
         * data.
         */
        peek = 0x2,
        /*!
         * @brief MSG_WAITALL This flag requests that the operation don't
         * complete until the full request is satisfied.
         */
        wait_all = 0x100,
        /*!
         * @brief MSG_DONTWAIT Enables nonblocking operation.
         */
        dont_wait = 0x40,
        /*!
         * @brief MSG_NOSIGNAL Don't generate a SIGPIPE signal if the peer on a
         * stream-oriented socket has closed the connection.
         */
        no_signal = 0x4000,
    };

    /*!
     * @brief Socket option level.
     */
    enum class socket_option_level {
        /// SOL_SOCKET
        socket = 1,
        /// IPPROTO_TCP
        proto_tcp = 6,
        /// IPv4 level.
        proto_ipv4 = 0,
        /// IPv6 level.
        proto_ipv6 = 41,
        /// IPPROTO_UDP.
        proto_udp = 17,
    };

    /*!
     * @brief Socket option name.
     */
    enum class socket_option_name : int {
        /*!
         * @brief SO_ERROR Retrieves error status and clear.
         */
        error = 4,
        /*!
         * @brief SO_REUSEADDR Allows the socket to be bound to an address that
         * is already in use.
         */
        reuse_addr = 2,
        /*!
         * @brief SO_RCVBUF Specifies the total per-socket buffer space reserved
         * for receives.
         */
        recv_buff = 8,
        /*!
         * @brief SO_SNDBUF Specifies the total per-socket buffer space reserved
         * for sends.
         */
        send_buff = 7,
        /*!
         * @brief SO_RCVTIMEO Sets the timeout, in milliseconds, for blocking
         * receive calls.
         */
        recv_timeout = 20,
        /*!
         * @brief SO_SNDTIMEO The timeout, in milliseconds, for blocking send
         * calls.
         */
        send_timeout = 21,
        /*!
         * @brief SO_KEEPALIVE Enables sending keep-alive packets for a socket
         * connection. Not supported on ATM sockets (results in an error).
         */
        keep_alive = 9,
        /*!
         * @brief SO_DONTROUTE Sets whether outgoing data should be sent on
         * interface the socket is bound to and not a routed on some other
         * interface.
         */
        dont_route = 5,
        /// SO_BROADCAST Configures a socket for sending broadcast data.
        broadcast = 6,
        /// SO_LINGER Lingers on close if unsent data is present.
        linger = 13,

        /*!
         * @brief TCP_NODELAY Enables or disables the Nagle algorithm for TCP
         * sockets.
         */
        tcp_no_delay = 1,

        /*!
         * @brief TCP_KEEPIDLE Gets or sets the number of seconds a TCP
         * connection will remain idle before keepalive probes are sent to the
         * remote.
         */
        tcp_keep_idle = 4,
        /*!
         * @brief TCP_KEEPCNT Gets or sets the number of TCP keep alive probes
         * that will be sent before the connection is terminated. It is illegal
         * to set TCP_KEEPCNT to a value greater than 255.
         */
        tcp_keep_count = 6,
        /*!
         * @brief TCP_KEEPINTVL Gets or sets the number of seconds a TCP
         * connection will wait for a keepalive response before sending another
         * keepalive probe.
         */
        tcp_keep_alive_interval = 5,

        /*!
         * @brief IP_MULTICAST_IF Gets or sets the outgoing interface for
         * sending IPv4 multicast traffic.
         */
        multi_cast_outbound_interface = 32,
        /*!
         * @brief IP_MULTICAST_TTL Sets/gets the TTL value associated with IP
         * multicast traffic on the socket.
         */
        multi_cast_hops = 33,
        /*!
         * @brief IP_MULTICAST_LOOP For a socket that is joined to one or more
         * multicast groups, this controls whether it will receive a copy of
         * outgoing packets sent to those multicast groups via the selected
         * multicast interface.
         */
        multi_cast_enable_loop_back = 34,
        /*!
         * @brief IP_ADD_MEMBERSHIP Join the socket to the supplied multicast
         * group on the specified interface.
         */
        multi_cast_join_group = 35,
        /*!
         * @brief IP_DROP_MEMBERSHIP Leaves the specified multicast group from
         * the specified interface.
         */
        multi_cast_leave_group = 36,
    };

    /*!
     * @brief Resolver flags used by getaddrinfo.
     */
    enum class resolver_flags : int {
        /// No flags.
        none = 0x0,
        /// AI_PASSIVE
        passive = 0x01,
        /// AI_CANONNAME
        canon_name = 0x02,
        /// AI_NUMERICHOST
        numeric_host = 0x04,
        /// AI_ALL
        all = 0x010,
        /// AI_ADDRCONFIG
        addr_config = 0x020,
        /// AI_V4MAPPED
        ipv4_mapped = 0x08,
    };

#else
    /*!
     * @brief Socket creation flags.
     */
    enum class socket_creation_flags : int {
        /// close inherited socket on child exec.
        close_exec = 02000000,
    };

    /*!
     * @brief Address family.
     */
    enum class address_family : uint8_t {
        /// Unspecified address family.
        unspecified = 0,
        /*!
         * @brief AF_LOCAL or AF_UNIX Local communication.
         * Used for unix domain sockets.
         */
        local = 1,
        /// AF_INET IPv4 Internet protocols.
        ipv4 = 2,
        /// AF_INET6 IPv6 Internet protocols.
        ipv6 = 28,
        /// AF_IRDA The Infrared Data Association (IrDA) address family.
        infrared = 23,
        /// AF_BLUETOOTH Bluetooth low-level socket protocol.
        bluetooth = 32,
        /// AF_NETBIOS The NetBIOS address family.
        net_bios = 6,
        /// AF_APPLETALK AppleTalk.
        apple_talk = 16,
        /// AF_IPX IPX - Novell protocols.
        ipx = 23,
    };

    enum class transfer_flags : int {
        /// No flags.
        none = 0,
        /*!
         * @brief MSG_DONTROUTE Don't use a gateway to send out the packet, send
         * to hosts only on directly connected networks.
         */
        dont_route = 0x4,
        /*!
         * @brief MSG_OOB Sends out-of-band data on sockets that support this
         * notion.
         */
        oob = 0x1,
        /*!
         * @brief MSG_PEEK This flag causes the receive operation to return data
         * from the beginning of the receive queue without removing that data
         * from the queue.  Thus, a subsequent receive call will return the same
         * data.
         */
        peek = 0x2,
        /*!
         * @brief MSG_WAITALL This flag requests that the operation don't
         * complete until the full request is satisfied.
         */
        wait_all = 0x40,
        /*!
         * @brief MSG_DONTWAIT Enables nonblocking operation.
         */
        dont_wait = 0x80,
        /*!
         * @brief MSG_NOSIGNAL Don't generate a SIGPIPE signal if the peer on a
         * stream-oriented socket has closed the connection.
         */
        no_signal = 0x00020000,
    };

    /*!
     * @brief Socket option level.
     */
    enum class socket_option_level {
        /// SOL_SOCKET
        socket = 0xffff,
        /// IPPROTO_TCP
        proto_tcp = 6,
        /// IPv4 level.
        proto_ipv4 = 0,
        /// IPv6 level.
        proto_ipv6 = 41,
        /// IPPROTO_UDP.
        proto_udp = 17,
    };

    /*!
     * @brief Socket option name.
     */
    enum class socket_option_name : int {
        /*!
         * @brief SO_ERROR Retrieves error status and clear.
         */
        error = 0x1007,
        /*!
         * @brief SO_REUSEADDR Allows the socket to be bound to an address that
         * is already in use.
         */
        reuse_addr = 0x4,
        /*!
         * @brief SO_RCVBUF Specifies the total per-socket buffer space reserved
         * for receives.
         */
        recv_buff = 0x1002,
        /*!
         * @brief SO_SNDBUF Specifies the total per-socket buffer space reserved
         * for sends.
         */
        send_buff = 0x1001,
        /*!
         * @brief SO_RCVTIMEO Sets the timeout, in milliseconds, for blocking
         * receive calls.
         */
        recv_timeout = 0x1006,
        /*!
         * @brief SO_SNDTIMEO The timeout, in milliseconds, for blocking send
         * calls.
         */
        send_timeout = 0x1005,
        /*!
         * @brief SO_KEEPALIVE Enables sending keep-alive packets for a socket
         * connection. Not supported on ATM sockets (results in an error).
         */
        keep_alive = 0x8,
        /*!
         * @brief SO_DONTROUTE Sets whether outgoing data should be sent on
         * interface the socket is bound to and not a routed on some other
         * interface.
         */
        dont_route = 0x10,
        /// SO_BROADCAST Configures a socket for sending broadcast data.
        broadcast = 0x20,
        /// SO_LINGER Lingers on close if unsent data is present.
        linger = 0x80,

        /*!
         * @brief TCP_NODELAY Enables or disables the Nagle algorithm for TCP
         * sockets.
         */
        tcp_no_delay = 1,

        /*!
         * @brief TCP_KEEPIDLE Gets or sets the number of seconds a TCP
         * connection will remain idle before keepalive probes are sent to the
         * remote.
         */
        tcp_keep_idle = 256,
        /*!
         * @brief TCP_KEEPCNT Gets or sets the number of TCP keep alive probes
         * that will be sent before the connection is terminated. It is illegal
         * to set TCP_KEEPCNT to a value greater than 255.
         */
        tcp_keep_count = 1024,
        /*!
         * @brief TCP_KEEPINTVL Gets or sets the number of seconds a TCP
         * connection will wait for a keepalive response before sending another
         * keepalive probe.
         */
        tcp_keep_alive_interval = 512,

        /*!
         * @brief IP_MULTICAST_IF Gets or sets the outgoing interface for
         * sending IPv4 multicast traffic.
         */
        multi_cast_outbound_interface = 9,
        /*!
         * @brief IP_MULTICAST_TTL Sets/gets the TTL value associated with IP
         * multicast traffic on the socket.
         */
        multi_cast_hops = 10,
        /*!
         * @brief IP_MULTICAST_LOOP For a socket that is joined to one or more
         * multicast groups, this controls whether it will receive a copy of
         * outgoing packets sent to those multicast groups via the selected
         * multicast interface.
         */
        multi_cast_enable_loop_back = 11,
        /*!
         * @brief IP_ADD_MEMBERSHIP Join the socket to the supplied multicast
         * group on the specified interface.
         */
        multi_cast_join_group = 12,
        /*!
         * @brief IP_DROP_MEMBERSHIP Leaves the specified multicast group from
         * the specified interface.
         */
        multi_cast_leave_group = 13,
    };

    /*!
     * @brief Resolver flags used by getaddrinfo.
     */
    enum class resolver_flags : int {
        /// No flags.
        none = 0x0,
        /// AI_PASSIVE
        passive = 0x01,
        /// AI_CANONNAME
        canon_name = 0x02,
        /// AI_NUMERICHOST
        numeric_host = 0x04,
        /// AI_ALL
        all = 0x00000100,
        /// AI_ADDRCONFIG
        addr_config = 0x00000400,
        /// AI_V4MAPPED
        ipv4_mapped = 0x00000800,
    };

#endif // _WIN32

#if defined(_WIN32) || defined(__linux__)
    /*!
     * @brief IP address type.
     * Either IPv4, or IPv6.
     */
    enum class address_type : uint16_t {
        ipv4 = static_cast<uint16_t>(address_family::ipv4),
        ipv6 = static_cast<uint16_t>(address_family::ipv6),
        invalid,
    };
#else
    /*!
     * @brief IP address type.
     * Either IPv4, or IPv6.
     */
    enum class address_type : uint8_t {
        ipv4 = static_cast<uint8_t>(address_family::ipv4),
        ipv6 = static_cast<uint8_t>(address_family::ipv6),
        invalid,
    };
#endif

    RAD_OVERLOAD_ENUM_OPERATORS(socket_creation_flags);
    RAD_OVERLOAD_ENUM_OPERATORS(resolver_flags);
    RAD_OVERLOAD_ENUM_OPERATORS(transfer_flags);

    /*!
     * @brief IPv4 address.
     *
     * This class provides the ability to use and manipulate IP version 4
     * addresses.
     */
    class ipv4 {
    public:
        /*!
         * @brief The type used to represent an IPv4 address as an array of
         * bytes. (4 bytes).
         */
        using bytes_type = std::array<uint8_t, 4>;

        /*!
         * @brief The type used to represent an IPv4 address as an unsigned
         * integer.
         */
        using uint_type = uint32_t;

        /*!
         * @brief Construct an any IPv4 (0.0.0.0).
         */
        ipv4() noexcept : ip_be_number_{0} {
        }

        /*!
         * @brief Parse a string IPv4.
         * On failure, an exception is thrown.
         * @param ipstr The IPv4 string.
         */
        explicit ipv4(std::string_view ipstr) {
            if (!from_string(ipstr)) {
                throw std::system_error(std::make_error_code(
                    std::errc::address_family_not_supported));
            }
        }

        /*!
         * @brief Construct an IPv4 with the ip 32 bit number in host byte
         * order.
         * @param ipv4_number The ip 32 bit numer in host byte order.
         */
        explicit ipv4(uint32_t ipv4_number) noexcept
            : ip_be_number_{ipv4_number} {
        }

        /*!
         * @brief Construct an IPv4 with the ip 4 bytes in network byte order.
         *
         * For address (127.1.2.0) ip_bytes[0] = 127, and ip_bytes[3] = 0.
         * @param ip_bytes The ip 4 bytes in network byte order.
         */
        explicit ipv4(const bytes_type& ip_bytes) noexcept
            : bytes_storage_{ip_bytes} {
        }

        /*!
         * @brief Get the 4 bytes of this IPv4 address in network byte order.
         *
         * For address (127.1.2.0) ip_bytes[0] = 127, and ip_bytes[3] = 0.
         * @return The 4 bytes of this IPv4 address in network byte order.
         */
        bytes_type to_bytes() const noexcept {
            return bytes_storage_;
        }

        /*!
         * @brief Get the 32 bit number of this IPv4 address in host byte order.
         * @return The 32 bit number of this IPv4 address in host byte order.
         */
        uint_type to_uint() const noexcept {
            return ip_be_number_;
        }

        /*!
         * @brief Check if this IPv4 is a loopback IPv4 address.
         * @return True if this IPv4 is a loopback IPv4 address,
         * otherwise false.
         */
        bool is_loopback() const noexcept {
            return bytes_storage_[0] == 127;
        }

        /*!
         * @brief Check if this IPv4 is a current network IPv4 address.
         * @return True if this IPv4 is a current network IPv4 address,
         * otherwise false.
         */
        bool is_current_network() const noexcept {
            return bytes_storage_[0] == 0;
        }

        /*!
         * @brief Check if this IPv4 is a private network IPv4 address.
         * @return True if this IPv4 is a private network IPv4 address,
         * otherwise false.
         */
        bool is_private_network() const noexcept {
            // 10.0.0.0/8: 10.0.0.0 : 10.255.255.255
            if (bytes_storage_[0] == 10) {
                return true;
            }
            // 100.64.0.0/10: 100.64.0.0 : 100.127.255.255
            if (bytes_storage_[0] == 100 && bytes_storage_[1] >= 64 &&
                bytes_storage_[1] <= 127) {
                return true;
            }
            // 172.16.0.0/12: 172.16.0.0 : 172.31.255.255
            if (bytes_storage_[0] == 172 && bytes_storage_[1] >= 16 &&
                bytes_storage_[1] <= 37) {
                return true;
            }
            // 192.0.0.0/24: 192.0.0.0 : 192.0.0.255
            if (bytes_storage_[0] == 192 && bytes_storage_[1] == 0 &&
                bytes_storage_[2] == 0) {
                return true;
            }
            // 192.168.0.0/16: 192.168.0.0 : 192.168.255.255
            if (bytes_storage_[0] == 192 && bytes_storage_[1] == 168) {
                return true;
            }
            // 198.18.0.0/15: 198.18.0.0 : 198.19.255.255
            if (bytes_storage_[0] == 192 && bytes_storage_[1] >= 18 &&
                bytes_storage_[1] <= 19) {
                return true;
            }
            return false;
        }

        /*!
         * @brief Check if this IPv4 is a documentation IPv4 address.
         * @return True if this IPv4 is a documentation IPv4 address,
         * otherwise false.
         */
        bool is_documentation() const noexcept {
            // 192.0.2.0/24: 192.0.2.0 : 192.0.2.255
            if (bytes_storage_[0] == 192 && bytes_storage_[1] == 0 &&
                bytes_storage_[2] == 2) {
                return true;
            }
            // 198.51.100.0/24: 198.51.100.0 : 198.51.100.255
            if (bytes_storage_[0] == 192 && bytes_storage_[1] == 51 &&
                bytes_storage_[2] == 100) {
                return true;
            }
            // 203.0.113.0/24: 203.0.113.0 : 203.0.113.255
            if (bytes_storage_[0] == 203 && bytes_storage_[1] == 0 &&
                bytes_storage_[2] == 113) {
                return true;
            }
            // 233.252.0.0/24: 233.252.0.0-233.252.0.255
            if (bytes_storage_[0] == 233 && bytes_storage_[1] == 252 &&
                bytes_storage_[2] == 0) {
                return true;
            }
            return false;
        }

        /*!
         * @brief Check if this IPv4 is a benchmarking IPv4 address.
         * @return True if this IPv4 is a benchmarking IPv4 address,
         * otherwise false.
         */
        bool is_benchmarking() const noexcept {
            // 198.18.0.0/15: 198.18.0.0 : 198.19.255.255
            return bytes_storage_[0] == 192 && bytes_storage_[1] >= 18 &&
                   bytes_storage_[1] <= 19;
        }

        /*!
         * @brief Check if this IPv4 is (0.0.0.0).
         * @return True if this IPv4 is (0.0.0.0), otherwise false.
         */
        bool is_this_host() const noexcept {
            return ip_be_number_ == 0;
        }

        /*!
         * @brief Check if this IPv4 is a shared address space IPv4 address.
         * @return True if this IPv4 is a shared address space IPv4 address,
         * otherwise false.
         */
        bool is_shared_address_space() const noexcept {
            // 100.64.0.0/10
            return bytes_storage_[0] == 100 && (bytes_storage_[1] & 64) == 64;
        }

        /*!
         * @brief Check if this IPv4 is a link local IPv4 address.
         * @return True if this IPv4 is a link local IPv4 address,
         * otherwise false.
         */
        bool is_link_local() const noexcept {
            // 169.254.0.0/16
            return bytes_storage_[0] == 169 && bytes_storage_[1] == 254;
        }

        /*!
         * @brief Check if this IPv4 is a multicast IPv4 address.
         * @return True if this IPv4 is a multicast IPv4 address,
         * otherwise false.
         */
        bool is_multicast() const noexcept {
            // 224.0.0.0 to 239.255.255.255
            return bytes_storage_[0] >= 224 && bytes_storage_[0] <= 239;
        }

        /*!
         * @brief Check if this IPv4 is a dummy IPv4 address.
         * @return True if this IPv4 is a dummy IPv4 address,
         * otherwise false.
         */
        bool is_dummy_address() const noexcept {
            // 192.0.0.8/32
            return bytes_storage_[0] == 192 && bytes_storage_[1] == 0 &&
                   bytes_storage_[2] == 0 && bytes_storage_[3] == 8;
        }

        /*!
         * @brief Check if this IPv4 is a port control protocol anycast IPv4
         * address.
         * @return True if this IPv4 is a port control protocol anycast IPv4
         * address, otherwise false.
         */
        bool is_port_control_protocol_anycast() const noexcept {
            // 192.0.0.9/32
            return bytes_storage_[0] == 192 && bytes_storage_[1] == 0 &&
                   bytes_storage_[2] == 0 && bytes_storage_[3] == 9;
        }

        /*!
         * @brief Check if this IPv4 is a AS112 v4 IPv4 address.
         * @return True if this IPv4 is a AS112 v4 IPv4 address,
         * otherwise false.
         */
        bool is_as112_v4() const noexcept {
            // 192.31.196.0/24
            return bytes_storage_[0] == 192 && bytes_storage_[1] == 31 &&
                   bytes_storage_[2] == 196;
        }

        /*!
         * @brief Check if this IPv4 is a direct delegation AS112 v4 IPv4
         * address.
         * @return True if this IPv4 is a direct delegation AS112 v4 IPv4
         * address, otherwise false.
         */
        bool is_direct_delegation_as112_v4() const noexcept {
            // 192.175.48.0/24
            return bytes_storage_[0] == 192 && bytes_storage_[1] == 175 &&
                   bytes_storage_[2] == 48;
        }

        /*!
         * @brief Check if this IPv4 is a reserved IPv4 address.
         * @return True if this IPv4 is a reserved IPv4 address,
         * otherwise false.
         */
        bool is_reserved() const noexcept {
            // 240.0.0.0/4
            return (bytes_storage_[0] & 240) == 240;
        }

        /*!
         * @brief Check if this IPv4 is a limited broadcast IPv4 address.
         * @return True if this IPv4 is a limited broadcast IPv4 address,
         * otherwise false.
         */
        bool is_limited_broadcast() const noexcept {
            // 255.255.255.255/32
            return bytes_storage_[0] == 255 && bytes_storage_[1] == 255 &&
                   bytes_storage_[2] == 255 && bytes_storage_[3] == 255;
        }

        /*!
         * @brief Check if this IPv4 is (0.0.0.0).
         * @return True if this IPv4 is (0.0.0.0), otherwise false.
         */
        bool is_any() const noexcept {
            return ip_be_number_ == 0;
        }

        /*!
         * @brief Parse a string IPv4.
         * On failure, false is returned and the current address is not changed.
         * @param ipstr The IPv4 string.
         * @return True on successful parsing, otherwise false.
         */
        RAD_EXPORT_DECL bool from_string(std::string_view ip_str) noexcept;

        /*!
         * @brief Serialize this IPv4 to string.
         * @return The serialized IPv4.
         */
        RAD_EXPORT_DECL std::string to_string() const noexcept;

        /*!
         * @brief Obtain an IPv4 address that represents any address.
         * @return An IPv4 address that represents any address.
         */
        static ipv4 any() noexcept {
            return ipv4{};
        }

        /*!
         * @brief Obtain an IPv4 address that represents the loopback address.
         * @return An IPv4 address that represents the loopback address.
         */
        static ipv4 loopback() noexcept {
            return ipv4{{127, 0, 0, 1}};
        }

        /*!
         * @brief Get a hash for this IPv4 address.
         * @return The hash of this IPv4 address, which equals
         * the unsigned integer in host byte order.
         */
        std::size_t hash() const noexcept {
            return ip_be_number_;
        }

        /*!
         * @brief Compare two IPv4 addresses for equality.
         * @param lhs The first IPv4 address.
         * @param rhs The second IPv4 address.
         * @return True if the two addresses are equal, otherwise false.
         */
        friend bool operator==(const ipv4& lhs, const ipv4& rhs) noexcept {
            return lhs.ip_be_number_ == rhs.ip_be_number_;
        }

        /*!
         * @brief Compare two IPv4 addresses for ordering.
         * @param lhs The first IPv4 address.
         * @param rhs The second IPv4 address.
         * @return True if the first address is less than the second.
         */
        friend bool operator<(const ipv4& lhs, const ipv4& rhs) noexcept {
            return lhs.ip_be_number_ < rhs.ip_be_number_;
        }

        /*!
         * @brief Compare two IPv4 addresses for ordering.
         * @param lhs The first IPv4 address.
         * @param rhs The second IPv4 address.
         * @return True if the first address is greater than the second.
         */
        friend bool operator>(const ipv4& lhs, const ipv4& rhs) noexcept {
            return lhs.ip_be_number_ > rhs.ip_be_number_;
        }

        /*!
         * @brief Compare two IPv4 addresses for ordering.
         * @param lhs The first IPv4 address.
         * @param rhs The second IPv4 address.
         * @return True if the first address is less than or equal to the
         * second.
         */
        friend bool operator<=(const ipv4& lhs, const ipv4& rhs) noexcept {
            return lhs.ip_be_number_ <= rhs.ip_be_number_;
        }

        /*!
         * @brief Compare two IPv4 addresses for ordering.
         * @param lhs The first IPv4 address.
         * @param rhs The second IPv4 address.
         * @return True if the first address is greater than or equal to the
         * second.
         */
        friend bool operator>=(const ipv4& lhs, const ipv4& rhs) noexcept {
            return lhs.ip_be_number_ >= rhs.ip_be_number_;
        }

    private:
        union {
            bytes_type bytes_storage_;
            beu32 ip_be_number_ = beu32{0}; // default is INADDR_ANY
        };
    };

    static_assert(sizeof(ipv4) == 4, "sizeof(ipv4) == 4");

    /*!
     * @brief IPv6 address.
     *
     * This class provides the ability to use and manipulate IP version 6
     * addresses.
     */
    class ipv6 {
    public:
        /*!
         * @brief The type used to represent an IPv6 address as an array of
         * bytes. (16 bytes).
         */
        using bytes_type = std::array<uint8_t, 16>;

        /*!
         * @brief The type used to represent an IPv6 address as an array of
         * words. (8 words, each word is 2 bytes).
         */
        using words_type = std::array<uint16_t, 8>;

        /*!
         * @brief The type used to represent an IPv6 address as an array of
         * unsigned integers. (4 unsigned integers, each unsigned integers is 4
         * bytes).
         */
        using uints_type = std::array<uint32_t, 4>;

        /*!
         * @brief Construct an IPv6 address that represents any address.
         */
        ipv6() : uints_{} {};

        /*!
         * @brief Parse a string IPv6.
         * On failure, an exception is thrown.
         * @param ipstr The IPv6 string.
         */
        explicit ipv6(std::string_view ipstr) {
            if (!from_string(ipstr)) {
                throw std::system_error(std::make_error_code(
                    std::errc::address_family_not_supported));
            }
        }

        /*!
         * @brief Construct an IPv6 address from 4 unsigned integers
         * array.
         * @param uints The 4 unsigned integers array.
         * Each unsigned integer is in host byte order.
         */
        explicit ipv6(const uints_type& uints) noexcept {
            for (size_t i = 0; i < uints.size(); ++i) {
                uints_[i] = uints[i];
            }
        }

        /*!
         * @brief Construct an IPv6 address from 8 words
         * array.
         * @param words The 8 words array.
         * Each words is in host byte order.
         */
        explicit ipv6(const words_type& words) noexcept {
            for (size_t i = 0; i < words.size(); ++i) {
                words_[i] = words[i];
            }
        }

        /*!
         * @brief Construct an IPv6 with the ip 16 bytes in network byte order.
         * @param ip_bytes The ip 16 bytes in network byte order.
         */
        explicit ipv6(const bytes_type& ip_bytes) noexcept
            : bytes_storage_{ip_bytes} {
        }

        /*!
         * @brief Create an IPv4-mapped IPv6 address from an IPv4 address.
         * @param v4_address The IPv4 address.
         */
        explicit ipv6(const ipv4& v4_address) : ipv6() {
            // 0:0:0:0:0:FFFF:w.x.y.z
            uints_.back() = v4_address.to_uint();
            words_[5] = 0xffff;
        }

        /*!
         * @brief Parse a string IPv6.
         * On failure, false is returned and the current address is not changed.
         * @param ipstr The IPv6 string.
         * @return True on successful parsing, otherwise false.
         */
        RAD_EXPORT_DECL bool from_string(std::string_view ipstr) noexcept;

        /*!
         * @brief Serialize this IPv6 address to string.
         * @return The serialized IPv6.
         */
        RAD_EXPORT_DECL std::string to_string() const;

        /*!
         * @brief Get the 16 bytes of this IPv6 address in network byte order.
         * @return The 16 bytes of this IPv6 address in network byte order.
         */
        bytes_type to_bytes() const noexcept {
            return bytes_storage_;
        }

        /*!
         * @brief Get the 8 words of this IPv6 address in host byte order.
         * @return The 8 words of this IPv6 address in host byte order.
         */
        words_type to_words() const noexcept {
            words_type w;
            for (size_t i = 0; i < words_.size(); ++i) {
                w[i] = words_[i];
            }
            return w;
        }

        /*!
         * @brief Get the 4 unsigned integers of this IPv6 address in host byte
         * order.
         * @return The 4 unsigned integers of this IPv6 address in host byte
         * order.
         */
        uints_type to_uints() const noexcept {
            uints_type d;
            for (size_t i = 0; i < uints_.size(); ++i) {
                d[i] = uints_[i];
            }
            return d;
        }

        /*!
         * @brief Check if this IPv6 is a loopback IPv6 address.
         * @return True if this IPv6 is a loopback IPv6 address,
         * otherwise false.
         */
        bool is_loopback() const noexcept {
            return uints_[3] == 1 && !uints_[0] && !uints_[1] && !uints_[2];
        }

        /*!
         * @brief Check if this IPv6 represents any address [::].
         * @return True if this IPv6 represents any address [::], otherwise
         * false.
         */
        bool is_any() const noexcept {
            return !uints_[0] && !uints_[1] && !uints_[2] && !uints_[3];
        }

        /*!
         * @brief Determine whether this address is a mapped IPv4 address.
         * @return True if this address is a mapped IPv4 address, otherwise
         * false.
         */
        bool is_v4_mapped() const noexcept {
            // 0:0:0:0:0:FFFF:w.x.y.z
            return words_[5] == 0xffff && words_[4] == 0 && uints_[0] == 0 &&
                   uints_[1] == 0;
        }

        /*!
         * @brief Obtain an IPv6 address that represents the loopback address.
         * @return An IPv6 address that represents the loopback address.
         */
        static ipv6 loopback() noexcept {
            ipv6 ip{};
            ip.words_.back() = 1;
            return ip;
        }

        /*!
         * @brief Obtain an IPv6 address that represents any address [::].
         * @return An IPv6 address that represents any address [::].
         */
        static ipv6 any() noexcept {
            return ipv6{};
        }

        /*!
         * @brief Get a hash for this IPv6 address.
         * @return The hash of this IPv6 address, which equals
         * the XORing of the 4 unsigned integers in host byte order.
         */
        std::size_t hash() const noexcept {
            auto uints = to_uints();
            const std::size_t result =
                uints[0] ^ uints[1] ^ uints[2] ^ uints[3];
            return result;
        }

        /*!
         * @brief Compare two IPv6 addresses for equality.
         * @param lhs The first IPv6 address.
         * @param rhs The second IPv6 address.
         * @return True if the two addresses are equal, otherwise false.
         */
        friend bool operator==(const ipv6& lhs, const ipv6& rhs) noexcept {
            for (std::size_t i = 0; i < 4; ++i) {
                if (lhs.uints_[i] != rhs.uints_[i]) {
                    return false;
                }
            }
            return true;
        }

        /*!
         * @brief Compare two IPv6 addresses for ordering.
         * @param lhs The first IPv6 address.
         * @param rhs The second IPv6 address.
         * @return True if the first address is less than the second.
         */
        friend bool operator<(const ipv6& lhs, const ipv6& rhs) noexcept {
            for (std::size_t i = 0; i < 4; ++i) {
                if (lhs.uints_[i] < rhs.uints_[i]) {
                    return true;
                }
                else if (lhs.uints_[i] > rhs.uints_[i]) {
                    return false;
                }
            }
            return false;
        }

        /*!
         * @brief Compare two IPv6 addresses for ordering.
         * @param lhs The first IPv6 address.
         * @param rhs The second IPv6 address.
         * @return True if the first address is greater than the second.
         */
        friend bool operator>(const ipv6& lhs, const ipv6& rhs) noexcept {
            for (std::size_t i = 0; i < 4; ++i) {
                if (lhs.uints_[i] > rhs.uints_[i]) {
                    return true;
                }
                else if (lhs.uints_[i] < rhs.uints_[i]) {
                    return false;
                }
            }
            return false;
        }

    private:
        union {
            bytes_type bytes_storage_;
            std::array<beu16, 8> words_;
            std::array<beu32, 4> uints_ = {0}; // default is IN6ADDR_ANY_INIT
        };
    };

    static_assert(sizeof(ipv6) == 16, "sizeof(ipv6) == 16");

    /*!
     * @brief A 16 bit socket port in network byte order.
     */
    struct socket_port : public beu16 {
        using beu16::beu16;

        using beu16::operator=;

        /*!
         * @brief Get a port that reperesents any port in host byte order.
         * @return The port that reperesents any port, which is 0 port.
         */
        static std::uint16_t any() noexcept {
            return 0;
        }
    };

    static_assert(sizeof(socket_port) == 2, "sizeof(socket_port) == 2");

    /*!
     * @brief Check if a string is valid IPv4 address.
     * @param ip_str The string to check.
     * @return True if the string is valid IPv4 address, otherwise false.
     */
    RAD_EXPORT_DECL bool is_valid_ipv4(std::string_view ip_str) noexcept;

    /*!
     * @brief Check if a string is valid IPv6 address.
     * @param ip_str The string to check.
     * @return True if the string is valid IPv6 address, otherwise false.
     */
    RAD_EXPORT_DECL bool is_valid_ipv6(std::string_view ip_str) noexcept;

    /*!
     * @brief Check if a string is valid IPv4 or IPv6 address.
     * @param ip_str The string to check.
     * @return True if the string is valid IPv4 or IPv6 address, otherwise
     * false.
     */
    RAD_EXPORT_DECL bool is_valid_ip(std::string_view ip_str) noexcept;

    class ipv4_endpoint;
    class ipv6_endpoint;

    /*!
     * @brief Prefix of internet socket addresses.
     * It contains the address family and the port in network byte order.
     * The address family will be either ipv4 or ipv6.
     *
     * Users should use endpoint, ipv4_endpoint and ipv6_endpoint instead.
     */
    class socket_address_prefix {
        friend ipv4_endpoint;
        friend ipv6_endpoint;

    private:
        /*!
         * @brief Construct an invalid socket address prefix.
         */
        socket_address_prefix() = default;

        /*!
         * @brief Construct a socket address from family and port.
         * @param type The address family which must be either ipv4 or ipv6.
         * @param port The port in host byte order.
         */
        socket_address_prefix(address_type type, uint16_t port) noexcept
            : type_{type}, port_{port} {
        }

    public:
        /*!
         * @brief The type used as the socket address length type.
         */
        using size_type = socklen_t;

        /*!
         * @brief Get the length of this socket address.
         * @return The length of this socket address.
         */
        size_type size() const noexcept;

        /*!
         * @brief Get the address family of this address.
         * @return The address family of this address.
         */
        constexpr address_family family() const noexcept {
            return static_cast<address_family>(family_);
        }

        /*!
         * @brief Get the port in host byte order.
         * @return The port in host byte order.
         */
        uint16_t port() const noexcept {
            return port_;
        }

        /*!
         * @brief Check if this address is an IPv4 address.
         * @return True if this address is an IPv4 address, otherwise false.
         */
        constexpr bool is_v4() const noexcept {
            return type_ == address_type::ipv4;
        }

        /*!
         * @brief Check if this address is an IPv6 address.
         * @return True if this address is an IPv6 address, otherwise false.
         */
        constexpr bool is_v6() const noexcept {
            return type_ == address_type::ipv6;
        }

    private:
#if !defined(_WIN32) && !defined(__linux__)
        uint8_t sa_len_ = 0;
#endif
        union {
            address_type type_;
            std::underlying_type_t<address_family> family_;
        };
        socket_port port_;
    };

    /*!
     * @brief IPv4 address and port in network byte order.
     */
    class ipv4_endpoint : public socket_address_prefix {
    public:
        /*!
         * @brief The type used to represent an IPv4 address as an array of
         * bytes. (4 bytes).
         */
        using bytes_type = ipv4::bytes_type;
        /*!
         * @brief The type used to represent an IPv4 address as an unsigned
         * integer.
         */
        using uint_type = ipv4::uint_type;
        /*!
         * @brief The type used as the socket address length type.
         */
        using size_type = socklen_t;

        /*!
         * @brief Create any IPv4 address (0.0.0.0) with any port (0) in host
         * byte order.
         */
        ipv4_endpoint() : socket_address_prefix(address_type::ipv4, 0) {
        }

        /*!
         * @brief Construct using an IPv4 address and a port.
         * @param ip The IPv4 address.
         * @param port The port in host byte order.
         */
        ipv4_endpoint(const net::ipv4& ip, uint16_t port) noexcept
            : socket_address_prefix(address_type::ipv4, port), ip_{ip} {
        }

        /*!
         * @brief Parse a string IPv4.
         * On failure, an exception is thrown.
         * @param ipstr The IPv4 string.
         * @param port The port in host byte order.
         */
        ipv4_endpoint(std::string_view ipstr, uint16_t port) noexcept
            : socket_address_prefix(address_type::ipv4, port), ip_{ipstr} {
        }

        /*!
         * @brief Construct an IPv4 with the ip 32 bit number in host byte
         * order.
         * @param ip_number The ip 32 bit numer in host byte order.
         * @param port The port in host byte order.
         */
        ipv4_endpoint(uint_type ip_number, uint16_t port) noexcept
            : socket_address_prefix(address_type::ipv4, port), ip_{ip_number} {
        }

        /*!
         * @brief Construct an IPv4 with the ip 4 bytes in network byte order.
         *
         * For address (127.1.2.0) ip_bytes[0] = 127, and ip_bytes[3] = 0.
         * @param ip_bytes The ip 4 bytes in network byte order.
         * @param port The port in host byte order.
         *
         */
        ipv4_endpoint(const bytes_type& ip_bytes, uint16_t port) noexcept
            : socket_address_prefix(address_type::ipv4, port), ip_{ip_bytes} {
        }

        /*!
         * @brief Get the size of this IPv4 endpoint in bytes.
         * @return The size of this IPv4 endpoint in bytes.
         */
        static constexpr size_type size() noexcept {
            return sizeof(ipv4_endpoint);
        }

        /*!
         * @brief Create any IPv4 address (0.0.0.0) with any port (0) in host
         * byte order.
         * @return IPv4 address (0.0.0.0) with port (0).
         */
        static ipv4_endpoint any() noexcept {
            return ipv4_endpoint{ipv4::any(), socket_port::any()};
        }

        /*!
         * @brief Get a reference to the IPv4 address.
         * @return A reference to the IPv4 address.
         */
        ipv4& ip() noexcept {
            return ip_;
        }

        /*!
         * @brief Get a const reference to the IPv4 address.
         * @return A const reference to the IPv4 address.
         */
        const ipv4& ip() const noexcept {
            return ip_;
        }

        /*!
         * @brief Serialize this IPv4 address and port to string.
         * @return The serialized IPv4 address and port separated by colon (:).
         */
        std::string to_string() const {
            return ip_.to_string() + ':' + std::to_string(port());
        }

        /*!
         * @brief Get a hash for this IPv4 endpoint.
         * @return The hash of this IPv4 endpoint.
         */
        std::size_t hash() const noexcept {
            return ip_.hash() + port();
        }

        /*!
         * @brief Compare two IPv4 endpoints for equality.
         * @param lhs The first IPv4 endpoint.
         * @param rhs The second IPv4 endpoint.
         * @return True if the two endpoints are equal, otherwise false.
         */
        friend bool operator==(const ipv4_endpoint& lhs,
                               const ipv4_endpoint& rhs) noexcept {
            return lhs.port() == rhs.port() && lhs.ip_ == rhs.ip_;
        }

        /*!
         * @brief Compare two IPv4 endpoints for ordering.
         * @param lhs The first IPv4 endpoint.
         * @param rhs The second IPv4 endpoint.
         * @return True if the first endpoint is less than the second.
         */
        friend bool operator<(const ipv4_endpoint& lhs,
                              const ipv4_endpoint& rhs) noexcept {
            if (lhs.port() < rhs.port()) {
                return true;
            }
            else if (lhs.port() > rhs.port()) {
                return false;
            }
            return lhs.ip_ < rhs.ip_;
        }

        /*!
         * @brief Compare two IPv4 endpoints for ordering.
         * @param lhs The first IPv4 endpoint.
         * @param rhs The second IPv4 endpoint.
         * @return True if the first endpoint is greater than the second.
         */
        friend bool operator>(const ipv4_endpoint& lhs,
                              const ipv4_endpoint& rhs) noexcept {
            if (lhs.port() > rhs.port()) {
                return true;
            }
            else if (lhs.port() > rhs.port()) {
                return false;
            }
            return lhs.ip_ > rhs.ip_;
        }

    private:
        net::ipv4 ip_;
        [[maybe_unused]] char sin_zero_[8] = {0};
    };

    /*!
     * @brief IPv6 address, port and scope id in network byte order.
     */
    class ipv6_endpoint : public socket_address_prefix {
    public:
        /*!
         * @brief The type used to represent an IPv6 address as an array of
         * bytes. (16 bytes).
         */
        using bytes_type = ipv6::bytes_type;
        /*!
         * @brief The type used to represent an IPv6 address as an array of
         * words. (8 words, each word is 2 bytes).
         */
        using words_type = ipv6::words_type;
        /*!
         * @brief The type used to represent an IPv6 address as an array of
         * unsigned integers. (4 unsigned integers, each unsigned integers is 4
         * bytes).
         */
        using uints_type = ipv6::uints_type;

        /*!
         * @brief Construct using an IPv6 address and a port.
         * @param ip The IPv6 address.
         * @param port The port in host byte order.
         * @param scope_id The scope id in host byte order.
         */
        ipv6_endpoint(const ipv6& ip, uint16_t port, uint32_t scope_id = 0)
            : socket_address_prefix(address_type::ipv6, port), ip_{ip},
              scope_id_{scope_id} {
        }

        /*!
         * @brief Parse a string IPv6.
         * On failure, an exception is thrown.
         * @param ipstr The IPv6 string.
         * @param port The port in host byte order.
         * @param scope_id The scope id in host byte order.
         */
        ipv6_endpoint(std::string_view ipstr, uint16_t port,
                      uint32_t scope_id = 0)
            : socket_address_prefix(address_type::ipv6, port), ip_{ipstr},
              scope_id_{scope_id} {
        }

        /*!
         * @brief Construct an IPv6 address from 4 unsigned integers
         * array.
         * @param uints The 4 unsigned integers array.
         * Each unsigned integer is in host byte order.
         * @param port The port in host byte order.
         * @param scope_id The scope id in host byte order.
         */
        ipv6_endpoint(const uints_type& uints, uint16_t port,
                      uint32_t scope_id = 0)
            : socket_address_prefix(address_type::ipv6, port), ip_{uints},
              scope_id_{scope_id} {
        }

        /*!
         * @brief Construct an IPv6 address from 8 words
         * array.
         * @param words The 8 words array.
         * Each words is in host byte order.
         * @param port The port in host byte order.
         * @param scope_id The scope id in host byte order.
         */
        ipv6_endpoint(const words_type& words, uint16_t port,
                      uint32_t scope_id = 0)
            : socket_address_prefix(address_type::ipv6, port), ip_{words},
              scope_id_{scope_id} {
        }

        /*!
         * @brief Construct an IPv6 with the ip 16 bytes in network byte order.
         * @param ip_bytes The ip 16 bytes in network byte order.
         * @param port The port in host byte order.
         * @param scope_id The scope id in host byte order.
         */
        ipv6_endpoint(const bytes_type& bytes, uint16_t port,
                      uint32_t scope_id = 0)
            : socket_address_prefix(address_type::ipv6, port), ip_{bytes},
              scope_id_{scope_id} {
        }

        /*!
         * @brief Get the size of this IPv6 endpoint in bytes.
         * @return The size of this IPv6 endpoint in bytes.
         */
        static constexpr size_type size() noexcept {
            return sizeof(ipv6_endpoint);
        }

        /*!
         * @brief Create any IPv6 address [::] with any port (0) in host
         * byte order.
         * @return IPv6 address [::] with port (0).
         */
        static ipv6_endpoint any() noexcept {
            return ipv6_endpoint{ipv6::any(), socket_port::any()};
        }

        /*!
         * @brief Get the flow info.
         * @return The flow info.
         */
        uint32_t flowinfo() const noexcept {
            return flowinfo_;
        }

        /*!
         * @brief Get the scope id in host byte order.
         * @return The scope id in host byte order.
         */
        uint32_t scope_id() const noexcept {
            return scope_id_;
        }

        /*!
         * @brief Get a reference to the IPv6 address.
         * @return A reference to the IPv6 address.
         */
        ipv6& ip() noexcept {
            return ip_;
        }

        /*!
         * @brief Get a const reference to the IPv6 address.
         * @return A const reference to the IPv6 address.
         */
        const ipv6& ip() const noexcept {
            return ip_;
        }

        /*!
         * @brief Serialize this IPv6 address and port to string.
         * @return The serialized IPv6 address and port separated by colon (:).
         */
        std::string to_string() const noexcept {
            return ip_.to_string() + ':' + std::to_string(port());
        }

        /*!
         * @brief Get a hash for this IPv6 endpoint.
         * @return The hash of this IPv6 endpoint.
         */
        std::size_t hash() const noexcept {
            return (ip_.hash() + port()) ^ (scope_id() + flowinfo());
        }

        /*!
         * @brief Compare two IPv6 endpoints for equality.
         * @param lhs The first IPv6 endpoint.
         * @param rhs The second IPv6 endpoint.
         * @return True if the two endpoints are equal, otherwise false.
         */
        friend bool operator==(const ipv6_endpoint& lhs,
                               const ipv6_endpoint& rhs) noexcept {
            return lhs.port() == rhs.port() && lhs.flowinfo_ == rhs.flowinfo_ &&
                   lhs.scope_id() == rhs.scope_id() && lhs.ip_ == rhs.ip_;
        }

        /*!
         * @brief Compare two IPv6 endpoints for ordering.
         * @param lhs The first IPv6 endpoint.
         * @param rhs The second IPv6 endpoint.
         * @return True if the first endpoint is less than the second.
         */
        friend bool operator<(const ipv6_endpoint& lhs,
                              const ipv6_endpoint& rhs) noexcept {
            if (lhs.port() < rhs.port()) {
                return true;
            }
            else if (lhs.port() > rhs.port()) {
                return false;
            }
            if (lhs.flowinfo_ < rhs.flowinfo_) {
                return true;
            }
            else if (lhs.flowinfo_ > rhs.flowinfo_) {
                return false;
            }
            if (lhs.scope_id_ < rhs.scope_id_) {
                return true;
            }
            else if (lhs.scope_id_ > rhs.scope_id_) {
                return false;
            }
            return lhs.ip_ < rhs.ip_;
        }

        /*!
         * @brief Compare two IPv6 endpoints for ordering.
         * @param lhs The first IPv6 endpoint.
         * @param rhs The second IPv6 endpoint.
         * @return True if the first endpoint is greater than the second.
         */
        friend bool operator>(const ipv6_endpoint& lhs,
                              const ipv6_endpoint& rhs) noexcept {
            if (lhs.port() > rhs.port()) {
                return true;
            }
            else if (lhs.port() < rhs.port()) {
                return false;
            }
            if (lhs.flowinfo_ > rhs.flowinfo_) {
                return true;
            }
            else if (lhs.flowinfo_ < rhs.flowinfo_) {
                return false;
            }
            if (lhs.scope_id_ > rhs.scope_id_) {
                return true;
            }
            else if (lhs.scope_id_ < rhs.scope_id_) {
                return false;
            }
            return lhs.ip_ > rhs.ip_;
        }

    private:
        beu32 flowinfo_ = 0;

        ipv6 ip_;

        struct scope_id_type {
            union {
                struct {
                    uint32_t zone : 28;
                    uint32_t level : 4;
                };
                uint32_t value = 0;
            };
        };

        beu32 scope_id_ = 0;
    };

    /*!
     * @brief Type tag to construct endpoint from IPv4 address.
     */
    struct init_ipv4_t {};

    /*!
     * @brief Type tag to construct endpoint from IPv6 address.
     */
    struct init_ipv6_t {};

    /*!
     * @brief Type tag to construct endpoint from raw sockaddr.
     */
    struct init_sockaddr_t {};

    /*!
     * @brief Used to construct endpoint from IPv4 address.
     */
    inline constexpr init_ipv4_t init_ipv4;

    /*!
     * @brief Used to construct endpoint from IPv6 address.
     */
    inline constexpr init_ipv6_t init_ipv6;

    /*!
     * @brief Used to construct endpoint from raw sockaddr.
     */
    inline constexpr init_sockaddr_t init_sockaddr;

    /*!
     * @brief An endpoint represents an either ipv4 or ipv6 with a port.
     *
     * Endpoint and all its members are relocatable and trivially copyable
     * and destructible.
     */
    class endpoint {
    public:
        using size_type = socklen_t;
        using ipv4_bytes_type = ipv4::bytes_type;
        using ipv6_bytes_type = ipv6::bytes_type;

        // size is calculated based on the family
        static constexpr bool resizable = false;

        /*!
         * @brief Default constructor constructs an invalid
         * endpoint.
         */
        endpoint() {
        }

        /*!
         * @brief Copy constructor performs byte wise copy
         * @param other endpoint to copy
         */
        endpoint(const endpoint& other) = default;

        /*!
         * @brief Construct an ipv4 endpoint from ipv4_endpoint.
         * @param v4addr the ipv4 address and port to use
         */
        endpoint(const ipv4_endpoint& v4addr) noexcept
            : type_{address_type::ipv4}, port_{v4addr.port()},
              v4_storage{v4addr.ip()} {
        }

        /*!
         * @brief Construct an ipv4 endpoint from  ipv4 and a
         * port number
         * @param v4addr the ipv4 address to use
         * @param port the port to use, passed in native host
         * byte order
         */
        endpoint(const ipv4& v4addr, uint16_t port) noexcept
            : type_{address_type::ipv4}, port_{port}, v4_storage{v4addr} {
        }

        /*!
         * @brief Construct an ipv4 endpoint from ipv4 4 bytes
         * array and a port number
         * @param ipv4_bytes the ipv4 4 bytes array to use
         * @param port the port to use, passed in native host
         * byte order
         */
        endpoint(const ipv4_bytes_type& ipv4_bytes, uint16_t port) noexcept
            : type_{address_type::ipv4}, port_{port},
              v4_storage{ipv4{ipv4_bytes}} {
        }

        /*!
         * @brief Construct an ipv4 endpoint from an ipv4 string
         * and a port number
         * @param  tag to indicate that the ip in the string is
         * ipv4
         * @param ipstr the ipv4 string
         * @param port the port to use, passed in native host
         * byte order
         */
        endpoint(init_ipv4_t, std::string_view ipstr, uint16_t port)
            : type_{address_type::ipv4}, port_{port}, v4_storage{ipv4{ipstr}} {
        }

        /*!
         * @brief Construct an ipv6 endpoint from ipv6_endpoint.
         * @param v6addr the ipv6 address and port to use
         */
        endpoint(const ipv6_endpoint& v6addr) noexcept
            : type_{address_type::ipv6}, port_{v6addr.port()},
              v6_storage{v6addr.ip()} {
        }

        /*!
         * @brief Construct an ipv6 endpoint from  ipv6 and a
         * port number
         * @param v6addr the ipv6 address to use
         * @param port the port to use, passed in native host
         * byte order
         */
        endpoint(const ipv6& v6addr, uint16_t port) noexcept
            : type_{address_type::ipv6}, port_{port}, v6_storage{v6addr} {
        }

        /*!
         * @brief Construct an ipv6 endpoint from ipv6 16 bytes
         * array and a port number
         * @param ipv6_bytes the ipv6 16 bytes array to use
         * @param port the port to use, passed in native host
         * byte order
         */
        endpoint(const ipv6_bytes_type& ipv6_bytes, uint16_t port) noexcept
            : type_{address_type::ipv6}, port_{port},
              v6_storage{ipv6{ipv6_bytes}} {
        }

        /*!
         * @brief Construct an ipv6 endpoint from an ipv6 string
         * and a port number
         * @param  tag to indicate that the ip in the string is
         * ipv6
         * @param ipstr the ipv6 string
         * @param port the port to use, passed in native host
         * byte order
         */
        endpoint(init_ipv6_t, std::string_view ipstr, uint16_t port)
            : type_{address_type::ipv6}, port_{port}, v6_storage{ipv6{ipstr}} {
        }

        /*!
         * @brief Attempt to Construct an ipv6 or ipv4 endpoint
         * from a string and a port number. If the string is not
         * a valid ipv4 or ipv6 an exception is thrown.
         * is_valid_ipv4() and is_valid_ipv6() are used to check
         * for the validity of the ip
         * @param ipstr the ip string
         * @param port the port to use, passed in native host
         * byte order
         */
        endpoint(std::string_view ipstr, uint16_t port) {
            if (is_valid_ipv4(ipstr)) {
                *this = endpoint(init_ipv4, ipstr, port);
            }
            else if (is_valid_ipv6(ipstr)) {
                *this = endpoint(init_ipv6, ipstr, port);
            }
            else {
                throw std::system_error(std::make_error_code(
                    std::errc::address_family_not_supported));
            }
        }

        /*!
         * @brief Construct an endpoint from a sockaddr pointer
         * with byte wise copy
         * @param  tag to indicate that it is a sockaddr
         * @param sockaddr a pointer to sockaddr
         * @param addrlen the size of sockaddr which must be
         * equal to or less than 28 bytes (the size of ipv6
         * address with port) otherwise the behavior is
         * undefined
         */
        endpoint(init_sockaddr_t, const void* sockaddr,
                 [[maybe_unused]] socklen_t addrlen) noexcept {
            assert(addrlen <= sizeof(*this));
            memcpy(this, sockaddr, static_cast<size_t>(addrlen));
            assert(is_valid_address(sockaddr, addrlen));
        }

        /*!
         * @brief Copy assignment performs byte wise copy
         * @param other the endpoint to copy
         * @return the endpoint itself
         */
        endpoint& operator=(const endpoint& other) noexcept = default;

        /*!
         * @brief Set the endpoint to an ipv4 address
         * @param v4addr the ipv4 address and port to use
         */

        void set_address(const ipv4_endpoint& v4addr) noexcept {
            new (this) endpoint(v4addr);
        }

        /*!
         * @brief Set the endpoint to an ipv6 address
         * @param v6addr the ipv6 address and port to use
         */
        void set_address(const ipv6_endpoint& v6addr) noexcept {
            new (this) endpoint(v6addr);
        }

        /*!
         * @brief Copy the contents of the memory pointed to by
         * sockaddr and sized by addrlen to the contents of this
         * endpoint
         * @param sockaddr the sockaddr pointer
         * @param addrlen the size of sockaddr which must be
         * equal to or less than 28 bytes (the size of ipv6
         * address with port) otherwise the behavior is
         * undefined
         */
        void set_address(const void* sockaddr, socklen_t addrlen) noexcept {
            new (this) endpoint(init_sockaddr, sockaddr, addrlen);
        }

        /*!
         * @brief Get an address of the stored socket address
         * suitable to be passed to system socket functions
         * @return an address of the stored socket address
         */
        void* address() noexcept {
            return this;
        }

        /*!
         * @brief Get an address of the stored socket address
         * suitable to be passed to system socket functions
         * @return an address of the stored socket address
         */
        const void* address() const noexcept {
            return this;
        }

        /*!
         * @brief Check whether the endpoint reperesents an ipv4
         * or ipv6 address or an invalid address
         * @return true if stores ipv4 or ipv6 address and false
         * otherwise
         */
        constexpr bool is_valid() const noexcept {
            return type_ != address_type::invalid;
        }

        /*!
         * @brief Check whether the endpoint reperesents an ipv4
         * address
         * @return true if stores ipv4 address and false
         * otherwise
         */
        constexpr bool is_v4() const noexcept {
            return type_ == address_type::ipv4;
        }

        /*!
         * @brief Check whether the endpoint reperesents an ipv6
         * address
         * @return true if stores ipv6 address and false
         * otherwise
         */
        constexpr bool is_v6() const noexcept {
            return type_ == address_type::ipv6;
        }

        /*!
         * @brief Cast the endpoint to an ipv4 address. Behavior
         * is undefined if the stored ip is not ipv4
         * @return a reference to this endpoint casted to
         * ipv4_address
         */
        ipv4_endpoint& as_ipv4() noexcept {
            assert(is_v4());
            return reinterpret_cast<ipv4_endpoint&>(*this);
        }

        /*!
         * @brief Cast the endpoint to an ipv4 address. Behavior
         * is undefined if the stored ip is not ipv4
         * @return a reference to this endpoint casted to
         * ipv4_address
         */
        const ipv4_endpoint& as_ipv4() const noexcept {
            assert(is_v4());
            return reinterpret_cast<const ipv4_endpoint&>(*this);
        }

        /*!
         * @brief Cast the endpoint to an ipv6 address. Behavior
         * is undefined if the stored ip is not ipv6
         * @return a reference to this endpoint casted to
         * ipv6_address
         */
        ipv6_endpoint& as_ipv6() noexcept {
            assert(is_v6());
            return reinterpret_cast<ipv6_endpoint&>(*this);
        }

        /*!
         * @brief Cast the endpoint to an ipv6 address. Behavior
         * is undefined if the stored ip is not ipv6
         * @return a reference to this endpoint casted to
         * ipv6_address
         */
        const ipv6_endpoint& as_ipv6() const noexcept {
            assert(is_v6());
            return reinterpret_cast<const ipv6_endpoint&>(*this);
        }

        /*!
         * @brief Get the size of stored address. Typically for
         * ipv4 16 bytes and otherwise 28 bytes this makes it
         * suitable to pass to os socket functions along with
         * address() that expects a sockaddr pointer and the
         * size of socket address
         * @return the size of stored address
         */
        size_type size() const noexcept {
            if (!is_valid() || is_v6()) {
                return sizeof(*this);
            }
            return sizeof(ipv4_endpoint);
        }

        /*!
         * @brief Get size of the max ip endpoint can store.
         * This is typically the size of ipv6 address and port
         * (28 bytes)
         * @return the size of the max ip endpoint can store
         */
        static constexpr size_type max_size() noexcept {
            return sizeof(endpoint);
        }

        // does nothing since size is calculated based on stored
        // ip family
        constexpr void resize(size_type) noexcept {
        }

        /*!
         * @brief Invalidate the stored ip if any
         */
        constexpr void reset() noexcept {
            type_ = address_type::invalid;
        }

        /*!
         * @brief Get the port of stored address. If the stored
         * address is invalid the returned value is meaningless
         * @return the port number in native host byte order
         */
        uint16_t port() const noexcept {
            return port_;
        }

        /*!
         * @brief Get the family of stored ip. If the stored
         * address is invalid the returned value is meaningless
         * @return the family of stored ip address
         */
        address_family family() const noexcept {
            return static_cast<address_family>(family_);
        }

        /*!
         * @brief format the endpoint (ip and port) to string.
         * Behavior is undefined if the stored address is
         * invalid. Formatted string has the format of: ip:port
         * @return formatted ip and port string
         */
        std::string to_string() const {
            assert(is_valid());
            if (is_v4()) {
                return as_ipv4().to_string();
            }
            else {
                return as_ipv6().to_string();
            }
        }

        /*!
         * @brief Check if a pointer to sockaddr points to a
         * valid ipv4 or ipv6 address
         * @param addr pointer to sockaddr
         * @param addrlen size of address
         * @return true if the address is valid and false
         * otherwise
         */
        static bool is_valid_address(const void* addr,
                                     socklen_t addrlen) noexcept {
            return (addrlen == sizeof(ipv4_endpoint) &&
                    reinterpret_cast<const endpoint*>(addr)->is_v4()) ||
                   (addrlen == sizeof(ipv6_endpoint) &&
                    reinterpret_cast<const endpoint*>(addr)->is_v6());
        }

        /*!
         * @brief Get a hash for this endpoint.
         * @return The hash of this endpoint.
         */
        std::size_t hash() const noexcept {
            if (is_v4()) {
                return as_ipv4().hash();
            }
            else if (is_v6()) {
                return as_ipv6().hash();
            }
            else {
                return 0;
            }
        }

        /*!
         * @brief Compare two endpoints for equality.
         * @param lhs The first endpoint.
         * @param rhs The second endpoint.
         * @return true if the two endpoints are equal and false
         * otherwise
         */
        friend bool operator==(const endpoint& lhs,
                               const endpoint& rhs) noexcept {
            if (lhs.family_ != rhs.family_) {
                return false;
            }
            if (lhs.is_v4()) {
                return lhs.as_ipv4() == rhs.as_ipv4();
            }
            else if (lhs.is_v6()) {
                return lhs.as_ipv6() == rhs.as_ipv6();
            }
            return true;
        }

        /*!
         * @brief Compare two endpoints for ordering.
         * @param lhs The first endpoint.
         * @param rhs The second endpoint.
         * @return True if the first endpoint is less than the second endpoint,
         * otherwise false.
         */
        friend bool operator<(const endpoint& lhs,
                              const endpoint& rhs) noexcept {
            if (lhs.family_ < rhs.family_) {
                return true;
            }
            else if (lhs.family_ > rhs.family_) {
                return false;
            }
            if (lhs.is_v4()) {
                assert(rhs.is_v4());
                return lhs.as_ipv4() < rhs.as_ipv4();
            }
            else if (lhs.is_v6()) {
                assert(rhs.is_v6());
                return lhs.as_ipv6() < rhs.as_ipv6();
            }
            else {
                return false;
            }
        }

    private:
#if !defined(_WIN32) && !defined(__linux__)
        uint8_t sa_len_ = 0;
#endif
        union {
            address_type type_ = address_type::invalid;
            std::underlying_type_t<address_family> family_;
        };

        socket_port port_;

        struct ipv4_storage {
            ipv4_storage() = default;

            ipv4_storage(const ipv4_storage&) = default;

            ipv4_storage(const ipv4& ip) noexcept : ip{ip} {
            }

            ipv4 ip;
            [[maybe_unused]] std::array<char, 8> sin_zero = {0};
        };

        struct ipv6_storage {
            ipv6_storage() = default;

            ipv6_storage(const ipv6_storage&) = default;

            ipv6_storage(const ipv6& ip) noexcept : ip{ip} {
            }

            [[maybe_unused]] beu32 flowinfo = 0;
            ipv6 ip;
            [[maybe_unused]] beu32 scope_id = 0;
        };

        union {
            ipv4_storage v4_storage;
            ipv6_storage v6_storage;
        };
    };

    /*!
     * @brief Get the system hostname.
     * @param ec Set to indicate error occured, if any.
     * @return The system hostname.
     */
    RAD_EXPORT_DECL std::string host_name(std::error_code& ec) noexcept;

    /*!
     * @brief Get the system hostname.
     * @return The system hostname.
     */
    inline std::string host_name() {
        std::error_code ec;
        auto name = host_name(ec);
        check_and_throw(ec, __func__);
        return name;
    }

#ifdef _WIN32
    /*!
     * @brief RAII wrapper for socket handle.
     */
    class socket_handle
        : public std::unique_ptr<
              void, // ignored, the pointer type is used from the deleter
              os::zero_neg_handle_deleter<
                  socket_fd_t, // underlying type that stores the handle
                  static_cast<socket_fd_t>(-1), // null handle value
                  false,       // a negative value represents an invalid handle
                  socket_fd_t, // the handle type used by winapi
                  detail::close_socket // the cleaner function
                  >> {
    private:
        using base =
            std::unique_ptr<void,
                            os::zero_neg_handle_deleter<
                                socket_fd_t, static_cast<socket_fd_t>(-1),
                                false, socket_fd_t, detail::close_socket>>;

    public:
        using base::base;

        socket_handle() = default;

        socket_handle(socket_handle&&) = default;

        socket_handle& operator=(socket_handle&& other) noexcept {
            base::operator=(std::move(other));
            return *this;
        }

        socket_handle& operator=(base&& other) noexcept {
            base::operator=(std::move(other));
            return *this;
        }
    };
#else
    /*!
     * @brief RAII wrapper for socket handle.
     */
    using socket_handle = os::handle;
#endif // _WIN32

    /*!
     * @brief Determine if a socket option is supported by
     * a specific socket protocol.
     * @tparam Option The socket option.
     * @tparam Protocol The socket protocol.
     */
    template <class Option, class Protocol>
    inline constexpr bool is_supported_option = true;

    /*!
     * @brief StreamSocketType determines if a socket is a stream socket or not.
     *
     * A stream socket has an inner type `protocol_type`, and this protocol type
     * has a static constant bool variable `is_stream_protocol` that must be
     * true.
     */
    template <class Stream>
    concept StreamSocketType = requires() {
        typename Stream::protocol_type;
        requires Stream::protocol_type::is_stream_protocol;
    };

    /*!
     * @brief EndpointSequence determines if a type is a range of endpoints of
     * type `EndpointType`.
     *
     * The following expressions must be valid:
     *
     * `epoint = std::begin(epoints)`
     *
     * `epoint = std::end(epoints)`
     */
    template <class EndpointRange, class EndpointType>
    concept EndpointSequence =
        requires(EndpointType epoint, const EndpointRange& epoints) {
            epoint = *std::begin(epoints);
            epoint = *std::end(epoints);
        };

    /*!
     * @brief `BasicHandler` requires a handler to be destructible and be either
     * copy or move constructible.
     */
    template <class Handler>
    concept BasicHandler = (std::is_copy_constructible_v<Handler> ||
                            std::is_move_constructible_v<Handler>) &&
                           std::is_destructible_v<Handler>;

    /*!
     * @brief `ConnectHandler` requires a handler to meet `BasicHandler`
     * requirements and to be callable with an argument of type `const
     * std::error_code&`.
     */
    template <class Handler>
    concept ConnectHandler =
        BasicHandler<Handler> &&
        requires(Handler handler, const std::error_code& ec) { handler(ec); };

    /*!
     * @brief `ConnectRangeHandler` requires a handler to meet `BasicHandler`
     * requirements and to be callable with two arguments: `const
     * std::error_code&, const EndpointType& epoint`.
     */
    template <class Handler, class EndpointType>
    concept ConnectRangeHandler =
        BasicHandler<Handler> &&
        requires(Handler handler, const EndpointType& epoint) {
            handler(std::error_code{}, epoint);
        };

    /*!
     * @brief `AcceptEndpointHandler` requires a handler to meet `BasicHandler`
     * requirements and to be callable with two arguments: `const
     * std::error_code&, const EndpointType& epoint`.
     */
    template <class Handler, class EndpointType>
    concept AcceptEndpointHandler =
        BasicHandler<Handler> &&
        requires(Handler handler, const EndpointType& peer) {
            handler(std::error_code{}, peer);
        };

    /*!
     * @brief `AcceptHandler` requires a handler to meet `BasicHandler`
     * requirements and to be callable with an argument of type `const
     * std::error_code&`.
     */
    template <class Handler>
    concept AcceptHandler = BasicHandler<Handler> && requires(Handler handler) {
        handler(std::error_code{});
    };

} // namespace RAD_LIB_NAMESPACE::net
