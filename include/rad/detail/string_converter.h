#pragma once
#include <rad/string.h>

namespace RAD_LIB_NAMESPACE::detail {
    template <class String, class Alternative1, class Alternative2>
    struct string_converter {
        Alternative1 to_native_string(const Alternative2& str) {
            using char_type = typename Alternative1::value_type;

            if constexpr (std::is_same_v<char_type, wchar_t>) {
                return to_wstring(str);
            }
            else {
                return to_string(str);
            }
        }

        template <class StrType>
        decltype(auto) operator()(const StrType& str) {
            if constexpr (std::is_same_v<String, StrType>) {
                return str;
            }
            else if constexpr (std::is_constructible_v<String, StrType>) {
                return static_cast<String>(str);
            }
            else if constexpr (std::is_constructible_v<Alternative1, StrType>) {
                return static_cast<Alternative1>(str);
            }
            else if constexpr (std::is_constructible_v<Alternative2, StrType>) {
                return to_native_string(static_cast<Alternative2>(str));
            }
        }
    };
} // namespace RAD_LIB_NAMESPACE::detail