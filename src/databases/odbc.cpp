#include <rad/databases/odbc.h>
#include <rad/views/enumerate.h>
#ifdef _WIN32
#include <Windows.h>
#endif // _WIN32
#include <sql.h>
#include <sqlext.h>

#include <clocale>
#include <ctime>

using namespace RAD_LIB_NAMESPACE;
using namespace odbc;

static_assert(std::is_same_v<SQLHANDLE, void*>);
static_assert(std::is_same_v<SQLHANDLE, SQLHENV>);
static_assert(std::is_same_v<SQLHANDLE, SQLHDBC>);
static_assert(std::is_same_v<SQLHANDLE, SQLHSTMT>);
static_assert(sizeof(wide_char_t) == sizeof(SQLWCHAR));

namespace {
    bool is_sql_error(SQLRETURN ret) {
        return ret != SQL_SUCCESS && ret != SQL_SUCCESS_WITH_INFO;
    }

    void terminate_if_sql_error(SQLRETURN ret) {
        if (is_sql_error(ret)) {
            std::terminate();
        }
    }

    /*
    const SQLWCHAR* cast_sqlwchar(const wide_char_t* p) {
            return reinterpret_cast<const SQLWCHAR*>(p);
    }
    */

    SQLWCHAR* cast_sqlwchar(wide_char_t* p) {
        return reinterpret_cast<SQLWCHAR*>(p);
    }

    SQLWCHAR* const_cast_sqlwchar(const wide_char_t* p) {
        return reinterpret_cast<SQLWCHAR*>(const_cast<wide_char_t*>(p));
    }

    struct sql_error_record {
        wide_string msg;
        wide_string state;
        long native_error = 0;
    };

    std::optional<sql_error_record> get_error_record(SQLSMALLINT handle_type,
                                                     SQLHANDLE handle,
                                                     int16_t record_index) {
        wide_string msg;
        msg.resize(4 * 1024);
        wide_string state;
        state.resize(6);
        while (1) {
            SQLINTEGER native_error = 0;
            SQLSMALLINT required_text_len = 0;
            SQLRETURN ret = ::SQLGetDiagRecW(
                handle_type, handle, record_index, cast_sqlwchar(state.data()),
                &native_error, cast_sqlwchar(msg.data()),
                as<int16_t>(msg.size()), &required_text_len);

            if (ret == SQL_SUCCESS) {
                msg.resize(required_text_len);
                sql_error_record error_record;
                error_record.msg = std::move(msg);
                error_record.state = std::move(state);
                error_record.native_error = native_error;
                return error_record;
            }
            else if (ret == SQL_SUCCESS_WITH_INFO) {
                msg.resize(required_text_len);
                continue;
            }
            else {
                return {};
            }
        }
    }

    struct sql_error_data {
        wide_string msg;
        wide_string state;
        long native_error = 0;
    };

    sql_error_data get_sql_error(SQLSMALLINT handle_type, SQLHANDLE handle) {
        sql_error_data data;
        int16_t record_index = 0;
        while (1) {
            auto record = get_error_record(handle_type, handle, ++record_index);
            if (!record) {
                break;
            }
            if (!data.msg.empty() && data.msg.back() != '\n') {
                data.msg += '\n';
            }
            data.msg += record->msg;
        }
        return data;
    }

    template <class T>
    constexpr bool is_i16 = std::is_same_v<T, int16_t>;
    template <class T>
    constexpr bool is_u16 = std::is_same_v<T, uint16_t>;
    template <class T>
    constexpr bool is_i32 =
        std::is_integral_v<T> && std::is_signed_v<T> &&
        !std::is_same_v<T, wchar_t> && sizeof(T) == sizeof(int32_t);
    template <class T>
    constexpr bool is_u32 =
        std::is_integral_v<T> && std::is_unsigned_v<T> &&
        !std::is_same_v<T, wchar_t> && sizeof(T) == sizeof(uint32_t);
    template <class T>
    constexpr bool is_i64 = std::is_integral_v<T> && std::is_signed_v<T> &&
                            sizeof(T) == sizeof(int64_t);
    template <class T>
    constexpr bool is_u64 = std::is_integral_v<T> && std::is_unsigned_v<T> &&
                            sizeof(T) == sizeof(uint64_t);

    template <class T>
    constexpr SQLSMALLINT get_c_type() {
        if constexpr (std::is_same_v<T, std::string_view> ||
                      std::is_same_v<T, std::string>) {
            return SQL_C_CHAR;
        }
        else if constexpr (std::is_same_v<T, std::wstring_view> ||
                           std::is_same_v<T, std::wstring>) {
            return SQL_C_WCHAR;
        }
        else if constexpr (is_i16<T>) {
            return SQL_C_SSHORT;
        }
        else if constexpr (is_u16<T>) {
            return SQL_C_USHORT;
        }
        else if constexpr (is_i32<T>) {
            return SQL_C_SLONG;
        }
        else if constexpr (is_u32<T>) {
            return SQL_C_ULONG;
        }
        else if constexpr (is_i64<T>) {
            return SQL_C_SBIGINT;
        }
        else if constexpr (is_u64<T>) {
            return SQL_C_UBIGINT;
        }
        else if constexpr (std::is_same_v<T, float>) {
            return SQL_C_FLOAT;
        }
        else if constexpr (std::is_same_v<T, double>) {
            return SQL_C_DOUBLE;
        }
    }

    template <class T>
    const void* get_in_ptr(const T& val) {
        if constexpr (std::is_same_v<T, std::string_view> ||
                      std::is_same_v<T, std::string>) {
            return val.data();
        }
        else if constexpr (std::is_same_v<T, std::wstring_view> ||
                           std::is_same_v<T, std::wstring>) {
            return val.data();
        }
        else {
            return &val;
        }
    }

    template <class T>
    int64_t get_val_len(const T& val) {
        if constexpr (std::is_same_v<T, std::string_view> ||
                      std::is_same_v<T, std::string>) {
            return static_cast<int64_t>(val.size());
        }
        else if constexpr (std::is_same_v<T, std::wstring_view> ||
                           std::is_same_v<T, std::wstring>) {
            return static_cast<int64_t>(val.size());
        }
        else {
            return 0;
        }
    }

