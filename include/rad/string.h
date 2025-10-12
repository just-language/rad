#pragma once
#include <rad/libbase.h>
#include <rad/utf.h>

#include <charconv>
#include <cstring>
#include <cwctype>
#include <string>
#include <string_view>
#include <type_traits>

template <class CharT, class Traits, class Allocator>
std::basic_string<CharT, Traits, Allocator>
operator+(const std::basic_string<CharT, Traits, Allocator>& lhs,
          const std::basic_string_view<CharT, Traits>& rhs) {
    std::basic_string<CharT, Traits, Allocator> str;
    str.reserve(lhs.size() + rhs.size());
    str += lhs;
    str += rhs;
    return str;
}

template <class CharT, class Traits, class Allocator>
std::basic_string<CharT, Traits, Allocator>
operator+(const std::basic_string_view<CharT, Traits>& lhs,
          const std::basic_string<CharT, Traits, Allocator>& rhs) {
    std::basic_string<CharT, Traits, Allocator> str;
    str.reserve(lhs.size() + rhs.size());
    str += lhs;
    str += rhs;
    return str;
}

template <class CharT, class Traits>
std::basic_string<CharT, Traits>
operator+(const std::basic_string_view<CharT, Traits>& lhs,
          const std::basic_string_view<CharT, Traits>& rhs) {
    std::basic_string<CharT, Traits> str;
    str.reserve(lhs.size() + rhs.size());
    str += lhs;
    str += rhs;
    return str;
}

template <class CharT, class Traits>
std::basic_string<CharT, Traits>
operator+(const std::basic_string_view<CharT, Traits>& lhs, const CharT* rhs) {
    return operator+(lhs, std::basic_string_view<CharT, Traits>(rhs));
}

template <class CharT, class Traits>
std::basic_string<CharT, Traits>
operator+(const CharT* lhs, const std::basic_string_view<CharT, Traits>& rhs) {
    return operator+(std::basic_string_view<CharT, Traits>(lhs), rhs);
}

template <class CharT, class Traits>
std::basic_string<CharT, Traits>
operator+(const std::basic_string_view<CharT, Traits>& lhs, const CharT ch) {
    return operator+(lhs, std::basic_string_view<CharT, Traits>(&ch, 1));
}

template <class CharT, class Traits>
std::basic_string<CharT, Traits>
operator+(const CharT ch, const std::basic_string_view<CharT, Traits>& rhs) {
    return operator+(std::basic_string_view<CharT, Traits>(&ch, 1), rhs);
}

namespace RAD_LIB_NAMESPACE::detail {
    template <class CharT, class Traits>
    using svt = typename std::basic_string_view<CharT, Traits>;

    template <class CharT, class Traits, class Allocator>
    constexpr std::basic_string_view<CharT, Traits> get_str_view(
        const std::basic_string<CharT, Traits, Allocator>& str) noexcept {
        return str;
    }

    template <class CharT, class Traits>
    constexpr std::basic_string_view<CharT, Traits>
    get_str_view(const std::basic_string_view<CharT, Traits>& str) noexcept {
        return str;
    }

    template <class CharT>
    constexpr std::basic_string_view<CharT> get_str_view(const CharT* str) {
        return str;
    }

    template <class CharT>
    std::basic_string_view<CharT> get_str_view(const CharT& ch) {
        return {std::addressof(ch), 1};
    }

    template <class CharT, class Traits>
    std::size_t hash_string(svt<CharT, Traits> str) {
        return std::hash{str}();
    }

    template <class CharT>
    constexpr CharT tolower_ch(CharT ch) {
        if (ch >= static_cast<CharT>('A') && ch <= static_cast<CharT>('Z')) {
            return ch - static_cast<CharT>('A') + static_cast<CharT>('a');
        }
        return ch;
    }

    template <class CharT>
    constexpr CharT toupper_ch(CharT ch) {
        if (ch >= static_cast<CharT>('a') && ch <= static_cast<CharT>('z')) {
            return ch - static_cast<CharT>('a') + static_cast<CharT>('A');
        }
        return ch;
    }

    template <class CharT, class Traits>
    constexpr bool iequal(std::basic_string_view<CharT, Traits> str1,
                          std::basic_string_view<CharT, Traits> str2) {
        using size_type =
            typename std::basic_string_view<CharT, Traits>::size_type;
        if (str1.size() != str2.size()) {
            return false;
        }
        for (size_type i = 0; i < str1.size(); ++i) {
            if (tolower_ch(str1[i]) != tolower_ch(str2[i])) {
                return false;
            }
        }
        return true;
    }

