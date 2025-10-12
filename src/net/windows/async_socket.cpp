#include <winsock2.h>
#include <ws2ipdef.h>
#include <ws2tcpip.h>
#include <In6addr.h>
#include <MSWSock.h>
#include <Windows.h>
#include <rad/async/work_guard.h>
#include <rad/detail/dprint.h>
#include <rad/net/async_socket.h>
#include <rad/net/blocking_socket.h>
#include <rad/net/socket_options.h>

#include <cassert>

using namespace RAD_LIB_NAMESPACE;
using namespace io;
using namespace net;
using namespace net::detail;

#include "../compiletime_checks.h"

namespace {

    inline std::error_code last_net_error() noexcept {
        return std::error_code{::WSAGetLastError(), system_category()};
    }

    inline async_result make_pending(work_guard<any_executor>& w) noexcept {
        w.release();
        return async_result::pending();
    }

    struct init_winsock_t {
        init_winsock_t();

        ~init_winsock_t() {
            if (::WSACleanup() == SOCKET_ERROR) {
                dprint("WSAEnumProtocolsW failed ! "
                       "WSAGetLastError: %d\n",
                       ::WSAGetLastError());
            }
        }

        struct fn_ptrs_t {
            LPFN_ACCEPTEX acceptex = nullptr;
            LPFN_CONNECTEX connectex = nullptr;
            LPFN_DISCONNECTEX disconnectex = nullptr;
            LPFN_GETACCEPTEXSOCKADDRS parseaddr = nullptr;

            std::array<std::pair<GUID, void**>, 4> get_guids_ptrs() noexcept {
                return std::array{
                    std::pair<GUID, void**>{
                        WSAID_ACCEPTEX, reinterpret_cast<void**>(&acceptex)},
                    std::pair<GUID, void**>{
                        WSAID_CONNECTEX, reinterpret_cast<void**>(&connectex)},
                    std::pair<GUID, void**>{
                        WSAID_DISCONNECTEX,
                        reinterpret_cast<void**>(&disconnectex)},
                    std::pair<GUID, void**>{
                        WSAID_GETACCEPTEXSOCKADDRS,
                        reinterpret_cast<void**>(&parseaddr)},
                };
            }
        };

        fn_ptrs_t fn_ptrs;
    };

    init_winsock_t::init_winsock_t() {
        // dprint("init_winsock_t::init_winsock_t()\n");
        {
            WSADATA wsa_data;
            const int wsa_error = ::WSAStartup(MAKEWORD(2, 2), &wsa_data);
            if (wsa_error) {
                dprint("init_winsock_t::init_winsock_t() "
                       "WSAStartup "
                       "failed ! %d\n",
                       wsa_error);
                std::terminate();
            }
        }

        socket_handle temp_sock{::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)};
        if (!temp_sock) {
            dprint("init_winsock_t::init_winsock_t() socket() failed "
                   "! "
                   "WSAGetLastError "
                   ": %d\n",
                   ::WSAGetLastError());
            std::terminate();
        }

