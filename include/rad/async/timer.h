#pragma once
#include <rad/async/executor.h>
#include <rad/libbase.h>
#include <rad/stack_list.h>
#include <rad/threading/synchronized_value.h>
#include <rad/trackable.h>

#include <chrono>

namespace RAD_LIB_NAMESPACE {
    template <class Lockable>
    class wait_queue_base;
}

namespace RAD_LIB_NAMESPACE::detail {
    class timer_state {
        template <typename>
        friend class rad::wait_queue_base;

        stack_forward_list<timer_op_base> handlers;
        bool is_ticking = false;
    };

    struct timer_impl : public stack_double_list_node {
        using clock = std::chrono::steady_clock;
        using time_point = clock::time_point;
        using duration = clock::duration;
        using executor_type = timer_executor;

        ref<timer_executor> ex_;
        // the timer state can only accessed by the wait queue
        timer_state state_;
        // the expiry time can't change while the timer is
        // linked to a wait queue
        time_point expiry_time;

        timer_impl(timer_executor& ex) noexcept : ex_{ex} {
        }

        timer_impl(timer_executor& ex, duration rel_time) noexcept
            : ex_{ex}, expiry_time{clock::now() + rel_time} {
            ex.schedule_timer(*this);
        }

        timer_impl(timer_executor& ex, time_point abs_time) noexcept
            : ex_{ex}, expiry_time{abs_time} {
            ex.schedule_timer(*this);
        }

        timer_impl(timer_impl&& other) noexcept
            : ex_{other.ex_}, expiry_time{other.expiry_time} {
            ex_->move_timers(other, *this);
        }

        timer_impl& operator=(timer_impl&& other) noexcept {
            cancel();
            ex_ = other.ex_;
            expiry_time = other.expiry_time;
            ex_->move_timers(other, *this);
            return *this;
        }

        duration expiry_duration() const noexcept {
            return std::max(expiry_time - clock::now(), duration{0});
        }

        bool expired(time_point now) const noexcept {
            return now >= expiry_time;
        }

        void expires(time_point time) noexcept {
            // remove the timer from the waiting queue if it
            // was ticking
            ex_->cancel_timer(*this);

            // the timer is not now in any queue so the
            // expiry time can be updated
            expiry_time = time;

            // insert the timer into the wait queue again
            ex_->schedule_timer(*this);
        }

        void cancel() noexcept {
            ex_->cancel_timer(*this);
        }
    };
} // namespace RAD_LIB_NAMESPACE::detail

namespace RAD_LIB_NAMESPACE {
    /*!
     * @brief The wait queue is an optional thread safe stack linked list of
     * fired timers to be used by executors to manage timers
     */
    template <class Lockable>
    class wait_queue_base {
    public:
        using timer_impl = detail::timer_impl;

        // the least timer's expiry time in the list, or
        // time_point::max() if no any timer is stored
        timer_impl::time_point least_expiry() const noexcept {
            auto [lock, timers] = timers_.lock_guard();
            return least_expiry_time_;
        }

        // returns least_expiry() - clock::now() and negative
        // durations are converted to 0
        timer_impl::duration least_duration() const noexcept {
            auto now = timer_impl::clock::now();
            auto dur = least_expiry() - now;
            if (dur.count() < 0) {
                dur = timer_impl::duration{0};
            }
            return dur;
        }

        /*!
         * @brief Add the timer new_timer to the list, this
         * timer can't already be in the list. The least expiry
         * time is updated if this timer's expiry time is the
         * earliest in the list
         * @param new_timer the timer to add
         * @return true if this timer's expiry time is the
         * erarliest in the list, otherwise false
         */
        bool add_timer(timer_impl& new_timer) noexcept {
            auto [lock, timers] = timers_.lock_guard();

            new_timer.state_.is_ticking = true;

            bool is_earliest = timers.empty() || new_timer.expiry_time <
                                                     timers.front().expiry_time;
            if (is_earliest) {
                least_expiry_time_ = new_timer.expiry_time;
                timers.push_front(new_timer);
                return true;
            }

            for (auto& t : timers) {
                if (t.expiry_time >= new_timer.expiry_time) {
                    timers.push_before(t, new_timer);
                    return false;
                }
            }

            timers.push_back(new_timer);
            return false;
        }

