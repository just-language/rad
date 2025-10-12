#include <netinet/in.h>
#include <rad/async/io_loop.h>
#include <rad/io/linux/async_waitable_timer.h>
#include <rad/io/posix/sync_waitable_timer.h>
#ifdef __linux__
#include <sys/epoll.h>
#include <sys/eventfd.h>
#else
#include <sys/event.h>
#endif // __linux__
#include <sys/syscall.h>
#include <sys/timerfd.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>

using namespace RAD_LIB_NAMESPACE;
using RAD_LIB_NAMESPACE::detail::async_waitable_timer;
using RAD_LIB_NAMESPACE::detail::sync_waitable_timer;
using namespace io;
using io_impl = io::detail::io_loop_impl;

namespace RAD_LIB_NAMESPACE::io::detail {
    struct eof_error_category : public std::error_category {
        const char* name() const noexcept override {
            return "system";
        }

        std::string message([[maybe_unused]] int ec) const override {
            return "Reached the end of the file";
        }
    };

    const eof_error_category eof_error_category_inst;

    const std::error_category& eof_category() noexcept {
        return eof_error_category_inst;
    }
} // namespace RAD_LIB_NAMESPACE::io::detail

#ifdef __linux__
namespace {
    void set_waitable_timer(int fd,
                            std::chrono::steady_clock::duration timeout) {
        using namespace std::chrono;

        seconds sec = duration_cast<seconds>(timeout);
        nanoseconds nsec = timeout - sec;

        struct itimerspec tspec{};
        tspec.it_value.tv_sec = static_cast<time_t>(sec.count());
        tspec.it_value.tv_nsec = static_cast<long>(nsec.count());

        if (::timerfd_settime(fd, 0, &tspec, nullptr) == -1) {
            throw std::system_error(errno, std::system_category(),
                                    "timerfd_settime");
        }
    }
} // namespace

sync_waitable_timer::sync_waitable_timer() {
    int fd = ::timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC);
    if (fd == -1) {
        throw std::system_error(errno, std::system_category(),
                                "timerfd_create");
    }
    handle_ = os::handle{fd};
}

async_waitable_timer::async_waitable_timer(epoll& ep) {
    int fd = ::timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
    if (fd == -1) {
        throw std::system_error(errno, std::system_category(),
                                "timerfd_create");
    }
    os::handle new_handle{fd};
    std::error_code ec;
    data_.inner->set_timer();
    ep.attach_writable_handle(fd, &data_, ec);
    check_and_throw(ec, "epoll_ctl");
    handle_ = std::move(new_handle);
}

void async_waitable_timer::set(duration timeout) {
    set_waitable_timer(handle_.get(), timeout);
}

void async_waitable_timer::on_child_fork(io::epoll& ep,
                                         std::error_code& ec) noexcept {
    ec.clear();
    handle_.reset();
    os::handle new_handle{
        ::timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC)};
    if (!new_handle) {
        ec = os::make_system_error(errno);
        return;
    }
    ep.attach_writable_handle(new_handle.get(), &data_, ec);
    if (!ec) {
        handle_ = std::move(new_handle);
    }
}

void sync_waitable_timer::wait(duration timeout) {
    if (timeout.count() <= 0) {
        return;
    }
    set_waitable_timer(handle_.get(), timeout);
    uint64_t val = 0;
    static_assert(sizeof(val) == 8, "read buffer size must be 8 bytes");
    auto ret = ::read(handle_.get(), &val, sizeof(val));
    if (ret != sizeof(val)) {
        throw std::system_error(errno, std::system_category(), "read");
    }
}

void sync_waitable_timer::cancel() noexcept {
    if (handle_) {
        set_waitable_timer(handle_.get(), std::chrono::nanoseconds{1});
    }
}

void async_waitable_timer::cancel() noexcept {
    if (handle_) {
        set_waitable_timer(handle_.get(), std::chrono::nanoseconds{1});
    }
}

io_impl::io_loop_impl(uint32_t max_threads) : reactor_fd{max_threads} {
    interrupter.reset(::eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK));
    if (!interrupter) {
        throw std::system_error(os::make_system_error(errno), "eventfd");
    }
    epoll_event ev{};
    ev.events = EPOLLIN;
    ev.data.ptr = nullptr;
    if (::epoll_ctl(reactor_fd.native_handle().get(), EPOLL_CTL_ADD,
                    interrupter.get(), &ev) != 0) {
        throw std::system_error(os::make_system_error(errno), "epoll_ctl");
    }
}

