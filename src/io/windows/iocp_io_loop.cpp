#include <Windows.h>
#include <rad/async/io_loop.h>
#include <rad/io/files.h>
#include <rad/io/windows/sync_waitable_timer.h>

using namespace RAD_LIB_NAMESPACE;
using namespace io;
using namespace RAD_LIB_NAMESPACE::detail;

namespace {
    os::handle create_waitable_timer() {
        HANDLE h = ::CreateWaitableTimerW(nullptr, FALSE, nullptr);
        if (!h) {
            throw std::system_error(os::make_system_error(GetLastError()),
                                    "CreateWaitableTimerW");
        }
        return os::handle{h};
    }

    void set_waitable_timer(os::handle& h, std::chrono::nanoseconds timeout,
                            PTIMERAPCROUTINE apc_routine) {
        using waitable_timer_duration =
            std::chrono::duration<int64_t,
                                  std::ratio_multiply<std::nano, std::hecto>>;
        auto transformed_timeout =
            std::chrono::duration_cast<waitable_timer_duration>(timeout);
        LARGE_INTEGER t;
        t.QuadPart = -transformed_timeout.count();
        if (!t.QuadPart) {
            t.QuadPart = -1;
        }
        [[maybe_unused]] auto result =
            ::SetWaitableTimer(h.get(), &t, 0, apc_routine, nullptr, FALSE);
        assert(result != FALSE && "SetWaitableTimer failed !");
    }

    void cancel_waitable_timer(os::handle& h) {
        LARGE_INTEGER t{};
        t.QuadPart = 1;
        [[maybe_unused]] auto result =
            ::SetWaitableTimer(h.get(), &t, 0, nullptr, nullptr, FALSE);
        assert(result != FALSE && "SetWaitableTimer failed !");
    }
} // namespace

async_waitable_timer::async_waitable_timer()
    : handle_{create_waitable_timer()} {
}

sync_waitable_timer::sync_waitable_timer() : handle_{create_waitable_timer()} {
}

void async_waitable_timer::set(duration timeout) {
    set_waitable_timer(handle_, timeout, [](void*, DWORD, DWORD) {});
}

void sync_waitable_timer::wait(duration timeout) {
    if (timeout.count() <= 0) {
        return;
    }
    set_waitable_timer(handle_, timeout, nullptr);
    [[maybe_unused]] DWORD ret = ::WaitForSingleObject(handle_.get(), INFINITE);
    if (ret == WAIT_FAILED) {
#ifndef NDEBUG
        printf("!!! WaitForSingleObject failed with error: %d\n",
               (int)GetLastError());
        assert(false);
#endif // !NDEBUG
    }
}

void async_waitable_timer::cancel() noexcept {
    cancel_waitable_timer(handle_);
}

void sync_waitable_timer::cancel() noexcept {
    cancel_waitable_timer(handle_);
}

io_loop::io_loop(uint32_t threads_num_hint, async_mode)
    : any_executor(detail::executor_type::io_loop), impl{threads_num_hint} {
}

void io_loop::interrupt() noexcept {
    impl.interrupt();
}

void io_loop::post(async_op_t& op) noexcept {
    add_work();
    post_finished(op);
}

void io_loop::post(stack_forward_list<async_op_t> ops) noexcept {
    work_count_ += ops.size();
    post_finished(std::move(ops));
}

void io_loop::post_finished(stack_forward_list<async_op_t> ready_ops) noexcept {
    if (ready_ops.empty()) {
        return;
    }
    auto ops = finished_ops_.synchronize();
    ops->merge_back(std::move(ready_ops));
    if (!executing_thds_count_) {
        interrupt();
    }
}

void io_loop::post_finished(async_op_t& op1, async_op_t& op2) noexcept {
    auto ops = finished_ops_.synchronize();
    ops->push_back(op1);
    ops->push_back(op2);
    if (!executing_thds_count_) {
        interrupt();
    }
}

void io_loop::post_finished(async_op_t& op) noexcept {
    auto ops = finished_ops_.synchronize();
    ops->push_back(op);
    if (!executing_thds_count_) {
        return interrupt();
    }
}

void io_loop::add_work(std::size_t n) noexcept {
    work_count_ += n;
}

void io_loop::consume_work(std::size_t n) noexcept {
    work_count_ -= n;
}

void io_loop::cancel_work() noexcept {
    std::size_t count = work_count_ -= 1;
    if (!count && !executing_thds_count_) {
        interrupt();
    }
}

std::size_t io_loop::work_count() const noexcept {
    return work_count_;
}

void io_loop::stop() noexcept {
    stopped_ = true;
    if (!thd_ids_list_->empty()) {
        interrupt();
    }
}

void io_loop::consume_work() noexcept {
    --work_count_;
}

void io_loop::poll_timers() noexcept {
    while (1) {
        stack_forward_list<async_op_t> ready_timers;
        timers_queue_.poll(ready_timers);
        if (!ready_timers.empty()) {
            finished_ops_->merge_back(std::move(ready_timers));
        }
        auto least_duration = timers_queue_.least_duration();
        if (least_duration.count() == 0) {
            continue;
        }
        impl.set_timer_timeout(least_duration);
        break;
    }
}

void io_loop::consume_finished_operations() {
    auto get_op = [this]() -> async_op_t* {
        auto [lock, list] = finished_ops_.lock_guard();
        if (list.empty()) {
            return nullptr;
        }
        return &list.pop_front();
    };

    while (auto op = get_op()) {
        op->invoke(*this);
    }
}

void io_loop::restart() noexcept {
    stopped_ = false;
}

void io_loop::run(std::error_code& ec) {
    stopped_ = false;
    loop_thread_id id{this_thread::get_id()};
    thd_ids_list_->push_back(id);

    scope_exit on_exit([&]() {
        auto [lock, list] = thd_ids_list_.lock_guard();
        list.erase(&id);
        if (!list.empty()) {
            interrupt();
        }
    });

    constexpr bool alertable = true;
    constexpr uint32_t wait_timeout = std::numeric_limits<uint32_t>::max();
    constexpr uint32_t max_results_count = 100;
    constexpr int apc_intrrupted_error = 0x000000C0L;

    std::array<iocp::completion_result, max_results_count> results;

    // consume any finished operations or results, because the loop may have
    // exited due to an exception
    poll_timers();
    consume_finished_operations();

    while (!stopped_ && has_work()) {
        std::span<iocp::completion_result> completed =
            impl.iocp_port.get_results(results, wait_timeout, alertable, ec);

        ++executing_thds_count_;
        auto on_exit = scope_exit([this] {
            auto ops = finished_ops_.synchronize();
            if (!--executing_thds_count_ && !ops->empty()) {
                interrupt();
            }
        });

        if (ec) {
            if (ec.value() == apc_intrrupted_error) {
                ec.clear();
                poll_timers();
                consume_finished_operations();
                continue;
            }
            return;
        }

        poll_timers();
        stack_forward_list<io::detail::io_op> results_list;
        for (auto& result : completed) {
            if (result.ov != nullptr) {
                results_list.push_back(
                    io::detail::io_op::from_ov_ptr(result.ov));
            }
        }

        finished_ops_->merge_back(std::move(results_list));
        consume_finished_operations();
    }
}
