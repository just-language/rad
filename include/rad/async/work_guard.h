#pragma once
#include <rad/async/executor.h>
#include <rad/libbase.h>

namespace RAD_LIB_NAMESPACE {
    /*!
     * @brief A work guard adds work to the executor on construction and
     * cancels this work on destruction or when reset() is called
     * explicitly. This keeps executors like io_loop running even if there
     * is no actual io work until the work guard is destroyed or reset.
     * @tparam Exec The type of executor.
     */
    template <Executor Exec>
    class work_guard {
    public:
        using executor_type = Exec;

        /*!
         * @brief Default construct the work guard object with
         * no bound executor
         */
        work_guard() = default;

        /*!
         * @brief Construct the work guard object and bind it to
         * an executor to prevent its run method from returning
         * if there is no more work until the work guard is
         * destoryed, moved to it or it or reset method is
         * called
         * @param ex The executor to bind to
         */
        explicit work_guard(executor_type& ex) noexcept : ex_{&ex} {
            ex.add_work();
        }

        /*!
         * @brief Move construct from another work guard and
         * take its executor if it has any
         * @param other Another work guard object to take its
         * executor
         * @post
         * other.has_work() == false
         * this->has_work() returns the value of
         * other.has_work() before move
         */
        work_guard(work_guard&& other) noexcept
            : ex_{std::exchange(other.ex_, nullptr)} {
        }

        /*!
         * @brief Move assign to another work guard and take its
         * executor if it has any, reset is called prior to
         * movement
         * @param other Another work guard object to take its
         * executor
         * @return The work guard itself
         * @post
         * other.has_work() == false
         * this->has_work() returns the value of
         * other.has_work() before move
         */
        work_guard& operator=(work_guard&& other) noexcept {
            reset();
            ex_ = std::exchange(other.ex_, nullptr);
            return *this;
        }

        ~work_guard() {
            reset();
        }

        /*!
         * @brief If bound to an executor detach from it and
         * allow its run method to return if there is no more
         * work
         * @post
         * has_work() == false
         */
        void reset() noexcept {
            if (ex_) {
                std::exchange(ex_, nullptr)->cancel_work();
            }
        }

        /*!
         * @brief If bound to an executor detach from it and
         * keep the outstanding work without canceling it
         * @post
         * has_work() == false
         */
        void release() noexcept {
            ex_ = nullptr;
        }

        /*!
         * @brief Check if bound to an executor and holds work
         * for it
         * @return true if bound to an executor, false otherwise
         */
        bool has_work() const noexcept {
            return ex_;
        }

    private:
        pointer<Exec> ex_ = nullptr;
    };
} // namespace RAD_LIB_NAMESPACE