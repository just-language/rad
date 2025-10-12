#include <rad/windows/winreg.h>
#include <Windows.h>

using namespace RAD_LIB_NAMESPACE;
using namespace winreg;

namespace {

    inline std::error_code make_registry_error(LSTATUS e) {
        return std::error_code{e, system_category()};
    }

    inline void throw_if_error(LSTATUS result, const char* fn_name) {
        if (result != ERROR_SUCCESS) {
            check_and_throw(true, make_registry_error(result), fn_name);
        }
    }

    bool is_predefined_hkey(HKEY h) noexcept {
        return h == HKEY_CLASSES_ROOT || h == HKEY_CURRENT_USER ||
               h == HKEY_LOCAL_MACHINE || h == HKEY_USERS ||
               h == HKEY_PERFORMANCE_DATA || h == HKEY_PERFORMANCE_TEXT ||
               h == HKEY_PERFORMANCE_NLSTEXT || h == HKEY_CURRENT_CONFIG ||
               h == HKEY_DYN_DATA || h == HKEY_CURRENT_USER_LOCAL_SETTINGS;
    }

    const wchar_t empty_str[] = {L'\0'};

    const wchar_t* get_zwstr_ptr(wzstring_view str) noexcept {
        const wchar_t* p = str.data();
        if (p == nullptr || str.empty()) {
            return empty_str;
        }
        return p;
    }

    template <class T> // either string_view, string, wstring_view or wstring
    std::vector<uint8_t> build_multi_str(std::span<const T> data) {
        using char_type = typename T::value_type;

        std::vector<uint8_t> built_str;
        if (data.empty()) {
            // make an empty multi string
            built_str.insert(built_str.end(), sizeof(char_type) * 2, 0);
            return built_str;
        }

        size_t data_size = 0;
        for (const auto& str : data) {
            data_size += (str.size() + 1) *
                         sizeof(char_type); // the size is in bytes
                                            // including the null terminator
        }
        data_size += sizeof(char_type); // for the last null terminator
        built_str.reserve(data_size);   // save some allocations

        for (const auto& str : data) {
            auto first = reinterpret_cast<const uint8_t*>(str.data());
            auto last = first + str.size() * sizeof(char_type);
            built_str.insert(built_str.end(), first, last);
            built_str.insert(built_str.end(), sizeof(char_type), 0);
        }
        // the last null terminator
        built_str.insert(built_str.end(), sizeof(char_type), 0);
        return built_str;
    }

} // namespace

namespace fns = winreg::detail;

HKEY fns::create_key(HKEY base_key, wzstring_view sub_key, options option,
                     access access, disposition& disposition, void* security) {
    HKEY new_key = nullptr;
    DWORD dw_disposition = 0;
    auto result = ::RegCreateKeyExW(
        base_key, get_zwstr_ptr(sub_key), 0, nullptr,
        static_cast<DWORD>(option), static_cast<DWORD>(access),
        static_cast<LPSECURITY_ATTRIBUTES>(security), &new_key,
        &dw_disposition);

    throw_if_error(result, "RegCreateKeyExW");

    disposition = static_cast<winreg::disposition>(dw_disposition);
    if (is_predefined_hkey(new_key)) {
        return nullptr;
    }
    return new_key;
}

void fns::close_handle(HKEY h) noexcept {
    const LSTATUS res = ::RegCloseKey(h);
    assert(res == ERROR_SUCCESS);
    ((void)res);
}

HKEY fns::open_key(HKEY base_key, wzstring_view sub_key, bool symbolic_link,
                   access access) {
    HKEY new_key = nullptr;
    DWORD dw_opt = symbolic_link ? REG_OPTION_OPEN_LINK : 0;
    LSTATUS result = ::RegOpenKeyExW(base_key, get_zwstr_ptr(sub_key), dw_opt,
                                     static_cast<REGSAM>(access), &new_key);
    throw_if_error(result, "RegCreateKeyExW");
    if (is_predefined_hkey(new_key)) {
        return nullptr;
    }
    return new_key;
}