        /*!
         * @brief Removes timer t from the list if it is already
         * there and update the current least expiry time if
         * needed. If this timer was the only timer in the list
         * then the least expiry time is max expiry.
         * @param t the timer to remove if it is ticking and
         * mark its handler as canceled
         * @param ops the list of operations to merge the timer
         * handlers to if it had handlers
         * @return true if the timer was ticking and removed,
         * otherwise false if the timer was not ticking and not
         * removed
         */
        bool
        remove_timer(timer_impl& t,
                     stack_forward_list<detail::async_op_base>& ops) noexcept {
            auto timers = timers_.synchronize();
            if (!t.state_.is_ticking) {
                return false;
            }

            t.state_.is_ticking = false;
            for (auto& handler : t.state_.handlers) {
                handler.canceled = true;
            }
            ops.merge_back(std::move(t.state_.handlers));

            timers->erase(&t);

            if (timers->empty()) {
                least_expiry_time_ = max_expiry();
            }
            else {
                least_expiry_time_ = timers->front().expiry_time;
            }

            return true;
        }

        bool add_timer_handler(timer_impl& t,
                               detail::timer_op_base& handler) noexcept {
            auto timers = timers_.synchronize();
            t.state_.handlers.push_back(handler);
            return true;
        }

        /*!
         * @brief Move the state of old timer to a new one. If
         * the old timer is now ticking then remove it from the
         * queue and insert the new timer in its place. Handlers
         * of the old timer are moved to the new timer, but if
         * any of them holds a reference to the old timer it
         * will keep pointing to the old timer so care must be
         * taken!
         * @param old_timer the old timer
         * @param new_timer the new timer
         */
        void move_timers(timer_impl& old_timer,
                         timer_impl& new_timer) noexcept {
            auto timers = timers_.synchronize();
            new_timer.state_ = std::move(old_timer.state_);
            if (!old_timer.state_.is_ticking) {
                return;
            }
            old_timer.state_.is_ticking = false;
            timers->push_before(old_timer, new_timer);
            timers->erase(old_timer);
        }

        /*!
         * @brief Find expired timers and take their handlers
         * and put them into ops. The least expiry time is
         * updated to the first timer in the list after removal
         * or to max expiry if the list is left empty.
         * @param ops the operations list to put handlers into
         */
        void poll(stack_forward_list<detail::async_op_base>& ops) noexcept {
            auto [lock, timers] = timers_.lock_guard();
            if (timers.empty()) {
                return;
            }

            auto now = timer_impl::clock::now();

            for (auto it = timers.begin(); it != timers.end();) {
                if (!it->expired(now)) {
                    least_expiry_time_ = it->expiry_time;
                    return;
                }

                auto t = it.get();
                it = timers.erase(it);
                t->state_.is_ticking = false;
                ops.merge_back(std::move(t->state_.handlers));
            }

            assert(timers.empty());
            least_expiry_time_ = max_expiry();
        }

    private:
        static timer_impl::time_point max_expiry() noexcept {
            return timer_impl::time_point::max();
        }

        sync_value<stack_list<timer_impl>, Lockable> timers_;
        timer_impl::time_point least_expiry_time_ = max_expiry();
    };

    using wait_queue = wait_queue_base<mutex>;

    template <class Handler>
    concept WaitHandler =
        requires(Handler handler) { handler(std::error_code{}); };

    /*!
     * @brief Timer is used to async wait on a timer executor.
     */
    class timer {
        using impl_type = detail::timer_impl;

        class awaiter;

        template <class Handler, class Alloc>
        struct wait_op;

        template <typename, typename>
        friend struct rebind_executor_helper;

    public:
        // the type of clock used by the timer (ususally
        // std::steady_clock)
        using clock = impl_type::clock;
        // the type of duration used by the timer (usually the
        // duration of the timer clock)
        using duration = impl_type::duration;
        // the type of time point used by the timer (usually the
        // time point of the timer clock)
        using time_point = impl_type::time_point;
        // the type of executor used by the timer
        // (timer_executor)
        using executor_type = impl_type::executor_type;

        template <std::size_t max_handler_size>
        static constexpr std::size_t wait_allocator_size() noexcept {
            using handler = handler_allocator_size_calculator<max_handler_size>;
            return sizeof(wait_op<handler, stateful_null_allocator>);
        }

