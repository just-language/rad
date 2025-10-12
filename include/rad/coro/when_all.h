#pragma once
#include <rad/coro/task.h>
#include <rad/libbase.h>

#include <atomic>
#include <vector>

namespace RAD_LIB_NAMESPACE {
    namespace detail {
        class wait_all_awaitable_base : noncopyable {
        public:
            wait_all_awaitable_base() noexcept = default;

            wait_all_awaitable_base(wait_all_awaitable_base&& other) noexcept
                : running_coros_{0},
                  waiter_{std::exchange(other.waiter_, nullptr)} {
            }

            wait_all_awaitable_base&
            operator=(wait_all_awaitable_base&& other) noexcept {
                waiter_ = std::exchange(other.waiter_,
                                        std::coroutine_handle<>{nullptr});
                running_coros_ = other.running_coros_.load();
                return *this;
            }

            std::size_t running_awaitables() const noexcept {
                return running_coros_;
            }

            void set_waiter(std::coroutine_handle<> waiter) {
                waiter_ = waiter;
            }

            void set_waiter(coroutine_final_suspend_observer& observer) {
                waiter_ = &observer;
            }

            std::coroutine_handle<> get_waiter_handle() const noexcept {
                if (auto h = std::get_if<std::coroutine_handle<>>(&waiter_)) {
                    return *h;
                }
                return std::get<coroutine_final_suspend_observer*>(waiter_)
                    ->get_waiter();
            }

        protected:
            std::atomic<std::size_t> running_coros_ = 0;
            std::variant<std::coroutine_handle<>,
                         coroutine_final_suspend_observer*>
                waiter_;
        };

        template <class T>
        struct wrap_awaitable_void_result {
            using type = T;
            using ref_type = T&;
        };

        template <>
        struct wrap_awaitable_void_result<void> {
            using type = std::monostate;
            using ref_type = std::monostate;
        };

        template <class T>
        using wrap_awaitable_void_result_t =
            typename wrap_awaitable_void_result<T>::type;

        template <class T>
        using wrap_awaitable_void_result_rt =
            typename wrap_awaitable_void_result<T>::ref_type;
    } // namespace detail

    template <class... T>
    class [[nodiscard]] wait_all_awaitable
        : public detail::wait_all_awaitable_base,
          detail::coroutine_final_suspend_observer {

        friend detail::awaitable_starter<wait_all_awaitable>;

    public:
        using value_type = std::tuple<
            detail::wrap_awaitable_void_result_t<typename T::value_type>...>;

        wait_all_awaitable(std::type_identity_t<T>&&... awaitables)
            : awaitables_{std::move(awaitables)...} {
        }

        static constexpr std::size_t awaitables_count() noexcept {
            return sizeof...(T);
        }

        constexpr bool await_ready() const noexcept {
            return awaitables_count() == 0;
        }

        void await_suspend(std::coroutine_handle<> waiter) {
            set_waiter(waiter);
            start_coros();
        }

        value_type await_resume() {
            auto result_error = get_error();
            if (result_error.has_value()) {
                std::rethrow_exception(*result_error);
            }
            return move_result();
        }

        std::tuple<T...> take_awaitables() & noexcept {
            return std::move(awaitables_);
        }

        std::tuple<T...>&& take_awaitables() && noexcept {
            return std::move(awaitables_);
        }

    private:
        // implement
        // coroutine_final_suspend_notification::get_waiter
        std::coroutine_handle<> get_waiter() override {
            std::size_t n = --running_coros_;
            if (!n) {
                return get_waiter_handle();
            }
            else {
                return std::noop_coroutine();
            }
        }

        void on_coroutine_has_unhandled_exception() noexcept override {
            std::apply(
                [](auto&&... awaitables) {
                    (..., (detail::cancel_one_awaitable(awaitables)));
                },
                awaitables_);
        }

        void start_coros() noexcept {
            // store running_coros_ before start_coros()
            // because coroutines may run to final suspend
            // and decrement the counter below 0
            running_coros_ = awaitables_count();
            // use
            // coroutine_final_suspend_notification::start_coro
            std::apply(
                [this](auto&&... awaitables) {
                    (..., detail::start_one_awaitable(awaitables, *this));
                },
                awaitables_);
        }

        void start_coro(coroutine_final_suspend_observer& observer) noexcept {
            set_waiter(observer);
            start_coros();
        }

        std::optional<std::exception_ptr> get_error() noexcept {
            assert(running_coros_ == 0);
            std::optional<std::exception_ptr> ex_ptr;
            std::apply(
                [&ex_ptr](auto&&... awaitables) {
                    return (... || detail::get_one_awaitable_error(awaitables,
                                                                   ex_ptr));
                },
                awaitables_);
            return ex_ptr;
        }

        value_type move_result() noexcept {
            return std::apply(
                [](auto&&... awaitables) {
                    return value_type{
                        detail::move_one_awaitable_result<std::monostate>(
                            awaitables)...};
                },
                awaitables_);
        }

        void cancel() noexcept {
            std::apply(
                [&](auto&&... awaitables) {
                    (..., detail::cancel_one_awaitable(awaitables));
                },
                awaitables_);
        }

        std::tuple<T...> awaitables_;
    };

