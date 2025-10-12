#pragma once
#include <rad/libbase.h>

#include <mutex>
#include <shared_mutex>
#include <utility>

namespace RAD_LIB_NAMESPACE {
    using mutex = std::mutex;

    using recursive_mutex = std::recursive_mutex;

    using shared_mutex = std::shared_mutex;

    namespace detail {
        inline void assume_locked(mutex&) {
        }

        inline void assume_unlocked(mutex&) {
        }
    } // namespace detail
} // namespace RAD_LIB_NAMESPACE
