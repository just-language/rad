#include <rad/databases/sqlite.h>
#include <sqlite3.h>

using namespace RAD_LIB_NAMESPACE;
using namespace sqlite;

namespace {
    struct sqlite_error_category : public std::error_category {
        const char* name() const noexcept override {
            return "sqlite";
        }

        std::string message(int condition) const override {
            return sqlite3_errstr(condition);
        }
    };

    sqlite_error_category sqlite_error_category_inst;

    bool is_null_statement(const query::native_handle_type& stmt,
                           std::error_code& ec) noexcept {
        if (!stmt) {
            ec = std::make_error_code(std::errc::invalid_argument);
            return true;
        }
        return false;
    }

    void make_sqlite_error(std::error_code& ec, int rc) noexcept {
        ec.assign(rc, sqlite_category());
    }
} // namespace

const std::error_category& sqlite::sqlite_category() noexcept {
    return sqlite_error_category_inst;
}

void database::open(zstring_view path, flags flags,
                    std::error_code& ec) noexcept {
    ec.clear();
    sqlite3* db_handle = nullptr;
    // sqlite3_open_v2 allocates a handle even if it fails to open the
    // database except if no memory
    int rc =
        sqlite3_open_v2(path.data(), &db_handle,
                        static_cast<int>(flags) | SQLITE_OPEN_NOMUTEX, nullptr);
    if (rc != SQLITE_OK) {
        if (!db_handle) {
            // no memory to allocate the database handle
            ec = std::make_error_code(std::errc::not_enough_memory);
        }
        else {
            ec.assign(sqlite3_errcode(db_handle), sqlite_category());
            sqlite3_close(db_handle);
        }
    }
    else {
        db_.reset(db_handle);
    }
}

void database::open(zstring_view path, flags flags) {
    sqlite3* db_handle = nullptr;
    // sqlite3_open_v2 allocates a handle even if it fails to open the
    // database except if no memory
    int rc = sqlite3_open_v2(path.data(), &db_handle, static_cast<int>(flags),
                             nullptr);
    if (rc != SQLITE_OK) {
        if (!db_handle) {
            // no memory to allocate the database handle
            throw std::system_error{
                std::make_error_code(std::errc::not_enough_memory)};
        }
        else {
            auto on_exit =
                scope_exit([db_handle] { sqlite3_close(db_handle); });
            throw std::system_error{sqlite3_errcode(db_handle),
                                    sqlite_category(),
                                    sqlite3_errmsg(db_handle)};
        }
    }
    else {
        db_.reset(db_handle);
    }
}

const char* database::last_error_msg() const noexcept {
    if (!db_) {
        return "";
    }
    const char* msg = sqlite3_errmsg(db_.get());
    return msg ? msg : "";
}

size_t database::changed_rows(std::error_code& ec) noexcept {
    if (!db_) {
        ec = std::make_error_code(std::errc::invalid_argument);
        return 0;
    }
    return static_cast<size_t>(sqlite3_changes(db_.get()));
}

size_t database::total_changed_rows(std::error_code& ec) noexcept {
    if (!db_) {
        ec = std::make_error_code(std::errc::invalid_argument);
        return 0;
    }
    return static_cast<size_t>(sqlite3_total_changes(db_.get()));
}

int64_t database::last_insert_rowid(std::error_code& ec) noexcept {
    if (!db_) {
        ec = std::make_error_code(std::errc::invalid_argument);
        return 0;
    }
    int64_t rowid = sqlite3_last_insert_rowid(db_.get());
    if (!rowid) {
        ec = std::make_error_code(std::errc::invalid_argument);
        return 0;
    }
    return rowid;
}

size_t database::execute(std::string_view statements,
                         std::error_code& ec) noexcept {
    ec.clear();
    size_t count = 0;
    while (!statements.empty()) {
        sqlite3_stmt* raw_stmt = nullptr;
        const char* next_stmt = nullptr;
        int rc = sqlite3_prepare_v2(db_.get(), statements.data(),
                                    static_cast<int>(statements.size()),
                                    &raw_stmt, &next_stmt);
        stmt_handle_type stmt{raw_stmt};
        if (rc != SQLITE_OK) {
            make_sqlite_error(ec, rc);
            return count;
        }
        // make statements point to the next statement
        if (!next_stmt || next_stmt < statements.data() ||
            next_stmt >= statements.data()) {
            statements = {};
        }
        else {
            const char* stmts_end = statements.data() + statements.size();
            statements = std::string_view{next_stmt, stmts_end};
        }
        // execute the prepared statement using one sqlite3_step() at
        // least
        do {
            rc = sqlite3_step(stmt.get());
        } while (rc == SQLITE_ROW);

        if (rc != SQLITE_DONE) {
            make_sqlite_error(ec, rc);
            return count;
        }
        count += 1;
    }
    return count;
}