    template <class CharT, class Traits>
    constexpr typename svt<CharT, Traits>::size_type
    ifind(svt<CharT, Traits> str, svt<CharT, Traits> to_find,
          typename svt<CharT, Traits>::size_type pos = 0) {
        constexpr auto npos = svt<CharT, Traits>::npos;
        using size_type = typename svt<CharT, Traits>::size_type;

        str = str.substr(pos);

        if (to_find.size() > str.size()) {
            return npos;
        }

        if (to_find.size() == str.size()) {
            return iequal(str, to_find) ? 0 : npos;
        }

        for (size_type i = 0; i < str.size(); ++i) {
            if (str.size() - i < to_find.size()) {
                return npos;
            }

            bool found = true;
            for (size_type j = 0; j < to_find.size(); ++j) {
                auto ch1 = tolower_ch(str[i + j]);
                auto ch2 = tolower_ch(to_find[j]);
                if (ch1 != ch2) {
                    found = false;
                    break;
                }
            }

            if (found) {
                return i + pos;
            }
        }

        return npos;
    }

    template <class CharT, class Traits>
    constexpr bool starts_with(svt<CharT, Traits> text,
                               svt<CharT, Traits> mark) noexcept {
        if (text.size() < mark.size()) {
            return false;
        }
        return svt<CharT, Traits>(text.data(), mark.size()) == mark;
    }

    template <class CharT, class Traits>
    constexpr bool
    istarts_with(std::basic_string_view<CharT, Traits> text,
                 std::basic_string_view<CharT, Traits> mark) noexcept {
        if (text.size() < mark.size()) {
            return false;
        }
        return iequal(svt<CharT, Traits>(text.data(), mark.size()), mark);
    }

    template <class CharT, class Traits>
    constexpr bool ends_with(svt<CharT, Traits> text,
                             svt<CharT, Traits> mark) noexcept {
        if (text.size() < mark.size()) {
            return false;
        }
        return svt<CharT, Traits>(text.data() + text.size() - mark.size(),
                                  mark.size()) == mark;
    }

    template <class CharT, class Traits>
    constexpr bool iends_with(svt<CharT, Traits> text,
                              svt<CharT, Traits> mark) noexcept {
        if (text.size() < mark.size()) {
            return false;
        }
        return iequal(svt<CharT, Traits>(
                          text.data() + text.size() - mark.size(), mark.size()),
                      mark);
    }

    template <class CharT, class Traits>
    constexpr bool contains(svt<CharT, Traits> text, svt<CharT, Traits> mark) {
        return text.find(mark) != svt<CharT, Traits>::npos;
    }

    template <class CharT, class Traits>
    constexpr bool icontains(svt<CharT, Traits> text, svt<CharT, Traits> mark) {
        return ifind(text, mark) != svt<CharT, Traits>::npos;
    }

    template <class CharT, class Traits>
    constexpr typename svt<CharT, Traits>::size_type
    count_occurance(svt<CharT, Traits> str, svt<CharT, Traits> to_find) {
        constexpr auto npos = svt<CharT, Traits>::npos;
        using size_type = typename svt<CharT, Traits>::size_type;

        size_type count = 0;
        size_type index = str.find(to_find, 0);

        while (index != npos) {
            ++count;
            index = str.find(to_find, ++index);
        }

        return count;
    }

    template <class CharT, class Traits>
    constexpr typename svt<CharT, Traits>::size_type
    icount_occurance(svt<CharT, Traits> str, svt<CharT, Traits> to_find) {
        constexpr auto npos = svt<CharT, Traits>::npos;
        using size_type = typename svt<CharT, Traits>::size_type;

        size_type count = 0;
        size_type index = ifind(str, to_find, 0);

        while (index != npos) {
            ++count;
            index = ifind(str, to_find, ++index);
        }

        return count;
    }

    template <class CharT, class Traits>
    constexpr typename svt<CharT, Traits>::size_type
    find_nth(svt<CharT, Traits> str, svt<CharT, Traits> to_find,
             typename svt<CharT, Traits>::size_type nth) {
        constexpr auto npos = svt<CharT, Traits>::npos;
        using size_type = typename svt<CharT, Traits>::size_type;

        size_type count = 0;
        size_type index = str.find(to_find);

        while (index != npos && count < nth) {
            ++count;
            index = str.find(to_find, index + to_find.size());
        }

        return index;
    }

    template <class CharT, class Traits>
    constexpr typename svt<CharT, Traits>::size_type
    ifind_nth(svt<CharT, Traits> str, svt<CharT, Traits> to_find,
              typename svt<CharT, Traits>::size_type nth) {
        constexpr auto npos = svt<CharT, Traits>::npos;
        using size_type = typename svt<CharT, Traits>::size_type;

        size_type count = 0;
        size_type index = ifind(str, to_find);

        while (index != npos && count < nth) {
            ++count;
            index = ifind(str, to_find, index + to_find.size());
        }

        return index;
    }

