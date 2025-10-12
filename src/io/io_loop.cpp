#include <rad/async/io_loop.h>

using namespace RAD_LIB_NAMESPACE;

thread_pool& io_loop::resolver_thread_pool() {
    auto pool = resolver_pool_.synchronize();
    if (!pool->running()) {
        pool->start(max_resolver_threads_);
    }
    return *pool;
}

any_executor& io_loop::as_any_executor() noexcept {
    return *this;
}

schedule_op<io_loop> io_loop::schedule() noexcept {
    return schedule_op{*this};
}

bool io_loop::running_on_current_thread() const noexcept {
    loop_thread_id current_thd{this_thread::get_id()};
    auto [lock, thds] = thd_ids_list_.lock_guard();
    return !thds.empty() && thds.contains(current_thd);
}

bool io_loop::running_not_on_current_thread() const noexcept {
    loop_thread_id current_thd{this_thread::get_id()};
    auto [lock, thds] = thd_ids_list_.lock_guard();
    return !thds.empty() && !thds.contains(current_thd);
}

any_executor& io_loop::thread_pool_executor() {
    return resolver_thread_pool();
}

bool io_loop::running() const noexcept {
    return !thd_ids_list_->empty();
}

void io_loop::schedule_timer(detail::timer_impl& t) noexcept {
    bool earliest = timers_queue_.add_timer(t);
    if (earliest) {
        impl.set_timer_timeout(timers_queue_.least_duration());
    }
}

void io_loop::cancel_timer(detail::timer_impl& t) noexcept {
    stack_forward_list<async_op_t> ops;
    if (timers_queue_.remove_timer(t, ops) && !ops.empty()) {
        post_finished(std::move(ops));
    }
}

void io_loop::move_timers(detail::timer_impl& old_t,
                          detail::timer_impl& new_t) noexcept {
    timers_queue_.move_timers(old_t, new_t);
}

void io_loop::add_timer_handler(detail::timer_impl& t,
                                detail::timer_op_base& handler) noexcept {
    add_work();
    timers_queue_.add_timer_handler(t, handler);
}

bool io_loop::try_schedule(async_op_t& op) noexcept {
    if (running_on_current_thread()) {
        return false;
    }
    add_work();
    post_finished(op);
    return true;
}