    struct SQLTypeDeduction {
        short c_type = 0;
        size_t len = 0;
        bool is_variable_len = false;
    };

    SQLTypeDeduction sql_type_to_c_type_size(short t, size_t n) {
        constexpr std::size_t max_variable_blob_size = 4096;

        if (n == 0) {
            n = std::numeric_limits<size_t>::max();
        }
        SQLTypeDeduction deduced_type;
        if (t == SQL_BIT || t == SQL_TINYINT || t == SQL_SMALLINT ||
            t == SQL_BIGINT || t == SQL_INTEGER) {
            deduced_type.c_type = SQL_C_SBIGINT;
            deduced_type.len = sizeof(int64_t);
        }
        else if (t == SQL_DOUBLE || t == SQL_FLOAT || t == SQL_REAL ||
                 t == SQL_DECIMAL || t == SQL_NUMERIC) {
            deduced_type.c_type = SQL_C_DOUBLE;
            deduced_type.len = sizeof(double);
        }
        else if (t == SQL_CHAR) {
            n += sizeof(SQLCHAR);
            deduced_type.c_type = SQL_C_CHAR;
            deduced_type.len = n;
        }
        else if (t == SQL_WCHAR) {
            n += 1;
            deduced_type.c_type = SQL_C_WCHAR;
            deduced_type.len = n * sizeof(SQLWCHAR);
        }
        else if (t == SQL_VARCHAR || t == SQL_LONGVARCHAR) {
            n += sizeof(SQLCHAR);
            deduced_type.c_type = SQL_C_CHAR;
            deduced_type.len = std::min(size_t{n}, max_variable_blob_size);
            deduced_type.is_variable_len = deduced_type.len <= n;
        }
        else if (t == SQL_WVARCHAR || t == SQL_WLONGVARCHAR) {
            n += 1;
            deduced_type.c_type = SQL_C_WCHAR;
            deduced_type.len =
                std::min(size_t{n}, max_variable_blob_size) * sizeof(SQLWCHAR);
            deduced_type.is_variable_len =
                deduced_type.len <= n * sizeof(SQLWCHAR);
        }
        else if ((t == SQL_BINARY && n <= 4096) ||
                 (t == SQL_VARBINARY && n <= 4096)) {
            deduced_type.c_type = SQL_C_BINARY;
            deduced_type.len = n;
        }
        else if (t == SQL_BINARY || t == SQL_VARBINARY ||
                 t == SQL_LONGVARBINARY) {
            deduced_type.c_type = SQL_C_BINARY;
            deduced_type.len = std::min(size_t{n}, max_variable_blob_size);
            deduced_type.is_variable_len = deduced_type.len < n;
        }
        else if (t == SQL_TYPE_DATE || t == SQL_DATE) {
            deduced_type.c_type = SQL_C_TYPE_DATE;
            deduced_type.len = sizeof(SQL_DATE_STRUCT);

            static_assert(sizeof(date_t) == sizeof(SQL_DATE_STRUCT),
                          "sizeof(date_t) != sizeof(SQL_DATE_STRUCT)");
        }
        else if (t == SQL_TYPE_TIME || t == SQL_TIME) {
            deduced_type.c_type = SQL_C_TYPE_TIME;
            deduced_type.len = sizeof(SQL_TIME_STRUCT);

            static_assert(sizeof(odbc::time_t) == sizeof(SQL_TIME_STRUCT),
                          "sizeof(time_t) != sizeof(SQL_TIME_STRUCT)");
        }
        else if (t == SQL_TYPE_TIMESTAMP || t == SQL_TIMESTAMP) {
            deduced_type.c_type = SQL_C_TYPE_TIMESTAMP;
            deduced_type.len = sizeof(SQL_TIMESTAMP_STRUCT);

            static_assert(sizeof(timestamp_t) == sizeof(SQL_TIMESTAMP_STRUCT),
                          "sizeof(timestamp_t) != "
                          "sizeof(SQL_TIMESTAMP_STRUCT)");
        }
        else if (t == SQL_GUID) {
            deduced_type.c_type = SQL_C_GUID;
            deduced_type.len = sizeof(SQLGUID);

            static_assert(sizeof(guid_t) == sizeof(SQLGUID),
                          "sizeof(guid_t) != sizeof(SQLGUID)");
        }
        else {
            deduced_type.c_type = SQL_C_CHAR;
            deduced_type.len = 256;
            deduced_type.is_variable_len = true;
        }

        return deduced_type;
    }

    bool is_blob_c_type(short t) {
        return t == SQL_C_CHAR || t == SQL_C_WCHAR || t == SQL_C_BINARY;
    }

    short sql_type_to_c_type(short t) {
        if (t == SQL_BIT || t == SQL_TINYINT || t == SQL_SMALLINT ||
            t == SQL_BIGINT) {
        }

        if (t == SQL_NUMERIC) {
            return SQL_C_NUMERIC;
        }
        if (t == SQL_WCHAR || t == SQL_WVARCHAR || t == SQL_WLONGVARCHAR) {
            return SQL_C_WCHAR;
        }
        if (t == SQL_CHAR || t == SQL_VARCHAR || t == SQL_DECIMAL ||
            t == SQL_NUMERIC) {
            return SQL_C_CHAR;
        }
        if (t == SQL_INTEGER) {
            return SQL_C_LONG;
        }
        if (t == SQL_SMALLINT) {
            return SQL_C_SHORT;
        }
        if (t == SQL_DOUBLE) {
            return SQL_C_DOUBLE;
        }
        if (t == SQL_REAL) {
            return SQL_C_FLOAT;
        }
        return t;
    }
} // namespace

void odbc::detail::env_handle_deleter::operator()(void* ptr) const noexcept {
    SQLRETURN ret = ::SQLFreeHandle(SQL_HANDLE_ENV, ptr);
    terminate_if_sql_error(ret);
}

