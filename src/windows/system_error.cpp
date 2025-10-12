#include <Windows.h>
#include <rad/string.h>
#include <rad/system_error.h>

using namespace rad;

namespace {

    constexpr bool is_whitespace(const wchar_t ch) {
        return ch == L' ' || ch == L'\n' || ch == L'\r' || ch == L'\t' ||
               ch == L'\0';
    }

    constexpr std::wstring_view remove_trailing_spaces(std::wstring_view str) {
        while (!str.empty() && is_whitespace(str.back())) {
            str.remove_suffix(1);
        }
        return str;
    }

    std::string format_system_error_code(DWORD code) {
        constexpr const char* unknown_error = "unknown error";

        wchar_t* ptr = nullptr;
        const DWORD flags = FORMAT_MESSAGE_ALLOCATE_BUFFER |
                            FORMAT_MESSAGE_FROM_SYSTEM |
                            FORMAT_MESSAGE_IGNORE_INSERTS;
        DWORD len =
            ::FormatMessageW(flags, nullptr, code, 0,
                             reinterpret_cast<wchar_t*>(&ptr), 0, nullptr);
        auto on_exit = scope_exit{[&] { ::LocalFree(ptr); }};
        if (len == 0 || !ptr) {
            return unknown_error;
        }
        auto msg =
            to_string(remove_trailing_spaces(std::wstring_view{ptr, len}));
        if (msg.empty()) {
            return unknown_error;
        }
        return msg;
    }

    struct system_category_t : public std::error_category {
        const char* name() const noexcept override {
            return "system";
        }

        std::string message(int condition) const override {
            return format_system_error_code(static_cast<DWORD>(condition));
        }
    };

    constexpr system_category_t system_category_inst;
} // namespace

namespace rad {
    const std::error_category& system_category() noexcept {
        return system_category_inst;
    }
} // namespace rad