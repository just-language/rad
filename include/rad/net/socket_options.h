#pragma once
#include <rad/libbase.h>
#include <rad/net/types.h>
#include <chrono>

namespace RAD_LIB_NAMESPACE::net::detail {
    template <class ValType>
    class value_storage {
    public:
        constexpr value_storage() : val{} {
        }

        constexpr value_storage(const ValType& val) : val{val} {
        }

        constexpr ValType& value() {
            return val;
        }

        constexpr const ValType& value() const {
            return val;
        }

        constexpr void value(const ValType& v) {
            val = v;
        }

        constexpr void* data() noexcept {
            return &val;
        }

        constexpr const void* data() const noexcept {
            return &val;
        }

        constexpr socklen_t size() const noexcept {
            return sizeof(val);
        }

    protected:
        ValType val;
    };

    template <>
    class value_storage<std::nullptr_t> {
    public:
        constexpr value_storage() = default;

        constexpr value_storage(std::nullptr_t) {
        }

        constexpr void* data() const noexcept {
            return nullptr;
        }

        constexpr socklen_t size() const noexcept {
            return 0;
        }
    };

    template <class ValType, socket_option_level Level, socket_option_name Name>
    class socket_option_base : public value_storage<ValType> {
        using base = value_storage<ValType>;

    public:
        constexpr socket_option_base() = default;

        constexpr socket_option_base(const ValType& v) : base(v) {
        }

        constexpr socket_option_level level() const noexcept {
            return Level;
        }

        constexpr socket_option_name name() const noexcept {
            return Name;
        }
    };

} // namespace RAD_LIB_NAMESPACE::net::detail

namespace RAD_LIB_NAMESPACE::net::socket_options {
    /*!
     * @brief The linger structure maintains information about a specific socket
     * that specifies how that socket should behave when data is queued to be
     * sent and the the socket is closed.
     */
    struct linger_t {
        /*!
         * @brief Specifies whether a socket should remain open for a specified
         * amount of time after a closesocket function call to enable queued
         * data to be sent.
         *
         * This member can be 0 to turn off linger in background.
         */
        uint16_t onoff = 0;
        /*!
         * @brief The linger time in seconds. This member specifies how long to
         * remain open after the socket is closed to enable queued data to be
         * sent. This member is only applicable if the l_onoff member of the
         * linger structure is set to a nonzero value.
         */
        std::chrono::duration<uint16_t> linger = {};
    };

    static_assert(sizeof(linger_t) == sizeof(int));

    /*!
     * @brief SO_REUSEADDR Allows the socket to be bound to an address that is
     * already in use.
     */
    using reuse_address =
        detail::socket_option_base<int, socket_option_level::socket,
                                   socket_option_name::reuse_addr>;

    /*!
     * @brief SO_RCVBUF Specifies the total per-socket buffer space reserved for
     * receives.
     */
    using receive_buffer =
        detail::socket_option_base<int, socket_option_level::socket,
                                   socket_option_name::recv_buff>;

    /*!
     * @brief SO_SNDBUF Specifies the total per-socket buffer space reserved for
     * sends.
     */
    using send_buffer =
        detail::socket_option_base<int, socket_option_level::socket,
                                   socket_option_name::send_buff>;

    /*!
     * @brief SO_KEEPALIVE Enables sending keep-alive packets for a socket
     * connection.
     */
    using keep_alive =
        detail::socket_option_base<int, socket_option_level::socket,
                                   socket_option_name::keep_alive>;

    /*!
     * @brief SO_DONTROUTE Sets whether outgoing data should be sent on
     * interface the socket is bound to and not a routed on some other
     * interface.
     */
    using dont_route =
        detail::socket_option_base<int, socket_option_level::socket,
                                   socket_option_name::dont_route>;

    /*!
     * @brief SO_BROADCAST Configures a socket for sending broadcast data.
     */
    using broadcast =
        detail::socket_option_base<int, socket_option_level::socket,
                                   socket_option_name::broadcast>;
    /*!
     * @brief Lingers on close if unsent data is present.
     */
    using linger =
        detail::socket_option_base<linger_t, socket_option_level::socket,
                                   socket_option_name::linger>;

#ifdef _WIN32
    /*!
     * @brief SO_EXCLUSIVEADDRUSE Enables a socket to be bound for exclusive
     * access.
     */
    using exclusive_address =
        net::detail::socket_option_base<int, socket_option_level::socket,
                                        socket_option_name::exclusive_addr>;