void fns::set_dword_value(HKEY k, wzstring_view name, uint32_t val) {
    throw_if_error(RegSetValueExW(k, get_zwstr_ptr(name), 0, REG_DWORD,
                                  reinterpret_cast<const uint8_t*>(&val),
                                  sizeof(DWORD)),
                   "RegSetValueExW");
}

void fns::set_qword_value(HKEY k, wzstring_view name, uint64_t val) {
    throw_if_error(RegSetValueExW(k, get_zwstr_ptr(name), 0, REG_QWORD,
                                  reinterpret_cast<const uint8_t*>(&val),
                                  sizeof(uint64_t)),
                   "RegSetValueExW");
}

void fns::set_bin_value(HKEY k, wzstring_view name, const_buffer val) {
    throw_if_error(RegSetValueExW(k, get_zwstr_ptr(name), 0, REG_BINARY,
                                  val.data_as<const uint8_t>(),
                                  static_cast<DWORD>(val.size())),
                   "RegSetValueExW");
}

void fns::set_none_value(HKEY k, wzstring_view name, const_buffer val) {
    throw_if_error(RegSetValueExW(k, get_zwstr_ptr(name), 0, REG_NONE,
                                  val.data_as<const uint8_t>(),
                                  static_cast<DWORD>(val.size())),
                   "RegSetValueExW");
}

void fns::set_string_value(HKEY k, wzstring_view name, wzstring_view val,
                           bool expand) {
    const uint8_t* ptr = reinterpret_cast<const uint8_t*>(val.data());
    // size includes the null terminator
    uint32_t size = static_cast<uint32_t>(val.size());
    if (!size) {
        ptr = nullptr;
    }
    else if (val.back() != L'\0') {
        // if there is already a null don't include another one!
        ++size;
    }

    DWORD val_type = expand ? REG_EXPAND_SZ : REG_SZ;
    throw_if_error(
        RegSetValueExW(k, get_zwstr_ptr(name), 0, val_type, ptr, size),
        "RegSetValueExW");
}

void fns::set_multi_string(HKEY k, wzstring_view name,
                           std::span<const std::wstring> val) {
    auto built_str = build_multi_str(val);
    throw_if_error(RegSetValueExW(k, get_zwstr_ptr(name), 0, REG_MULTI_SZ,
                                  built_str.data(),
                                  static_cast<uint32_t>(built_str.size())),
                   "RegSetValueExW");
}

value_type fns::get_value_type(HKEY k, wzstring_view name) {
    DWORD type;
    throw_if_error(RegQueryValueExW(k, get_zwstr_ptr(name), nullptr, &type,
                                    nullptr, nullptr),
                   "RegQueryValueExW");
    return static_cast<value_type>(type);
}

key_info fns::get_key_info(HKEY k) {
    DWORD sub_keys = 0, max_sub_key_name = 0, values = 0, max_value_name = 0,
          max_value_data = 0;
    FILETIME last_write_time{};

    const LSTATUS result = RegQueryInfoKeyW(
        k, nullptr, nullptr, nullptr, &sub_keys, &max_sub_key_name, nullptr,
        &values, &max_value_name, &max_value_data, nullptr, &last_write_time);

    throw_if_error(result, "RegQueryInfoKeyW");

    key_info info;
    info.sub_keys = sub_keys;
    info.max_sub_key_name = max_sub_key_name;
    info.values = values;
    info.max_value_name = max_value_name;
    info.max_value_data = max_value_data;
    ULARGE_INTEGER ul{};
    ul.LowPart = last_write_time.dwLowDateTime;
    ul.HighPart = last_write_time.dwHighDateTime;
    info.last_write_time =
        windows_clock::time_point{windows_clock::duration{ul.QuadPart}};
    return info;
}

bool fns::fetch_sub_key(HKEY k, uint32_t index, wchar_t* name, uint32_t& len) {
    len = 0;
    DWORD dw_len = 0;
    auto result = RegEnumKeyExW(k, index, name, &dw_len, nullptr, nullptr,
                                nullptr, nullptr);
    len = dw_len;

    if (result == ERROR_SUCCESS) {
        return false;
    }

    if (result == ERROR_NO_MORE_ITEMS) {
        return true;
    }

    throw_if_error(result, "RegEnumKeyExW");
    return false;
}