        for (auto [guid, fn_ptr] : fn_ptrs.get_guids_ptrs()) {
            DWORD bytes_returned = 0;
            if (::WSAIoctl(temp_sock.get(), SIO_GET_EXTENSION_FUNCTION_POINTER,
                           &guid, sizeof(GUID), fn_ptr, sizeof(void*),
                           &bytes_returned, nullptr, nullptr) != 0) {
                dprint("init_winsock_t::init_winsock_t() WSAIoctl "
                       "failed "
                       "WSAGetLastError: %d\n",
                       WSAGetLastError());
                std::terminate();
            }
        }
    }

    const init_winsock_t winsock_inst;

    inline bool supports_skip_iocp_on_sync_completion(socket_fd_t s) noexcept {
        WSAPROTOCOL_INFOW proto_info;
        int proto_len = sizeof(proto_info);
        int res =
            ::getsockopt(s, SOL_SOCKET, SO_PROTOCOL_INFOW,
                         reinterpret_cast<char*>(&proto_info), &proto_len);
        if (res != 0) {
            dprint("getsockopt(SOL_SOCKET, SO_PROTOCOL_INFOW) failed "
                   "! "
                   "WSAGetLastError(): %d\n",
                   ::WSAGetLastError());
            return false;
        }
        bool is_ifs_handle =
            (proto_info.dwServiceFlags1 & XP1_IFS_HANDLES) == XP1_IFS_HANDLES;
        if (!is_ifs_handle) {
            dprint("XP1_IFS_HANDLES is not set, the socket is not IFS "
                   "handle !\n");
        }
        return is_ifs_handle;
    }

    struct open_socket_result {
        socket_handle handle;
        bool skip_iocp_on_sync_completion = false;
    };

    open_socket_result open_socket(address_family af, socket_type type,
                                   protocol_type proto,
                                   std::error_code& ec) noexcept {
        ec.clear();
        const DWORD flags =
            static_cast<DWORD>(socket_creation_flags::overlapped |
                               socket_creation_flags::no_inherit);
        auto sock = socket_handle{::WSASocketW(
            as<int>(af), as<int>(type), as<int>(proto), nullptr, 0, flags)};
        if (!sock) {
            ec = last_net_error();
            return {};
        }

        bool skip_executor = supports_skip_iocp_on_sync_completion(sock.get());
        if (skip_executor) {
            BOOL res = ::SetFileCompletionNotificationModes(
                reinterpret_cast<HANDLE>(static_cast<socket_fd_t>(sock.get())),
                FILE_SKIP_COMPLETION_PORT_ON_SUCCESS);
            if (!res) {
                dprint("SetFileCompletionNotificationModes(socket,"
                       " "
                       "FILE_SKIP_COMPLETION_PORT_ON_SUCCESS) "
                       "failed ! "
                       "GetLastError(): %d\n",
                       (int)GetLastError());
                skip_executor = false;
            }
        }

        return open_socket_result{std::move(sock), skip_executor};
    }

    constexpr std::size_t max_stack_buffers = 100;

    using wsabuf_stack_storage = std::array<WSABUF, max_stack_buffers>;
    using wsabuf_heap_storage = std::vector<WSABUF>;

    inline WSABUF* get_wsabuffs(const const_buffer* buffs, std::size_t n,
                                wsabuf_stack_storage& stack_buffs,
                                wsabuf_heap_storage& heap_buffs) {
        if (!n) {
            stack_buffs[0].buf = nullptr;
            stack_buffs[0].len = 0;
            return stack_buffs.data();
        }

        if (n <= stack_buffs.size()) {
            for (auto i : range(n)) {
                stack_buffs[i].buf = buffs[i].data_as<char>();
                stack_buffs[i].len = static_cast<ULONG>(buffs[i].size());
            }
            return stack_buffs.data();
        }

        heap_buffs.reserve(n);
        for (auto i : range(n)) {
            WSABUF wsabuf;
            wsabuf.buf = buffs[i].data_as<char>();
            wsabuf.len = static_cast<ULONG>(buffs[i].size());
            heap_buffs.emplace_back(wsabuf);
        }

        return heap_buffs.data();
    }

    inline WSABUF* get_wsabuffs(const mutable_buffer* buffs, std::size_t n,
                                wsabuf_stack_storage& stack_buffs,
                                wsabuf_heap_storage& heap_buffs) {
        return get_wsabuffs(reinterpret_cast<const const_buffer*>(buffs), n,
                            stack_buffs, heap_buffs);
    }

    inline void get_async_result(socket_fd_t sock, io::detail::io_op& io_ctx,
                                 DWORD& transferred,
                                 std::error_code& ec) noexcept {
        transferred = 0;
        ec.clear();
        if (sock == INVALID_SOCKET) {
            ec = std::make_error_code(std::errc::operation_canceled);
            return;
        }
        DWORD recv_flags = 0;
        BOOL ret = ::WSAGetOverlappedResult(sock, io_ctx.get_ov_ptr(),
                                            &transferred, FALSE, &recv_flags);
        if (!ret) {
            const int error_code = ::WSAGetLastError();
            assert(error_code != WSA_IO_INCOMPLETE); // this function should not
                                                     // have been called before
                                                     // operation is completed
            if (error_code == WSA_OPERATION_ABORTED) {
                ec = std::make_error_code(std::errc::operation_canceled);
            }
            else if (error_code == WSAEMSGSIZE) {
                ec = std::make_error_code(std::errc::message_size);
            }
            else {
                ec = os::make_system_error(error_code);
            }
        }
    }
} // namespace

