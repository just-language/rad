#include <rad/io/posix/async_file_impl.h>
#include <fcntl.h>
#include <poll.h>
#include <sys/uio.h>
#include <unistd.h>

using namespace RAD_LIB_NAMESPACE;
using namespace io;
using namespace io::detail;

namespace {

    static_assert(sizeof(iovec) == sizeof(iovec_buff));

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

    using iovec_stack_array = std::array<iovec, 64>;

    template <class Buffers>
    std::pair<iovec*, std::size_t>
    copy_buffers_to_iovecs(const Buffers* buffs, std::size_t buffsn,
                           iovec_stack_array& stack_arr,
                           std::vector<iovec>& heap_arr) {
        if (buffsn == 0) {
            return {};
        }
        iovec* vecs_ptr = stack_arr.data();
        if (buffsn > stack_arr.size()) {
            heap_arr.resize(buffsn);
            vecs_ptr = heap_arr.data();
        }
        std::size_t copied_n = 0;
        for (auto i : range(buffsn)) {
            if (buffs[i].empty()) {
                continue;
            }
            auto& v = vecs_ptr[copied_n++];
            v.iov_base = const_cast<void*>(buffs[i].data());
            v.iov_len = buffs[i].size();
        }
        return {vecs_ptr, copied_n};
    }
} // namespace

std::pair<io_op*, io_op*>
io::detail::descriptor_data_inner_t::perform(bool out, bool in) noexcept {
    io_op *performed_out = nullptr, *performed_in = nullptr;
    if (out) {
        flags |= descriptor_flags::ready_out;
    }
    if (in) {
        flags |= descriptor_flags::ready_in;
    }
    if (in && in_op && in_op->perform()) {
        performed_in = std::exchange(in_op, nullptr);
    }
    if (out && out_op && out_op->perform()) {
        performed_out = std::exchange(out_op, nullptr);
    }
    return std::pair{performed_out, performed_in};
}

std::pair<io_op*, io_op*>
io::detail::descriptor_data_inner_t::cancel() noexcept {
    if (out_op) {
        out_op->canceled = true;
    }
    if (in_op) {
        in_op->canceled = true;
    }
    return std::pair{std::exchange(out_op, nullptr),
                     std::exchange(in_op, nullptr)};
}

void io::detail::descriptor_data::cancel(any_executor& ex) noexcept {
#ifdef RAD_HAS_IO_URING
    if (is_io_uring()) {
        uring_backend->cancel_descriptor(*this);
    }
    else {
#endif // RAD_HAS_IO_URING
        auto [out_op, in_op] = inner->cancel();
        if (!in_op && !out_op) {
            return;
        }
        stack_forward_list<rad::detail::async_op_base> canceled_ops;
        if (in_op) {
            canceled_ops.push_back(*in_op);
        }
        if (out_op) {
            canceled_ops.push_back(*out_op);
        }
        if (!canceled_ops.empty()) {
            ex.post_finished(std::move(canceled_ops));
        }
#ifdef RAD_HAS_IO_URING
    }
#endif // RAD_HAS_IO_URING
}

void io::detail::wait_until_writable(int fd, std::error_code& ec) noexcept {
    ec.clear();
    pollfd pfd;
    pfd.fd = fd;
    pfd.events = POLLOUT;
    pfd.revents = 0;
    int ret = exec_while_eintr(::poll, &pfd, nfds_t{1}, -1);
    if (ret < 0) {
        ec = os::make_system_error(errno);
    }
}

void io::detail::wait_until_readable(int fd, std::error_code& ec) noexcept {
    ec.clear();
    pollfd pfd;
    pfd.fd = fd;
    pfd.events = POLLIN;
    pfd.revents = 0;
    int ret = exec_while_eintr(::poll, &pfd, nfds_t{1}, -1);
    if (ret < 0) {
        ec = os::make_system_error(errno);
    }
}

