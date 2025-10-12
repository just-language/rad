#pragma once
#include <rad/net/socket_options.h>

// address_family

#ifdef _WIN32
static_assert(sizeof(ADDRESS_FAMILY) == sizeof(address_family));
#else
static_assert(sizeof(sa_family_t) == sizeof(address_family));
#endif // _WIN32
static_assert(sizeof(sockaddr) == sizeof(ipv4_endpoint));
static_assert(sizeof(sockaddr_in) == sizeof(ipv4_endpoint));
static_assert(sizeof(sockaddr_in6) == sizeof(ipv6_endpoint));
static_assert(sizeof(sockaddr_in6) == sizeof(endpoint));

static_assert(AF_UNSPEC == ienum(address_family::unspecified));
static_assert(AF_INET == ienum(address_family::ipv4));
static_assert(AF_INET6 == ienum(address_family::ipv6));
static_assert(AF_IPX == ienum(address_family::ipx));
static_assert(AF_APPLETALK == ienum(address_family::apple_talk));
#ifdef AF_LOCAL
static_assert(AF_LOCAL == ienum(address_family::local));
#endif
#ifdef AF_NETBIOS
static_assert(AF_NETBIOS == ienum(address_family::net_bios));
#endif
#ifdef AF_IRDA
static_assert(AF_IRDA == ienum(address_family::infrared));
#endif
#ifdef AF_BTH
static_assert(AF_BTH == ienum(address_family::bluetooth));
#endif

// socket_type

static_assert(SOCK_STREAM == ienum(socket_type::tcp_stream));
static_assert(SOCK_DGRAM == ienum(socket_type::udp_dgram));
static_assert(SOCK_RAW == ienum(socket_type::raw));
static_assert(SOCK_RDM == ienum(socket_type::rdm));
static_assert(SOCK_SEQPACKET == ienum(socket_type::seq_packet));

// protocol_type

static_assert(IPPROTO_ICMP == ienum(protocol_type::icmp));
static_assert(IPPROTO_IGMP == ienum(protocol_type::igmp));
static_assert(IPPROTO_TCP == ienum(protocol_type::tcp));
static_assert(IPPROTO_UDP == ienum(protocol_type::udp));
static_assert(IPPROTO_ICMPV6 == ienum(protocol_type::icmpv6));
#ifdef IPPROTO_PGM
static_assert(IPPROTO_PGM == ienum(protocol_type::pgm));
#endif // IPPROTO_PGM

// socket_shutdown

#ifdef _WIN32
static_assert(SD_RECEIVE == ienum(socket_shutdown::receive));
static_assert(SD_SEND == ienum(socket_shutdown::send));
static_assert(SD_BOTH == ienum(socket_shutdown::both));
#else
static_assert(SHUT_RD == ienum(socket_shutdown::receive));
static_assert(SHUT_WR == ienum(socket_shutdown::send));
static_assert(SHUT_RDWR == ienum(socket_shutdown::both));
#endif // _WIN32

// transfer_flags

static_assert(0 == ienum(transfer_flags::none));
static_assert(MSG_DONTROUTE == ienum(transfer_flags::dont_route));
static_assert(MSG_OOB == ienum(transfer_flags::oob));
static_assert(MSG_PEEK == ienum(transfer_flags::peek));
static_assert(MSG_WAITALL == ienum(transfer_flags::wait_all));

#ifdef MSG_DONTWAIT
static_assert(MSG_DONTWAIT == ienum(transfer_flags::dont_wait));
#endif // MSG_DONTWAIT
#ifdef MSG_NOSIGNAL
static_assert(MSG_NOSIGNAL == ienum(transfer_flags::no_signal));
#endif // MSG_NOSIGNAL

// socket_option_level
static_assert(SOL_SOCKET == ienum(socket_option_level::socket));
static_assert(IPPROTO_TCP == ienum(socket_option_level::proto_tcp));
static_assert(IPPROTO_IP == ienum(socket_option_level::proto_ipv4));
static_assert(IPPROTO_IPV6 == ienum(socket_option_level::proto_ipv6));
static_assert(IPPROTO_UDP == ienum(socket_option_level::proto_udp));

// socket_option_name

