#pragma once
#include <rad/async/async_phaser.h>
#include <rad/coro/task.h>

namespace RAD_LIB_NAMESPACE::detail {
    class forked_task_promise_base;
}

namespace RAD_LIB_NAMESPACE {
    /*!
     * @brief Type used for coroutines that starts immediately upon
     * calling the coroutine function and does not return a value.
     * It is similar to starting a thread and detaching it.
     * For most cases, use of task<T> with or without spwaner is better
     * than using forked_task.
     */
    class forked_task {
        friend class detail::forked_task_promise_base;

        forked_task() = default;
    };

    /*!
     * @brief Type used as a base class to work as a reference count
     * for started forked_task coroutine methods.
     * To used it: inherit publicly for this type, declare forked_task
     * coroutine methods and call them. Then await the async_phaser returned
     * from wait_tasks().
     */
    class task_counter : public noncopyable, public trackable {
    public:
        /*!
         * @brief Construct using a counter reference.
         * @param c Reference to the counter to use.
         * This counter must be valid as long as it is used by
         * this class.
         */
        task_counter(async_phaser& c) noexcept : counter{c} {
        }

        /*!
         * @brief Construct using a counter pointer.
         * @param c Pointer to the counter to use.
         * This counter must be valid as long as it is used by
         * this class.
         */
        task_counter(async_phaser* c) noexcept : counter{*c} {
        }

        /*!
         * @brief Increase the count of running tasks.
         * This method is called from the coroutine promise,
         * so don't call it directly.
         */
        void increase_task_counter() noexcept {
            counter.count_up();
        }

        /*!
         * @brief Decrease the count of running tasks.
         * This method is called from the coroutine promise,
         * so don't call it directly.
         * When the count reaches 0, the coroutines waiting on
         * wait_tasks() will be resumed.
         */
        void decrease_task_counter() noexcept {
            counter.count_down();
        }

        /*!
         * @brief Get the underlying counter reference to await
         * it and wait for coroutines finish.
         * @return Reference to the counter.
         */
        [[nodiscard]] async_phaser& wait_tasks() noexcept {
            return counter;
        }

    private:
        async_phaser& counter;
    };
} // namespace RAD_LIB_NAMESPACE

namespace RAD_LIB_NAMESPACE::detail {
    struct task_refc_base {
        template <class Class, class... Args>
        task_refc_base(Class& self, Args&&...) noexcept
            : base(static_cast<task_counter*>(&self)) {
            base->increase_task_counter();
        }

        ~task_refc_base() {
            base->decrease_task_counter();
        }

        pointer<task_counter> base = nullptr;
    };

    struct empty_refc_base {};

    template <class Class>
    using select_refc_base =
        std::conditional_t<std::is_convertible_v<Class*, task_counter*>,
                           task_refc_base, empty_refc_base>;

    // for non members with no allocation options
    class forked_task_promise_base {
    public:
        forked_task get_return_object() {
            return {};
        }

        constexpr std::suspend_never initial_suspend() noexcept {
            return {};
        }

        constexpr std::suspend_never final_suspend() noexcept {
            return {};
        }

        void unhandled_exception() noexcept {
#ifndef NDEBUG
            try {
                std::rethrow_exception(std::current_exception());
            }
            catch (const std::exception& ex) {
                printf("[!!] an exception was thrown and "
                       "not "
                       "caught "
                       "from forked_task : %s\n",
                       ex.what());
            }
            catch (...) {
                printf("[!!] an exception was thrown and "
                       "not "
                       "caught "
                       "from forked_task !\n");
            }
#endif // !NDEBUG
            std::terminate();
        }

        constexpr void return_void() const noexcept {
        }
    };

    // for members and non members with allocation options and ref counting
    template <class AllocBase, class Class = empty_base>
    class forked_task_promise : public forked_task_promise_base,
                                public select_refc_base<Class>,
                                public AllocBase {
        using refc_base = select_refc_base<Class>;
        using refc_base::refc_base;
    };
} // namespace RAD_LIB_NAMESPACE::detail

namespace std {
    // for forked_task non members with no allocation options
    template <class... Args>
    struct coroutine_traits<rad::forked_task, Args...> {
        using promise_type = rad::detail::forked_task_promise_base;
    };

