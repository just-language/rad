#pragma once
#include <rad/async/async_wait_queue.h>
#include <rad/async/checked_counter.h>
#include <rad/async/executor.h>
#include <rad/coro/awaitable_traits.h>
#include <rad/coro/task.h>
#include <rad/libbase.h>
#include <rad/threading/spinlock.h>

#include <atomic>
#ifndef NDEBUG
#include <rad/threading/thread.h>
#endif // !NDEBUG

namespace RAD_LIB_NAMESPACE {
    template <class T>
    struct spawn_handler_bound_to_executor
        : std::conditional_t<std::is_empty_v<T>, std::false_type,
                             std::true_type> {};

    namespace detail {
        struct dummy_spawn_final_handler_t {
            constexpr void operator()() const noexcept {
            }
        };

        template <class T, std::size_t N,
                  bool inherit = std::is_class_v<T> && std::is_empty_v<T> &&
                                 !std::is_final_v<T>>
        struct spawn_compressed_wrapped_type : T {
            template <class U>
            spawn_compressed_wrapped_type(U&& t) : T(std::forward<U>(t)) {
            }

            template <class U>
            spawn_compressed_wrapped_type& operator=(U&& t) {
                static_cast<T&>(*this) = std::forward<U>(t);
                return *this;
            }

            T& get() noexcept {
                return static_cast<T&>(*this);
            }
        };

        template <class T, std::size_t N>
        struct spawn_compressed_wrapped_type<T, N, false> {
            T elem;

            template <class U>
            spawn_compressed_wrapped_type(U&& t) : elem(std::forward<U>(t)) {
            }

            template <class U>
            spawn_compressed_wrapped_type& operator=(U&& t) {
                elem = std::forward<U>(t);
                return *this;
            }

            T& get() noexcept {
                return elem;
            }
        };

        template <class T1, class T2, class T3>
        struct spawn_op_compressed_3 : spawn_compressed_wrapped_type<T1, 1>,
                                       spawn_compressed_wrapped_type<T2, 2>,
                                       spawn_compressed_wrapped_type<T3, 3> {
            using base1 = spawn_compressed_wrapped_type<T1, 1>;
            using base2 = spawn_compressed_wrapped_type<T2, 2>;
            using base3 = spawn_compressed_wrapped_type<T3, 3>;

            template <class U1, class U2, class U3>
            spawn_op_compressed_3(U1&& t1, U2&& t2, U3&& t3)
                : base1(std::forward<U1>(t1)), base2(std::forward<U2>(t2)),
                  base3(std::forward<U3>(t3)) {
            }

            T1& get0() noexcept {
                return static_cast<base1&>(*this).get();
            }

            T2& get1() noexcept {
                return static_cast<base2&>(*this).get();
            }

            T3& get2() noexcept {
                return static_cast<base3&>(*this).get();
            }
        };