void odbc::detail::db_handle_deleter::operator()(void* ptr) const noexcept {
    if (connected) {
        SQLRETURN ret = ::SQLDisconnect(ptr);
        terminate_if_sql_error(ret);
    }
    SQLRETURN ret = ::SQLFreeHandle(SQL_HANDLE_DBC, ptr);
    terminate_if_sql_error(ret);
}

void odbc::detail::stmt_handle_deleter::operator()(void* ptr) const noexcept {
    SQLRETURN ret = ::SQLFreeHandle(SQL_HANDLE_STMT, ptr);
    terminate_if_sql_error(ret);
}

void odbc::set_connection_pooling(connection_pooling pooling) {
    const SQLINTEGER value = static_cast<SQLINTEGER>(pooling);
    SQLRETURN ret = ::SQLSetEnvAttr(nullptr, SQL_ATTR_CONNECTION_POOLING,
                                    reinterpret_cast<SQLPOINTER>(value), 0);
    if (!SQL_SUCCEEDED(ret)) {
        throw std::runtime_error{
            "SQLSetEnvAttr failed to set connection pooling"};
    }
}

environment::environment() {
    SQLHANDLE handle = nullptr;
    SQLRETURN ret = ::SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &handle);
    if (!SQL_SUCCEEDED(ret)) {
        throw std::runtime_error{
            "SQLAllocHandle failed to allocate environment handle"};
    }
    handle_.reset(handle);
}

void environment::list_sources(std::vector<data_source>& sources, bool user,
                               bool system) const {
    uint16_t first_fetch = 0;
    constexpr uint16_t next_fetch = SQL_FETCH_NEXT;
    if (user && system) {
        first_fetch = SQL_FETCH_FIRST;
    }
    else if (user) {
        first_fetch = SQL_FETCH_FIRST_USER;
    }
    else if (system) {
        first_fetch = SQL_FETCH_FIRST_SYSTEM;
    }
    wide_string name, description;

    // returns true if there is more sources
    auto get_source = [&](uint16_t direction) -> bool {
        name.resize((SQL_MAX_DSN_LENGTH + 1) * 2);
        description.resize((SQL_MAX_DSN_LENGTH + 1) * 2);
        short name_len = 0, desc_len = 0;
        SQLRETURN ret = ::SQLDataSourcesW(
            handle_.get(), direction, cast_sqlwchar(name.data()),
            as<short>(name.size()), &name_len,
            cast_sqlwchar(description.data()), as<short>(description.size()),
            &desc_len);
        if (ret == SQL_NO_DATA) {
            return false;
        }
        else if (SQL_SUCCEEDED(ret)) {
            name.resize(name_len);
            description.resize(desc_len);
            while (!name.empty() && name.back() == L'\0') {
                name.pop_back();
            }
            while (!description.empty() && description.back() == L'\0') {
                description.pop_back();
            }
            sources.emplace_back(name, description);
            return true;
        }
        else {
            throw_if_error(ret, "SQLDataSourcesW");
            return false;
        }
    };

    if (!get_source(first_fetch)) {
        return;
    }
    while (get_source(next_fetch)) {
    }
}

void environment::set_connection_pooling(connection_pooling pooling) {
    const SQLINTEGER attr = SQL_ATTR_CONNECTION_POOLING;
    uintptr_t value = static_cast<uintptr_t>(pooling);
    SQLRETURN ret = ::SQLSetEnvAttr(handle_.get(), attr, (SQLPOINTER)value, 0);
    throw_if_error(ret, "SQLSetEnvAttr");
}

void environment::set_connection_pooling_match(
    connection_pooling_match pooling_match) {
    const SQLINTEGER attr = SQL_ATTR_CP_MATCH;
    uintptr_t value = static_cast<uintptr_t>(pooling_match);
    SQLRETURN ret = ::SQLSetEnvAttr(handle_.get(), attr, (SQLPOINTER)value, 0);
    throw_if_error(ret, "SQLSetEnvAttr");
}

void environment::set_version(environment_version v) {
    const SQLINTEGER attr = SQL_ATTR_ODBC_VERSION;
    uintptr_t value = static_cast<uintptr_t>(v);
    SQLRETURN ret = ::SQLSetEnvAttr(handle_.get(), attr, (SQLPOINTER)value, 0);
    throw_if_error(ret, "SQLSetEnvAttr");
}

void environment::set_driver_output_string(driver_output_string s) {
    const SQLINTEGER attr = SQL_ATTR_OUTPUT_NTS;
    uintptr_t value = static_cast<uintptr_t>(s);
    SQLRETURN ret = ::SQLSetEnvAttr(handle_.get(), attr, (SQLPOINTER)value, 0);
    throw_if_error(ret, "SQLSetEnvAttr");
}

void environment::throw_if_error(short ret, const char* msg) const {
    if (ret != SQL_SUCCESS && ret != SQL_SUCCESS_WITH_INFO) {
        auto error_data = get_sql_error(SQL_HANDLE_ENV, handle_.get());
        throw std::runtime_error(std::string{msg} + " : " +
                                 to_string(error_data.msg));
    }
    else {
        get_sql_error(SQL_HANDLE_ENV, handle_.get());
    }
}

void database::init(environment& env) {
    SQLHANDLE handle = nullptr;
    SQLRETURN ret =
        ::SQLAllocHandle(SQL_HANDLE_DBC, env.native_handle().get(), &handle);
    throw_if_error(ret, "SQLAllocHandle");
    handle_.reset(handle);
}

void database::connect(wide_string_view conn_str) {
    SQLRETURN ret = ::SQLDriverConnectW(
        handle_.get(), nullptr, const_cast_sqlwchar(conn_str.data()),
        static_cast<SQLSMALLINT>(conn_str.size()), nullptr, 0, nullptr, 0);
    throw_if_error(ret, "SQLDriverConnectW");
    handle_.get_deleter().connected = true;

    SQLUINTEGER result = 0;
    SQLSMALLINT res_size = sizeof(result);
    ret = ::SQLGetInfoW(handle_.get(), SQL_GETDATA_EXTENSIONS, &result,
                        sizeof(result), &res_size);
    std::ignore = ret;
    get_data_exts_ = result;
}

