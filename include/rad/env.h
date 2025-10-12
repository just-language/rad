#pragma once
#include <rad/libbase.h>
#include <rad/os_types.h>
#include <rad/string.h>

#include <filesystem>
#include <string>
#include <vector>

namespace RAD_LIB_NAMESPACE::env {
    RAD_EXPORT_DECL std::vector<os::os_string> args_os();

    RAD_EXPORT_DECL std::filesystem::path executable_path();

    inline std::filesystem::path::string_type native_executable_name() {
        return executable_path().filename().native();
    }

    inline std::string executable_name() {
        return executable_path().filename().string();
    }

    RAD_EXPORT_DECL void set_var(os::os_zstring_view key,
                                 os::os_zstring_view value);

    RAD_EXPORT_DECL void remove_var(os::os_zstring_view key);

    RAD_EXPORT_DECL os::os_string var_os(os::os_zstring_view key);

    RAD_EXPORT_DECL std::vector<os::os_string> vars_os();

#ifdef _WIN32
    inline std::vector<std::string> args() {
        return os::string_to_string(args_os());
    }

    inline std::string var(std::string_view key) {
        return to_string(var_os(to_wstring(key)));
    }

    inline std::vector<std::string> vars() {
        return os::string_to_string(vars_os());
    }
#else
    inline std::vector<std::string> args() {
        return args_os();
    }

    inline std::string var(std::string_view key) {
        return var_os(key);
    }

    inline std::vector<std::string> vars() {
        return vars_os();
    }
#endif // _WIN32

} // namespace RAD_LIB_NAMESPACE::env