    namespace detail {
        template <class... T>
        struct awaitable_starter<wait_all_awaitable<T...>> {
            static void
            start(wait_all_awaitable<T...>& a,
                  coroutine_final_suspend_observer& observer) noexcept {
                a.start_coro(observer);
            }

            static std::optional<std::exception_ptr>
            get_error(wait_all_awaitable<T...>& a) noexcept {
                return a.get_error();
            }

            static auto move_result(wait_all_awaitable<T...>& a) {
                return a.move_result();
            }

            static void cancel(wait_all_awaitable<T...>& a) noexcept {
                a.cancel();
            }
        };
    } // namespace detail

    template <class T>
    class [[nodiscard]] wait_all_awaitable_vec
        : public detail::wait_all_awaitable_base,
          detail::coroutine_final_suspend_observer {

        friend detail::awaitable_starter<wait_all_awaitable_vec>;

        using awaitable_value_type = typename T::value_type;

    public:
        using value_type =
            std::conditional_t<std::is_same_v<awaitable_value_type, void>, void,
                               std::vector<awaitable_value_type>>;

        wait_all_awaitable_vec(std::vector<T> awaitables)
            : awaitables_{std::move(awaitables)} {
        }

        std::size_t awaitables_count() const noexcept {
            return awaitables_.size();
        }

        std::vector<T> take_awaitables() & noexcept {
            return std::move(awaitables_);
        }

        std::vector<T>&& take_awaitables() && noexcept {
            return std::move(awaitables_);
        }

        void push_front(T&& t) {
            awaitables_.emplace(awaitables_.begin(), std::move(t));
        }

        void push_back(T&& t) {
            awaitables_.emplace_back(std::move(t));
        }

        void push_back(std::vector<T>&& awaitables) {
            awaitables_.insert(awaitables_.end(),
                               std::make_move_iterator(awaitables.begin()),
                               std::make_move_iterator(awaitables.end()));
        }

        constexpr bool await_ready() const noexcept {
            return awaitables_.empty();
        }

        void await_suspend(std::coroutine_handle<> waiter) {
            set_waiter(waiter);
            start_coros();
        }

        value_type await_resume() {
            auto result_error = get_error();
            if (result_error.has_value()) {
                std::rethrow_exception(*result_error);
            }
            return move_result();
        }

    private:
        // implement
        // coroutine_final_suspend_notification::get_waiter
        std::coroutine_handle<> get_waiter() override {
            std::size_t n = --running_coros_;
            if (!n) {
                return get_waiter_handle();
            }
            else {
                return std::noop_coroutine();
            }
        }

        void on_coroutine_has_unhandled_exception() noexcept override {
            for (auto& t : awaitables_) {
                t.cancel();
            }
        }

        void start_coros() noexcept {
            // store running_coros_ before start_coros()
            // because coroutines may run to final suspend
            // and decrement the counter below 0
            running_coros_ = awaitables_count();
            // use coroutine_final_suspend_notification::start_coro
            for (auto& a : awaitables_) {
                detail::start_one_awaitable(a, *this);
            }
        }

        void start_coro(coroutine_final_suspend_observer& observer) noexcept {
            set_waiter(observer);
            start_coros();
        }

        std::optional<std::exception_ptr> get_error() noexcept {
            std::optional<std::exception_ptr> ex_ptr;
            for (auto& a : awaitables_) {
                if (detail::get_one_awaitable_error(a, ex_ptr)) {
                    return ex_ptr;
                }
            }
            return std::nullopt;
        }

        value_type move_result() noexcept {
            if constexpr (std::is_same_v<value_type, void>) {
                return;
            }
            else {
                value_type result;
                result.reserve(awaitables_.size());
                for (auto& a : awaitables_) {
                    result.emplace_back(
                        detail::move_one_awaitable_result<void>(a));
                }
                return result;
            }
        }

        void cancel() noexcept {
            for (auto& a : awaitables_) {
                detail::cancel_one_awaitable(a);
            }
        }

        std::vector<T> awaitables_;
    };