void database::disconnect() {
    SQLRETURN ret = ::SQLDisconnect(handle_.get());
    throw_if_error(ret, "SQLDisconnect");
}

statement database::prepare(wide_string_view stmt) {
    SQLHANDLE handle = nullptr;
    SQLRETURN ret = ::SQLAllocHandle(SQL_HANDLE_STMT, handle_.get(), &handle);
    throw_if_error(ret, "SQLAllocHandle");
    statement::native_handle_type wrapped_handle{handle};
    ret = ::SQLPrepareW(wrapped_handle.get(), const_cast_sqlwchar(stmt.data()),
                        as<SQLINTEGER>(stmt.size()));
    throw_if_error(ret, "SQLPrepareW");
    statement s{std::move(wrapped_handle), get_data_exts_};
    return s;
}

uint32_t database::get_data_extensions() const {
    if (!handle_) {
        return 0;
    }
    SQLUINTEGER result = 0;
    SQLSMALLINT res_size = sizeof(result);
    SQLRETURN ret = ::SQLGetInfoW(handle_.get(), SQL_GETDATA_EXTENSIONS,
                                  &result, sizeof(result), &res_size);
    throw_if_error(ret, "SQLGetInfoW");
    return result;
}

void database::throw_if_error(short ret, const char* msg) const {
    if (ret != SQL_SUCCESS && ret != SQL_SUCCESS_WITH_INFO) {
        auto error_data = get_sql_error(SQL_HANDLE_DBC, handle_.get());
        throw std::runtime_error(std::string{msg} + ": " +
                                 to_string(error_data.msg));
    }
    else {
        get_sql_error(SQL_HANDLE_DBC, handle_.get());
    }
}

void statement::bound_column::clear() noexcept {
    flags &= ~(bound_column_flags::result | bound_column_flags::null |
               bound_column_flags::converted_to_string |
               bound_column_flags::converted_to_wstring);
    get_data_buff.clear();
    converted_string.clear();
    converted_wstring.clear();
}

std::span<const uint8_t> statement::bound_column::get_blob_data() const {
    assert(has_result() && is_blob_c_type(c_type));
    const uint8_t* data_ptr =
        get_data_buff.empty() ? blob.data() : get_data_buff.data();
    std::size_t data_size = get_data_buff.empty()
                                ? std::min((size_t)blob_size, blob.size())
                                : get_data_buff.size();
    if (data_size == 0) {
        return {};
    }
    return std::span{data_ptr, data_size};
}

std::string_view statement::bound_column::get_string_view_data() const {
    assert(has_result() && c_type == SQL_C_CHAR);
    auto blob_data = get_blob_data();
    if (blob_data.empty()) {
        return {};
    }
    auto str = std::string_view{reinterpret_cast<const char*>(blob_data.data()),
                                blob_data.size()};
    if (!str.empty() && str.back() == '\0') {
        str.remove_suffix(1);
    }
    return str;
}

wide_string_view statement::bound_column::get_wstring_view_data() const {
    assert(has_result() && c_type == SQL_C_WCHAR);
    auto blob_data = get_blob_data();
    if (blob_data.empty()) {
        return {};
    }
    std::size_t str_len = blob_data.size() / sizeof(wide_char_t);
    auto str = wide_string_view{
        reinterpret_cast<const wide_char_t*>(blob_data.data()), str_len};
    if (!str.empty() && str.back() == 0) {
        str.remove_suffix(1);
    }
    return str;
}

void statement::bound_column::convert_to_string() {
    assert(has_result() && !has_string() && c_type != SQL_C_CHAR &&
           converted_string.empty());

    struct restore_time_locale_t : noncopyable {
        std::string locale_str;

        restore_time_locale_t(std::string&& l) : locale_str{std::move(l)} {
        }

        ~restore_time_locale_t() {
            if (!locale_str.empty()) {
                std::setlocale(LC_TIME, locale_str.c_str());
            }
        }
    };

    auto override_time_locale = [] {
        const char* old_locale = std::setlocale(LC_TIME, nullptr);
        std::string old_locale_str;
        if (old_locale != nullptr) {
            old_locale_str = old_locale;
        }
        std::setlocale(LC_TIME, "");
        return restore_time_locale_t{std::move(old_locale_str)};
    };

    if (c_type == SQL_C_WCHAR) {
        auto data = get_wstring_view_data();
        to_string(data, converted_string);
    }
    else if (c_type == SQL_C_SBIGINT) {
        converted_string = std::to_string(i64_val);
    }
    else if (c_type == SQL_C_DOUBLE) {
        converted_string = std::to_string(dval);
    }
    else if (c_type == SQL_C_TYPE_TIME || c_type == SQL_C_TIME) {
        auto save_restore = override_time_locale();
        std::tm t{};
        t.tm_hour = time_val.hour;
        t.tm_min = time_val.minute;
        t.tm_sec = time_val.second;
        char time_buff[256];
        std::size_t n =
            std::strftime(time_buff, sizeof(time_buff), "%H:%M:%S", &t);
        if (n > 0) {
            converted_string.append(time_buff, n);
        }
    }
    else if (c_type == SQL_C_TYPE_DATE || c_type == SQL_C_DATE) {
        auto save_restore = override_time_locale();
        std::tm t{};
        t.tm_year = date_val.year - 1900;
        t.tm_mon = date_val.month - 1;
        t.tm_mday = date_val.day;
        char time_buff[256];
        std::size_t n =
            std::strftime(time_buff, sizeof(time_buff), "%Y-%m-%d", &t);
        if (n > 0) {
            converted_string.append(time_buff, n);
        }
    }
    else if (c_type == SQL_C_TYPE_TIMESTAMP || c_type == SQL_C_TIMESTAMP) {
        auto save_restore = override_time_locale();
        std::tm t{};
        t.tm_hour = timestamp_val.hour;
        t.tm_min = timestamp_val.minute;
        t.tm_sec = timestamp_val.second;

        t.tm_year = timestamp_val.year - 1900;
        t.tm_mon = timestamp_val.month - 1;
        t.tm_mday = timestamp_val.day;

        char time_buff[512];
        std::size_t n = std::strftime(time_buff, sizeof(time_buff),
                                      "%Y-%m-%d %H:%M:%S %z", &t);
        if (n > 0) {
            converted_string.append(time_buff, n);
        }
    }
    else {
        throw std::system_error{
            std::make_error_code(std::errc::wrong_protocol_type)};
    }

    flags |= bound_column_flags::converted_to_string;
}

