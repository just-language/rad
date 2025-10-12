#include <rad/utf.h>

#include <algorithm>

namespace {
    constexpr auto cp1256_to_unicode_table = std::array<uint16_t, 128>{
        // Windows-1256 specific range (0x80-0xFF)
        0x20AC, 0x067E, 0x201A, 0x0192,
        0x201E, 0x2026, 0x2020, 0x2021, // 0x80-0x87
        0x02C6, 0x2030, 0x0679, 0x2039,
        0x0152, 0x0686, 0x0698, 0x0688, // 0x88-0x8F
        0x06AF, 0x2018, 0x2019, 0x201C,
        0x201D, 0x2022, 0x2013, 0x2014, // 0x90-0x97
        0x06A9, 0x2122, 0x0691, 0x203A,
        0x0153, 0x200C, 0x200D, 0x06BA, // 0x98-0x9F
        0x00A0, 0x060C, 0x00A2, 0x00A3,
        0x00A4, 0x00A5, 0x00A6, 0x00A7, // 0xA0-0xA7
        0x00A8, 0x00A9, 0x06BE, 0x00AB,
        0x00AC, 0x00AD, 0x00AE, 0x00AF, // 0xA8-0xAF
        0x00B0, 0x00B1, 0x00B2, 0x00B3,
        0x00B4, 0x00B5, 0x00B6, 0x00B7, // 0xB0-0xB7
        0x00B8, 0x00B9, 0x061B, 0x00BB,
        0x00BC, 0x00BD, 0x00BE, 0x061F, // 0xB8-0xBF
        0x06C1, 0x0621, 0x0622, 0x0623,
        0x0624, 0x0625, 0x0626, 0x0627, // 0xC0-0xC7
        0x0628, 0x0629, 0x062A, 0x062B,
        0x062C, 0x062D, 0x062E, 0x062F, // 0xC8-0xCF
        0x0630, 0x0631, 0x0632, 0x0633,
        0x0634, 0x0635, 0x0636, 0x00D7, // 0xD0-0xD7
        0x0637, 0x0638, 0x0639, 0x063A,
        0x0640, 0x0641, 0x0642, 0x0643, // 0xD8-0xDF
        0x00E0, 0x0644, 0x00E2, 0x0645,
        0x0646, 0x0647, 0x0648, 0x00E7, // 0xE0-0xE7
        0x00E8, 0x00E9, 0x00EA, 0x00EB,
        0x0649, 0x064A, 0x00EE, 0x00EF, // 0xE8-0xEF
        0x064B, 0x064C, 0x064D, 0x064E,
        0x00F4, 0x064F, 0x0650, 0x00F7, // 0xF0-0xF7
        0x0651, 0x00F9, 0x0652, 0x00FB,
        0x00FC, 0x200E, 0x200F, 0x06D2 // 0xF8-0xFF
    };
}

namespace RAD_LIB_NAMESPACE {

    codepoint_t
    detail::map_cp1256_to_unicode(std::uint8_t cp1256_char) noexcept {
        if (cp1256_char <= 127) {
            return cp1256_char;
        }
        return cp1256_to_unicode_table[cp1256_char - 128];
    }

    std::pair<bool, std::uint8_t>
    detail::map_unicode_to_cp1256(codepoint_t cp) noexcept {
        if (cp <= 127) {
            return std::pair{true, static_cast<uint8_t>(cp)};
        }
        auto it = std::find(cp1256_to_unicode_table.begin(),
                            cp1256_to_unicode_table.end(), cp);
        if (it != cp1256_to_unicode_table.end()) {
            size_t idx =
                std::distance(cp1256_to_unicode_table.begin(), it) + 128;
            return std::pair{true, static_cast<uint8_t>(idx)};
        }
        return std::pair{false, 0};
    }

    void string_to_wstring(std::string_view from, std::wstring& to) {
        utf_converter<utf8_codecvt, wchar_codecvt> cvt;
        std::error_code ec;
        cvt.convert(from, to, ec);
        if (ec) {
            throw std::system_error{ec};
        }
    }

    void wstring_to_string(std::wstring_view from, std::string& to) {
        utf_converter<wchar_codecvt, utf8_codecvt> cvt;
        std::error_code ec;
        cvt.convert(from, to, ec);
        if (ec) {
            throw std::system_error{ec};
        }
    }

    void string_to_u16string(std::string_view from, std::u16string& to) {
        utf_converter<utf8_codecvt, utf16_codecvt> cvt;
        std::error_code ec;
        cvt.convert(from, to, ec);
        if (ec) {
            throw std::system_error{ec};
        }
    }

    void u16string_to_string(std::u16string_view from, std::string& to) {
        utf_converter<utf16_codecvt, utf8_codecvt> cvt;
        std::error_code ec;
        cvt.convert(from, to, ec);
        if (ec) {
            throw std::system_error{ec};
        }
    }

    void string_to_u32string(std::string_view from, std::u32string& to) {
        utf_converter<utf8_codecvt, utf32_codecvt> cvt;
        std::error_code ec;
        cvt.convert(from, to, ec);
        if (ec) {
            throw std::system_error{ec};
        }
    }

    void u32string_to_string(std::u32string_view from, std::string& to) {
        utf_converter<utf32_codecvt, utf8_codecvt> cvt;
        std::error_code ec;
        cvt.convert(from, to, ec);
        if (ec) {
            throw std::system_error{ec};
        }
    }

    void u16string_to_u32string(std::u16string_view from, std::u32string& to) {
        utf_converter<utf16_codecvt, utf32_codecvt> cvt;
        std::error_code ec;
        cvt.convert(from, to, ec);
        if (ec) {
            throw std::system_error{ec};
        }
    }

    void u32string_to_u16string(std::u32string_view from, std::u16string& to) {
        utf_converter<utf32_codecvt, utf16_codecvt> cvt;
        std::error_code ec;
        cvt.convert(from, to, ec);
        if (ec) {
            throw std::system_error{ec};
        }
    }

    void wstring_to_u16string(std::wstring_view from, std::u16string& to) {
        utf_converter<wchar_codecvt, utf16_codecvt> cvt;
        std::error_code ec;
        cvt.convert(from, to, ec);
        if (ec) {
            throw std::system_error{ec};
        }
    }

    void u16string_to_wstring(std::u16string_view from, std::wstring& to) {
        utf_converter<utf16_codecvt, wchar_codecvt> cvt;
        std::error_code ec;
        cvt.convert(from, to, ec);
        if (ec) {
            throw std::system_error{ec};
        }
    }

    void wstring_to_u32string(std::wstring_view from, std::u32string& to) {
        utf_converter<wchar_codecvt, utf32_codecvt> cvt;
        std::error_code ec;
        cvt.convert(from, to, ec);
        if (ec) {
            throw std::system_error{ec};
        }
    }

    void u32string_to_wstring(std::u32string_view from, std::wstring& to) {
        utf_converter<utf32_codecvt, wchar_codecvt> cvt;
        std::error_code ec;
        cvt.convert(from, to, ec);
        if (ec) {
            throw std::system_error{ec};
        }
    }

    void cp1256_to_utf8(std::string_view from, std::string& to) {
        utf_converter<cp1256_codecvt, utf8_codecvt> cvt;
        std::error_code ec;
        cvt.convert(from, to, ec);
        if (ec) {
            throw std::system_error{ec};
        }
    }
} // namespace RAD_LIB_NAMESPACE