    template <class CharT, class Traits, class Allocator>
    void replace(std::basic_string<CharT, Traits, Allocator>& str,
                 svt<CharT, Traits> from, svt<CharT, Traits> to) {
        constexpr auto npos = std::basic_string<CharT, Traits, Allocator>::npos;
        using size_type =
            typename std::basic_string<CharT, Traits, Allocator>::size_type;

        size_type pos = str.find(from);
        if (pos != npos) {
            str.replace(pos, from.size(), to.data(), to.size());
        }
    }

    template <class CharT, class Traits, class Allocator>
    void ireplace(std::basic_string<CharT, Traits, Allocator>& str,
                  svt<CharT, Traits> from, svt<CharT, Traits> to) {
        constexpr auto npos = std::basic_string<CharT, Traits, Allocator>::npos;
        using size_type =
            typename std::basic_string<CharT, Traits, Allocator>::size_type;

        size_type pos = ifind(str, from);
        if (pos != npos) {
            str.replace(pos, from.size(), to.data(), to.size());
        }
    }

    template <class CharT, class Traits, class Allocator>
    void replace(
        std::basic_string<CharT, Traits, Allocator>& str,
        svt<CharT, Traits> from, svt<CharT, Traits> to,
        typename std::basic_string<CharT, Traits, Allocator>::size_type count) {
        constexpr auto npos = std::basic_string<CharT, Traits, Allocator>::npos;
        using size_type =
            typename std::basic_string<CharT, Traits, Allocator>::size_type;

        for (size_type pos = str.find(from); pos != npos && count;
             --count, pos = str.find(from, pos + to.size())) {
            str.replace(pos, from.size(), to.data(), to.size());
        }
    }

    template <class CharT, class Traits, class Allocator>
    void ireplace(
        std::basic_string<CharT, Traits, Allocator>& str,
        svt<CharT, Traits> from, svt<CharT, Traits> to,
        typename std::basic_string<CharT, Traits, Allocator>::size_type count) {
        constexpr auto npos = std::basic_string<CharT, Traits, Allocator>::npos;
        using size_type =
            typename std::basic_string<CharT, Traits, Allocator>::size_type;

        for (size_type pos = ifind(str, from); pos != npos && count;
             --count, pos = ifind(str, from, pos + to.size())) {
            str.replace(pos, from.size(), to.data(), to.size());
        }
    }

    template <class CharT, class Traits, class Allocator>
    void replace_all(std::basic_string<CharT, Traits, Allocator>& str,
                     svt<CharT, Traits> from, svt<CharT, Traits> to) {
        constexpr auto npos = std::basic_string<CharT, Traits, Allocator>::npos;
        using size_type =
            typename std::basic_string<CharT, Traits, Allocator>::size_type;

        for (size_type pos = str.find(from); pos != npos;
             pos = str.find(from, pos + to.size())) {
            str.replace(pos, from.size(), to.data(), to.size());
        }
    }

    template <class CharT, class Traits, class Allocator>
    void ireplace_all(std::basic_string<CharT, Traits, Allocator>& str,
                      svt<CharT, Traits> from, svt<CharT, Traits> to) {
        constexpr auto npos = std::basic_string<CharT, Traits, Allocator>::npos;
        using size_type =
            typename std::basic_string<CharT, Traits, Allocator>::size_type;

        for (size_type pos = ifind(svt<CharT, Traits>{str}, from); pos != npos;
             pos = ifind(svt<CharT, Traits>{str}, from, pos + to.size())) {
            str.replace(pos, from.size(), to.data(), to.size());
        }
    }
} // namespace RAD_LIB_NAMESPACE::detail

namespace RAD_LIB_NAMESPACE {
    template <class StringType>
    std::size_t hash_string(const StringType& str) {
        return detail::hash_string(detail::get_str_view(str));
    }

    template <class StringType1, class StringType2>
    constexpr bool iequal(const StringType1& str1, const StringType2& str2) {
        return detail::iequal(detail::get_str_view(str1),
                              detail::get_str_view(str2));
    }

    template <class StringType1, class StringType2>
    constexpr std::size_t ifind(const StringType1& str,
                                const StringType2& to_find,
                                std::size_t pos = 0) {
        return detail::ifind(detail::get_str_view(str),
                             detail::get_str_view(to_find), pos);
    }

    template <class StringType1, class StringType2>
    constexpr bool starts_with(const StringType1& text,
                               const StringType2& mark) noexcept {
        return detail::starts_with(detail::get_str_view(text),
                                   detail::get_str_view(mark));
    }