descriptor_data_ptr
io::detail::attach_fd_to_executor(int fd, io_executor& ex,
                                  std::error_code& ec) noexcept {
    ec.clear();
    os::handle new_handle{fd};
    auto new_data = ex.allocate_descriptor_data(ec);
    if (new_data == nullptr) {
        assert(ec);
        return descriptor_data_ptr{nullptr, ex};
    }

    ex.attach_handle(new_handle, *new_data, ec);
    if (ec) {
        // the caller is responsible for closing fd on failure
        new_handle.release();
        return descriptor_data_ptr{nullptr, ex};
    }
    // make sure executor new_data of new_data points to file's executor
    // the io loop will set the executor to point to itself but the file's
    // executor may be an strand
    new_data.get_deleter().ex = &ex;
    new_data->handle = std::move(new_handle);
    return std::move(new_data);
}

void io::detail::set_fd_non_blocking_close_on_exec(
    int fd, std::error_code& ec) noexcept {
    ec.clear();
    if (fd == -1) {
        ec = std::make_error_code(std::errc::bad_file_descriptor);
        return;
    }
    int ret = ::fcntl(fd, F_SETFD, FD_CLOEXEC);
    if (ret == -1) {
        ec = os::make_system_error(errno);
        return;
    }
    ret = ::fcntl(fd, F_SETFL, O_NONBLOCK);
    if (ret == -1) {
        ec = os::make_system_error(errno);
    }
}

void io::detail::make_error_if_descriptor_is_closed(bool is_open,
                                                    std::error_code& ec,
                                                    const char* msg) {
    if (is_open) {
        return;
    }
    const auto closed_ec = std::make_error_code(std::errc::bad_file_descriptor);
    if (use_exceptions(ec)) {
        throw std::system_error(closed_ec, msg);
    }
    else {
        ec = closed_ec;
    }
}

std::size_t async_file_impl::write(const const_buffer* buffs, std::size_t n,
                                   std::error_code& ec) noexcept {
    ec.clear();
    if (!is_open()) {
        ec = std::make_error_code(std::errc::bad_file_descriptor);
        return 0;
    }

    assert(data_ != nullptr);
    auto inner = data_->inner.synchronize();
    if (inner->out_pending()) {
        ec = std::make_error_code(std::errc::operation_in_progress);
        return 0;
    }

    std::size_t transferred = 0;
    iovec_buffers iovecs{buffs, n};
    bool first_write = true;
    while (1) {
        // try to write
        auto result = perform_async_write(first_write, iovecs);
        first_write = false;
        transferred += result.transferred();

        if (!result.is_pending()) {
            if (result.has_error()) {
                ec = result.error();
                return 0;
            }
            return transferred;
        }

        wait_until_writable(native_handle().get(), ec);
        if (ec) {
            return 0;
        }
    }
}

std::size_t async_file_impl::read(const mutable_buffer* buffs, std::size_t n,
                                  bool read_all, std::error_code& ec) noexcept {
    ec.clear();
    if (!is_open()) {
        ec = std::make_error_code(std::errc::bad_file_descriptor);
        return 0;
    }

    assert(data_ != nullptr);
    auto inner = data_->inner.synchronize();
    if (inner->in_op != nullptr) {
        ec = std::make_error_code(std::errc::operation_in_progress);
        return 0;
    }

    std::size_t transferred = 0;
    iovec_buffers iovecs{buffs, n};
    while (1) {
        // try to read
        auto result = perform_async_read(iovecs, read_all);
        transferred += result.transferred();

        if (!result.is_pending()) {
            if (result.has_error()) {
                ec = result.error();
                return 0;
            }
            return transferred;
        }

        wait_until_readable(native_handle().get(), ec);
        if (ec) {
            return 0;
        }
    }
}

void async_file_impl::cancel() noexcept {
    if (data_ == nullptr) {
        return;
    }
    data_->cancel(get_any_executor());
}