        template <class Exec, class Awaitable, class Alloc, class ResultHandler,
                  class ErrorHandler, class FinalHandler>
        class spawn_op final : public async_op_base,
                               public allocator_storage<Alloc>,
                               coroutine_final_suspend_observer {
            using T = typename Awaitable::value_type;
            using non_void_result_t =
                std::conditional_t<std::is_same_v<T, void>, std::monostate, T>;
            using alloc_base = allocator_storage<Alloc>;

            static constexpr bool is_void_result = std::is_same_v<T, void>;
            static constexpr bool bound_to_executor =
                true ||
                spawn_handler_bound_to_executor<
                    std::decay_t<ResultHandler>>() ||
                spawn_handler_bound_to_executor<std::decay_t<ErrorHandler>>() ||
                spawn_handler_bound_to_executor<std::decay_t<FinalHandler>>();

            Exec& ex_;
            Awaitable awaitable_;
            spawn_op_compressed_3<std::decay_t<ResultHandler>,
                                  std::decay_t<ErrorHandler>,
                                  std::decay_t<FinalHandler>>
                r_e_f_;
            bool started_ = false;

            void invoke_handlers() {
                using starter = detail::awaitable_starter<Awaitable>;
                // move everything outside spawn op before free!
                auto awaitable2 = std::move(awaitable_);
                auto result_handler = std::move(r_e_f_.get0());
                auto error_handler = std::move(r_e_f_.get1());
                auto final_handler = std::move(r_e_f_.get2());
                free_op(this);

                final_handler();
                auto awaitable_error = starter::get_error(awaitable2);
                if (awaitable_error.has_value()) {
                    error_handler(*awaitable_error);
                }
                else {
                    if constexpr (is_void_result) {
                        result_handler();
                    }
                    else {
                        result_handler(starter::move_result(awaitable2));
                    }
                }
            }

            virtual void invoke_operation() override {
                // At some release point, MSVC broke the
                // coroutine_handle::done() method so
                // fallback to using a bool flag

                // if (!task_.done()) {
                if (!started_) {
                    if constexpr (bound_to_executor) {
                        ex_.add_work();
                    }
                    started_ = true;
                    using starter = detail::awaitable_starter<Awaitable>;
                    starter::start(awaitable_, *this);
                }
                else {
                    invoke_handlers();
                }
            }

            virtual any_executor&
            associated_executor() const noexcept override {
                return ex_;
            }

            virtual std::coroutine_handle<> get_waiter() override {
                // Again due to broken
                // coroutine_handle::done() with MSVC

                // assert(task_.done() && "the spawn
                // task must be awaiting final_suspend
                // awaiter at this point");
                if constexpr (bound_to_executor) {
                    ex_.post_finished(*this);
#ifndef NDEBUG
                    // Give a chance for other
                    // threads to run so task_ may
                    // be destroyed on another
                    // thread. This may reveal a
                    // race condition bug caused by
                    // the compiler
                    // this_thread::sleep_for(std::chrono::milliseconds{
                    // 50
                    // });
#endif // !NDEBUG
                }
                else {
                    invoke_handlers();
                }
                // don't access this after post or
                // delete !
                return std::noop_coroutine();
            }

        public:
            template <class RHandler, class EHandler, class FHandler>
            spawn_op(Awaitable&& a, Exec& ex, RHandler&& rhandler,
                     EHandler&& ehandler, FHandler&& fhandler,
                     const Alloc& alloc)
                : async_op_base(async_op_type::spawn), alloc_base(alloc),
                  ex_{ex}, awaitable_{std::move(a)},
                  r_e_f_{std::forward<RHandler>(rhandler),
                         std::forward<EHandler>(ehandler),
                         std::forward<FHandler>(fhandler)} {
            }
        };
    } // namespace detail

    template <>
    struct spawn_handler_bound_to_executor<detail::dummy_spawn_final_handler_t>
        : std::false_type {};

    template <class Fn, class T>
    concept SpawnResultHandler =
        BasicHandler<Fn> &&
        ((std::invocable<Fn, T> && !std::same_as<T, void>) ||
         (std::invocable<Fn> && std::same_as<T, void>));

    /*!
     * @brief An error handler wrapper to use with spawn. This is useful
     * when the spawned task return type is `std::exception_ptr` and an
     * error handler only is passed. To distinguish between the error
     * handler and the result handler, the error handler is wrapped in this
     * struct.
     * @tparam ErrorHandler The type of wrapped error handler
     */
    template <class Handler>
    struct error_handler_wrapper {
        Handler handler;

        template <class H>
        error_handler_wrapper(H&& handler)
            : handler{std::forward<Handler>(handler)} {
        }
    };

    /*!
     * @brief Wrap an error handler to distinguish between the error handler
     * and the result handler if the result type is `std::exception_ptr`.
     * @param handler The error handler to wrap.
     */
    template <std::invocable<std::exception_ptr> ErrorHandler>
    error_handler_wrapper<ErrorHandler> redirect_error(ErrorHandler&& handler) {
        return error_handler_wrapper<std::remove_cvref_t<ErrorHandler>>{
            std::forward<ErrorHandler>(handler)};
    };

    /*!
     * @brief Terminate error handler that will call std::terminate() on
     * coroutines unhandled execptions.
     */
    struct terminate_on_error_t {
        void
        operator()([[maybe_unused]] std::exception_ptr ex_ptr) const noexcept {
#ifndef NDEBUG
            try {
                std::rethrow_exception(ex_ptr);
            }
            catch (const std::exception& ex) {
                printf("[!!] terminate will be called "
                       "because an "
                       "exception was thrown "
                       "and not caught from spawned task "
                       ": %s\n",
                       ex.what());
            }
            catch (...) {
                printf("[!!] terminate will be called "
                       "because an "
                       "exception was thrown "
                       "and not caught from spawned task "
                       "!\n");
            }
#endif // !NDEBUG
            std::terminate();
        }
    };

    /*!
     * @brief Ignore error handler that will ignore coroutines unhandled
     * execptions.
     */
    struct ignore_errors_t {
        void operator()(std::exception_ptr) const noexcept {
        }
    };

    static_assert(SpawnResultHandler<any_invcable_t, void>);
    static_assert(SpawnResultHandler<any_invcable_t, int>);

    /*!
     * @brief Handlers of type `terminate_on_error_t` don't require to be
     * executed on the executor.
     */
    template <>
    struct spawn_handler_bound_to_executor<terminate_on_error_t>
        : std::false_type {};

