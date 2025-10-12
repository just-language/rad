#include <fcntl.h>
#include <rad/os_types.h>
#include <rad/sysinfo.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <unistd.h>
#ifdef __linux__
#include <sys/sysinfo.h>
#else
#include <sys/sysctl.h>
#endif // __linux__
#include <memory>

using namespace RAD_LIB_NAMESPACE;
using namespace sysinfo;

#ifdef __linux__
uint32_t rad::sysinfo::cache_size() {
#ifdef _SC_LEVEL1_DCACHE_SIZE
    long result = std::max(::sysconf(_SC_LEVEL1_DCACHE_SIZE), long{0});
    result += std::max(::sysconf(_SC_LEVEL2_CACHE_SIZE), long{0});
    result += std::max(::sysconf(_SC_LEVEL3_CACHE_SIZE), long{0});
    return static_cast<uint32_t>(result);
#else
    std::string file_buffer;
    const char* cpuinfo_path = "/proc/cpuinfo";
    int fd = ::open(cpuinfo_path, O_RDONLY);
    if (fd == -1) {
        throw std::system_error(errno, std::system_category(), "open");
    }

    {
        os::handle cpu_file(fd);
        struct stat file_stat;
        if (::fstat(cpu_file.get(), &file_stat) == -1) {
            throw std::system_error(errno, std::system_category(), "fstat");
        }
        if (file_stat.st_size == 0) {
            return 0;
        }
        file_buffer.resize(static_cast<size_t>(file_stat.st_size));

        ssize_t was_read =
            ::read(cpu_file.get(), file_buffer.data(), file_buffer.size());
        if (was_read == -1) {
            throw std::system_error(errno, std::system_category(), "read");
        }
        file_buffer.resize(static_cast<size_t>(was_read));
    }

    constexpr std::string_view cache_size_line = "cache size";
    size_t pos = file_buffer.find(cache_size_line);
    if (pos == std::string::npos) {
        return 0;
    }
    auto line_view = subview(file_buffer, pos + cache_size_line.size());
    while (!line_view.empty() &&
           !(line_view.front() >= '0' && line_view.front() <= '9')) {
        line_view.remove_prefix(1);
    }
    while (!line_view.empty() &&
           !(line_view.back() >= '0' && line_view.back() <= '9')) {
        line_view.remove_suffix(1);
    }
    if (!line_view.empty()) {
        std::error_code ec;
        return to_uint32(line_view, 10, ec);
    }
    return 0;
#endif // _SC_LEVEL1_DCACHE_SIZE
}
#else
uint32_t rad::sysinfo::cache_size() {
    return 0;
}
#endif // __linux__

uint32_t rad::sysinfo::cores() {
    return static_cast<uint32_t>(::sysconf(_SC_NPROCESSORS_ONLN));
}

std::string rad::sysinfo::os_name() {
    struct utsname name;
    if (::uname(&name) == -1) {
        throw std::system_error(errno, std::system_category(), "uname");
    }

    std::string_view sysname(name.sysname), release(name.release),
        version(name.version);

    std::string os_name;
    os_name.reserve(sysname.size() + release.size() + version.size() + 2);
    os_name += sysname;
    os_name += ' ';
    os_name += release;
    os_name += ' ';
    os_name += version;

    return os_name;
}

#ifdef __linux__
system_memory_info_t rad::sysinfo::system_memory_info() {
    struct sysinfo info;
    if (::sysinfo(&info) == -1) {
        throw std::system_error(errno, std::system_category(), "sysinfo");
    }

    system_memory_info_t minfo;
    minfo.total_memory = info.totalram;
    minfo.total_memory *= info.mem_unit;
    minfo.free_memory = info.freeram;
    minfo.free_memory *= info.mem_unit;
    minfo.used_memory = minfo.total_memory - minfo.free_memory;
    minfo.memory_usage =
        static_cast<uint32_t>((static_cast<double>(minfo.used_memory) /
                               static_cast<double>(minfo.total_memory)) *
                              100);

    return minfo;
}

std::filesystem::path rad::sysinfo::executable_path() {
    std::array<char, PATH_MAX * 2> path_buff;
    ssize_t ret =
        ::readlink("/proc/self/exe", path_buff.data(), path_buff.size());
    if (ret == -1) {
        throw std::system_error{os::make_system_error(errno)};
    }
    if (ret > path_buff.size()) {
        ret = path_buff.size();
    }
    return std::filesystem::path{
        std::string_view{path_buff.data(), static_cast<size_t>(ret)}};
}
#else
system_memory_info_t rad::sysinfo::system_memory_info() {
    long pages = ::sysconf(_SC_PHYS_PAGES);
    if (pages < 0) {
        throw std::system_error(errno, std::system_category(), "sysconf");
    }
    // long available_pages = ::sysconf(_SC_AVPHYS_PAGES);
    unsigned long available_pages = 0;
    size_t param_len = sizeof(available_pages);
    sysctlbyname("vm.stats.vm.v_free_count", &available_pages, &param_len, NULL,
                 0);
    if (available_pages < 0) {
        throw std::system_error(errno, std::system_category(), "sysctlbyname");
    }
    long page_size = ::sysconf(_SC_PAGESIZE);
    if (page_size < 0) {
        throw std::system_error(errno, std::system_category(), "sysconf");
    }

    system_memory_info_t minfo;
    minfo.total_memory = pages;
    minfo.total_memory *= page_size;
    minfo.free_memory = available_pages;
    minfo.free_memory *= page_size;
    minfo.used_memory = minfo.total_memory - minfo.free_memory;
    minfo.memory_usage =
        static_cast<uint32_t>((static_cast<double>(minfo.used_memory) /
                               static_cast<double>(minfo.total_memory)) *
                              100);

    return minfo;
}
#endif // __linux__