    // for forked_task members function with no allocation options and
    // possibly ref counting
    template <class Class, class... Args>
    struct coroutine_traits<rad::forked_task, Class&, Args...> {
        using promise_type =
            rad::detail::forked_task_promise<rad::empty_base, Class>;
    };

    // for forked_task non members with leading allocation option
    template <class Allocator, class... Args>
    struct coroutine_traits<rad::forked_task, std::allocator_arg_t,
                            const Allocator&, Args...> {
        using promise_type = rad::detail::forked_task_promise<
            rad::detail::leading_alloc_operators<Allocator>>;
    };

    // for forked_task non members with leading allocation option
    template <class Allocator, class... Args>
    struct coroutine_traits<rad::forked_task, std::allocator_arg_t, Allocator&,
                            Args...> {
        using promise_type = rad::detail::forked_task_promise<
            rad::detail::leading_alloc_operators<Allocator>>;
    };

    // for forked_task non members with leading allocation option
    template <class Allocator, class... Args>
    struct coroutine_traits<rad::forked_task, std::allocator_arg_t, Allocator&&,
                            Args...> {
        using promise_type = rad::detail::forked_task_promise<
            rad::detail::leading_alloc_operators<Allocator>>;
    };

    // for forked_task members with leading allocation option and possibly
    // ref counting
    template <class Class, class Allocator, class... Args>
    struct coroutine_traits<rad::forked_task, Class&, std::allocator_arg_t,
                            const Allocator&, Args...> {
        using promise_type = rad::detail::forked_task_promise<
            rad::detail::leading_alloc_operators<Allocator>, Class>;
    };

    // for forked_task members with leading allocation option and possibly
    // ref counting
    template <class Class, class Allocator, class... Args>
    struct coroutine_traits<rad::forked_task, Class&, std::allocator_arg_t,
                            Allocator&, Args...> {
        using promise_type = rad::detail::forked_task_promise<
            rad::detail::leading_alloc_operators<Allocator>, Class>;
    };

    // for forked_task members with leading allocation option and possibly
    // ref counting
    template <class Class, class Allocator, class... Args>
    struct coroutine_traits<rad::forked_task, Class&, std::allocator_arg_t,
                            Allocator&&, Args...> {
        using promise_type = rad::detail::forked_task_promise<
            rad::detail::leading_alloc_operators<Allocator>, Class>;
    };

    // for forked_task non members with use_allocator option
    template <class Allocator, class... Args>
    struct coroutine_traits<rad::forked_task,
                            const rad::use_allocator<Allocator>&, Args...> {
        using promise_type = rad::detail::forked_task_promise<
            rad::detail::use_alloc_operators<Allocator>>;
    };

    // for forked_task non members with use_allocator option
    template <class Allocator, class... Args>
    struct coroutine_traits<rad::forked_task, rad::use_allocator<Allocator>&,
                            Args...> {
        using promise_type = rad::detail::forked_task_promise<
            rad::detail::use_alloc_operators<Allocator>>;
    };

    // for forked_task non members with use_allocator option
    template <class Allocator, class... Args>
    struct coroutine_traits<rad::forked_task, rad::use_allocator<Allocator>&&,
                            Args...> {
        using promise_type = rad::detail::forked_task_promise<
            rad::detail::use_alloc_operators<Allocator>>;
    };

    // for forked_task members with use_allocator option and possibly ref
    // counting
    template <class Allocator, class Class, class... Args>
    struct coroutine_traits<rad::forked_task, Class&,
                            const rad::use_allocator<Allocator>&, Args...> {
        using promise_type = rad::detail::forked_task_promise<
            rad::detail::use_alloc_operators<Allocator>, Class>;
    };

    // for forked_task members with use_allocator option and possibly ref
    // counting
    template <class Allocator, class Class, class... Args>
    struct coroutine_traits<rad::forked_task, Class&,
                            rad::use_allocator<Allocator>&, Args...> {
        using promise_type = rad::detail::forked_task_promise<
            rad::detail::use_alloc_operators<Allocator>, Class>;
    };

    // for forked_task members with use_allocator option and possibly ref
    // counting
    template <class Allocator, class Class, class... Args>
    struct coroutine_traits<rad::forked_task, Class&,
                            rad::use_allocator<Allocator>&&, Args...> {
        using promise_type = rad::detail::forked_task_promise<
            rad::detail::use_alloc_operators<Allocator>, Class>;
    };
} // namespace std