    namespace detail {
        template <class T>
        struct awaitable_starter<wait_all_awaitable_vec<T>> {
            static void
            start(wait_all_awaitable_vec<T>& a,
                  coroutine_final_suspend_observer& observer) noexcept {
                a.start_coro(observer);
            }

            static std::optional<std::exception_ptr>
            get_error(wait_all_awaitable_vec<T>& a) noexcept {
                return a.get_error();
            }

            static auto move_result(wait_all_awaitable_vec<T>& a) {
                return a.move_result();
            }

            static void cancel(wait_all_awaitable_vec<T>& a) noexcept {
                a.cancel();
            }
        };
    } // namespace detail

    static_assert(
        ObservableAwaitable<wait_all_awaitable<task<int>>>,
        "wait_all_awaitable<task<T>> must satisfy ObservableAwaitable");
    static_assert(
        ObservableAwaitable<wait_all_awaitable<task<int>, task<bool>>>,
        "wait_all_awaitable<task<T>> must satisfy ObservableAwaitable");

    namespace detail {
        template <class... T>
        void validate_wait_all_awaitable(wait_all_awaitable<T...>&) {
        }

        template <class Awaitable>
        concept WaitAllAwaitableInstance =
            requires(Awaitable a) { detail::validate_wait_all_awaitable(a); };
    } // namespace detail

    /*!
     * @brief Decide if an Awaitable type is instance of
     * wait_all_awaitable<T...>.
     */
    template <class Awaitable>
    concept WaitAllAwaitable = ObservableAwaitable<Awaitable> &&
                               detail::WaitAllAwaitableInstance<Awaitable>;

    template <class Awaitable>
    concept ObservableNotWaitAllAwaitable =
        ObservableAwaitable<Awaitable> &&
        !detail::WaitAllAwaitableInstance<Awaitable>;

    /*!
     * @brief Combine @p awaitable1 with other @p awaitables in one
     * wait_all_awaitable that is when awaited will start the
     * awaitables, wait for all of them to finish and return their results in a
     * tuple.
     *
     * The result of awaiting the returned wait_all_awaitable
     * will be a tuple of types of awaitables results, where void results are
     * replaced with std::monostate.
     *
     * The awaitables will not start, until the returned awaitable is awaited.
     *
     * If any coroutine throws an exception, the rest of coroutines are
     * canceled which will cause them to throw on the first next co_await
     * point, then after all coroutines finish the exception of first in
     * order coroutine which has throwed will be rethrown.
     *
     * When all the awaitables finish, the awaiting coroutine will resume on the
     * executor the last awaitable finished on.
     * @tparam ...Awaitables Types of @p awaitables. Each type must be an
     * instance of task.
     * @tparam T Type of value of task.
     * @param awaitable1 The first awaitable.
     * @param awaitable2 The second awaitable.
     * @param ...awaitables The other awaitables.
     * @return A wait_all_awaitable that combines the first, second and
     * other @p awaitables. When awaited it will start each awaitable in order,
     * then wait for the result of all of them.
     */
    template <ObservableAwaitable A1, ObservableAwaitable A2,
              ObservableAwaitable... Awaitables>
    auto when_all(A1&& awaitable1, A2&& awaitable2, Awaitables&&... awaitables)
        -> wait_all_awaitable<A1, A2, Awaitables...> {
        return wait_all_awaitable<A1, A2, Awaitables...>{
            std::move(awaitable1), std::move(awaitable2),
            std::forward<Awaitables>(awaitables)...};
    }

