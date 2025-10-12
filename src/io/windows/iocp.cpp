#include <Windows.h>
#include <rad/io/windows/iocp.h>
#include <rad/net/types.h>

#include <vector>

using namespace RAD_LIB_NAMESPACE;
using namespace io;
using namespace iocp;

static_assert(sizeof(completion_result) == sizeof(OVERLAPPED_ENTRY),
              "completion_result must have the same size as OVERLAPPED_ENTRY");
static_assert(sizeof(uint32_t) == sizeof(ULONG));
static_assert(sizeof(io::detail::overlapped_result) == sizeof(OVERLAPPED),
              "overlapped_result must have the same result as OVERLAPPED");

void io_port::create(uint32_t max_threads, std::error_code& ec) noexcept {
    os::handle new_port{::CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr,
                                                 0, max_threads)};
    ec.clear();
    if (new_port) {
        port_ = std::move(new_port);
        return;
    }
    ec = os::make_system_error(GetLastError());
}

void io_port::add_handle(void* handle, uintptr_t key,
                         std::error_code& ec) noexcept {
    ec.clear();
    if (::CreateIoCompletionPort(handle, port_.get(), key, 0) != nullptr) {
        return;
    }
    ec = os::make_system_error(GetLastError());
}

void io_port::get_result(completion_result& result, uint32_t wait_time_ms,
                         std::error_code& ec) noexcept {
    ec.clear();
    DWORD transferred = 0;
    ULONG_PTR key = 0;
    LPOVERLAPPED ov = nullptr;
    if (::GetQueuedCompletionStatus(port_.get(), &transferred, &key, &ov,
                                    wait_time_ms)) {
        return;
    }
    result.transferred = transferred;
    result.key = static_cast<uintptr_t>(key);
    result.ov = reinterpret_cast<detail::overlapped_result*>(ov);
    ec = os::make_system_error(GetLastError());
}

void io_port::get_results(completion_result* results, uint32_t results_count,
                          uint32_t& results_removed, uint32_t wait_time_ms,
                          bool alterable, std::error_code& ec) noexcept {
    ec.clear();
    if (::GetQueuedCompletionStatusEx(
            port_.get(), reinterpret_cast<LPOVERLAPPED_ENTRY>(results),
            results_count, reinterpret_cast<PULONG>(&results_removed),
            wait_time_ms, static_cast<BOOL>(alterable))) {
        return;
    }
    ec = os::make_system_error(GetLastError());
}

void io_port::post(const completion_result& result,
                   std::error_code& ec) noexcept {
    ec.clear();
    if (::PostQueuedCompletionStatus(port_.get(), result.transferred,
                                     result.key, (LPOVERLAPPED)result.ov)) {
        return;
    }
    ec = os::make_system_error(GetLastError());
}

void io_port::skip_iocp_on_sucess(void* handle, std::error_code& ec) noexcept {
    ec.clear();
    if (::SetFileCompletionNotificationModes(
            handle, FILE_SKIP_COMPLETION_PORT_ON_SUCCESS)) {
        return;
    }
    ec = os::make_system_error(GetLastError());
}
