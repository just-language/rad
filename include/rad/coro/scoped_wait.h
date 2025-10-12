#pragma once
#include <rad/coro/awaitable_traits.h>
#include <rad/coro/task.h>
#include <rad/libbase.h>

#include <optional>

namespace RAD_LIB_NAMESPACE {
    /*!
     * @brief Decide if type Object has a method async_wait() that is
     * callable wihtout passing arguments, and returns an awaitable object
     * that can be awaited inside a task coroutine.
     */
    template <class Object>
    concept AsyncWaitableObject = requires(Object o) {
        { o.async_wait() } noexcept -> ValidAwaitable<int>;
    };

    /*!
     * @brief Construct a waitable object of type Object, call function
     * object @p fn passing a reference to object, await the result of
     * async_wait() method of object and return the result value of calling
     * @p fn.
     *
     * If @p fn throws an exception, then the exception is stored, object
     * awaited and after awaiting object the exception will be rethrown.
     *
     * This function can be used with strand and spawner to start many async
     * operations and waiting for all of them to finish, even in the face of
     * exceptions.
     * @tparam Object Type of a waitable object.
     * @tparam Fn Type of function object that is callable with Object&
     * argument and does not return an awaitable.
     * @tparam ...Args Type arguments to pass to Object constructor.
     * @param fn The function object that is callable with Object& argument
     * and does not return an awaitable.
     * @param ...args The arguments to pass to Object constructor.
     * @return A task that is when awaited will result in the return value
     * of calling @p fn with the waitable object.
     */
    template <AsyncWaitableObject Object, NonAwaitableFunctor<Object&> Fn,
              class... Args>
        requires std::constructible_from<Object, Args...>
    auto scoped_wait(Fn fn, Args&&... args)
        -> task<std::invoke_result_t<Fn, Object&>> {
        using fn_result = std::invoke_result_t<Fn, Object&>;
        constexpr bool returns_void = std::is_same_v<fn_result, void>;
        using result_t =
            std::conditional_t<returns_void, std::monostate, fn_result>;

        Object o{std::forward<Args>(args)...};
        std::optional<std::exception_ptr> ex_ptr;
        std::optional<result_t> result;

        try {
            if constexpr (returns_void) {
                fn(o);
            }
            else {
                result.emplace(fn(o));
            }
        }
        catch (...) {
            ex_ptr.emplace(std::current_exception());
        }

        co_await o.async_wait();
        if (ex_ptr) {
            std::rethrow_exception(*ex_ptr);
        }

        if constexpr (!returns_void) {
            co_return std::move(*result);
        }
    }

    /*!
     * @brief Construct a waitable object of type Object, call function
     * object @p fn passing a reference to object and await its result
     * awaitable, await the result of async_wait() method of object and
     * return the result value of awaiting the result of calling @p fn.
     *
     * If calling @p fn or awaiting its result throws an exception,
     * then the exception is stored, object awaited
     * and after awaiting object the exception will be rethrown.
     *
     * This function can be used with strand and spawner to start many async
     * operations and waiting for all of them to finish, even in the face of
     * exceptions.
     * @tparam Object Type of a waitable object.
     * @tparam Fn Type of function object that is callable with Object&
     * argument and returns an awaitable.
     * @tparam ...Args Type arguments to pass to Object constructor.
     * @param fn The function object that is callable with Object& argument
     * and returns an awaitable.
     * @param ...args The arguments to pass to Object constructor.
     * @return A task that is when awaited will result in the return value
     * of calling @p fn with the waitable object and awaiting its result.
     */
    template <AsyncWaitableObject Object, AwaitableFunctor<int, Object&> Fn,
              class... Args>
        requires std::constructible_from<Object, Args...>
    auto scoped_wait(Fn fn, Args&&... args)
        -> task<awaitable_result<std::invoke_result_t<Fn, Object&>>> {
        using fn_result = awaitable_result<std::invoke_result_t<Fn, Object&>>;
        constexpr bool returns_void = std::is_same_v<fn_result, void>;
        using result_t =
            std::conditional_t<returns_void, std::monostate, fn_result>;

        Object o{std::forward<Args>(args)...};
        std::optional<std::exception_ptr> ex_ptr;
        std::optional<result_t> result;

        try {
            if constexpr (returns_void) {
                co_await fn(o);
            }
            else {
                result.emplace(co_await fn(o));
            }
        }
        catch (...) {
            ex_ptr.emplace(std::current_exception());
        }

        co_await o.async_wait();
        if (ex_ptr) {
            std::rethrow_exception(*ex_ptr);
        }
        if constexpr (!returns_void) {
            co_return std::move(*result);
        }
    }
} // namespace RAD_LIB_NAMESPACE