        /*!
         * @brief Construct the timer and use the provided
         * executor to dispatch wait operations
         * @param ex the executor to dispatch wait operations on
         */
        timer(timer_executor& ex) noexcept : impl{ex} {
        }

        /*!
         * @brief Construct the timer and use the provided
         * executor to dispatch wait operations. And set the
         * relative to now expiry time @p rel_time. This is
         * equivalent to calling expires_after()
         * @param ex the executor to dispatch wait operations on
         * @param rel_time the relative to now expiry time to
         * set
         */
        timer(timer_executor& ex, duration rel_time) noexcept
            : impl{ex, rel_time} {
        }

        /*!
         * @brief Construct the timer and use the provided
         * executor to dispatch wait operations. And set the
         * absolute expiry time @p abs_time. This is equivalent
         * to calling expires_at()
         * @param ex the executor to dispatch wait operations on
         * @param abs_time the absolute expiry time to set
         */
        timer(timer_executor& ex, time_point abs_time) noexcept
            : impl{ex, abs_time} {
        }

        /*!
         * @brief Move construct a timer. If the moved from
         * timer was ticking the moved to timer will be ticking
         * on the same executor. All pending wait operations of
         * the old timer are moved to the new timer. After move
         * the moved from timer is not ticking and has now wait
         * operations.
         */
        timer(timer&&) noexcept = default;

        /*!
         * @brief Move assign to the timer. This timer will be
         * canceled before assigned to the moved from timer. If
         * the moved from timer was ticking this timer will be
         * ticking on the same executor. All pending wait
         * operations of the moved from timer are moved to this
         * timer. After move the moved from timer is not ticking
         * and has now wait operations.
         */
        timer& operator=(timer&&) noexcept = default;

        /*!
         * @brief Destroy the timer and cancel all pending wait
         * operations. Note that unlike other async objects, the
         * timer is safe to destroy while it has a pending async
         * wait operation
         */
        ~timer() {
            cancel();
        }

        /*!
         * @brief Get a reference to the timer executor used by
         * the timer
         * @return a reference to timer the executor used by the
         * timer
         */
        executor_type& executor() noexcept {
            return impl.ex_;
        }

        /*!
         * @brief Get a reference to the timer executor used by
         * the timer
         * @return a reference to timer the executor used by the
         * timer
         */
        const executor_type& executor() const noexcept {
            return impl.ex_;
        }

        /*!
         * @brief Start an async wait operation which will be
         * invoked when the timer is canceled or expired. A
         * timer can have multiple pending wait operations at
         * the same time.
         * @tparam Handler The type of handler which must
         * satisfy WaitHandler
         * @tparam Alloc The type of allocator which must
         * satisfy HandlerAllocator
         * @param handler a handler to invoke when the operation
         * is done and will be passed an error_code that
         * determines whether the operation was canceled or the
         * timer was expired.
         * @param alloc An allocator used to allocate the
         * operation. The size passed to its allocate method is
         * in bytes and the alignment of the allocator must be
         * suitable for std::max_align_t
         */
        template <WaitHandler Handler,
                  HandlerAllocator Alloc = default_io_allocator>
        void async_wait(Handler&& handler, const Alloc& alloc = Alloc()) {
            using namespace detail;
            using op_t = wait_op<std::remove_reference_t<Handler>, Alloc>;
            auto op = allocate_op<op_t>(alloc, std::forward<Handler>(handler),
                                        impl.ex_->as_any_executor());
            impl.ex_->add_timer_handler(impl, *op);
        }

        /*!
         * @brief Async wait on the timer until the timer is
         * canceled or expired. Note that the wait operation
         * will not start until the returned awaitable is
         * awaited.
         * @param ec if set to no_ec then errors are reported
         * via exceptions, otherwise errors are reported via @p
         * ec.
         * @return an awaitable that is when awaited will start
         * the wait operation.
         */
        awaiter wait(std::error_code& ec = no_ec) noexcept;

