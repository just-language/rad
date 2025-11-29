#include <rad/async/io_executor.h>
#include <rad/io/linux/epoll.h>
#include <sys/epoll.h>

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
} // namespace

static_assert(sizeof(epoll_event_t) == sizeof(epoll_event));
static_assert(offsetof(epoll_event_t, events) == offsetof(epoll_event, events));
static_assert(offsetof(epoll_event_t, data) == offsetof(epoll_event, data));

void epoll::create(std::error_code& ec) noexcept {
    ec.clear();
    int epfd = epoll_create1(EPOLL_CLOEXEC);
    if (epfd == -1) {
        ec.assign(errno, std::system_category());
        return;
    }
    epoll_fd.reset(epfd);
}

void epoll::ctl(epoll_op op, int fd, const epoll_event_t& ev,
                std::error_code& ec) noexcept {
    ec.clear();
    struct epoll_event epev;
    epev.events = static_cast<uint32_t>(ev.events);
    epev.data.ptr = ev.data.ptr;
    int result = ::epoll_ctl(epoll_fd.get(), static_cast<int>(op), fd, &epev);
    if (result == -1) {
        ec.assign(errno, std::system_category());
    }
}

std::span<epoll_event_t> epoll::wait(std::span<epoll_event_t> evs,
                                     duration timeout,
                                     std::error_code& ec) noexcept {
    ec.clear();
    if (evs.empty()) {
        return {};
    }
    if (timeout.count() < 0) {
        timeout = duration{-1};
    }
    int evs_count = as<int>(evs.size());
    auto events = reinterpret_cast<epoll_event*>(evs.data());
    int count = exec_while_eintr(::epoll_wait, epoll_fd.get(), events,
                                 evs_count, timeout.count());

    if (count == -1) {
        ec.assign(errno, std::system_category());
        return {};
    }

    return evs.subspan(0, as<size_t>(count));
}
