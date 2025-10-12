#include <Windows.h>
#include <rad/os_types.h>

using namespace RAD_LIB_NAMESPACE;

void os::detail::close_handle(HANDLE h) noexcept {
    BOOL result = ::CloseHandle(h);
    assert(result == TRUE);
    ((void)result);
}