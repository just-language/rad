#pragma once
#include <rad/libbase.h>
#include <rad/threading/mutex.h>

#include <functional>
#include <mutex>
#include <shared_mutex>

namespace RAD_LIB_NAMESPACE {
    struct init_mutex_t {};

    inline constexpr init_mutex_t init_mutex{};

    template <class T, class Lockable = mutex>
    class strict_lock_ptr {
    public:
        using guard_type = std::lock_guard<Lockable>;
        using unique_type = std::unique_lock<Lockable>;
        using shared_type = std::shared_lock<Lockable>;

        strict_lock_ptr(T& value, Lockable& lock) noexcept
            : value{value}, guard{lock} {
        }

        strict_lock_ptr(T& value, Lockable& lock, std::adopt_lock_t) noexcept
            : value{value}, guard{lock, std::adopt_lock} {
        }

        constexpr T& operator*() noexcept {
            return value;
        }

        constexpr const T& operator*() const noexcept {
            return value;
        }

        constexpr T* operator->() noexcept {
            return std::addressof(value);
        }

        constexpr const T* operator->() const noexcept {
            return std::addressof(value);
        }

    private:
        T& value;
        std::lock_guard<Lockable> guard;
    };

    template <class T, class Lockable = mutex>
    class const_strict_lock_ptr {
    public:
        using guard_type = std::lock_guard<Lockable>;
        using unique_type = std::unique_lock<Lockable>;
        using shared_type = std::shared_lock<Lockable>;

        const_strict_lock_ptr(const T& value, Lockable& lock) noexcept
            : value{value}, guard{lock} {
        }

        const_strict_lock_ptr(const T& value, Lockable& lock,
                              std::adopt_lock_t) noexcept
            : value{value}, guard{lock, std::adopt_lock} {
        }

        constexpr const T& operator*() const noexcept {
            return value;
        }

        constexpr const T* operator->() const noexcept {
            return std::addressof(value);
        }

    private:
        const T& value;
        std::lock_guard<Lockable> guard;
    };

    template <class T, class Lockable = mutex>
    class synchronized_value {
    public:
        using lock_ptr_type = strict_lock_ptr<T, Lockable>;
        using const_lock_ptr_type = const_strict_lock_ptr<T, Lockable>;
        using guard_type = typename lock_ptr_type::guard_type;
        using unique_type = typename lock_ptr_type::unique_type;
        using shared_type = typename lock_ptr_type::shared_type;

        template <class... Args,
                  bool valid = std::is_default_constructible_v<Lockable>,
                  std::enable_if_t<valid, int> = 0>
        synchronized_value(Args&&... args)
            : value{std::forward<Args>(args)...} {
        }

        template <class MutexArg, class... Args>
        synchronized_value(init_mutex_t, MutexArg&& arg, Args&&... args)
            : value{std::forward<Args>(args)...},
              lock{std::forward<MutexArg>(arg)} {
        }

        synchronized_value(const synchronized_value& other)
            : value{other.copy()} {
        }

        synchronized_value(synchronized_value&& other) noexcept
            : value{other.move()} {
        }

        synchronized_value& operator=(synchronized_value&& other) noexcept {
            auto [gaurd, val] = this->lock_guard();
            val = std::move(other.value);
            return *this;
        }

        synchronized_value& operator=(const synchronized_value& other) {
            auto [gaurd, val] = this->lock_guard();
            val = other.value;
            return *this;
        }

        template <
            class Arg,
            std::enable_if_t<!std::is_same_v<Arg, synchronized_value>, int> = 0>
        synchronized_value& operator=(Arg&& arg) {
            auto [gaurd, val] = this->lock_guard();
            val = std::forward<Arg>(arg);
            return *this;
        }

        std::pair<guard_type, T&> lock_guard() {
            return std::pair<guard_type, T&>(lock, std::ref(value));
        }

        std::pair<guard_type, const T&> lock_guard() const {
            return std::pair<guard_type, const T&>(lock, std::cref(value));
        }

        std::pair<std::lock_guard<Lockable>, T&> std_lock_guard() {
            return std::pair<std::lock_guard<Lockable>, T&>(lock,
                                                            std::ref(value));
        }

        std::pair<std::lock_guard<Lockable>, const T&> std_lock_guard() const {
            return std::pair<std::lock_guard<Lockable>, const T&>(
                lock, std::cref(value));
        }

        std::pair<unique_type, T&> unique_lock() {
            return std::pair<unique_type, T&>(lock, std::ref(value));
        }

        std::pair<unique_type, const T&> unique_lock() const {
            return std::pair<unique_type, const T&>(lock, std::cref(value));
        }

        std::pair<std::unique_lock<Lockable>, T&> std_unique_lock() {
            return std::pair<std::unique_lock<Lockable>, T&>(lock,
                                                             std::ref(value));
        }

        std::pair<std::unique_lock<Lockable>, const T&>
        std_unique_lock() const {
            return std::pair<std::unique_lock<Lockable>, const T&>(
                lock, std::cref(value));
        }

        std::pair<shared_type, T&> shared_lock() {
            return std::pair<shared_type, T&>(lock, std::ref(value));
        }

        std::pair<shared_type, const T&> shared_lock() const {
            return std::pair<shared_type, const T&>(lock, std::cref(value));
        }

        lock_ptr_type operator->() noexcept {
            return lock_ptr_type{value, lock};
        }

        const_lock_ptr_type operator->() const noexcept {
            return const_lock_ptr_type{value, lock};
        }

        lock_ptr_type synchronize() noexcept {
            return lock_ptr_type{value, lock};
        }

        const_lock_ptr_type synchronize() const noexcept {
            return const_lock_ptr_type{value, lock};
        }

        T copy() const {
            return *synchronize();
        }

        T move() {
            return std::move(*synchronize());
        }

        T copy_shared() const {
            shared_type do_lock(lock);
            return value;
        }

        T move_shared() const {
            shared_type do_lock(lock);
            return std::move(value);
        }

        template <class... Args>
        void assign(Args&&... args) {
            auto [gaurd, val] = this->lock_guard();
            val = {std::forward<Args>(args)...};
        }

        template <class Fn>
        void apply(Fn fn) {
            auto [gaurd, val] = this->lock_guard();
            fn(val);
        }

    private:
        T value;
        mutable Lockable lock;
    };

    template <class T, class Lockable = mutex>
    using sync_value = synchronized_value<T, Lockable>;

} // namespace RAD_LIB_NAMESPACE