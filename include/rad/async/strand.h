#pragma once
#include <rad/async/async_wait_queue.h>
#include <rad/async/checked_counter.h>
#include <rad/async/executor.h>
#include <rad/async/io_executor.h>
#include <rad/threading/synchronized_value.h>
#include <rad/threading/thread.h>
#include <rad/trackable.h>

namespace RAD_LIB_NAMESPACE {
    namespace detail {
        struct strand_empty_timer_executor {
            virtual ~strand_empty_timer_executor() = 0;

            virtual any_executor& as_any_executor() noexcept = 0;

            virtual void schedule_timer(detail::timer_impl&) noexcept = 0;

            virtual void cancel_timer(detail::timer_impl&) noexcept = 0;

            virtual void move_timers(detail::timer_impl& old_t,
                                     detail::timer_impl& new_t) noexcept = 0;

            virtual void add_timer_handler(detail::timer_impl&,
                                           detail::async_op_base&) noexcept = 0;
        };

        struct strand_empty_io_executor {
            using descriptor_data = int;
            using descriptor_data_ptr = void*;

            virtual ~strand_empty_io_executor() = 0;

            virtual bool is_async() const noexcept = 0;

            virtual void attach_handle(net::socket_handle&, descriptor_data&,
                                       std::error_code&) noexcept = 0;

#ifdef _WIN32
            virtual void attach_handle(os::file_handle& handle,
                                       descriptor_data& data,
                                       std::error_code& ec) noexcept = 0;
#endif // _WIN32

            virtual descriptor_data_ptr
            allocate_descriptor_data(std::error_code& ec) noexcept = 0;

            virtual void delete_descriptor_data(descriptor_data*) noexcept = 0;

            virtual any_executor& thread_pool_executor() = 0;
        };
    } // namespace detail

    /*!
     * @brief The base of strand.
     */
    class RAD_EXPORT_VTABLE strand_base
        : detail::async_op_base, // the strand itself is an operation
          public any_executor    // the strand alaways implements any_executor
                                 // since its inner executor must satisfy
                                 // Executor
    {
        using op_type = detail::async_op_base;
        using list_type = stack_forward_list<op_type>;
        struct state_t {
            enum class run_state {
                idle,
                pending,
                executing,
            };

            list_type ops;
            thread_id id;
            single_wait_operation wait_op;
            std::size_t work_count = 0;
            run_state runstate = run_state::idle;

            bool is_idle() const noexcept {
                return runstate == run_state::idle;
            }

            bool is_pending() const noexcept {
                return runstate == run_state::pending;
            }

            bool is_executing() const noexcept {
                return runstate == run_state::executing;
            }

            void set_idle() noexcept {
                runstate = run_state::idle;
            }

            void set_pending() noexcept {
                runstate = run_state::pending;
            }

            void set_executing() noexcept {
                runstate = run_state::executing;
            }
        };

        class awaitable;
        friend class awaitable;
        friend schedule_op<strand_base>;

    public:
        strand_base() noexcept
            : detail::async_op_base(detail::async_op_type::strand),
              any_executor(detail::executor_type::strand) {
        }

#ifndef NDEBUG
        RAD_EXPORT_DECL ~strand_base();
#endif // !NDEBUG

        // any_executor implementation

        RAD_EXPORT_DECL void post(op_type& op) noexcept override;

        RAD_EXPORT_DECL void post(list_type ops) noexcept;

        RAD_EXPORT_DECL void post_finished(op_type& op) noexcept override;

        RAD_EXPORT_DECL void post_finished(list_type ops) noexcept override;

        RAD_EXPORT_DECL void add_work(std::size_t n = 1) noexcept override;

        RAD_EXPORT_DECL void consume_work(std::size_t n) noexcept override;

        RAD_EXPORT_DECL void cancel_work() noexcept override;

        RAD_EXPORT_DECL std::size_t work_count() const noexcept override;

        /*!
         * @brief Check if the strand is currently executing on
         * the current thread.
         * @return true if the strand is currently executing on
         * the current thread, and false otherwise.
         */
        RAD_EXPORT_DECL bool
        running_on_current_thread() const noexcept override;

        /*!
         * @brief Check if the strand has pending work.
         * @return true if the strand has pending work, and
         * false otherwise.
         */
        bool has_work() const noexcept {
            return work_count() != 0;
        }

        /*!
         * @brief Check if the strand is currently running and
         * executing handlers.
         * @return true if the strand is currently running and
         * executing handlers, and false otherwise.
         */
        bool running() const noexcept {
            return state_->is_executing();
        }

        /*!
         * @brief Async wait the strand to finish executing
         * handlers posted to it. If the work count is already
         * zero, then the coroutine will not suspend and
         * continue executing. Otherwise the coroutine will be
         * suspended and resumed later on the strand inner
         * executor when the work count drops to zero again.
         * Note that the wait operation will not start until the
         * returned awaitable is awaited.
         * @return An awaitable that is when awaited will start
         * the async wait operation.
         */
        awaitable async_wait() noexcept;

        /*!
         * @brief Schedule the coroutine on the strand.
         * If the strand is currently executing on the this
         * thread the coroutine will not suspend and continue
         * executing. Otherwise the coroutine will be suspended
         * and resumed later on the strand executing thread.
         * Note that the schedule operation will not start until
         * the returned awaitable is awaited.
         * @return An awaitable that is when awaited will start
         * the async schedule operation.
         */
        schedule_op<strand_base> schedule() noexcept {
            return schedule_op{*this};
        }

    private:
        void do_post_finished(state_t& state, op_type& op) noexcept;

        void do_post_finished(state_t& state, list_type ops) noexcept;

        // returns true if the coroutine should be suspended
        // otherwise false
        bool try_schedule(detail::async_op_base& op) noexcept;

        void run();

        void after_run(list_type remaining_ops) noexcept;

        // async_op_base implementation

        RAD_EXPORT_DECL void invoke_operation() override;

        // associated_executor is implemented by derived class

        sync_value<state_t> state_;
    };

