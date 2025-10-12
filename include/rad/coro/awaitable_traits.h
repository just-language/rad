#pragma once
#include <rad/libbase.h>

#include <coroutine>
#include <memory>

namespace RAD_LIB_NAMESPACE::detail {
    struct invalid_awaiter_result_t {};

    template <class Promise, class Awaitable>
    concept has_await_transform = requires(Promise&& p, Awaitable&& a) {
        p.await_transform(std::forward<Awaitable>(a));
    };

    template <class Awaitable>
    concept has_co_await_member_operator =
        requires(Awaitable&& a) { a.operator co_await(); };

    template <class Awaitable>
    concept has_co_await_free_operator = requires(Awaitable&& a) {
        operator co_await(static_cast<Awaitable&&>(a));
    };

    template <class T>
    struct is_coroutine_handle_instance {
        static constexpr bool value = false;
    };

    template <class P>
    struct is_coroutine_handle_instance<std::coroutine_handle<P>> {
        static constexpr bool value = true;
    };

    template <class T>
    concept await_suspend_result =
        std::same_as<T, void> || std::same_as<T, bool> ||
        is_coroutine_handle_instance<T>::value;

    struct dummy_awaiter {
        bool await_ready() const noexcept {
            return true;
        }

        void await_suspend(std::coroutine_handle<>) {
        }

        bool await_resume() {
            return false;
        }

        dummy_awaiter operator co_await() {
            return {};
        }
    };

    template <class Awaitable>
    concept noexcept_awaiter =
        requires(Awaitable&& a, std::coroutine_handle<> h) {
            { a.await_ready() } noexcept -> std::same_as<bool>;
            { a.await_suspend(h) } noexcept -> await_suspend_result;
            { a.await_resume() } noexcept;
        };

    template <class Awaitable>
    concept is_awaiter = requires(Awaitable&& a, std::coroutine_handle<> h) {
        { a.await_ready() } -> std::same_as<bool>;
        { a.await_suspend(h) } -> await_suspend_result;
        a.await_resume();
    };

    template <is_awaiter Awaitable>
    decltype(auto) get_await_resume_result(Awaitable&& a) {
        return std::forward<Awaitable>(a).await_resume();
    }

    template <class T>
    void get_await_resume_result(T&&) {
    }

    template <has_co_await_free_operator Awaitable>
    decltype(auto) get_free_co_await_result(Awaitable&& a) {
        return operator co_await(static_cast<Awaitable&&>(a));
    }

    template <class T>
    dummy_awaiter get_free_co_await_result(T&&) {
        return {};
    }

    template <has_co_await_member_operator Awaitable>
    decltype(auto) get_member_co_await_result(Awaitable&& a) {
        return std::forward<Awaitable>(a).operator co_await();
    }

    template <class T>
    dummy_awaiter get_member_co_await_result(T&&) {
        return {};
    }

    template <class Promise, class Awaitable>
    decltype(auto) get_await_transform_result(Promise&& p, Awaitable&& a) {
        if constexpr (has_await_transform<Promise, Awaitable>) {
            return std::forward<Promise>(p).await_transform(
                std::forward<Awaitable>(a));
        }
        else {
            return std::forward<Awaitable>(a);
        }
    }

    template <class Promise, class Awaitable>
    using find_awaitable_type =
        std::conditional_t<has_await_transform<Promise, Awaitable>,
                           decltype(get_await_transform_result(
                               std::declval<Promise>(),
                               std::declval<Awaitable>())),
                           Awaitable>;

    template <class Awaitable>
    using awaiter_from_member_co_await =
        decltype(get_member_co_await_result(std::declval<Awaitable>()));

    template <class Awaitable>
    using awaiter_from_free_co_await =
        decltype(get_free_co_await_result(std::declval<Awaitable>()));

    template <class Awaitable>
    using awaitable_result3 =
        std::conditional_t<is_awaiter<Awaitable>,
                           decltype(get_await_resume_result(
                               std::declval<Awaitable>())),
                           invalid_awaiter_result_t>;

    template <class Awaitable>
    using awaitable_result2 = std::conditional_t<
        has_co_await_free_operator<Awaitable>,
        awaitable_result3<awaiter_from_free_co_await<Awaitable>>,
        awaitable_result3<Awaitable>>;

    template <class Awaitable>
    using awaitable_result1 = std::conditional_t<
        has_co_await_member_operator<Awaitable>,
        awaitable_result3<awaiter_from_member_co_await<Awaitable>>,
        awaitable_result2<Awaitable>>;
} // namespace RAD_LIB_NAMESPACE::detail

namespace RAD_LIB_NAMESPACE {
    /*!
     * @brief Find the result type of awaiting an awaitable inside a
     * coroutine.
     * @tparam Awaitable The awaitable type.
     * @tparam Promise The promise type of the coroutine which its
     * await_transform(), if it has one, may change the type of the
     * awaitable. This can be set to int to ignore the promise type.
     */
    template <class Awaitable, class Promise = int>
    using awaitable_result = detail::awaitable_result1<
        detail::find_awaitable_type<Promise, Awaitable>>;

    /*!
     * @brief Decide if Awaitable is a valid awaitable that can be awaited
     * inside a coroutine whose promise_type is Promise.
     */
    template <class Awaitable, class Promise>
    concept ValidAwaitable = !std::same_as<awaitable_result<Awaitable, Promise>,
                                           detail::invalid_awaiter_result_t>;

    /*!
     * @brief Decide if Fn when called with Args... returns a valid
     * awaitable that can be awaited inside a coroutine whose promise_type
     * is Promise and the result of awaiting the awaitable is not void.
     */
    template <class Fn, class Promise, class... Args>
    concept NonVoidAwaitableFunctor = requires(Fn fn, Args&&... args) {
        { fn(std::forward<Args>(args)...) } -> ValidAwaitable<Promise>;
        requires !std::same_as<decltype(detail::get_await_resume_result(fn())),
                               void>;
    };

    /*!
     * @brief Decide if Fn when called with Args... returns a valid
     * awaitable that can be awaited inside a coroutine whose promise_type
     * is Promise and the result of awaiting the awaitable is void.
     */
    template <class Fn, class Promise, class... Args>
    concept VoidAwaitableFunctor = requires(Fn fn, Args&&... args) {
        { fn(std::forward<Args>(args)...) } -> ValidAwaitable<Promise>;
        { detail::get_await_resume_result(fn()) } -> std::same_as<void>;
    };

    /*!
     * @brief Decide if Fn when called with Args... returns a valid
     * awaitable that can be awaited inside a coroutine whose promise_type
     * is Promise.
     */
    template <class Fn, class Promise, class... Args>
    concept AwaitableFunctor = requires(Fn fn, Args&&... args) {
        { fn(std::forward<Args>(args)...) } -> ValidAwaitable<Promise>;
    };

    /*!
     * @brief Decide if Fn when called with Args... wiil not return a valid
     * awaitable that can be awaited inside a coroutine whose promise_type
     * is Promise.
     */
    template <class Fn, class... Args>
    concept NonAwaitableFunctor = requires(Fn fn, Args&&... args) {
        fn(std::forward<Args>(args)...);
        requires !ValidAwaitable<std::invoke_result_t<Fn, Args...>, int>;
    };
} // namespace RAD_LIB_NAMESPACE