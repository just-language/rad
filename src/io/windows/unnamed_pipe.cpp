#include <Windows.h>
#include <rad/io/windows/unnamed_pipe_impl.h>
#include <stdio.h>

#include <atomic>

using namespace RAD_LIB_NAMESPACE;
using namespace io;
using namespace io::detail;

namespace {
    std::atomic<int> pipe_serial = 0;
}

std::array<unnamed_pipe_impl, 2>
unnamed_pipe_impl::create_pair(io_executor& ex, std::error_code& ec) noexcept {
    ec.clear();
    std::array<unnamed_pipe_impl, 2> pipes{unnamed_pipe_impl{ex},
                                           unnamed_pipe_impl{ex}};
    os::file_handle read_pipe, write_pipe;

    constexpr std::size_t max_size_t_digits =
        std::numeric_limits<std::size_t>::digits10;
    std::array<wchar_t, 30 + max_size_t_digits * 2 + 1> name_buff;
    int serial_number = pipe_serial.fetch_add(1);

    int len =
        _snwprintf_s(name_buff.data(), name_buff.size(), name_buff.size() - 1,
                     L"\\\\.\\Pipe\\Win32Pipes.%08x.%08x",
                     GetCurrentProcessId(), serial_number);
    if (len <= 0) {
        ec = std::make_error_code(std::errc::no_buffer_space);
        return pipes;
    }

    read_pipe.reset(::CreateNamedPipeW(
        name_buff.data(), PIPE_ACCESS_INBOUND | FILE_FLAG_OVERLAPPED,
        PIPE_TYPE_BYTE | PIPE_WAIT, 1, 1024, 1024, 0, nullptr));
    if (!read_pipe) {
        ec = os::make_system_error(::GetLastError());
        return pipes;
    }
    iocp::io_port::skip_iocp_on_sucess(read_pipe, ec);
    if (ec) {
        return pipes;
    }

    write_pipe.reset(::CreateFileW(name_buff.data(), GENERIC_WRITE, 0, nullptr,
                                   OPEN_EXISTING, FILE_FLAG_OVERLAPPED,
                                   nullptr));
    if (!write_pipe) {
        ec = os::make_system_error(::GetLastError());
        return pipes;
    }
    iocp::io_port::skip_iocp_on_sucess(write_pipe, ec);
    if (ec) {
        return pipes;
    }

    std::wstring empty_path;
    pipes[0].set_handle_path(read_pipe, empty_path, ec);
    if (ec) {
        return {unnamed_pipe_impl{ex}, unnamed_pipe_impl{ex}};
    }
    pipes[1].set_handle_path(write_pipe, empty_path, ec);
    if (ec) {
        return {unnamed_pipe_impl{ex}, unnamed_pipe_impl{ex}};
    }

    return pipes;
}