        /*!
         * @brief Set the relative to now expiry time using
         * expires_after(), then async wait on the timer using
         * wait()
         * @param relative_time the relative to now expiry time.
         * Note that expires_after() cancels all pending wait
         * operations
         * @param ec if set to no_ec then errors are reported
         * via exceptions, otherwise errors are reported via @p
         * ec.
         * @return an awaitable that is when awaited will start
         * the wait operation.
         */
        template <class Rep, class Period>
        awaiter wait(const std::chrono::duration<Rep, Period>& relative_time,
                     std::error_code& ec = no_ec) noexcept;

        /*!
         * @brief Set the relative to now expiry time using
         * expires_after(), then async wait on the timer using
         * wait()
         * @param relative_time the relative to now expiry time.
         * Note that expires_after() cancels all pending wait
         * operations
         * @param ec if set to no_ec then errors are reported
         * via exceptions, otherwise errors are reported via @p
         * ec.
         * @return an awaitable that is when awaited will start
         * the wait operation.
         */
        template <class Rep, class Period>
        awaiter
        wait_for(const std::chrono::duration<Rep, Period>& relative_time,
                 std::error_code& ec = no_ec) noexcept;

        /*!
         * @brief Set the absolute expiry time using
         * expires_at(), then async wait on the timer using
         * wait()
         * @param abs_time the absolute expiry time
         * @param ec if set to no_ec then errors are reported
         * via exceptions, otherwise errors are reported via @p
         * ec.
         * @return an awaitable that is when awaited will start
         * the wait operation.
         */
        template <class Clock, class Duration>
        awaiter wait(const std::chrono::time_point<Clock, Duration>& abs_time,
                     std::error_code& ec = no_ec) noexcept;

        /*!
         * @brief Set the absolute expiry time using
         * expires_at(), then async wait on the timer using
         * wait()
         * @param abs_time the absolute expiry time
         * @param ec if set to no_ec then errors are reported
         * via exceptions, otherwise errors are reported via @p
         * ec.
         * @return an awaitable that is when awaited will start
         * the wait operation.
         */
        template <class Clock, class Duration>
        awaiter
        wait_until(const std::chrono::time_point<Clock, Duration>& abs_time,
                   std::error_code& ec = no_ec) noexcept;

        /*!
         * @brief Get the absolute timer expiry time
         * @return the absolute timer expiry time
         */
        time_point expires_at() const noexcept {
            return impl.expiry_time;
        }

        /*!
         * @brief Set the timer's expiry time to the absolute
         * time @p abs_time. Pending wait operations will be
         * canceled.
         * @param abs_time the absolute expiry time
         */
        template <class Clock, class Duration>
        void expires_at(
            const std::chrono::time_point<Clock, Duration>& abs_time) noexcept {
            using namespace std::chrono;
            if constexpr (std::is_same_v<time_point, std::chrono::time_point<
                                                         Clock, Duration>>) {
                impl.expires(abs_time);
            }
            else {
                impl.expires(clock::now() + (abs_time - Clock::now()));
            }
        }

        /*!
         * @brief Get the timer expiry time relative to now
         * @return the timer expiry time relative to now
         */
        duration expires_after() const noexcept {
            return impl.expiry_duration();
        }

        /*!
         * @brief Set the timer's expiry time to @p
         * relative_time relative to now. Pending wait
         * operations will be canceled.
         * @param relative_time the relative to now expiry time
         */
        template <class Rep, class Period>
        void expires_after(
            const std::chrono::duration<Rep, Period>& relative_time) noexcept {
            time_point expiry_time = clock::now() + relative_time;
            impl.expires(expiry_time);
        }

        /*!
         * @brief Cancel all pending wait operations.
         * Canceled operations are passed an error_code that
         * indicates cancelation. Note that not all operations
         * can be canceled. Operations that have completed and
         * are scheduled for invocation can no longer be
         * canceled.
         */
        void cancel() noexcept {
            impl.cancel();
        }

    private:
        impl_type impl;
    };

