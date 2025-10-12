#include <Windows.h>
#include <rad/async/work_guard.h>
#include <rad/io/windows/async_file_impl.h>

using namespace RAD_LIB_NAMESPACE;
using namespace io;
using namespace io::detail;

namespace {
    inline async_result make_pending(work_guard<any_executor>& w) noexcept {
        w.release();
        return async_result::pending();
    }
} // namespace

void async_file_impl::set_handle_path(native_handle_type& handle,
                                      native_path_type& path,
                                      std::error_code& ec) noexcept {
    ec.clear();
    descriptor_data unused;
    ex_->attach_handle(handle, unused, ec);
    if (ec) {
        return;
    }
    handle_ = std::move(handle);
    path_ = std::move(path);
}

std::size_t async_file_impl::write(const_buffer buff,
                                   std::error_code& ec) noexcept {
    ec.clear();
    auto total_size = buff.size();
    do {
        DWORD written = 0;
        BOOL result =
            WriteFile(handle_.get(), buff.data(),
                      static_cast<DWORD>(buff.size()), &written, nullptr);
        if (!result) {
            ec = os::make_system_error(GetLastError());
            return 0;
        }
        buff += written;
    } while (!buff.empty());
    return total_size;
}

std::size_t async_file_impl::read(mutable_buffer buff,
                                  std::error_code& ec) noexcept {
    ec.clear();
    DWORD read_num = 0;
    if (ReadFile(handle_.get(), buff.data(), static_cast<DWORD>(buff.size()),
                 &read_num, nullptr)) {
        return read_num;
    }
    ec = os::make_system_error(GetLastError());
    return 0;
}

void async_file_impl::cancel() noexcept {
    if (!is_open()) {
        return;
    }
    (void)CancelIoEx(handle_.get(), nullptr);
}

async_result async_file_impl::do_async_write(const const_buffer& buff,
                                             io::detail::io_op& ctx) noexcept {
    if (!is_open()) {
        return async_result::failed(
            std::make_error_code(std::errc::bad_file_descriptor));
    }
    work_guard wguard{get_any_exuector()};
    DWORD written = 0;
    BOOL result =
        WriteFile(handle_.get(), buff.data(), static_cast<DWORD>(buff.size()),
                  &written, ctx.get_ov_ptr());
    if (!result) {
        DWORD dw_ec = GetLastError();
        if (dw_ec == ERROR_IO_PENDING) {
            return make_pending(wguard);
        }
        return async_result::failed(os::make_system_error(dw_ec));
    }
    return async_result::success(written);
}

async_result async_file_impl::do_async_read(const mutable_buffer& buff,
                                            io::detail::io_op& ctx) noexcept {
    if (!is_open()) {
        return async_result::failed(
            std::make_error_code(std::errc::bad_file_descriptor));
    }
    work_guard wguard{get_any_exuector()};
    DWORD was_read = 0;
    BOOL result =
        ReadFile(handle_.get(), buff.data(), static_cast<DWORD>(buff.size()),
                 &was_read, ctx.get_ov_ptr());
    if (!result) {
        DWORD dw_ec = GetLastError();
        if (dw_ec == ERROR_IO_PENDING) {
            return make_pending(wguard);
        }
        return async_result::failed(os::make_system_error(dw_ec));
    }
    return async_result::success(was_read);
}

std::size_t async_file_impl::get_write_result(io::detail::io_op& op,
                                              std::error_code& ec) noexcept {
    ec.clear();
    DWORD transferred = 0;
    if (!::GetOverlappedResult(handle_.get(), op.get_ov_ptr(), &transferred,
                               0)) {
        ec = os::make_system_error(GetLastError());
    }
    return transferred;
}

std::size_t async_file_impl::get_read_result(io::detail::io_op& op,
                                             std::error_code& ec) noexcept {
    ec.clear();
    DWORD transferred = 0;
    if (!::GetOverlappedResult(handle_.get(), op.get_ov_ptr(), &transferred,
                               0)) {
        ec = os::make_system_error(GetLastError());
    }
    return transferred;
}

bool async_file_impl::write_awaiter::await_suspend(
    std::coroutine_handle<> coro) {
    waiter = coro;
    auto result = impl->do_async_write(buff, *this);
    if (result.is_pending()) {
        return true;
    }
    transferred = result.transferred();
    store(result.error());
    return false;
}

std::size_t async_file_impl::write_awaiter::await_resume() const {
    raise("async_write");
    return transferred;
}

void async_file_impl::write_awaiter::invoke_operation() {
    transferred = impl->get_write_result(*this, error());
    waiter.resume();
}

any_executor&
async_file_impl::write_awaiter::associated_executor() const noexcept {
    return impl->get_any_exuector();
}

bool async_file_impl::read_awaiter::await_suspend(
    std::coroutine_handle<> coro) {
    waiter = coro;
    do {
        auto result = impl->do_async_read(buff, *this);
        if (result.is_pending()) {
            return true;
        }
        buff += result.transferred();
        store(result.error());
    } while (want_more_read());
    return false;
}

std::size_t async_file_impl::read_awaiter::await_resume() const {
    raise("async_read");
    return total_size - buff.size();
}

void async_file_impl::read_awaiter::invoke_operation() {
    buff += impl->get_read_result(*this, error());
    while (want_more_read()) {
        auto result = impl->do_async_read(buff, *this);
        if (result.is_pending()) {
            return;
        }
        buff += result.transferred();
        store(result.error());
    }
    waiter.resume();
}

any_executor&
async_file_impl::read_awaiter::associated_executor() const noexcept {
    return impl->get_any_exuector();
}