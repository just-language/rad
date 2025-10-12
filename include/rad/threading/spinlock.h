#pragma once
#include <rad/libbase.h>

#include <atomic>
#include <cassert>

namespace RAD_LIB_NAMESPACE {
    class spinlock {
        std::atomic_flag lock_state = ATOMIC_FLAG_INIT;

    public:
        constexpr spinlock() = default;

        void lock() {
            while (lock_state.test_and_set(std::memory_order_acquire)) {
            }
        }

        void unlock() {
            lock_state.clear(std::memory_order_release);
        }

        ~spinlock() {
            assert(!lock_state.test_and_set(std::memory_order_acquire) &&
                   "spinLock::~spinLock() a spinlock mutex "
                   "is being "
                   "destroyed while it "
                   "is locked");
        }
    };
}; // namespace RAD_LIB_NAMESPACE
