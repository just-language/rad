#pragma once
#include <rad/async/executor.h>

namespace RAD_LIB_NAMESPACE {
    /*!
     * @brief Wrapper around intrusive linked list of async operations
     * to simplify managing pending async operations.
     */
    class async_wait_queue {
    public:
#ifndef NDEBUG
        ~async_wait_queue() {
            assert(ops_.empty() && "pending wait operations will be lost");
        }
#endif // !NDEBUG

        /*!
         * @brief Add an operation to the intrusive list and
         * increase the work count of the executor.
         * @param ex The executor to increase its work count.
         * @param op The operation to add to the list.
         */
        template <Executor Exec>
        void add_wait_op(Exec& ex, detail::async_op_base& op) noexcept {
            ops_.push_back(op);
            ex.add_work();
        }

        /*!
         * @brief Post pending operations to an executor in the
         * list and clear the list.
         * @param ex The executor to post the operations for
         * invocation on.
         */
        template <Executor Exec>
        void post(Exec& ex) noexcept {
            if (!ops_.empty()) {
                ex.post_finished(std::move(ops_));
            }
        }

        /*!
         * @brief Post the first operation to an executor and
         * remove it from the list. If the list is empty, this
         * is a no op.
         * @param ex The executor to post the first operation
         * for invocation on.
         */
        template <Executor Exec>
        void post_one(Exec& ex) noexcept {
            if (!ops_.empty()) {
                ex.post_finished(ops_.pop_front());
            }
        }

        /*!
         * @brief Check if the intrusive list is empty.
         * @return True if the intrusive list is empty,
         * otherwise false.
         */
        bool empty() const noexcept {
            return ops_.empty();
        }

    private:
        stack_forward_list<detail::async_op_base> ops_;
    };

    /*!
     * @brief Wrapper around non owned async operation
     * to simplify managing pending async operations.
     */
    class single_wait_operation {
    public:
#ifndef NDEBUG
        ~single_wait_operation() {
            assert(!op_ && "pending wait operation will be lost");
        }
#endif // !NDEBUG

        /*!
         * @brief Check if there is an operation.
         * @return True if there is an operation, otherwise
         * false.
         */
        bool has_op() const noexcept {
            return op_ != nullptr;
        }

        /*!
         * @brief Set the operation pointer to @p op and
         * increase the work count of executor @p ex.
         * @tparam Exec The executor type.
         * @param ex The executor to increase its work count.
         * @param op The operation to store its address.
         */
        template <Executor Exec>
        void set_op(Exec& ex, detail::async_op_base& op) noexcept {
            assert(!op_ && "single_wait_operation support only one "
                           "waiter");
            ex.add_work();
            op_ = &op;
        }

        /*!
         * @brief Post the pointed to operation to be invoked on
         * executor @p ex. If there is no operation, this is a
         * no op. After this call, there is no stored operation.
         * @tparam Exec The executor type.
         * @param ex The executor to post the operation for
         * invocation on.
         */
        template <Executor Exec>
        void post(Exec& ex) {
            if (op_) {
                ex.post_finished(*std::exchange(op_, nullptr));
            }
        }

    private:
        detail::async_op_base* op_ = nullptr;
    };
} // namespace RAD_LIB_NAMESPACE