io_impl::~io_loop_impl() {
}

void io_impl::interrupt() noexcept {
    uint64_t val = 1;
    if (::write(interrupter.get(), &val, sizeof(val)) == -1) {
        assert(errno == EAGAIN);
    }
}

void io_impl::reset_interrupter() noexcept {
    uint64_t val;
    if (::read(interrupter.get(), &val, sizeof(val)) == -1) {
        assert(errno == EAGAIN);
    }
}

void io_impl::on_child_fork(std::error_code& ec) noexcept {
    reactor_fd.close();
    interrupter.reset();
    timer_.close();

    epoll new_epoll;
    new_epoll.create(ec);
    if (ec) {
        return;
    }

    os::handle new_interrupter{::eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK)};
    if (!interrupter) {
        ec = os::make_system_error(errno);
        return;
    }

    epoll_event ev{};
    ev.events = EPOLLIN;
    ev.data.ptr = nullptr;
    if (::epoll_ctl(new_epoll.native_handle().get(), EPOLL_CTL_ADD,
                    new_interrupter.get(), &ev) != 0) {
        ec = os::make_system_error(errno);
        return;
    }

    timer_.on_child_fork(new_epoll, ec);
    if (ec) {
        return;
    }

    reactor_fd = std::move(new_epoll);
    interrupter = std::move(new_interrupter);
}
#else
sync_waitable_timer::sync_waitable_timer() : kq_{0} {
}

void sync_waitable_timer::wait(duration timeout) {
    if (timeout.count() <= 0) {
        return;
    }
    std::array<kqueue_event, 2> events;
    auto received = kq_.wait(events, timeout);
    if (!received.empty()) {
        std::error_code ec;
        kq_.disable_event(event_id, ec);
    }
}

void sync_waitable_timer::cancel() noexcept {
    if (!kq_.is_open()) {
        return;
    }
    std::error_code ec;
    kq_.trigger_event(event_id, nullptr, ec);
#ifndef NDEBUG
    if (ec) {
        printf(
            "kevent(EVFILT_USER, NOTE_TRIGGER) failed with errno: %d => %s\n",
            ec.value(), ec.message().c_str());
    }
#endif // !NDEBUG
    assert(!ec);
    std::ignore = ec;
}

io_impl::io_loop_impl(uint32_t max_threads) : reactor_fd{max_threads} {
}

io_impl::~io_loop_impl() {
}

void io_impl::interrupt() noexcept {
    std::error_code ec;
    reactor_fd.trigger_event(interrupter, nullptr, ec);
#ifndef NDEBUG
    if (ec) {
        printf(
            "kevent(EVFILT_USER, NOTE_TRIGGER) failed with errno: %d => %s\n",
            ec.value(), ec.message().c_str());
    }
#endif // !NDEBUG
    assert(!ec);
    std::ignore = ec;
}

void io_impl::reset_interrupter() noexcept {
    std::error_code ec;
    reactor_fd.disable_event(interrupter, ec);
#ifndef NDEBUG
    if (ec) {
        printf("kevent(EVFILT_USER, EV_DISABLE) failed with errno: %d => %s\n",
               ec.value(), ec.message().c_str());
    }
#endif // !NDEBUG
    assert(!ec);
    std::ignore = ec;
}

void io_impl::set_timer_timeout(std::chrono::steady_clock::duration timeout) {
    std::ignore = timeout;
}

void io_impl::on_child_fork(std::error_code& ec) noexcept {
    ec.clear();
    // kqueue is not inherited by fork
    reactor_fd.native_handle().release();
    reactor_fd.create(ec);
}
#endif // __linux__

io_loop::io_loop(uint32_t threads_num_hint, async_mode mode)
    : any_executor(detail::executor_type::io_loop), impl{threads_num_hint} {
    std::ignore = mode;
#ifdef RAD_HAS_IO_URING
    amode_ = mode;
    if (mode != async_mode::disabled) {
        io_uring_loop_ = std::make_unique<io::io_uring_loop>();
        std::error_code ec;
        io_uring_loop_->init(impl.reactor_fd, mode == async_mode::enabled, ec);
        if (ec) {
            io_uring_loop_ = nullptr;
            amode_ = async_mode::disabled;
        }
    }
#endif // RAD_HAS_IO_URING
}

