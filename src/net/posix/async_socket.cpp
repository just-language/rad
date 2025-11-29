#include <rad/async/io_loop.h> // to implement open_socket
#include <rad/net/detail/posix/async_socket_impl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <sys/fcntl.h>
#include <sys/socket.h>
#include <sys/types.h>

using namespace RAD_LIB_NAMESPACE;
using namespace net;
using net::detail::async_socket_impl;

#include "../compiletime_checks.h"

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

    inline bool connect_would_block(const void* addr, int ec) noexcept {
        address_family af = *static_cast<const address_family*>(addr);
        if (af == address_family::local) {
            return ec == EAGAIN;
        }
        return ec == EINPROGRESS;
    }
} // namespace

#ifdef __linux__
// static
bool async_socket_impl::accept_should_retry(int ec) noexcept {
    if (ec == EWOULDBLOCK || ec == EAGAIN || ec == EPROTO || ec == ENETDOWN ||
        ec == ENOPROTOOPT || ec == EHOSTDOWN || ec == ENONET ||
        ec == EHOSTUNREACH || ec == EOPNOTSUPP || ec == ENETUNREACH) {
        return true;
    }
    return false;
}
#else
bool accept_should_retry(int ec) noexcept {
    if (ec == EWOULDBLOCK || ec == EAGAIN) {
        return true;
    }
    return false;
}
#endif // __linux__

void async_socket_impl::open(address_family af, socket_type type,
                             protocol_type proto,
                             std::error_code& ec) noexcept {
    ec.clear();
    int extra_flags = SOCK_CLOEXEC;
    if (!executor().is_async()) {
        extra_flags |= SOCK_NONBLOCK;
    }
    int fd = ::socket(as<int>(af), as<int>(type) | extra_flags, as<int>(proto));
    if (fd == -1) {
        ec = os::make_system_error(errno);
        return;
    }
    auto data = io::detail::attach_fd_to_executor(fd, executor(), ec);
    if (ec) {
        ::close(fd);
        return;
    }
    data_ = std::move(data);
}

void async_socket_impl::shutdown(socket_shutdown how,
                                 std::error_code& ec) noexcept {
    ec.clear();
    if (::shutdown(native_handle().get(), static_cast<int>(how)) == -1) {
        ec = os::make_system_error(errno);
    }
}

void async_socket_impl::set_option(socket_option_level level,
                                   socket_option_name optname,
                                   const void* optdata, socket_len_t optlen,
                                   std::error_code& ec) noexcept {
    ec.clear();
    if (::setsockopt(native_handle().get(), as<int>(level), as<int>(optname),
                     optdata, optlen) == -1) {
        ec = os::make_system_error(errno);
    }
}

void async_socket_impl::get_option(socket_option_level level,
                                   socket_option_name optname, void* optdata,
                                   socket_len_t& optlen,
                                   std::error_code& ec) const noexcept {
    ec.clear();
    socklen_t opt_len = optlen;
    if (::getsockopt(native_handle().get(), as<int>(level), as<int>(optname),
                     optdata, &opt_len) == -1) {
        ec = os::make_system_error(errno);
    }
    optlen = opt_len;
}

void async_socket_impl::local_endpoint(void* address, socket_len_t& size,
                                       std::error_code& ec) const noexcept {
    ec.clear();
    socklen_t add_len = size;
    if (::getsockname(native_handle().get(), static_cast<sockaddr*>(address),
                      &add_len) == -1) {
        ec = os::make_system_error(errno);
    }
    size = add_len;
}

void async_socket_impl::remote_endpoint(void* address, socket_len_t& size,
                                        std::error_code& ec) const noexcept {
    ec.clear();
    socklen_t add_len = size;
    if (::getpeername(native_handle().get(), static_cast<sockaddr*>(address),
                      &add_len) == -1) {
        ec = os::make_system_error(errno);
    }
    size = add_len;
}