    /*!
     * @brief Handlers of type `ignore_errors_t` don't require to be
     * executed on the executor.
     */
    template <>
    struct spawn_handler_bound_to_executor<ignore_errors_t> : std::false_type {
    };

    /*!
     * @brief An instance of `terminate_on_error_t` to be passed as the
     * spawn error handler.
     */
    inline constexpr terminate_on_error_t terminate_on_error{};

    /*!
     * @brief An instance of `ignore_errors_t` to be passed as the spawn
     * error handler.
     */
    inline constexpr ignore_errors_t ignore_errors{};

    /*!
     * @brief Post an awaitable to be executed on an executor. While the
     * coroutine will start executing on the provided executor it is not
     * guaranteed that the coroutine will not switch to another executor
     * after any awaiting point. If all the handlers passed have
     * specializations of trait `spawn_handler_bound_to_executor` that
     * inherits `std::false_type` then the result and error handlers are not
     * guaranteed to be executed on the provided executor. Otherwise the
     * handlers are guaranteed to be executed on the provided executor.
     * @tparam Exec The type of executor.
     * @tparam Awaitable The type of awaitable which must satisfy
     * ObservableAwaitable.
     * @tparam ResultHandler The type of the result handler which must
     * satisfy SpawnResultHandler<typename Awaitable::value_type>.
     * @tparam ErrorHandler The type of the error handler which must be
     * callable with `std::exception_ptr`
     * @tparam Alloc The type of allocator which must satisfy
     * HandlerAllocator
     * @param ex The executor to post the task to be executed on.
     * @param t The task to post on the executor.
     * @param rhandler The result handler. This handler will be called with
     * the result of the task if the coroutine reaches its final suspend
     * point without exiting due to an unhandled exception.
     * @param ehandler The error handler. This handler will be called with
     * parameter of type `std::exception_ptr` if the coroutine exits due to
     * an unhandled exception.
     * @param alloc An allocator used to allocate the operation. The size
     * passed to its allocate method is in bytes and the alignment of the
     * allocator must be suitable for std::max_align_t
     */
    template <Executor Exec, ObservableAwaitable Awaitable,
              SpawnResultHandler<typename Awaitable::value_type> ResultHandler,
              std::invocable<std::exception_ptr> ErrorHandler,
              HandlerAllocator Alloc = default_io_allocator>
    void spawn(Exec& ex, Awaitable&& awaitable, ResultHandler&& rhandler,
               ErrorHandler&& ehandler, const Alloc& alloc = Alloc()) {
// #ifdef __clang__
#if 0
		static_assert(!std::is_same_v<Exec, any_executor>,
			"clang breaks spawn when HALO optimization is enalbed and any_executor is used");
#endif // __clang__
        using fhandler_t = detail::dummy_spawn_final_handler_t;
        using op_t =
            detail::spawn_op<Exec, Awaitable, Alloc,
                             std::remove_cvref_t<ResultHandler>,
                             std::remove_cvref_t<ErrorHandler>, fhandler_t>;
        op_t* op = detail::allocate_op<op_t>(
            alloc, std::move(awaitable), ex,
            std::forward<ResultHandler>(rhandler),
            std::forward<ErrorHandler>(ehandler), fhandler_t{});
        ex.post(*op);
    }

    /*!
     * @brief Post an awaitable to be executed on an executor. While the
     * coroutine will start executing on the provided executor it is not
     * guaranteed that the coroutine will not switch to another executor
     * after any awaiting point. If all the handlers passed have
     * specializations of trait `spawn_handler_bound_to_executor` that
     * inherits `std::false_type` then the result and error handlers are not
     * guaranteed to be executed on the provided executor. Otherwise the
     * handlers are guaranteed to be executed on the provided executor.
     * @tparam Exec The type of executor.
     * @tparam T The type of coroutine task value which must satisfy
     * TaskValue
     * @tparam ResultHandler The type of the result handler which must
     * satisfy SpawnResultHandler<typename Awaitable::value_type>
     * @tparam Alloc The type of allocator which must satisfy
     * HandlerAllocator
     * @param ex The executor to post the task to be executed on.
     * @param t The task to post on the executor.
     * @param rhandler rhandler The result handler. This handler will be
     * called with the result of the task if the coroutine reaches its final
     * suspend point without exiting due to an unhandled exception. In case
     * the coroutine exits due to an unhandled exception, then
     * `std::terminate()` will be called.
     * @param alloc An allocator used to allocate the operation. The size
     * passed to its allocate method is in bytes and the alignment of the
     * allocator must be suitable for std::max_align_t
     */
    template <Executor Exec, ObservableAwaitable Awaitable,
              SpawnResultHandler<typename Awaitable::value_type> ResultHandler,
              HandlerAllocator Alloc = default_io_allocator>
    void spawn(Exec& ex, Awaitable&& awaitable, ResultHandler&& rhandler,
               const Alloc& alloc = Alloc()) {
        spawn(ex, std::move(awaitable), std::forward<ResultHandler>(rhandler),
              terminate_on_error, alloc);
    }