bool fns::fetch_value_type(HKEY k, uint32_t index, std::wstring& name,
                           value_type& type) {

    name.resize(name.capacity());
    DWORD name_size = static_cast<DWORD>(name.size()) + 1;

    DWORD val_type = 0;
    LSTATUS result = RegEnumValueW(k, index, name.data(), &name_size, nullptr,
                                   &val_type, nullptr, nullptr);
    type = static_cast<value_type>(val_type);

    if (result == ERROR_SUCCESS) {
        name.resize(name_size);
        return false;
    }

    if (result == ERROR_NO_MORE_ITEMS) {
        name.resize(name_size);
        return true;
    }

    throw_if_error(result, "RegEnumValueW");
    return false;
}

void fns::delete_subkey(HKEY k, wzstring_view sub_key) {
    throw_if_error(RegDeleteKeyW(k, get_zwstr_ptr(sub_key)), "RegDeleteKeyW");
}

void fns::delete_subkey(HKEY k, wzstring_view sub_key, key_arch arch) {
    throw_if_error(RegDeleteKeyExW(k, get_zwstr_ptr(sub_key),
                                   static_cast<REGSAM>(arch), 0),
                   "RegDeleteKeyExW");
}

void fns::delete_value(HKEY k, wzstring_view name) {
    throw_if_error(RegDeleteValueW(k, get_zwstr_ptr(name)), "RegDeleteValueW");
}

void fns::delete_value(HKEY k, wzstring_view sub_key, wzstring_view name) {
    throw_if_error(
        RegDeleteKeyValueW(k, get_zwstr_ptr(sub_key), get_zwstr_ptr(name)),
        "RegDeleteKeyValueW");
}

void fns::delete_subkeys(HKEY k, wzstring_view sub_key) {
    const wchar_t* ptr = sub_key.empty() ? nullptr : sub_key.data();
    throw_if_error(RegDeleteTreeW(k, ptr), "RegDeleteTreeW");
}

uint32_t fns::get_dword_value(HKEY k, wzstring_view name, value_type& type,
                              bool allow_conversion) {
    DWORD val = 0, val_size = sizeof(DWORD);
    const DWORD any_type_flag = allow_conversion ? RRF_RT_ANY : 0;
    DWORD val_type = 0;
    throw_if_error(RegGetValueW(k, nullptr, get_zwstr_ptr(name),
                                RRF_RT_DWORD | any_type_flag, &val_type, &val,
                                &val_size),
                   "RegGetValueW");
    type = static_cast<value_type>(val_type);
    return val;
}

uint64_t fns::get_qword_value(HKEY k, wzstring_view name, value_type& type,
                              bool allow_conversion) {
    uint64_t val = 0;
    DWORD val_size = sizeof(uint64_t);
    const DWORD any_type_flag = allow_conversion ? RRF_RT_ANY : 0;
    DWORD val_type = 0;
    throw_if_error(RegGetValueW(k, nullptr, get_zwstr_ptr(name),
                                RRF_RT_QWORD | any_type_flag, &val_type, &val,
                                &val_size),
                   "RegGetValueW");
    type = static_cast<value_type>(val_type);
    return val;
}

void fns::get_binary_none_value(HKEY k, wzstring_view name, dynamic_buffer buff,
                                bool none, value_type& type,
                                bool allow_conversion) {
    DWORD dw_flags = none ? RRF_RT_REG_NONE : RRF_RT_REG_BINARY;
    dw_flags |= allow_conversion ? RRF_RT_ANY : 0;
    DWORD buff_size = 0;
    DWORD val_type = 0;
    throw_if_error(RegGetValueW(k, nullptr, get_zwstr_ptr(name), dw_flags,
                                nullptr, nullptr, &buff_size),
                   "RegGetValueW");

    auto old_size = buff.size();
    buff.resize(old_size + buff_size);

    throw_if_error(RegGetValueW(k, nullptr, get_zwstr_ptr(name), dw_flags,
                                &val_type, buff.data_as<uint8_t>() + old_size,
                                &buff_size),
                   "RegGetValueW");
    type = static_cast<value_type>(val_type);
}

