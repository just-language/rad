#include <rad/io/linux/io_uring_loop.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/socket.h>
#include <sys/types.h>

using namespace RAD_LIB_NAMESPACE;
using namespace io;

namespace {
    template <class Fn, class... Args>
    std::invoke_result_t<Fn, Args...> exec_while_eintr(Fn fn, Args&&... args) {
        while (1) {
            auto result = fn(std::forward<Args>(args)...);
            // io_uring functions returns -errno on error!
            if (result == -EINTR) {
                continue;
            }
            return result;
        }
    }

    constexpr unsigned int io_uring_sq_size = 16 * 1024;
    constexpr std::size_t submit_sqes_batch_size = 128;

    const iovec_buff null_iovec{};

    struct iovec* get_null_iovec() {
        return reinterpret_cast<struct iovec*>(
            const_cast<iovec_buff*>(&null_iovec));
    }

    bool valid_submit_handle(int fd, std::error_code& ec) noexcept {
        assert(fd >= 0);
        if (fd < 0) {
            ec = std::make_error_code(std::errc::bad_file_descriptor);
            return false;
        }
        return true;
    }
} // namespace

io_uring_loop::~io_uring_loop() {
    close();
}

void io_uring_loop::init(epoll& e, bool standalone,
                         std::error_code& ec) noexcept {
    ec.clear();
    auto ring_lock = std::unique_lock{io_uring_lock_};
    if (ring_.ring_fd != -1) {
        if (event_handle_) {
            ::io_uring_unregister_eventfd(&ring_);
            event_handle_.reset();
        }
        ::io_uring_queue_exit(&ring_);
    }
    ring_.ring_fd = -1;

    int ret = ::io_uring_queue_init(io_uring_sq_size, &ring_, 0);
    if (ret < 0) {
        ring_.ring_fd = -1;
        ec = os::make_system_error(-ret);
        return;
    }
    if (!supports_no_drop()) {
        ec = std::make_error_code(std::errc::not_supported);
        ::io_uring_queue_exit(&ring_);
        ring_.ring_fd = -1;
        return;
    }

    if (!standalone) {
        event_handle_.reset(::eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK));
        if (!event_handle_) {
            ec = os::make_system_error(errno);
            ::io_uring_queue_exit(&ring_);
            ring_.ring_fd = -1;
            return;
        }
        ret = ::io_uring_register_eventfd(&ring_, event_handle_.get());
        if (ret < 0) {
            ec = os::make_system_error(-ret);
            ::io_uring_queue_exit(&ring_);
            ring_.ring_fd = -1;
            event_handle_.reset();
            return;
        }

        epoll_event ev{};
        ev.events = EPOLLIN;
        ev.data.ptr = this;
        if (::epoll_ctl(e.native_handle().get(), EPOLL_CTL_ADD,
                        event_handle_.get(), &ev) != 0) {
            ec = os::make_system_error(errno);
            ::io_uring_queue_exit(&ring_);
            ring_.ring_fd = -1;
            event_handle_.reset();
            return;
        }
    }
}

void io_uring_loop::close() noexcept {
    auto ring_lock = std::unique_lock{io_uring_lock_};
    if (ring_.ring_fd != -1) {
        if (event_handle_) {
            ::io_uring_unregister_eventfd(&ring_);
            event_handle_.reset();
        }
        ::io_uring_queue_exit(&ring_);
    }
    ring_.ring_fd = -1;
}

void io_uring_loop::attach_descriptor(descriptor_data& data) noexcept {
    descriptors_->push_back(data);
}

void io_uring_loop::delete_descriptor_data(
    descriptor_data* p, stack_forward_list<op_t>& ops) noexcept {
    if (p == nullptr) {
        return;
    }
    descriptors_->erase(*p);
    bool delete_now = false;
    {
        auto inner = p->inner.synchronize();
        assert(!inner->pending_delete);
        cancel_descriptor_ops(*p, *inner, ops);
        if (inner->pending_ops == 0) {
            delete_now = true;
        }
        else {
            inner->pending_delete = true;
        }
    }
    if (delete_now) {
        delete p;
    }
}