    /*!
     * @brief Post an awaitable to be executed on an executor. While the
     * coroutine will start executing on the provided executor it is not
     * guaranteed that the coroutine will not switch to another executor
     * after any awaiting point. If all the handlers passed have
     * specializations of trait `spawn_handler_bound_to_executor` that
     * inherits `std::false_type` then the result and error handlers are not
     * guaranteed to be executed on the provided executor. Otherwise the
     * handlers are guaranteed to be executed on the provided executor.
     * @tparam Exec The type of executor.
     * @tparam T The type of coroutine task value which must satisfy
     * TaskValue
     * @tparam ErrorHandler The type of the error handler which must be
     * callable with `std::exception_ptr`
     * @tparam Alloc The type of allocator which must satisfy
     * HandlerAllocator
     * @param ex The executor to post the task to be executed on.
     * @param t The task to post on the executor.
     * @param ehandler The error handler. This handler will be called with
     * parameter of type `std::exception_ptr` if the coroutine exits due to
     * an unhandled exception. In case the coroutine reaches its final
     * suspend point without an unhandled exception the error handler will
     * not be called and the coroutine result will be discarded.
     * @param alloc An allocator used to allocate the operation. The size
     * passed to its allocate method is in bytes and the alignment of the
     * allocator must be suitable for std::max_align_t
     */
    template <Executor Exec, ObservableAwaitable Awaitable,
              std::invocable<std::exception_ptr> ErrorHandler,
              HandlerAllocator Alloc = default_io_allocator>
    void spawn(Exec& ex, Awaitable&& awaitable, ErrorHandler&& ehandler,
               const Alloc& alloc = Alloc())
        requires(
            !std::same_as<typename Awaitable::value_type, std::exception_ptr> &&
            !SpawnResultHandler<ErrorHandler, typename Awaitable::value_type>)
    {
        spawn(ex, std::move(awaitable), any_invcable,
              std::forward<ErrorHandler>(ehandler), alloc);
    }

    /*!
     * @brief Post an awaitable to be executed on an executor. While the
     * coroutine will start executing on the provided executor it is not
     * guaranteed that the coroutine will not switch to another executor
     * after any awaiting point. If all the handlers passed have
     * specializations of trait `spawn_handler_bound_to_executor` that
     * inherits `std::false_type` then the result and error handlers are not
     * guaranteed to be executed on the provided executor. Otherwise the
     * handlers are guaranteed to be executed on the provided executor.
     * @tparam Exec The type of executor.
     * @tparam T The type of coroutine task value which must satisfy
     * TaskValue
     * @tparam ErrorHandler The type of the error handler which must be
     * callable with `std::exception_ptr`
     * @tparam Alloc The type of allocator which must satisfy
     * HandlerAllocator
     * @param ex The executor to post the task to be executed on.
     * @param t The task to post on the executor.
     * @param ewrapper The error handler. This handler will be called with
     * parameter of type `std::exception_ptr` if the coroutine exits due to
     * an unhandled exception. In case the coroutine reaches its final
     * suspend point without an unhandled exception the error handler will
     * not be called and the coroutine result will be discarded.
     * @param alloc An allocator used to allocate the operation. The size
     * passed to its allocate method is in bytes and the alignment of the
     * allocator must be suitable for std::max_align_t
     */
    template <Executor Exec, ObservableAwaitable Awaitable,
              std::invocable<std::exception_ptr> ErrorHandler,
              HandlerAllocator Alloc = default_io_allocator>
    void spawn(Exec& ex, Awaitable&& awaitable,
               error_handler_wrapper<ErrorHandler> ewrapper,
               const Alloc& alloc = Alloc()) {
        spawn(ex, std::move(awaitable), any_invcable,
              std::move(ewrapper.handler), alloc);
    }

