#pragma once
#include <rad/net/async_resolver.h>
#include <rad/net/async_socket.h>
#include <rad/net/types.h>

namespace RAD_LIB_NAMESPACE {
    template <class T>
    class task;
}

namespace RAD_LIB_NAMESPACE::net {
    /*!
     * @brief Encapsulates the flags needed for TCP.
     *
     * The `net::tcp` class contains flags necessary for TCP sockets.
     */
    class tcp {
    public:
        /*!
         * @brief The type of a TCP endpoint.
         */
        using endpoint_type = endpoint;

        /*!
         * @brief The TCP async resolver type.
         */
        using resolver = async_resolver<tcp>;

        /*!
         * @brief The TCP async socket type.
         */
        using socket = async_stream_socket<tcp>;

        /*!
         * @brief The TCP async acceptor type.
         */
        using acceptor = async_acceptor<tcp>;

        /*!
         * @brief This protocol is a stream oriented protocol.
         */
        static constexpr bool is_stream_protocol = true;

        /*!
         * @brief Construct with `address_family::unspecified`.
         * This may be used with the resolvers to get both IPv4
         * and IPv6 results.
         */
        constexpr tcp() = default;

        /*!
         * @brief Construct with address family.
         *
         * Valid families are `address_family::ipv4`, `address_family::ipv6`
         * and `address_family::unspecified`.
         * @param family The address family.
         */
        constexpr explicit tcp(address_family family) noexcept
            : family_{family} {
        }

        /*!
         * @brief Obtain an identifier for the socket type of the protocol.
         * @return An identifier for the socket type of the protocol.
         * The return value will always be `socket_type::tcp_stream`.
         */
        static constexpr socket_type type() noexcept {
            return socket_type::tcp_stream;
        }

        /*!
         * @brief Obtain an identifier for the protocol.
         * @return An identifier for the protocol.
         */
        static constexpr protocol_type protocol() noexcept {
            return protocol_type::tcp;
        }

        /*!
         * @brief Obtain an identifier for the protocol family.
         * @return An identifier for the protocol family.
         */
        constexpr address_family family() const noexcept {
            return family_;
        }

        /*!
         * @brief Construct to represent the IPv4 TCP protocol.
         * @return A TCP object that represents the IPv4 TCP protocol.
         */
        constexpr static tcp ipv4() noexcept {
            return tcp{address_family::ipv4};
        }

        /*!
         * @brief Construct to represent the IPv6 TCP protocol.
         * @return A TCP object that represents the IPv6 TCP protocol.
         */
        constexpr static tcp ipv6() noexcept {
            return tcp{address_family::ipv6};
        }

        /*!
         * @brief Construct with `address_family::unspecified`.
         * This may be used with the resolvers to get both IPv4
         * and IPv6 results.
         * @return A TCP object with `address_family::unspecified`.
         */
        constexpr static tcp any() noexcept {
            return tcp{};
        }

        /*!
         * @brief Create an acceptor socket, bind it to a local endpoint,
         * start listening for connections and return the acceptor.
         * @param ex The I/O executor that the stream socket
         * will use to dispatch handlers for any asynchronous
         * operations performed on the socket.
         * @param local_epoint The local address to bind the
         * acceptor socket to.
         * @param max_backlog The number of connections to reserve
         * in the listen queue. Pass 0 to use the default
         * backlog (SOMAXCONN).
         * @return The acceptor socket ready for accepting incoming connections.
         */
        static acceptor listen(io_executor& ex,
                               const endpoint_type& local_epoint,
                               int max_backlog = 0) {
            acceptor a{ex, local_epoint};
            a.listen(max_backlog);
            return a;
        }

        // The task<T> is not a complete type here.
        // don't force users to include the task header.
        // define the functions body in tcp_factories.h

        /*!
         * @brief Perform async connect to an endpoint and return
         * the connected stream socket.
         *
         * The async operation will not start until the returned awaitable
         * is awaited.
         * @param ex The I/O executor that the stream socket
         * will use to dispatch handlers for any asynchronous
         * operations performed on the socket.
         * @param epoint The remote endpoint to connect
         * the socket to
         * @return An awaitable that is when awaited will start the async
         * operation.
         * The result of the awaitable is the stream connected socket.
         */
        static task<socket> connect(io_executor& ex,
                                    const endpoint_type& epoint);

        /*!
         * @brief Perform async connect to remote endpoints and return
         * the connected stream socket.
         *
         * The endpoints will be tryed one by one in order until a
         * connection is established.
         *
         * The async operation will not start until the returned awaitable
         * is awaited.
         * @tparam EndpointRange The endpoints range type.
         * @param ex The I/O executor that the stream socket
         * will use to dispatch handlers for any asynchronous
         * operations performed on the socket.
         * @param endpoints The remote endpoints to connect
         * the socket to.
         * @return An awaitable that is when awaited will start the async
         * operation.
         * The result of the awaitable is the stream connected socket.
         */
        template <EndpointSequence<endpoint> EndpointRange>
        static task<socket> connect(io_executor& ex,
                                    const EndpointRange& endpoints);

        /*!
         * @brief Compare two protocols for equality.
         * @param lhs The first protocol.
         * @param rhs The second protocol.
         * @return True if the two protocol are equal, otherwise false.
         */
        friend constexpr bool operator==(const tcp& lhs,
                                         const tcp& rhs) noexcept {
            return lhs.family_ == rhs.family_;
        }

    private:
        address_family family_ = address_family::unspecified;
    };

    static_assert(StreamSocketType<tcp::socket>,
                  "tcp::socket is not a stream socket");
}; // namespace RAD_LIB_NAMESPACE::net