void statement::bound_column::convert_to_wstring() {
    assert(has_result() && !has_wstring() && c_type != SQL_C_WCHAR &&
           converted_wstring.empty());

    if (c_type == SQL_C_CHAR) {
        auto data = get_string_view_data();
        to_wide_string(data, converted_wstring);
    }
    /*
    else if (c_type == SQL_C_SBIGINT) {
            converted_wstring = std::to_wstring(i64_val);
    }
    else if (c_type == SQL_C_DOUBLE) {
            converted_wstring = std::to_wstring(dval);
    }
    */
    else {
        convert_to_string();
        to_wide_string(converted_string, converted_wstring);
    }

    flags |= bound_column_flags::converted_to_wstring;
}

statement::statement(native_handle_type handle, uint32_t get_data_exts)
    : handle_{std::move(handle)}, get_data_exts_{get_data_exts} {
    if (!handle_) {
        return;
    }
    get_params_descriptions();
}

auto statement::describe_param(uint16_t index) -> bound_param {
    bound_param bp;
    bp.index = index;
    SQLSMALLINT nullable = 0;
    SQLULEN param_size = 0;
    SQLRETURN ret = ::SQLDescribeParam(handle_.get(), index + 1, &bp.sql_type,
                                       &param_size, &bp.scale, &nullable);
    throw_if_error(ret, "SQLDescribeParam");
    bp.sql_size = param_size;
    return bp;
}

auto statement::describe_column(uint16_t index0) -> result_column {
    result_column col;
    col.index = index0;
    short is_nullable = 0;
    col.name.resize(255);
    SQLRETURN ret = 0;
    do {
        short actual_name_len = 0;
        short name_len = static_cast<SQLSMALLINT>(col.name.size());
        SQLULEN sql_size = 0;
        ret = ::SQLDescribeColW(handle_.get(), index0 + 1,
                                cast_sqlwchar(col.name.data()), name_len,
                                &actual_name_len, &col.sql_type, &sql_size,
                                &col.scale, &is_nullable);
        col.sql_size = static_cast<std::size_t>(sql_size);
        const bool name_truncated = actual_name_len > name_len;
        col.name.resize(actual_name_len);
        if (ret != SQL_SUCCESS_WITH_INFO || !name_truncated) {
            break;
        }
    } while (1);

    throw_if_error(ret, "SQLDescribeColW");

    return col;
}

void statement::get_columns_descriptions() {
    size_t res_cols = columns_count();
    result_cols_.resize(res_cols);
    if (res_cols == 0) {
        return;
    }
    for (auto i : range(res_cols)) {
        auto& col = result_cols_[i];
        col = describe_column(static_cast<uint16_t>(i));
        col.index = static_cast<uint16_t>(i);
    }
}

void statement::bind_result_columns() {
    cols_.resize(result_cols_.size());
    if (result_cols_.empty()) {
        return;
    }

    for (auto i : range(result_cols_.size())) {
        auto& col = cols_[i];
        col.index = static_cast<uint16_t>(i);

        auto deduced_type = sql_type_to_c_type_size(result_cols_[i].sql_type,
                                                    result_cols_[i].sql_size);
        col.c_type = deduced_type.c_type;
        col.c_len = deduced_type.len;
        if (deduced_type.is_variable_len) {
            col.set_variable_len();
        }

        if (deduced_type.is_variable_len &&
            !(get_data_exts_ & SQL_GD_ANY_COLUMN)) {
            // don't continue binding as SQLGetData will only work
            // for columns after bound
            break;
        }

        if (deduced_type.is_variable_len && !(get_data_exts_ & SQL_GD_BOUND)) {
            // don't bind this column as SQLGetData only work for
            // unbound columns
            continue;
        }

        uint8_t* data_ptr = col.data_bytes;

        if (deduced_type.is_variable_len ||
            col.c_len > sizeof(col.data_bytes)) {
            col.blob.resize(col.c_len);
            data_ptr = col.blob.data();
        }

        static_assert(sizeof(col.blob_size) == sizeof(SQLLEN));
        SQLRETURN ret =
            ::SQLBindCol(handle_.get(), static_cast<uint16_t>(i + 1),
                         col.c_type, data_ptr, static_cast<SQLLEN>(col.c_len),
                         reinterpret_cast<SQLLEN*>(&col.blob_size));

        throw_if_error(ret, "SQLBindCol");

        col.set_bound();
    }
}

void statement::get_results_until(size_t i) {
    if ((get_data_exts_ & SQL_GD_ANY_ORDER) == SQL_GD_ANY_ORDER) {
        // don't need to fetch all previous columns
        auto& col = cols_.at(i);
        if (col.has_result()) {
            return;
        }
        return get_column_result(col);
    }
    for (auto& col : cols_) {
        if (col.index > i) {
            break;
        }
        if (col.has_result()) {
            continue;
        }
        get_column_result(col);
    }
}

void statement::get_column_result(bound_column& col) {
    assert(!col.has_result());
    if (col.has_result()) {
        return;
    }
    if (col.is_variable_len() || is_blob_c_type(col.c_type)) {
        col.get_data_buff.clear();
        if (col.blob_size > 0) {
            size_t init_size = col.blob_size;
            if (col.c_type == SQL_C_CHAR) {
                init_size += 1;
            }
            else if (col.c_type == SQL_C_WCHAR) {
                init_size += sizeof(SQLWCHAR);
            }
            col.get_data_buff.resize(init_size);
        }
        bool is_null =
            get_blob_result(col.index + 1, col.c_type, col.get_data_buff);
        if (is_null) {
            col.set_null();
        }
        col.set_has_result();
    }
    else {
        SQLLEN indicator = 0;
        SQLRETURN ret = ::SQLGetData(
            handle_.get(), static_cast<SQLUSMALLINT>(col.index + 1), col.c_type,
            col.data_bytes, sizeof(col.data_bytes), &indicator);
        throw_if_error(ret, "SQLGetData");
        if (indicator == SQL_NULL_DATA) {
            col.set_null();
        }
        col.set_has_result();
    }
}