void async_socket_impl::listen(int backlog, std::error_code& ec) noexcept {
    ec.clear();
    backlog = std::min(backlog, SOMAXCONN);
    if (::listen(native_handle().get(), backlog) == -1) {
        ec = os::make_system_error(errno);
    }
}

void async_socket_impl::bind(const void* address, socket_len_t size,
                             std::error_code& ec) noexcept {
    ec.clear();
    const sockaddr* saddr = static_cast<const sockaddr*>(address);
    if (::bind(native_handle().get(), saddr, size) == -1) {
        ec = os::make_system_error(errno);
    }
    else {
        bound_af_ = static_cast<address_family>(saddr->sa_family);
    }
}

// sync operations

auto async_socket_impl::accept(void* address, socket_len_t& size,
                               std::error_code& ec) noexcept
    -> native_handle_type {
    ec.clear();
    if (!is_open()) {
        ec = std::make_error_code(std::errc::bad_file_descriptor);
        return {};
    }

    assert(data_ != nullptr);
    auto inner = data_->inner.synchronize();
    if (inner->in_pending()) {
        ec = std::make_error_code(std::errc::operation_in_progress);
        return {};
    }

    while (1) {
        auto result = perform_accept(address, &size);
        if (!result.is_pending()) {
            if (result.has_error()) {
                ec = result.error();
                return {};
            }
            return socket_handle{result.fd()};
        }

        io::detail::wait_until_readable(native_fd(), ec);
        if (ec) {
            return {};
        }
    }
}

void async_socket_impl::connect(const void* address, socket_len_t size,
                                std::error_code& ec) noexcept {
    ec.clear();
    if (!is_open()) {
        ec = std::make_error_code(std::errc::bad_file_descriptor);
        return;
    }

    assert(data_ != nullptr);
    auto inner = data_->inner.synchronize();
    if (inner->out_pending()) {
        ec = std::make_error_code(std::errc::operation_in_progress);
        return;
    }
    // try to connect
    int result = exec_while_eintr(::connect, native_handle().get(),
                                  static_cast<const sockaddr*>(address), size);
    // returns 0 on success
    if (!result) {
        return;
    }
    result = errno;
    // if failed and not pending
    if (!connect_would_block(address, result)) {
        ec = os::make_system_error(result);
        return;
    }

    // connect is pending so wait for the socket to be writable (connected)
    // and then get the result
    io::detail::wait_until_writable(native_fd(), ec);
    // if failed to wait return with error
    if (ec) {
        return;
    }

    // the socket is now connected or failed to connect, get the result
    ec = os::make_system_error(get_connect_result());
}

std::size_t async_socket_impl::send_to(const const_buffer* buffs, std::size_t n,
                                       transfer_flags flags,
                                       const void* address, socket_len_t size,
                                       std::error_code& ec) noexcept {
    ec.clear();
    if (!is_open()) {
        ec = std::make_error_code(std::errc::bad_file_descriptor);
        return 0;
    }
    flags |= transfer_flags::no_signal;

    assert(data_ != nullptr);
    auto inner = data_->inner.synchronize();
    if (inner->out_pending()) {
        ec = std::make_error_code(std::errc::operation_in_progress);
        return 0;
    }

    std::size_t transferred = 0;
    io::iovec_buffers iovecs{buffs, n};
    bool first_send = true;
    while (1) {
        // try to send
        auto result = perform_sendto(first_send, iovecs, flags, address, size);
        first_send = false;
        transferred += result.transferred();

        if (!result.is_pending()) {
            if (result.has_error()) {
                ec = result.error();
                return 0;
            }
            return transferred;
        }

        io::detail::wait_until_writable(native_fd(), ec);
        if (ec) {
            return 0;
        }
    }
}

