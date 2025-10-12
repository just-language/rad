#pragma once
#include <rad/libbase.h>

#include <cassert>
#include <coroutine>
#include <memory>
#include <optional>
#include <utility>
#include <variant>
#include <atomic>

namespace RAD_LIB_NAMESPACE {
    /*!
     * @brief Requirement of types of values used by tasks.
     * Must be either void, movable type or copyable.
     */
    template <class T>
    concept TaskValue = std::same_as<T, void> || std::move_constructible<T> ||
                        std::copy_constructible<T>;

    template <class T>
    class task;

    /*!
     * @brief Wrapper around allocator to use custom allocators with
     * coroutines.
     * @tparam Allocator The type of allocator.
     */
    template <class Allocator>
    class alignas(16) use_allocator {
        using alloc_traits = std::allocator_traits<Allocator>;
        using value_type = typename alloc_traits::value_type;
        using pointer = typename alloc_traits::pointer;

    public:
        /*!
         * @brief Copy @p alloc to the stored allocator.
         * @param alloc The allocator to copy.
         */
        use_allocator(const Allocator& alloc) : alloc_{alloc} {
        }

        /*!
         * @brief Move @p alloc to the stored allocator.
         * @param alloc The allocator to move.
         */
        use_allocator(Allocator&& alloc) : alloc_{std::move(alloc)} {
        }

        /*!
         * @brief Get a reference to the underlying allocator.
         * @return Reference to the underlying allocator.
         */
        Allocator& get_allocator() noexcept {
            return alloc_;
        }

        /*!
         * @brief Get a const reference to the underlying
         * allocator.
         * @return Const reference to the underlying allocator.
         */
        const Allocator& get_allocator() const noexcept {
            return alloc_;
        }

        /*!
         * @brief Allocate count of bytes using the underlying
         * allocator.
         * @param n_bytes Count of bytes to allocate.
         * @return Pointer to the allocated memory.
         */
        uint8_t* allocate_bytes(std::size_t n_bytes) {
            std::size_t nvalues = n_bytes;
            if constexpr (sizeof(value_type) > 1) {
                nvalues = (n_bytes / sizeof(value_type)) +
                          (n_bytes % sizeof(value_type) != 0);
            }
            pointer p = alloc_traits::allocate(alloc_, nvalues);
            return reinterpret_cast<uint8_t*>(p);
        }

        /*!
         * @brief Deallocate previously allocated memory.
         * @param p Pointer to the previously allocated memory
         * returned from allocate_bytes().
         * @param n_bytes Count of bytes to deallocate.
         * Must be the same as the count that was passed to
         * allocate_bytes().
         */
        void deallocate_bytes(void* p, std::size_t n_bytes) noexcept {
            std::size_t nvalues = n_bytes;
            if constexpr (sizeof(value_type) > 1) {
                nvalues = (n_bytes / sizeof(value_type)) +
                          (n_bytes % sizeof(value_type) != 0);
            }
            alloc_traits::deallocate(alloc_, reinterpret_cast<pointer>(p),
                                     nvalues);
        }

    private:
        Allocator alloc_;
    };
} // namespace RAD_LIB_NAMESPACE

namespace RAD_LIB_NAMESPACE::detail {
    struct coroutine_final_suspend_observer;

    template <class Awaitable>
    struct awaitable_starter;

    template <class Awaitable>
    void
    start_one_awaitable(Awaitable& a,
                        coroutine_final_suspend_observer& observer) noexcept {
        using starter = detail::awaitable_starter<Awaitable>;
        starter::start(a, observer);
    }

    template <class Awaitable>
    bool get_one_awaitable_error(
        Awaitable& a, std::optional<std::exception_ptr>& ex_ptr) noexcept {
        using starter = detail::awaitable_starter<Awaitable>;
        if (ex_ptr.has_value()) {
            return true;
        }
        ex_ptr = starter::get_error(a);
        return ex_ptr.has_value();
    }

    template <class VoidAlternative, class Awaitable>
    auto move_one_awaitable_result(Awaitable& a) {
        using value_type = typename Awaitable::value_type;
        using starter = detail::awaitable_starter<Awaitable>;
        if constexpr (std::is_same_v<value_type, void>) {
            if constexpr (std::is_same_v<VoidAlternative, void>) {
                return;
            }
            else {
                return VoidAlternative{};
            }
        }
        else {
            return starter::move_result(a);
        }
    }

