#pragma once
#include <rad/async/timer.h>
#include <rad/coro/awaitable_traits.h>
#include <rad/coro/spawn.h>
#include <rad/coro/task.h>
#include <rad/coro/when_all.h>

#include <atomic>
#include <chrono>

namespace RAD_LIB_NAMESPACE {
    namespace detail {
        template <class Rep, class Period, class CancelFn>
        task<> timeout_wait_on_timer(timer& t,
                                     const std::atomic<bool>& cancel_flag,
                                     std::chrono::duration<Rep, Period> timeout,
                                     CancelFn cancel_fn) {
            std::error_code ec;
            co_await t.wait_for(timeout, ec);
            if (ec || cancel_flag) {
                co_return;
            }
            cancel_fn();
        };

        template <class Fn, class... Args>
        auto timeout_execute_fn(timer& t, std::atomic<bool>& cancel_flag, Fn f,
                                Args&&... args)
            -> task<awaitable_result<std::invoke_result_t<Fn, Args...>>> {
            auto on_exit = scope_exit([&] {
                cancel_flag = true;
                t.cancel();
            });
            co_return co_await f(std::forward<Args>(args)...);
        };

        template <class T>
        task<T> timeout_execute_task(timer& t, std::atomic<bool>& cancel_flag,
                                     task<T> task_fn) {
            auto on_exit = scope_exit([&] {
                cancel_flag = true;
                t.cancel();
            });
            if constexpr (!std::is_same_v<T, void>) {
                co_return co_await task_fn;
            }
            else {
                co_await task_fn;
            }
        };
    } // namespace detail

    /*!
     * @brief Await an awaitable returned by @p f with timeout.
     *
     * If @p timeout passes before the awaitable finishes, @p cancel_fn is
     * called.
     *
     * The awaitable starts on the current thread, the return awaitable was
     * awaited on. When the execution finishes the current awaiting
     * coroutine resumes on the thread and executor the awaitable finished
     * on.
     *
     * The execution and timeout setting does not start until the returned
     * task is awaited.
     * @tparam Fn Type of function that returns the awaitable.
     * @tparam ...Args Type of function arguments.
     * @tparam CancelFn Type of cancel function.
     * @param ex Timer executor used for waiting for timeout.
     * @param timeout The timeout durration.
     * @param cancel_fn The function that will be called if @p timeout
     * passes before the awaitable finishes.
     * @param f The function that returns the awaitable when called with @p
     * args.
     * @param ...args The arguments to @p f.
     * @return A task that is when awaited will set the timeout, start
     * execution of the awaitable. The result of awaiting the task will be
     * the result of the awaitable.
     */
    template <class Rep, class Period, std::invocable CancelFn, class Fn,
              class... Args>
        requires AwaitableFunctor<Fn, int, Args...>
    auto execute_timeout(timer_executor& ex,
                         std::chrono::duration<Rep, Period> timeout,
                         CancelFn cancel_fn, Fn f, Args&&... args)
        -> task<awaitable_result<std::invoke_result_t<Fn, Args...>>> {
        timer t{ex};
        std::atomic<bool> cancel_flag{false};
        auto res = co_await when_all(
            detail::timeout_wait_on_timer(t, cancel_flag, timeout,
                                          std::move(cancel_fn)),
            detail::timeout_execute_fn(t, cancel_flag, std::move(f),
                                       std::forward<Args>(args)...));
        using result_t = awaitable_result<std::invoke_result_t<Fn, Args...>>;
        if constexpr (!std::is_same_v<result_t, void>) {
            co_return std::get<1>(res);
        }
    }

    /*!
     * @brief Await a task @p task_fn with timeout.
     *
     * If @p timeout passes before the task finishes, @p cancel_fn is
     * called.
     *
     * The task starts on the current thread, the return awaitable was
     * awaited on. When the execution finishes the current awaiting
     * coroutine resumes on the thread and executor the task finished on.
     *
     * The execution and timeout setting does not start until the returned
     * task is awaited.
     * @tparam CancelFn Type of cancel function.
     * @tparam T Type of task value.
     * @param ex Timer executor used for waiting for timeout.
     * @param timeout The timeout durration.
     * @param task_fn The task awaitable to await.
     * @param cancel_fn The function that will be called if @p timeout
     * passes
     * @return A task that is when awaited will set the timeout, start
     * execution of @p task_fn. The result of awaiting the task will be the
     * result of the awaitable.
     */
    template <class Rep, class Period, std::invocable CancelFn, TaskValue T>
    task<T> execute_timeout(timer_executor& ex,
                            std::chrono::duration<Rep, Period> timeout,
                            task<T> task_fn, CancelFn cancel_fn) {
        timer t{ex};
        std::atomic<bool> cancel_flag{false};
        auto res = co_await when_all(
            detail::timeout_wait_on_timer(t, cancel_flag, timeout,
                                          std::move(cancel_fn)),
            detail::timeout_execute_task(t, cancel_flag, std::move(task_fn)));
        if constexpr (!std::is_same_v<T, void>) {
            co_return std::get<1>(res);
        }
    }
} // namespace RAD_LIB_NAMESPACE