async_result async_file_impl::do_async_write(
    const_buffer& buff, function_view<write_op_base*()> alloc_fn) noexcept {
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
            executor().as_any_executor().post(
                *std::exchange(inner->out_op, nullptr));
            return make_pending(0);
        }
        inner->pending_ops += 1;
        return make_pending(0);
    }
#endif // RAD_HAS_IO_URING

    auto result = perform_async_write(true, buff);
    if (!result.is_pending()) {
        return result;
    }

    // the file isn't writable right now, wait for it to become ready for
    // writing
    inner->set_ready_out(false);
    auto write_op = alloc_fn();
    write_op->transferred = result.transferred();
    inner->set_out(write_op);

    return make_pending(result.transferred());
}

async_result async_file_impl::do_async_write(
    iovec_buffers& buffs, function_view<write_op_base*()> alloc_fn) noexcept {
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
            executor().as_any_executor().post(
                *std::exchange(inner->out_op, nullptr));
            return make_pending(0);
        }
        inner->pending_ops += 1;
        return make_pending(0);
    }
#endif // RAD_HAS_IO_URING

    auto result = perform_async_write(true, buffs);
    if (!result.is_pending()) {
        return result;
    }

    // the file isn't writable right now, wait for it to become ready for
    // writing
    inner->set_ready_out(false);
    auto write_op = alloc_fn();
    write_op->transferred = result.transferred();
    inner->set_out(write_op);

    return make_pending(result.transferred());
}

async_result async_file_impl::do_async_read(
    mutable_buffer& buff, bool read_all,
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
            executor().as_any_executor().post(
                *std::exchange(inner->in_op, nullptr));
            return make_pending(0);
        }
        inner->pending_ops += 1;
        return make_pending(0);
    }
#endif // RAD_HAS_IO_URING

    auto result = perform_async_read(buff, read_all);
    if (!result.is_pending()) {
        return result;
    }

    // the file isn't readable right now, wait for it to become ready for
    // reading
    inner->set_ready_in(false);
    auto read_op = alloc_fn();
    read_op->transferred = result.transferred();
    read_op->read_all = read_all;
    inner->set_in(read_op);

    return make_pending(result.transferred());
}

async_result async_file_impl::do_async_read(
    iovec_buffers& buffs, bool read_all,
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
            executor().as_any_executor().post(
                *std::exchange(inner->in_op, nullptr));
            return make_pending(0);
        }
        inner->pending_ops += 1;
        return make_pending(0);
    }
#endif // RAD_HAS_IO_URING
    auto result = perform_async_read(buffs, read_all);
    if (!result.is_pending()) {
        return result;
    }
    // the file isn't readable right now, wait for it to become ready for
    // writing
    inner->set_ready_in(false);
    auto read_op = alloc_fn();
    read_op->transferred = result.transferred();
    read_op->read_all = read_all;
    inner->set_in(read_op);

    return make_pending(result.transferred());
}

void async_file_impl::set_handle_path(native_handle_type& handle,
                                      native_path_type& path,
                                      std::error_code& ec) noexcept {
    ec.clear();
    int fd = handle.get();
    auto data = attach_fd_to_executor(fd, executor(), ec);
    if (ec || data == nullptr) {
        return;
    }
    // ownership of handle was taken!
    handle.release();
    data_ = std::move(data);
    path_ = std::move(path);
}

async_result async_file_impl::perform_async_write(bool first_time,
                                                  const_buffer& buff) {
    // the lock is held
    std::size_t total_transferred = 0;
    while (1) {
        if (buff.empty() && !first_time) {
            return async_result::success(total_transferred);
        }
        first_time = false;
        ssize_t result = exec_while_eintr(::write, native_handle().get(),
                                          buff.data(), buff.size());
        if (result == -1) {
            result = errno;
            if (result == EWOULDBLOCK || result == EAGAIN) {
                return async_result::pending(total_transferred);
            }
            return async_result::failed(os::make_system_error(result),
                                        total_transferred);
        }
        size_t transferred = static_cast<size_t>(result);
        total_transferred += transferred;
        buff += transferred;
    }
}