socket_handle socket_fns::open(address_family af, socket_type type,
                               protocol_type proto,
                               std::error_code& ec) noexcept {
    ec.clear();
    socket_fd_t sock =
        ::WSASocketW(static_cast<int>(af), static_cast<int>(type),
                     static_cast<int>(proto), nullptr, 0,
                     static_cast<DWORD>(socket_creation_flags::overlapped |
                                        socket_creation_flags::no_inherit));
    if (sock == SOCKET_ERROR) {
        ec = last_net_error();
    }
    return socket_handle{sock};
}

void socket_fns::shutdown(socket_fd_t s, socket_shutdown how,
                          std::error_code& ec) noexcept {
    ec.clear();
    if (::shutdown(s, static_cast<int>(how)) != 0) {
        ec = last_net_error();
    }
}

void socket_fns::setopt(socket_fd_t s, socket_option_level level,
                        socket_option_name optname, const void* optdata,
                        socklen_t optlen, std::error_code& ec) noexcept {
    ec.clear();
    if (::setsockopt(s, static_cast<int>(level), static_cast<int>(optname),
                     reinterpret_cast<const char*>(optdata), optlen) != 0) {
        ec = last_net_error();
    }
}

void socket_fns::getopt(socket_fd_t s, socket_option_level level,
                        socket_option_name optname, void* optdata,
                        socklen_t* optlen, std::error_code& ec) noexcept {
    ec.clear();
    if (getsockopt(s, static_cast<int>(level), static_cast<int>(optname),
                   reinterpret_cast<char*>(optdata), optlen) != 0) {
        ec = last_net_error();
    }
}

void socket_fns::bind(socket_fd_t s, const void* addr, socklen_t addr_len,
                      std::error_code& ec) noexcept {
    ec.clear();
    if (::bind(s, static_cast<const sockaddr*>(addr), addr_len) != 0) {
        ec = last_net_error();
    }
}

void socket_fns::bind_if_not(socket_fd_t s, address_family af,
                             std::error_code& ec) noexcept {
    ec.clear();
    // unix sockets can't bind to addr_any, only tcp and udp sockets do this
    uint8_t addrs[sizeof(endpoint)] = {0};
    *reinterpret_cast<address_family*>(addrs) = af;

    int result = ::bind(s, reinterpret_cast<const sockaddr*>(addrs),
                        af == address_family::ipv4 ? sizeof(ipv4_endpoint)
                                                   : sizeof(ipv6_endpoint));

    if (result != 0 && WSAGetLastError() != WSAEINVAL) {
        ec = last_net_error();
    }
}

int socket_fns::max_listen_backlog() noexcept {
    return SOMAXCONN;
}

void socket_fns::listen(socket_fd_t s, uint32_t backlog,
                        std::error_code& ec) noexcept {
    ec.clear();
    backlog = (backlog == 0 || backlog > SOMAXCONN) ? SOMAXCONN : backlog;
    if (::listen(s, backlog) != 0) {
        ec = last_net_error();
    }
}

socket_handle socket_fns::accept(socket_fd_t s, void* addr,
                                 socklen_t& addr_size,
                                 std::error_code& ec) noexcept {
    ec.clear();
    socket_fd_t new_socket =
        ::accept(s, static_cast<sockaddr*>(addr), &addr_size);
    if (new_socket != SOCKET_ERROR) {
        return socket_handle{new_socket};
    }
    ec = last_net_error();
    return socket_handle{};
}