void database::create_sql_fn(zstring_view name, int args_n, void* appdata,
                             sqlite_fn_t func, sqlite_destroy_t destroy,
                             std::error_code& ec) noexcept {
    // destroy fn is called immediately to free appdata if
    // sqlite3_create_function_v2() fails
    int rc =
        sqlite3_create_function_v2(db_.get(), name.data(), args_n, SQLITE_UTF8,
                                   appdata, func, nullptr, nullptr, destroy);
    if (rc != SQLITE_OK) {
        ec = {rc, sqlite_category()};
    }
}

void query::prepare(database& db, std::string_view query_statement,
                    std::error_code& ec) noexcept {
    ec.clear();
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(
        db.native_handle().get(), query_statement.data(),
        static_cast<int>(query_statement.size()), &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return make_sqlite_error(ec, rc);
    }
    db_ = db;
    stmt_.reset(stmt);
}

void query::execute(std::error_code& ec) noexcept {
    if (is_null_statement(stmt_, ec)) {
        return;
    }
    current_get_index_ = 0;
    has_rows_ = false;
    cols_count_ = 0;
    int rc = sqlite3_step(stmt_.get());
    if (rc != SQLITE_DONE && rc != SQLITE_ROW) {
        make_sqlite_error(ec, rc);
    }
    cols_count_ = sqlite3_column_count(stmt_.get());
    has_rows_ = rc == SQLITE_ROW;
}

bool query::next(std::error_code& ec) {
    if (is_null_statement(stmt_, ec)) {
        return false;
    }
    current_get_index_ = 0;
    has_rows_ = false;
    cols_count_ = 0;
    int rc = sqlite3_step(stmt_.get());
    if (rc == SQLITE_ROW) {
        has_rows_ = true;
        cols_count_ = sqlite3_column_count(stmt_.get());
        return true;
    }
    if (rc == SQLITE_DONE) {
        cols_count_ = sqlite3_column_count(stmt_.get());
        return false;
    }
    make_sqlite_error(ec, rc);
    return false;
}

void query::clear_bindings() noexcept {
    if (stmt_) {
        sqlite3_clear_bindings(stmt_.get());
    }
}

void query::reset(std::error_code& ec) noexcept {
    ec.clear();
    if (!stmt_) {
        return;
    }
    // sqlite3_reset may return the error code of last sqlite3_step
    int rc = sqlite3_reset(stmt_.get());
    (void)rc;
    // if (rc != SQLITE_OK)
    //	make_sqlite_error(ec, rc);
}

size_t query::bind_params_count() noexcept {
    if (!stmt_) {
        return 0;
    }
    return static_cast<size_t>(sqlite3_bind_parameter_count(stmt_.get()));
}

std::optional<size_t> query::bind_param_index(zstring_view name) noexcept {
    if (stmt_) {
        int index = sqlite3_bind_parameter_index(stmt_.get(), name.data());
        if (index > 0) {
            return static_cast<size_t>(index - 1);
        }
    }
    return std::nullopt;
}

std::string_view query::param_name_view(size_t index) noexcept {
    if (stmt_) {
        ++index;
        const char* name =
            sqlite3_bind_parameter_name(stmt_.get(), static_cast<int>(index));
        if (name) {
            return name;
        }
    }
    return {};
}

std::size_t query::columns_count() const noexcept {
    return sqlite3_column_count(stmt_.get());
}

std::size_t query::column_size(std::size_t index) const noexcept {
    return sqlite3_column_bytes(stmt_.get(), static_cast<int>(index));
}

column_type query::column_type(std::size_t index) const noexcept {
    return static_cast<sqlite::column_type>(
        sqlite3_column_type(stmt_.get(), static_cast<int>(index)));
}

std::string_view query::column_name_view(std::size_t index) const noexcept {
    const char* name =
        sqlite3_column_name(stmt_.get(), static_cast<int>(index));
    if (!name) {
        return std::string_view();
    }
    return std::string_view(name);
}

void query::validate_get_index(std::size_t index,
                               std::error_code& ec) const noexcept {
    ec.clear();
    if (index >= cols_count_ || !has_rows_) {
        ec.assign(SQLITE_RANGE, sqlite_category());
    }
}

void sqlite::detail::close_db(sqlite3* db) noexcept {
    assert(db != nullptr);
    int rc = sqlite3_close_v2(db);
    std::ignore = rc;
    assert(rc == SQLITE_OK);
}

void sqlite::detail::close_stmt(sqlite3_stmt* stmt) noexcept {
    assert(stmt != nullptr);
    sqlite3_finalize(stmt);
}

void sqlite::detail::bind_int(sqlite3_stmt* stmt, std::size_t index0, int val,
                              std::error_code& ec) noexcept {
    ec.clear();
    int rc = sqlite3_bind_int(stmt, static_cast<int>(index0 + 1), val);
    if (rc != SQLITE_OK) {
        make_sqlite_error(ec, rc);
    }
}