    /*!
     * @brief Combine @p awaitables in one wait_all_awaitable_vec awaitable that
     * is when awaited will start the awaitables, wait for all of them to finish
     * and return their results in a vector.
     *
     * The result of awaiting the returned wait_all_awaitable_vec
     * will be a vector of types of awaitables results, where void results are
     * replaced with std::monostate.
     *
     * The awaitables will not start, until the returned awaitable is awaited.
     *
     * If any coroutine throws an exception, the rest of coroutines are
     * canceled which will cause them to throw on the first next co_await
     * point, then after all coroutines finish the exception of first in
     * order coroutine which has throwed will be rethrown.
     *
     * When all the awaitables finish, the awaiting coroutine will resume on the
     * executor the last awaitable finished on.
     * @tparam T Type of value of task.
     * @param awaitables The awaitables to wait for them all.
     * @return A wait_all_awaitable_vec awaitable that combines the
     * @p awaitables. When awaited it will start each awaitable in order, then
     * wait for the result of all of them.
     */
    template <ObservableAwaitable T>
    auto when_all(std::vector<T>&& awaitables) -> wait_all_awaitable_vec<T> {
        return wait_all_awaitable_vec<T>{std::move(awaitables)};
    }

    /*!
     * @brief Combine @p awaitable1 and @p awaitable2 in one wait_all_awaitable
     * awaitable. This is the same as calling when_all(std::move(awaitable1),
     * std::move(awaitable2)).
     * @tparam T1 Type of value of @p task1.
     * @tparam T2 Type of value of @p task2.
     * @param awaitable1 The first awaitable.
     * @param awaitable2 The second awaitable.
     * @return A wait_all_awaitable.
     */
    template <ObservableAwaitable A1, ObservableAwaitable A2>
    wait_all_awaitable<A1, A2> operator&&(A1&& awaitable1, A2&& awaitable2) {
        return wait_all_awaitable<A1, A2>{std::move(awaitable1),
                                          std::move(awaitable2)};
    }

    /*!
     * @brief Combine @p awaitable1 and the awaitables from @p awaitables in one
     * wait_all_awaitable. After call to this operator, awaitables has
     * no awaitables in it.
     * @tparam ...Awaitables Types of awaitables in @p awaitables.
     * Each type must be an instance of task.
     * @tparam T Type of value of @p task1.
     * @param awaitable1 The first awaitable.
     * @param awaitables The other awaitables.
     * @return A wait_all_awaitable.
     */
    template <ObservableNotWaitAllAwaitable A1,
              ObservableAwaitable... Awaitables>
    wait_all_awaitable<A1, Awaitables...>
    operator&&(A1&& awaitable1,
               wait_all_awaitable<Awaitables...>&& awaitables) {
        return std::apply(
            [&](auto&&... awaitables) {
                return wait_all_awaitable<A1, Awaitables...>{
                    std::move(awaitable1), std::move(awaitables)...};
            },
            std::move(awaitables).take_awaitables());
    }

