#include <Windows.h>
#include <rad/io/console.h>

using namespace RAD_LIB_NAMESPACE;
using namespace io;
using namespace io::detail;

void console_impl::open() {
    errcon = INVALID_HANDLE_VALUE;

    incon = GetStdHandle(STD_INPUT_HANDLE);
    if (incon != INVALID_HANDLE_VALUE) {
        outcon = GetStdHandle(STD_OUTPUT_HANDLE);
        if (outcon) {
            errcon = GetStdHandle(STD_ERROR_HANDLE);
        }
    }

    if (errcon == INVALID_HANDLE_VALUE) {
        throw std::system_error(os::make_system_error(GetLastError()),
                                "GetStdHandle");
    }
}

std::size_t console_impl::do_write(const_buffer buff,
                                   std::error_code& ec) noexcept {
    DWORD written = 0;
    BOOL result = WriteFile(outcon, buff.data(),
                            static_cast<DWORD>(buff.size()), &written, nullptr);
    if (!result) {
        ec = os::make_system_error(GetLastError());
        return 0;
    }
    return written;
}

std::size_t console_impl::do_read(mutable_buffer buff,
                                  std::error_code& ec) noexcept {
    DWORD was_read = 0;
    BOOL result = ReadFile(incon, buff.data(), static_cast<DWORD>(buff.size()),
                           &was_read, nullptr);
    if (!result) {
        ec = os::make_system_error(GetLastError());
        return 0;
    }
    if (was_read >= 2) {
        const char* last_two = buff.data_as<const char>() + buff.size() - 3;
        if (last_two[0] == '\r' && last_two[1] == '\n') {
            was_read -= 2;
        }
    }
    return was_read;
}

void console_impl::do_async_read(mutable_buffer buff,
                                 console_read_handler_base* op) {
    if (!reader_thd_.running()) {
        reader_thd_.start(1);
    }

    any_ex().add_work();

    auto job = [this, buff, op]() {
        std::error_code ec;
        auto n = do_read(buff, ec);
        op->store_results(ec, n);
        any_ex().post_finished(*op);
    };

    post(reader_thd_, job);
}