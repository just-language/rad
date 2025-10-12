#pragma once
#include <rad/net/async_resolver.h>
#include <rad/net/async_socket.h>
#include <rad/net/types.h>

namespace RAD_LIB_NAMESPACE::net {
    /*!
     * @brief Encapsulates the flags needed for UDP.
     *
     * The `net::udp` class contains flags necessary for UDP sockets.
     */
    class udp {
    public:
        /*!
         * @brief The type of a UDP endpoint.
         */
        using endpoint_type = endpoint;

        /*!
         * @brief The UDP async resolver type.
         */
        using resolver = async_resolver<udp>;

        /*!
         * @brief The UDP async socket type.
         */
        using socket = async_datagram_socket<udp>;

        /*!
         * @brief This protocol is a message oriented protocol.
         * It is not a stream protocol.
         */
        static constexpr bool is_stream_protocol = false;

        /*!
         * @brief Construct with `address_family::unspecified`.
         * This may be used with the resolvers to get both IPv4
         * and IPv6 results.
         */
        constexpr udp() = default;

        /*!
         * @brief Construct with address family.
         *
         * Valid families are `address_family::ipv4`, `address_family::ipv6`
         * and `address_family::unspecified`.
         * @param family The address family.
         */
        constexpr udp(address_family family) noexcept : family_{family} {
        }

        /*!
         * @brief Obtain an identifier for the socket type of the protocol.
         * @return An identifier for the socket type of the protocol.
         * The return value will always be `socket_type::udp_dgram`.
         */
        static constexpr socket_type type() noexcept {
            return socket_type::udp_dgram;
        }

        /*!
         * @brief Obtain an identifier for the protocol.
         * @return An identifier for the protocol.
         */
        static constexpr protocol_type protocol() noexcept {
            return protocol_type::udp;
        }

        /*!
         * @brief Obtain an identifier for the protocol family.
         * @return An identifier for the protocol family.
         */
        constexpr address_family family() const noexcept {
            return family_;
        }

        /*!
         * @brief Construct to represent the IPv4 UDP protocol.
         * @return A UDP object that represents the IPv4 UDP protocol.
         */
        constexpr static udp ipv4() noexcept {
            return udp{address_family::ipv4};
        }

        /*!
         * @brief Construct to represent the IPv6 UDP protocol.
         * @return A UDP object that represents the IPv6 UDP protocol.
         */
        constexpr static udp ipv6() noexcept {
            return udp{address_family::ipv6};
        }

        /*!
         * @brief Construct with `address_family::unspecified`.
         * This may be used with the resolvers to get both IPv4
         * and IPv6 results.
         * @return A UDP object with `address_family::unspecified`.
         */
        constexpr static udp any() noexcept {
            return udp{};
        }

        /*!
         * @brief Compare two protocols for equality.
         * @param lhs The first protocol.
         * @param rhs The second protocol.
         * @return True if the two protocol are equal, otherwise false.
         */
        friend constexpr bool operator==(const udp& lhs,
                                         const udp& rhs) noexcept {
            return lhs.family_ == rhs.family_;
        }

    private:
        address_family family_ = address_family::unspecified;
    };

    static_assert(!StreamSocketType<udp::socket>,
                  "udp::socket is not a message socket");

}; // namespace RAD_LIB_NAMESPACE::net