std::size_t async_socket_impl::receive_from(const mutable_buffer* buffs,
                                            std::size_t n, bool not_zero,
                                            transfer_flags flags, void* address,
                                            socket_len_t& size,
                                            std::error_code& ec) noexcept {
    ec.clear();
    if (!is_open()) {
        ec = std::make_error_code(std::errc::bad_file_descriptor);
        return 0;
    }

    assert(data_ != nullptr);
    auto inner = data_->inner.synchronize();
    if (inner->in_pending()) {
        ec = std::make_error_code(std::errc::operation_in_progress);
        return 0;
    }

    std::size_t transferred = 0;
    io::iovec_buffers iovecs{buffs, n};
    while (1) {
        auto result = perform_recvfrom(iovecs, flags, address, &size, not_zero);
        transferred += result.transferred();

        if (!result.is_pending()) {
            if (result.has_error()) {
                ec = result.error();
                return 0;
            }
            return transferred;
        }
        io::detail::wait_until_readable(native_fd(), ec);
        if (ec) {
            return 0;
        }
    }
}

// async operations

async_result async_socket_impl::async_accept(void* address, socket_len_t* size,
                                             op_alloc_fn alloc_fn) noexcept {
    if (!is_open()) {
        return async_result::failed(
            std::make_error_code(std::errc::bad_file_descriptor));
    }

    assert(data_ != nullptr);
    auto inner = data_->inner.synchronize();
    if (inner->in_pending()) {
        return async_result::failed(
            std::make_error_code(std::errc::operation_in_progress));
    }

#ifdef RAD_HAS_IO_URING
    if (data_->is_io_uring()) {
        inner->in_op = alloc_fn();
        inner->in_op->descriptor = data_.get();
        std::error_code ec;
        inner->in_op->submit(*inner, ec);
        if (ec) {
            inner->in_op->complete(ec, 0);
            any_ex().post(*std::exchange(inner->in_op, nullptr));
            return make_pending();
        }
        inner->pending_ops += 1;
        return make_pending();
    }
#endif // RAD_HAS_IO_URING
    auto result = perform_accept(address, size);
    // finished early, no need to allocate an io op
    if (!result.is_pending()) {
        return result;
    }

    // let the loop do the accept operation when the socket becomes ready
    // for accepting
    auto accept_op = alloc_fn();
    inner->set_in(accept_op);
    return make_pending();
}

async_result async_socket_impl::perform_accept(void* address,
                                               socket_len_t* size) noexcept {
    // the lock is held

    int flags = SOCK_CLOEXEC | SOCK_NONBLOCK;
    // if the socket is ready for in operations, try to accept immediately
    socklen_t addr_len = size != nullptr ? *size : 0;
    int result = exec_while_eintr(::accept4, native_handle().get(),
                                  static_cast<sockaddr*>(address),
                                  size == nullptr ? nullptr : &addr_len, flags);

    if (result != -1) {
        if (size != nullptr) {
            *size = addr_len;
        }
        return async_result::success(
            static_cast<std::size_t>(result)); // result is the fd
    }

    // examine errno
    result = errno;

    if (!accept_should_retry(result)) {
        return async_result::failed(os::make_system_error(result));
    }

    // there is no incoming connections now, so wait for a connection to
    // arrive before retry
    return async_result::pending();
}

async_result async_socket_impl::async_connect(const void* addr,
                                              socket_len_t len,
                                              op_alloc_fn alloc_fn) noexcept {
    if (!is_open()) {
        return async_result::failed(
            std::make_error_code(std::errc::bad_file_descriptor));
    }

    assert(data_ != nullptr);
    auto inner = data_->inner.synchronize();
    if (inner->out_pending()) {
        return async_result::failed(
            std::make_error_code(std::errc::operation_in_progress));
    }
#ifdef RAD_HAS_IO_URING
    if (data_->is_io_uring()) {
        inner->out_op = alloc_fn();
        inner->out_op->descriptor = data_.get();
        std::error_code ec;
        inner->out_op->submit(*inner, ec);
        if (ec) {
            inner->out_op->complete(ec, 0);
            any_ex().post(*std::exchange(inner->out_op, nullptr));
            return make_pending();
        }
        inner->pending_ops += 1;
        return make_pending(0);
    }
#endif // RAD_HAS_IO_URING

    // issue the connect request and return immediately if the result is not
    // pending
    auto result = perform_connect(addr, len);
    if (!result.is_pending()) {
        return result;
    }

    // let the loop get the result of connect when the socket finishes the
    // pending connect
    inner->set_out(alloc_fn());
    return make_pending();
}