    /*!
     * @brief SO_UPDATE_ACCEPT_CONTEXT used with AcceptEx.
     */
    using update_accept_context = net::detail::socket_option_base<
        socket_fd_t, socket_option_level::socket,
        socket_option_name::update_accept_context>;
    /*!
     * @brief SO_UPDATE_CONNECT_CONTEXT used with ConnectEx.
     */
    using update_connect_context = net::detail::socket_option_base<
        std::nullptr_t, socket_option_level::socket,
        socket_option_name::update_connect_context>;
    /*!
     * @brief SO_RCVTIMEO Sets the timeout, in milliseconds, for blocking
     * receive calls.
     */
    using receive_timeout =
        net::detail::socket_option_base<DWORD, socket_option_level::socket,
                                        socket_option_name::recv_timeout>;
    /*!
     * @brief SO_SNDTIMEO The timeout, in milliseconds, for blocking send
     * calls.
     */
    using send_timeout =
        net::detail::socket_option_base<DWORD, socket_option_level::socket,
                                        socket_option_name::send_timeout>;
#endif

    // IPPROTO_TCP Socket options

    /*!
     * @brief TCP_NODELAY Enables or disables the Nagle algorithm for TCP
     * sockets. This option is disabled (set to FALSE) by default.
     */
    using tcp_nodelay =
        net::detail::socket_option_base<int, socket_option_level::proto_tcp,
                                        socket_option_name::tcp_no_delay>;

    // IPPROTO_IP Socket options

    /*!
     * @brief IP_MULTICAST_IF Gets or sets the outgoing interface for
     * sending IPv4 multicast traffic.
     */
    using multi_cast_outbound_interface = net::detail::socket_option_base<
        ipv4, socket_option_level::proto_ipv4,
        socket_option_name::multi_cast_outbound_interface>;

    /*!
     * @brief IP_MULTICAST_TTL Sets/gets the TTL value associated with IP
     * multicast traffic on the socket.
     */
    using multi_cast_hops =
        net::detail::socket_option_base<int, socket_option_level::proto_ipv4,
                                        socket_option_name::multi_cast_hops>;

    /*!
     * @brief IP_MULTICAST_LOOP For a socket that is joined to one or more
     * multicast groups, this controls whether it will receive a copy of
     * outgoing packets sent to those multicast groups via the selected
     * multicast interface.
     */
    using multi_cast_enable_loop_back = net::detail::socket_option_base<
        int, socket_option_level::proto_ipv4,
        socket_option_name::multi_cast_enable_loop_back>;

    /*!
     * @brief This structure provides multicast group information for IPv4
     * addresses.
     */
    struct multi_cast_group_info {
        /// The address of the IPv4 multicast group.
        ipv4 multicast_group;
        /*!
         * @brief The local IPv4 address of the interface or the interface index
         * on which the multicast group should be joined or dropped. This value
         * is in network byte order. If this member specifies an IPv4 address of
         * 0.0.0.0, the default IPv4 multicast interface is used. To use an
         * interface index of 1 would be the same as an IP address of 0.0.0.1.
         */
        ipv4 local_interface;
    };

    /*!
     * @brief IP_ADD_MEMBERSHIP Join the socket to the supplied multicast
     * group on the specified interface.
     */
    using multi_cast_join_group = net::detail::socket_option_base<
        multi_cast_group_info, socket_option_level::proto_ipv4,
        socket_option_name::multi_cast_join_group>;

    /*!
     * @brief IP_DROP_MEMBERSHIP Leaves the specified multicast group from
     * the specified interface.
     */
    using multi_cast_leave_group = net::detail::socket_option_base<
        multi_cast_group_info, socket_option_level::proto_ipv4,
        socket_option_name::multi_cast_leave_group>;
} // namespace RAD_LIB_NAMESPACE::net::socket_options
