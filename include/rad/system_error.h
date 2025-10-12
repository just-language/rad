#pragma once
#include <rad/libbase.h>

#include <system_error>

namespace rad {
#ifdef _WIN32
    // implement a system_category for windows that supports utf-8
    // see: https://github.com/microsoft/STL/issues/3254
    RAD_EXPORT_DECL const std::error_category& system_category() noexcept;
#else
    inline const std::error_category& system_category() noexcept {
        return std::system_category();
    }
#endif //_WIN32
} // namespace rad