async_result async_socket_impl::perform_connect(const void* addr,
                                                socket_len_t len) noexcept {
    // the lock is held here

    int result = exec_while_eintr(::connect, native_handle().get(),
                                  static_cast<const sockaddr*>(addr), len);

    // if connect returned 0 then it has succeeded
    if (!result) {
        return async_result::success(0);
    }

    // check errno value
    result = errno;
    if (connect_would_block(addr, result)) {
        return async_result::pending();
    }
    return async_result::failed(os::make_system_error(result));
}

int async_socket_impl::get_connect_result() noexcept {
    // the lock is held

    int connect_result;
    socklen_t result_size = sizeof(connect_result);
    int ec = ::getsockopt(
        native_handle().get(), as<int>(socket_option_level::socket),
        as<int>(socket_option_name::error), &connect_result, &result_size);
    if (ec) {
        return errno;
    }
    return connect_result;
}

async_result async_socket_impl::async_sendto(
    io::iovec_buffers& buffs, const void* addr, socket_len_t addr_len,
    transfer_flags flags, function_view<write_op_base*()> alloc_fn) noexcept {
    flags |= transfer_flags::no_signal | transfer_flags::dont_wait;
    flags &= ~transfer_flags::wait_all;

    if (!is_open()) {
        return async_result::failed(
            std::make_error_code(std::errc::bad_file_descriptor));
    }

    assert(data_ != nullptr);
    auto inner = data_->inner.synchronize();
    if (inner->out_pending()) {
        return async_result::failed(
            std::make_error_code(std::errc::operation_in_progress));
    }
#ifdef RAD_HAS_IO_URING
    if (data_->is_io_uring()) {
        inner->out_op = alloc_fn();
        inner->out_op->descriptor = data_.get();
        std::error_code ec;
        inner->out_op->submit(*inner, ec);
        if (ec) {
            inner->out_op->complete(ec, 0);
            any_ex().post(*std::exchange(inner->out_op, nullptr));
            return make_pending();
        }
        inner->pending_ops += 1;
        return make_pending(0);
    }
#endif // RAD_HAS_IO_URING
    auto result = perform_sendto(true, buffs, flags, addr, addr_len);
    // if sent all the data without blocking or failed with error other than
    // would block
    if (!result.is_pending()) {
        return result;
    }

    // the socket isn't writable right now, wait for it to become ready for
    // writing
    inner->set_ready_out(false);
    auto sendto_op = alloc_fn();
    sendto_op->transferred = result.transferred();
    sendto_op->flags = flags;
    inner->set_out(sendto_op);

    return make_pending(result.transferred());
}