    class RAD_EXPORT_VTABLE strand_base::awaitable final
        : public detail::async_op_base {
        strand_base& st_;
        std::coroutine_handle<> waiter_;

    public:
        explicit awaitable(strand_base& st) noexcept
            : detail::async_op_base(detail::async_op_type::wait), st_{st} {
        }

        RAD_EXPORT_DECL ~awaitable();

        constexpr bool await_ready() const noexcept {
            return false;
        }

        RAD_EXPORT_DECL bool
        await_suspend(std::coroutine_handle<> waiter) noexcept;

        constexpr void await_resume() noexcept {
        }

        RAD_EXPORT_DECL void invoke_operation() override;

        RAD_EXPORT_DECL any_executor&
        associated_executor() const noexcept override;
    };

    inline auto strand_base::async_wait() noexcept -> awaitable {
        return awaitable{*this};
    }

    /*!
     * @brief Strand is a proxy executor that executes handlers posted to it
     * in sequential order. Thus ensuring no two handlers will execute
     * concurrently even if the underlying executor is running on multiple
     * threads. The strand will run on one of its inner executor threads.
     * @tparam Exec The inner executor type.
     */
    template <Executor Exec>
    class strand final
        : noncopyable,
          nonmovable, // strand is pinned
          public strand_base,
          public std::conditional_t<
              TimerExecutor<Exec>, timer_executor,
              detail::strand_empty_timer_executor>, // implement
                                                    // timer_executor if
                                                    // the inner executor
                                                    // does so
          public std::conditional_t<
              IoExecutor<Exec>, io_executor,
              detail::strand_empty_io_executor> // implement io_executor if
                                                // the inner executor does
                                                // so
    {
        static_assert(!ProxyExecutor<Exec>,
                      "strand can't be a proxy over a proxy executor");

        using op_type = detail::async_op_base;
        using list_type = stack_forward_list<op_type>;

        using io_base = std::conditional_t<IoExecutor<Exec>, io_executor,
                                           detail::strand_empty_io_executor>;

        using descriptor_data = typename io_base::descriptor_data;
        using descriptor_data_ptr = typename io_base::descriptor_data_ptr;

    public:
        /*!
         * @brief The inner executor type.
         */
        using inner_executor_type = Exec;

        /*!
         * @brief Construct strand with inner executor.
         * @param ex The inner executor.
         */
        strand(inner_executor_type& ex) noexcept : ex_{ex} {
        }

        /*!
         * @brief Get a reference to the inner executor.
         * @return A reference to the inner executor.
         */
        inner_executor_type& inner_executor() noexcept {
            return ex_;
        }

        /*!
         * @brief Get a reference to the inner executor.
         * @return A reference to the inner executor.
         */
        const inner_executor_type& inner_executor() const noexcept {
            return ex_;
        }

        /*!
         * @brief Get a reference to the inner executor casted
         * to any_executor.
         * @return A reference to the inner executor casted to
         * any_executor.
         */
        any_executor& inner_any_executor() noexcept {
            if constexpr (std::is_same_v<inner_executor_type, any_executor>) {
                return ex_;
            }
            else {
                return ex_.as_any_executor();
            }
        }

        /*!
         * @brief Cast this strand to any_executor since the
         * strand itself is an executor.
         * @return A reference to this strand casted to
         * any_executor.
         */
        any_executor& as_any_executor() noexcept override {
            return *this;
        }

        /*!
         * @brief Schedule a timer on the underlying executor if
         * it implements TimerExecutor.
         * @param t The timer to schedule.
         */
        void schedule_timer(detail::timer_impl& t) noexcept override {
            if constexpr (TimerExecutor<Exec>) {
                ex_.schedule_timer(t);
            }
        }

        /*!
         * @brief Delegate timer cancelation to the underlying
         * executor if it implements TimerExecutor.
         * @param t The timer to cancel.
         */
        void cancel_timer(detail::timer_impl& t) noexcept override {
            if constexpr (TimerExecutor<Exec>) {
                ex_.cancel_timer(t);
            }
        }

        /*!
         * @brief Delegate move timers to the underlying
         * executor if it implements TimerExecutor.
         * @param old_t The old timer
         * @param new_t The new timer
         */
        void move_timers(detail::timer_impl& old_t,
                         detail::timer_impl& new_t) noexcept override {
            if constexpr (TimerExecutor<Exec>) {
                ex_.move_timers(old_t, new_t);
            }
        }

        /*!
         * @brief Add timer handler to the underlying executor
         * if it implements TimerExecutor
         * @param t The timer to add its handler.
         * @param handler The handler to add.
         */
        void
        add_timer_handler(detail::timer_impl& t,
                          detail::timer_op_base& handler) noexcept override {
            if constexpr (TimerExecutor<Exec>) {
                add_work();
                ex_.add_timer_handler(t, handler);
            }
        }

        /*!
         * @brief Check if the underlying executor is an io
         * executor and it is async.
         * @return true if the underlying executor is an io
         * executor and it is async, and false otherwise.
         */
        bool is_async() const noexcept override {
            if constexpr (IoExecutor<Exec>) {
                return ex_.is_async();
            }
            return false;
        }

        /*!
         * @brief Delegate attach handle to the underlying
         * executor if it implements IoExecutor.
         * @param handle The handle to attach
         * @param data The handle associated data
         * @param ec Used to report errors
         */
        void attach_handle(net::socket_handle& handle, descriptor_data& data,
                           std::error_code& ec) noexcept override {
            if constexpr (IoExecutor<Exec>) {
                return ex_.attach_handle(handle, data, ec);
            }
        }

#ifdef _WIN32
        /*!
         * @brief Delegate attach handle to the underlying
         * executor if it implements IoExecutor.
         * @param handle The handle to attach
         * @param data The handle associated data
         * @param ec Used to report errors
         */
        void attach_handle(os::file_handle& handle, descriptor_data& data,
                           std::error_code& ec) noexcept override {
            if constexpr (IoExecutor<Exec>) {
                return ex_.attach_handle(handle, data, ec);
            }
        }
#endif // _WIN32

        /*!
         * @brief Delegate io descriptor data allocation to the
         * underlying executor if it implements IoExecutor.
         * @param ec Used to report errors
         * @return Pointer to the allocated io descriptor data,
         * or nullptr.
         */
        descriptor_data_ptr
        allocate_descriptor_data(std::error_code& ec) noexcept override {
            if constexpr (IoExecutor<Exec>) {
                return ex_.allocate_descriptor_data(ec);
            }
            else {
                return {};
            }
        }

        /*!
         * @brief Deleage delete of io descriptor data to the
         * underlying executor if it implements IoExecutor.
         * @param p Pointer to the allocated io descriptor data
         */
        void delete_descriptor_data(descriptor_data* p) noexcept override {
            if constexpr (IoExecutor<Exec>) {
                ex_.delete_descriptor_data(p);
            }
        }

        /*!
         * @brief Get the resolvers executor of the underlying
         * executor if it implements IoExecutor.
         * @return The resolvers executor of the underlying
         * executor if it implements IoExecutor, and the strand
         * itself otherwise.
         */
        any_executor& thread_pool_executor() override {
            if constexpr (IoExecutor<Exec>) {
                return ex_.thread_pool_executor();
            }
            else {
                return *this;
            }
        }

    private:
        // implemented for strand_base
        // the strand can't be posted to or executed on any
        // executor except its inner executor
        any_executor& associated_executor() const noexcept override {
            return ex_;
        }

        inner_executor_type& ex_;
    };

    /*!
     * @brief Rebind an async object to a strand executor whose inner
     * executor is the same as the current async object executor
     * @param st the strand executor to bind the object to
     * @param o the object to consume and bind to the strand executor. If
     * the object executor is not the same as the strand inner executor then
     * std::terminate() is called
     * @return the moved object after being bound to the strand
     */
    template <Executor Exec, RebindableAsyncObject<strand<Exec>> Object>
    Object rebind_executor(strand<Exec>& st, Object&& o)
        requires(std::is_rvalue_reference_v<Object &&>)
    {
        assert(std::addressof(st.inner_executor()) ==
               std::addressof(o.executor()));
        if (std::addressof(st.inner_executor()) !=
            std::addressof(o.executor())) {
            std::terminate();
        }
        return rebind_executor_helper<strand<Exec>, Object>::rebind(
            st, std::move(o));
    }
} // namespace RAD_LIB_NAMESPACE
