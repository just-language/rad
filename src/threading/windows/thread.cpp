#include <Windows.h>
#include <rad/threading/thread.h>

using namespace RAD_LIB_NAMESPACE;

void detail::join_thd(native_thread_handle_type& handle) {
    DWORD ret =
        WaitForSingleObject(handle.get(), std::numeric_limits<DWORD>::max());
    if (ret == WAIT_FAILED) {
        throw std::system_error{os::make_system_error(GetLastError())};
    }
}

auto detail::start_thd(thread_fn_type* f, void* arg) noexcept
    -> thread_start_result_t {
    thread_id_type id = {};
    void* thd =
        reinterpret_cast<void*>(_beginthreadex(nullptr, 0, f, arg, 0, &id));
    if (!thd) {
        return {};
    }
    return {native_thread_handle_type{thd}, id};
}

void detail::detach_thd(native_thread_handle_type& handle) noexcept {
    handle.reset();
}

void detail::sleep_thd(std::chrono::nanoseconds,
                       thread_sleep_duration_type duration) {
    ::Sleep(duration.count());
}

thread_id this_thread::get_id() noexcept {
    return thread_id{::GetCurrentThreadId()};
}

void this_thread::yield() noexcept {
    ::SwitchToThread();
}