std::size_t socket_fns::send(socket_fd_t s, const const_buffer* buffers,
                             std::size_t count, transfer_flags flags,
                             std::error_code& ec) noexcept {
    ec.clear();
    DWORD sent;
    DWORD dwFlags = static_cast<DWORD>(flags);

    wsabuf_stack_storage stack_storage;
    wsabuf_heap_storage heap_storage;
    auto buffsptr = get_wsabuffs(buffers, count, stack_storage, heap_storage);

    int result = WSASend(s, buffsptr, static_cast<DWORD>(count), &sent, dwFlags,
                         nullptr, nullptr);

    if (result == 0) {
        return sent;
    }

    ec = last_net_error();
    return 0;
}

std::size_t socket_fns::recv(socket_fd_t s, const mutable_buffer* buffers,
                             std::size_t count, bool not_zero,
                             transfer_flags flags,
                             std::error_code& ec) noexcept {
    ec.clear();
    DWORD recved;
    DWORD dwFlags = static_cast<DWORD>(flags);

    wsabuf_stack_storage stack_storage;
    wsabuf_heap_storage heap_storage;
    auto buffsptr = get_wsabuffs(buffers, count, stack_storage, heap_storage);

    int result = WSARecv(s, buffsptr, static_cast<DWORD>(count), &recved,
                         &dwFlags, nullptr, nullptr);

    if (result == 0) {
        if (not_zero && !recved) {
            ec = io::detail::make_eof_error_code();
            return 0;
        }
        return recved;
    }

    ec = last_net_error();
    return 0;
}

std::size_t socket_fns::sendto(socket_fd_t s, const const_buffer* buffers,
                               std::size_t count, transfer_flags flags,
                               const void* addr, socklen_t addr_size,
                               std::error_code& ec) noexcept {
    ec.clear();
    DWORD sent;
    DWORD dwFlags = static_cast<DWORD>(flags);

    wsabuf_stack_storage stack_storage;
    wsabuf_heap_storage heap_storage;
    auto buffsptr = get_wsabuffs(buffers, count, stack_storage, heap_storage);

    int result = WSASendTo(s, buffsptr, static_cast<DWORD>(count), &sent,
                           dwFlags, static_cast<const sockaddr*>(addr),
                           addr_size, nullptr, nullptr);

    if (result == 0) {
        return sent;
    }

    ec = last_net_error();
    return 0;
}

std::size_t socket_fns::recvfrom(socket_fd_t s, const mutable_buffer* buffers,
                                 std::size_t count, transfer_flags flags,
                                 void* addr, socklen_t* addr_size,
                                 bool not_zero, std::error_code& ec) noexcept {
    ec.clear();
    DWORD recved;
    DWORD dwFlags = static_cast<DWORD>(flags);

    wsabuf_stack_storage stack_storage;
    wsabuf_heap_storage heap_storage;
    auto buffsptr = get_wsabuffs(buffers, count, stack_storage, heap_storage);

    int result =
        WSARecvFrom(s, buffsptr, static_cast<DWORD>(count), &recved, &dwFlags,
                    static_cast<sockaddr*>(addr), addr_size, nullptr, nullptr);

    if (result == 0) {
        if (not_zero && !recved) {
            ec = io::detail::make_eof_error_code();
            return 0;
        }
        return recved;
    }

    ec = last_net_error();
    return 0;
}

void socket_fns::local_endpoint(socket_fd_t s, void* addr, socklen_t& addr_len,
                                std::error_code& ec) noexcept {
    ec.clear();
    if (::getsockname(s, static_cast<sockaddr*>(addr), &addr_len) != 0) {
        ec = last_net_error();
    }
}

void socket_fns::remote_endpoint(socket_fd_t s, void* addr, socklen_t& addr_len,
                                 std::error_code& ec) noexcept {
    ec.clear();
    if (::getpeername(s, static_cast<sockaddr*>(addr), &addr_len) != 0) {
        ec = last_net_error();
    }
}

void socket_fns::connect(socket_fd_t s, const void* addr, socklen_t addr_len,
                         std::error_code& ec) noexcept {
    ec.clear();
    if (::connect(s, static_cast<const sockaddr*>(addr), addr_len) != 0) {
        ec = last_net_error();
    }
}