    template <class Awaitable>
    void cancel_one_awaitable(Awaitable& a) noexcept {
        using starter = detail::awaitable_starter<Awaitable>;
        starter::cancel(a);
    }

    struct RAD_EXPORT_VTABLE coroutine_final_suspend_observer {
        virtual ~coroutine_final_suspend_observer() = default;

        /*!
         * @brief Get a handle to the waiter coroutine that is
         * to be resumed after the coroutine arrives at
         * final_suspend(). If no waiter is to be resumed now
         * this method must return noop_coroutine_handle.
         * when_all awaitable will return noop handles for each
         * task except the last task. spawn operation will
         * return noop handle and schedule the operation to free
         * itself
         * @return the waiter handle to resume or
         * noop_coroutine_handle if no waiter
         */
        virtual std::coroutine_handle<> get_waiter() = 0;

        virtual void on_coroutine_has_unhandled_exception() noexcept {
        }
    };

    template <class T>
    struct task_result {
        static constexpr std::size_t ex_index = 1;
        static constexpr std::size_t result_index = 2;

        std::variant<std::monostate, std::exception_ptr, T> storage;

        void unhandled_exception() noexcept {
            storage.template emplace<ex_index>(std::current_exception());
        }

        void return_value(const T& value)
            requires(std::copy_constructible<T>)
        {
            storage.template emplace<result_index>(value);
        }

        void return_value(T&& value)
            requires(std::move_constructible<T>)
        {
            storage.template emplace<result_index>(std::move(value));
        }

        template <class U>
        void return_value(U&& value)
            requires(!std::same_as<T, U> && std::constructible_from<T, U>)
        {
            if constexpr ((std::is_integral_v<T> && std::is_integral_v<U>) ||
                          (std::is_floating_point_v<T> &&
                           std::is_floating_point_v<U>)) {
                storage.template emplace<result_index>(
                    static_cast<T>(std::forward<U>(value))); // get rid of the
                                                             // cast warning
            }
            else {
                storage.template emplace<result_index>(std::forward<U>(value));
            }
        }

        std::optional<std::exception_ptr> get_error() noexcept {
            if (storage.index() == ex_index) {
                return std::get<ex_index>(storage);
            }
            return std::nullopt;
        }

        T& get_result() {
            // either return_value or unhandled_exception
            // must have been called
            if (storage.index() == ex_index) {
                std::rethrow_exception(std::get<ex_index>(storage));
            }
            return std::get<result_index>(storage);
        }

        T move_result() {
            // either return_value or unhandled_exception
            // must have been called
            if (storage.index() == ex_index) {
                std::rethrow_exception(std::get<ex_index>(storage));
            }
            return std::move(std::get<result_index>(storage));
        }

        std::variant<T&, std::exception_ptr> get_result_or_error() noexcept(
            std::is_nothrow_move_constructible_v<T>) {
            // either return_value or unhandled_exception
            // must have been called
            if (storage.index() == ex_index) {
                return std::get<ex_index>(storage);
            }
            else {
                return std::get<result_index>(storage);
            }
        }

        std::variant<T, std::exception_ptr> move_result_or_error() noexcept(
            std::is_nothrow_move_constructible_v<T>) {
            // either return_value or unhandled_exception
            // must have been called
            if (storage.index() == ex_index) {
                return std::get<ex_index>(storage);
            }
            else {
                return std::move(std::get<result_index>(storage));
            }
        }
    };

    template <>
    struct task_result<void> {
        std::optional<std::exception_ptr> ex_ptr;

        void unhandled_exception() noexcept {
            ex_ptr = std::current_exception();
        }

        constexpr void return_void() const noexcept {
        }

        std::optional<std::exception_ptr> get_error() noexcept {
            if (ex_ptr) {
                return ex_ptr;
            }
            return std::nullopt;
        }

        void get_result() const {
            if (ex_ptr) {
                std::rethrow_exception(*ex_ptr);
            }
        }

        void move_result() const {
            if (ex_ptr) {
                std::rethrow_exception(*ex_ptr);
            }
        }

