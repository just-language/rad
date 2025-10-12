#pragma once
#include <rad/async/async_phaser.h>

namespace RAD_LIB_NAMESPACE {
    /*!
     * @brief Async manual reset event that can be used to
     * wait for other tasks to set the event.
     *
     * Although async event is used by different operations and tasks,
     * it is not thread safe and may not be accessed by different threads
     * concurrently. So, the executor used by the event must be strand
     * or an executor that runs on only one thread.
     *
     * The event supports multiple wait operations at the same time.
     *
     * The event is not set by default, and this can be changed on
     * construction.
     *
     * When a wait operation is performed on a set (ready) event, the
     * operation will immediately be posted for invocation.
     *
     * When a wait operation is performed on a non set event, the operation
     * will not be posted for invocation until the event is set.
     *
     * To set the event manually call set().
     * This will cause pending wait operations to be posted
     * for invocation.
     * Later wait operations will be posted for invocation without waiting.
     *
     * To reset the event again and call reset(), which will make the event
     * non ready, and wait operations will not be invoked until the event is
     * set.
     */
    class async_event : async_phaser {
        using base = async_phaser;

    public:
        /// The type of executor. (any_executor)
        using executor_type = typename base::executor_type;

        /*!
         * @brief Construct an async event with executor and
         * initial set.
         * @param ex The executor that the event will use to
         * dispatch handlers for asynchronous wait operations
         * performed on the event.
         * @param init_set If true, the event will be set after
         * construction. Otherwise the event will not be set
         * until set() is called. The default value is false.
         */
        async_event(executor_type& ex, bool init_set = false) noexcept
            : base(ex, !init_set) {
        }

        using base::executor;

        using base::async_wait;

        using base::operator co_await;

        /*!
         * @brief Check if the event is currently set (ready).
         * @return True if the event is currently set, otherwise
         * false.
         */
        bool is_set() const noexcept {
            return base::is_ready();
        }

        /*!
         * @brief Set the event if it is not set.
         * Pending wait operations will be posted for
         * invocation. The event is now considered ready and
         * further wait operations will be posted for invocation
         * immediately until the event is reset.
         */
        void set() noexcept {
            bool was_not_set = count_.zero_if(1);
            if (was_not_set) {
                waiters_.post(executor());
            }
        }

        /*!
         * @brief Reset the event if it is set.
         * Further wait operations will not be posted for
         * invocation until the event is set.
         */
        void reset() noexcept {
            count_.one_if(0);
        }
    };
} // namespace RAD_LIB_NAMESPACE