    /*!
     * @brief Post an awaitable to be executed on an executor. While the
     * coroutine will start executing on the provided executor it is not
     * guaranteed that the coroutine will not switch to another executor
     * after any awaiting point.
     * @tparam Exec The type of executor.
     * @tparam T The type of coroutine task value which must satisfy
     * TaskValue
     * @tparam Alloc The type of allocator which must satisfy
     * HandlerAllocator
     * @param ex The executor to post the task to be executed on.
     * @param t The task to post on the executor. The result of the task is
     * discarded if the coroutine reaches its final suspend point without
     * exiting due to an unhandled exception. In case the coroutine exits
     * due to an unhandled exception, then `std::terminate()` will be
     * called.
     * @param alloc An allocator used to allocate the operation. The size
     * passed to its allocate method is in bytes and the alignment of the
     * allocator must be suitable for std::max_align_t
     */
    template <Executor Exec, ObservableAwaitable Awaitable,
              HandlerAllocator Alloc = default_io_allocator>
    void spawn(Exec& ex, Awaitable&& awaitable, const Alloc& alloc = Alloc()) {
        spawn(ex, std::move(awaitable), any_invcable, terminate_on_error,
              alloc);
    }

    /*!
     * @brief A spawner that spawns coroutines on a provided executor and
     * enables waiting for them to finish.
     * @tparam Exec The type of the executor used by the spawner.
     */
    template <Executor Exec>
    class spawner {
// #ifdef __clang__
#if 0
		static_assert(!std::is_same_v<Exec, any_executor>,
			"clang breaks spawn when HALO optimization is enalbed and any_executor is used");
#endif // __clang__

        class awaitable final : public detail::async_op_base, noncopyable {
            spawner& s;
            std::coroutine_handle<> waiter_;

        public:
            awaitable(spawner& s) noexcept
                : detail::async_op_base(detail::async_op_type::wait), s{s} {
            }

            constexpr bool await_ready() const noexcept {
                return false;
            }

            bool await_suspend(std::coroutine_handle<> waiter) noexcept {
                waiter_ = waiter;
                auto state = s.state_.synchronize();
                if (state->count == 0) {
                    return false;
                }
                state->waiters.add_wait_op(s.ex_, *this);
                return true;
            }

            constexpr void await_resume() const noexcept {
            }

        private:
            virtual void invoke_operation() override {
                waiter_.resume();
            }

            virtual any_executor&
            associated_executor() const noexcept override {
                return s.ex_;
            }
        };

        struct spawner_final_handler_t {
            spawner& s;
            void operator()() const noexcept {
                s.on_finished();
            }
        };

        friend awaitable;
        friend spawner_final_handler_t;

        struct state_t {
            std::size_t count = 0;
            async_wait_queue waiters;
        };

        Exec& ex_;

        sync_value<state_t, spinlock> state_;

        spawner_final_handler_t make_final_handler() noexcept {
            return {*this};
        }

    public:
        /*!
         * @brief The type of the executor used by the spawner.
         */
        using executor_type = Exec;

        /*!
         * @brief Construct a spawner with the provided
         * executor.
         * @param ex The executor which the spawner will spawn
         * tasks on.
         */
        spawner(Exec& ex) noexcept : ex_{ex} {
        }

        /*!
         * @brief Get a reference to the executor used by the
         * spawner.
         * @return A reference to the executor used by the
         * spawner.
         */
        executor_type& executor() noexcept {
            return ex_;
        }

        /*!
         * @brief Get a reference to the executor used by the
         * spawner.
         * @return A reference to the executor used by the
         * spawner.
         */
        const executor_type& executor() const noexcept {
            return ex_;
        }

        /*!
         * @brief Get the count of active spawned tasks by the
         * spawner.
         * @return The count of active spawned tasks by the
         * spawner.
         */
        std::size_t work_count() const noexcept {
            return state_->count;
        }

        /*!
         * @brief Async wait on the spawner until all spawned
         * tasks are completed and destroyed. If there is no any
         * active spawned task when the returned awaitable is
         * awaited, then the coroutine will not suspend. Note
         * that the wait operation will not start until the
         * returned awaitable is awaited.
         * @return An awaitable that is when awaited will start
         * the wait operation.
         */
        awaitable async_wait() noexcept {
            return awaitable{*this};
        }

