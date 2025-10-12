#pragma once
#if !defined(_WIN32)
#error "this file is to be compiled only on windows os"
#endif

#include <cstdint>

extern "C" {
using DWORD = unsigned long;
// using LPDWORD = DWORD*;
using BOOL = int;
// using UCHAR = unsigned char;
using HANDLE = void*;
#ifdef _WIN64
using ULONG_PTR = uintptr_t;
#else
using ULONG_PTR = unsigned long;
#endif // _WIN64
using socket_fd_t = ULONG_PTR;
using socklen_t = int;

struct _OVERLAPPED;
using LPOVERLAPPED = _OVERLAPPED*;
using LPWSAOVERLAPPED = LPOVERLAPPED;
}

namespace windows_detail {
    struct winapi_consts {
        // static constexpr auto error_io_pending = 997L;
        // static constexpr auto error_pipe_connected = 535L;
        // static constexpr auto apc_interrupted = 0x000000C0L;
        // static constexpr auto max_reg_key_name = 255UL;
    };
} // namespace windows_detail