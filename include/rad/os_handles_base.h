#pragma once
#include <rad/libbase.h>

#include <type_traits>

namespace RAD_LIB_NAMESPACE::os {
    template <class T, auto CloseFn>
    struct handle_deleter {
        using pointer = T;
        void operator()(pointer handle) {
            CloseFn(handle);
        }
    };

    template <class T, T null_value, bool MinusIsValid, class HTYPE>
    class zero_neg_handle {
        T handle;

    public:
        zero_neg_handle() noexcept : handle(null_value) {
        }

        zero_neg_handle(T handle) noexcept : handle(handle) {
        }

        template <class H = HTYPE,
                  std::enable_if_t<!std::is_same_v<H, T>, bool> = true>
        zero_neg_handle(HTYPE handle) noexcept
            : handle(reinterpret_cast<T>(handle)) {
        }

        zero_neg_handle(std::nullptr_t) noexcept : zero_neg_handle() {
        }

        [[nodiscard]] bool to_be_deleted() const noexcept {
            if constexpr (MinusIsValid) {
                return (long long)handle > 0;
            }
            else {
                return true;
            }
        }

        explicit operator bool() const noexcept {
            return handle != null_value;
        }

        template <class H = HTYPE,
                  std::enable_if_t<!std::is_same_v<H, T>, bool> = true>
        operator HTYPE() const noexcept {
            return reinterpret_cast<HTYPE>(handle);
        }

        operator T() const noexcept {
            return handle;
        }

        bool operator==(const zero_neg_handle& rhs) const noexcept {
            return handle == rhs.handle;
        }

        bool operator!=(const zero_neg_handle& rhs) const noexcept {
            return handle != rhs.handle;
        }

        bool operator==(std::nullptr_t) const noexcept {
            return handle == null_value;
        }

        bool operator!=(std::nullptr_t) const noexcept {
            return handle != null_value;
        }

        bool operator>(const zero_neg_handle& rhs) const noexcept {
            return handle > rhs.handle;
        }

        bool operator>=(const zero_neg_handle& rhs) const noexcept {
            return handle >= rhs.handle;
        }

        bool operator<(const zero_neg_handle& rhs) const noexcept {
            return handle < rhs.handle;
        }

        bool operator<=(const zero_neg_handle& rhs) const noexcept {
            return handle <= rhs.handle;
        }
    };

    template <class T, T null_value, bool MinusIsValid, class HTYPE,
              auto Deleter>
    struct zero_neg_handle_deleter {
        using pointer = zero_neg_handle<T, null_value, MinusIsValid, HTYPE>;

        void operator()(pointer p) noexcept {
            if (p.to_be_deleted()) {
                Deleter(p);
            }
        }
    };
} // namespace RAD_LIB_NAMESPACE::os