#include <rad/async/io_executor.h>
#include <rad/net/blocking_socket.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>

using namespace RAD_LIB_NAMESPACE;
using namespace net;
using namespace net::detail;

namespace {
    template <class Fn, class... Args>
    std::invoke_result_t<Fn, Args...> exec_while_eintr(Fn fn, Args&&... args) {
        while (1) {
            auto result = fn(std::forward<Args>(args)...);
            if (result == -1 && errno == EINTR) {
                continue;
            }
            return result;
        }
    }
} // namespace

socket_handle socket_fns::open(address_family af, socket_type type,
                               protocol_type proto,
                               std::error_code& ec) noexcept {
    auto fd = ::socket(as<int>(af), as<int>(type), as<int>(proto));
    if (fd == -1) {
        ec = os::make_system_error(errno);
    }
    return socket_handle{fd};
}

void socket_fns::shutdown(socket_fd_t s, socket_shutdown how,
                          std::error_code& ec) noexcept {
    if (::shutdown(s, static_cast<int>(how)) != 0) {
        ec = os::make_system_error(errno);
    }
}

void socket_fns::setopt(socket_fd_t s, socket_option_level level,
                        socket_option_name optname, const void* optdata,
                        socket_len_t optlen, std::error_code& ec) noexcept {
    int res =
        ::setsockopt(s, as<int>(level), as<int>(optname), optdata, optlen);
    if (res != 0) {
        ec = os::make_system_error(errno);
    }
}

void socket_fns::getopt(socket_fd_t s, socket_option_level level,
                        socket_option_name optname, void* optdata,
                        socket_len_t* optlen, std::error_code& ec) noexcept {
    socklen_t len = *optlen;
    int res = ::getsockopt(s, as<int>(level), as<int>(optname), optdata, &len);
    *optlen = len;
    if (res != 0) {
        ec = os::make_system_error(errno);
    }
}

void socket_fns::bind(socket_fd_t s, const void* addr, socket_len_t addr_len,
                      std::error_code& ec) noexcept {
    if (::bind(s, as<const sockaddr*>(addr), addr_len) != 0) {
        ec = os::make_system_error(errno);
    }
}

int socket_fns::max_listen_backlog() noexcept {
    return SOMAXCONN;
}

void socket_fns::listen(socket_fd_t s, uint32_t backlog,
                        std::error_code& ec) noexcept {
    backlog = (backlog == 0 || backlog > SOMAXCONN) ? SOMAXCONN : backlog;
    if (::listen(s, backlog) != 0) {
        ec = os::make_system_error(errno);
    }
}

socket_handle socket_fns::accept(socket_fd_t s, void* addr,
                                 socket_len_t& addr_size,
                                 std::error_code& ec) noexcept {
    socklen_t addr_len = addr_size;
    int fd = ::accept(s, as<sockaddr*>(addr), &addr_len);
    if (fd == -1) {
        ec = os::make_system_error(errno);
    }
    addr_size = addr_len;
    return socket_handle{fd};
}

namespace {
    inline std::pair<const_buffer*, size_t>
    skip_bytes(const_buffer* buffs, size_t count, size_t transferred) noexcept {
        while (transferred) {
            if (buffs->size() > transferred) {
                *buffs += transferred;
                break;
            }

            transferred -= buffs->size();
            ++buffs;
            --count;
        }

        while (count && buffs->empty()) {
            ++buffs;
            --count;
        }

        return {buffs, count};
    }

    inline std::pair<mutable_buffer*, size_t>
    skip_bytes(mutable_buffer* buffs, size_t count,
               size_t transferred) noexcept {
        while (transferred) {
            if (buffs->size() > transferred) {
                *buffs += transferred;
                break;
            }

            transferred -= buffs->size();
            ++buffs;
            --count;
        }

        while (count && buffs->empty()) {
            ++buffs;
            --count;
        }

        return {buffs, count};
    }

    inline size_t sendto1buff(socket_fd_t s, const_buffer buffer,
                              transfer_flags flags, const void* addr,
                              socklen_t len, std::error_code& ec) {
        flags &= ~transfer_flags::dont_wait;
        flags |= transfer_flags::no_signal;

        size_t total_size = buffer.size();

        do {
            auto result =
                exec_while_eintr(::sendto, s, buffer.data(), buffer.size(),
                                 static_cast<int>(flags),
                                 static_cast<const sockaddr*>(addr), len);
            if (result == -1) {
                ec = os::make_system_error(errno);
                return 0;
            }
            buffer += static_cast<size_t>(result);
        } while (!buffer.empty());

        return total_size;
    }

    inline size_t recvfrom1buff(socket_fd_t s, mutable_buffer buffer,
                                transfer_flags flags, void* addr,
                                socket_len_t* len, bool not_zero,
                                std::error_code& ec) {
        flags &= ~transfer_flags::dont_wait;
        bool read_all_buffer = not_zero && flags & transfer_flags::wait_all;

        size_t total_size = buffer.size();

        do {
            socklen_t slen = len == nullptr ? 0 : *len;
            auto result = exec_while_eintr(
                ::recvfrom, s, buffer.data(), buffer.size(),
                static_cast<int>(flags), static_cast<sockaddr*>(addr),
                len != nullptr ? &slen : nullptr);

            if (result == -1) {
                ec = os::make_system_error(errno);
                return 0;
            }

            if (!result) {
                if (not_zero) {
                    ec = io::detail::make_eof_error_code();
                }
                return 0;
            }

            buffer += static_cast<size_t>(result);
        } while (read_all_buffer && !buffer.empty());

        return total_size - buffer.size();
    }
} // namespace