        std::variant<std::monostate, std::exception_ptr>
        get_result_or_error() const noexcept {
            if (ex_ptr) {
                return *ex_ptr;
            }
            return {};
        }

        std::variant<std::monostate, std::exception_ptr>
        move_result_or_error() const noexcept {
            if (ex_ptr) {
                return *ex_ptr;
            }
            return {};
        }
    };

    template <class Allocator>
    struct leading_alloc_operators {
    private:
        static void* allocate_mem(std::size_t n, Allocator alloc) {
            constexpr std::size_t prefix_size =
                sizeof(use_allocator<Allocator>);
            use_allocator<Allocator> use_alloc{std::move(alloc)};
            uint8_t* ptr = use_alloc.allocate_bytes(n + prefix_size);
            new (ptr) use_allocator<Allocator>(std::move(use_alloc));
            return ptr + prefix_size;
        }

    public:
        template <class... Args>
        void* operator new(std::size_t n, std::allocator_arg_t,
                           const Allocator& alloc, Args&&... args) {
            return allocate_mem(n, alloc);
        }

        template <class... Args>
        void* operator new(std::size_t n, std::allocator_arg_t,
                           Allocator&& alloc, Args&&... args) {
            return allocate_mem(n, std::move(alloc));
        }

        template <class Class, class... Args>
        void* operator new(std::size_t n, Class& self, std::allocator_arg_t,
                           const Allocator& alloc, Args&&... args) {
            return allocate_mem(n, alloc);
        }

        template <class Class, class... Args>
        void* operator new(std::size_t n, Class& self, std::allocator_arg_t,
                           Allocator&& alloc, Args&&... args) {
            return allocate_mem(n, std::move(alloc));
        }

        void operator delete(void* p, std::size_t n) noexcept {
            constexpr std::size_t prefix_size =
                sizeof(use_allocator<Allocator>);
            uint8_t* ptr = static_cast<uint8_t*>(p) - prefix_size;
            n += prefix_size;
            // move the use_allocator<Allocator> out of ptr
            auto alloc =
                std::move(*reinterpret_cast<use_allocator<Allocator>*>(ptr));
            alloc.deallocate_bytes(ptr, n);
        }
    };

    template <class Allocator>
    struct use_alloc_operators {
    private:
        static void* allocate_mem(std::size_t n,
                                  use_allocator<Allocator> alloc) {
            constexpr std::size_t prefix_size =
                sizeof(use_allocator<Allocator>);
            uint8_t* ptr = alloc.allocate_bytes(n + prefix_size);
            new (ptr) use_allocator<Allocator>(std::move(alloc));
            return ptr + prefix_size;
        }

    public:
        template <class... Args>
        void* operator new(std::size_t n, const use_allocator<Allocator>& alloc,
                           Args&&... args) {
            return allocate_mem(n, alloc);
        }

        template <class... Args>
        void* operator new(std::size_t n, use_allocator<Allocator>&& alloc,
                           Args&&... args) {
            return allocate_mem(n, std::move(alloc));
        }

        template <class Class, class... Args>
        void* operator new(std::size_t n, Class& self,
                           const use_allocator<Allocator>& alloc,
                           Args&&... args) {
            return allocate_mem(n, alloc);
        }

        template <class Class, class... Args>
        void* operator new(std::size_t n, Class& self,
                           use_allocator<Allocator>&& alloc, Args&&... args) {
            return allocate_mem(n, std::move(alloc));
        }

        void operator delete(void* p, std::size_t n) noexcept {
            constexpr std::size_t prefix_size =
                sizeof(use_allocator<Allocator>);
            uint8_t* ptr = static_cast<uint8_t*>(p) - prefix_size;
            n += prefix_size;
            // move the use_allocator<Allocator> out of ptr
            auto alloc =
                std::move(*reinterpret_cast<use_allocator<Allocator>*>(ptr));
            alloc.deallocate_bytes(ptr, n);
        }
    };

    using coro_final_suspend_waiter_t =
        std::variant<std::coroutine_handle<>,
                     coroutine_final_suspend_observer*>;

    // Suspend the current coroutine in final_suspend and resume the waiter
    // coroutine. The current coroutine will be destroyed in the destructor
    // of the task
    class resume_waiter {
    public:
        resume_waiter(coro_final_suspend_waiter_t waiter) noexcept
            : waiter_{waiter} {
        }