void async_socket_impl::open(address_family af, socket_type type,
                             protocol_type proto,
                             std::error_code& ec) noexcept {
    ec.clear();
    auto [sock, skip_executor] = open_socket(af, type, proto, ec);
    if (ec) {
        return;
    }
    io::detail::descriptor_data unused;
    ex_->attach_handle(sock, unused, ec);
    if (!ec) {
        sock_fd_ = std::move(sock);
        skip_iocp_on_success_ = skip_executor;
        open_af_ = af;
    }
}

void async_socket_impl::cancel() noexcept {
    if (!is_open()) {
        return;
    }
    BOOL result = CancelIoEx(reinterpret_cast<HANDLE>(native_fd()), nullptr);
    if (!result) {
        assert(GetLastError() == ERROR_NOT_FOUND);
    }
}

void async_socket_impl::shutdown(socket_shutdown how,
                                 std::error_code& ec) noexcept {
    ec.clear();
    if (::shutdown(native_fd(), static_cast<int>(how)) != 0) {
        ec = last_net_error();
    }
}

void async_socket_impl::set_option(socket_option_level level,
                                   socket_option_name optname,
                                   const void* optdata, socklen_t optlen,
                                   std::error_code& ec) noexcept {
    ec.clear();
    if (setsockopt(native_fd(), ienum(level), ienum(optname),
                   static_cast<const char*>(optdata), optlen) != 0) {
        ec = last_net_error();
    }
}

void async_socket_impl::get_option(socket_option_level level,
                                   socket_option_name optname, void* optdata,
                                   socklen_t& optlen,
                                   std::error_code& ec) const noexcept {
    ec.clear();
    if (getsockopt(native_fd(), ienum(level), ienum(optname),
                   static_cast<char*>(optdata), &optlen) != 0) {
        ec = last_net_error();
    }
}

void async_socket_impl::local_endpoint(void* address, socklen_t& size,
                                       std::error_code& ec) const noexcept {
    ec.clear();
    if (::getsockname(native_fd(), static_cast<sockaddr*>(address), &size) !=
        0) {
        ec = last_net_error();
    }
}

void async_socket_impl::remote_endpoint(void* address, socklen_t& size,
                                        std::error_code& ec) const noexcept {
    ec.clear();
    if (::getpeername(native_fd(), static_cast<sockaddr*>(address), &size) !=
        0) {
        ec = last_net_error();
    }
}

void async_socket_impl::listen(uint32_t backlog, std::error_code& ec) noexcept {
    socket_fns::listen(sock_fd_.get(), backlog, ec);
}

void async_socket_impl::bind(const void* address, socklen_t size,
                             std::error_code& ec) noexcept {
    ec.clear();
    if (::bind(native_fd(), static_cast<const sockaddr*>(address), size) != 0) {
        ec = last_net_error();
    }
    else {
        bound_af_ = *static_cast<const address_family*>(address);
    }
}

auto async_socket_impl::accept(void* address, socklen_t& size,
                               std::error_code& ec) noexcept
    -> native_handle_type {
    ec.clear();
    socket_fd_t new_socket =
        ::accept(native_fd(), static_cast<sockaddr*>(address), &size);
    if (new_socket != SOCKET_ERROR) {
        return socket_handle{new_socket};
    }
    ec = last_net_error();
    return socket_handle{};
}

void async_socket_impl::connect(const void* address, socklen_t size,
                                std::error_code& ec) noexcept {
    ec.clear();
    if (::connect(native_fd(), static_cast<const sockaddr*>(address), size) !=
        0) {
        ec = last_net_error();
    }
}

std::size_t async_socket_impl::send(const const_buffer* buffs, std::size_t n,
                                    transfer_flags flags,
                                    std::error_code& ec) noexcept {
    return socket_fns::send(native_fd(), buffs, n, flags, ec);
}

std::size_t async_socket_impl::send_to(const const_buffer* buffs, std::size_t n,
                                       transfer_flags flags,
                                       const void* address, socklen_t size,
                                       std::error_code& ec) noexcept {
    return socket_fns::sendto(native_fd(), buffs, n, flags, address, size, ec);
}

