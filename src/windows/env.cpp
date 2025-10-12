#include <Windows.h>
#include <rad/cli.h>
#include <rad/env.h>
#include <rad/os_types.h>

using namespace RAD_LIB_NAMESPACE;
using namespace os;

std::vector<os_string> env::args_os() {
    wchar_t* cmdline = ::GetCommandLineW();
    // check if cmdline is null
    return cli::split_winmain(cmdline);
}

std::filesystem::path env::executable_path() {
    std::filesystem::path path;
    std::wstring wbuff;
    wbuff.resize(MAX_PATH + 1);

    while (1) {
        DWORD size = GetModuleFileNameW(nullptr, wbuff.data(),
                                        static_cast<DWORD>(wbuff.size()));

        if (!size) {
            throw std::system_error(make_system_error(GetLastError()));
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

void env::set_var(os_zstring_view key, os_zstring_view value) {
    BOOL result = ::SetEnvironmentVariableW(key.data(), value.data());
    if (!result) {
        throw std::system_error(make_system_error(GetLastError()));
    }
}

void env::remove_var(os_zstring_view key) {
    ::SetEnvironmentVariableW(key.data(), nullptr);
}

os_string env::var_os(os_zstring_view key) {
    //::GetEnvironmentVariableW(key.data(), , );
    return {};
}