void io_uring_loop::cancel_descriptor(descriptor_data& d) noexcept {
    auto inner = d.inner.synchronize();
    cancel_descriptor_ops(d, *inner, completions_);
}

void io_uring_loop::cancel_descriptor_ops(
    descriptor_data& data, descriptor_data_inner_t& inner,
    stack_forward_list<op_t>& ops) noexcept {
    void *user_data1 = nullptr, *user_data2 = nullptr;
    if (inner.out_op != nullptr) {
        inner.out_op->canceled = true;
        user_data1 = inner.out_op;
    }
    if (inner.in_op != nullptr) {
        inner.in_op->canceled = true;
        user_data2 = inner.in_op;
    }

    if (user_data1 != nullptr || user_data2 != nullptr) {
        cancel_ops_by_user_data(inner, user_data1, user_data2, ops);
    }
}

void io_uring_loop::cancel_ops_by_user_data(
    descriptor_data_inner_t& inner, void* user_data1, void* user_data2,
    stack_forward_list<op_t>& ops) noexcept {
    std::error_code ec;
    uint32_t canceled_n = 0;
    auto ring_lock = std::unique_lock{io_uring_lock_};
    if (user_data1 != nullptr) {
        io_uring_sqe* sqe = try_get_sqe(ops, ec, ring_lock);
        if (sqe == nullptr) {
            return;
        }
        ::io_uring_prep_cancel(sqe, user_data1, 0);
        ::io_uring_sqe_set_data(sqe, nullptr);
        canceled_n += 1;
    }
    if (user_data2 != nullptr) {
        io_uring_sqe* sqe = try_get_sqe(ops, ec, ring_lock);
        if (sqe != nullptr) {
            ::io_uring_prep_cancel(sqe, user_data2, 0);
            ::io_uring_sqe_set_data(sqe, nullptr);
            canceled_n += 1;
        }
    }
    if (canceled_n > 0) {
        pending_sqes_ += canceled_n;
        pending_ops_ += canceled_n;
        int ret = exec_while_eintr(::io_uring_submit, &ring_);
        if (ret > 0) {
            pending_sqes_ = 0;
        }
    }
}

void io_uring_loop::submit_pending_operations(
    stack_forward_list<op_t>& completed, std::error_code& ec) noexcept {
    ec.clear();
    stack_forward_list<detail::io_op> incompleted;
    {
        auto ring_lock = std::unique_lock{io_uring_lock_};
        incompleted = std::move(incompleted_);
    }
    while (1) {
        submit_incompleted_operations(incompleted, completed, false);
        auto ring_lock = std::unique_lock{io_uring_lock_};
        completed.merge_back(std::move(completions_));
        submit_pending_sqes(completed, incompleted_, false, ec, ring_lock);
        if (incompleted_.empty()) {
            break;
        }
        incompleted = std::move(incompleted_);
    }
}

std::size_t io_uring_loop::get_completions(stack_forward_list<op_t>& completed,
                                           std::error_code& ec) noexcept {
    std::size_t n = 0;
    stack_forward_list<detail::io_op> incompleted;
    {
        auto ring_lock = std::unique_lock{io_uring_lock_};
        n += get_completions(completed, incompleted_, false, ec, ring_lock);
        incompleted = std::move(incompleted_);
    }
    if (!incompleted.empty()) {
        submit_incompleted_operations(incompleted, completed, false);
    }
    return n;
}

