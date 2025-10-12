#include <Windows.h>
#include <rad/async/work_guard.h>
#include <rad/io/detail/file_common.h>
#include <rad/ipc/async_pipe.h>

using namespace RAD_LIB_NAMESPACE;
using namespace pipe;

namespace {
    inline async_result make_pending(work_guard<any_executor>& w) noexcept {
        w.release();
        return async_result::pending();
    }

    os::file_handle open_named_pipe(const pipe::endpoint& epoint,
                                    std::error_code& ec) noexcept {
        ec.clear();
        os::file_handle new_handle{CreateNamedPipeW(
            epoint.name_with_prefix().c_str(),
            static_cast<DWORD>(epoint.flow_dir()),
            static_cast<DWORD>(epoint.transfer()), epoint.instances(),
            epoint.input_size(), epoint.output_size(), epoint.timeout(),
            reinterpret_cast<LPSECURITY_ATTRIBUTES>(
                epoint.security_attributes()))};
        if (!new_handle) {
            ec = os::make_system_error(GetLastError());
            return {};
        }
        io::iocp::io_port::skip_iocp_on_sucess(new_handle, ec);
        if (ec) {
            return {};
        }
        return {std::move(new_handle)};
    }

    os::file_handle connect_named_pipe(const pipe::endpoint& epoint,
                                       std::error_code& ec) noexcept {
        using namespace io::files;

        access rights;
        auto flow_dir = epoint.flow_dir();

        if (flow_dir & pipe::open_mode::duplex) {
            rights = access::read_write;
        }
        else if (flow_dir & pipe::open_mode::client_to_server) {
            rights = access::write;
        }
        else {
            rights = access::read;
        }

        constexpr io::files::open_mode open_flags =
            io::files::open_mode::existing;
        constexpr attributes attributes = attributes::overlapped;

        ec.clear();
        os::file_handle new_handle{CreateFileW(
            epoint.name_with_prefix().c_str(), static_cast<DWORD>(rights), 0,
            reinterpret_cast<LPSECURITY_ATTRIBUTES>(
                epoint.security_attributes()),
            static_cast<DWORD>(open_flags), static_cast<DWORD>(attributes),
            nullptr)};
        if (!new_handle) {
            ec = os::make_system_error(GetLastError());
            return {};
        }
        io::iocp::io_port::skip_iocp_on_sucess(new_handle, ec);
        if (ec) {
            return {};
        }
        return {std::move(new_handle)};
    }

} // namespace

void async_pipe::open(const endpoint& epoint, std::error_code& ec) noexcept {
    ec.clear();
    auto new_handle = open_named_pipe(epoint, ec);
    if (!ec) {
        std::wstring pipe_name = epoint.pipe_name();
        impl_.set_handle_path(new_handle, pipe_name, ec);
        if (!ec) {
            name_utf8_ = to_string(epoint.pipe_name());
        }
    }
}

void async_pipe::connect(const endpoint& epoint, std::error_code& ec) noexcept {
    auto new_handle = connect_named_pipe(epoint, ec);
    if (!ec) {
        name_utf8_ = to_string(epoint.pipe_name());
        std::wstring pipe_name = epoint.pipe_name();
        impl_.set_handle_path(new_handle, pipe_name, ec);
    }
}

void async_pipe::disconnect(std::error_code& ec) noexcept {
    if (!DisconnectNamedPipe(native_handle().get())) {
        ec = os::make_system_error(GetLastError());
    }
}

void async_pipe::accept(std::error_code& ec) noexcept {
    if (ConnectNamedPipe(native_handle().get(), nullptr)) {
        return;
    }
    ec = os::make_system_error(GetLastError());
}

void async_pipe::flush(std::error_code& ec) noexcept {
    if (!FlushFileBuffers(native_fd())) {
        ec = os::make_system_error(GetLastError());
    }
}

async_result async_pipe::do_async_accept(io::detail::io_op& op) noexcept {
    if (!is_open()) {
        return async_result::failed(
            std::make_error_code(std::errc::bad_file_descriptor));
    }
    work_guard wguard{get_any_exuector()};
    if (ConnectNamedPipe(native_fd(), op.get_ov_ptr())) {
        return async_result::success(0);
    }
    DWORD dw_ec = GetLastError();
    if (!dw_ec || dw_ec == ERROR_PIPE_CONNECTED) {
        return async_result::success(0);
    }
    if (dw_ec == ERROR_IO_PENDING) {
        return make_pending(wguard);
    }
    return async_result::failed(os::make_system_error(dw_ec));
}

std::error_code async_pipe::get_accept_result(io::detail::io_op& op) noexcept {
    DWORD transferred = 0;
    if (!::GetOverlappedResult(native_fd(), op.get_ov_ptr(), &transferred, 0)) {
        return os::make_system_error(GetLastError());
    }
    return std::error_code{};
}

bool async_pipe::accept_awaiter::await_suspend(std::coroutine_handle<> coro) {
    waiter = coro;
    auto result = pipe->do_async_accept(*this);
    if (result.is_pending()) {
        return true;
    }

    store(result.error());
    return false;
}

void async_pipe::accept_awaiter::await_resume() const {
    raise("async_accept");
}

void async_pipe::accept_awaiter::invoke_operation() {
    store(pipe->get_accept_result(*this));
    waiter.resume();
}

any_executor& async_pipe::accept_awaiter::associated_executor() const noexcept {
    return pipe->executor().as_any_executor();
}