    /*!
     * @brief Combine the awaitables from @p awaitables and @p awaitable1 in one
     * wait_all_awaitable. After call to this operator, awaitables has
     * no awaitables in it.
     * @tparam ...Awaitables Types of awaitables in @p awaitables.
     * Each type must be an instance of task.
     * @tparam T Type of value of @p task2.
     * @param awaitables The first awaitables.
     * @param awaitable2 The last awaitable.
     * @return A wait_all_awaitable.
     */
    template <ObservableNotWaitAllAwaitable A1, class... Awaitables>
    wait_all_awaitable<Awaitables..., A1>
    operator&&(wait_all_awaitable<Awaitables...>&& awaitables,
               A1&& awaitable2) {
        return std::apply(
            [&](auto&&... awaitables) {
                return wait_all_awaitable<Awaitables..., A1>{
                    std::move(awaitables)..., std::move(awaitable2)};
            },
            std::move(awaitables).take_awaitables());
    }

    /*!
     * @brief Combine awaitables of @p awaitable1 and @p awaitable2 in one
     * wait_all_awaitable. After call to this operator, @p
     * awaitable1 and
     * @p awaitable2 has no awaitables in either of them.
     * @tparam Awaitable2 Instance of wait_all_awaitable.
     * @tparam Awaitable1 Instance of wait_all_awaitable.
     * @param awaitable1 The first wait_all_awaitable.
     * @param awaitable2 The second wait_all_awaitable.
     * @return A wait_all_awaitable.
     */
    template <WaitAllAwaitable Awaitable1, WaitAllAwaitable Awaitable2>
    auto operator&&(Awaitable1&& awaitable1, Awaitable2&& awaitable2) {
        return std::apply(
            [&](auto&&... awaitables1) {
                return std::apply(
                    [&](auto&&... awaitables2) {
                        return wait_all_awaitable<
                            std::decay_t<decltype(awaitables1)>...,
                            std::decay_t<decltype(awaitables2)>...>{
                            std::move(awaitables1)...,
                            std::move(awaitables2)...};
                    },
                    awaitable2.take_awaitables());
            },
            awaitable1.take_awaitables());
    }

    /*!
     * @brief Combine @p awaitable1 and the awaitables from @p awaitables in one
     * wait_all_awaitable_vec awaitable. After call to this operator, @p
     * awaitables has no awaitables in it.
     * @tparam T Type of value of the awaitables.
     * @param awaitable1 The first awaitable.
     * @param awaitables The other awaitables.
     * @return A wait_all_awaitable_vec awaitable.
     */
    template <ObservableAwaitable A>
    wait_all_awaitable_vec<A>
    operator&&(A&& awaitable1, wait_all_awaitable_vec<A>&& awaitables) {
        awaitables.push_front(std::move(awaitable1));
        return std::move(awaitables);
    }

    /*!
     * @brief Combine the awaitables from @p awaitables and @p awaitable1 in one
     * wait_all_awaitable_vec awaitable. After call to this operator, @p
     * awaitables has no awaitables in it.
     * @tparam T Type of value of the awaitables.
     * @param awaitables The first awaitables.
     * @param awaitable2 The last awaitable.
     * @return A wait_all_awaitable_vec awaitable.
     */
    template <ObservableAwaitable A>
    wait_all_awaitable_vec<A> operator&&(wait_all_awaitable_vec<A>&& awaitables,
                                         A&& awaitable2) {
        awaitables.push_front(std::move(awaitable2));
        return std::move(awaitables);
    }

    /*!
     * @brief Combine awaitables of @p awaitables1 and @p awaitables2 in one
     * wait_all_awaitable_vec awaitable. After call to this operator, @p
     * awaitables1 and @p awaitables2 has no awaitables in either of them.
     * @tparam T Type of value of the awaitables.
     * @param awaitables1 The first awaitables.
     * @param awaitables2 The last awaitables.
     * @return A wait_all_awaitable_vec awaitable.
     */
    template <ObservableAwaitable A>
    wait_all_awaitable_vec<A>
    operator&&(wait_all_awaitable_vec<A>&& awaitable1,
               wait_all_awaitable_vec<A>&& awaitable2) {
        awaitable1.push_back(std::move(awaitable2).take_awaitables());
        return std::move(awaitable1);
    }
} // namespace RAD_LIB_NAMESPACE