std::size_t async_socket_impl::receive(const mutable_buffer* buffs,
                                       std::size_t n, bool not_zero,
                                       transfer_flags flags,
                                       std::error_code& ec) noexcept {
    return socket_fns::recv(native_fd(), buffs, n, not_zero, flags, ec);
}

std::size_t async_socket_impl::receive_from(const mutable_buffer* buffs,
                                            std::size_t n, bool not_zero,
                                            transfer_flags flags, void* address,
                                            socklen_t& size,
                                            std::error_code& ec) noexcept {
    return socket_fns::recvfrom(native_fd(), buffs, n, flags, address, &size,
                                not_zero, ec);
}

async_result async_socket_impl::do_async_accept(native_handle_type& new_sock,
                                                void* addrs,
                                                DWORD one_addr_size,
                                                io_op& op) noexcept {
    DWORD recved = 0;
    if (!is_open()) {
        return async_result::failed(
            std::make_error_code(std::errc::bad_file_descriptor));
    }
    work_guard wguard{any_ex()};
    if (winsock_inst.fn_ptrs.acceptex(native_fd(), new_sock.get(), addrs, 0,
                                      one_addr_size, one_addr_size, &recved,
                                      op.get_ov_ptr()) != TRUE) {
        const int ecode = WSAGetLastError();
        if (ecode == WSA_IO_PENDING) {
            return make_pending(wguard);
        }
        return async_result::failed(os::make_system_error(ecode));
    }
    return skip_iocp_on_success_ ? async_result::success(0)
                                 : make_pending(wguard);
}

async_result async_socket_impl::do_async_connect(const void* address,
                                                 socklen_t size,
                                                 io_op& op) noexcept {
    // unix socket can't be bound to addr any
    address_family af = *reinterpret_cast<const address_family*>(address);
    if (af != address_family::local) {
        std::error_code ec;
        socket_fns::bind_if_not(native_fd(), af, ec);
        if (ec) {
            return async_result::failed(ec);
        }
    }
    work_guard wguard{any_ex()};
    DWORD sent = 0;
    if (winsock_inst.fn_ptrs.connectex(
            native_fd(), static_cast<const sockaddr*>(address), size, nullptr,
            0, &sent, op.get_ov_ptr()) != TRUE) {
        const int ecode = WSAGetLastError();
        if (ecode == WSA_IO_PENDING) {
            return make_pending(wguard);
        }
        return async_result::failed(os::make_system_error(ecode));
    }
    return skip_iocp_on_success_ ? async_result::success(0)
                                 : make_pending(wguard);
}

async_result async_socket_impl::do_async_write(const const_buffer* buffs,
                                               uint32_t n, transfer_flags flags,
                                               io_op& op) noexcept {
    if (!is_open()) {
        return async_result::failed(
            std::make_error_code(std::errc::bad_file_descriptor));
    }
    DWORD sent = 0;
    DWORD dw_flags = static_cast<DWORD>(flags);
    wsabuf_stack_storage stack_storage;
    wsabuf_heap_storage heap_storage;
    auto buffsptr = get_wsabuffs(buffs, n, stack_storage, heap_storage);
    work_guard wguard{any_ex()};
    if (::WSASend(native_fd(), buffsptr, n, &sent, dw_flags, op.get_ov_ptr(),
                  nullptr) != 0) {
        const int ecode = WSAGetLastError();
        if (ecode == WSA_IO_PENDING) {
            return make_pending(wguard);
        }
        return async_result::failed(os::make_system_error(ecode), sent);
    }
    return skip_iocp_on_success_ ? async_result::success(sent)
                                 : make_pending(wguard);
}

