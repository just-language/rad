#pragma once
#include <rad/libbase.h>

#include <string>
#include <system_error>

namespace RAD_LIB_NAMESPACE::detail {
    template <class T, class... Args>
    inline constexpr bool is_one_variadic_arg = false;

    template <class T>
    inline constexpr bool is_one_variadic_arg<T> = true;

    template <class T, class... Args>
    inline constexpr bool copy_move_ctor_arg =
        is_one_variadic_arg<Args...> &&
        (std::is_same_v<T, const T&> || std::is_same_v<T, T&> ||
         std::is_same_v<T, T&&>);
} // namespace RAD_LIB_NAMESPACE::detail

namespace RAD_LIB_NAMESPACE::sync {
    enum class channel_error_code {
        no_error,
        recv,
        send,
        send_consumed,
        recv_consumed,
        send_detached,
        recv_detached,
        consumed,
    };

    RAD_EXPORT_DECL const std::error_category& channel_category() noexcept;

    inline void throw_channel_error(channel_error_code code) {
        throw std::system_error{static_cast<int>(code), channel_category()};
    }
} // namespace RAD_LIB_NAMESPACE::sync