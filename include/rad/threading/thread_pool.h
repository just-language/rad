#pragma once
#include <rad/async/executor.h>
#include <rad/async/timer.h>
#include <rad/threading/condition_variable.h>
#include <rad/threading/synchronized_value.h>
#include <rad/threading/thread.h>
#ifdef _WIN32
#include <rad/io/windows/sync_waitable_timer.h>
#else
#include <rad/io/posix/sync_waitable_timer.h>
#endif // _WIN32
#include <variant>
#include <vector>

namespace RAD_LIB_NAMESPACE {
    /*!
     * @brief A fixed-size thread pool executor that executes submitted
     * handlers on different threads concurrently. The thread pool also
     * implements `timer_executor` so it can be used with timers.
     */
    class RAD_EXPORT_VTABLE thread_pool final : public noncopyable,
                                                public any_executor,
                                                public timer_executor {
        friend schedule_op<thread_pool>;

        using op_type = detail::async_op_base;

    public:
        /*!
         * @brief Construct a thread pool.
         * The thread pool is not yet running after
         * construction.
         */
        thread_pool() noexcept
            : any_executor(detail::executor_type::thread_pool) {
        }

        /*!
         * @brief Construct and start a thread pool.
         * The thread pool will start to run after construction.
         * @param n The number of threads to use.
         */
        explicit thread_pool(uint32_t n)
            : any_executor(detail::executor_type::thread_pool) {
            start(n);
        }

        /*!
         * @brief Check if the thread pool is currently running
         * on any thread.
         * @return True if the thread pool is currently running,
         * and false otherwise.
         */
        bool running() const noexcept {
            return state_->workers_num >= 1;
        }

        /*!
         * @brief Start the thread pool.
         * The thread pool can be started once, and any attempt
         * to start it again will result in exception thrown.
         * @param n The number of threads to use.
         * @throw On error `std::system_error` is thrown.
         */
        RAD_EXPORT_DECL void start(uint32_t n = 1);

        /*!
         * @brief Stop the thread pool if it is currently
         * running. This method will wait until all threads of
         * the thread pool has exited. Not yet processed
         * operations are not discarded and will be in the
         * operations queue. The thread pool can be started
         * again after being stopped. Calling this function on
         * one of the pool worker threads may cause deadlock.
         */
        RAD_EXPORT_DECL void stop() noexcept;

        /*!
         * @brief Wait for the pool to stop.
         * If the pool is not currently running this function
         * returns immediately. Calling this function on one of
         * the pool worker threads may cause deadlock.
         */
        void wait() noexcept;

        /*!
         * @brief stop() is called by the thread pool
         * destructor.
         */
        RAD_EXPORT_DECL ~thread_pool();

        /*!
         * @brief Get the count of thread pool running threads.
         * @return The count of thread pool running threads.
         */
        std::uint32_t threads_count() const noexcept {
            return state_->workers_num;
        }

        // any_executor implementation

        RAD_EXPORT_DECL void post(op_type& op) noexcept override;

        RAD_EXPORT_DECL void post_finished(op_type& op) noexcept override;

        RAD_EXPORT_DECL void
        post_finished(stack_forward_list<op_type> ops) noexcept override;

        RAD_EXPORT_DECL void add_work(std::size_t n = 1) noexcept override;

        RAD_EXPORT_DECL void cancel_work() noexcept override;

        RAD_EXPORT_DECL void consume_work(std::size_t n) noexcept override;

        RAD_EXPORT_DECL std::size_t work_count() const noexcept override;

        RAD_EXPORT_DECL bool
        running_on_current_thread() const noexcept override;

        // timer executor implementation

        RAD_EXPORT_DECL any_executor& as_any_executor() noexcept override;

        RAD_EXPORT_DECL schedule_op<thread_pool> schedule() noexcept;

        RAD_EXPORT_DECL void
        schedule_timer(detail::timer_impl& t) noexcept override;

        RAD_EXPORT_DECL void
        cancel_timer(detail::timer_impl& t) noexcept override;

        RAD_EXPORT_DECL void
        move_timers(detail::timer_impl& old_t,
                    detail::timer_impl& new_t) noexcept override;

        RAD_EXPORT_DECL void
        add_timer_handler(detail::timer_impl& t,
                          detail::timer_op_base& handler) noexcept override;

    private:
        // state shared between threads
        struct shared_state_t {
            stack_forward_list<op_type> jobs;
            std::variant<std::monostate, scoped_thread,
                         std::vector<scoped_thread>>
                workers;
            scoped_thread timer_worker;

            std::uint32_t workers_num = 0;
            bool stopped = false;
        };

        void start_timer_thread_if_not(shared_state_t& state);

        // returns true if the couroutine have to suspend, like
        // await_suspend
        bool try_schedule(op_type& job) noexcept;

        void work_fn();

        void timer_fn();

        void execute_operations();

        // std::variant<std::monostate, scoped_thread,
        // std::vector<scoped_thread>> workers_;
        // std::atomic<uint32_t> workers_num_ = 0;

        wait_queue timers_queue_;
        detail::sync_waitable_timer timer_;

        sync_value<shared_state_t> state_;
        condition_variable queue_cv_;
        condition_variable wait_cv_;
    };

    static_assert(Executor<thread_pool>,
                  "thread_pool does not meet Executor requirements");
    static_assert(TimerExecutor<thread_pool>,
                  "thread_pool does not meet TimerExecutor requirements");
} // namespace RAD_LIB_NAMESPACE