        constexpr bool await_ready() const noexcept {
            return false;
        }

        std::coroutine_handle<>
        await_suspend(std::coroutine_handle<> suspended) noexcept {
            // assert(suspended.done() && "the coroutine
            // must be suspended at this point");
            ((void)suspended);
            // the coroutine is suspended here so get_waiter
            // may schedule its coroutine (suspended) on
            // another thread and it may be destroyed by the
            // return of get_waiter() so it must not be
            // accessed afterwards and in this case the
            // returned waiter_handle is noop coroutine
            auto waiter_handle = waiter_.index() == 0
                                     ? std::get<0>(waiter_)
                                     : std::get<1>(waiter_)->get_waiter();
            return waiter_handle;
        }

        void await_resume() const noexcept {
            assert(false && "execution should nenver reach here");
        }

    private:
        coro_final_suspend_waiter_t waiter_;
    };

    // for members and non members with no allocation options
    template <class T>
    class task_promise_base : public task_result<T> {
        template <typename>
        friend class rad::task;

    public:
        void unhandled_exception() noexcept {
            task_result<T>::unhandled_exception();
            if (std::holds_alternative<coroutine_final_suspend_observer*>(
                    waiter_)) {
                std::get<coroutine_final_suspend_observer*>(waiter_)
                    ->on_coroutine_has_unhandled_exception();
            }
        }

        task<T> get_return_object() noexcept {
            return {
                std::coroutine_handle<task_promise_base>::from_promise(*this)};
        }

        constexpr std::suspend_always initial_suspend() noexcept {
            return {};
        }

        resume_waiter final_suspend() noexcept {
            return resume_waiter{waiter_};
        }

        void cancel() noexcept {
            canceled_ = true;
        }

        template <class A>
        A&& await_transform(A&& a) {
            if (canceled_) {
                throw std::system_error{
                    std::make_error_code(std::errc::operation_canceled)};
            }
            return std::forward<A>(a);
        }

    private:
        coro_final_suspend_waiter_t waiter_;
        std::atomic<bool> canceled_ = false;
    };

    // for members and non members with allocation options
    template <class T, class AllocBase>
    class task_promise : public task_promise_base<T>, public AllocBase {};
} // namespace RAD_LIB_NAMESPACE::detail

namespace RAD_LIB_NAMESPACE {
    template <class T>
    concept ObservableAwaitable =
        requires(T& a, detail::coroutine_final_suspend_observer& o) {
            requires TaskValue<typename T::value_type>;
            typename detail::awaitable_starter<T>;
            detail::awaitable_starter<T>::start(a, o);
            {
                detail::awaitable_starter<T>::get_error(a)
            } noexcept -> std::same_as<std::optional<std::exception_ptr>>;
            {
                detail::awaitable_starter<T>::move_result(a)
            } -> std::same_as<typename T::value_type>;
            { detail::awaitable_starter<T>::cancel(a) } noexcept;
        };