std::size_t io_uring_loop::get_completions(
    stack_forward_list<op_t>& completed,
    stack_forward_list<detail::io_op>& incompleted, bool cancel_anyway,
    std::error_code& ec, std::unique_lock<executor_mutex>& lock) noexcept {
    ec.clear();
    if (waiting_cqes_now_) {
        return 0;
    }
    if (ring_.ring_fd == -1) {
        ec = std::make_error_code(std::errc::bad_file_descriptor);
        return 0;
    }

    std::size_t consumed = completions_.size();
    completed.merge_back(std::move(completions_));

    while (1) {
        unsigned head;
        struct io_uring_cqe* cqe;
        unsigned int cqes_count = 0;
        io_uring_for_each_cqe(&ring_, head, cqe) {
            cqes_count += 1;
            pending_ops_ -= 1;
            void* cqe_user_data = io_uring_cqe_get_data(cqe);
            if (cqe_user_data == nullptr) {
                continue;
            }
            handle_cqe(cqe_user_data, cqe->res, cancel_anyway, completed,
                       incompleted, lock);
        }
        if (cqes_count == 0) {
            break;
        }
        io_uring_cq_advance(&ring_, cqes_count);
        consumed += cqes_count;
    }
    return consumed;
}

// static
void io_uring_loop::submit_incompleted_operations(
    stack_forward_list<detail::io_op>& incompleted,
    stack_forward_list<op_t>& completed, bool cancel_anyway) noexcept {
    while (!incompleted.empty()) {
        bool to_delete_descriptor = false;
        auto& op = incompleted.pop_front();
        descriptor_data* descriptor = op.descriptor;
        {
            auto inner = descriptor->inner.synchronize();
            assert(inner->pending_ops > 0);
            assert(std::addressof(op) == inner->in_op ||
                   std::addressof(op) == inner->out_op);
            std::error_code ec;
            if (inner->pending_delete || cancel_anyway || op.canceled) {
                op.canceled = true;
                ec = std::make_error_code(std::errc::operation_canceled);
            }
            else {
                op.submit(*inner, ec);
            }
            if (ec) {
                op.complete(ec, 0);
                if (inner->in_op == std::addressof(op)) {
                    inner->in_op = nullptr;
                }
                else {
                    assert(inner->out_op == std::addressof(op));
                    inner->out_op = nullptr;
                }
                completed.push_back(op);
                inner->pending_ops -= 1;
            }
            to_delete_descriptor =
                inner->pending_delete && inner->pending_ops == 0;
        }
        if (to_delete_descriptor) {
            delete descriptor;
        }
    }
}

void io_uring_loop::reset_eventfd() noexcept {
    uint64_t val;
    if (::read(event_handle_.get(), &val, sizeof(val)) == -1) {
        assert(errno == EAGAIN);
    }
}

void io_uring_loop::interrupt() noexcept {
    auto ring_lock = std::unique_lock{io_uring_lock_};
    std::error_code ec;
    io_uring_sqe* sqe = try_get_sqe(completions_, ec, ring_lock);
    if (sqe == nullptr) {
        return;
    }
    pending_sqes_ += 1;
    pending_ops_ += 1;
    ::io_uring_prep_nop(sqe);
    ::io_uring_sqe_set_data(sqe, nullptr);
    const int ret = exec_while_eintr(::io_uring_submit, &ring_);
    if (ret > 0) {
        pending_sqes_ = 0;
    }
}