bool statement::get_blob_result(short index1, short c_type,
                                std::vector<uint8_t>& blob) {
    const size_t blob_increase_step = 1024;
    if (blob.empty()) {
        blob.resize(blob_increase_step);
    }
    SQLLEN offset = 0;
    while (1) {
        SQLLEN len_or_indicator = 0;
        SQLLEN buff_size = static_cast<SQLLEN>(blob.size()) - offset;
        SQLRETURN ret =
            ::SQLGetData(handle_.get(), index1, c_type, blob.data() + offset,
                         buff_size, &len_or_indicator);
        throw_if_error(ret, "SQLGetData");

        // the column contained null
        if (len_or_indicator == SQL_NULL_DATA) {
            blob.clear();
            return false;
        }
        // fetched all the data
        if (len_or_indicator > 0 && len_or_indicator < buff_size) {
            blob.resize(blob.size() - (buff_size - len_or_indicator));
            break;
        }
        // fetched buff_size data except the last charachter in case of
        // string
        else if (len_or_indicator == SQL_NO_TOTAL ||
                 len_or_indicator >= buff_size) {
            // driver has truncated the last charachter and replaced
            // it with null terminator
            SQLLEN null_size = c_type == SQL_C_CHAR    ? sizeof(char)
                               : c_type == SQL_C_WCHAR ? sizeof(wchar_t)
                                                       : 0;
            buff_size -= null_size;
            offset += buff_size;
            // if it was binary data and no data is remaining then
            // break
            if (null_size == 0 && len_or_indicator == buff_size) {
                break;
            }
            // increase the buffer size
            if (len_or_indicator != SQL_NO_TOTAL) {
                blob.resize(blob.size() + (len_or_indicator - buff_size));
            }
            else {
                blob.resize(blob.size() + blob_increase_step);
            }
        }

        if (ret != SQL_SUCCESS_WITH_INFO) {
            break;
        }
    }

    return false;
}

void statement::get_params_descriptions() {
    bound_params_.clear();
    size_t params_n = bind_params_count();
    bound_params_.resize(params_n);

    for (auto&& [i, p] : enumerate(bound_params_)) {
        p.index = static_cast<uint16_t>(i);
        continue;
        p = describe_param(static_cast<uint16_t>(i));
    }
}

void statement::bind_int32(size_t i, int32_t val) {
    auto& param = bound_params_.at(i);
    param.clear();
    param.c_type = SQL_C_LONG;
    param.sql_type = SQL_INTEGER;
    param.size = sizeof(int32_t);
    param.i32_val = val;
    bind_param(param, &param.i32_val);
}

void statement::bind_uint32(size_t i, uint32_t val) {
    auto& param = bound_params_.at(i);
    param.clear();
    param.c_type = SQL_C_ULONG;
    param.sql_type = SQL_INTEGER;
    param.size = sizeof(uint32_t);
    param.u32_val = val;
    bind_param(param, &param.u32_val);
}

void statement::bind_int64(size_t i, int64_t val) {
    auto& param = bound_params_.at(i);
    param.clear();
    if (val > std::numeric_limits<int32_t>::max() ||
        val < std::numeric_limits<int32_t>::min()) {
        param.c_type = SQL_C_SBIGINT;
        param.sql_type = SQL_BIGINT;
    }
    else {
        param.c_type = SQL_C_LONG;
        param.sql_type = SQL_INTEGER;
    }
    param.size = sizeof(int64_t);
    param.i64_val = val;
    bind_param(param, &param.i64_val);
}

void statement::bind_uint64(size_t i, uint64_t val) {
    auto& param = bound_params_.at(i);
    param.clear();
    if (val > std::numeric_limits<uint32_t>::max()) {
        param.c_type = SQL_C_UBIGINT;
        param.sql_type = SQL_BIGINT;
    }
    else {
        param.c_type = SQL_C_ULONG;
        param.sql_type = SQL_INTEGER;
    }
    param.size = sizeof(uint64_t);
    param.i64_val = val;
    bind_param(param, &param.u64_val);
}

void statement::bind_double(size_t i, double val) {
    auto& param = bound_params_.at(i);
    param.clear();
    param.c_type = SQL_C_DOUBLE;
    param.sql_type = SQL_DOUBLE;
    param.size = sizeof(double);
    param.dval = val;
    bind_param(param, &param.dval);
}

void statement::bind_time(size_t i, time_t val) {
    auto& param = bound_params_.at(i);
    param.clear();
    param.c_type = SQL_C_TIME;
    param.sql_type = SQL_TIME;
    param.size = sizeof(time_t);
    param.time_val = val;
    bind_param(param, &param.time_val);
}

void statement::bind_date(size_t i, date_t val) {
    auto& param = bound_params_.at(i);
    param.clear();
    param.c_type = SQL_C_DATE;
    param.sql_type = SQL_DATE;
    param.size = sizeof(date_t);
    param.date_val = val;
    bind_param(param, &param.date_val);
}

void statement::bind_timestamp(size_t i, const timestamp_t& val) {
    auto& param = bound_params_.at(i);
    param.clear();
    param.c_type = SQL_C_TIMESTAMP;
    param.sql_type = SQL_TIMESTAMP;
    param.size = sizeof(timestamp_t);
    param.timestamp_val = val;
    bind_param(param, &param.timestamp_val);
}

