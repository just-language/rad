#pragma once
#include <rad/async/async_wait_queue.h>
#include <rad/async/checked_counter.h>
#include <rad/async/executor.h>

namespace RAD_LIB_NAMESPACE {

    template <class Handler>
    concept AsyncCounterHandler = requires(Handler handler) { handler(); };

    /*!
     * @brief Async reusable dynamic barrier.
     *
     * Although async phaser is used by different operations and tasks,
     * it is not thread safe and may not be accessed by different threads
     * concurrently. So, the executor used by the phaser must be strand
     * or an executor that runs on only one thread.
     *
     * The phaser supports multiple wait operations at the same time.
     *
     * The default initial count is 0 or the provided count on construction.
     *
     * When count_up() is called the internal counter is increased.
     *
     * When count_down() is called the internal counter is decreased,
     * and when the count reaches 0, all the waiers operations are scheduled
     * for invocation.
     *
     * When async_wait() is called and the count is 0 the wait
     * operation is scheduled immediately for invocation.
     *
     * When async_wait() is called and the count is greater than 0 the wait
     * operation is will not be scheduled immediately for invocation until
     * the count reaches 0 by calls to count_down().
     */
    class async_phaser : noncopyable, nonmovable, public trackable {
        class auto_decrement;

        class awaiter;

        template <class Handler, class Alloc>
        struct wait_op;

        friend class awaiter;

    public:
        template <std::size_t max_handler_size>
        static constexpr std::size_t max_allocator_size() noexcept {
            using handler = handler_allocator_size_calculator<max_handler_size>;
            return sizeof(wait_op<handler, stateful_null_allocator>);
        }

        /// The type of executor. (any_executor)
        using executor_type = any_executor;

        /*!
         * @brief Construct an async phaser with executor and
         * initial count.
         * @param ex The executor that the phaser will use to
         * dispatch handlers for asynchronous wait operations
         * performed on the phaser.
         * @param init_count The initial count of the phaser.
         */
        async_phaser(executor_type& ex, std::size_t init_count = 0) noexcept
            : ex_{ex}, count_{init_count} {
        }

#ifndef NDEBUG
        ~async_phaser() {
            // The checked counter will fail an assert if
            // its count is not zero. it's valid to destroy
            // async_phaser with non zero count but not with
            // waiter.
            count_.reset();
        }
#endif // !NDEBUG

        /*!
         * @brief Get a reference to the executor used by the
         * phaser.
         * @return A reference to the executor used by the
         * phaser.
         */
        executor_type& executor() noexcept {
            return ex_;
        }

        /*!
         * @brief Get a const reference to the executor used by
         * the phaser.
         * @return A const reference to the executor used by the
         * phaser.
         */
        const executor_type& executor() const noexcept {
            return ex_;
        }

        /*!
         * @brief Get the maximum allowed value for the internal
         * counter.
         * @return The maximum allowed value for the internal
         * counter.
         */
        static constexpr std::size_t max_value() noexcept {
            return std::numeric_limits<std::size_t>::max();
        }

        /*!
         * @brief Get the current value of the internal counter
         * of the phaser.
         * @return The current value of the internal counter of
         * the phaser.
         */
        std::size_t value() const noexcept {
            return count_;
        }

        /*!
         * @brief Check if the phaser is ready, which means its
         * internal count is 0.
         * @return True if the internal count is 0, otherwise
         * false.
         */
        bool is_ready() const noexcept {
            return value() == 0;
        }

        /*!
         * @brief Increment the internal counter and return an
         * object that is when destroyed will decrement the
         * counter.
         * @return An object that is when destroyed will
         * decrement the counter.
         */
        auto_decrement make_auto_counter() noexcept;

        /*!
         * @brief Increase the internal counter by one.
         * If the current counter value is equal to the maximum
         * value, the behavior is undefined.
         */
        void count_up() noexcept {
            ++count_;
        }

        /*!
         * @brief Decrease the internal counter by one.
         * If the current counter value is equal to 0,
         * the behavior is undefined.
         */
        void count_down() noexcept {
            if (!--count_) {
                waiters_.post(*ex_);
            }
        }