    /*!
     * @brief Awaitable type used to chain coroutines and get their results.
     *
     * A coroutine which returns task<T> is called a lazy coroutine,
     * because it will not start until the returned task is awaited.
     * @tparam T The type of coroutine result value.
     */
    template <class T = void>
    class [[nodiscard("task will not start unless it is awaited")]] task
        : noncopyable {
        static_assert(TaskValue<T>, "T must be void, movable or copyable type");

        template <typename>
        friend class detail::task_promise_base;

        friend detail::awaitable_starter<task>;

        using promise_t = detail::task_promise_base<T>;
        using handle = std::coroutine_handle<promise_t>;

        // used by promise get_return_object
        task(handle coro) noexcept : this_coro_{coro} {
        }

    public:
        /*!
         * @brief The type of coroutine result value.
         */
        using value_type = T;

        /*!
         * @brief Move construct a task. The moved from task
         * will be empty and its coroutine handle will be moved
         * to this task.
         * @param other The other task to take its coroutine
         * handle.
         */
        task(task&& other) noexcept
            : this_coro_{std::exchange(other.this_coro_, nullptr)} {
        }

        /*!
         * @brief Destroy the task and if it owns a coroutine
         * handle its destroy() method is called to destroy and
         * deallocate the coroutine.
         */
        ~task() {
            if (this_coro_) {
                this_coro_.destroy();
            }
        }

        /*!
         * @brief Move assign a task. This task must be either
         * empty or suspended since it will be destroyed. The moved from task
         * will be empty and its coroutine handle will be moved to this task.
         * @param other The other task to take its coroutine
         * handle.
         * @return Reference to self.
         */
        task& operator=(task&& other) noexcept {
            if (std::addressof(other) == this) {
                return *this;
            }
            assert(this_coro_ == nullptr || done());
            if (this_coro_) {
                this_coro_.destroy();
            }
            this_coro_ = std::exchange(other.this_coro_, nullptr);
            return *this;
        }

        /*!
         * @brief Check if the coroutine has reached its final
         * suspension point (using coroutine_handle::done()).
         * This task must be valid otherwise behavior is
         * undefined
         * @return True if the coroutine has reached its final
         * suspension point, and false otherwise.
         */
        bool done() const noexcept {
            assert(this_coro_ != nullptr);
            return this_coro_.done();
        }

        /*!
         * @brief Check if the coroutine is ready to continue
         * without suspension. The task always starts suspended.
         * Note: don't use await_* methods directly!
         * @return Always returns false.
         */
        constexpr bool await_ready() const noexcept {
            return false;
        }

        /*!
         * @brief Suspend the caller coroutine and start the
         * coroutine owned by the task. The caller coroutine is
         * arranged to be resumed after the task's coroutine
         * reaches its final suspension point. Note: don't use
         * await_* methods directly!
         * @param waiter The caller coroutine handle.
         * @return The handle of the coroutine owned by the task
         * to be resumed now.
         */
        handle await_suspend(std::coroutine_handle<> waiter) noexcept {
            assert(this_coro_ != nullptr);
            this_promise().waiter_ = waiter;
            return this_coro_;
        }

        /*!
         * @brief Get the result of the coroutine owned by the
         * task after it has reached its final suspension point.
         * Note: don't use await_* methods directly!
         * @return
         */
        T await_resume() {
            assert(this_coro_ != nullptr);
            return this_promise().move_result();
        }

        /*!
         * @brief Check if this task own a coroutine handle.
         * @return True if this task own a coroutine handle, and
         * false otherwise.
         */
        bool is_valid() const noexcept {
            return this_coro_ != nullptr;
        }

        /*!
         * @brief Mark the owned coroutine as canceled.
         * Canceled coroutines will throw a system error on the
         * next suspension point with error code
         * `std::errc::operation_canceled`.
         */
        void cancel() noexcept {
            if (!is_valid()) {
                return;
            }
            this_promise().cancel();
        }

    private:
        void start_coro(
            detail::coroutine_final_suspend_observer& observer) noexcept {
            assert(this_coro_ != nullptr);
            this_promise().waiter_ = &observer;
            this_coro_.resume(); // never throws since all
                                 // exceptions are caught
                                 // by the promise
        }

        std::optional<std::exception_ptr> get_error() noexcept {
            assert(this_coro_ != nullptr);
            return this_promise().get_error();
        }

        T move_result() {
            assert(this_coro_ != nullptr);
            return this_promise().move_result();
        }

        auto move_result_or_error() noexcept(
            std::is_nothrow_move_constructible_v<T>) {
            assert(this_coro_ != nullptr);
            return this_promise().move_result_or_error();
        }

        promise_t& this_promise() const noexcept {
            return this_coro_.promise();
        }

        handle this_coro_;
    };

    namespace detail {
        template <class T>
        struct awaitable_starter<task<T>> {
            static void
            start(task<T>& a,
                  coroutine_final_suspend_observer& observer) noexcept {
                a.start_coro(observer);
            }

            static std::optional<std::exception_ptr>
            get_error(task<T>& a) noexcept {
                return a.get_error();
            }

            static T move_result(task<T>& a) {
                return a.move_result();
            }

            static void cancel(task<T>& a) noexcept {
                a.cancel();
            }
        };
    } // namespace detail

    static_assert(ObservableAwaitable<task<int>>,
                  "task<T> must satisfy ObservableAwaitable");
} // namespace RAD_LIB_NAMESPACE

