#pragma once
#include <rad/local/endpoint.h>
#include <rad/net/async_socket.h>
#include <rad/net/types.h>

namespace RAD_LIB_NAMESPACE::local {
    /*!
     * @brief The local::tcp struct contains flags necessary for
     * stream-oriented UNIX domain sockets.
     */
    struct tcp {
        constexpr tcp() = default;

        constexpr tcp(net::address_family) noexcept {
        }

        /// Obtain an identifier for the type of the protocol.
        static constexpr net::socket_type type() noexcept {
            return net::socket_type::tcp_stream;
        }

        /// Obtain an identifier for the protocol.
        static constexpr net::protocol_type protocol() noexcept {
            return net::protocol_type::auto_protocol;
        }

        /// Obtain an identifier for the protocol family.
        static constexpr net::address_family family() noexcept {
            return net::address_family::local;
        }

        /// This is a stream protocol.
        static constexpr bool is_stream_protocol = true;

        /// The type of a UNIX domain endpoint.
        using endpoint_type = endpoint;

        /// The UNIX domain socket type.
        using socket = net::async_stream_socket<tcp>;

        /// The UNIX domain acceptor type.
        using acceptor = net::async_acceptor<tcp>;
    };
} // namespace RAD_LIB_NAMESPACE::local