    template <class StringType1, class StringType2>
    constexpr bool istarts_with(const StringType1& text,
                                const StringType2& mark) noexcept {
        return detail::istarts_with(detail::get_str_view(text),
                                    detail::get_str_view(mark));
    }

    template <class StringType1, class StringType2>
    constexpr bool ends_with(const StringType1& text,
                             const StringType2& mark) noexcept {
        return detail::ends_with(detail::get_str_view(text),
                                 detail::get_str_view(mark));
    }

    template <class StringType1, class StringType2>
    constexpr bool iends_with(const StringType1& text,
                              const StringType2& mark) noexcept {
        return detail::iends_with(detail::get_str_view(text),
                                  detail::get_str_view(mark));
    }

    template <class StringType1, class StringType2>
    constexpr bool contains(const StringType1& text, const StringType2& mark) {
        return detail::contains(detail::get_str_view(text),
                                detail::get_str_view(mark));
    }

    template <class StringType1, class StringType2>
    constexpr bool icontains(const StringType1& text, const StringType2& mark) {
        return detail::icontains(detail::get_str_view(text),
                                 detail::get_str_view(mark));
    }

    template <class StringType1, class StringType2>
    constexpr std::size_t count_occurance(const StringType1& str,
                                          const StringType2& to_find) {
        return detail::count_occurance(detail::get_str_view(str),
                                       detail::get_str_view(to_find));
    }

    template <class StringType1, class StringType2>
    constexpr std::size_t icount_occurance(const StringType1& str,
                                           const StringType2& to_find) {
        return detail::icount_occurance(detail::get_str_view(str),
                                        detail::get_str_view(to_find));
    }

    template <class StringType1, class StringType2>
    constexpr std::size_t find_nth(const StringType1& str,
                                   const StringType2& to_find,
                                   std::size_t nth) {
        return detail::find_nth(detail::get_str_view(str),
                                detail::get_str_view(to_find), nth);
    }

    template <class StringType1, class StringType2>
    constexpr std::size_t ifind_nth(const StringType1& str,
                                    const StringType2& to_find,
                                    std::size_t nth) {
        return detail::ifind_nth(detail::get_str_view(str),
                                 detail::get_str_view(to_find), nth);
    }

    template <class CharT, class Traits, class Allocator>
    constexpr std::basic_string_view<CharT, Traits>
    subview(const std::basic_string<CharT, Traits, Allocator>& str,
            typename std::basic_string_view<CharT, Traits>::size_type pos = 0,
            typename std::basic_string_view<CharT, Traits>::size_type len =
                std::basic_string_view<CharT, Traits>::npos) {
        return std::basic_string_view<CharT, Traits>(str).substr(pos, len);
    }

    template <class CharT, class Traits, class Allocator, class StringType1,
              class StringType2>
    void replace(std::basic_string<CharT, Traits, Allocator>& str,
                 const StringType1& from, const StringType2& to) {
        return detail::replace(str, detail::get_str_view(from),
                               detail::get_str_view(to));
    }

    template <class CharT, class Traits, class Allocator, class StringType1,
              class StringType2>
    void ireplace(std::basic_string<CharT, Traits, Allocator>& str,
                  const StringType1& from, const StringType2& to) {
        return detail::ireplace(str, detail::get_str_view(from),
                                detail::get_str_view(to));
    }

    template <class CharT, class Traits, class Allocator, class StringType1,
              class StringType2>
    void replace(
        std::basic_string<CharT, Traits, Allocator>& str,
        const StringType1& from, const StringType2& to,
        typename std::basic_string<CharT, Traits, Allocator>::size_type count) {
        return detail::replace(str, detail::get_str_view(from),
                               detail::get_str_view(to), count);
    }

    template <class CharT, class Traits, class Allocator, class StringType1,
              class StringType2>
    void ireplace(
        std::basic_string<CharT, Traits, Allocator>& str,
        const StringType1& from, const StringType2& to,
        typename std::basic_string<CharT, Traits, Allocator>::size_type count) {
        return detail::ireplace(str, detail::get_str_view(from),
                                detail::get_str_view(to), count);
    }

    template <class CharT, class Traits, class Allocator, class StringType1,
              class StringType2>
    void replace_all(std::basic_string<CharT, Traits, Allocator>& str,
                     const StringType1& from, const StringType2& to) {
        return detail::replace_all(str, detail::get_str_view(from),
                                   detail::get_str_view(to));
    }