namespace std {
    // for task members and non members with no allocation options
    template <class Ret, class... Args>
    struct coroutine_traits<rad::task<Ret>, Args...> {
        using promise_type = rad::detail::task_promise_base<Ret>;
    };

    // for task non members with leading allocation option
    template <class Ret, class Allocator, class... Args>
    struct coroutine_traits<rad::task<Ret>, std::allocator_arg_t,
                            const Allocator&, Args...> {
        using promise_type = rad::detail::task_promise<
            Ret, rad::detail::leading_alloc_operators<Allocator>>;
    };

    // for task non members with leading allocation option
    template <class Ret, class Allocator, class... Args>
    struct coroutine_traits<rad::task<Ret>, std::allocator_arg_t, Allocator&,
                            Args...> {
        using promise_type = rad::detail::task_promise<
            Ret, rad::detail::leading_alloc_operators<Allocator>>;
    };

    // for task non members with leading allocation option
    template <class Ret, class Allocator, class... Args>
    struct coroutine_traits<rad::task<Ret>, std::allocator_arg_t, Allocator&&,
                            Args...> {
        using promise_type = rad::detail::task_promise<
            Ret, rad::detail::leading_alloc_operators<Allocator>>;
    };

    // for task members with leading allocation option
    template <class Ret, class Class, class Allocator, class... Args>
    struct coroutine_traits<rad::task<Ret>, Class&, std::allocator_arg_t,
                            const Allocator&, Args...> {
        using promise_type = rad::detail::task_promise<
            Ret, rad::detail::leading_alloc_operators<Allocator>>;
    };

    // for task members with leading allocation option
    template <class Ret, class Class, class Allocator, class... Args>
    struct coroutine_traits<rad::task<Ret>, Class&, std::allocator_arg_t,
                            Allocator&, Args...> {
        using promise_type = rad::detail::task_promise<
            Ret, rad::detail::leading_alloc_operators<Allocator>>;
    };

    // for task members with leading allocation option
    template <class Ret, class Class, class Allocator, class... Args>
    struct coroutine_traits<rad::task<Ret>, Class&, std::allocator_arg_t,
                            Allocator&&, Args...> {
        using promise_type = rad::detail::task_promise<
            Ret, rad::detail::leading_alloc_operators<Allocator>>;
    };

    // for task non members with use_allocator option
    template <class Ret, class Allocator, class... Args>
    struct coroutine_traits<rad::task<Ret>,
                            const rad::use_allocator<Allocator>&, Args...> {
        using promise_type = rad::detail::task_promise<
            Ret, rad::detail::use_alloc_operators<Allocator>>;
    };

    // for task non members with use_allocator option
    template <class Ret, class Allocator, class... Args>
    struct coroutine_traits<rad::task<Ret>, rad::use_allocator<Allocator>&,
                            Args...> {
        using promise_type = rad::detail::task_promise<
            Ret, rad::detail::use_alloc_operators<Allocator>>;
    };

    // for task non members with use_allocator option
    template <class Ret, class Allocator, class... Args>
    struct coroutine_traits<rad::task<Ret>, rad::use_allocator<Allocator>&&,
                            Args...> {
        using promise_type = rad::detail::task_promise<
            Ret, rad::detail::use_alloc_operators<Allocator>>;
    };

    // for task members with use_allocator option
    template <class Ret, class Allocator, class Class, class... Args>
    struct coroutine_traits<rad::task<Ret>, Class&,
                            const rad::use_allocator<Allocator>&, Args...> {
        using promise_type = rad::detail::task_promise<
            Ret, rad::detail::use_alloc_operators<Allocator>>;
    };

    // for task members with use_allocator option
    template <class Ret, class Allocator, class Class, class... Args>
    struct coroutine_traits<rad::task<Ret>, Class&,
                            rad::use_allocator<Allocator>&, Args...> {
        using promise_type = rad::detail::task_promise<
            Ret, rad::detail::use_alloc_operators<Allocator>>;
    };

    // for task members with use_allocator option
    template <class Ret, class Allocator, class Class, class... Args>
    struct coroutine_traits<rad::task<Ret>, Class&,
                            rad::use_allocator<Allocator>&&, Args...> {
        using promise_type = rad::detail::task_promise<
            Ret, rad::detail::use_alloc_operators<Allocator>>;
    };
} // namespace std