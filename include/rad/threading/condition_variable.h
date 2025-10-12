#pragma once
#include <rad/threading/mutex.h>

#include <chrono>
#include <mutex>

#ifdef _WIN32
extern "C" {
struct _RTL_CONDITION_VARIABLE;
using RTL_CONDITION_VARIABLE = _RTL_CONDITION_VARIABLE;
using CONDITION_VARIABLE = RTL_CONDITION_VARIABLE;
using PCONDITION_VARIABLE = CONDITION_VARIABLE*;
}

namespace RAD_LIB_NAMESPACE::detail {
    using condvar_storage_type = void*;
    using native_condvar_type = PCONDITION_VARIABLE;
    using condvar_time_unit = std::chrono::duration<unsigned long, std::milli>;

    inline native_condvar_type
    condvar_storage_to_native(condvar_storage_type& storage) noexcept {
        return reinterpret_cast<native_condvar_type>(&storage);
    }

    inline void init_condvar(condvar_storage_type& cvar) noexcept {
        cvar = nullptr;
    }

    inline void destroy_condvar(native_condvar_type) noexcept {
    }

    RAD_EXPORT_DECL void notify_one_condvar(native_condvar_type cvar) noexcept;

    RAD_EXPORT_DECL void notify_all_condvar(native_condvar_type cvar) noexcept;

    RAD_EXPORT_DECL void wait_condvar(native_condvar_type cvar,
                                      mutex* mtx) noexcept;

    RAD_EXPORT_DECL bool
    wait_condvar_timeout(native_condvar_type cvar, mutex* mtx,
                         condvar_time_unit timeout) noexcept;
} // namespace RAD_LIB_NAMESPACE::detail
#else
#include <pthread.h>
namespace RAD_LIB_NAMESPACE::detail {
    using condvar_storage_type = pthread_cond_t;
    using native_condvar_type = pthread_cond_t*;
    using condvar_time_unit = std::chrono::nanoseconds;

    inline native_condvar_type
    condvar_storage_to_native(condvar_storage_type& storage) noexcept {
        return &storage;
    }

    inline void init_condvar(condvar_storage_type& storage) noexcept {
        storage = PTHREAD_COND_INITIALIZER;
    }

    inline void destroy_condvar(native_condvar_type cvar) noexcept {
        pthread_cond_destroy(cvar);
    }

    inline void notify_one_condvar(native_condvar_type cvar) noexcept {
        pthread_cond_signal(cvar);
    }

    inline void notify_all_condvar(native_condvar_type cvar) noexcept {
        pthread_cond_broadcast(cvar);
    }

    inline void wait_condvar(native_condvar_type cvar, mutex* mtx) noexcept {
        pthread_cond_wait(cvar, mtx->native_handle());
    }

    inline bool wait_condvar_timeout(native_condvar_type cvar, mutex* mtx,
                                     condvar_time_unit timeout) noexcept {
        using namespace std::chrono;
        seconds secs = duration_cast<seconds>(timeout);
        timeout -= secs;
        struct timespec t;
        t.tv_sec = static_cast<time_t>(secs.count());
        t.tv_nsec = static_cast<long>(timeout.count());

        return pthread_cond_timedwait(cvar, mtx->native_handle(), &t) == 0;
    }
} // namespace RAD_LIB_NAMESPACE::detail
#endif // _WIN32

namespace RAD_LIB_NAMESPACE {

    enum class cv_status { no_timeout, timeout };

    class condition_variable {
        condition_variable(const condition_variable&) = delete;

        condition_variable(condition_variable&&) = delete;

    public:
        using native_handle_type = detail::native_condvar_type;

        condition_variable() noexcept {
            detail::init_condvar(cvar);
        }

        ~condition_variable() {
            detail::destroy_condvar(native_handle());
        }

        void notify_one() noexcept {
            detail::notify_one_condvar(native_handle());
        }

        void notify_all() noexcept {
            detail::notify_all_condvar(native_handle());
        }

        void wait(std::unique_lock<mutex>& lock) noexcept {
            wait(*lock.mutex());
        }

        template <class Predicate>
        void wait(std::unique_lock<mutex>& lock, Predicate pred) {
            while (!pred()) {
                wait(lock);
            }
        }

    private:
        void wait(mutex& lock) noexcept {
            detail::assume_unlocked(lock);
            detail::wait_condvar(native_handle(), &lock);
            detail::assume_locked(lock);
        }

    public:
        template <class Rep, class Period>
        cv_status
        wait_for(std::unique_lock<mutex>& lock,
                 const std::chrono::duration<Rep, Period>& rel_time) noexcept {
            return wait_for(*lock.mutex(), rel_time);
        }

        template <class Rep, class Period, class Predicate>
        bool wait_for(std::unique_lock<mutex>& lock,
                      const std::chrono::duration<Rep, Period>& rel_time,
                      Predicate pred) {
            while (!pred()) {
                if (wait_for(lock, rel_time) == cv_status::timeout) {
                    return pred();
                }
            }
            return true;
        }

    private:
        template <class Rep, class Period>
        cv_status
        wait_for(mutex& lock,
                 const std::chrono::duration<Rep, Period>& rel_time) noexcept {
            return wait_until(lock,
                              std::chrono::steady_clock::now() + rel_time);
        }

    public:
        template <class Clock, class Duration>
        cv_status wait_until(std::unique_lock<mutex>& lock,
                             const std::chrono::time_point<Clock, Duration>&
                                 timeout_time) noexcept {
            return wait_until(*lock.mutex(), timeout_time);
        }

        template <class Clock, class Duration, class Predicate>
        bool
        wait_until(std::unique_lock<mutex>& lock,
                   const std::chrono::time_point<Clock, Duration>& timeout_time,
                   Predicate pred) {
            while (!pred()) {
                if (wait_until(lock, timeout_time) == cv_status::timeout) {
                    return pred();
                }
            }
            return true;
        }

    private:
        template <class Clock, class Duration>
        cv_status wait_until(mutex& lock,
                             const std::chrono::time_point<Clock, Duration>&
                                 timeout_time) noexcept {
            using namespace std::chrono;
            auto now = Clock::now();
            if (now > timeout_time) {
                return cv_status::timeout;
            }
            auto timeout =
                duration_cast<detail::condvar_time_unit>(timeout_time - now);

            detail::assume_unlocked(lock);
            return detail::wait_condvar_timeout(native_handle(), &lock, timeout)
                       ? cv_status::no_timeout
                       : cv_status::timeout;
            detail::assume_locked(lock);
        }

    public:
        native_handle_type native_handle() noexcept {
            return detail::condvar_storage_to_native(cvar);
        }

    private:
        detail::condvar_storage_type cvar;
    };

    void notify_all_at_thread_exit(condition_variable& cond,
                                   std::unique_lock<mutex> lock);

}; // namespace RAD_LIB_NAMESPACE