size_t socket_fns::send(socket_fd_t s, const const_buffer* buffers,
                        size_t count, transfer_flags flags,
                        std::error_code& ec) noexcept {
    flags &= ~transfer_flags::dont_wait;
    flags |= transfer_flags::no_signal;

    if (count == 1) {
        return sendto1buff(s, *buffers, flags, nullptr, 0, ec);
    }
    return sendto(s, buffers, count, flags, nullptr, 0, ec);
}

size_t socket_fns::recv(socket_fd_t s, const mutable_buffer* buffers,
                        size_t count, bool not_zero, transfer_flags flags,
                        std::error_code& ec) noexcept {
    if (count == 1) {
        return recvfrom1buff(s, *buffers, flags, nullptr, nullptr, not_zero,
                             ec);
    }
    return recvfrom(s, buffers, count, flags, nullptr, nullptr, not_zero, ec);
}

size_t socket_fns::sendto(socket_fd_t s, const const_buffer* buffers,
                          size_t count, transfer_flags flags, const void* addr,
                          socket_len_t addr_size,
                          std::error_code& ec) noexcept {
    flags &= ~transfer_flags::dont_wait;
    flags |= transfer_flags::no_signal;

    if (count == 1) {
        return sendto1buff(s, *buffers, flags, addr, addr_size, ec);
    }

    msghdr msg = {};
    msg.msg_name = const_cast<void*>(addr);
    msg.msg_namelen = addr_size;

    auto buffs_ptr = const_cast<const_buffer*>(buffers);

    size_t transferred = 0;

    while (count) {
        msg.msg_iov = reinterpret_cast<iovec*>(buffs_ptr);
        msg.msg_iovlen = count;

        auto result =
            exec_while_eintr(::sendmsg, s, &msg, static_cast<int>(flags));

        if (result == -1) {
            ec = os::make_system_error(errno);
            return 0;
        }

        size_t chunk_size = static_cast<size_t>(result);
        transferred += chunk_size;

        std::tie(buffs_ptr, count) = skip_bytes(buffs_ptr, count, chunk_size);
    }

    return transferred;
}

size_t socket_fns::recvfrom(socket_fd_t s, const mutable_buffer* buffers,
                            size_t count, transfer_flags flags, void* addr,
                            socket_len_t* addr_size, bool not_zero,
                            std::error_code& ec) noexcept {
    static_assert(sizeof(iovec) == sizeof(mutable_buffer),
                  "sizeof iovec != sizeof mutable_buffer");

    if (count == 1) {
        return recvfrom1buff(s, *buffers, flags, addr, addr_size, not_zero, ec);
    }

    flags &= ~transfer_flags::dont_wait;
    bool read_all_buffers = flags & transfer_flags::wait_all;

    msghdr msg = {};
    msg.msg_name = addr;
    if (addr_size) {
        msg.msg_namelen = *addr_size;
    }

    auto buffs_ptr = const_cast<mutable_buffer*>(buffers);

    size_t transferred = 0;

    do {
        msg.msg_iov = reinterpret_cast<iovec*>(buffs_ptr);
        msg.msg_iovlen = count;

        auto result =
            exec_while_eintr(::recvmsg, s, &msg, static_cast<int>(flags));

        if (result == -1) {
            ec = os::make_system_error(errno);
            return 0;
        }

        if (!result) {
            if (not_zero) {
                ec = io::detail::make_eof_error_code();
            }
            return 0;
        }

        size_t chunk_size = static_cast<size_t>(result);
        transferred += chunk_size;

        if (!not_zero) {
            break;
        }

        std::tie(buffs_ptr, count) = skip_bytes(buffs_ptr, count, chunk_size);

    } while (read_all_buffers && count);

    if (addr_size) {
        *addr_size = msg.msg_namelen;
    }

    return transferred;
}

void socket_fns::local_endpoint(socket_fd_t s, void* addr,
                                socket_len_t& addr_len,
                                std::error_code& ec) noexcept {
    socklen_t slen = addr_len;
    int result = ::getsockname(s, static_cast<sockaddr*>(addr), &slen);
    addr_len = slen;
    if (result != 0) {
        ec = os::make_system_error(errno);
    }
}

void socket_fns::remote_endpoint(socket_fd_t s, void* addr,
                                 socket_len_t& addr_len,
                                 std::error_code& ec) noexcept {
    socklen_t slen = addr_len;
    int result = ::getpeername(s, static_cast<sockaddr*>(addr), &slen);
    addr_len = slen;
    if (result != 0) {
        ec = os::make_system_error(errno);
    }
}

void socket_fns::connect(socket_fd_t s, const void* addr, socket_len_t addr_len,
                         std::error_code& ec) noexcept {
    if (::connect(s, static_cast<const sockaddr*>(addr), addr_len) != 0) {
        ec = os::make_system_error(errno);
    }
}