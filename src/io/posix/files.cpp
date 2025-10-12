#include <rad/io/files.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

using namespace RAD_LIB_NAMESPACE;
using namespace io;
using namespace files;
using io::files::detail::file_impl;

#ifndef __linux__
#define stat64 stat
#define fstat64 fstat
#define lseek64 lseek
#define ftruncate64 ftruncate
#endif // ! __linux__

namespace {
    std::error_code last_error() noexcept {
        return std::error_code{errno, std::system_category()};
    }

    int access_to_flags(files::access access_rights) {
        int flags = O_CLOEXEC;

        if (access_rights == access::read) {
            flags |= O_RDONLY;
        }
        else if (access_rights == access::write) {
            flags |= O_WRONLY;
        }
        else if (access_rights == access::read_write) {
            flags |= O_RDWR;
        }

        return flags;
    }

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

void file_impl::create(native_string_type path, create_mode mode,
                       access access_rights, share_mode share, attributes attr,
                       std::error_code& ec) noexcept {
    int flags = access_to_flags(access_rights);

    if (mode == create_mode::overwrite) {
        flags |= O_CREAT | O_TRUNC;
    }
    else if (mode == create_mode::if_new_file_or_fail) {
        flags |= O_CREAT | O_EXCL;
    }
    else if (mode == create_mode::if_new_file_or_open) {
        flags |= O_CREAT;
    }

    mode_t user_mode = S_IRWXU;

    int fd = ::open(path.data(), flags, user_mode);
    if (fd == -1) {
        ec = os::make_system_error(errno);
        return;
    }

    handle_.reset(fd);
    path_ = path;
}

void file_impl::open(native_string_type path, open_mode mode,
                     access access_rights, share_mode share, attributes attr,
                     std::error_code& ec) noexcept {
    int flags = access_to_flags(access_rights);

    if (mode == open_mode::create_if_not_exist) {
        flags |= O_CREAT;
    }

    mode_t user_mode = S_IRWXU;

    int fd = ::open(path.data(), flags, user_mode);
    if (fd == -1) {
        ec = os::make_system_error(errno);
        return;
    }

    handle_.reset(fd);
    path_ = path;
}

uint64_t file_impl::size(std::error_code& ec) const noexcept {
    if (!handle_) {
        ec = std::make_error_code(std::errc::bad_file_descriptor);
        return 0;
    }

    struct stat64 st;
    if (fstat64(handle_.get(), &st) == -1) {
        ec = os::make_system_error(errno);
        return 0;
    }

    return st.st_size;
}

times_t file_impl::times(std::error_code& ec) const noexcept {
    if (!handle_) {
        ec = std::make_error_code(std::errc::bad_file_descriptor);
        return {};
    }

    struct stat64 st;
    if (::fstat64(handle_.get(), &st) == -1) {
        ec = os::make_system_error(errno);
        return {};
    }

    times_t times;

    using namespace std::chrono;

    times.creation = file_clock::time_point{nanoseconds(st.st_ctim.tv_nsec) +
                                            seconds(st.st_ctim.tv_sec)};
    times.last_access = file_clock::time_point{nanoseconds(st.st_atim.tv_nsec) +
                                               seconds(st.st_atim.tv_sec)};
    times.last_write = file_clock::time_point{nanoseconds(st.st_mtim.tv_nsec) +
                                              seconds(st.st_mtim.tv_sec)};

    return times;
}

int64_t file_impl::position(seek_mode mode,
                            std::error_code& ec) const noexcept {
    if (!handle_) {
        ec = std::make_error_code(std::errc::bad_file_descriptor);
        return 0;
    }

    if (mode == seek_mode::current) {
        return 0;
    }

    int whence = SEEK_CUR;
    int offset = lseek64(handle_.get(), 0, whence);
    if (offset == -1) {
        ec = os::make_system_error(errno);
        return {};
    }

    int64_t file_size = 0;
    if (mode == seek_mode::end) {
        file_size = static_cast<int64_t>(size(ec));
        if (ec) {
            return 0;
        }
    }

    switch (mode) {
    case seek_mode::current:
        return 0;

    case seek_mode::begin:
        return offset;

    case seek_mode::end:
        return file_size - offset;

    default:
        ec = std::make_error_code(std::errc::invalid_argument);
        return 0;
    }
}

void file_impl::seek(int64_t distance, seek_mode mode,
                     std::error_code& ec) noexcept {
    if (!handle_) {
        ec = std::make_error_code(std::errc::bad_file_descriptor);
        return;
    }

    int whence;

    switch (mode) {
    case seek_mode::begin:
        whence = SEEK_SET;
        break;
    case seek_mode::current:
        whence = SEEK_CUR;
        break;
    case seek_mode::end:
        whence = SEEK_END;
        break;
    default:
        ec = std::make_error_code(std::errc::invalid_argument);
        return;
    }

    if (lseek64(handle_.get(), 0, whence) == -1) {
        ec = os::make_system_error(errno);
    }
}

std::size_t file_impl::write(const_buffer buff, std::error_code& ec) noexcept {
    if (!handle_) {
        ec = std::make_error_code(std::errc::bad_file_descriptor);
        return 0;
    }

    std::size_t total_written = 0;

    do {
        auto written =
            exec_while_eintr(::write, handle_.get(), buff.data(), buff.size());
        if (written == -1) {
            ec = os::make_system_error(errno);
            return total_written;
        }
        total_written += written;
        buff += written;
    } while (!buff.empty());

    return total_written;
}

std::size_t file_impl::read(mutable_buffer buff, std::error_code& ec) noexcept {
    if (!handle_) {
        ec = std::make_error_code(std::errc::bad_file_descriptor);
        return 0;
    }
    auto was_read =
        exec_while_eintr(::read, handle_.get(), buff.data(), buff.size());
    if (was_read == -1) {
        ec = os::make_system_error(errno);
        return 0;
    }
    return static_cast<std::size_t>(was_read);
}

std::size_t file_impl::write_at(const_buffer buff, uint64_t pos,
                                std::error_code& ec) noexcept {
    if (!handle_) {
        ec = std::make_error_code(std::errc::bad_file_descriptor);
        return 0;
    }

    std::size_t total_written = 0;

    do {
        auto written = exec_while_eintr(::pwrite, handle_.get(), buff.data(),
                                        buff.size(), pos);
        if (written == -1) {
            ec = os::make_system_error(errno);
            return total_written;
        }
        total_written += written;
        buff += written;
        pos += written;
    } while (!buff.empty());

    return total_written;
}

std::size_t file_impl::read_at(mutable_buffer buff, uint64_t pos,
                               std::error_code& ec) noexcept {
    if (!handle_) {
        ec = std::make_error_code(std::errc::bad_file_descriptor);
        return 0;
    }
    auto was_read =
        exec_while_eintr(::pread, handle_.get(), buff.data(), buff.size(), pos);
    if (was_read == -1) {
        ec = os::make_system_error(errno);
        return 0;
    }
    return static_cast<std::size_t>(was_read);
}

void file_impl::truncate(int64_t pos, seek_mode mode,
                         std::error_code& ec) noexcept {
    if (mode == seek_mode::current) {
        pos += position(seek_mode::begin, ec);
        if (ec) {
            return;
        }
    }
    else if (mode == seek_mode::end) {
        pos -= size(ec);
        if (ec) {
            return;
        }
    }
    else if (mode != seek_mode::begin) {
        ec = std::make_error_code(std::errc::invalid_argument);
        return;
    }

    if (ftruncate64(handle_.get(), pos) == -1) {
        ec = os::make_system_error(errno);
    }
}

void file_impl::truncate_here(std::error_code& ec) noexcept {
    truncate(0, seek_mode::current, ec);
}
