#include <Windows.h>
#include <rad/io/files.h>

using namespace RAD_LIB_NAMESPACE;
using namespace io;
using namespace files;
using io::files::detail::file_impl;

namespace {
    inline std::error_code last_error() noexcept {
        return std::error_code{static_cast<int>(GetLastError()),
                               system_category()};
    }
} // namespace

void file_impl::create(wzstring_view path, create_mode mode,
                       access access_rights, share_mode share, attributes attr,
                       std::error_code& ec) noexcept {
    os::file_handle new_handle{
        ::CreateFileW(path.data(), (uint32_t)access_rights, (uint32_t)share,
                      nullptr, (uint32_t)mode, (uint32_t)attr, nullptr)};
    if (new_handle) {
        handle_ = std::move(new_handle);
        path_ = path;
        return;
    }
    ec = last_error();
}

void file_impl::open(wzstring_view path, open_mode mode, access access_rights,
                     share_mode share, attributes attr,
                     std::error_code& ec) noexcept {
    os::file_handle new_handle{
        ::CreateFileW(path.data(), (uint32_t)access_rights, (uint32_t)share,
                      nullptr, (uint32_t)mode, (uint32_t)attr, nullptr)};
    if (new_handle) {
        handle_ = std::move(new_handle);
        path_ = path;
        return;
    }
    ec = last_error();
}

uint64_t file_impl::size(std::error_code& ec) const noexcept {
    LARGE_INTEGER file_size{};
    if (::GetFileSizeEx(handle_.get(), &file_size)) {
        return file_size.QuadPart;
    }
    ec = last_error();
    return 0;
}

times_t file_impl::times(std::error_code& ec) const noexcept {
    FILETIME create_time, access_time, write_time;
    if (!::GetFileTime(handle_.get(), &create_time, &access_time,
                       &write_time)) {
        ec = last_error();
        return {};
    }

    times_t times;

    auto filetime_to_ns = [](FILETIME ft) {
        using namespace std::chrono;
        using std::chrono::file_clock;
        static_assert(file_clock::period::den == 10'000'000);

        LARGE_INTEGER i;
        i.LowPart = ft.dwLowDateTime;
        i.HighPart = ft.dwHighDateTime;
        return file_clock::time_point{file_clock::duration{i.QuadPart}};
    };

    times.creation = filetime_to_ns(create_time);
    times.last_access = filetime_to_ns(access_time);
    times.last_write = filetime_to_ns(write_time);

    return times;
}

void file_impl::set_times(const times_t& times, std::error_code& ec) noexcept {
    ec.clear();
    auto ns_to_ftime = [](std::chrono::file_clock::time_point t) {
        using namespace std::chrono;
        file_clock::duration fdur = t.time_since_epoch();
        LARGE_INTEGER i;
        i.QuadPart = fdur.count();
        FILETIME ft;
        ft.dwLowDateTime = i.LowPart;
        ft.dwHighDateTime = i.HighPart;
        return ft;
    };
    FILETIME create_time = ns_to_ftime(times.creation);
    FILETIME access_time = ns_to_ftime(times.last_access);
    FILETIME write_time = ns_to_ftime(times.last_write);
    BOOL result =
        ::SetFileTime(handle_.get(), &create_time, &access_time, &write_time);
    if (!result) {
        ec = last_error();
    }
}

int64_t file_impl::position(seek_mode mode,
                            std::error_code& ec) const noexcept {
    LARGE_INTEGER to_move{}, current_pos{};
    if (!::SetFilePointerEx(handle_.get(), to_move, &current_pos,
                            static_cast<DWORD>(seek_mode::current))) {
        ec = last_error();
        return 0;
    }
    return current_pos.QuadPart;
}

void file_impl::set_attributes(attributes attributes,
                               std::error_code& ec) noexcept {
    if (::SetFileAttributesW(path_.c_str(), static_cast<DWORD>(attributes))) {
        return;
    }
    ec = last_error();
}

attributes file_impl::get_attributes() const noexcept {
    return static_cast<attributes>(::GetFileAttributesW(path_.c_str()));
}

void file_impl::seek(int64_t distance, seek_mode mode,
                     std::error_code& ec) noexcept {
    LARGE_INTEGER lg{};
    lg.QuadPart = distance;
    if (!::SetFilePointerEx(native_handle().get(), lg, &lg,
                            static_cast<DWORD>(mode))) {
        ec = last_error();
    }
}

std::size_t file_impl::write(const_buffer buff, std::error_code& ec) noexcept {
    std::size_t total_written = 0;
    do {
        DWORD written = 0;
        if (!::WriteFile(handle_.get(), buff.data(),
                         static_cast<DWORD>(buff.size()), &written, nullptr)) {
            ec = last_error();
            return total_written;
        }
        total_written += written;
        buff += written;
    } while (!buff.empty());

    return total_written;
}

std::size_t file_impl::read(mutable_buffer buff, std::error_code& ec) noexcept {
    DWORD read_num = 0;
    if (::ReadFile(handle_.get(), buff.data(), static_cast<DWORD>(buff.size()),
                   &read_num, nullptr)) {
        return read_num;
    }
    ec = last_error();
    return 0;
}

std::size_t file_impl::write_at(const_buffer buff, uint64_t pos,
                                std::error_code& ec) noexcept {
    std::size_t total_written = 0;
    do {
        OVERLAPPED ov = {};
        ov.Pointer = reinterpret_cast<void*>(pos);
        DWORD written = 0;
        if (!::WriteFile(handle_.get(), buff.data(),
                         static_cast<DWORD>(buff.size()), &written, &ov)) {
            ec = last_error();
            return total_written;
        }
        total_written += written;
        pos += written;
        buff += written;
    } while (!buff.empty());
    return total_written;
}

std::size_t file_impl::read_at(mutable_buffer buff, uint64_t pos,
                               std::error_code& ec) noexcept {
    DWORD read_num = 0;
    OVERLAPPED ov = {};
    ov.Pointer = reinterpret_cast<void*>(pos);
    if (::ReadFile(handle_.get(), buff.data(), static_cast<DWORD>(buff.size()),
                   &read_num, &ov)) {
        return read_num;
    }
    ec = last_error();
    return 0;
}

void file_impl::truncate_here(std::error_code& ec) noexcept {
    if (::SetEndOfFile(handle_.get())) {
        return;
    }
    ec = last_error();
}

void file_impl::truncate(int64_t pos, seek_mode mode,
                         std::error_code& ec) noexcept {
    auto oldpos = position(mode, ec);
    if (!ec) {
        auto on_exit = scope_exit([&] { seek(oldpos, mode, ec); });

        seek(pos, mode, ec);
        if (!ec) {
            truncate_here(ec);
            if (!ec) {
                return;
            }
        }
    }
    ec = last_error();
}