void sqlite::detail::bind_int(sqlite3_stmt* stmt, std::size_t index0,
                              int64_t val, std::error_code& ec) noexcept {
    ec.clear();
    int rc = sqlite3_bind_int64(stmt, static_cast<int>(index0 + 1), val);
    if (rc != SQLITE_OK) {
        make_sqlite_error(ec, rc);
    }
}

void sqlite::detail::bind_double(sqlite3_stmt* stmt, std::size_t index0,
                                 double val, std::error_code& ec) noexcept {
    ec.clear();
    int rc = sqlite3_bind_double(stmt, static_cast<int>(index0 + 1), val);
    if (rc != SQLITE_OK) {
        make_sqlite_error(ec, rc);
    }
}

void sqlite::detail::bind_null(sqlite3_stmt* stmt, std::size_t index0,
                               std::error_code& ec) noexcept {
    ec.clear();
    int rc = sqlite3_bind_null(stmt, static_cast<int>(index0 + 1));
    if (rc != SQLITE_OK) {
        make_sqlite_error(ec, rc);
    }
}

void sqlite::detail::bind_text(sqlite3_stmt* stmt, std::size_t index0,
                               std::string_view text, bool copy,
                               std::error_code& ec) noexcept {
    ec.clear();
    const int index = static_cast<int>(index0 + 1);
    auto destructor = copy ? SQLITE_TRANSIENT : SQLITE_STATIC;
    const char* data_ptr = text.empty() ? "" : text.data();
    const bool is_64 =
        text.size() > static_cast<std::size_t>(std::numeric_limits<int>::max());
    int rc = 0;
    if (is_64) {
        rc = sqlite3_bind_text64(stmt, index, data_ptr, text.size(), destructor,
                                 SQLITE_UTF8);
    }
    else {
        rc = sqlite3_bind_text(stmt, index, data_ptr,
                               static_cast<int>(text.size()), destructor);
    }
    if (rc != SQLITE_OK) {
        make_sqlite_error(ec, rc);
    }
}

void sqlite::detail::bind_text16(sqlite3_stmt* stmt, std::size_t index0,
                                 std::u16string_view text, bool copy,
                                 std::error_code& ec) noexcept {
    ec.clear();
    const int index = static_cast<int>(index0 + 1);
    auto destructor = copy ? SQLITE_TRANSIENT : SQLITE_STATIC;
    const char16_t* data_ptr = text.empty() ? u"" : text.data();
    size_t nbytes = text.size() * sizeof(char16_t);
    const bool is_64 =
        nbytes > static_cast<std::size_t>(std::numeric_limits<int>::max());
    int rc = 0;
    if (is_64) {
        rc = sqlite3_bind_text64(stmt, index,
                                 reinterpret_cast<const char*>(data_ptr),
                                 nbytes, destructor, SQLITE_UTF16);
    }
    else {
        rc = sqlite3_bind_text16(stmt, index, data_ptr,
                                 static_cast<int>(nbytes), destructor);
    }
    if (rc != SQLITE_OK) {
        make_sqlite_error(ec, rc);
    }
}

void sqlite::detail::bind_blob(sqlite3_stmt* stmt, std::size_t index0,
                               const_buffer blob, bool copy,
                               std::error_code& ec) noexcept {
    ec.clear();
    const int index = static_cast<int>(index0 + 1);
    auto destructor = copy ? SQLITE_TRANSIENT : SQLITE_STATIC;
    const void* data_ptr = blob.empty() ? &blob : blob.data();
    const bool is_64 =
        blob.size() > static_cast<std::size_t>(std::numeric_limits<int>::max());
    int rc = 0;
    if (is_64) {
        rc =
            sqlite3_bind_blob64(stmt, index, data_ptr, blob.size(), destructor);
    }
    else {
        rc = sqlite3_bind_blob(stmt, index, data_ptr,
                               static_cast<int>(blob.size()), destructor);
    }
    if (rc != SQLITE_OK) {
        make_sqlite_error(ec, rc);
    }
}

int64_t sqlite::detail::extract_sqlite_int64_column(sqlite3_stmt* stmt,
                                                    int col) noexcept {
    assert(stmt != nullptr);
    return sqlite3_column_int64(stmt, col);
}

double sqlite::detail::extract_sqlite_double_column(sqlite3_stmt* stmt,
                                                    int col) noexcept {
    assert(stmt != nullptr);
    return sqlite3_column_double(stmt, col);
}