static_assert(SO_ERROR == ienum(socket_option_name::error));
static_assert(SO_REUSEADDR == ienum(socket_option_name::reuse_addr));
static_assert(SO_RCVBUF == ienum(socket_option_name::recv_buff));
static_assert(SO_SNDBUF == ienum(socket_option_name::send_buff));
static_assert(SO_RCVTIMEO == ienum(socket_option_name::recv_timeout));
static_assert(SO_SNDTIMEO == ienum(socket_option_name::send_timeout));
static_assert(SO_KEEPALIVE == ienum(socket_option_name::keep_alive));
static_assert(SO_DONTROUTE == ienum(socket_option_name::dont_route));
static_assert(SO_BROADCAST == ienum(socket_option_name::broadcast));
static_assert(SO_LINGER == ienum(socket_option_name::linger));
static_assert(TCP_NODELAY == ienum(socket_option_name::tcp_no_delay));

#ifdef SO_EXCLUSIVEADDRUSE
static_assert(SO_EXCLUSIVEADDRUSE == ienum(socket_option_name::exclusive_addr));
#endif
#ifdef SO_UPDATE_ACCEPT_CONTEXT
static_assert(SO_UPDATE_ACCEPT_CONTEXT ==
              ienum(socket_option_name::update_accept_context));
#endif
#ifdef SO_UPDATE_CONNECT_CONTEXT
static_assert(SO_UPDATE_CONNECT_CONTEXT ==
              ienum(socket_option_name::update_connect_context));
#endif

#ifdef TCP_KEEPIDLE
static_assert(TCP_KEEPIDLE == ienum(socket_option_name::tcp_keep_idle));
#endif // TCP_KEEPIDLE
#ifdef TCP_MAXRT
static_assert(TCP_MAXRT == ienum(socket_option_name::tcp_max_rt));
#endif // TCP_MAXRT
#ifdef TCP_KEEPCNT
static_assert(TCP_KEEPCNT == ienum(socket_option_name::tcp_keep_count));
#endif // TCP_KEEPCNT
#ifdef TCP_KEEPINTVL
static_assert(TCP_KEEPINTVL ==
              ienum(socket_option_name::tcp_keep_alive_interval));
#endif // TCP_KEEPINTVL

static_assert(IP_MULTICAST_IF ==
              ienum(socket_option_name::multi_cast_outbound_interface));
static_assert(IP_MULTICAST_TTL == ienum(socket_option_name::multi_cast_hops));
static_assert(IP_MULTICAST_LOOP ==
              ienum(socket_option_name::multi_cast_enable_loop_back));
static_assert(IP_ADD_MEMBERSHIP ==
              ienum(socket_option_name::multi_cast_join_group));
static_assert(IP_DROP_MEMBERSHIP ==
              ienum(socket_option_name::multi_cast_leave_group));

static_assert(sizeof(ip_mreq) == sizeof(socket_options::multi_cast_group_info));

// ipv4
static_assert(sizeof(ipv4) == sizeof(in_addr));
static_assert(std::is_trivially_copyable_v<ipv4> &&
              std::is_trivially_destructible_v<ipv4>);

// ipv6
static_assert(sizeof(ipv6) == sizeof(in6_addr));
static_assert(std::is_trivially_copyable_v<ipv6> &&
              std::is_trivially_destructible_v<ipv6>);

// socket_port
static_assert(sizeof(socket_port) == sizeof(uint16_t));

// ipv4_endpoint
static_assert(sizeof(ipv4_endpoint) == sizeof(sockaddr_in));
static_assert(std::is_trivially_copyable_v<ipv4_endpoint> &&
              std::is_trivially_destructible_v<ipv4_endpoint>);

// ipv6_endpoint
static_assert(sizeof(ipv6_endpoint) == sizeof(sockaddr_in6));
static_assert(std::is_trivially_copyable_v<ipv6_endpoint> &&
              std::is_trivially_destructible_v<ipv6_endpoint>);

// endpoint
static_assert(sizeof(endpoint) == sizeof(sockaddr_in6));
static_assert(std::is_trivially_copyable_v<endpoint> &&
              std::is_trivially_destructible_v<endpoint>);