io_loop::~io_loop() {
    delete_destroyed_fds_data();
}

void io_loop::interrupt() noexcept {
    if (!thds_state_->waiting) {
        return;
    }
#ifdef RAD_HAS_IO_URING
    if (io_uring_loop_ != nullptr && amode_ == async_mode::enabled) {
        return io_uring_loop_->interrupt();
    }
#endif // RAD_HAS_IO_URING
    impl.interrupt();
}

void io_loop::post(async_op_t& op) noexcept {
    {
        auto state = thds_state_.synchronize();
        state->work_count += 1;
        state->finished_ops.push_back(op);
        if (state->stopped) {
            return;
        }
    }
    interrupt();
    running_thds_cv_.notify_all();
}

void io_loop::post(stack_forward_list<async_op_t> ops) noexcept {
    {
        auto state = thds_state_.synchronize();
        state->work_count += ops.size();
        state->finished_ops.merge_back(std::move(ops));
        if (state->stopped) {
            return;
        }
    }
    interrupt();
    running_thds_cv_.notify_all();
}

void io_loop::post_finished(stack_forward_list<async_op_t> ready_ops) noexcept {
    if (ready_ops.empty()) {
        return;
    }
    {
        auto state = thds_state_.synchronize();
        state->finished_ops.merge_back(std::move(ready_ops));
        if (state->stopped) {
            return;
        }
    }
    interrupt();
    running_thds_cv_.notify_all();
}

void io_loop::post_finished(async_op_t& op1, async_op_t& op2) noexcept {
    {
        auto state = thds_state_.synchronize();
        state->finished_ops.push_back(op1);
        state->finished_ops.push_back(op2);
        if (state->stopped) {
            return;
        }
    }
    interrupt();
    running_thds_cv_.notify_all();
}

void io_loop::post_finished(async_op_t& op) noexcept {
    {
        auto state = thds_state_.synchronize();
        state->finished_ops.push_back(op);
        if (state->stopped) {
            return;
        }
    }
    interrupt();
    running_thds_cv_.notify_all();
}

void io_loop::add_work(std::size_t n) noexcept {
    thds_state_->work_count += n;
}

void io_loop::consume_work(std::size_t n) noexcept {
    auto state = thds_state_.synchronize();
    assert(state->work_count >= n);
    state->work_count -= n;
}

void io_loop::cancel_work() noexcept {
    {
        auto state = thds_state_.synchronize();
        assert(state->work_count >= 1);
        state->work_count -= 1;
        if (state->work_count > 0 || state->stopped) {
            return;
        }
    }
    interrupt();
    running_thds_cv_.notify_all();
}

std::size_t io_loop::work_count() const noexcept {
    return thds_state_->work_count;
}

void io_loop::stop() noexcept {
    thds_state_->stopped = true;
    if (!thd_ids_list_->empty()) {
        interrupt();
        running_thds_cv_.notify_all();
    }
}

void io_loop::consume_work() noexcept {
    auto state = thds_state_.synchronize();
    assert(state->work_count >= 1);
    state->work_count -= 1;
}

void io_loop::poll_timers() noexcept {
    while (1) {
        stack_forward_list<async_op_t> ready_timers;
        timers_queue_.poll(ready_timers);
        if (!ready_timers.empty()) {
            thds_state_->finished_ops.merge_back(std::move(ready_timers));
        }
        auto least_duration = timers_queue_.least_duration();
        if (least_duration.count() == 0) {
            continue;
        }
#ifdef RAD_HAS_IO_URING
        if (is_async() && amode_ == async_mode::enabled) {
            return;
        }
#endif // RAD_HAS_IO_URING
        impl.set_timer_timeout(least_duration);
        break;
    }
}

void io_loop::consume_finished_operations() {
    auto get_op = [this]() -> async_op_t* {
        auto state = thds_state_.synchronize();
        if (state->finished_ops.empty()) {
            return nullptr;
        }
        return &state->finished_ops.pop_front();
    };

    while (auto op = get_op()) {
        op->invoke(*this);
    }
}