std::span<const uint8_t>
sqlite::detail::extract_sqlite_bytes_column(sqlite3_stmt* stmt,
                                            int col) noexcept {
    assert(stmt != nullptr);
    int len = sqlite3_column_bytes(stmt, col);
    const void* p = sqlite3_column_blob(stmt, col);
    if (len <= 0 || !p) {
        return {};
    }
    return std::span<const uint8_t>{static_cast<const uint8_t*>(p),
                                    static_cast<size_t>(len)};
}

std::string_view sqlite::detail::extract_sqlite_text_column(sqlite3_stmt* stmt,
                                                            int col) noexcept {
    assert(stmt != nullptr);
    int len = sqlite3_column_bytes(stmt, col);
    const char* p =
        reinterpret_cast<const char*>(sqlite3_column_text(stmt, col));
    if (!p || len <= 0) {
        return "";
    }
    return std::string_view{p, static_cast<size_t>(len)};
}

std::u16string_view
sqlite::detail::extract_sqlite_text16_column(sqlite3_stmt* stmt,
                                             int col) noexcept {
    assert(stmt != nullptr);
    int len = sqlite3_column_bytes16(stmt, col);
    len /= sizeof(char16_t);
    const char16_t* p =
        static_cast<const char16_t*>(sqlite3_column_text16(stmt, col));
    if (!p || len <= 0) {
        return u"";
    }
    return std::u16string_view{p, static_cast<size_t>(len)};
}

int64_t
sqlite::detail::extract_sqlite_int64_value(sqlite3_value* value) noexcept {
    assert(value != nullptr);
    return sqlite3_value_int64(value);
}

double
sqlite::detail::extract_sqlite_double_value(sqlite3_value* value) noexcept {
    assert(value != nullptr);
    return sqlite3_value_double(value);
}

std::span<const uint8_t>
sqlite::detail::extract_sqlite_bytes_value(sqlite3_value* value) noexcept {
    assert(value != nullptr);
    int len = sqlite3_value_bytes(value);
    const void* p = sqlite3_value_blob(value);
    if (len <= 0 || !p) {
        return {};
    }
    return std::span<const uint8_t>{static_cast<const uint8_t*>(p),
                                    static_cast<size_t>(len)};
}

std::string_view
sqlite::detail::extract_sqlite_text_value(sqlite3_value* value) noexcept {
    assert(value != nullptr);
    int len = sqlite3_value_bytes(value);
    const char* p = reinterpret_cast<const char*>(sqlite3_value_text(value));
    if (!p || len <= 0) {
        return "";
    }
    return std::string_view{p, static_cast<size_t>(len)};
}

std::u16string_view
sqlite::detail::extract_sqlite_text16_value(sqlite3_value* value) noexcept {
    assert(value != nullptr);
    int len = sqlite3_value_bytes16(value);
    len /= sizeof(char16_t);
    const char16_t* p =
        static_cast<const char16_t*>(sqlite3_value_text16(value));
    if (!p || len <= 0) {
        return u"";
    }
    return std::u16string_view{p, static_cast<size_t>(len)};
}

void sqlite::detail::store_sqlite_int_result(sqlite3_context* context,
                                             int res) noexcept {
    assert(context != nullptr);
    sqlite3_result_int(context, res);
}

void sqlite::detail::store_sqlite_int_result(sqlite3_context* context,
                                             int64_t res) noexcept {
    assert(context != nullptr);
    sqlite3_result_int64(context, res);
}

void sqlite::detail::store_sqlite_double_result(sqlite3_context* context,
                                                double res) noexcept {
    assert(context != nullptr);
    sqlite3_result_double(context, res);
}

void sqlite::detail::store_sqlite_null_result(
    sqlite3_context* context) noexcept {
    assert(context != nullptr);
    sqlite3_result_null(context);
}

void sqlite::detail::store_sqlite_text_result(sqlite3_context* context,
                                              std::string_view res) noexcept {
    assert(context != nullptr);
    sqlite3_result_text(context, res.data(), static_cast<int>(res.size()),
                        SQLITE_TRANSIENT);
}

void sqlite::detail::store_sqlite_text16_result(
    sqlite3_context* context, std::u16string_view res) noexcept {
    assert(context != nullptr);
    sqlite3_result_text16(context, res.data(),
                          static_cast<int>(res.size() * sizeof(char16_t)),
                          SQLITE_TRANSIENT);
}

void sqlite::detail::store_sqlite_blob_result(sqlite3_context* context,
                                              const_buffer res) noexcept {
    assert(context != nullptr);
    sqlite3_result_blob64(context, res.data(), res.size(), SQLITE_TRANSIENT);
}

void sqlite::detail::store_sqlite_error_result(sqlite3_context* context,
                                               std::string_view res) noexcept {
    assert(context != nullptr);
    sqlite3_result_error(context, res.data(), static_cast<int>(res.size()));
}

void* sqlite::detail::get_sqlite_user_data(sqlite3_context* context) noexcept {
    assert(context != nullptr);
    return sqlite3_user_data(context);
}