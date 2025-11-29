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
                                    const void* optdata, socket_len_t optlen,
                                    std::error_code& ec) noexcept;

        RAD_EXPORT_DECL void getopt(socket_fd_t s, socket_option_level level,
                                    socket_option_name optname, void* optdata,
                                    socket_len_t* optlen,
                                    std::error_code& ec) noexcept;

        RAD_EXPORT_DECL void bind(socket_fd_t s, const void* addr,
                                  socket_len_t addr_len,
                                  std::error_code& ec) noexcept;

        RAD_EXPORT_DECL void bind_if_not(socket_fd_t s, address_family af,
                                         std::error_code& ec) noexcept;

        RAD_EXPORT_DECL int max_listen_backlog() noexcept;

        RAD_EXPORT_DECL void listen(socket_fd_t s, uint32_t backlog,
                                    std::error_code& ec) noexcept;

        RAD_EXPORT_DECL socket_handle accept(socket_fd_t s, void* addr,
                                             socket_len_t& addr_size,
                                             std::error_code& ec) noexcept;

        RAD_EXPORT_DECL std::size_t
        send(socket_fd_t s, const const_buffer* buffers, std::size_t count,
             transfer_flags flags, std::error_code& ec) noexcept;

        RAD_EXPORT_DECL std::size_t
        recv(socket_fd_t s, const mutable_buffer* buffers, std::size_t count,
             bool not_zero, transfer_flags flags, std::error_code& ec) noexcept;

        RAD_EXPORT_DECL std::size_t
        sendto(socket_fd_t s, const const_buffer* buffers, std::size_t count,
               transfer_flags flags, const void* addr, socket_len_t addr_size,
               std::error_code& ec) noexcept;

        RAD_EXPORT_DECL std::size_t
        recvfrom(socket_fd_t s, const mutable_buffer* buffers,
                 std::size_t count, transfer_flags flags, void* addr,
                 socket_len_t* addr_size, bool not_zero,
                 std::error_code& ec) noexcept;

        RAD_EXPORT_DECL void local_endpoint(socket_fd_t s, void* addr,
                                            socket_len_t& addr_len,
                                            std::error_code& ec) noexcept;

        RAD_EXPORT_DECL void remote_endpoint(socket_fd_t s, void* addr,
                                             socket_len_t& addr_len,
                                             std::error_code& ec) noexcept;

        RAD_EXPORT_DECL void connect(socket_fd_t s, const void* addr,
                                     socket_len_t addr_len,
                                     std::error_code& ec) noexcept;
    }; // namespace socket_fns
} // namespace RAD_LIB_NAMESPACE::net::detail