void io_uring_loop::submit_and_get(stack_forward_list<op_t>& completed,
                                   std::chrono::nanoseconds timeout,
                                   std::error_code& ec) noexcept {
    using namespace std::chrono;
    ec.clear();
    assert(completed.empty());
    timeout = std::max(timeout, nanoseconds{0});
    auto wait_secs = duration_cast<seconds>(timeout);
    timeout -= wait_secs;
    struct __kernel_timespec wait_time{};
    wait_time.tv_sec = wait_secs.count();
    wait_time.tv_nsec = timeout.count();
    struct __kernel_timespec* wait_time_ptr = &wait_time;

    stack_forward_list<detail::io_op> incompleted;
    {
        auto ring_lock = std::unique_lock{io_uring_lock_};
        incompleted = std::move(incompleted_);
    }
    while (1) {
        submit_incompleted_operations(incompleted, completed, false);
        {
            auto ring_lock = std::unique_lock{io_uring_lock_};
            if (wait_time_ptr != nullptr &&
                ((ring_.features & IORING_FEAT_EXT_ARG) !=
                 IORING_FEAT_EXT_ARG)) {
                auto sqe = try_get_sqe(completed, ec, ring_lock);
                if (sqe == nullptr) {
                    return;
                }
                assert(!ec);
                ec.clear();
                ::io_uring_prep_timeout(sqe, wait_time_ptr, 0, 0);
                ::io_uring_sqe_set_data(sqe, nullptr);
                pending_sqes_ += 1;
                pending_ops_ += 1;
                wait_time_ptr = nullptr;
            }
            const int ret = exec_while_eintr(::io_uring_submit, &ring_);
            if (ret < 0) {
                ec = os::make_system_error(-ret);
                return;
            }
            pending_sqes_ = 0;
            waiting_cqes_now_ = true;
        }
        io_uring_cqe* cqe = nullptr;
        const int ret = completed.empty()
                            ? exec_while_eintr(::io_uring_wait_cqe_timeout,
                                               &ring_, &cqe, wait_time_ptr)
                            : 0;
        auto ring_lock = std::unique_lock{io_uring_lock_};
        waiting_cqes_now_ = false;
        // If IORING_FEAT_NODROP is supported and -EBADR is
        // returned then this means that the completion queue
        // has overflowed.
        // If the timeout passes witout completions -ETIME is returned.
        if (ret < 0 && ret != -ETIME && ret != -EBADR) {
            ec = os::make_system_error(-ret);
            return;
        }
        get_completions(completed, incompleted_, false, ec, ring_lock);
        if (ec || incompleted_.empty()) {
            return;
        }
        // subsequent submit and wait should not wait!
        wait_time.tv_sec = 0;
        wait_time.tv_nsec = 0;
        wait_time_ptr = &wait_time;
        incompleted = std::move(incompleted_);
    }
}

void io_uring_loop::cancel_and_wait(
    stack_forward_list<op_t>& completed) noexcept {
    // cancel all pending operations and submit
    {
        auto descriptors = descriptors_.synchronize();
        for (auto& descriptor : *descriptors) {
            cancel_descriptor(descriptor);
        }
    }
    std::error_code ec;
    // before acquiring the lock cancel any incompleted operation
    if (!incompleted_.empty()) {
        submit_incompleted_operations(incompleted_, completed, true);
        assert(incompleted_.empty());
    }
    auto ring_lock = std::unique_lock{io_uring_lock_};
    submit_pending_sqes(completed, incompleted_, true, ec, ring_lock);
    assert(incompleted_.empty());
    // wait for all operations
    while (pending_ops_ > 0) {
        io_uring_cqe* cqe = nullptr;
        int ret = exec_while_eintr(::io_uring_wait_cqe, &ring_, &cqe);
        if (ret != 0) {
            break;
        }
        pending_ops_ -= 1;
        void* cqe_user_data = io_uring_cqe_get_data(cqe);
        if (cqe_user_data == nullptr) {
            continue;
        }
        stack_forward_list<detail::io_op> incompleted;
        handle_cqe(cqe_user_data, cqe->res, true, completed, incompleted,
                   ring_lock);
        assert(incompleted.empty());
    }
}

void io_uring_loop::submit_writev(detail::descriptor_data& d,
                                  detail::descriptor_data_inner_t& inner,
                                  int fd, const iovec_buff* iovecs,
                                  unsigned n_vecs, uint64_t offset,
                                  std::error_code& ec) noexcept {
    ec.clear();
    if (!valid_submit_handle(fd, ec)) {
        return;
    }
    auto ring_lock = std::unique_lock{io_uring_lock_};
    auto sqe = try_get_sqe(completions_, ec, ring_lock);
    if (sqe == nullptr) {
        return;
    }
    assert(!ec);
    ::io_uring_sqe_set_data(sqe, inner.out_op);
    if (n_vecs == 0) {
        ::io_uring_prep_write(sqe, fd, nullptr, 0, -1);
    }
    else if (n_vecs == 1) {
        ::io_uring_prep_write(sqe, fd, iovecs->iov_base, iovecs->iov_len, -1);
    }
    else {
        ::io_uring_prep_writev(sqe, fd, reinterpret_cast<const iovec*>(iovecs),
                               n_vecs, -1);
    }
    submit_if_batch_exceeded(d, inner, ec, ring_lock);
}