async_result
async_socket_impl::do_async_sendto(const const_buffer* buffs, uint32_t n,
                                   transfer_flags flags, const void* address,
                                   socklen_t size, io_op& op) noexcept {
    if (!is_open()) {
        return async_result::failed(
            std::make_error_code(std::errc::bad_file_descriptor));
    }
    DWORD sent = 0;
    DWORD dw_flags = static_cast<DWORD>(flags);
    wsabuf_stack_storage stack_storage;
    wsabuf_heap_storage heap_storage;
    auto buffsptr = get_wsabuffs(buffs, n, stack_storage, heap_storage);
    work_guard wguard{any_ex()};
    if (::WSASendTo(native_fd(), buffsptr, n, &sent, dw_flags,
                    static_cast<const sockaddr*>(address), size,
                    op.get_ov_ptr(), nullptr) != 0) {
        const int ecode = WSAGetLastError();
        if (ecode == WSA_IO_PENDING) {
            return make_pending(wguard);
        }
        const std::error_code ec =
            ecode == WSAEMSGSIZE ? std::make_error_code(std::errc::message_size)
                                 : os::make_system_error(ecode);
        return async_result::failed(ec, sent);
    }
    return skip_iocp_on_success_ ? async_result::success(sent)
                                 : make_pending(wguard);
}

async_result async_socket_impl::do_async_read(const mutable_buffer* buffs,
                                              uint32_t n, transfer_flags flags,
                                              bool not_zero,
                                              io_op& op) noexcept {
    if (!is_open()) {
        return async_result::failed(
            std::make_error_code(std::errc::bad_file_descriptor));
    }
    DWORD recved = 0;
    DWORD dw_flags = static_cast<DWORD>(flags);
    wsabuf_stack_storage stack_storage;
    wsabuf_heap_storage heap_storage;
    auto buffsptr = get_wsabuffs(buffs, n, stack_storage, heap_storage);
    work_guard wguard{any_ex()};
    if (::WSARecv(native_fd(), buffsptr, n, &recved, &dw_flags, op.get_ov_ptr(),
                  nullptr) != 0) {
        const int ecode = WSAGetLastError();
        if (ecode == WSA_IO_PENDING) {
            return make_pending(wguard);
        }
        return async_result::failed(os::make_system_error(ecode), recved);
    }
    if (!not_zero && !recved) {
        return async_result::failed(io::detail::make_eof_error_code());
    }
    return skip_iocp_on_success_ ? async_result::success(recved)
                                 : make_pending(wguard);
}

async_result
async_socket_impl::do_async_recvfrom(const mutable_buffer* buffs, uint32_t n,
                                     transfer_flags flags, void* address,
                                     socklen_t* size, io_op& op) noexcept {
    if (!is_open()) {
        return async_result::failed(
            std::make_error_code(std::errc::bad_file_descriptor));
    }
    DWORD recved = 0;
    DWORD dw_flags = static_cast<DWORD>(flags);
    wsabuf_stack_storage stack_storage;
    wsabuf_heap_storage heap_storage;
    auto buffsptr = get_wsabuffs(buffs, n, stack_storage, heap_storage);
    work_guard wguard{any_ex()};
    if (::WSARecvFrom(native_fd(), buffsptr, n, &recved, &dw_flags,
                      static_cast<sockaddr*>(address), size, op.get_ov_ptr(),
                      nullptr) != 0) {
        const int ecode = WSAGetLastError();
        if (ecode == WSA_IO_PENDING) {
            return make_pending(wguard);
        }
        const std::error_code ec =
            ecode == WSAEMSGSIZE ? std::make_error_code(std::errc::message_size)
                                 : os::make_system_error(ecode);
        return async_result::failed(ec, recved);
    }
    return skip_iocp_on_success_ ? async_result::success(recved)
                                 : make_pending(wguard);
}

std::pair<const uint8_t*, socklen_t>
async_socket_impl::get_accept_result(io_op& op, uint8_t* addrs,
                                     DWORD one_addr_size,
                                     std::error_code& ec) noexcept {
    DWORD transferred;
    get_async_result(native_fd(), op, transferred, ec);
    if (ec) {
        return {nullptr, 0};
    }
    return parse_accept_addrs(addrs, one_addr_size);
}

std::error_code async_socket_impl::get_connect_result(io_op& op) noexcept {
    DWORD transferred;
    std::error_code ec;
    get_async_result(native_fd(), op, transferred, ec);
    if (!ec) {
        auto opt = socket_options::update_connect_context{};
        set_option(opt.level(), opt.name(), opt.data(), opt.size(), ec);
    }
    return ec;
}