        /*!
         * @brief Start async wait operation on the phaser.
         *
         * The operation will not start until the returned
         * awaitable is awaited.
         *
         * If the current count of the phaser is 0 when the
         * awaitable is awaited, the awaiting coroutine will be
         * resumed immediately.
         * @return An awaitable that is when awaited will start
         * the wait operation.
         */
        awaiter async_wait() noexcept;

        /*!
         * @brief Start async wait operation on the phaser.
         *
         * The operation will not start until the returned
         * awaitable is awaited.
         *
         * If the current count of the phaser is 0 when the
         * awaitable is awaited, the awaiting coroutine will be
         * resumed immediately.
         * @return An awaitable that is when awaited will start
         * the wait operation.
         */
        awaiter operator co_await() noexcept;

        /*!
         * @brief Start async wait operation on the phaser.
         *
         * When the phaser counter reaches 0, the provided
         * handler will be posted for invocation.
         *
         * If the current count of the phaser is 0 the provided
         * handler will immediately be posted for invocation.
         * @tparam Handler The handler type.
         * @tparam Alloc The allocator type.
         * @param handler The handler to invoke when the count
         * is 0. It must be callable with no arguemnts.
         * @param alloc The allocator used to allocate memory
         * for the operation.
         */
        template <AsyncCounterHandler Handler,
                  HandlerAllocator Alloc = default_io_allocator>
        void async_wait(Handler&& handler, const Alloc& alloc = Alloc()) {
            if (!count_) {
                post(
                    *ex_,
                    [handler = std::forward<Handler>(handler)]() mutable {
                        handler();
                    },
                    alloc);
                return;
            }
            using op_t = wait_op<Handler, Alloc>;
            op_t* op = detail::allocate_op<op_t>(
                alloc, std::forward<Handler>(handler), ex_);
            waiters_.add_wait_op(ex_, *op);
        }

    private:
        ref<executor_type> ex_;

    protected:
        async_wait_queue waiters_;
        checked_counter<std::size_t> count_;
    };

    class async_phaser::auto_decrement : noncopyable, nonmovable {
    public:
        auto_decrement(async_phaser& counter) noexcept : counter_{counter} {
            counter.count_up();
        }

        ~auto_decrement() {
            counter_->count_down();
        }

    private:
        ref<async_phaser> counter_;
    };

    class [[nodiscard]] RAD_EXPORT_VTABLE async_phaser::awaiter final
        : noncopyable,
          detail::async_op_base {
    public:
        awaiter(async_phaser& c, any_executor& ex) noexcept
            : detail::async_op_base(detail::async_op_type::wait), c_{c},
              ex_{ex} {
        }

        bool await_ready() const noexcept {
            return c_->is_ready();
        }

        bool await_suspend(std::coroutine_handle<> waiter) noexcept {
            waiter_ = waiter;
            if (!c_->value()) {
                return false;
            }
            c_->waiters_.add_wait_op(*ex_, *this);
            return true;
        }

        constexpr void await_resume() const noexcept {
        }

        virtual void invoke_operation() override {
            waiter_.resume();
        }

        any_executor& associated_executor() const noexcept override {
            return ex_;
        }

    private:
        ref<async_phaser> c_;
        ref<any_executor> ex_;
        std::coroutine_handle<> waiter_;
    };

    template <class Handler, class Alloc>
    struct async_phaser::wait_op : detail::async_op_base,
                                   allocator_storage<Alloc> {
        using alloc_base = allocator_storage<Alloc>;
        using handler_type = Handler;

        handler_type handler;
        ref<any_executor> ex_;

        wait_op(Handler&& handler, any_executor& ex, const Alloc& alloc)
            : detail::async_op_base(detail::async_op_type::wait),
              alloc_base(alloc), handler{std::forward<Handler>(handler)},
              ex_{ex} {
        }

        virtual void invoke_operation() override {
            detail::invoke_handler(this);
        }

        virtual any_executor& associated_executor() const noexcept override {
            return ex_;
        }
    };

    inline auto async_phaser::make_auto_counter() noexcept -> auto_decrement {
        return auto_decrement{*this};
    }

    inline auto async_phaser::async_wait() noexcept -> awaiter {
        return {*this, ex_};
    }

    inline auto async_phaser::operator co_await() noexcept -> awaiter {
        return {*this, ex_};
    }
} // namespace RAD_LIB_NAMESPACE