    template <class CharT, class Traits, class Allocator, class StringType1,
              class StringType2>
    void ireplace_all(std::basic_string<CharT, Traits, Allocator>& str,
                      const StringType1& from, const StringType2& to) {
        return detail::ireplace_all(str, detail::get_str_view(from),
                                    detail::get_str_view(to));
    }

    template <class CharT, class Traits, class Allocator>
    void
    erase_head(std::basic_string<CharT, Traits, Allocator>& str,
               typename std::basic_string<CharT, Traits, Allocator>::size_type
                   length) noexcept {
        str.erase(0, length);
    }

    template <class CharT, class Traits, class Allocator>
    void
    erase_tail(std::basic_string<CharT, Traits, Allocator>& str,
               typename std::basic_string<CharT, Traits, Allocator>::size_type
                   length) noexcept {
        str.erase(str.size() - length, length);
    }

    inline void lower_string(std::string& str) noexcept {
        for (auto& ch : str) {
            ch = detail::tolower_ch(ch);
        }
    }

    inline std::string to_lower(std::string_view str) {
        std::string lstr{str};
        lower_string(lstr);
        return lstr;
    }

    inline void lower_string(std::wstring& str) noexcept {
        for (auto& ch : str) {
            ch = detail::tolower_ch(ch);
        }
    }

    inline std::wstring to_lower(std::wstring_view str) {
        std::wstring lstr{str};
        lower_string(lstr);
        return lstr;
    }

    inline void upper_string(std::string& str) noexcept {
        for (auto& ch : str) {
            ch = detail::toupper_ch(ch);
        }
    }

    inline std::string to_upper(std::string_view str) {
        std::string ustr{str};
        upper_string(ustr);
        return ustr;
    }

    inline void upper_string(std::wstring& str) noexcept {
        for (auto& ch : str) {
            ch = detail::toupper_ch(ch);
        }
    }

    inline std::wstring to_upper(std::wstring_view str) {
        std::wstring ustr{str};
        upper_string(ustr);
        return ustr;
    }

    // utf-8 to utf-16 and utf-32 conversion, the result is in host byte
    // order

    inline void to_string(std::string_view from, std::string& to) {
        to = from;
    }

    inline std::string to_string(std::string_view from) {
        return std::string{from};
    }

    inline void to_string(std::wstring_view from, std::string& to) {
        wstring_to_string(from, to);
    }

    inline std::string to_string(std::wstring_view from) {
        std::string to;
        to_string(from, to);
        return to;
    }

    inline void to_string(std::u16string_view from, std::string& to) {
        u16string_to_string(from, to);
    }

    inline std::string to_string(std::u16string_view from) {
        std::string to;
        to_string(from, to);
        return to;
    }

    inline void to_string(std::u32string_view from, std::string& to) {
        u32string_to_string(from, to);
    }

    inline std::string to_string(std::u32string_view from) {
        std::string to;
        to_string(from, to);
        return to;
    }

    // wide to utf-8 and utf-32 conversion, the input is in host byte order

    inline void to_wstring(std::wstring_view from, std::wstring& to) {
        to = from;
    }

    inline std::wstring to_wstring(std::wstring_view from) {
        return std::wstring{from};
    }

    inline void to_wstring(std::string_view from, std::wstring& to) {
        string_to_wstring(from, to);
    }

    inline std::wstring to_wstring(std::string_view from) {
        std::wstring to;
        to_wstring(from, to);
        return to;
    }

    inline void to_wstring(std::u16string_view from, std::wstring& to) {
        u16string_to_wstring(from, to);
    }

    inline std::wstring to_wstring(std::u16string_view from) {
        std::wstring to;
        to_wstring(from, to);
        return to;
    }

    inline void to_wstring(std::u32string_view from, std::wstring& to) {
        u32string_to_wstring(from, to);
    }

    inline std::wstring to_wstring(std::u32string_view from) {
        std::wstring to;
        to_wstring(from, to);
        return to;
    }

    // to utf-16 conversion, the input is in host byte order

    inline void to_u16string(std::u16string_view from, std::u16string& to) {
        to = from;
    }

    inline std::u16string to_u16string(std::u16string_view from) {
        return std::u16string{from};
    }

    inline void to_u16string(std::string_view from, std::u16string& to) {
        string_to_u16string(from, to);
    }

    inline std::u16string to_u16string(std::string_view from) {
        std::u16string to;
        to_u16string(from, to);
        return to;
    }

    inline void to_u16string(std::wstring_view from, std::u16string& to) {
        wstring_to_u16string(from, to);
    }

    inline std::u16string to_u16string(std::wstring_view from) {
        std::u16string to;
        to_u16string(from, to);
        return to;
    }

    inline void to_u16string(std::u32string_view from, std::u16string& to) {
        u32string_to_u16string(from, to);
    }