void io_loop::attach_handle(net::socket_handle& sock,
                            io::detail::descriptor_data& data,
                            std::error_code& ec) noexcept {
    using namespace io::detail;
    ec.clear();
#ifdef RAD_HAS_IO_URING
    if (io_uring_loop_ != nullptr) {
        return io_uring_loop_->attach_descriptor(data);
    }
#endif // RAD_HAS_IO_URING
    impl.attach_handle(sock, data, ec);
    if (!ec) {
        descriptors_data_->push_back(data);
    }
}

auto io_loop::allocate_descriptor_data(std::error_code& ec) noexcept
    -> descriptor_data_ptr {
    ec.clear();
#ifdef RAD_HAS_IO_URING
    auto* ptr =
        new (std::nothrow) io::detail::descriptor_data{io_uring_loop_.get()};
#else
    auto* ptr = new (std::nothrow) io::detail::descriptor_data{};
#endif // RAD_HAS_IO_URING
    if (ptr == nullptr) {
        ec = std::make_error_code(std::errc::not_enough_memory);
    }
    return descriptor_data_ptr{ptr, io::detail::descriptor_data_deleter{*this}};
}

void io_loop::delete_descriptor_data(io::detail::descriptor_data* p) noexcept {
    assert(p != nullptr);
    if (p == nullptr) {
        return;
    }

#ifdef RAD_HAS_IO_URING
    if (io_uring_loop_ != nullptr) {
        // delegate deletion to the io_uring backend
        stack_forward_list<detail::async_op_base> canceled_ops;
        io_uring_loop_->delete_descriptor_data(p, canceled_ops);
        post_finished(std::move(canceled_ops));
        return;
    }
#endif // RAD_HAS_IO_URING

    // close the handle to remove it from the epoll fd set
    p->handle.reset();
    // remove the descriptor from the descriptors list
    descriptors_data_->erase(*p);
    // cancel any pending operations
    auto [out_op, in_op] = p->inner->cancel();
    stack_forward_list<detail::async_op_base> canceled_ops;
    if (in_op) {
        canceled_ops.push_back(*in_op);
    }
    if (out_op) {
        canceled_ops.push_back(*out_op);
    }
    post_finished(std::move(canceled_ops));
    // schedule the descriptor data for deletion because it may be
    // referenced by events returned from epoll or kqueue now
    pending_delete_fds_->push_back(p);
}

void io_loop::notify_fork(fork_event event, std::error_code& ec) noexcept {
    ec.clear();
#ifdef RAD_HAS_IO_URING
    if (event == fork_event::prepare && io_uring_loop_ != nullptr) {
        stack_forward_list<detail::async_op_base> completed_ops;
        io_uring_loop_->cancel_and_wait(completed_ops);
        post_finished(std::move(completed_ops));
        return;
    }
#endif // RAD_HAS_IO_URING
    if (event != fork_event::child) {
        return;
    }
    // recreate the epoll, eventfd and timer
    // or recreate kqueue
    impl.on_child_fork(ec);
    if (ec) {
        return;
    }
#ifdef RAD_HAS_IO_URING
    // recreate the io_uring instance
    if (io_uring_loop_ != nullptr) {
        io_uring_loop_->close();
        io_uring_loop_->init(impl.reactor_fd, amode_ == async_mode::enabled,
                             ec);
        if (ec) {
            io_uring_loop_ = nullptr;
        }
    }
#endif // RAD_HAS_IO_URING
       // attach all descriptors to the new epoll instance
    auto descriptors = descriptors_data_.synchronize();
    for (auto& descriptor : *descriptors) {
#ifdef RAD_HAS_IO_URING
        if (io_uring_loop_ != nullptr) {
            io_uring_loop_->attach_descriptor(descriptor);
            continue;
        }
#endif // RAD_HAS_IO_URING
        impl.attach_handle(descriptor.handle, descriptor, ec);
        if (ec) {
            return;
        }
    }
    // restart the timer
    poll_timers();
}

