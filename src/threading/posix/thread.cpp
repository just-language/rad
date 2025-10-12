#include <rad/threading/thread.h>
#include <pthread.h>
#include <sched.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

using namespace RAD_LIB_NAMESPACE;

void detail::join_thd(native_thread_handle_type& handle) {
    void* ret = nullptr;
    int res = ::pthread_join(handle.get(), &ret);
    if (res != 0) {
        throw std::system_error{os::make_system_error(res)};
    }
}

auto detail::start_thd(thread_fn_type* f, void* arg) noexcept
    -> thread_start_result_t {
    pthread_t thd = {};
    int res = ::pthread_create(&thd, nullptr, f, arg);
    if (res != 0) {
        return {};
    }
    return {native_thread_handle_type{thd}, thd};
}

void detail::detach_thd(native_thread_handle_type& handle) noexcept {
    int res = ::pthread_detach(handle.get());
    // assert(res == 0);
    ((void)res);
}

void detail::sleep_thd(std::chrono::nanoseconds nanos,
                       thread_sleep_duration_type duration) {
    nanos -= duration;
    struct timespec req, res;
    req.tv_sec = static_cast<time_t>(duration.count());
    req.tv_nsec = static_cast<long>(nanos.count());
    ::nanosleep(&req, &res);
}

thread_id this_thread::get_id() noexcept {
    return thread_id{::pthread_self()};
}

void this_thread::yield() noexcept {
    ::sched_yield();
}