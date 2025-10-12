#include <rad/io/bsd/kqueue.h>
#include <sys/param.h>

using namespace RAD_LIB_NAMESPACE;
using namespace io;

namespace {
    template <class Fn, class... Args>
    std::invoke_result_t<Fn, Args...> exec_while_eintr(Fn fn, Args&&... args) {
        while (1) {
            auto result = fn(std::forward<Args>(args)...);
            if (result == -1 && errno == EINTR) {
                continue;
            }
            return result;
        }
    }

#if defined(__NetBSD__) && __NetBSD_Version__ < 999001500
    intptr_t as_kqueue_udata(void* data) {
        return reinterpret_cast<intptr_t>(data);
    }

    constexpr int64_t as_kqueue_time(std::chrono::nanoseconds rel_time) {
        using namespace std::chrono;
        return duration_cast<milliseconds>(rel_time).count();
    }

    constexpr uint32_t kq_timer_fflags = 0;
#else
    constexpr void* as_kqueue_udata(void* data) {
        return data;
    }

    constexpr int64_t as_kqueue_time(std::chrono::nanoseconds rel_time) {
        return rel_time.count();
    }

    constexpr uint32_t kq_timer_fflags = NOTE_NSECONDS;
#endif
} // namespace

void kqueue_handle::create(std::error_code& ec) noexcept {
    ec.clear();
    int kfd = ::kqueue();
    if (kfd == -1) {
        ec = os::make_system_error(errno);
        return;
    }
    kq_fd_.reset(kfd);
}

void kqueue_handle::attach_handle(int fd, void* data,
                                  std::error_code& ec) noexcept {
    ec.clear();
    struct kevent kev{};
    EV_SET(&kev, fd, EVFILT_READ, EV_ADD | EV_ENABLE | EV_CLEAR, 0, 0,
           as_kqueue_udata(data));
    const int res = ::kevent(kq_fd_.get(), &kev, 1, nullptr, 0, nullptr);
    if (res == -1) {
        ec = os::make_system_error(errno);
    }
}

void kqueue_handle::attach_writable_handle(int fd, void* data,
                                           std::error_code& ec) noexcept {
    ec.clear();
    struct kevent changelist[2];
    EV_SET(&changelist[0], fd, EVFILT_READ, EV_ADD | EV_ENABLE | EV_CLEAR, 0, 0,
           as_kqueue_udata(data));
    EV_SET(&changelist[1], fd, EVFILT_WRITE, EV_ADD | EV_ENABLE | EV_CLEAR, 0,
           0, as_kqueue_udata(data));
    const int res = ::kevent(kq_fd_.get(), changelist, 2, nullptr, 0, nullptr);
    if (res == -1) {
        ec = os::make_system_error(errno);
    }
}

void kqueue_handle::trigger_event(uintptr_t ident, void* data,
                                  std::error_code& ec) noexcept {
    ec.clear();
    struct kevent changes[2];
    EV_SET(&changes[0], ident, EVFILT_USER, EV_ADD | EV_ENABLE, 0, 0,
           as_kqueue_udata(data));
    EV_SET(&changes[1], ident, EVFILT_USER, EV_ENABLE, NOTE_TRIGGER, 0,
           as_kqueue_udata(data));
    const int res = ::kevent(kq_fd_.get(), changes, 2, nullptr, 0, nullptr);
    if (res == -1) {
        ec = os::make_system_error(errno);
    }
}

void kqueue_handle::disable_event(uintptr_t ident,
                                  std::error_code& ec) noexcept {
    ec.clear();
    struct kevent changes[1];
    EV_SET(&changes[0], ident, EVFILT_USER, EV_DISABLE | EV_CLEAR, 0, 0, 0);
    const int res = ::kevent(kq_fd_.get(), changes, 1, nullptr, 0, nullptr);
    if (res == -1) {
        ec = os::make_system_error(errno);
    }
}

void kqueue_handle::remove(int fd, std::error_code& ec) noexcept {
    ec.clear();
    struct kevent changelist[2];
    EV_SET(&changelist[0], fd, EVFILT_READ, EV_DELETE, 0, 0, nullptr);
    EV_SET(&changelist[1], fd, EVFILT_WRITE, EV_DELETE, 0, 0, nullptr);
    const int res = ::kevent(kq_fd_.get(), changelist, 2, nullptr, 0, nullptr);
    if (res == -1) {
        ec = os::make_system_error(errno);
    }
}

std::span<kqueue_event> kqueue_handle::wait(std::span<kqueue_event> evs,
                                            std::chrono::nanoseconds timeout,
                                            std::error_code& ec) noexcept {
    using namespace std::chrono;
    ec.clear();
    if (evs.empty()) {
        return {};
    }
    struct timespec wait_time{};
    const struct timespec* wait_time_ptr = nullptr;
    if (timeout.count() >= 0) {
        seconds secs = duration_cast<seconds>(timeout);
        wait_time.tv_sec = static_cast<time_t>(secs.count());
        wait_time.tv_nsec = static_cast<int>((timeout - secs).count());
        wait_time_ptr = &wait_time;
    }
    const int res =
        exec_while_eintr(::kevent, kq_fd_.get(), nullptr, 0,
                         reinterpret_cast<struct kevent*>(evs.data()),
                         static_cast<int>(evs.size()), wait_time_ptr);
    if (res == -1) {
        ec = os::make_system_error(errno);
        return {};
    }
    const size_t ret_count = static_cast<size_t>(std::max(res, 0));
    return evs.subspan(0, ret_count);
}