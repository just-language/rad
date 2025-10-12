#pragma once
#include <rad/libbase.h>
#include <rad/os_types.h>
#ifndef __linux__
#include <rad/io/bsd/kqueue.h>
#endif // !__linux__

#include <chrono>

namespace RAD_LIB_NAMESPACE::detail {
    class sync_waitable_timer {
    public:
        using clock_type = std::chrono::steady_clock;
        using time_point = clock_type::time_point;
        using duration = clock_type::duration;

        RAD_EXPORT_DECL sync_waitable_timer();

        RAD_EXPORT_DECL void wait(duration timeout);

        RAD_EXPORT_DECL void cancel() noexcept;

    private:
#ifdef __linux__
        os::handle handle_;
#else
        static constexpr uintptr_t event_id = 2;
        io::kqueue_handle kq_;
#endif // __linux__
    };

} // namespace RAD_LIB_NAMESPACE::detail