async_result async_file_impl::perform_async_write(bool first_time,
                                                  iovec_buffers& buffs) {
    // the lock is held
    std::size_t total_transferred = 0;
    iovec_buff null_iovec{};
    while (1) {
        iovec_buff* buffs_ptr = buffs.get_buffers();
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
            result = exec_while_eintr(::write, native_handle().get(),
                                      buffs_ptr->iov_base, buffs_ptr->iov_len);
        }
        else {
            const iovec* vecs = reinterpret_cast<const iovec*>(buffs_ptr);
            int vecs_count = std::min(static_cast<int>(buffs_count), IOV_MAX);
            result = exec_while_eintr(::writev, native_handle().get(), vecs,
                                      vecs_count);
        }
        if (result == -1) {
            result = errno;
            if (result == EWOULDBLOCK || result == EAGAIN) {
                return async_result::pending(total_transferred);
            }
            return async_result::failed(os::make_system_error(result),
                                        total_transferred);
        }
        size_t transferred = static_cast<size_t>(result);
        total_transferred += transferred;
        buffs.advance(transferred);
    }
}

async_result async_file_impl::perform_async_read(mutable_buffer& buff,
                                                 bool read_all) noexcept {
    // the lock is held
    std::size_t total_transferred = 0;
    const bool is_zero_buffer = buff.empty();
    do {
        ssize_t result = exec_while_eintr(::read, native_handle().get(),
                                          buff.data(), buff.size());
        if (result == -1) {
            result = errno;
            if (result == EWOULDBLOCK || result == EAGAIN) {
                return async_result::pending(total_transferred);
            }
            return async_result::failed(os::make_system_error(result),
                                        total_transferred);
        }
        else if (result == 0) {
            // if read_all is false then total_transferred must be 0
            // since the read will be called only once
            assert(read_all || total_transferred == 0);
            if (!is_zero_buffer) {
                return async_result::failed(make_eof_error_code(),
                                            total_transferred);
            }
            else {
                assert(total_transferred == 0);
                return async_result::success(total_transferred);
            }
        }
        size_t transferred = static_cast<size_t>(result);
        total_transferred += transferred;
        buff += transferred;
    } while (!buff.empty() && read_all);

    return async_result::success(total_transferred);
}

async_result async_file_impl::perform_async_read(iovec_buffers& buffs,
                                                 bool read_all) noexcept {
    // the lock is held
    std::size_t total_transferred = 0;
    while (1) {
        iovec_buff* buffs_ptr = buffs.get_buffers();
        std::size_t buffs_count = buffs.get_count();
        if (buffs_count == 0) {
            // readv can't be called with null buffers pointer!
            return async_result::success(total_transferred);
        }
        ssize_t result = -1;
        if (buffs_count == 1) {
            result = exec_while_eintr(::read, native_handle().get(),
                                      buffs_ptr->iov_base, buffs_ptr->iov_len);
        }
        else {
            const iovec* vecs = reinterpret_cast<const iovec*>(buffs_ptr);
            int vecs_count = std::min(static_cast<int>(buffs_count), IOV_MAX);
            result = exec_while_eintr(::readv, native_handle().get(), vecs,
                                      vecs_count);
        }
        if (result == -1) {
            result = errno;
            if (result == EWOULDBLOCK || result == EAGAIN) {
                return async_result::pending(total_transferred);
            }
            return async_result::failed(os::make_system_error(result),
                                        total_transferred);
        }
        else if (result == 0) {
            // if read_all is false then total_transferred must be 0
            // since the read will be called only once
            assert(read_all || total_transferred == 0);
            // since empty buffers are not used always fail with EOF
            return async_result::failed(make_eof_error_code(),
                                        total_transferred);
        }
        size_t transferred = static_cast<size_t>(result);
        total_transferred += transferred;
        buffs.advance(transferred);
        if (!read_all) {
            return async_result::success(total_transferred);
        }
    }
}