    inline std::u16string to_u16string(std::u32string_view from) {
        std::u16string to;
        to_u16string(from, to);
        return to;
    }

    // to utf-32 conversion

    inline void to_u32string(std::u32string_view from, std::u32string& to) {
        to = from;
    }

    inline std::u32string to_u32string(std::u32string_view from) {
        return std::u32string{from};
    }

    inline void to_u32string(std::string_view from, std::u32string& to) {
        string_to_u32string(from, to);
    }

    inline std::u32string to_u32string(std::string_view from) {
        std::u32string to;
        to_u32string(from, to);
        return to;
    }

    inline void to_u32string(std::wstring_view from, std::u32string& to) {
        wstring_to_u32string(from, to);
    }

    inline std::u32string to_u32string(std::wstring_view from) {
        std::u32string to;
        to_u32string(from, to);
        return to;
    }

    inline void to_u32string(std::u16string_view from, std::u32string& to) {
        u16string_to_u32string(from, to);
    }

    inline std::u32string to_u32string(std::u16string_view from) {
        std::u32string to;
        to_u32string(from, to);
        return to;
    }

    template <class NumType>
    inline NumType to_numeric(std::string_view from, int base,
                              std::error_code& ec) noexcept {
        ec.clear();
        NumType ret{};
        const char* last = from.data() + from.size();
        auto res = std::from_chars(from.data(), last, ret, base);
        if (res.ec != std::errc{}) {
            ec = std::make_error_code(res.ec);
        }
        if (res.ptr != last) {
            ec = std::make_error_code(std::errc::invalid_argument);
        }
        return ret;
    }

    template <class NumType>
    inline NumType to_numeric(std::string_view from, int base) {
        std::error_code ec;
        auto ret = to_numeric<NumType>(from, base, ec);
        check_and_throw(ec, "");
        return ret;
    }

#if __cpp_lib_to_chars >= 201611L
    template <class FloatType>
    inline FloatType to_floating_point(std::string_view from,
                                       std::chars_format fmt,
                                       std::error_code& ec) noexcept {
        FloatType ret{};
        const char* last = from.data() + from.size();
        auto res = std::from_chars(from.data(), last, ret, fmt);
        if (res.ec != std::errc{}) {
            ec = std::make_error_code(res.ec);
        }
        if (res.ptr != last) {
            ec = std::make_error_code(std::errc::invalid_argument);
        }
        return ret;
    }

    template <class FloatType>
    inline FloatType
    to_floating_point(std::string_view from,
                      std::chars_format fmt = std::chars_format::general) {
        std::error_code ec;
        auto ret = to_floating_point<FloatType>(from, fmt, ec);
        check_and_throw(ec, "");
        return ret;
    }
#endif // __cpp_lib_to_chars >= 201611L

    inline auto to_int8(std::string_view from, int base,
                        std::error_code& ec) noexcept {
        return to_numeric<int8_t>(from, base, ec);
    }

    inline auto to_int8(std::string_view from, int base = 10) {
        return to_numeric<int8_t>(from, base);
    }

    inline auto to_int16(std::string_view from, int base,
                         std::error_code& ec) noexcept {
        return to_numeric<int16_t>(from, base, ec);
    }

    inline auto to_int16(std::string_view from, int base = 10) {
        return to_numeric<int16_t>(from, base);
    }

    inline auto to_int32(std::string_view from, int base,
                         std::error_code& ec) noexcept {
        return to_numeric<int32_t>(from, base, ec);
    }

    inline auto to_int32(std::string_view from, int base = 10) {
        return to_numeric<int32_t>(from, base);
    }

    inline auto to_int64(std::string_view from, int base,
                         std::error_code& ec) noexcept {
        return to_numeric<int64_t>(from, base, ec);
    }

    inline auto to_int64(std::string_view from, int base = 10) {
        return to_numeric<int64_t>(from, base);
    }

    inline auto to_int(std::string_view from, int base,
                       std::error_code& ec) noexcept {
        return to_numeric<int>(from, base, ec);
    }

    inline auto to_int(std::string_view from, int base = 10) {
        return to_numeric<int>(from, base);
    }

    inline auto to_uint8(std::string_view from, int base,
                         std::error_code& ec) noexcept {
        return to_numeric<uint8_t>(from, base, ec);
    }

    inline auto to_uint8(std::string_view from, int base = 10) {
        return to_numeric<uint8_t>(from, base);
    }

    inline auto to_uint16(std::string_view from, int base,
                          std::error_code& ec) noexcept {
        return to_numeric<uint16_t>(from, base, ec);
    }