async_result async_socket_impl::perform_sendto(bool first_time,
                                               io::iovec_buffers& buffs,
                                               transfer_flags flags,
                                               const void* addr,
                                               socket_len_t addr_len) noexcept {
    // the lock is held
    std::size_t total_transferred = 0;
    io::iovec_buff null_iovec{};
    while (1) {
        auto buffs_ptr = buffs.get_buffers();
        std::size_t buffs_count = buffs.get_count();
        if (buffs_ptr == nullptr || buffs_count == 0) {
            if (first_time) {
                buffs_ptr = &null_iovec;
                buffs_count = 0;
            }
            else {
                return async_result::success(total_transferred);
            }
        }
        first_time = false;

        ssize_t result = -1;
        if (buffs_count <= 1) {
            if (addr_len != 0 && addr != nullptr) {
                result = exec_while_eintr(::sendto, native_handle().get(),
                                          buffs_ptr->iov_base,
                                          buffs_ptr->iov_len, as<int>(flags),
                                          as<const sockaddr*>(addr), addr_len);
            }
            else {
                result = exec_while_eintr(::send, native_handle().get(),
                                          buffs_ptr->iov_base,
                                          buffs_ptr->iov_len, as<int>(flags));
            }
        }
        else {
            msghdr msg = {};
            msg.msg_name = const_cast<void*>(addr);
            msg.msg_namelen = addr_len;
            msg.msg_iov = reinterpret_cast<iovec*>(buffs_ptr);
            msg.msg_iovlen = buffs_count;
            if (msg.msg_iovlen > IOV_MAX) {
                msg.msg_iovlen = IOV_MAX;
            }
            result = exec_while_eintr(::sendmsg, native_handle().get(), &msg,
                                      as<int>(flags));
        }

        if (result < 0) {
            result = errno;
            if (result == EWOULDBLOCK || result == EAGAIN) {
                return async_result::pending(total_transferred);
            }
            return async_result::failed(os::make_system_error(result),
                                        total_transferred);
        }

        total_transferred += static_cast<std::size_t>(result);
        buffs.advance(static_cast<std::size_t>(result));
    }
}

async_result async_socket_impl::async_recvfrom(
    io::iovec_buffers& buffs, void* addr, socket_len_t& addr_len,
    transfer_flags flags, bool not_zero,
    function_view<read_op_base*()> alloc_fn) noexcept {
    if (!is_open()) {
        return async_result::failed(
            std::make_error_code(std::errc::bad_file_descriptor));
    }

    assert(data_ != nullptr);
    auto inner = data_->inner.synchronize();
    if (inner->in_pending()) {
        return async_result::failed(
            std::make_error_code(std::errc::operation_in_progress));
    }

#ifdef RAD_HAS_IO_URING
    if (data_->is_io_uring()) {
        inner->in_op = alloc_fn();
        inner->in_op->descriptor = data_.get();
        std::error_code ec;
        inner->in_op->submit(*inner, ec);
        if (ec) {
            inner->in_op->complete(ec, 0);
            any_ex().post(*std::exchange(inner->in_op, nullptr));
            return make_pending();
        }
        inner->pending_ops += 1;
        return make_pending();
    }
#endif // RAD_HAS_IO_URING
    // try to recevie some data
    auto result = perform_recvfrom(buffs, flags, addr, &addr_len, not_zero);

    // if received all the data or received some data and wait_all was not
    // specified or failed
    if (!result.is_pending()) {
        return result;
    }

    inner->set_ready_in(false);
    auto recvfrom_op = alloc_fn();
    recvfrom_op->transferred = result.transferred();
    recvfrom_op->flags = flags;
    recvfrom_op->not_zero = not_zero;
    inner->set_in(recvfrom_op);

    return make_pending(result.transferred());
}