void io_loop::delete_destroyed_fds_data() noexcept {
    // wait for other threads if they are performing any io operations
    std::lock_guard<shared_mutex> lock{perform_lock_};
    while (1) {
        auto pending = pending_delete_fds_.move();
        if (pending.empty()) {
            return;
        }
        while (!pending.empty()) {
            auto dp = &pending.pop_front();
            delete dp;
        }
    }
}

void io_loop::run(std::error_code& ec) {
    ec.clear();
    loop_thread_id this_id{this_thread::get_id()};
    thd_ids_list_->push_back(this_id);
    auto on_exit = scope_exit([&] { thd_ids_list_->erase(&this_id); });
    do_run(ec);
}

void io_loop::restart() noexcept {
    thds_state_->stopped = false;
}

void io_loop::do_run(std::error_code& ec) {
    bool keep_running = true;
    while (keep_running) {
        // all threads will share the finished operations here so if one
        // operation blocks on one thread, another thread may proceed to
        // epoll_wait, kevent or io_uring_enter.
        consume_finished_operations();

        bool expected_value = false;
        const bool run_as_main_thd =
            is_there_main_thread_.compare_exchange_strong(expected_value, true);

        auto on_exit = scope_exit([&] {
            if (run_as_main_thd) {
                is_there_main_thread_ = false;
            }
        });

        if (run_as_main_thd) {
            // the main thread will only get results, delete
            // fds data and perform io operations
            auto on_main_exit =
                scope_exit([this] { running_thds_cv_.notify_all(); });
            keep_running = run_main_once(ec);
            if (keep_running) {
                on_main_exit.release();
            }
        }
        else {
            // the other threads will only perform io operations
            auto on_thd_exit = scope_exit([this] { interrupt(); });
            keep_running = run_thd_once();
            if (keep_running) {
                on_thd_exit.release();
            }
        }
    }
}

bool io_loop::run_main_once(std::error_code& ec) {
    thds_state_->waiting = true;
    auto on_exit = scope_exit([this] { thds_state_->waiting = false; });
    poll_timers();
    if (!is_async()) {
        delete_destroyed_fds_data();
    }

    // if stopped or there is no more pending work
    if (thds_state_->should_stop()) {
        thds_state_->stopped = true;
        return false;
    }
#ifdef RAD_HAS_IO_URING
    if (io_uring_loop_ != nullptr) {
        stack_forward_list<detail::async_op_base> completed_ops;
        auto wait_timeout = timers_queue_.least_duration();
        if (!thds_state_->finished_ops.empty()) {
            wait_timeout = {};
        }
        if (amode_ == async_mode::enabled) {
            io_uring_loop_->submit_and_get(completed_ops, wait_timeout, ec);
            poll_timers();
        }
        else {
            io_uring_loop_->submit_pending_operations(completed_ops, ec);
        }
        if (!completed_ops.empty()) {
            auto state = thds_state_.synchronize();
            state->finished_ops.merge_back(std::move(completed_ops));
            if (state->finished_ops.size() < thd_ids_list_->size()) {
                for (auto i : range(state->finished_ops.size())) {
                    running_thds_cv_.notify_one();
                }
            }
            else {
                running_thds_cv_.notify_all();
            }
            return true;
        }
        if (ec) {
            return false;
        }
        if (amode_ == async_mode::enabled) {
            return true;
        }
    }
#endif // RAD_HAS_IO_URING
#ifdef __linux__
    std::chrono::duration<int, std::milli> wait_timeout{-1};
#else
    auto wait_timeout = timers_queue_.least_duration();
#endif // __linux__
    constexpr size_t max_results_count = 128;
    reactor_evs_.resize(max_results_count);
    if (!thds_state_->finished_ops.empty()) {
        wait_timeout = {};
    }

    auto pending =
        impl.reactor_fd.wait(std::span{reactor_evs_}, wait_timeout, ec);
    thds_state_->waiting = false;
#ifndef __linux__
    poll_timers();
#endif // !__linux__
    // if reactor wait resulted in an error or the loop is to be stopped
    // return
    if (ec || thds_state_->should_stop()) {
        thds_state_->stopped = true;
        return false;
    }

    // increase the count of currently executing threads
    ++executing_thds_count_;
    auto on_exit2 = scope_exit([this] { --executing_thds_count_; });

    // no operations currently now, why was the wakeup?
    if (pending.empty() && thds_state_->finished_ops.empty()) {
        return true;
    }

    bool notified_other_thds = false;
    // if there is more than one pending io operation notify other threads
    if (pending.size() > 1) {
        thds_state_->pending_io_ops.set_objects(pending);
        running_thds_cv_.notify_all();
        notified_other_thds = true;
        perform_pending_io_ops();
        // pending_ops_ must now be empty and not used by other threads
    }
    else if (!pending.empty()) {
        // one io operation, perform it here, don't insert into pending_io_ops
        auto& op = pending.front();
        do_perform(op.get_ident(), op.get_data(), op.get_events());
    }

    if (!notified_other_thds && thds_state_->finished_ops.size() > 0) {
        running_thds_cv_.notify_all();
    }

    return true;
}

