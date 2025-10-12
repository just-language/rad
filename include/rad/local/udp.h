#pragma once
#include <rad/local/endpoint.h>
#include <rad/net/async_socket.h>
#include <rad/net/types.h>

namespace RAD_LIB_NAMESPACE::local {
    /*!
     * @brief The local::udp struct contains flags necessary for
     * datagram-oriented UNIX domain sockets.
     */
    struct udp {
        constexpr udp() = default;

        constexpr udp(net::address_family) noexcept {
        }

        /// Obtain an identifier for the type of the protocol.
        static constexpr net::socket_type type() noexcept {
            return net::socket_type::udp_dgram;
        }

        /// Obtain an identifier for the protocol.
        static constexpr net::protocol_type protocol() noexcept {
            return net::protocol_type::udp;
        }

        /// Obtain an identifier for the protocol family.
        static constexpr net::address_family family() noexcept {
            return net::address_family::local;
        }

        /// This is not a stream protocol.
        static constexpr bool is_stream_protocol = false;

        /// The type of a UNIX domain endpoint.
        using endpoint_type = endpoint;

        /// The UNIX domain socket type.
        using socket = net::async_datagram_socket<udp>;
    };
} // namespace RAD_LIB_NAMESPACE::local