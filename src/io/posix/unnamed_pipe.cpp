#include <rad/io/unnamed_pipe.h>
#include <fcntl.h>
#include <unistd.h>

using namespace RAD_LIB_NAMESPACE;
using namespace io;
using namespace io::detail;

std::array<unnamed_pipe_impl, 2>
unnamed_pipe_impl::create_pair(io_executor& ex, std::error_code& ec) noexcept {
    ec.clear();
    std::array<unnamed_pipe_impl, 2> pipes{unnamed_pipe_impl{ex},
                                           unnamed_pipe_impl{ex}};
    int handles[2];
    int flags = O_CLOEXEC;
    if (!ex.is_async()) {
        flags |= O_NONBLOCK;
    }
    int ret = ::pipe2(handles, flags);
    if (ret == -1) {
        ec = os::make_system_error(errno);
        return pipes;
    }

    os::file_handle pipe1{handles[0]};
    os::file_handle pipe2{handles[1]};
    std::string empty_path;
    pipes[0].set_handle_path(pipe1, empty_path, ec);
    if (ec) {
        return {unnamed_pipe_impl{ex}, unnamed_pipe_impl{ex}};
    }
    pipes[1].set_handle_path(pipe2, empty_path, ec);
    if (ec) {
        return {unnamed_pipe_impl{ex}, unnamed_pipe_impl{ex}};
    }

    return pipes;
}