void statement::bind_string(size_t i, std::string_view val, bool copy) {
    auto& param = bound_params_.at(i);
    param.clear();
    param.c_type = SQL_C_CHAR;
    param.sql_type = SQL_LONGVARCHAR;
    param.size = val.size();
    if (!val.empty()) {
        if (copy) {
            param.owned_data.insert(param.owned_data.end(), val.begin(),
                                    val.end());
            param.view_data = param.owned_data;
        }
        else {
            param.view_data = std::span<const uint8_t>{
                reinterpret_cast<const uint8_t*>(val.data()), val.size()};
        }
    }
    bind_param(param, const_cast<uint8_t*>(param.view_data.data()));
}

void statement::bind_wstring(size_t i, wide_string_view val, bool copy) {
    auto& param = bound_params_.at(i);
    param.clear();
    param.c_type = SQL_C_WCHAR;
    param.sql_type = SQL_WLONGVARCHAR;
    param.size = val.size() * sizeof(wide_char_t);
    if (!val.empty()) {
        const uint8_t* start_ptr = reinterpret_cast<const uint8_t*>(val.data());
        const uint8_t* end_ptr = start_ptr + (val.size() * sizeof(wide_char_t));
        if (copy) {
            param.owned_data.insert(param.owned_data.end(), start_ptr, end_ptr);
            param.view_data = param.owned_data;
        }
        else {
            param.view_data = std::span<const uint8_t>{start_ptr, end_ptr};
        }
    }
    bind_param(param, const_cast<uint8_t*>(param.view_data.data()));
}

void statement::bind_binary(size_t i, std::span<const uint8_t> val, bool copy) {
    auto& param = bound_params_.at(i);
    param.clear();
    param.c_type = SQL_C_BINARY;
    param.sql_type = SQL_BINARY;
    param.size = val.size();
    if (!val.empty()) {
        if (copy) {
            param.owned_data.insert(param.owned_data.end(), val.begin(),
                                    val.end());
            param.view_data = param.owned_data;
        }
        else {
            param.view_data = val;
        }
    }
    bind_param(param, const_cast<uint8_t*>(param.view_data.data()));
}

void statement::bind_null(size_t i) {
    auto& param = bound_params_.at(i);
    param.c_type = SQL_C_CHAR;
    param.sql_type = SQL_CHAR;
    param.size = SQL_NULL_DATA;
    bind_param(param, nullptr);
}

void statement::bind_param(bound_param& param, void* val_ptr) {
    uint16_t index1 = param.index + 1;
    static_assert(sizeof(param.size) == sizeof(SQLLEN));
    SQLRETURN ret =
        ::SQLBindParameter(handle_.get(), index1, SQL_PARAM_INPUT, param.c_type,
                           param.sql_type, param.size, 0, val_ptr, param.size,
                           reinterpret_cast<SQLLEN*>(&param.size));
    throw_if_error(ret, "SQLBindParameter");
}

size_t statement::columns_count() const {
    short cols_n = 0;
    throw_if_error(::SQLNumResultCols(handle_.get(), &cols_n),
                   "SQLNumResultCols");
    return static_cast<size_t>(cols_n);
}

std::size_t statement::bind_params_count() {
    SQLSMALLINT params_n = 0;
    SQLRETURN ret = ::SQLNumParams(handle_.get(), &params_n);
    throw_if_error(ret, "SQLNumParams");
    return static_cast<size_t>(params_n);
}

void statement::clear_bindings() {
    if (!handle_) {
        return;
    }
    SQLRETURN ret = ::SQLFreeStmt(handle_.get(), SQL_RESET_PARAMS);
    throw_if_error(ret, "SQLFreeStmt(SQL_RESET_PARAMS)");
}

void statement::execute() {
    SQLRETURN ret = ::SQLExecute(handle_.get());
    throw_if_error(ret, "SQLExecute");

    {
        // cleare bound columns
        SQLRETURN ret = ::SQLFreeStmt(handle_.get(), SQL_UNBIND);
        throw_if_error(ret, "SQLFreeStmt(SQL_UNBIND)");
    }

    result_cols_.clear();
    cols_.clear();
    current_get_index_ = 0;
    current_bind_index_ = 0;
    get_columns_descriptions();
    bind_result_columns();
}

void statement::execute_select() {
    execute();
    short cols_n = static_cast<short>(columns_count());
    cols_.resize(cols_n);
    for (auto i : range(cols_n)) {
        auto& col_desc = result_cols_.emplace_back(describe_column(i));
        col_desc.c_type = sql_type_to_c_type(col_desc.sql_type);
        bound_column& bcol = cols_[i];
        bcol.index = i;
        bcol.c_type = sql_type_to_c_type(col_desc.sql_type);
        SQLRETURN ret = 0;
        if (bcol.c_type == SQL_C_CHAR || bcol.c_type == SQL_C_BINARY) {
            const size_t max_char_size = 4096;
            const size_t col_bind_size =
                std::min(max_char_size, col_desc.sql_size);
            bcol.blob.resize(col_bind_size);
            static_assert(sizeof(bcol.blob_size) == sizeof(SQLLEN));
            ret = ::SQLBindCol(handle_.get(), i + 1, bcol.c_type,
                               bcol.blob.data(),
                               static_cast<SQLLEN>(col_bind_size),
                               reinterpret_cast<SQLLEN*>(&bcol.blob_size));
        }
        else if (bcol.c_type == SQL_C_WCHAR) {
            // const size_t max_wchar_size = 4096;
            const size_t max_wchar_size = 20;
            const size_t col_bind_size =
                std::min(max_wchar_size, col_desc.sql_size * sizeof(wchar_t));
            bcol.blob.resize(col_bind_size);
            ret = ::SQLBindCol(handle_.get(), i + 1, bcol.c_type,
                               bcol.blob.data(),
                               static_cast<SQLLEN>(col_bind_size),
                               reinterpret_cast<SQLLEN*>(&bcol.blob_size));
        }
        else {
            ret = ::SQLBindCol(handle_.get(), i + 1, bcol.c_type, &bcol.u64_val,
                               sizeof(double),
                               reinterpret_cast<SQLLEN*>(&bcol.blob_size));
        }
        throw_if_error(ret, "SQLBindCol");
    }
}