void fns::get_string_value(HKEY k, wzstring_view name, std::wstring& str,
                           value_type& type, bool allow_conversion) {
    DWORD str_size = 0; // size in bytes
    const DWORD any_type_flag = allow_conversion ? RRF_RT_ANY : 0;
    DWORD val_type = 0;
    throw_if_error(RegGetValueW(k, nullptr, get_zwstr_ptr(name),
                                RRF_RT_REG_SZ | any_type_flag, nullptr, nullptr,
                                &str_size),
                   "RegGetValueW");

    auto old_size = str.size();
    str.resize(old_size + str_size / sizeof(wchar_t));

    throw_if_error(RegGetValueW(k, nullptr, get_zwstr_ptr(name),
                                RRF_RT_REG_SZ | any_type_flag, &val_type,
                                str.data() + old_size, &str_size),
                   "RegGetValueW");
    type = static_cast<value_type>(val_type);
    // remove the last null character
    while (!str.empty() && str.back() == L'\0') {
        str.pop_back();
    }
}

void fns::get_expand_string_value(HKEY k, wzstring_view name, std::wstring& str,
                                  value_type& type, bool expand,
                                  bool allow_conversion) {
    DWORD dw_flags = RRF_RT_REG_EXPAND_SZ;
    dw_flags |= expand ? RRF_NOEXPAND : 0;
    dw_flags |= allow_conversion ? RRF_RT_ANY : 0;
    DWORD val_type = 0;
    DWORD str_size = 0; // size in bytes

    throw_if_error(RegGetValueW(k, nullptr, get_zwstr_ptr(name), dw_flags,
                                nullptr, nullptr, &str_size),
                   "RegGetValueW");

    auto old_size = str.size();
    str.resize(old_size + str_size / sizeof(wchar_t));

    throw_if_error(RegGetValueW(k, nullptr, get_zwstr_ptr(name), dw_flags,
                                &val_type, str.data() + old_size, &str_size),
                   "RegGetValueW");
    type = static_cast<value_type>(val_type);
    str.pop_back();
}

void fns::get_multi_string_value(HKEY k, wzstring_view name,
                                 std::vector<std::wstring>& strs,
                                 value_type& type, bool allow_conversion) {
    std::vector<uint8_t> strs_buffer;
    DWORD strs_size = 0;
    const DWORD any_type_flag = allow_conversion ? RRF_RT_ANY : 0;
    DWORD val_type = 0;

    throw_if_error(RegGetValueW(k, nullptr, get_zwstr_ptr(name),
                                RRF_RT_REG_MULTI_SZ | any_type_flag, nullptr,
                                nullptr, &strs_size),
                   "RegGetValueW");

    strs_buffer.resize(strs_size);

    throw_if_error(RegGetValueW(k, nullptr, get_zwstr_ptr(name),
                                RRF_RT_REG_MULTI_SZ | any_type_flag, &val_type,
                                strs_buffer.data(), &strs_size),
                   "RegGetValueW");
    type = static_cast<value_type>(val_type);
    auto strs_view = buffer(strs_buffer).to_string_view<wchar_t>();
    constexpr auto two_nulls = std::wstring_view(L"\0\0", 2);
    constexpr auto one_null = std::wstring_view(L"\0", 1);

    if (strs_view.ends_with(two_nulls)) {
        if (strs_view.size() == two_nulls.size()) {
            return;
        }
        strs_view.remove_suffix(two_nulls.size());
    }

    for (auto str : strs_view | split(one_null)) {
        strs.emplace_back(str);
    }
}

void fns::save_key_to_file(HKEY k, wzstring_view file_path,
                           save_format format) {
    const LSTATUS result = ::RegSaveKeyExW(k, get_zwstr_ptr(file_path), nullptr,
                                           static_cast<DWORD>(format));
    throw_if_error(result, "RegSaveKeyExW");
}

HKEY fns::open_current_user(access a) {
    HKEY new_key = nullptr;
    const LSTATUS result =
        ::RegOpenCurrentUser(static_cast<REGSAM>(a), &new_key);
    throw_if_error(result, "RegOpenCurrentUser");
    return new_key;
}