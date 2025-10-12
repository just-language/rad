#pragma once
#include <rad/libbase.h>
#include <rad/threading/synchronized_value.h>

#include <atomic>

namespace RAD_LIB_NAMESPACE {
#ifndef NDEBUG
    template <std::unsigned_integral T>
    class checked_counter {
        static constexpr T max_value = std::numeric_limits<T>::max();

    public:
        checked_counter() = default;

        explicit checked_counter(T init) noexcept : val_{init} {
        }

        checked_counter(checked_counter&& other) noexcept
            : val_{std::exchange(*other.val_, 0)} {
        }

        checked_counter& operator=(checked_counter&& other) noexcept {
            val_ = std::exchange(*other.val_, 0);
            return *this;
        }

        ~checked_counter() {
            assert(val_.copy() == 0 && "counter is destroyed but it is not "
                                       "0 ! some work will be leaked");
        }

        void store(T desired,
                   std::memory_order = std::memory_order_seq_cst) noexcept {
            val_.assign(desired);
        }

        T load(std::memory_order = std::memory_order_seq_cst) const noexcept {
            return val_.copy();
        }

        operator T() const noexcept {
            return val_.copy();
        }

        explicit operator bool() const noexcept {
            return val_.copy() != 0;
        }

        bool operator!() const noexcept {
            return val_.copy() == 0;
        }

        void reset() noexcept {
            val_ = T{0};
        }

        bool one_if(T cmp) {
            auto val = val_.synchronize();
            if (*val == cmp) {
                *val = 1;
                return true;
            }
            return false;
        }

        bool zero_if(T cmp) {
            auto val = val_.synchronize();
            if (*val == cmp) {
                *val = 0;
                return true;
            }
            return false;
        }

        T operator++() noexcept {
            auto val = val_.synchronize();
            assert(*val != max_value && "attempting to increment counter "
                                        "above max_value will wrap to 0");
            *val += 1;
            return *val;
        }

        T operator--() noexcept {
            auto val = val_.synchronize();
            assert(*val != 0 && "attempting to decrement counter below 0 "
                                "will wrap to max_value");
            *val -= 1;
            return *val;
        }

        T operator+=(T n) noexcept {
            auto val = val_.synchronize();
            assert(((*val + n) > *val) && "attempting to increment counter "
                                          "above max_value will wrap to 0");
            *val += n;
            return *val;
        }

        T operator-=(T n) noexcept {
            auto val = val_.synchronize();
            assert(((*val - n) < *val) && "attempting to decrement counter "
                                          "below 0 will wrap to max_value");
            *val -= n;
            return *val;
        }

        friend bool operator==(const checked_counter& lhs,
                               const checked_counter& rhs) noexcept {
            return lhs.val_.copy() == rhs.val_.copy();
        }

    private:
        sync_value<T> val_ = T{0};
    };
#else
    template <std::unsigned_integral T>
    class checked_counter {
    public:
        checked_counter() = default;

        explicit checked_counter(T init) noexcept : val_{init} {
        }

#ifndef RAD_DISABLE_EXECUTOR_THREADS
        checked_counter(checked_counter&& other) noexcept
            : val_{other.val_.exchange(0)} {
        }

        checked_counter& operator=(checked_counter&& other) noexcept {
            val_ = other.val_.exchange(0);
            return *this;
        }

        void
        store(T desired,
              std::memory_order order = std::memory_order_seq_cst) noexcept {
            val_.store(desired, order);
        }

        T load(std::memory_order order =
                   std::memory_order_seq_cst) const noexcept {
            return val_.load(order);
        }

        bool one_if(T cmp) {
            return val_.compare_exchange_strong(cmp, 1);
        }

        bool zero_if(T cmp) {
            return val_.compare_exchange_strong(cmp, 0);
        }
#else
        checked_counter(checked_counter&& other) noexcept
            : val_{std::exchange(other.val_, 0)} {
        }

        checked_counter& operator=(checked_counter&& other) noexcept {
            val_ = std::exchange(other.val_, 0);
            return *this;
        }

        void store(T desired,
                   std::memory_order = std::memory_order_seq_cst) noexcept {
            val_ = desired;
        }

        T load(std::memory_order = std::memory_order_seq_cst) const noexcept {
            return val_;
        }

        T one_if(T cmp) {
            if (val_ == cmp) {
                val_ = 1;
                return true;
            }
            return false;
        }

        T zero_if(T cmp) {
            if (val_ == cmp) {
                val_ = 0;
                return true;
            }
            return false;
        }
#endif // !RAD_DISABLE_EXECUTOR_THREADS
        operator T() const noexcept {
            return val_;
        }

        explicit operator bool() const noexcept {
            return val_ != 0;
        }

        bool operator!() const noexcept {
            return val_ == 0;
        }

        void reset() noexcept {
            val_ = 0;
        }

        T operator++() noexcept {
            return ++val_;
        }

        T operator--() noexcept {
            return --val_;
        }

        T operator+=(T n) noexcept {
            return val_ += n;
        }

        T operator-=(T n) noexcept {
            return val_ -= n;
        }

        friend bool operator==(const checked_counter& lhs,
                               const checked_counter& rhs) noexcept {
            return lhs.val_ == rhs.val_;
        }

    private:
#ifndef RAD_DISABLE_EXECUTOR_THREADS
        std::atomic<T> val_ = {0};
#else
        T val_ = T{0};
#endif // !RAD_DISABLE_EXECUTOR_THREADS
    };
#endif // !NDEBUG
} // namespace RAD_LIB_NAMESPACE