bool statement::next() {
    for (auto& col : cols_) {
        col.clear();
    }
    current_get_index_ = 0;

    SQLRETURN ret = ::SQLFetch(handle_.get());
    if (ret == SQL_NO_DATA) {
        return false;
    }
    throw_if_error(ret, "SQLFetch");

    for (auto& col : cols_) {
        if (col.is_bound()) {
            if (col.null_indicator == SQL_NULL_DATA) {
                col.set_null();
            }
            if (col.is_variable_len() && !col.is_null()) {
                int64_t col_blob_size = static_cast<int64_t>(col.blob.size());
                int64_t null_size = col.c_type == SQL_C_CHAR ? sizeof(char)
                                    : col.c_type == SQL_C_WCHAR
                                        ? sizeof(wchar_t)
                                        : 0;
                col_blob_size -= null_size;
                if (col_blob_size >= col.blob_size) {
                    col.set_has_result();
                }
            }
            else {
                col.set_has_result();
            }
        }
    }
    return true;
}

std::optional<int64_t> statement::get_i64_value(size_t i) {
    auto& col = cols_.at(i);
    get_results_until(i);

    if (col.is_null()) {
        return std::nullopt;
    }
    if (col.c_type == SQL_C_SBIGINT) {
        return col.i64_val;
    }
    if (col.c_type == SQL_C_DOUBLE) {
        return static_cast<int64_t>(col.dval);
    }
    if (col.c_type == SQL_C_CHAR) {
        std::string_view data = col.get_string_view_data();
        return to_int64(data);
    }
    else if (col.c_type == SQL_C_WCHAR) {
        wide_string_view data = col.get_wstring_view_data();
        return to_int64(to_string(data));
    }
    throw std::system_error{
        std::make_error_code(std::errc::wrong_protocol_type)};
    return 0;
}

std::optional<double> statement::get_double_value(size_t i) {
    auto& col = cols_.at(i);
    get_results_until(i);

    if (col.is_null()) {
        return std::nullopt;
    }
    if (col.c_type == SQL_C_SBIGINT) {
        return static_cast<double>(col.i64_val);
    }
    if (col.c_type == SQL_C_DOUBLE) {
        return col.dval;
    }
    if (col.c_type == SQL_C_CHAR) {
        std::string_view data = col.get_string_view_data();
        return to_double(data);
    }
    else if (col.c_type == SQL_C_WCHAR) {
        wide_string_view data = col.get_wstring_view_data();
        return to_double(to_string(data));
    }
    throw std::system_error{
        std::make_error_code(std::errc::wrong_protocol_type)};
    return 0;
}

std::optional<std::string_view> statement::get_string_value(size_t i) {
    auto& col = cols_.at(i);
    get_results_until(i);

    if (col.is_null()) {
        return std::nullopt;
    }
    if (col.c_type == SQL_C_CHAR) {
        return col.get_string_view_data();
    }

    if (!col.has_string()) {
        col.convert_to_string();
    }

    return col.converted_string;
}

std::optional<wide_string_view> statement::get_wstring_value(size_t i) {
    auto& col = cols_.at(i);
    get_results_until(i);

    if (col.is_null()) {
        return std::nullopt;
    }
    if (col.c_type == SQL_C_WCHAR) {
        return col.get_wstring_view_data();
    }

    if (!col.has_wstring()) {
        col.convert_to_wstring();
    }

    return col.converted_wstring;
}

std::optional<std::span<const uint8_t>> statement::get_binary_value(size_t i) {
    auto& col = cols_.at(i);
    get_results_until(i);

    if (col.is_null()) {
        return std::nullopt;
    }

    if (is_blob_c_type(col.c_type)) {
        return col.get_blob_data();
    }

    throw std::system_error{
        std::make_error_code(std::errc::wrong_protocol_type)};
    return std::nullopt;
}

std::optional<odbc::time_t> statement::get_time_value(size_t i) {
    auto& col = cols_.at(i);
    get_results_until(i);

    if (col.is_null()) {
        return std::nullopt;
    }

    if (col.c_type == SQL_C_TYPE_TIME || col.c_type == SQL_C_TIME) {
        return col.time_val;
    }
    if (col.c_type == SQL_C_TYPE_TIMESTAMP || col.c_type == SQL_C_TIMESTAMP) {
        const auto& ts = col.timestamp_val;
        return odbc::time_t{ts.hour, ts.minute, ts.second};
    }

    throw std::system_error{
        std::make_error_code(std::errc::wrong_protocol_type)};
    return std::nullopt;
}

std::optional<date_t> statement::get_date_value(size_t i) {
    auto& col = cols_.at(i);
    get_results_until(i);

    if (col.is_null()) {
        return std::nullopt;
    }

    if (col.c_type == SQL_C_TYPE_DATE || col.c_len == SQL_C_DATE) {
        return col.date_val;
    }
    if (col.c_type == SQL_C_TYPE_TIMESTAMP || col.c_type == SQL_C_TIMESTAMP) {
        const auto& ts = col.timestamp_val;
        return date_t{ts.year, ts.month, ts.day};
    }

    throw std::system_error{
        std::make_error_code(std::errc::wrong_protocol_type)};
    return std::nullopt;
}

std::optional<timestamp_t> statement::get_timestamp_value(size_t i) {
    auto& col = cols_.at(i);
    get_results_until(i);

    if (col.is_null()) {
        return std::nullopt;
    }

    if (col.c_type == SQL_C_TYPE_TIMESTAMP || col.c_type == SQL_C_TIMESTAMP) {
        return col.timestamp_val;
    }

    throw std::system_error{
        std::make_error_code(std::errc::wrong_protocol_type)};
    return std::nullopt;
}

void statement::throw_if_error(short ret, const char* msg) const {
    if (ret != SQL_SUCCESS && ret != SQL_SUCCESS_WITH_INFO) {
        auto error_data = get_sql_error(SQL_HANDLE_STMT, handle_.get());
        throw std::runtime_error(std::string{msg} + " : " +
                                 to_string(error_data.msg));
    }
}