        /*!
         * @brief Post an awaitable to be executed on the spawner
         * executor. While the coroutine will start executing on
         * the provided executor it is not guaranteed that the
         * coroutine will not switch to another executor after
         * any awaiting point. If all the handlers passed have
         * specializations of trait
         * `spawn_handler_bound_to_executor` that inherits
         * `std::false_type` then the result and error handlers
         * are not guaranteed to be executed on the provided
         * executor. Otherwise the handlers are guaranteed to be
         * executed on the provided executor.
         * @tparam T The type of coroutine task value which must
         * satisfy TaskValue
         * @tparam ResultHandler The type of the result handler
         * which must satisfy SpawnResultHandler<typename Awaitable::value_type>
         * @tparam ErrorHandler The type of the error handler
         * which must be callable with `std::exception_ptr`
         * @tparam Alloc The type of allocator which must
         * satisfy HandlerAllocator
         * @param t The task to post on the executor.
         * @param rhandler The result handler. This handler will
         * be called with the result of the task if the
         * coroutine reaches its final suspend point without
         * exiting due to an unhandled exception.
         * @param ehandler The error handler. This handler will
         * be called with parameter of type `std::exception_ptr`
         * if the coroutine exits due to an unhandled exception.
         * @param alloc An allocator used to allocate the
         * operation. The size passed to its allocate method is
         * in bytes and the alignment of the allocator must be
         * suitable for std::max_align_t
         */
        template <
            ObservableAwaitable Awaitable,
            SpawnResultHandler<typename Awaitable::value_type> ResultHandler,
            std::invocable<std::exception_ptr> ErrorHandler,
            HandlerAllocator Alloc = default_io_allocator>
        void spawn(Awaitable&& awaitable, ResultHandler&& rhandler,
                   ErrorHandler&& ehandler, const Alloc& alloc = Alloc()) {
            using op_t = detail::spawn_op<
                Exec, Awaitable, Alloc, std::remove_cvref_t<ResultHandler>,
                std::remove_cvref_t<ErrorHandler>, spawner_final_handler_t>;
            op_t* op = detail::allocate_op<op_t>(
                alloc, std::move(awaitable), ex_,
                std::forward<ResultHandler>(rhandler),
                std::forward<ErrorHandler>(ehandler), make_final_handler());
            ++state_->count;
            ex_.post(*op);
        }

        /*!
         * @brief Post an awaitable to be executed on the spawner
         * executor. While the coroutine will start executing on
         * the provided executor it is not guaranteed that the
         * coroutine will not switch to another executor after
         * any awaiting point. If all the handlers passed have
         * specializations of trait
         * `spawn_handler_bound_to_executor` that inherits
         * `std::false_type` then the result and error handlers
         * are not guaranteed to be executed on the provided
         * executor. Otherwise the handlers are guaranteed to be
         * executed on the provided executor.
         * @tparam T The type of coroutine task value which must
         * satisfy TaskValue
         * @tparam ResultHandler The type of the result handler
         * which must satisfy SpawnResultHandler<typename Awaitable::value_type>
         * @tparam Alloc The type of allocator which must
         * satisfy HandlerAllocator
         * @param t The task to post on the executor.
         * @param rhandler rhandler The result handler. This
         * handler will be called with the result of the task if
         * the coroutine reaches its final suspend point without
         * exiting due to an unhandled exception. In case the
         * coroutine exits due to an unhandled exception, then
         * `std::terminate()` will be called.
         * @param alloc An allocator used to allocate the
         * operation. The size passed to its allocate method is
         * in bytes and the alignment of the allocator must be
         * suitable for std::max_align_t
         */
        template <
            ObservableAwaitable Awaitable,
            SpawnResultHandler<typename Awaitable::value_type> ResultHandler,
            HandlerAllocator Alloc = default_io_allocator>
        void spawn(Awaitable&& awaitable, ResultHandler&& rhandler,
                   const Alloc& alloc = Alloc()) {
            spawn(std::move(awaitable), std::forward<ResultHandler>(rhandler),
                  terminate_on_error, alloc);
        }

        /*!
         * @brief Post an awaitable to be executed on the spawner
         * executor. While the coroutine will start executing on
         * the provided executor it is not guaranteed that the
         * coroutine will not switch to another executor after
         * any awaiting point. If all the handlers passed have
         * specializations of trait
         * `spawn_handler_bound_to_executor` that inherits
         * `std::false_type` then the result and error handlers
         * are not guaranteed to be executed on the provided
         * executor. Otherwise the handlers are guaranteed to be
         * executed on the provided executor.
         * @tparam T The type of coroutine task value which must
         * satisfy TaskValue
         * @tparam ErrorHandler The type of the error handler
         * which must be callable with `std::exception_ptr`
         * @tparam Alloc The type of allocator which must
         * satisfy HandlerAllocator
         * @param t The task to post on the executor.
         * @param ehandler The error handler. This handler will
         * be called with parameter of type `std::exception_ptr`
         * if the coroutine exits due to an unhandled exception.
         * In case the coroutine reaches its final suspend point
         * without an unhandled exception the error handler will
         * not be called and the coroutine result will be
         * discarded.
         * @param alloc An allocator used to allocate the
         * operation. The size passed to its allocate method is
         * in bytes and the alignment of the allocator must be
         * suitable for std::max_align_t
         */
        template <ObservableAwaitable Awaitable,
                  std::invocable<std::exception_ptr> ErrorHandler,
                  HandlerAllocator Alloc = default_io_allocator>
        void spawn(Awaitable&& awaitable, ErrorHandler&& ehandler,
                   const Alloc& alloc = Alloc())
            requires(!std::same_as<typename Awaitable::value_type,
                                   std::exception_ptr> &&
                     !SpawnResultHandler<ErrorHandler,
                                         typename Awaitable::value_type>)
        {
            spawn(std::move(awaitable), any_invcable,
                  std::forward<ErrorHandler>(ehandler), alloc);
        }