void async_file_impl::init_by_fd(int fd, std::error_code& ec) noexcept {
    assert(data_ == nullptr);
    auto data = attach_fd_to_executor(fd, executor(), ec);
    if (ec || data == nullptr) {
        return;
    }
    data_ = std::move(data);
}

bool async_file_impl::write_awaiter::await_suspend(
    std::coroutine_handle<> coro) {
    waiter = coro;
    // buff will be advanced by the amount of written bytes
    auto result = impl->do_async_write(
        buff, [this]() { return static_cast<write_op_base*>(this); });
    if (result.is_pending()) {
        return true;
    }
    transferred = result.transferred();
    store(result.error());
    return false;
}

std::size_t async_file_impl::write_awaiter::await_resume() const {
    raise("async_write");
    return has_error() ? 0 : transferred;
}

void async_file_impl::write_awaiter::invoke_operation() {
    if (canceled) {
        store(std::make_error_code(std::errc::operation_canceled));
    }
    waiter.resume();
}

any_executor&
async_file_impl::write_awaiter::associated_executor() const noexcept {
    return impl->get_any_executor();
}

bool async_file_impl::write_awaiter::perform() noexcept {
    // the lock is held
    assert(!canceled);
    // buff will be advanced by the amount of written bytes
    auto result = impl->perform_async_write(false, buff);
    transferred += result.transferred();
    if (result.is_pending()) {
        return false;
    }
    store(result.error());
    return true;
}

#ifdef RAD_HAS_IO_URING
void async_file_impl::write_awaiter::submit(descriptor_data_inner_t& inner,
                                            std::error_code& ec) noexcept {
    iovec_buff io_buff;
    io_buff.iov_base = const_cast<void*>(buff.data());
    io_buff.iov_len = buff.size();
    descriptor->uring_backend->submit_writev(
        *descriptor, inner, impl->native_handle().get(), &io_buff, 1, -1, ec);
}

bool async_file_impl::write_awaiter::complete(const std::error_code& ec,
                                              int result) noexcept {
    buff += static_cast<std::size_t>(result);
    transferred += static_cast<std::size_t>(result);
    store(ec);
    if (ec || canceled || buff.empty()) {
        return true;
    }
    return false;
}
#endif // RAD_HAS_IO_URING

bool async_file_impl::read_awaiter::await_suspend(
    std::coroutine_handle<> coro) {
    waiter = coro;
    // buff will be advanced by the amount of read bytes
    auto result = impl->do_async_read(
        buff, read_all, [this]() { return static_cast<read_op_base*>(this); });
    if (result.is_pending()) {
        return true;
    }
    transferred = result.transferred();
    store(result.error());
    return false;
}

std::size_t async_file_impl::read_awaiter::await_resume() const {
    raise("async_read");
    return has_error() ? 0 : transferred;
}

void async_file_impl::read_awaiter::invoke_operation() {
    if (canceled) {
        store(std::make_error_code(std::errc::operation_canceled));
    }
    waiter.resume();
}

any_executor&
async_file_impl::read_awaiter::associated_executor() const noexcept {
    return impl->get_any_executor();
}

bool async_file_impl::read_awaiter::perform() noexcept {
    // the lock is held
    assert(!canceled);
    // buff will be advanced by the amount of read bytes
    auto result = impl->perform_async_read(buff, read_all);
    transferred += result.transferred();
    if (result.is_pending()) {
        return false;
    }
    store(result.error());
    return true;
}

#ifdef RAD_HAS_IO_URING
void async_file_impl::read_awaiter::submit(descriptor_data_inner_t& inner,
                                           std::error_code& ec) noexcept {
    iovec_buff io_buff;
    io_buff.iov_base = buff.data();
    io_buff.iov_len = buff.size();
    descriptor->uring_backend->submit_readv(
        *descriptor, inner, impl->native_handle().get(), &io_buff, 1, -1, ec);
}

