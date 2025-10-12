#include <rad/async/strand.h>
#include <cassert>

using namespace RAD_LIB_NAMESPACE;

#ifndef NDEBUG
strand_base::~strand_base() {
    auto state = state_.synchronize();
    assert(!state->is_executing() &&
           "the strand is destroyed while it is executing handlers !");
    assert(state->ops.empty() && "the strand is destroyed while it has pending "
                                 "operation which will be lost and leaked !");
    assert(!state->work_count && "the strand is destroyed while outstanding "
                                 "operations are holding reference to it !");
}
#endif // !NDEBUG

void strand_base::post(op_type& op) noexcept {
    auto state = state_.synchronize();
    state->work_count += 1;
    associated_executor().add_work(1);
    do_post_finished(*state, op);
}

void strand_base::post(list_type ops) noexcept {
    if (ops.empty()) {
        return;
    }
    auto state = state_.synchronize();
    state->work_count += ops.size();
    associated_executor().add_work(ops.size());
    do_post_finished(*state, std::move(ops));
}

void strand_base::post_finished(op_type& op) noexcept {
    auto state = state_.synchronize();
    do_post_finished(*state, op);
}

void strand_base::post_finished(list_type ops) noexcept {
    auto state = state_.synchronize();
    do_post_finished(*state, std::move(ops));
}

void strand_base::add_work(std::size_t n) noexcept {
    state_->work_count += n;
    associated_executor().add_work(n);
#ifdef RAD_DEBUG_ASYNC_OPERATIONS
    printf("** executor (%s:%zu) increased work count by: %zu. strand work "
           "count: %zu, inner work count: %zu\n",
           executor_type_string(), get_ex_id(), n, state.work_count,
           ex_.work_count());
#endif // RAD_DEBUG_ASYNC_OPERATIONS
}

void strand_base::consume_work(std::size_t n) noexcept {
    {
        auto state = state_.synchronize();
        assert(state->work_count >= n);
        state->work_count -= n;
    }
    associated_executor().consume_work(n);
#ifdef RAD_DEBUG_ASYNC_OPERATIONS
    printf("** executor (%s:%zu) decreased work count by: %zu. strand work "
           "count: %zu, inner work count: %zu\n",
           executor_type_string(), get_ex_id(), n, state_->work_count,
           ex_.work_count());
#endif // RAD_DEBUG_ASYNC_OPERATIONS
}

void strand_base::cancel_work() noexcept {
    auto state = state_.synchronize();
    auto& ex = associated_executor();
    ex.cancel_work();
    assert(state->work_count >= 1);
    state->work_count -= 1;
    if (!state->work_count) {
        state->wait_op.post(ex);
    }
}

std::size_t strand_base::work_count() const noexcept {
    return state_->work_count;
}

bool strand_base::running_on_current_thread() const noexcept {
    return state_->id == this_thread::get_id();
}

void strand_base::do_post_finished(state_t& state, op_type& op) noexcept {
    state.ops.push_back(op);
    if (!state.is_idle()) {
        return;
    }
    state.set_pending();
#ifdef RAD_DEBUG_ASYNC_OPERATIONS
    printf("** executor (%s:%zu) is being posted to its inner executor "
           "(%s:%zu)\n",
           executor_type_string(), get_ex_id(),
           associated_executor().executor_type_string(),
           associated_executor().get_ex_id());
#endif // RAD_DEBUG_ASYNC_OPERATIONS
    associated_executor().post(*this);
}

void strand_base::do_post_finished(state_t& state, list_type ops) noexcept {
    state.ops.merge_back(std::move(ops));
    if (!state.is_idle()) {
        return;
    }
    state.set_pending();
#ifdef RAD_DEBUG_ASYNC_OPERATIONS
    printf("** executor (%s:%zu) is being posted to its inner executor "
           "(%s:%zu)\n",
           executor_type_string(), get_ex_id(),
           associated_executor().executor_type_string(),
           associated_executor().get_ex_id());
#endif // RAD_DEBUG_ASYNC_OPERATIONS
    associated_executor().post(*this);
}

bool strand_base::try_schedule(detail::async_op_base& op) noexcept {
    auto state = state_.synchronize();
    if (state->id != thread_id{} && state->id == this_thread::get_id()) {
        return false;
    }
    associated_executor().add_work(1);
    state->work_count += 1;
    do_post_finished(*state, op);
    return true;
}

void strand_base::run() {
    list_type ready_ops;
    {
        auto state = state_.synchronize();
        // the strand can't enter here twice since it can't exist in two
        // queues
        assert(!state->is_executing());
        state->set_executing();
        state->id = this_thread::get_id();
        ready_ops = std::move(state->ops);
    }

    auto on_exit = scope_exit([&] { after_run(std::move(ready_ops)); });

    while (!ready_ops.empty()) {
        while (!ready_ops.empty()) {
            // invoke() decreases the work count by one
            ready_ops.pop_front().invoke(*this);
        }
        ready_ops = std::move(state_->ops);
    }
}

void strand_base::after_run(list_type remaining_ops) noexcept {
    auto state = state_.synchronize();
    // since this is called from run() the strand must have been in
    // executing state
    assert(state->is_executing());
    // the strand is not currently executing on any thread
    state->id = {};
    // if exited due to an exception insert remaining operations at the
    // front
    if (!remaining_ops.empty()) {
        state->ops.merge_front(std::move(remaining_ops));
    }

    auto& ex = associated_executor();
    if (!state->ops.empty()) {
        // exited due to an exception or some operations were posted
        // between the last invoked one and entering after_run so
        // arrange for the strand to run again
        state->set_pending();
        ex.post(*this);
    }
    else {
        // set idle so any subsequent post/post_finished will post the
        // strand again
        state->set_idle();
        // notify the waiter if there is no more work
        if (state->work_count == 0) {
            state->wait_op.post(ex);
        }
    }
}

void strand_base::invoke_operation() {
    run();
}

strand_base::awaitable::~awaitable() {
}

bool strand_base::awaitable::await_suspend(
    std::coroutine_handle<> waiter) noexcept {
    waiter_ = waiter;
    auto state = st_.state_.synchronize();
    if (state->work_count == 0) {
        return false;
    }
    state->wait_op.set_op(st_.associated_executor(), *this);
    return true;
}

void strand_base::awaitable::invoke_operation() {
    waiter_.resume();
}

any_executor& strand_base::awaitable::associated_executor() const noexcept {
    // the awaitable will not be resumed on the strand itself but on its
    // inner executor since it is resumed when the strand has finished
    // exeuting operations !
    return st_.associated_executor();
}