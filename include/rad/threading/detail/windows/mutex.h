#pragma once
#include <rad/libbase.h>

#include <atomic>
#include <cassert>
#include <chrono>
#include <system_error>
#include <tuple>

extern "C" {
struct _RTL_SRWLOCK;
using RTL_SRWLOCK = _RTL_SRWLOCK;
using SRWLOCK = RTL_SRWLOCK;
using PSRWLOCK = RTL_SRWLOCK*;

struct _RTL_CRITICAL_SECTION;
using CRITICAL_SECTION = _RTL_CRITICAL_SECTION;
using PCRITICAL_SECTION = _RTL_CRITICAL_SECTION*;
using LPCRITICAL_SECTION = _RTL_CRITICAL_SECTION*;

__declspec(dllimport) unsigned long __stdcall GetCurrentThreadId();

__declspec(dllimport) int __stdcall SwitchToThread();

__declspec(dllimport) void __stdcall AcquireSRWLockExclusive(PSRWLOCK SRWLock);

__declspec(dllimport) unsigned char __stdcall
TryAcquireSRWLockExclusive(PSRWLOCK SRWLock);

__declspec(dllimport) void __stdcall ReleaseSRWLockExclusive(PSRWLOCK SRWLock);

__declspec(dllimport) void __stdcall AcquireSRWLockShared(PSRWLOCK SRWLock);

__declspec(dllimport) unsigned char __stdcall
TryAcquireSRWLockShared(PSRWLOCK SRWLock);

__declspec(dllimport) void __stdcall ReleaseSRWLockShared(PSRWLOCK SRWLock);

__declspec(dllimport) void __stdcall
InitializeCriticalSection(LPCRITICAL_SECTION lpCriticalSection);

__declspec(dllimport) void __stdcall
EnterCriticalSection(LPCRITICAL_SECTION lpCriticalSection);

__declspec(dllimport) int __stdcall
TryEnterCriticalSection(LPCRITICAL_SECTION lpCriticalSection);

__declspec(dllimport) void __stdcall
LeaveCriticalSection(LPCRITICAL_SECTION lpCriticalSection);

__declspec(dllimport) void __stdcall
DeleteCriticalSection(LPCRITICAL_SECTION lpCriticalSection);
}

namespace RAD_LIB_NAMESPACE::detail {
    using native_exclusive_mutex_type = PSRWLOCK;
    using exclusive_mutex_storage = void*;
    using native_shared_mutex_type = PSRWLOCK;
    using shared_mutex_storage = void*;
    using native_recursive_mutex_type = LPCRITICAL_SECTION;
#ifdef _WIN64
    using recursive_mutex_storage = uint8_t[40];
#else
    using recursive_mutex_storage = uint8_t[24];
#endif // _WIN64

    inline unsigned long get_current_tid() noexcept {
        return GetCurrentThreadId();
    }

    inline void init_mutex(exclusive_mutex_storage& mtx) noexcept {
        mtx = nullptr;
    }

    inline void init_recursive_mutex(recursive_mutex_storage& mtx) noexcept {
        InitializeCriticalSection(
            reinterpret_cast<native_recursive_mutex_type>(mtx));
    }

    inline void destroy_mutex(native_exclusive_mutex_type) noexcept {
    }

    inline void destroy_mutex(native_recursive_mutex_type mtx) noexcept {
        DeleteCriticalSection(mtx);
    }

    inline void lock_mutex(native_exclusive_mutex_type mtx) noexcept {
        AcquireSRWLockExclusive(mtx);
    }

    inline void lock_mutex(native_recursive_mutex_type mtx) noexcept {
        EnterCriticalSection(mtx);
    }

    inline void lock_shared_mutex(native_shared_mutex_type mtx) noexcept {
        AcquireSRWLockShared(mtx);
    }

    inline bool try_lock_mutex(native_exclusive_mutex_type mtx) noexcept {
        return TryAcquireSRWLockExclusive(mtx) != 0;
    }

    inline bool try_lock_mutex(native_recursive_mutex_type mtx) noexcept {
        return TryEnterCriticalSection(mtx) != 0;
    }

    inline bool try_lock_shared_mutex(native_shared_mutex_type mtx) noexcept {
        return TryAcquireSRWLockShared(mtx) != 0;
    }

    inline void unlock_mutex(native_exclusive_mutex_type mtx) noexcept {
        ReleaseSRWLockExclusive(mtx);
    }

    inline void unlock_mutex(native_recursive_mutex_type mtx) noexcept {
        LeaveCriticalSection(mtx);
    }

    inline void unlock_shared_mutex(native_shared_mutex_type mtx) noexcept {
        ReleaseSRWLockShared(mtx);
    }
} // namespace RAD_LIB_NAMESPACE::detail

namespace RAD_LIB_NAMESPACE {
    class mutex;

    namespace detail {
        template <bool recursive>
        class lock_checks_base {
        public:
#ifndef NDEBUG
            void pre_lock() noexcept {
                if constexpr (!recursive) {
                    assert(owner_id != get_current_tid() &&
                           "locking a mutex which is "
                           "already locked by the "
                           "current thread");
                }
            }

            void after_lock() noexcept {
                owner_id = get_current_tid();
            }

            void after_lock_shared() noexcept {
                owner_id = get_current_tid();
                is_shared_lock = true;
            }

            void on_unlock() noexcept {
                assert(owner_id == get_current_tid() &&
                       "unlocking a mutex  which was not "
                       "locked by the current "
                       "thread");
                owner_id = 0;
            }

