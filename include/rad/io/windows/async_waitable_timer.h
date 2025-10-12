#pragma once
#include <rad/libbase.h>
#include <rad/os_types.h>

#include <chrono>

namespace RAD_LIB_NAMESPACE::detail {
    class async_waitable_timer {
    public:
        using clock_type = std::chrono::steady_clock;
        using time_point = clock_type::time_point;
        using duration = clock_type::duration;

        RAD_EXPORT_DECL async_waitable_timer();

        RAD_EXPORT_DECL void set(duration timeout);

        RAD_EXPORT_DECL void cancel() noexcept;

    private:
        os::handle handle_;
    };

} // namespace RAD_LIB_NAMESPACE::detail