async_result async_socket_impl::perform_recvfrom(io::iovec_buffers& buffs,
                                                 transfer_flags flags,
                                                 void* addr,
                                                 socket_len_t* addr_len,
                                                 bool not_zero) noexcept {
    // the lock is held
    std::size_t total_transferred = 0;
    const bool is_stream = not_zero;
    bool read_all_buffs = flags & transfer_flags::wait_all;
    flags &= ~transfer_flags::wait_all;
    flags |= transfer_flags::dont_wait;

    io::iovec_buff empty_buff{};

    while (1) {
        auto buffs_ptr = buffs.get_buffers();
        std::size_t buffs_count = buffs.get_count();
        if (buffs_ptr == nullptr || buffs_count == 0) {
            if (not_zero) {
                return async_result::success(total_transferred);
            }
            buffs_ptr = &empty_buff;
        }
        not_zero = true;

        ssize_t result = -1;
        socket_len_t msg_namelen = 0;
        if (buffs_count == 1) {
            if (addr_len != nullptr && addr != nullptr) {
                socklen_t from_len = *addr_len;
                result = exec_while_eintr(::recvfrom, native_handle().get(),
                                          buffs_ptr->iov_base,
                                          buffs_ptr->iov_len, as<int>(flags),
                                          as<sockaddr*>(addr), &from_len);
                *addr_len = from_len;
            }
            else {
                result = exec_while_eintr(::recv, native_handle().get(),
                                          buffs_ptr->iov_base,
                                          buffs_ptr->iov_len, as<int>(flags));
            }
        }
        else {
            msghdr msg = {};
            msg.msg_iov = reinterpret_cast<iovec*>(buffs_ptr);
            msg.msg_iovlen = buffs_count;
            if (msg.msg_iovlen > IOV_MAX) {
                msg.msg_iovlen = IOV_MAX;
            }
            if (addr_len && addr) {
                msg.msg_name = addr;
                msg.msg_namelen = *addr_len;
            }
            result = exec_while_eintr(::recvmsg, native_handle().get(), &msg,
                                      as<int>(flags));
            msg_namelen = msg.msg_namelen;
        }

        if (result < 0) {
            result = errno;
            if (result == EWOULDBLOCK || result == EAGAIN) {
                return async_result::pending(total_transferred);
            }
            return async_result::failed(os::make_system_error(result),
                                        total_transferred);
        }

        if (result == 0) {
            // if read_all_buffs is true then total_transferred must be 0
            // since recv will be called only once
            assert(read_all_buffs == false || total_transferred == 0);
            // tcp 0 indicates EOF
            if (is_stream) {
                return async_result::failed(io::detail::make_eof_error_code(),
                                            total_transferred);
            }
            // udp packets may have 0 length
            if (addr_len && addr) {
                *addr_len = msg_namelen;
            }
            // it was a zero read or some data was read
            return async_result::success(total_transferred);
        }

        std::size_t transferred = static_cast<std::size_t>(result);
        total_transferred += transferred;
        buffs.advance(transferred);

        if (!read_all_buffs || !is_stream) {
            if (addr_len && addr) {
                *addr_len = msg_namelen;
            }
            return async_result::success(total_transferred);
        }
    }
}

// awaiter implementaions
// if the result is pending return immediately and don't use the awaiter
// before doing anything in perform check if the operations is canceled

async_socket_impl::write_awaiter::~write_awaiter() = default;

bool async_socket_impl::write_awaiter::await_suspend(
    std::coroutine_handle<> coro) {
    waiter = coro;
    auto result = impl->async_send(
        buffers, flags, [this]() { return static_cast<write_op_base*>(this); });
    if (result.is_pending()) {
        return true;
    }
    transferred = result.transferred();
    store(result.error());
    return false;
}

std::size_t async_socket_impl::write_awaiter::await_resume() const {
    raise("async_write");
    return has_error() ? 0 : transferred;
}

bool async_socket_impl::write_awaiter::perform() noexcept {
    // the lock is held
    assert(!canceled);
    auto result = impl->perform_send(buffers, flags);
    transferred += result.transferred();
    if (result.is_pending()) {
        return false;
    }
    store(result.error());
    return true;
}

void async_socket_impl::write_awaiter::invoke_operation() {
    if (canceled) {
        store(std::make_error_code(std::errc::operation_canceled));
    }
    waiter.resume();
}

any_executor&
async_socket_impl::write_awaiter::associated_executor() const noexcept {
    return impl->any_ex();
}