bool async_file_impl::read_awaiter::complete(const std::error_code& ec,
                                             int result) noexcept {
    if (!buff.empty() && result == 0 && !ec) {
        store(io::detail::make_eof_error_code());
        return true;
    }
    buff += static_cast<std::size_t>(result);
    transferred += static_cast<std::size_t>(result);
    store(ec);
    if (ec || canceled || buff.empty() || (!read_all && transferred > 0)) {
        return true;
    }
    return false;
}
#endif // RAD_HAS_IO_URING

bool async_file_impl::writev_awaiter::await_suspend(
    std::coroutine_handle<> coro) {
    waiter = coro;
    auto result = impl->do_async_write(
        buffers, [this]() { return static_cast<write_op_base*>(this); });
    if (result.is_pending()) {
        return true;
    }
    transferred = result.transferred();
    store(result.error());
    return false;
}

std::size_t async_file_impl::writev_awaiter::await_resume() const {
    raise("async_write");
    return has_error() ? 0 : transferred;
}

void async_file_impl::writev_awaiter::invoke_operation() {
    if (canceled) {
        store(std::make_error_code(std::errc::operation_canceled));
    }
    waiter.resume();
}

any_executor&
async_file_impl::writev_awaiter::associated_executor() const noexcept {
    return impl->get_any_executor();
}

bool async_file_impl::writev_awaiter::perform() noexcept {
    // the lock is held
    assert(!canceled);
    auto result = impl->perform_async_write(false, buffers);
    transferred += result.transferred();
    if (result.is_pending()) {
        return false;
    }
    store(result.error());
    return true;
}

#ifdef RAD_HAS_IO_URING
void async_file_impl::writev_awaiter::submit(descriptor_data_inner_t& inner,
                                             std::error_code& ec) noexcept {
    descriptor->uring_backend->submit_writev(
        *descriptor, inner, impl->native_handle().get(), buffers.get_buffers(),
        buffers.get_count(), -1, ec);
}

bool async_file_impl::writev_awaiter::complete(const std::error_code& ec,
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

bool async_file_impl::readv_awaiter::await_suspend(
    std::coroutine_handle<> coro) {
    waiter = coro;
    auto result = impl->do_async_read(buffers, read_all, [this]() {
        return static_cast<read_op_base*>(this);
    });
    if (result.is_pending()) {
        return true;
    }
    transferred = result.transferred();
    store(result.error());
    return false;
}

std::size_t async_file_impl::readv_awaiter::await_resume() const {
    raise("async_read");
    return has_error() ? 0 : transferred;
}

void async_file_impl::readv_awaiter::invoke_operation() {
    if (canceled) {
        store(std::make_error_code(std::errc::operation_canceled));
    }
    waiter.resume();
}

any_executor&
async_file_impl::readv_awaiter::associated_executor() const noexcept {
    return impl->get_any_executor();
}

bool async_file_impl::readv_awaiter::perform() noexcept {
    // the lock is held
    assert(!canceled);
    auto result = impl->perform_async_read(buffers, read_all);
    transferred += result.transferred();
    if (result.is_pending()) {
        return false;
    }
    store(result.error());
    return true;
}

#ifdef RAD_HAS_IO_URING
void async_file_impl::readv_awaiter::submit(descriptor_data_inner_t& inner,
                                            std::error_code& ec) noexcept {
    descriptor->uring_backend->submit_readv(
        *descriptor, inner, impl->native_handle().get(), buffers.get_buffers(),
        buffers.get_count(), -1, ec);
}

bool async_file_impl::readv_awaiter::complete(const std::error_code& ec,
                                              int result) noexcept {
    if (!buffers.empty() && result == 0 && !ec) {
        store(io::detail::make_eof_error_code());
        return true;
    }
    buffers.advance(static_cast<std::size_t>(result));
    transferred += static_cast<std::size_t>(result);
    store(ec);
    if (ec || canceled || buffers.get_count() == 0 ||
        (!read_all && transferred > 0)) {
        return true;
    }
    return false;
}
#endif // RAD_HAS_IO_URING