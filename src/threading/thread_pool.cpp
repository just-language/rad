#include <rad/threading/thread_pool.h>

#include <algorithm>

using namespace RAD_LIB_NAMESPACE;

thread_pool::~thread_pool() {
    stop();
}

void thread_pool::start(uint32_t n) {
    if (n == 0) {
        return;
    }
    auto state = state_.synchronize();
    if (state->workers_num > 0) {
        return;
    }
    state->stopped = false;
    if (n == 1) {
        state->workers = scoped_thread(&thread_pool::work_fn, this);
        state->workers_num = 1;
    }
    else {
        std::vector<scoped_thread> workers;
        workers.reserve(n);
        while (n--) {
            workers.emplace_back(scoped_thread(&thread_pool::work_fn, this));
        }
        state->workers_num = static_cast<uint32_t>(workers.size());
        state->workers = std::move(workers);
    }
}

void thread_pool::stop() noexcept {
    {
        auto state = state_.synchronize();
        if (state->workers_num == 0) {
            return;
        }
        state->stopped = true;
        queue_cv_.notify_all();
        // unlock the state so other thread can see stopped!
    }

    if (scoped_thread* worker = std::get_if<scoped_thread>(&state_->workers)) {
        if (worker->joinable()) {
            worker->join();
        }
    }
    else if (auto workers =
                 std::get_if<std::vector<scoped_thread>>(&state_->workers)) {
        workers->clear();
    }

    scoped_thread timer_worker;
    {
        auto state = state_.synchronize();
        if (state->timer_worker.joinable()) {
            timer_.cancel();
            timer_worker = std::move(state->timer_worker);
        }
    }
    if (timer_worker.joinable()) {
        timer_worker.join();
    }

    auto state = state_.synchronize();
    state->workers = std::monostate{};
    state->workers_num = 0;
    wait_cv_.notify_all();
}

void thread_pool::wait() noexcept {
    auto [lock, state] = state_.unique_lock();
    wait_cv_.wait(lock, [&state] { return state.workers_num == 0; });
}

bool thread_pool::running_on_current_thread() const noexcept {
    auto state = state_.synchronize();
    if (state->workers_num == 0) {
        return false;
    }
    auto curr_id = this_thread::get_id();
    if (auto worker = std::get_if<scoped_thread>(&state->workers)) {
        return worker->get_id() == curr_id;
    }
    else if (auto workers =
                 std::get_if<std::vector<scoped_thread>>(&state->workers)) {
        auto it = std::find_if(
            workers->begin(), workers->end(),
            [curr_id](const auto& w) { return w.get_id() == curr_id; });
        return it != workers->end();
    }
    return false;
}

void thread_pool::post(op_type& op) noexcept {
    state_->jobs.push_back(op);
    queue_cv_.notify_one();
}

void thread_pool::post_finished(op_type& op) noexcept {
    post(op);
}

void thread_pool::post_finished(stack_forward_list<op_type> ops) noexcept {
    state_->jobs.merge_back(std::move(ops));
    queue_cv_.notify_all();
}

void thread_pool::add_work(std::size_t n) noexcept {
    (void)n;
}

void thread_pool::cancel_work() noexcept {
}

void thread_pool::consume_work(std::size_t n) noexcept {
    (void)n;
}

std::size_t thread_pool::work_count() const noexcept {
    return 0;
}

any_executor& thread_pool::as_any_executor() noexcept {
    return *this;
}

schedule_op<thread_pool> thread_pool::schedule() noexcept {
    return schedule_op{*this};
}

void thread_pool::schedule_timer(detail::timer_impl& t) noexcept {
    auto state = state_.synchronize();
    start_timer_thread_if_not(*state);
    bool earliest = timers_queue_.add_timer(t);
    if (earliest) {
        timer_.cancel();
    }
}

void thread_pool::cancel_timer(detail::timer_impl& t) noexcept {
    stack_forward_list<op_type> ops;
    if (timers_queue_.remove_timer(t, ops) && !ops.empty()) {
        post_finished(std::move(ops));
    }
}

void thread_pool::move_timers(detail::timer_impl& old_t,
                              detail::timer_impl& new_t) noexcept {
    timers_queue_.move_timers(old_t, new_t);
}

void thread_pool::add_timer_handler(detail::timer_impl& t,
                                    detail::timer_op_base& handler) noexcept {
    add_work();
    timers_queue_.add_timer_handler(t, handler);
}

void thread_pool::start_timer_thread_if_not(shared_state_t& state) {
    if (!state.timer_worker.joinable()) {
        state.timer_worker = scoped_thread([this] { timer_fn(); });
    }
}

bool thread_pool::try_schedule(op_type& job) noexcept {
    if (running_on_current_thread()) {
        return false;
    }
    post(job);
    return true;
}

void thread_pool::work_fn() {
    while (1) {
        // wait for a job or exit signal
        {
            auto [lock, state] = state_.unique_lock();
            queue_cv_.wait(lock, [&state] {
                return state.stopped || !state.jobs.empty();
            });
            if (state.stopped) {
                return;
            }
        }
        // execute ready operations
        execute_operations();
    }
}

void thread_pool::timer_fn() {
    while (!state_->stopped) {
        // 0 duration will not cause any wait
        timer_.wait(timers_queue_.least_duration());
        stack_forward_list<op_type> ops;
        timers_queue_.poll(ops);
        if (!ops.empty()) {
            post_finished(std::move(ops));
        }
    }
}

void thread_pool::execute_operations() {
    while (1) {
        op_type* op_ptr = nullptr;
        {
            auto state = state_.synchronize();
            if (state->stopped || state->jobs.empty()) {
                return;
            }
            op_ptr = &state->jobs.pop_front();
        }
        op_ptr->invoke(*this);
    }
}