void io_uring_loop::submit_readv(detail::descriptor_data& d,
                                 detail::descriptor_data_inner_t& inner, int fd,
                                 const iovec_buff* iovecs, unsigned n_vecs,
                                 uint64_t offset,
                                 std::error_code& ec) noexcept {
    ec.clear();
    if (!valid_submit_handle(fd, ec)) {
        return;
    }
    auto ring_lock = std::unique_lock{io_uring_lock_};
    auto sqe = try_get_sqe(completions_, ec, ring_lock);
    if (sqe == nullptr) {
        return;
    }
    ::io_uring_sqe_set_data(sqe, inner.in_op);
    if (n_vecs == 0) {
        ::io_uring_prep_read(sqe, fd, nullptr, 0, -1);
    }
    else if (n_vecs == 1) {
        ::io_uring_prep_read(sqe, fd, iovecs->iov_base, iovecs->iov_len, -1);
    }
    else {
        ::io_uring_prep_readv(sqe, fd, reinterpret_cast<const iovec*>(iovecs),
                              n_vecs, -1);
    }
    submit_if_batch_exceeded(d, inner, ec, ring_lock);
}

void io_uring_loop::submit_connect(descriptor_data& d,
                                   detail::descriptor_data_inner_t& inner,
                                   int fd, const void* addr, size_t addr_len,
                                   std::error_code& ec) noexcept {
    ec.clear();
    if (!valid_submit_handle(fd, ec)) {
        return;
    }
    auto ring_lock = std::unique_lock{io_uring_lock_};
    auto sqe = try_get_sqe(completions_, ec, ring_lock);
    if (sqe == nullptr) {
        return;
    }
    ::io_uring_sqe_set_data(sqe, inner.out_op);
    ::io_uring_prep_connect(sqe, fd, static_cast<const sockaddr*>(addr),
                            addr_len);
    submit_if_batch_exceeded(d, inner, ec, ring_lock);
}

void io_uring_loop::submit_accept(descriptor_data& d,
                                  detail::descriptor_data_inner_t& inner,
                                  int fd, void* addr, socket_len_t* addr_len,
                                  std::error_code& ec) noexcept {
    ec.clear();
    if (!valid_submit_handle(fd, ec)) {
        return;
    }
    auto ring_lock = std::unique_lock{io_uring_lock_};
    auto sqe = try_get_sqe(completions_, ec, ring_lock);
    if (sqe == nullptr) {
        return;
    }
    ::io_uring_sqe_set_data(sqe, inner.in_op);
    static_assert(sizeof(socket_len_t) == sizeof(socklen_t));
    ::io_uring_prep_accept(sqe, fd, static_cast<sockaddr*>(addr),
                           reinterpret_cast<socklen_t*>(addr_len),
                           SOCK_CLOEXEC);
    submit_if_batch_exceeded(d, inner, ec, ring_lock);
}

void io_uring_loop::submit_send(descriptor_data& d,
                                detail::descriptor_data_inner_t& inner, int fd,
                                const void* buff, std::size_t n,
                                std::error_code& ec) noexcept {
    ec.clear();
    if (!valid_submit_handle(fd, ec)) {
        return;
    }
    auto ring_lock = std::unique_lock{io_uring_lock_};
    auto sqe = try_get_sqe(completions_, ec, ring_lock);
    if (sqe == nullptr) {
        return;
    }
    ::io_uring_sqe_set_data(sqe, inner.out_op);
    ::io_uring_prep_send(sqe, fd, buff, n, MSG_NOSIGNAL);
    submit_if_batch_exceeded(d, inner, ec, ring_lock);
}