        /*!
         * @brief Post an awaitable to be executed on the spawner
         * executor. While the coroutine will start executing on
         * the provided executor it is not guaranteed that the
         * coroutine will not switch to another executor after
         * any awaiting point. If all the handlers passed have
         * specializations of trait
         * `spawn_handler_bound_to_executor` that inherits
         * `std::false_type` then the result and error handlers
         * are not guaranteed to be executed on the provided
         * executor. Otherwise the handlers are guaranteed to be
         * executed on the provided executor.
         * @tparam T The type of coroutine task value which must
         * satisfy TaskValue
         * @tparam ErrorHandler The type of the error handler
         * which must be callable with `std::exception_ptr`
         * @tparam Alloc The type of allocator which must
         * satisfy HandlerAllocator
         * @param t The task to post on the executor.
         * @param ewrapper The error handler. This handler will
         * be called with parameter of type `std::exception_ptr`
         * if the coroutine exits due to an unhandled exception.
         * In case the coroutine reaches its final suspend point
         * without an unhandled exception the error handler will
         * not be called and the coroutine result will be
         * discarded.
         * @param alloc An allocator used to allocate the
         * operation. The size passed to its allocate method is
         * in bytes and the alignment of the allocator must be
         * suitable for std::max_align_t
         */
        template <ObservableAwaitable Awaitable,
                  std::invocable<std::exception_ptr> ErrorHandler,
                  HandlerAllocator Alloc = default_io_allocator>
        void spawn(Awaitable&& awaitable,
                   error_handler_wrapper<ErrorHandler> ewrapper,
                   const Alloc& alloc = Alloc()) {
            spawn(std::move(awaitable), any_invcable,
                  std::move(ewrapper.handler), alloc);
        }

        /*!
         * @brief Post an awaitable to be executed on the spawner
         * executor. While the coroutine will start executing on
         * the provided executor it is not guaranteed that the
         * coroutine will not switch to another executor after
         * any awaiting point.
         * @tparam T The type of coroutine task value which must
         * satisfy TaskValue
         * @tparam Alloc The type of allocator which must
         * satisfy HandlerAllocator
         * @param t The task to post on the executor. The result
         * of the task is discarded if the coroutine reaches its
         * final suspend point without exiting due to an
         * unhandled exception. In case the coroutine exits due
         * to an unhandled exception, then `std::terminate()`
         * will be called.
         * @param alloc An allocator used to allocate the
         * operation. The size passed to its allocate method is
         * in bytes and the alignment of the allocator must be
         * suitable for std::max_align_t
         */
        template <ObservableAwaitable Awaitable,
                  HandlerAllocator Alloc = default_io_allocator>
        void spawn(Awaitable&& awaitable, const Alloc& alloc = Alloc()) {
            spawn(std::move(awaitable), any_invcable, terminate_on_error,
                  alloc);
        }

    private:
        void on_finished() noexcept {
            auto state = state_.synchronize();
            std::size_t n = --state->count;
            if (!n) {
                state->waiters.post(ex_);
            }
        }
    };

    template <class T, class Promise>
    concept Waitable = requires(T t) {
        { t.async_wait() } noexcept -> ValidAwaitable<Promise>;
    };

    /*!
     * @brief Execute a function, then wait on an async waitable object.
     * After wait the function result is returned. If the function has
     * thrown an exception the exception will be stored then rethrown after
     * wait. Typically the waited on object will be a strand or a spawner,
     * and the executed function will submit operations on this object.
     * @tparam Fn The type of the function to execute.
     * @tparam ...Args The types of the function arguments.
     * @tparam W The type of the waited on objects which must satisfy
     * Waitable.
     * @param w The object to async wait on.
     * @param f The function to execute with the passed arguments.
     * @param ...args The arguments to be passed to the function.
     * @return The result of the executed function.
     */
    template <Waitable<int> W, class Fn, class... Args>
        requires NonAwaitableFunctor<Fn, Args...>
    auto wait_on(W& w, Fn f, Args&&... args)
        -> task<std::invoke_result_t<Fn, Args...>> {
        using fn_result = std::invoke_result_t<Fn, Args...>;
        constexpr bool returns_void = std::is_same_v<fn_result, void>;
        using result_t =
            std::conditional_t<returns_void, std::monostate, fn_result>;

        std::optional<std::exception_ptr> ex_ptr;
        [[maybe_unused]] std::optional<result_t> result;

        try {
            if constexpr (returns_void) {
                f(std::forward<Args>(args)...);
            }
            else {
                result.emplace(f(std::forward<Args>(args)...));
            }
        }
        catch (...) {
            ex_ptr.emplace(std::current_exception());
        }

        co_await w.async_wait();
        if (ex_ptr) {
            std::rethrow_exception(*ex_ptr);
        }

        if constexpr (!returns_void) {
            co_return std::move(*result);
        }
    }

