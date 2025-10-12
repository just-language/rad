#pragma once
#ifdef _WIN32
#include <rad/io/windows/iocp_io_loop.h>
#elif __linux__
#include <rad/io/linux/epoll_io_loop.h>
#ifdef RAD_HAS_IO_URING
#include <rad/io/linux/io_uring_loop.h>
#endif // RAD_HAS_IO_URING
#include <condition_variable>
#include <mutex>
#else
#include <rad/io/bsd/kqueue_io_loop.h>
#include <condition_variable>
#include <mutex>
#endif // _WIN32
#include <rad/async/checked_counter.h>
#include <rad/async/executor.h>
#include <rad/async/io_executor.h>
#include <rad/async/timer.h>
#include <rad/threading/synchronized_value.h>
#include <rad/threading/thread_pool.h>

namespace RAD_LIB_NAMESPACE {
    /*!
     * @brief Fork-related event notifications.
     *
     * This is only used on UNIX systems.
     */
    enum class fork_event {
        /*!
         * @brief Notify the executor that the process is about to fork.
         */
        prepare,
        /*!
         * @brief Notify the executor that the process has forked and is the
         * parent.
         */
        parent,
        /*!
         * @brief Notify the executor that the process has forked and is the
         * child.
         */
        child,
    };

    /*!
     * @brief Specifies how to use io_uring with epoll under linux.
     *
     * io_uring will only be used if it is supported by the kernel
     * and the kernel supports IORING_FEAT_NODROP which is supported since
     * kernel 5.5.
     */
    enum class async_mode {
        /*!
         * @brief Enable standalone use of io_uring if it is available and don't
         * use epoll.
         */
        enabled,
        /*!
         * @brief Disable use of io_uring entirely, even if it is supported.
         */
        disabled,
        /*!
         * @brief Enable use of io_uring if available alongside epoll.
         * The io_uring backend will be used for all IO operations while
         * the epoll backendwill be used for timer and interruptions.
         */
        multiplex,
    };