std::size_t async_socket_impl::get_write_result(io_op& op,
                                                std::error_code& ec) noexcept {
    DWORD dw_trans = 0;
    get_async_result(native_fd(), op, dw_trans, ec);
    return dw_trans;
}

std::size_t async_socket_impl::get_read_result(io_op& op, bool not_zero,
                                               std::error_code& ec) noexcept {
    DWORD dw_trans = 0;
    get_async_result(native_fd(), op, dw_trans, ec);
    if (!ec && not_zero && !dw_trans) {
        ec = io::detail::make_eof_error_code();
    }
    return dw_trans;
}

std::pair<const uint8_t*, socklen_t>
async_socket_impl::parse_accept_addrs(uint8_t* addrs,
                                      DWORD one_addr_size) noexcept {
    sockaddr *local_addr, *remote_addr;
    int local_len, remote_len;
    winsock_inst.fn_ptrs.parseaddr(addrs, 0, one_addr_size, one_addr_size,
                                   &local_addr, &local_len, &remote_addr,
                                   &remote_len);
    return {reinterpret_cast<const uint8_t*>(remote_addr), remote_len};
}

/*
awaiters implementation

- await_ready will return true only if there was an error before the operation
has started so the coroutine will go immediately to await_resume

- await_suspend will store the waiting coroutine handle and issue
impl->do_async_op() and check the reuslt, if the operation is still pending then
it will return true immediately and won't access any members because the awaiter
may have been destructed on another thread, otherwise if the operation is not
pending then collect the result and return false because the operation won't go
through the executor
*/

bool async_socket_impl::connect_awaiter::await_suspend(
    std::coroutine_handle<> coro) {
    waiter = coro;
    auto result = impl->do_async_connect(address, size, *this);
    if (result.is_pending()) {
        return true;
    }
    store(result.error());
    return false;
}

void async_socket_impl::connect_awaiter::invoke_operation() {
    store(impl->get_connect_result(*this));
    waiter.resume();
}

bool async_socket_impl::write_awaiter::await_suspend(
    std::coroutine_handle<> coro) {
    waiter = coro;
    auto result = impl->do_async_write(buffs, nf.count, nf.flags, *this);
    if (result.is_pending()) {
        return true;
    }
    transferred = result.transferred();
    store(result.error());
    return false;
}

void async_socket_impl::write_awaiter::invoke_operation() {
    std::error_code ec;
    transferred = impl->get_write_result(*this, ec);
    store(ec);
    waiter.resume();
}

bool async_socket_impl::read_awaiter::await_suspend(
    std::coroutine_handle<> coro) {
    waiter = coro;
    auto result =
        impl->do_async_read(buffs, bn.count, flags, bn.not_zero, *this);
    if (result.is_pending()) {
        return true;
    }
    bool eof_if_zero = bn.not_zero;
    transferred = result.transferred();
    store(result.error());
    if (!result.has_error() && !transferred && eof_if_zero) {
        store(io::detail::make_eof_error_code());
    }
    impl->sync_completed_in_ops_ += 1;
    if (impl->sync_completed_in_ops_ >
        async_socket_impl::max_sync_in_ops_completions) {
        impl->sync_completed_in_ops_ = 0;
        // so that invoke_operation don't use get_read_result!
        assert(buffs != nullptr);
        buffs = nullptr;
        auto& ex = associated_executor();
        ex.post(*this);
        // the awaiable may have been destructed!
        return true;
    }
    return false;
}

void async_socket_impl::read_awaiter::invoke_operation() {
    if (buffs != nullptr) {
        std::error_code ec;
        transferred = impl->get_read_result(*this, bn.not_zero, ec);
        store(ec);
    }
    waiter.resume();
}

bool async_socket_impl::sendto_awaiter::await_suspend(
    std::coroutine_handle<> coro) {
    waiter = coro;
    auto result = impl->do_async_sendto(buffs, nf.count, nf.flags, address,
                                        address_size, *this);
    if (result.is_pending()) {
        return true;
    }

    transferred = result.transferred();
    store(result.error());
    return false;
}