    inline auto to_uint16(std::string_view from, int base = 10) {
        return to_numeric<uint16_t>(from, base);
    }

    inline auto to_uint32(std::string_view from, int base,
                          std::error_code& ec) noexcept {
        return to_numeric<uint32_t>(from, base, ec);
    }

    inline auto to_uint32(std::string_view from, int base = 10) {
        return to_numeric<uint32_t>(from, base);
    }

    inline auto to_uint64(std::string_view from, int base,
                          std::error_code& ec) noexcept {
        return to_numeric<uint64_t>(from, base, ec);
    }

    inline auto to_uint64(std::string_view from, int base = 10) {
        return to_numeric<uint64_t>(from, base);
    }

    inline auto to_uint(std::string_view from, int base,
                        std::error_code& ec) noexcept {
        return to_numeric<unsigned int>(from, base, ec);
    }

    inline auto to_uint(std::string_view from, int base = 10) {
        return to_numeric<unsigned int>(from, base);
    }

    inline auto to_size_t(std::string_view from, int base,
                          std::error_code& ec) noexcept {
        return to_numeric<std::size_t>(from, base, ec);
    }

    inline auto to_size_t(std::string_view from, int base = 10) {
        return to_numeric<std::size_t>(from, base);
    }

#if __cpp_lib_to_chars >= 201611L
    inline auto to_float(std::string_view from, std::chars_format fmt,
                         std::error_code& ec) noexcept {
        return to_floating_point<float>(from, fmt, ec);
    }

    inline auto to_float(std::string_view from,
                         std::chars_format fmt = std::chars_format::general) {
        return to_floating_point<float>(from, fmt);
    }

    inline auto to_double(std::string_view from, std::chars_format fmt,
                          std::error_code& ec) noexcept {
        return to_floating_point<double>(from, fmt, ec);
    }

    inline auto to_double(std::string_view from,
                          std::chars_format fmt = std::chars_format::general) {
        return to_floating_point<double>(from, fmt);
    }
#endif // __cpp_lib_to_chars >= 201611L

    template <class CharT, class Traits = std::char_traits<CharT>>
    class basic_zstring_view : public std::basic_string_view<CharT, Traits> {
        using base = std::basic_string_view<CharT, Traits>;

    public:
        using typename base::size_type;

        constexpr basic_zstring_view() noexcept = default;

        constexpr basic_zstring_view(const CharT* s) noexcept : base(s) {
        }

        constexpr basic_zstring_view(const CharT* s, size_type n) noexcept
            : base(s, n) {
        }

        template <class Allocator>
        basic_zstring_view(
            const std::basic_string<CharT, Traits, Allocator>& str) noexcept {
            size_type n = str.size();
            const CharT* s = n ? str.c_str() : nullptr;
            *this = basic_zstring_view{s, n};
        }

        void remove_suffix(size_type n) = delete;
    };

    using zstring_view = basic_zstring_view<char>;
    using wzstring_view = basic_zstring_view<wchar_t>;
    using u16zstring_view = basic_zstring_view<char16_t>;
    using u32zstring_view = basic_zstring_view<char32_t>;
#ifdef __cpp_char8_t
    using u8zstring_view = basic_zstring_view<char8_t>;
#endif // __cpp_char8_t

} // namespace RAD_LIB_NAMESPACE

namespace RAD_LIB_NAMESPACE::detail::split_utils {
    template <class CharT>
    inline constexpr bool is_char = false;

    template <>
    inline constexpr bool is_char<char> = true;

    template <>
    inline constexpr bool is_char<wchar_t> = true;

    template <>
    inline constexpr bool is_char<char16_t> = true;

    template <>
    inline constexpr bool is_char<char32_t> = true;

    template <class CharT, class Traits, bool OwnString>
    struct string_type {
        using type = std::basic_string_view<CharT, Traits>;
    };

    template <class CharT, class Traits>
    struct string_type<CharT, Traits, true> {
        using type = std::basic_string<CharT, Traits>;
    };

    template <class StringType>
    class split_range_struct {
    public:
        using string_type = StringType;
        using size_type = typename string_type::size_type;

        struct splitter_end_mark {};

        class iterator {
        public:
            constexpr iterator(string_type str, string_type separator) noexcept
                : src{str}, delim{separator}, first_pos{0},
                  last_pos{src.find(delim)} {
            }

            constexpr iterator& operator++() noexcept {
                first_pos = last_pos + delim.size();
                last_pos = src.find(delim, first_pos);
                return *this;
            }

            constexpr string_type operator*() const noexcept {
                if (last_pos != string_type::npos) {
                    return src.substr(first_pos, last_pos - first_pos);
                }
                finished = true;
                return src.substr(first_pos, src.size() - first_pos);
            }