    /*!
     * @brief IO executor that provides the core I/O functionality for users of
     * the asynchronous I/O objects.
     *
     * This executor also is a timer executor so it can be used with timers.
     */
    class io_loop final : public any_executor,
                          public timer_executor,
                          public io_executor {
        struct loop_thread_id : stack_double_list_node {
            thread_id id;

            loop_thread_id(thread_id id) noexcept : id{id} {
            }

            bool operator==(const loop_thread_id& other) const noexcept {
                return id == other.id;
            }
        };

        friend schedule_op<io_loop>;

        using async_op_t = detail::async_op_base;

    public:
        /*!
         * @brief Construct an IO loop
         * @param threads_num_hint The number of threads the
         * caller wish to run the loop on, if 0 is passed the
         * default is used. This parameter has an effect only on
         * windows.
         * @param mode When `async_mode::enabled` use the operation system
         * async API like io_uring on linux wihtout polling.
         *
         * When `async_mode::disabled` use the use the operation system
         * polling API like epoll on linux.
         *
         * When `async_mode::multiplex` use io_uring for IO operations
         * and epoll for timers and interruptions.
         *
         * This parameter has an effect only on linux.
         */
        RAD_EXPORT_DECL io_loop(uint32_t threads_num_hint = 0,
                                async_mode mode = async_mode::enabled);

#ifndef _WIN32
        RAD_EXPORT_DECL ~io_loop();
#endif // !_WIN32

        /*!
         * @brief Set the max number of threads the resolver
         * pool thread may run on. This has an effect only
         * before using the resolver threads and on platforms
         * that does not have native os async resolver. Only
         * Windows 8 and later versions of Windows has an async
         * resolver
         * @param n the number of threads the resolver pool
         * thread may run on
         */
        void set_max_resolver_threads(uint32_t n) noexcept {
            max_resolver_threads_ = max(n, 1);
        }

        /*!
         * @brief Get a reference to the thread pool used to
         * emulate async dns resolving. The thread pool will
         * only start after the first call to this method. The
         * number of threads used by the pool defaults to 1 and
         * can be changed by a call to set_max_resolver_threads
         * before calling this method and does not change after
         * the pool has started. The pool is valid for as long
         * as the the io_loop containing it is valid.
         * @return a reference to a thread_pool owned by the
         * io_loop
         */
        RAD_EXPORT_DECL thread_pool& resolver_thread_pool();

        RAD_EXPORT_DECL void post(async_op_t& op) noexcept override;

        /*!
         * @brief Post list of io operations to be invokde in
         * the loop. No additional allocation for the operation
         * is performed
         * @param ops the operations to post
         */
        RAD_EXPORT_DECL void post(stack_forward_list<async_op_t> ops) noexcept;

        RAD_EXPORT_DECL void post_finished(
            stack_forward_list<async_op_t> ready_ops) noexcept override;

        /*!
         * @brief Post two finished or canceled operations
         * without inrementing the work counter. Should be used
         * only by library io objects
         * @param op1 an operation to post
         * @param op2 an operation to post
         */
        RAD_EXPORT_DECL void post_finished(async_op_t& op1,
                                           async_op_t& op2) noexcept;

        RAD_EXPORT_DECL void post_finished(async_op_t& op) noexcept override;

        RAD_EXPORT_DECL any_executor& as_any_executor() noexcept override;

        RAD_EXPORT_DECL void add_work(std::size_t n = 1) noexcept override;

        RAD_EXPORT_DECL void consume_work(std::size_t n) noexcept override;

        RAD_EXPORT_DECL void cancel_work() noexcept override;

        RAD_EXPORT_DECL std::size_t work_count() const noexcept override;

        /*!
         * @brief Transfer the execution of a coroutine to one
         * of the io loop threads. If the coroutine is already
         * on a loop worker thread then it will continue without
         * suspension. Otherwise the coroutine is suspeneded and
         * then resumed on a loop worker thread
         * @return an awaitable type that is when awaited will
         * schedule the coroutine
         */
        RAD_EXPORT_DECL schedule_op<io_loop> schedule() noexcept;

        /*!
         * @brief interrupt (wake) the loop while it is waiting
         * for notifications in order to handle new posted
         * operations (work)
         */
        RAD_EXPORT_DECL void interrupt() noexcept;

        /*!
         * @return true if there is no longer pending work
         * (operations) , otherwise false
         */
        bool finished() const noexcept {
            return work_count() == 0;
        }

        /*!
         * @return true if there is still pending work
         * (operations), otherwise false
         */
        bool has_work() const noexcept {
            return !finished();
        }

        /*!
         * @brief Check if the current thread is executing run()
         * method for example if it is called from the handler
         * while being invoked by the loop
         * @return true if the loop is running on the current
         * thread, otherwise false if the loop is not running or
         * running on a different thread
         */
        RAD_EXPORT_DECL bool
        running_on_current_thread() const noexcept override;

        /*!
         * @brief Check if the loop is currently running and not
         * running on the current thread
         * @return true if the loop is running on a different
         * thread, otherwise false if the loop is not running or
         * running on the current thread
         */
        RAD_EXPORT_DECL bool running_not_on_current_thread() const noexcept;

        // io executor implementation

        RAD_EXPORT_DECL any_executor& thread_pool_executor() override;

#ifdef _WIN32
        bool is_async() const noexcept override {
            return true;
        }

        void attach_handle(net::socket_handle& sock, descriptor_data&,
                           std::error_code& ec) noexcept override {
            impl.attach_handle(sock, ec);
        }

        void attach_handle(os::file_handle& handle, descriptor_data&,
                           std::error_code& ec) noexcept override {
            impl.attach_handle(handle, ec);
        }

        descriptor_data_ptr
        allocate_descriptor_data(std::error_code& ec) noexcept override {
            ec.clear();
            return nullptr;
        }

        void delete_descriptor_data(descriptor_data*) noexcept override {
        }
#else
        bool is_async() const noexcept override {
#ifdef RAD_HAS_IO_URING
            return io_uring_loop_ != nullptr;
#else
            return false;
#endif // RAD_HAS_IO_URING
        }

        RAD_EXPORT_DECL void
        attach_handle(net::socket_handle& sock,
                      io::detail::descriptor_data& data,
                      std::error_code& ec) noexcept override;

        RAD_EXPORT_DECL descriptor_data_ptr
        allocate_descriptor_data(std::error_code& ec) noexcept override;

        RAD_EXPORT_DECL void
        delete_descriptor_data(descriptor_data* p) noexcept override;

        /*!
         * @brief Notify the executor of a fork-related event.
         *
         * This function is used to inform the executor that the process is
         * about to fork, or has just forked. This allows the executor to
         * perform any necessary housekeeping to ensure correct operation
         * following a fork.
         *
         * This function must not be called while any other executor function is
         * being called in another thread. It is, however, safe to call this
         * function from within a completion handler, provided no other thread
         * is accessing the executor.
         * @param event A fork-related event.
         * @param ec Used to indicate error occured, if any.
         */
        RAD_EXPORT_DECL void notify_fork(fork_event event,
                                         std::error_code& ec) noexcept;

        /*!
         * @brief Notify the executor of a fork-related event.
         *
         * This function is used to inform the executor that the process is
         * about to fork, or has just forked. This allows the executor to
         * perform any necessary housekeeping to ensure correct operation
         * following a fork.
         *
         * This function must not be called while any other executor function is
         * being called in another thread. It is, however, safe to call this
         * function from within a completion handler, provided no other thread
         * is accessing the executor.
         * @param event A fork-related event.
         */
        void notify_fork(fork_event event) {
            std::error_code ec;
            notify_fork(event, ec);
            check_and_throw(ec, __func__);
        }
#endif // _WIN32

        /*!
         * @brief run the io loop to perform ready IO and invoke
         * handlers of completed operations. this method returns
         * if :
         * -# there is no more work to do : has_work() == false
         * -# stop() has been called, in this case there is may
         * be pending operations in the queue
         * -# a system error is reported from the os functions
         * -# an exception is thrown and not caught from a
         * handler
         * @param ec an error code used to report errors from os
         * functions
         */
        RAD_EXPORT_DECL void run(std::error_code& ec);

        /*!
         * @brief run the io loop to perform ready IO and invoke
         * handlers of completed operations this method returns
         * if :
         *
         * - there is no more work to do : has_work() == false
         *
         * - stop() has been called, in this case there may
         * be pending operations in the queue
         *
         * - an exception is thrown from the os functions
         *
         * - an exception is thrown and not caught from a
         * handler
         */
        void run() {
            std::error_code ec;
            run(ec);
            check_and_throw(ec, __func__);
        }

        /*!
         * @brief prepare the io loop to start again after
         * stop() has been called or run() has returned. must be
         * called before calling run() again
         */
        RAD_EXPORT_DECL void restart() noexcept;

        /*!
         * @return true if the loop is currently running (run()
         * is being executed) on any thread, otherwise false
         */
        RAD_EXPORT_DECL bool running() const noexcept;

        /*!
         * @brief Request the loop to stop if it is currently
         * running and allow run() method to return. This does
         * not mean that the loop will stop immediately but it
         * may process some queued events before stopping. Non
         * processed events will still exist in the events queue
         */
        RAD_EXPORT_DECL void stop() noexcept;

        // timer executor implementation

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
#ifndef _WIN32
        bool run_main_once(std::error_code& ec);

        bool run_thd_once();

        void do_run(std::error_code& ec);

        void perform_pending_io_ops() noexcept;

        void do_perform(uintptr_t ident, void* ptr, uint32_t events) noexcept;

        void do_perform_reactor_in_out_ops(void* ptr, bool ready_in,
                                           bool ready_out) noexcept;
#endif // !WIN32

        void consume_work() noexcept;

        bool try_schedule(async_op_t& op) noexcept;

        void poll_timers() noexcept;

        void consume_finished_operations();

#ifndef _WIN32
        // implemented only for unix to delete fd data
        // associated with destroyed io objects
        void delete_destroyed_fds_data() noexcept;
#endif // !_WIN32

        io::detail::io_loop_impl impl;
        wait_queue timers_queue_;
        checked_counter<std::size_t>
            executing_thds_count_; // number of threads
                                   // currently performing
                                   // operations (not waiting)
        sync_value<stack_list<loop_thread_id>, executor_mutex> thd_ids_list_;

        uint32_t max_resolver_threads_ = 1;
        sync_value<thread_pool, executor_mutex> resolver_pool_;

#ifdef _WIN32
        checked_counter<std::size_t> work_count_;
        sync_value<stack_forward_list<async_op_t>, executor_mutex>
            finished_ops_;
        std::atomic<bool> stopped_ = true;
#else

        template <class T>
        class drainer {
        public:
            void set_objects(std::span<T> objects) noexcept {
                objects_ = objects;
            }

            bool empty() const noexcept {
                return objects_.empty();
            }

            T* try_pop() noexcept {
                if (objects_.empty()) {
                    return nullptr;
                }
                T* ptr = objects_.data();
                objects_ = objects_.subspan(1);
                return ptr;
            }

        private:
            std::span<T> objects_;
        };

#ifdef __linux__
        using reactor_event_type = io::epoll_event_t;
#else
        using reactor_event_type = io::kqueue_event;
#endif // __linux__

        std::vector<reactor_event_type> reactor_evs_;

        struct shared_state_t {
            stack_forward_list<async_op_t> finished_ops;
            drainer<reactor_event_type> pending_io_ops;
            std::size_t work_count = 0;
            uint32_t executing_threads = 0;
            bool waiting = false;
            bool stopped = false;

            bool should_stop() const noexcept {
                return stopped || work_count == 0;
            }

            bool should_wakeup() {
                return should_stop() || !finished_ops.empty() ||
                       !pending_io_ops.empty();
            }
        };

        sync_value<shared_state_t, std::mutex> thds_state_;
        std::condition_variable running_thds_cv_;

        // running threads (except main thread) will lock this
        // mutex in read mode while performing operations and
        // main thread will lock it in write mode upon deleting
        // the operations to ensure they are not being used by
        // another threads
        std::shared_mutex perform_lock_;
        sync_value<stack_list<io::detail::descriptor_data>> descriptors_data_;
        sync_value<stack_list<io::detail::descriptor_data>> pending_delete_fds_;
        std::atomic<bool> is_there_main_thread_ = false;
#ifdef RAD_HAS_IO_URING
        std::unique_ptr<io::io_uring_loop> io_uring_loop_;
        async_mode amode_ = async_mode::enabled;
#endif // RAD_HAS_IO_URING
#endif // _WIN32
    };
} // namespace RAD_LIB_NAMESPACE