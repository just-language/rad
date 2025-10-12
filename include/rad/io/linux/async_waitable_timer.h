#pragma once
#include <rad/async/io_executor.h> // for descriptor_data
#include <rad/io/linux/epoll.h>
#include <rad/libbase.h>
#include <rad/os_types.h>

#include <chrono>

namespace RAD_LIB_NAMESPACE::io {
    class epoll;
}

namespace RAD_LIB_NAMESPACE::detail {
    class async_waitable_timer {
    public:
        using clock_type = std::chrono::steady_clock;
        using time_point = clock_type::time_point;
        using duration = clock_type::duration;

        RAD_EXPORT_DECL async_waitable_timer(io::epoll& ep);

        RAD_EXPORT_DECL void set(duration timeout);

        RAD_EXPORT_DECL void cancel() noexcept;

        RAD_EXPORT_DECL void on_child_fork(io::epoll& ep,
                                           std::error_code& ec) noexcept;

        void close() noexcept {
            handle_.reset();
        }

    private:
        os::handle handle_;
        io::detail::descriptor_data data_;
    };

} // namespace RAD_LIB_NAMESPACE::detail