#ifdef RAD_HAS_IO_URING
void async_socket_impl::write_awaiter::submit(
    io::detail::descriptor_data_inner_t& inner, std::error_code& ec) noexcept {
    std::size_t buffs_count = buffers.get_count();
    io::iovec_buff* buffs_ptr = buffers.get_buffers();
    if (buffs_count == 1) {
        descriptor->uring_backend->submit_send(
            *descriptor, inner, impl->native_fd(), buffs_ptr->iov_base,
            buffs_ptr->iov_len, ec);
    }
    else {
        msg.msg_name = nullptr;
        msg.msg_namelen = 0;
        msg.msg_iov = reinterpret_cast<iovec*>(buffs_ptr);
        msg.msg_iovlen = buffs_count;
        descriptor->uring_backend->submit_sendmsg(*descriptor, inner,
                                                  impl->native_fd(), &msg, ec);
    }
}

bool async_socket_impl::write_awaiter::complete(const std::error_code& ec,
                                                int result) noexcept {
    buffers.advance(static_cast<std::size_t>(result));
    transferred += static_cast<std::size_t>(result);
    store(ec);
    if (ec || canceled || buffers.get_count() == 0) {
        return true;
    }
    return false;
}
#endif // RAD_HAS_IO_URING

async_socket_impl::read_awaiter::~read_awaiter() = default;

bool async_socket_impl::read_awaiter::await_suspend(
    std::coroutine_handle<> coro) {
    waiter = coro;
    auto result =
        impl->async_recv(buffers, flags, not_zero, [this]() { return this; });
    if (result.is_pending()) {
        return true;
    }
    transferred = result.transferred();
    store(result.error());
    return false;
}

std::size_t async_socket_impl::read_awaiter::await_resume() const {
    raise("async_read");
    return has_error() ? 0 : transferred;
}

bool async_socket_impl::read_awaiter::perform() noexcept {
    // the lock is held
    assert(!canceled);
    auto result = impl->perform_recv(buffers, flags, not_zero);
    transferred += result.transferred();
    if (result.is_pending()) {
        return false;
    }
    store(result.error());
    return true;
}

void async_socket_impl::read_awaiter::invoke_operation() {
    if (canceled) {
        store(std::make_error_code(std::errc::operation_canceled));
    }
    waiter.resume();
}

any_executor&
async_socket_impl::read_awaiter::associated_executor() const noexcept {
    return impl->any_ex();
}

#ifdef RAD_HAS_IO_URING
void async_socket_impl::read_awaiter::submit(
    io::detail::descriptor_data_inner_t& inner, std::error_code& ec) noexcept {
    std::size_t buffs_count = buffers.get_count();
    io::iovec_buff* buffs_ptr = buffers.get_buffers();
    if (buffs_count == 0) {
        descriptor->uring_backend->submit_recv(
            *descriptor, inner, impl->native_fd(), nullptr, 0, ec);
    }
    else if (buffs_count == 1) {
        descriptor->uring_backend->submit_recv(
            *descriptor, inner, impl->native_fd(), buffs_ptr->iov_base,
            buffs_ptr->iov_len, ec);
    }
    else {
        msg.msg_name = nullptr;
        msg.msg_namelen = 0;
        msg.msg_iov = reinterpret_cast<iovec*>(buffs_ptr);
        msg.msg_iovlen = buffs_count;
        descriptor->uring_backend->submit_recvmsg(*descriptor, inner,
                                                  impl->native_fd(), &msg, ec);
    }
}

bool async_socket_impl::read_awaiter::complete(const std::error_code& ec,
                                               int result) noexcept {
    if (not_zero && result == 0 && !ec) {
        store(io::detail::make_eof_error_code());
        return true;
    }
    buffers.advance(static_cast<std::size_t>(result));
    transferred += static_cast<std::size_t>(result);
    store(ec);
    bool read_all = flags & transfer_flags::wait_all;
    bool is_stream = not_zero;
    if (ec || canceled || !is_stream || buffers.get_count() == 0 ||
        (!read_all && transferred > 0)) {
        return true;
    }
    return false;
}
#endif // RAD_HAS_IO_URING