    class [[nodiscard]] timer::awaiter final : noncopyable,
                                               detail::timer_op_base,
                                               error_storage {
    public:
        awaiter(impl_type& impl, std::error_code& ec) noexcept
            : error_storage(ec), impl{impl}, ex_{impl.ex_->as_any_executor()} {
        }

        // always return false, the wait operation always causes
        // the coroutine to suspend
        constexpr bool await_ready() const noexcept {
            return false;
        }

        // start the wait operation, the wait operation is not
        // started until this method is called
        void await_suspend(std::coroutine_handle<> coro) noexcept {
            waiter = coro;
            impl->ex_->add_timer_handler(*impl, *this);
        }

        // store error or throw an exception if the wait
        // operation was canceled
        void await_resume() {
            if (canceled) {
                store(std::make_error_code(std::errc::operation_canceled));
                raise("wait");
            }
        }

    private:
        void invoke_operation() override {
            waiter.resume();
        }

        any_executor& associated_executor() const noexcept override {
            return ex_;
        }

        ref<impl_type> impl;
        ref<any_executor> ex_; // store the executor reference to make
                               // the operation independent from the
                               // timer once started
        std::coroutine_handle<> waiter;
    };

    template <class Handler, class Alloc>
    struct timer::wait_op final : detail::timer_op_base,
                                  allocator_storage<Alloc> {
        using alloc_base = allocator_storage<Alloc>;

        Handler handler;
        // store the executor reference to make the operation
        // independent from the timer once started
        ref<any_executor> ex_;

        wait_op(Handler&& handler, any_executor& ex, const Alloc& alloc)
            : alloc_base(alloc), handler{std::forward<Handler>(handler)},
              ex_{ex} {
        }

        void invoke_operation() override {
            std::error_code ec =
                canceled ? std::make_error_code(std::errc::operation_canceled)
                         : std::error_code{};
            detail::invoke_handler(this, ec);
        }

        any_executor& associated_executor() const noexcept override {
            return ex_;
        }
    };

    inline auto timer::wait(std::error_code& ec) noexcept -> awaiter {
        return {impl, ec};
    }

    template <class Rep, class Period>
    inline auto
    timer::wait(const std::chrono::duration<Rep, Period>& relative_time,
                std::error_code& ec) noexcept -> awaiter {
        expires_after(relative_time);
        return wait(ec);
    }

    template <class Rep, class Period>
    inline auto
    timer::wait_for(const std::chrono::duration<Rep, Period>& relative_time,
                    std::error_code& ec) noexcept -> awaiter {
        return wait(relative_time, ec);
    }

    template <class Clock, class Duration>
    inline auto
    timer::wait(const std::chrono::time_point<Clock, Duration>& abs_time,
                std::error_code& ec) noexcept -> awaiter {
        expires_at(abs_time);
        return wait(ec);
    }

    template <class Clock, class Duration>
    inline auto
    timer::wait_until(const std::chrono::time_point<Clock, Duration>& abs_time,
                      std::error_code& ec) noexcept -> awaiter {
        return wait(abs_time, ec);
    }

    template <ProxyTimerExecutor Exec>
    struct rebind_executor_helper<Exec, timer> {
        static timer rebind(Exec& ex, timer&& t) {
            t.cancel();
            timer new_t{std::move(t)};
            new_t.impl.ex_ = ex;
            return new_t;
        }
    };

    struct [[nodiscard]] timeout_guard {
        /*!
         * @brief start an async wait operation on the timer and
         * execute the handler if the operation was not canceled
         * @param t the timer to wait on, must outlive the
         * timeout_guard object. The caller must not use this
         * timer as far as the timeout_guard is still in the
         * scope
         * @param cancel_flag must outlive the timeout_guard
         * object and the wait operation even if canceled, it
         * must be valid inside the handler. The caller must not
         * use this flag until the handler is invoked
         * @param handler the handler to execute if the wait
         * operation is finished (timed out) and not canceled.
         * The operation is canceled in the destructor of the
         * timeout_guard object
         */
        template <class Handler, HandlerAllocator Alloc = default_io_allocator>
        timeout_guard(timer& t, bool& cancel_flag, Handler&& handler,
                      const Alloc& alloc = Alloc())
            : t{t}, cancel_flag{cancel_flag} {
            cancel_flag = false;
            t.async_wait(
                [&cancel_flag, handler = std::forward<Handler>(handler)](
                    const std::error_code& ec) {
                    if (cancel_flag || ec) {
                        return;
                    }
                    handler();
                },
                alloc);
        }

        ~timeout_guard() {
            cancel_flag = true;
            t.cancel();
        }

        timer& t;
        bool& cancel_flag;
    };
} // namespace RAD_LIB_NAMESPACE
