#pragma once
#include <rad/coro/awaitable_traits.h>
#include <rad/coro/task.h>

#include <optional>

namespace RAD_LIB_NAMESPACE {
    /*!
     * @brief Tag scheduler type used to bypass schedule.
     */
    struct current_scheduler_t {
        constexpr std::suspend_never schedule() const noexcept {
            return {};
        }
    };

    /*!
     * @brief A scheduler whose schedule method is a no-op used with
     * execute(ex, back_ex, fn) to execute on the current thread then return
     * on another executor.
     */
    inline current_scheduler_t current_scheduler;

    /*!
     * @brief Decide if a type is scheduler which means
     * it has a method schedule() that is noeacept and returns an awaitable.
     */
    template <class Exec>
    concept Scheduler = requires(Exec& ex) {
        { ex.schedule() } noexcept -> ValidAwaitable<int>;
    };

    /*!
     * @brief Execute a non awaitable function on scheduler @p ex.
     * The awaiting coroutine will resume on scheduler @p ex.
     * The scheduling and execution will not start until the returned
     * task is awaited.
     * @tparam Exec The type of scheduler.
     * @tparam Fn The type of function to execute.
     * @param ex The scheduler to execute on.
     * @param fn The function to execute.
     * @return A task that is when awaited will switch to the scheduler
     * and execute @p fn. The result of awaiting the task is the result of
     * @p fn.
     */
    template <Scheduler Exec, NonAwaitableFunctor Fn>
    auto execute(Exec& ex, Fn fn) -> task<std::invoke_result_t<Fn>> {
        co_await ex.schedule();
        co_return fn();
    }

    /*!
     * @brief Execute a non awaitable function on scheduler @p ex.
     * The awaiting coroutine will resume on scheduler @p back_ex.
     * The scheduling and execution will not start until the returned
     * task is awaited.
     * @tparam Exec The type of execution scheduler.
     * @tparam Exec2 The type of resume scheduler.
     * @tparam Fn The type of function to execute.
     * @param ex The scheduler to execute on.
     * @param back_ex The scheduler to resume on.
     * @param fn The function to execute.
     * @return A task that is when awaited will switch to @p ex,
     * execute @p fn and switch @p back_ex. The result of awaiting the task
     * is the result of @p fn.
     */
    template <Scheduler Exec, Scheduler Exec2, NonAwaitableFunctor Fn>
    auto execute(Exec& ex, Exec2& back_ex, Fn fn)
        -> task<std::invoke_result_t<Fn>> {
        co_await ex.schedule();

        constexpr bool returns_void =
            std::is_same_v<std::invoke_result_t<Fn>, void>;

        std::optional<std::exception_ptr> ex_ptr;
        std::optional<std::conditional_t<returns_void, std::monostate,
                                         std::invoke_result_t<Fn>>>
            result;

        try {
            if constexpr (returns_void) {
                fn();
            }
            else {
                result.emplace(fn());
            }
        }
        catch (...) {
            ex_ptr.emplace(std::current_exception());
        }

        co_await back_ex.schedule();

        if (ex_ptr) {
            std::rethrow_exception(*ex_ptr);
        }

        if constexpr (!returns_void) {
            co_return std::move(*result);
        }
    }

    /*!
     * @brief Await the awaitable returned by @p fn on scheduler @p ex.
     * The awaiting coroutine will resume on scheduler @p ex.
     * The scheduling and execution will not start until the returned
     * task is awaited.
     * @tparam Exec The type of scheduler.
     * @tparam Fn The type of function to call and await its result.
     * @param ex The scheduler to execute on.
     * @param fn The function to call and await its awaitable result.
     * @return A task that is when awaited will switch to the scheduler,
     * call @p fn and await its result.
     * The result of awaiting the task is the result of awaiting the result
     * of @p fn.
     */
    template <Scheduler Exec, AwaitableFunctor<int> Fn>
    auto execute(Exec& ex, Fn fn)
        -> task<awaitable_result<std::invoke_result_t<Fn>>> {
        co_await ex.schedule();
        co_return co_await fn();
    }

    /*!
     * @brief Await the awaitable returned by @p fn on scheduler @p ex.
     * The awaiting coroutine will resume on scheduler @p back_ex.
     * The scheduling and execution will not start until the returned
     * task is awaited.
     * @tparam Exec The type of execution scheduler.
     * @tparam Exec2 The type of resume scheduler.
     * @tparam Fn The type of function to call and await its result.
     * @param ex The scheduler to execute on.
     * @param back_ex The scheduler to resume on.
     * @param fn The function to call and await its awaitable result.
     * @return A task that is when awaited will switch to the scheduler,
     * call @p fn, await its result and switch back to @p back_ex.
     * The result of awaiting the task is the result of awaiting the result
     * of @p fn.
     */
    template <Scheduler Exec, Scheduler Exec2, AwaitableFunctor<int> Fn>
    auto execute(Exec& ex, Exec2& back_ex, Fn fn)
        -> task<awaitable_result<std::invoke_result_t<Fn>>> {
        co_await ex.schedule();

        using result_t = awaitable_result<std::invoke_result_t<Fn>>;
        constexpr bool returns_void = std::is_same_v<result_t, void>;

        std::optional<std::exception_ptr> ex_ptr;
        std::optional<
            std::conditional_t<returns_void, std::monostate, result_t>>
            result;

        try {
            if constexpr (returns_void) {
                co_await fn();
            }
            else {
                result.emplace(co_await fn());
            }
        }
        catch (...) {
            ex_ptr.emplace(std::current_exception());
        }

        co_await back_ex.schedule();

        if (ex_ptr) {
            std::rethrow_exception(*ex_ptr);
        }

        if constexpr (!returns_void) {
            co_return std::move(*result);
        }
    }
} // namespace RAD_LIB_NAMESPACE