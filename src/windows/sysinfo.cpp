#include <Windows.h>
#include <rad/stack_allocator.h>
#include <rad/sysinfo.h>
#include <rad/windows/winreg.h>

#include <array>
#include <cassert>
#include <ctime>
#include <vector>

using namespace RAD_LIB_NAMESPACE;

uint32_t sysinfo::cache_size() {
    DWORD size = 0;

    (void)GetLogicalProcessorInformation(nullptr, &size);
    DWORD last_error = GetLastError();
    if (last_error != ERROR_INSUFFICIENT_BUFFER) {
        throw std::system_error(last_error, system_category());
    }
    assert(size && size > sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION) &&
           size % sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION) == 0);

    std::vector<SYSTEM_LOGICAL_PROCESSOR_INFORMATION> cpu_info(
        size / sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION));

    if (!GetLogicalProcessorInformation(cpu_info.data(), &size)) {
        throw std::system_error(GetLastError(), system_category());
    }

    for (const auto& info : cpu_info) {
        if (info.Relationship == RelationCache) {
            size += info.Cache.Size;
        }
    }

    return size;
}

uint32_t sysinfo::cores() {
    SYSTEM_INFO sinfo;
    GetNativeSystemInfo(&sinfo);
    return sinfo.dwNumberOfProcessors;
}

auto sysinfo::system_memory_info() -> system_memory_info_t {
    MEMORYSTATUSEX mem_status{};
    mem_status.dwLength = sizeof(mem_status);
    if (!GlobalMemoryStatusEx(&mem_status)) {
        throw std::system_error(GetLastError(), system_category());
    }

    system_memory_info_t info;
    info.total_memory = mem_status.ullTotalPhys;
    info.memory_usage = mem_status.dwMemoryLoad;
    info.free_memory = mem_status.ullAvailPhys;
    info.used_memory = info.total_memory - info.free_memory;

    return info;
}

std::filesystem::path sysinfo::executable_path() {
    std::filesystem::path path;
    std::wstring wbuff;
    wbuff.resize(MAX_PATH + 1);

    while (1) {
        DWORD size = GetModuleFileNameW(nullptr, wbuff.data(),
                                        static_cast<DWORD>(wbuff.size()));

        if (!size) {
            throw std::system_error(os::make_system_error(GetLastError()));
        }

        if (size < wbuff.size()) {
            wbuff.resize(size);
            break;
        }

        if (size == wbuff.size()) {
            wbuff.resize(size * 2);
        }
    }

    path = std::wstring_view{wbuff.data(), wbuff.size()};
    return path;
}