    /*!
     * @brief Await the awaitable returned by a function, then wait on an
     * async waitable object. After wait the awaitable result is returned.
     * If the awaitable has thrown an exception the exception will be stored
     * then rethrown after wait. Typically the waited on object will be a
     * strand or a spawner, and the awaited awaitable will submit operations
     * on this object.
     * @tparam Fn The type of the function which its returned awaitable will
     * be awaited.
     * @tparam ...Args The types of the function arguments.
     * @tparam W The type of the waited on objects which must satisfy
     * Waitable.
     * @param w The object to async wait on.
     * @param f The function to execute with the passed arguments, then
     * await its returned awaitable.
     * @param ...args The arguments to be passed to the function.
     * @return The result of the awaitable returned by the function.
     */
    template <Waitable<int> W, class Fn, class... Args>
        requires AwaitableFunctor<Fn, int, Args...>
    auto wait_on(W& w, Fn f, Args&&... args)
        -> task<awaitable_result<std::invoke_result_t<Fn, Args...>>> {
        using fn_result = awaitable_result<std::invoke_result_t<Fn, Args...>>;
        constexpr bool returns_void = std::is_same_v<fn_result, void>;
        using result_t =
            std::conditional_t<returns_void, std::monostate, fn_result>;

        std::optional<std::exception_ptr> ex_ptr;
        [[maybe_unused]] std::optional<result_t> result;
        try {
            if constexpr (returns_void) {
                co_await f(std::forward<Args>(args)...);
            }
            else {
                result.emplace(co_await f(std::forward<Args>(args)...));
            }
        }
        catch (...) {
            ex_ptr.emplace(std::current_exception());
        }
        co_await w.async_wait();
        if (ex_ptr) {
            std::rethrow_exception(*ex_ptr);
        }

        if constexpr (!returns_void) {
            co_return std::move(*result);
        }
    }

    /*!
     * @brief Create a spawner using the provided executor, pass it to a
     * function that will use it to spawn tasks, and wait on the spawner
     * until all spawned tasks are completed. If the function has thrown an
     * exception the exception will be stored then rethrown after wait.
     * @tparam Exec The type of the executor to construct the spawner with.
     * @tparam Fn The type of the function to execute with the spawner. It
     * must be callable with `spawner<Exec>&`
     * @param ex The executor to construct the spawner with.
     * @param f The function to pass the spawner to. It must be callable
     * with `spawner<Exec>&`
     * @return The result of the executed function.
     */
    template <Executor Exec, NonAwaitableFunctor<spawner<Exec>&> Fn>
    auto scoped_spawn(Exec& ex, Fn f)
        -> task<std::invoke_result_t<Fn, spawner<Exec>&>> {
        spawner<Exec> s{ex};
        co_return co_await wait_on(s, std::move(f), s);
    }

    /*!
     * @brief Create a spawner using the provided executor, pass it to a
     * function that will use it to spawn tasks, and wait on the spawner
     * until all spawned tasks are completed. If the function has thrown an
     * exception the exception will be stored then rethrown after wait.
     * @tparam Exec The type of the executor to construct the spawner with.
     * @tparam Fn The type of the function to execute with the spawner, then
     * await its returned awaitable. It must be callable with
     * `spawner<Exec>&`
     * @param ex The executor to construct the spawner with.
     * @param f The function to pass the spawner to, then await its returned
     * awaitable. It must be callable with `spawner<Exec>&`
     * @return The result of the awaitable returned by the function.
     */
    template <Executor Exec, AwaitableFunctor<int, spawner<Exec>&> Fn>
    auto scoped_spawn(Exec& ex, Fn f)
        -> task<awaitable_result<std::invoke_result_t<Fn, spawner<Exec>&>>> {
        spawner<Exec> s{ex};
        co_return co_await wait_on(s, std::move(f), s);
    }
} // namespace RAD_LIB_NAMESPACE
