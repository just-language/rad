#pragma once
#ifdef _WIN32
#include <rad/detail/windows_details.h>
#else
#include <rad/detail/posix_details.h>
#endif // _WIN32
#include <rad/libbase.h>
#include <rad/os_handles_base.h>
#include <rad/string.h>
#include <rad/system_error.h>

#include <memory>
#include <vector>

namespace RAD_LIB_NAMESPACE::os {
#ifdef _WIN32

    namespace detail {
        RAD_EXPORT_DECL void close_handle(HANDLE h) noexcept;
    }

    using handle =
        std::unique_ptr<void,
                        handle_deleter<HANDLE, // the handle type used by winapi
                                       detail::close_handle>>; // the cleaner
                                                               // function

    using process_handle = std::unique_ptr<
        void, // ignored, the pointer type is used from the deleter
        zero_neg_handle_deleter<intptr_t, // underlying type that stores the
                                          // handle
                                0,        // null handle value
                                true,     // a negative value represents a valid
                                          // handle
                                HANDLE,   // the handle type used by winapi
                                detail::close_handle // the cleaner function
                                >>;

    using file_handle =
        std::unique_ptr<void,
                        zero_neg_handle_deleter<
                            intptr_t, // underlying type that stores the handle
                            -1,     // null handle value (INVALID_HANDLE_VALUE)
                            false,  // a negative value represents an invalid
                                    // handle
                            HANDLE, // the handle type used by os api
                            detail::close_handle // the cleaner function
                            >>;

    // a thread handle can also be negative like process handle
    using thread_handle = process_handle;
#else
    using handle = std::unique_ptr<
        void,
        zero_neg_handle_deleter<int,   // underlying type that stores the handle
                                -1,    // null handle value
                                false, // a negative value represents an
                                       // invalid handle
                                int,   // the handle type used by os api
                                close  // the cleaner function
                                >>;
    using file_handle = handle;
#endif // _WIN32

#ifdef _WIN32
    // save typing some casts

    // for GetLastError()
    inline std::error_code make_system_error(DWORD val) noexcept {
        return std::error_code{static_cast<int>(val), system_category()};
    }
#endif // _WIN32

    // for WSAGetLastError() and unix errors
    inline std::error_code make_system_error(int val) noexcept {
        return std::error_code{val, system_category()};
    }

#ifndef _WIN32
    inline std::error_code make_system_error(ssize_t val) noexcept {
        return std::error_code{static_cast<int>(val), system_category()};
    }
#endif

#ifdef _WIN32

    using os_string = std::wstring; // null terminated owning os string
    using os_string_view =
        std::wstring_view; // not always null terminated view os string
    using os_zstring_view = wzstring_view; // null terminated view os string

    inline std::string string_to_string(os_string_view str) {
        return to_string(str);
    }

    inline std::vector<std::string>
    string_to_string(const std::vector<os_string>& os_strs) {
        std::vector<std::string> strs;
        strs.resize(os_strs.size());
        for (const auto& str : os_strs) {
            strs.emplace_back(to_string(str));
        }
        return strs;
    }

    /*
    return a null terminated wide string suitable for winapi functions.
    Caution: don't pass a temporary std::wstring or wzstring_view ! because
    the function will return a reference to this temporary
    */
    template <class StringType>
    decltype(auto) get_wstring(const StringType& str) {
        if constexpr (std::is_same_v<StringType, std::wstring> ||
                      std::is_same_v<StringType, wzstring_view>) {
            return str;
        }

        else if constexpr (std::is_convertible_v<StringType, wzstring_view>) {
            return static_cast<wzstring_view>(str);
        }

        else if constexpr (std::is_same_v<StringType, std::wstring_view>) {
            return std::wstring{str};
        }

        else if constexpr (std::is_convertible_v<StringType,
                                                 std::wstring_view>) {
            return std::wstring{static_cast<std::wstring_view>(str)};
        }

        else if constexpr (std::is_convertible_v<StringType,
                                                 std::string_view>) {
            return to_wstring(static_cast<std::string_view>(str));
        }
    }
#endif // _WIN32

}; // namespace RAD_LIB_NAMESPACE::os