bool io_loop::run_thd_once() {
    // wait for notification from the main thread or exit signal
    {
        auto [lock, state] = thds_state_.std_unique_lock();
        running_thds_cv_.wait(lock, [&] { return state.should_wakeup(); });
        if (state.should_stop()) {
            return false;
        }
    }
    // perform io operations
#ifdef RAD_HAS_IO_URING
    if (io_uring_loop_ == nullptr || amode_ != async_mode::enabled)
#endif // RAD_HAS_IO_URING
    {
        std::shared_lock<shared_mutex> lock{perform_lock_};
        perform_pending_io_ops();
    }

    return true;
}

void io_loop::perform_pending_io_ops() noexcept {
    while (1) {
        auto* op = thds_state_->pending_io_ops.try_pop();
        if (!op) {
            break;
        }
        do_perform(op->get_ident(), op->get_data(), op->get_events());
    }
}

void io_loop::do_perform_reactor_in_out_ops(void* ptr, bool ready_in,
                                            bool ready_out) noexcept {
    auto data = static_cast<io::detail::descriptor_data*>(ptr);
    auto inner = data->inner.synchronize();
    // the fd lock is held here, so no callbacks should be called directly

#ifdef __linux__
    // timer is edge triggered so it won't post any further operations until
    // it is rearmed and expired
    if (inner->is_timer()) {
        poll_timers();
        return;
    }
#endif // __linux__

    auto [out_op, in_op] = inner->perform(ready_out, ready_in);
    if (out_op || in_op) {
        auto state = thds_state_.synchronize();
        if (in_op) {
            state->finished_ops.push_back(*in_op);
        }
        if (out_op) {
            state->finished_ops.push_back(*out_op);
        }
    }
}

#ifdef __linux__
void io_loop::do_perform(uintptr_t, void* ptr, uint32_t events) noexcept {
    auto epevs = static_cast<epoll_events>(events);
    // the eventfd was set
    if (ptr == nullptr) {
        impl.reset_interrupter();
        return;
    }
#ifdef RAD_HAS_IO_URING
    else if (ptr == io_uring_loop_.get()) {
        // ptr != nullptr
        io_uring_loop_->reset_eventfd();
        stack_forward_list<detail::async_op_base> completed_ops;
        std::error_code ec;
        io_uring_loop_->get_completions(completed_ops, ec);
        if (!completed_ops.empty()) {
            thds_state_->finished_ops.merge_back(std::move(completed_ops));
            running_thds_cv_.notify_all();
        }
        return;
    }
#endif // RAD_HAS_IO_URING

    bool ready_in = false, ready_out = false;
    if (epevs & epoll_events::hup || epevs & epoll_events::in ||
        epevs & epoll_events::rdhup || epevs & epoll_events::error) {
        ready_in = true;
    }
    if (epevs & epoll_events::out || epevs & epoll_events::error) {
        ready_out = true;
    }
    do_perform_reactor_in_out_ops(ptr, ready_in, ready_out);
}
#else
void io_loop::do_perform(uintptr_t ident, void* udata,
                         uint32_t filter) noexcept {
    // the user event was set
    if (filter == EVFILT_USER) {
        impl.reset_interrupter();
        return;
    }
    // the timer expired
    if (filter == EVFILT_TIMER) {
        poll_timers();
        return;
    }
    const bool ready_in = filter == EVFILT_READ;
    const bool ready_out = filter == EVFILT_WRITE;
    do_perform_reactor_in_out_ops(udata, ready_in, ready_out);
}
#endif // __linux__