void io_uring_loop::submit_sendmsg(descriptor_data& d,
                                   detail::descriptor_data_inner_t& inner,
                                   int fd, msghdr* msg,
                                   std::error_code& ec) noexcept {
    ec.clear();
    if (!valid_submit_handle(fd, ec)) {
        return;
    }
    auto ring_lock = std::unique_lock{io_uring_lock_};
    auto sqe = try_get_sqe(completions_, ec, ring_lock);
    if (sqe == nullptr) {
        return;
    }
    if (msg->msg_iov == nullptr && msg->msg_iovlen == 0) {
        msg->msg_iov = get_null_iovec();
    }
    else if (msg->msg_iovlen > IOV_MAX) {
        msg->msg_iovlen = IOV_MAX;
    }
    ::io_uring_sqe_set_data(sqe, inner.out_op);
    ::io_uring_prep_sendmsg(sqe, fd, msg, MSG_NOSIGNAL);
    submit_if_batch_exceeded(d, inner, ec, ring_lock);
}

void io_uring_loop::submit_sendto(descriptor_data& d,
                                  detail::descriptor_data_inner_t& inner,
                                  int fd, const void* buff, std::size_t n,
                                  const void* addr, socket_len_t addrlen,
                                  std::error_code& ec) noexcept {
    ec.clear();
    if (!valid_submit_handle(fd, ec)) {
        return;
    }
    auto ring_lock = std::unique_lock{io_uring_lock_};
    auto sqe = try_get_sqe(completions_, ec, ring_lock);
    if (sqe == nullptr) {
        return;
    }
    ::io_uring_sqe_set_data(sqe, inner.out_op);
    ::io_uring_prep_sendto(sqe, fd, buff, n, MSG_NOSIGNAL,
                           static_cast<const sockaddr*>(addr), addrlen);
    submit_if_batch_exceeded(d, inner, ec, ring_lock);
}

void io_uring_loop::submit_recv(descriptor_data& d,
                                detail::descriptor_data_inner_t& inner, int fd,
                                void* buff, std::size_t n,
                                std::error_code& ec) noexcept {
    ec.clear();
    if (!valid_submit_handle(fd, ec)) {
        return;
    }
    auto ring_lock = std::unique_lock{io_uring_lock_};
    auto sqe = try_get_sqe(completions_, ec, ring_lock);
    if (sqe == nullptr) {
        return;
    }
    ::io_uring_sqe_set_data(sqe, inner.in_op);
    ::io_uring_prep_recv(sqe, fd, buff, n, 0);
    submit_if_batch_exceeded(d, inner, ec, ring_lock);
}

void io_uring_loop::submit_recvmsg(descriptor_data& d,
                                   detail::descriptor_data_inner_t& inner,
                                   int fd, msghdr* msg,
                                   std::error_code& ec) noexcept {
    ec.clear();
    if (!valid_submit_handle(fd, ec)) {
        return;
    }
    auto ring_lock = std::unique_lock{io_uring_lock_};
    auto sqe = try_get_sqe(completions_, ec, ring_lock);
    if (sqe == nullptr) {
        return;
    }
    if (msg->msg_iov == nullptr && msg->msg_iovlen == 0) {
        msg->msg_iov = get_null_iovec();
    }
    else if (msg->msg_iovlen > IOV_MAX) {
        msg->msg_iovlen = IOV_MAX;
    }
    ::io_uring_sqe_set_data(sqe, inner.in_op);
    ::io_uring_prep_recvmsg(sqe, fd, msg, 0);
    submit_if_batch_exceeded(d, inner, ec, ring_lock);
}

void io_uring_loop::submit_if_batch_exceeded(
    descriptor_data& d, detail::descriptor_data_inner_t& inner,
    std::error_code& ec, std::unique_lock<executor_mutex>& lock) noexcept {
    assert(current_descriptor_ == nullptr && current_inner_ == nullptr);
    current_descriptor_ = &d;
    current_inner_ = &inner;
    auto on_exit = scope_exit([this] {
        current_descriptor_ = nullptr;
        current_inner_ = nullptr;
    });
    ec.clear();
    pending_sqes_ += 1;
    pending_ops_ += 1;
    if (pending_sqes_ >= submit_sqes_batch_size || waiting_cqes_now_) {
        submit_pending_sqes(completions_, incompleted_, false, ec, lock);
    }
}