            void on_unlock_shared() noexcept {
                assert(owner_id == get_current_tid() &&
                       "unlocking a mutex  which was not "
                       "locked by the current "
                       "thread");
                assert(is_shared_lock && "unlocking a shared mutex  "
                                         "which was "
                                         "locked in exclusive mode");
                is_shared_lock = false;
            }

            void after_unlock() noexcept {
                ::SwitchToThread();
            }

            ~lock_checks_base() {
                assert(owner_id == 0 && "destroying a mutex while it is "
                                        "being "
                                        "locked");
            }

        private:
            std::atomic<uint32_t> owner_id = 0;
            bool is_shared_lock = false;
#else
            constexpr void pre_lock() noexcept {
            }

            constexpr void after_lock() noexcept {
            }

            constexpr void after_lock_shared() noexcept {
            }

            constexpr void on_unlock() noexcept {
            }

            constexpr void on_unlock_shared() noexcept {
            }

            constexpr void after_unlock() noexcept {
            }
#endif // !NDEBUG
        };

        void assume_locked(mutex& mtx);

        void assume_unlocked(mutex& mtx);
    } // namespace detail

    class mutex : detail::lock_checks_base<false>, pinned {
        using checks_base = detail::lock_checks_base<false>;

        friend void detail::assume_unlocked(mutex&);

        friend void detail::assume_locked(mutex&);

    public:
        using native_handle_type = detail::native_exclusive_mutex_type;

        mutex() noexcept {
            detail::init_mutex(lock_);
        }

        mutex(const mutex&) = delete;

        mutex& operator=(const mutex&) = delete;

        ~mutex() {
            detail::destroy_mutex(native_handle());
        }

        void lock() noexcept {
            checks_base::pre_lock();
            detail::lock_mutex(native_handle());
            checks_base::after_lock();
        }

        bool try_lock() noexcept {
            checks_base::pre_lock();
            bool owns_lock = detail::try_lock_mutex(native_handle()) != 0u;
            if (owns_lock) {
                checks_base::after_lock();
            }
            return owns_lock;
        }

        void unlock() noexcept {
            checks_base::on_unlock();
            detail::unlock_mutex(native_handle());
            checks_base::after_unlock();
        }

        native_handle_type native_handle() noexcept {
            return reinterpret_cast<native_handle_type>(std::addressof(lock_));
        }

    private:
        void assume_locked() noexcept {
            checks_base::after_lock();
        }

        void assume_unlocked() noexcept {
            checks_base::on_unlock();
        }

        detail::exclusive_mutex_storage lock_;
    };

    inline void detail::assume_locked(mutex& mtx) {
        mtx.assume_locked();
    }

    inline void detail::assume_unlocked(mutex& mtx) {
        mtx.assume_unlocked();
    }

    class recursive_mutex : private detail::lock_checks_base<true>, pinned {
        using checks_base = detail::lock_checks_base<true>;

    public:
        using native_handle_type = detail::native_recursive_mutex_type;

        recursive_mutex() noexcept {
            detail::init_recursive_mutex(lock_);
        }

        ~recursive_mutex() {
            detail::destroy_mutex(native_handle());
        }

        void lock() noexcept {
            checks_base::pre_lock();
            detail::lock_mutex(native_handle());
            checks_base::after_lock();
        }

        bool try_lock() noexcept {
            checks_base::pre_lock();
            bool owns_lock = detail::try_lock_mutex(native_handle());
            if (owns_lock) {
                checks_base::after_lock();
            }
            return owns_lock;
        }

        void unlock() noexcept {
            checks_base::on_unlock();
            detail::unlock_mutex(native_handle());
            checks_base::after_unlock();
        }

        native_handle_type native_handle() noexcept {
            return reinterpret_cast<native_handle_type>(&lock_[0]);
        }

    private:
        alignas(sizeof(void*)) detail::recursive_mutex_storage
            lock_; // sizeof(CRITICAL_SECTION) = 40 (x64) or 24 (x32)
    };

    class shared_mutex : pinned {
        using checks_base = detail::lock_checks_base<false>;

    public:
        using native_handle_type = detail::native_shared_mutex_type;

        shared_mutex() noexcept {
            detail::init_mutex(lock_);
        };

        ~shared_mutex() {
            detail::destroy_mutex(native_handle());
        }

        void lock() noexcept {
            // checks_base::pre_lock();
            detail::lock_mutex(native_handle());
            // checks_base::after_lock();
        }

        bool try_lock() noexcept {
            // checks_base::pre_lock();
            bool lock_result = detail::try_lock_mutex(native_handle());
            // if (lock_result)
            //	checks_base::after_lock();
            return lock_result;
        }

        void unlock() noexcept {
            // checks_base::on_unlock();
            detail::unlock_shared_mutex(native_handle());
        }

        void lock_shared() noexcept {
            // checks_base::pre_lock();
            detail::lock_shared_mutex(native_handle());
            // checks_base::after_lock_shared();
        }

        bool try_lock_shared() noexcept {
            // checks_base::pre_lock();
            bool owns_lock = detail::try_lock_shared_mutex(native_handle());
            // if (owns_lock)
            //	checks_base::after_lock_shared();
            return owns_lock;
        }

        void unlock_shared() noexcept {
            // checks_base::on_unlock_shared();
            detail::unlock_shared_mutex(native_handle());
        }

        native_handle_type native_handle() noexcept {
            return reinterpret_cast<native_handle_type>(&lock_);
        }

    private:
        detail::shared_mutex_storage lock_;
    };
} // namespace RAD_LIB_NAMESPACE