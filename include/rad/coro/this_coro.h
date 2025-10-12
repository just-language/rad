#pragma once
#include <rad/async/timer.h>
#include <rad/coro/task.h>

namespace RAD_LIB_NAMESPACE::this_coro {
    class [[nodiscard]] get_id_awaitable {
    public:
        constexpr bool await_ready() const noexcept {
            return false;
        }

        bool await_suspend(std::coroutine_handle<> coro) noexcept {
            this_coro_ = coro;
            return false;
        }

        std::coroutine_handle<> await_resume() const noexcept {
            return this_coro_;
        }

    private:
        std::coroutine_handle<> this_coro_;
    };

    template <Executor Exec>
    class yield_awaitable;

    template <Executor Exec>
    yield_awaitable<Exec> yield(Exec& ex) noexcept;

    template <Executor Exec>
    class [[nodiscard]] yield_awaitable final : detail::async_op_base, pinned {
        Exec& ex_;
        std::coroutine_handle<> waiter_;

        friend yield_awaitable<Exec> yield<Exec>(Exec& ex) noexcept;

        // yield_awaitable must be allocated on the stack
        yield_awaitable(Exec& ex) noexcept
            : detail::async_op_base(detail::async_op_type::yield), ex_{ex} {
        }

    public:
        constexpr bool await_ready() const noexcept {
            return false;
        }

        constexpr void await_resume() const noexcept {
        }

        void await_suspend(std::coroutine_handle<> coro) noexcept {
            waiter_ = coro;
            ex_.post(*this);
        }

    private:
        virtual void invoke_operation() override {
            waiter_.resume();
        }

        virtual any_executor& associated_executor() const noexcept override {
            return ex_;
        }
    };

    class [[nodiscard]] sleep_awaitable final : pinned {
        timer t_;
        std::array<std::uint8_t, timer::wait_allocator_size<sizeof(
                                     std::coroutine_handle<>)>()>
            alloc_buff_;

        template <class Rep, class Period>
        friend sleep_awaitable
        sleep_for(timer_executor& ex,
                  std::chrono::duration<Rep, Period> relative_time) noexcept;

        template <class Clock, class Duration>
        friend sleep_awaitable
        sleep_until(timer_executor& ex,
                    std::chrono::time_point<Clock, Duration> abs_time) noexcept;

        template <class Rep, class Period>
        sleep_awaitable(
            timer_executor& ex,
            std::chrono::duration<Rep, Period> relative_time) noexcept
            : t_{ex, relative_time} {
        }

        template <class Clock, class Duration>
        sleep_awaitable(
            timer_executor& ex,
            std::chrono::time_point<Clock, Duration> abs_time) noexcept
            : t_{ex, abs_time} {
        }

    public:
        constexpr bool await_ready() const noexcept {
            return false;
        }

        void await_suspend(std::coroutine_handle<> coro) noexcept {
            t_.async_wait(
                [coro](const std::error_code& ec) {
                    assert(!ec);
                    coro.resume();
                },
                static_buffer_allocator(alloc_buff_));
        }

        constexpr void await_resume() const noexcept {
        }
    };

    /*!
     * @brief Gets the handle of the current coroutine. Note that this is
     * for debugging and testing purposes only so don't miss with the
     * coroutine handle!
     * @returns an awaitable that is when awaited will return the awaiting
     * coroutine handle without suspension
     */
    get_id_awaitable get_handle() noexcept {
        return {};
    }

    /*!
     * @brief Suspend the current coroutine for some time using non cancelable
     * timer.
     * @param ex the executor to sleep on and on which this coroutine will
     * resume after sleep
     * @param relative_time the duration to sleep for
     * @return sleep awaitable that is when awaited will perform sleep
     */
    template <class Rep, class Period>
    inline sleep_awaitable
    sleep_for(timer_executor& ex,
              std::chrono::duration<Rep, Period> relative_time) noexcept {
        return sleep_awaitable{ex, relative_time};
    }

    /*!
     * @brief Suspend the current coroutine until a time point is reached
     * using non cancelable timer.
     * @param ex the executor to sleep on and on which this coroutine will
     * resume after sleep
     * @param abs_time the time point to sleep until
     * @return sleep awaitable that is when awaited will perform sleep
     */
    template <class Clock, class Duration>
    inline sleep_awaitable
    sleep_until(timer_executor& ex,
                std::chrono::time_point<Clock, Duration> abs_time) noexcept {
        return sleep_awaitable{ex, abs_time};
    }

    /*!
     * @brief Suspend the coroutine and yield the execution to the executor
     * to give other async operations a chance to run. If this is the only
     * coroutine on the excecutor it will suspend then be resumed afterward
     * @param ex the executor to yield the execution to and also on which
     * this coroutine will resume after suspension
     * @return a yield awaitable that is when awaited will perform yield
     */
    template <Executor Exec>
    inline yield_awaitable<Exec> yield(Exec& ex) noexcept {
        return yield_awaitable{ex};
    }
} // namespace RAD_LIB_NAMESPACE::this_coro