            friend constexpr bool
            operator!=(const iterator& iter,
                       splitter_end_mark /*unused*/) noexcept {
                return !iter.finished;
            }

            friend constexpr bool operator!=(splitter_end_mark /*unused*/,
                                             const iterator& iter) noexcept {
                return !iter.finished;
            }

        private:
            string_type src;
            string_type delim;
            size_type first_pos;
            size_type last_pos;
            mutable bool finished = false;
        };

        constexpr split_range_struct(StringType src, StringType delim) noexcept
            : src{src}, delim{delim} {
        }

        auto begin() const noexcept {
            return iterator{src, delim};
        }

        constexpr auto end() const noexcept {
            return splitter_end_mark{};
        }

    private:
        string_type src;
        string_type delim;
    };

    template <class StringType>
    class split_with_pipe {
    public:
        using range_type = split_range_struct<StringType>;
        using string_type = StringType;

        constexpr split_with_pipe(StringType delim) noexcept
            : delim_view{delim} {
        }

        friend constexpr range_type
        operator|(string_type str, split_with_pipe splitter) noexcept {
            return range_type{str, splitter.delim_view};
        }

    private:
        string_type delim_view;
    };

    template <class CharT, class Traits, class Fn>
    inline void split_fn(std::basic_string_view<CharT, Traits> str,
                         std::basic_string_view<CharT, Traits> delim, Fn fn) {
        using string_type = typename std::basic_string_view<CharT, Traits>;
        using size_type = typename string_type::size_type;
        size_type start_pos = 0;
        for (size_type end_pos = str.find(delim); end_pos != string_type::npos;
             start_pos = end_pos + delim.size(),
                       end_pos = str.find(delim, start_pos)) {
            fn(str.substr(start_pos, end_pos - start_pos));
        }
        fn(str.substr(start_pos, str.size() - start_pos));
    }
} // namespace RAD_LIB_NAMESPACE::detail::split_utils

namespace RAD_LIB_NAMESPACE {
    inline constexpr auto split(std::string_view delim) noexcept {
        using namespace detail::split_utils;
        return split_with_pipe{delim};
    }

#if __cplusplus >= 202002L
    inline constexpr auto split(std::u8string_view delim) noexcept {
        using namespace detail::split_utils;
        return split_with_pipe{delim};
    }
#endif // __cplusplus >= 202002L

    inline constexpr auto split(std::u16string_view delim) noexcept {
        using namespace detail::split_utils;
        return split_with_pipe{delim};
    }

    inline constexpr auto split(std::u32string_view delim) noexcept {
        using namespace detail::split_utils;
        return split_with_pipe{delim};
    }

    inline constexpr auto split(std::wstring_view delim) noexcept {
        using namespace detail::split_utils;
        return split_with_pipe{delim};
    }

    inline constexpr auto split(std::string_view str, std::string_view delim) {
        using namespace detail::split_utils;
        return split_range_struct{str, delim};
    }

#if __cplusplus >= 202002L
    inline constexpr auto split(std::u8string_view str,
                                std::u8string_view delim) {
        using namespace detail::split_utils;
        return split_range_struct{str, delim};
    }
#endif // __cplusplus >= 202002L

    inline constexpr auto split(std::u16string_view str,
                                std::u16string_view delim) {
        using namespace detail::split_utils;
        return split_range_struct{str, delim};
    }

    inline constexpr auto split(std::u32string_view str,
                                std::u32string_view delim) {
        using namespace detail::split_utils;
        return split_range_struct{str, delim};
    }

    inline constexpr auto split(std::wstring_view str,
                                std::wstring_view delim) {
        using namespace detail::split_utils;
        return split_range_struct{str, delim};
    }

    template <class Fn>
    void split(std::string_view str, std::string_view delim, Fn fn) {
        detail::split_utils::split_fn(str, delim, fn);
    }

#if __cplusplus >= 202002L
    template <class Fn>
    void split(std::u8string_view str, std::u8string_view delim, Fn fn) {
        detail::split_utils::split_fn(str, delim, fn);
    }
#endif // __cplusplus >= 202002L

    template <class Fn>
    void split(std::u16string_view str, std::u16string_view delim, Fn fn) {
        detail::split_utils::split_fn(str, delim, fn);
    }

    template <class Fn>
    void split(std::u32string_view str, std::u32string_view delim, Fn fn) {
        detail::split_utils::split_fn(str, delim, fn);
    }

    template <class Fn>
    void split(std::wstring_view str, std::wstring_view delim, Fn fn) {
        detail::split_utils::split_fn(str, delim, fn);
    }
} // namespace RAD_LIB_NAMESPACE