void io_uring_loop::submit_pending_sqes(
    stack_forward_list<op_t>& completed,
    stack_forward_list<detail::io_op>& incompleted, bool cancel_anyway,
    std::error_code& ec, std::unique_lock<executor_mutex>& lock) noexcept {
    // try to drain the CQ first to make sure it does not overflow?
    ec.clear();
    // get_completions(completed, incompleted, cancel_anyway, ec, lock);
    if (ec) {
        return;
    }

    while (1) {
        int ret = exec_while_eintr(::io_uring_submit, &ring_);
        if (ret < 0) {
            ret = -ret;
            // if IORING_FEAT_NODROP is supported and -EBUSY is
            // returned then this means that the completion queue
            // has overflowed
            if (ret == EBUSY) {
                const std::size_t n = get_completions(completed, incompleted,
                                                      cancel_anyway, ec, lock);
                if (!ec && n > 0) {
                    continue;
                }
            }
            ec = os::make_system_error(ret);
            return;
        }
        pending_sqes_ = 0;
        break;
    }
}

io_uring_sqe*
io_uring_loop::try_get_sqe(stack_forward_list<op_t>& ops, std::error_code& ec,
                           std::unique_lock<executor_mutex>& lock) noexcept {
    ec.clear();
    if (ring_.ring_fd == -1) {
        ec = std::make_error_code(std::errc::bad_file_descriptor);
        return nullptr;
    }

    while (1) {
        io_uring_sqe* sqe = ::io_uring_get_sqe(&ring_);
        if (sqe != nullptr) {
            return sqe;
        }
        // the submission queue is full now, try to submit then retry
        submit_pending_sqes(ops, incompleted_, false, ec, lock);
        if (ec) {
            break;
        }
    }

    return nullptr;
}

void io_uring_loop::handle_cqe(
    void* cqe_user_data, int res, bool cancel_anyway,
    stack_forward_list<op_t>& completed,
    stack_forward_list<detail::io_op>& incompleted,
    std::unique_lock<executor_mutex>& lock) noexcept {
    detail::io_op* cqe_op = static_cast<detail::io_op*>(cqe_user_data);
    descriptor_data* descriptor = cqe_op->descriptor;

    bool to_delete_descriptor = false;

    {
        std::unique_lock<executor_mutex> inner_lock;
        detail::descriptor_data_inner_t* inner_ptr = nullptr;
        if (current_descriptor_ == descriptor) {
            inner_ptr = current_inner_;
        }
        else {
            auto [lock, inner] = descriptor->inner.unique_lock();
            inner_lock = std::move(lock);
            inner_ptr = std::addressof(inner);
        }

        auto inner = inner_ptr;
        assert(inner->pending_ops >= 1);
        const bool is_out = cqe_op == inner->out_op;
        const bool is_in = cqe_op == inner->in_op;
        assert(is_out || is_in);
        if (!is_out && !is_in) {
            return;
        }
        detail::io_op*& op = is_out ? inner->out_op : inner->in_op;
        assert(op != nullptr);
        if (op != nullptr) {
            std::error_code op_ec;
            bool was_canceled = op->canceled || inner->pending_delete ||
                                res == -ECANCELED || cancel_anyway;
            op->canceled = was_canceled;
            if (res < 0 && !op->canceled) {
                op_ec = os::make_system_error(-res);
            }
            else if (op->canceled) {
                op_ec = std::make_error_code(std::errc::operation_canceled);
            }

            bool op_completed = op->complete(op_ec, std::max(0, res));
            const bool op_finished =
                op_completed || inner->pending_delete || was_canceled;
            if (op_finished) {
                inner->pending_ops -= 1;
                completed.push_back(*std::exchange(op, nullptr));
            }
            else {
                to_delete_descriptor = false;
                // incompleted.push_back(*std::exchange(op, nullptr));
                incompleted.push_back(op);
            }
            to_delete_descriptor =
                inner->pending_delete && inner->pending_ops == 0;
        }
    }

    if (to_delete_descriptor) {
        delete descriptor;
    }
}