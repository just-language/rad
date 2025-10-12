#pragma once
#include <rad/buffer.h>
#include <rad/function_traits.h>
#include <rad/os_handles_base.h>
#include <rad/string.h>
#include <rad/trackable.h>

#include <chrono>
#include <memory>
#include <optional>
#include <span>
#include <utility>
#include <vector>

extern "C" {
struct sqlite3;
struct sqlite3_stmt;
struct sqlite3_context;
struct sqlite3_value;
}

namespace RAD_LIB_NAMESPACE::sqlite {
    namespace detail {
        RAD_EXPORT_DECL void close_db(sqlite3* db) noexcept;

        RAD_EXPORT_DECL void close_stmt(sqlite3_stmt* stmt) noexcept;

        RAD_EXPORT_DECL void bind_int(sqlite3_stmt* stmt, std::size_t index0,
                                      int val, std::error_code& ec) noexcept;

        RAD_EXPORT_DECL void bind_int(sqlite3_stmt* stmt, std::size_t index0,
                                      int64_t val,
                                      std::error_code& ec) noexcept;

        RAD_EXPORT_DECL void bind_double(sqlite3_stmt* stmt, std::size_t index0,
                                         double val,
                                         std::error_code& ec) noexcept;

        RAD_EXPORT_DECL void bind_null(sqlite3_stmt* stmt, std::size_t index0,
                                       std::error_code& ec) noexcept;

        RAD_EXPORT_DECL void bind_text(sqlite3_stmt* stmt, std::size_t index0,
                                       std::string_view text, bool copy,
                                       std::error_code& ec) noexcept;

        RAD_EXPORT_DECL void bind_text16(sqlite3_stmt* stmt, std::size_t index0,
                                         std::u16string_view text, bool copy,
                                         std::error_code& ec) noexcept;

        RAD_EXPORT_DECL void bind_blob(sqlite3_stmt* stmt, std::size_t index0,
                                       const_buffer val, bool copy,
                                       std::error_code& ec) noexcept;

        RAD_EXPORT_DECL int64_t extract_sqlite_int64_column(sqlite3_stmt* stmt,
                                                            int col) noexcept;

        RAD_EXPORT_DECL double extract_sqlite_double_column(sqlite3_stmt* stmt,
                                                            int col) noexcept;

        RAD_EXPORT_DECL std::span<const uint8_t>
        extract_sqlite_bytes_column(sqlite3_stmt* stmt, int col) noexcept;

        RAD_EXPORT_DECL std::string_view
        extract_sqlite_text_column(sqlite3_stmt* stmt, int col) noexcept;

        RAD_EXPORT_DECL std::u16string_view
        extract_sqlite_text16_column(sqlite3_stmt* stmt, int col) noexcept;

        RAD_EXPORT_DECL int64_t
        extract_sqlite_int64_value(sqlite3_value* value) noexcept;

        RAD_EXPORT_DECL double
        extract_sqlite_double_value(sqlite3_value* value) noexcept;

        RAD_EXPORT_DECL std::span<const uint8_t>
        extract_sqlite_bytes_value(sqlite3_value* value) noexcept;

        RAD_EXPORT_DECL std::string_view
        extract_sqlite_text_value(sqlite3_value* value) noexcept;

        RAD_EXPORT_DECL std::u16string_view
        extract_sqlite_text16_value(sqlite3_value* value) noexcept;

        RAD_EXPORT_DECL void store_sqlite_int_result(sqlite3_context* context,
                                                     int res) noexcept;

        RAD_EXPORT_DECL void store_sqlite_int_result(sqlite3_context* context,
                                                     int64_t res) noexcept;

        RAD_EXPORT_DECL void
        store_sqlite_double_result(sqlite3_context* context,
                                   double res) noexcept;

        RAD_EXPORT_DECL void
        store_sqlite_null_result(sqlite3_context* context) noexcept;

        RAD_EXPORT_DECL void
        store_sqlite_text_result(sqlite3_context* context,
                                 std::string_view res) noexcept;

        RAD_EXPORT_DECL void
        store_sqlite_text16_result(sqlite3_context* context,
                                   std::u16string_view res) noexcept;

        RAD_EXPORT_DECL void
        store_sqlite_blob_result(sqlite3_context* context,
                                 const_buffer res) noexcept;

        RAD_EXPORT_DECL void
        store_sqlite_error_result(sqlite3_context* context,
                                  std::string_view res) noexcept;

        RAD_EXPORT_DECL void*
        get_sqlite_user_data(sqlite3_context* context) noexcept;

        template <class T>
        using SQLiteIntType =
            std::conditional_t<(sizeof(T) < sizeof(int)) ||
                                   (sizeof(T) == sizeof(int) &&
                                    std::is_signed_v<T>),
                               int, int64_t>;

        template <class T>
            requires(!std::integral<std::remove_cvref_t<T>>)
        T&& cast_sqlite_arg(T&& arg) {
            return std::forward<T>(arg);
        }

        template <std::integral T>
        SQLiteIntType<T> cast_sqlite_arg(T arg) {
            return static_cast<SQLiteIntType<T>>(arg);
        }

        inline double cast_sqlite_arg(float arg) {
            return arg;
        }

        inline double cast_sqlite_arg(double arg) {
            return arg;
        }
    } // namespace detail

    /// Type of RAII handle used to hold the sqlite database connection.
    using db_handle_type =
        std::unique_ptr<void, os::handle_deleter<sqlite3*, detail::close_db>>;

    /// Type of RAII handle used to hold the sqlite statement.
    using stmt_handle_type =
        std::unique_ptr<void,
                        os::handle_deleter<sqlite3_stmt*, detail::close_stmt>>;

    /*!
     * @brief SQLite open flags.
     */
    enum class flags { read = 1, write = 2, read_write_create = 2 | 4 };

    /*!
     * @brief SQLite column type.
     */
    enum class column_type {
        integeral = 1,  // a signed integral type (short, int, long, int64 ...)
        floating_point, // double and float
        text,           // text encoded in utf-8 or utf-16 or ...
        blob,           // data is stored as it is provided, usually raw binary
                        // data
        null,           // the column contains null value
    };

    /*!
     * @brief Wrapper type used to bind a value by view to a quey parameter.
     * This wrapper does not store a value but only references the passed
     * value which must be valid as long as this type is used.
     * Additional sqlite functions may place more restrictions on the
     * lifetime of referenced value.
     * @tparam T Type of wrapped value.
     */
    template <class T>
    class as_view {
    public:
        /// Type of wrapped value.
        using value_type = T;

        /*!
         * @brief Wrap @p val in as_view.
         * @param val Reference to value.
         * @p val must be valid as long as this instance is
         * used.
         */
        as_view(const T& val) noexcept : val{val} {
        }

        /*!
         * @brief Get the stored const reference.
         * @return The stored const reference.
         */
        const T& value() const noexcept {
            return val;
        }

    private:
        const T& val;
    };

    class database;

    /// returns a reference to the sqlite error category
    RAD_EXPORT_DECL const std::error_category& sqlite_category() noexcept;

    template <class T>
    struct SQLiteValueExtractor;

    template <class T>
    struct SQLiteColumnExtractor;

    template <class T>
    struct SQLiteResultStore;

    template <class T>
    struct SQLiteBinder;

    template <class T>
    concept SQLiteValue = requires(sqlite3_value* value) {
        typename SQLiteValueExtractor<T>;
        { SQLiteValueExtractor<T>::extract(value) } -> std::same_as<T>;
    };

    template <class T>
    concept SQLiteColumnValue = requires(sqlite3_stmt* stmt, int col) {
        typename SQLiteColumnExtractor<T>;
        { SQLiteColumnExtractor<T>::extract(stmt, col) } -> std::same_as<T>;
    };

    template <class T>
    concept SQLiteResult = std::same_as<void, T> ||
                           requires(sqlite3_context* context, const T& value) {
                               typename SQLiteResultStore<T>;
                               SQLiteResultStore<T>::store(context, value);
                           };

    template <class T>
    concept SQLiteBindable =
        requires(sqlite3_stmt* stmt, std::size_t index0, const T& value,
                 bool copy, std::error_code& ec) {
            typename SQLiteBinder<T>;
            SQLiteBinder<T>::bind(stmt, index0, value, copy, ec);
        };

    namespace detail {
        inline void throw_sqlite_exception(database& db,
                                           const std::error_code& ec);

        template <class T>
        inline constexpr bool are_sqlite_args = false;

        template <class... T>
        inline constexpr bool are_sqlite_args<std::tuple<T...>> =
            (SQLiteValue<T> && ...);

        template <class Tuple>
        concept SQLiteArgs = are_sqlite_args<Tuple>;

        template <class Tuple, std::size_t... I>
        static Tuple extract_sqlite_fn_args(sqlite3_value** argv,
                                            std::index_sequence<I...>) {
            return Tuple{
                SQLiteValueExtractor<std::tuple_element_t<I, Tuple>>::extract(
                    argv[I])...};
        }

        template <class Fn, class Ret, class ArgsTuple, std::size_t ArgsN>
        void sqlite_fn_wrapper(sqlite3_context* context, int argc,
                               sqlite3_value** argv) noexcept {
            void* userdata = get_sqlite_user_data(context);
            assert(userdata != nullptr);
            assert(argc >= ArgsN);
            if (userdata == nullptr || argc < ArgsN) {
                return;
            }
            Fn& fn = *static_cast<Fn*>(userdata);
            auto args = extract_sqlite_fn_args<ArgsTuple>(
                argv, std::make_index_sequence<ArgsN>{});
            try {
                if constexpr (std::is_same_v<Ret, void>) {
                    std::apply(fn, std::move(args));
                }
                else {
                    SQLiteResultStore<Ret>::store(
                        context,
                        cast_sqlite_arg(std::apply(fn, std::move(args))));
                }
            }
            catch (const std::exception& ex) {
                store_sqlite_error_result(context, ex.what());
            }
            catch (...) {
                store_sqlite_error_result(context, "unknown error");
            }
        }
    } // namespace detail

    template <class Fn>
    concept SQLiteFunction = requires() {
        typename function_traits<Fn>;
        requires SQLiteResult<typename function_traits<Fn>::result_type>;
        requires detail::SQLiteArgs<typename function_traits<Fn>::args_tuple>;
    };

    template <std::integral T>
    struct SQLiteBinder<T> {
        static void bind(sqlite3_stmt* stmt, std::size_t index0, T value,
                         bool copy, std::error_code& ec) {
            std::ignore = copy;
            detail::bind_int(stmt, index0, detail::cast_sqlite_arg(value), ec);
        }
    };

    template <std::floating_point T>
    struct SQLiteBinder<T> {
        static void bind(sqlite3_stmt* stmt, std::size_t index0, T value,
                         bool copy, std::error_code& ec) {
            std::ignore = copy;
            detail::bind_double(stmt, index0, value, ec);
        }
    };

    template <>
    struct SQLiteBinder<std::nullptr_t> {
        static void bind(sqlite3_stmt* stmt, std::size_t index0, std::nullptr_t,
                         bool copy, std::error_code& ec) {
            std::ignore = copy;
            detail::bind_null(stmt, index0, ec);
        }
    };

    template <>
    struct SQLiteBinder<std::string_view> {
        static void bind(sqlite3_stmt* stmt, std::size_t index0,
                         std::string_view value, bool copy,
                         std::error_code& ec) {
            detail::bind_text(stmt, index0, value, copy, ec);
        }
    };

    template <>
    struct SQLiteBinder<std::string> {
        static void bind(sqlite3_stmt* stmt, std::size_t index0,
                         std::string_view value, bool copy,
                         std::error_code& ec) {
            SQLiteBinder<std::string_view>::bind(stmt, index0, value, copy, ec);
        }
    };

    template <>
    struct SQLiteBinder<std::u16string_view> {
        static void bind(sqlite3_stmt* stmt, std::size_t index0,
                         std::u16string_view value, bool copy,
                         std::error_code& ec) {
            detail::bind_text16(stmt, index0, value, copy, ec);
        }
    };

    template <>
    struct SQLiteBinder<std::u16string> {
        static void bind(sqlite3_stmt* stmt, std::size_t index0,
                         std::u16string_view value, bool copy,
                         std::error_code& ec) {
            SQLiteBinder<std::u16string_view>::bind(stmt, index0, value, copy,
                                                    ec);
        }
    };

    template <>
    struct SQLiteBinder<const_buffer> {
        static void bind(sqlite3_stmt* stmt, std::size_t index0,
                         const_buffer value, bool copy, std::error_code& ec) {
            detail::bind_blob(stmt, index0, value, copy, ec);
        }
    };

    template <>
    struct SQLiteBinder<mutable_buffer> {
        static void bind(sqlite3_stmt* stmt, std::size_t index0,
                         mutable_buffer value, bool copy, std::error_code& ec) {
            detail::bind_blob(stmt, index0, value, copy, ec);
        }
    };

    template <>
    struct SQLiteBinder<std::span<const uint8_t>> {
        static void bind(sqlite3_stmt* stmt, std::size_t index0,
                         std::span<const uint8_t> value, bool copy,
                         std::error_code& ec) {
            detail::bind_blob(stmt, index0, buffer(value), copy, ec);
        }
    };

    template <>
    struct SQLiteBinder<std::span<uint8_t>> {
        static void bind(sqlite3_stmt* stmt, std::size_t index0,
                         std::span<uint8_t> value, bool copy,
                         std::error_code& ec) {
            detail::bind_blob(stmt, index0, buffer(value), copy, ec);
        }
    };

    template <>
    struct SQLiteBinder<std::vector<uint8_t>> {
        static void bind(sqlite3_stmt* stmt, std::size_t index0,
                         const std::vector<uint8_t>& value, bool copy,
                         std::error_code& ec) {
            detail::bind_blob(stmt, index0, buffer(value), copy, ec);
        }
    };

    template <SQLiteBindable T>
    struct SQLiteBinder<std::optional<T>> {
        static void bind(sqlite3_stmt* stmt, std::size_t index0,
                         const std::optional<T>& value, bool copy,
                         std::error_code& ec) {
            if (value.has_value()) {
                SQLiteBinder<T>::bind(stmt, index0, *value, copy, ec);
            }
            else {
                SQLiteBinder<std::nullptr_t>::bind(stmt, index0, nullptr, copy,
                                                   ec);
            }
        }
    };

    template <SQLiteBindable T>
    struct SQLiteBinder<as_view<T>> {
        static void bind(sqlite3_stmt* stmt, std::size_t index0,
                         const as_view<T>& value, bool copy,
                         std::error_code& ec) {
            std::ignore = copy;
            SQLiteBinder<T>::bind(stmt, index0, value.value(), true, ec);
        }
    };

    template <std::integral T>
    struct SQLiteValueExtractor<T> {
        static T extract(sqlite3_value* value) {
            return static_cast<T>(detail::extract_sqlite_int64_value(value));
        }
    };

    template <std::floating_point T>
    struct SQLiteValueExtractor<T> {
        static T extract(sqlite3_value* value) {
            return static_cast<T>(detail::extract_sqlite_double_value(value));
        }
    };

    template <>
    struct SQLiteValueExtractor<std::string_view> {
        static std::string_view extract(sqlite3_value* value) {
            return detail::extract_sqlite_text_value(value);
        }
    };

    template <>
    struct SQLiteValueExtractor<std::string> {
        static std::string extract(sqlite3_value* value) {
            return std::string{
                SQLiteValueExtractor<std::string_view>::extract(value)};
        }
    };

    template <>
    struct SQLiteValueExtractor<std::u16string_view> {
        static std::u16string_view extract(sqlite3_value* value) {
            return detail::extract_sqlite_text16_value(value);
        }
    };

    template <>
    struct SQLiteValueExtractor<std::u16string> {
        static std::u16string extract(sqlite3_value* value) {
            return std::u16string{
                SQLiteValueExtractor<std::u16string_view>::extract(value)};
        }
    };

    template <>
    struct SQLiteValueExtractor<std::span<const uint8_t>> {
        static std::span<const uint8_t> extract(sqlite3_value* value) {
            return detail::extract_sqlite_bytes_value(value);
        }
    };

    template <>
    struct SQLiteValueExtractor<std::vector<uint8_t>> {
        static std::vector<uint8_t> extract(sqlite3_value* value) {
            std::vector<uint8_t> res;
            auto bytes = detail::extract_sqlite_bytes_value(value);
            res.insert(res.end(), bytes.begin(), bytes.end());
            return res;
        }
    };

    template <>
    struct SQLiteValueExtractor<const_buffer> {
        static const_buffer extract(sqlite3_value* value) {
            auto bytes = detail::extract_sqlite_bytes_value(value);
            return buffer(bytes);
        }
    };

    template <class Rep, class Period>
    struct SQLiteValueExtractor<std::chrono::duration<Rep, Period>> {
        static std::chrono::duration<Rep, Period>
        extract(sqlite3_value* value) {
            return std::chrono::duration<Rep, Period>{
                detail::extract_sqlite_int64_value(value)};
        }
    };

    template <std::integral T>
    struct SQLiteColumnExtractor<T> {
        static T extract(sqlite3_stmt* stmt, int col) {
            return static_cast<T>(
                detail::extract_sqlite_int64_column(stmt, col));
        }
    };

    template <std::floating_point T>
    struct SQLiteColumnExtractor<T> {
        static T extract(sqlite3_stmt* stmt, int col) {
            return static_cast<T>(
                detail::extract_sqlite_double_column(stmt, col));
        }
    };

    template <>
    struct SQLiteColumnExtractor<std::string_view> {
        static std::string_view extract(sqlite3_stmt* stmt, int col) {
            return detail::extract_sqlite_text_column(stmt, col);
        }
    };

    template <>
    struct SQLiteColumnExtractor<std::string> {
        static std::string extract(sqlite3_stmt* stmt, int col) {
            return std::string{
                SQLiteColumnExtractor<std::string_view>::extract(stmt, col)};
        }
    };

    template <>
    struct SQLiteColumnExtractor<std::u16string_view> {
        static std::u16string_view extract(sqlite3_stmt* stmt, int col) {
            return detail::extract_sqlite_text16_column(stmt, col);
        }
    };

    template <>
    struct SQLiteColumnExtractor<std::u16string> {
        static std::u16string extract(sqlite3_stmt* stmt, int col) {
            return std::u16string{
                SQLiteColumnExtractor<std::u16string_view>::extract(stmt, col)};
        }
    };

    template <>
    struct SQLiteColumnExtractor<std::span<const uint8_t>> {
        static std::span<const uint8_t> extract(sqlite3_stmt* stmt, int col) {
            return detail::extract_sqlite_bytes_column(stmt, col);
        }
    };

    template <>
    struct SQLiteColumnExtractor<std::vector<uint8_t>> {
        static std::vector<uint8_t> extract(sqlite3_stmt* stmt, int col) {
            std::vector<uint8_t> res;
            auto bytes = detail::extract_sqlite_bytes_column(stmt, col);
            res.insert(res.end(), bytes.begin(), bytes.end());
            return res;
        }
    };

    template <>
    struct SQLiteColumnExtractor<const_buffer> {
        static const_buffer extract(sqlite3_stmt* stmt, int col) {
            auto bytes = detail::extract_sqlite_bytes_column(stmt, col);
            return buffer(bytes);
        }
    };

    template <class Rep, class Period>
    struct SQLiteColumnExtractor<std::chrono::duration<Rep, Period>> {
        static std::chrono::duration<Rep, Period> extract(sqlite3_stmt* stmt,
                                                          int col) {
            return std::chrono::duration<Rep, Period>{
                detail::extract_sqlite_int64_column(stmt, col)};
        }
    };

    template <>
    struct SQLiteResultStore<void> {
        static void store(sqlite3_context* context, int = 0) {
        }
    };

    template <std::integral T>
    struct SQLiteResultStore<T> {
        static void store(sqlite3_context* context, T value) {
            detail::store_sqlite_int_result(context,
                                            detail::cast_sqlite_arg(value));
        }
    };

    template <std::floating_point T>
    struct SQLiteResultStore<T> {
        static void store(sqlite3_context* context, T value) {
            detail::store_sqlite_double_result(context,
                                               detail::cast_sqlite_arg(value));
        }
    };

    template <>
    struct SQLiteResultStore<std::string_view> {
        static void store(sqlite3_context* context, std::string_view value) {
            detail::store_sqlite_text_result(context, value);
        }
    };

    template <>
    struct SQLiteResultStore<std::string> {
        static void store(sqlite3_context* context, const std::string& value) {
            detail::store_sqlite_text_result(context, value);
        }
    };

    template <>
    struct SQLiteResultStore<std::u16string_view> {
        static void store(sqlite3_context* context, std::u16string_view value) {
            detail::store_sqlite_text16_result(context, value);
        }
    };

    template <>
    struct SQLiteResultStore<std::u16string> {
        static void store(sqlite3_context* context,
                          const std::u16string& value) {
            detail::store_sqlite_text16_result(context, value);
        }
    };

    template <>
    struct SQLiteResultStore<std::span<const uint8_t>> {
        static void store(sqlite3_context* context,
                          std::span<const uint8_t> value) {
            detail::store_sqlite_blob_result(context, buffer(value));
        }
    };

    template <>
    struct SQLiteResultStore<std::vector<uint8_t>> {
        static void store(sqlite3_context* context,
                          const std::vector<uint8_t>& value) {
            detail::store_sqlite_blob_result(context, buffer(value));
        }
    };

    template <>
    struct SQLiteResultStore<const_buffer> {
        static void store(sqlite3_context* context, const_buffer value) {
            detail::store_sqlite_blob_result(context, value);
        }
    };

    template <>
    struct SQLiteResultStore<std::nullptr_t> {
        static void store(sqlite3_context* context, std::nullptr_t) {
            detail::store_sqlite_null_result(context);
        }
    };

    // used to set the index of bind of query's parameters and get of SELECT
    // queries and rows
    struct index {
        int idx;
        index(int i) : idx{i} {
        }
    };

    class row;

    class query;

    class select_range;

    /*!
     * @brief SQLite query prepared statement.
     */
    class query : public trackable {
        friend class database;
        friend class row;

    public:
        /// The type of sqlite statement handle.
        using native_handle_type = stmt_handle_type;

        /*!
         * @brief Construct an empty query.
         * @param db The database the query is attached to.
         */
        query(database& db) noexcept : db_{db} {
        }

        /*!
         * @brief Construct a query and prepare it with a
         * database connection and query statement string. If
         * the query string contains multiple statements only
         * the first one will be used to prepare the query. To
         * execute multiple statements without obtaining the
         * resulting rows use execute() instead.
         * @param db database connection to query on
         * @param query_statement query statement string
         */
        query(database& db, std::string_view query_statement);

        /*!
         * @brief Construct a query and prepare it with a
         * database connection and query statement string.
         * Formatting is done with std::format. Before
         * formatting unsigned 64-bit integers are casted to
         * signed 64-bit integers because sqlite does not
         * support unsigned 64-bit integers and may convert the
         * passed integer to floating point if it does not fit
         * in a signed 64-bit integer. If the query string
         * contains multiple statements only the first one will
         * be used to prepare the query. To execute multiple
         * statements without obtaining the resulting rows use
         * execute() instead
         * @param db database connection to query on
         * @param query_statement statement format string
         * @param ...args arguments to be formatted
         */
        template <class... Args>
        query(database& db, std::string_view query_statement,
              const Args&... args);

        /*!
         * @return reference to the statement handle
         */
        native_handle_type& native_handle() noexcept {
            return stmt_;
        }

        /*!
         * @return const reference to the statement handle
         */
        const native_handle_type& native_handle() const noexcept {
            return stmt_;
        }

        /*!
         * @brief Prepare a sql statement using a database and
         * statement string. If the query string contains
         * multiple statements only the first one will be used
         * to prepare the query. To execute multiple statements
         * without obtaining the resulting rows use execute()
         * instead
         * @param db database connection to query on
         * @param query_statement query statement string
         * @param ec used to report errors if any
         */
        RAD_EXPORT_DECL void prepare(database& db,
                                     std::string_view query_statement,
                                     std::error_code& ec) noexcept;

        /*!
         * @brief Prepare a sql statement using a database and
         * statement string. If the query string contains
         * multiple statements only the first one will be used
         * to prepare the query. To execute multiple statements
         * without obtaining the resulting rows use execute()
         * instead
         * @param db database connection to query on
         * @param query_statement query statement string
         */
        void prepare(database& db, std::string_view query_statement) {
            std::error_code ec;
            prepare(db, query_statement, ec);
            detail::throw_sqlite_exception(db, ec);
        }

    private:
        template <class... Args, std::size_t... I>
        void bind_args(const Args&... args, std::index_sequence<I...>) {
            (bind(I, args), ...);
        }

    public:
        /*!
         * @brief Prepare a sql statement using a database and
         * formatted statement string. Formatting is done using
         * the proper specialization of SQLiteBinder which ends
         * up using sqlite3_bind_*. Before formatting unsigned
         * 64-bit integers are casted to signed 64-bit integers
         * because sqlite does not support unsigned 64-bit
         * integers and may convert the passed integer to
         * floating point if it does not fit in a signed 64-bit
         * integer. If the query string contains multiple
         * statements only the first one will be used to prepare
         * the query. To execute multiple statements without
         * obtaining the resulting rows use execute() instead
         * @param db database connection to query on
         * @param query_statement statement format string
         * @param ...args arguments to be formatted
         */
        template <class... Args>
        void prepare(database& db, std::string_view query_statement,
                     const Args&... args) {
            query new_query{db};
            new_query.prepare(db, query_statement);
            new_query.bind_args<Args...>(
                args..., std::make_index_sequence<sizeof...(args)>{});
            *this = std::move(new_query);
        }

        /*!
         * @brief Execute a query using sqlite3_step. For select
         * queries use next() or select() instead
         * @param ec used to report errors if any
         */
        RAD_EXPORT_DECL void execute(std::error_code& ec) noexcept;

        /*!
         * @brief Execute a query using sqlite3_step. For select
         * queries use next() or select() instead
         */
        void execute() {
            std::error_code ec;
            execute(ec);
            detail::throw_sqlite_exception(db_, ec);
        }

        /*!
         * @brief Bind values to this prepared statement then
         * execute the statement using sqlite3_step
         * @tparam ...Args Types of values to bind
         * @param ...args values to bind
         */
        template <SQLiteBindable... Args>
        void execute_bind(const Args&... args) {
            ((*this << args), ...);
            execute();
        }

        /*!
         * @brief Step to the next row for select queries using
         * sqlite3_step
         * @param ec used to report errors if any
         * @return true if fetched a new row, otherwise false if
         * finished or encountered an error
         */
        RAD_EXPORT_DECL bool next(std::error_code& ec);

        /*!
         * @brief Step to the next row for select queries using
         * sqlite3_step
         * @return true if fetched a new row, otherwise false if
         * finished
         */
        bool next() {
            std::error_code ec;
            bool result = next(ec);
            detail::throw_sqlite_exception(db_, ec);
            return result;
        }

        /*!
         * @brief execute a prepared SELECT query and return a
         * lazy range of the results
         * @param ec used to report errors if any
         * @return range of results which may be empty in case
         * of error or no results
         */
        RAD_EXPORT_DECL select_range select(std::error_code& ec) & noexcept;

        /*!
         * @brief execute a prepared SELECT query and return a
         * lazy range of the results
         * @return range of results which may be empty in case
         * of no results
         */
        select_range select() &;

    private:
        size_t execute_and_return_changes(std::error_code& ec) noexcept;

        size_t execute_and_return_changes() {
            std::error_code ec;
            auto n = execute_and_return_changes(ec);
            detail::throw_sqlite_exception(db_, ec);
            return n;
        }

    public:
        /*!
         * @brief Execute a prepared DELETE query and return the
         * number of rows deleted
         * @param ec to report errors if any
         * @return The number of rows deleted
         */
        size_t delete_rows(std::error_code& ec) noexcept {
            return execute_and_return_changes(ec);
        }

        /*!
         * @brief Execute a prepared DELETE query and return the
         * number of rows deleted
         * @return The number of rows deleted
         */
        size_t delete_rows() {
            return execute_and_return_changes();
        }

        /*!
         * @brief Execute a prepared INSERT query and return the
         * number of rows inserted
         * @param ec to report errors if any
         * @return The number of rows inserted
         */
        size_t insert(std::error_code& ec) noexcept {
            return execute_and_return_changes(ec);
        }

        /*!
         * @brief Execute a prepared INSERT query and return the
         * number of rows inserted
         * @return The number of rows inserted
         */
        size_t insert() {
            return execute_and_return_changes();
        }

        /*!
         * @brief Execute a prepared UPDATE query and return the
         * number of rows updated
         * @param ec to report errors if any
         * @return The number of rows updated
         */
        size_t update(std::error_code& ec) noexcept {
            return execute_and_return_changes(ec);
        }

        /*!
         * @brief Execute a prepared UPDATE query and return the
         * number of rows updated
         * @return The number of rows updated
         */
        size_t update() {
            return execute_and_return_changes();
        }

        /*!
         * @brief Clear set binidings if any using
         * sqlite3_clear_bindings
         */
        RAD_EXPORT_DECL void clear_bindings() noexcept;

        /*!
         * @brief Reset the statement if open to its initial
         * state using sqlite3_reset. This does not clear
         * bindings
         * @param ec used to report errors if any
         */
        RAD_EXPORT_DECL void reset(std::error_code& ec) noexcept;

        /*!
         * @brief Reset the statement if open to its initial
         * state using sqlite3_reset. This does not clear
         * bindings
         */
        void reset() {
            std::error_code ec;
            reset(ec);
            detail::throw_sqlite_exception(db_, ec);
        }

        /*!
         * @brief Close the sql statement if open using
         * sqlite3_finalize
         */
        void close() noexcept {
            stmt_.reset();
        }

        /*!
         * @brief return the count of bind parameters in the
         * prepared statement using sqlite3_bind_parameter_count
         * @return the result of sqlite3_bind_parameter_count
         * which may be 0 if there is no bind parameters.
         */
        RAD_EXPORT_DECL size_t bind_params_count() noexcept;

        /*!
         * @brief Get the zero based index of bind parameter by
         * its name
         * @param name the name of the bind parameter to return
         * its index
         * @return the zero based index of the parameter or
         * nullopt if the satement is not valid or the parameter
         * is not found
         */
        RAD_EXPORT_DECL std::optional<size_t>
        bind_param_index(zstring_view name) noexcept;

        /*!
         * @brief Get the name of the bind parameter by its zero
         * based index
         * @param index the zero based index of bind parameter
         * @return the name of the bind parameter at the given
         * zero based index or an empty string if the index is
         * invalid, the statement is invalid or the parameter at
         * the given index is nameless. The life time of the
         * returned string is managed by the sqlite library and
         * may be invalidated by subsequent sqlite api calls so
         * make sure to make a copy of the string if needed
         */
        RAD_EXPORT_DECL std::string_view param_name_view(size_t index) noexcept;

        /*!
         * @brief Get the name of the bind parameter by its zero
         * based index
         * @param index the zero based index of bind parameter
         * @return the name of the bind parameter at the given
         * zero based index or an empty string if the index is
         * invalid, the statement is invalid or the parameter at
         * the given index is nameless
         */
        std::string param_name(size_t index) {
            return std::string{param_name_view(index)};
        }

        /*!
         * @brief Bind a value to a prepared statement using
         * SQLiteBinder specialization. Note that unsigned
         * 64-bit integers are casted to signed 64-bit integers
         * because sqlite does not support unsigned 64-bit
         * integers and may convert the passed integer to
         * floating point if it does not fit in a signed 64-bit
         * integer
         * @param index the zero based index (not 1 based) of
         * the sql parameter
         * @param val the value to bind to the parameter
         * @param copy whether to make a copy of the data buffer
         * or not. Typically this is only used if the data is
         * string or binary buffer. If data is not copied then
         * the referenced buffer must be valid until bindings
         * are cleared, the same param is bound to another value
         * or the query is closed
         * @param ec used to report errors if any
         */
        template <SQLiteBindable T>
        void bind(std::size_t index, const T& val, bool copy,
                  std::error_code& ec) noexcept {
            ec.clear();
            SQLiteBinder<T>::bind(stmt_.get(), index, val, copy, ec);
        }

        /*!
         * @brief Bind a value to a prepared statement using
         * SQLiteBinder specialization. Note that unsigned
         * 64-bit integers are casted to signed 64-bit integers
         * because sqlite does not support unsigned 64-bit
         * integers and may convert the passed integer to
         * floating point if it does not fit in a signed 64-bit
         * integer
         * @param index the zero based index (not 1 based) of
         * the sql parameter
         * @param val the value to bind to the parameter
         * @param ec used to report errors if any
         */
        template <SQLiteBindable T>
        void bind(std::size_t index, const T& val,
                  std::error_code& ec) noexcept {
            bind(index, val, true, ec);
        }

        /*!
         * @brief Bind a value to a prepared statement using
         * SQLiteBinder specialization. Note that unsigned
         * 64-bit integers are casted to signed 64-bit integers
         * because sqlite does not support unsigned 64-bit
         * integers and may convert the passed integer to
         * floating point if it does not fit in a signed 64-bit
         * integer
         * @param index the zero based index (not 1 based) of
         * the sql parameter
         * @param val the value to bind to the parameter
         * @param copy whether to make a copy of the data buffer
         * or not. Typically this is only used if the data is
         * string or binary buffer. If data is not copied then
         * the referenced buffer must be valid until bindings
         * are cleared, the same param is bound to another value
         * or the query is closed
         */
        template <SQLiteBindable T>
        void bind(std::size_t index, const T& val, bool copy = true) {
            std::error_code ec;
            bind(index, val, copy, ec);
            detail::throw_sqlite_exception(db_, ec);
        }

        /*!
         * @brief Reset the current bind index to i
         * @param i the new zero based (not 1 based) bind index
         * @return the query itself
         */
        query& operator<<(index i) noexcept {
            current_bind_index_ = i.idx;
            return *this;
        }

        /*!
         * @brief Bind a value to a prepared statement using
         * SQLiteBinder trait specialization. The current bind
         * index will be incremented by one if the bind succeeds
         * @param val the value to bind
         * @return the query itself
         */
        template <SQLiteBindable T>
        query& operator<<(const T& val) {
            bind(current_bind_index_, val);
            ++current_bind_index_;
            return *this;
        }

        /*!
         * @brief Get value from the current row result using
         * SQLiteColumnExtractor trait specialization.
         * @tparam T type of value
         * @param index the zero based (not 1 based) get index
         * @param ec used to report errors if any
         * @return the result value
         */
        template <SQLiteColumnValue T>
        T get_value(std::size_t index, std::error_code& ec) const {
            validate_get_index(index, ec);
            if (ec) {
                return {};
            }
            return SQLiteColumnExtractor<T>::extract(stmt_.get(),
                                                     static_cast<int>(index));
        }

        /*!
         * @brief Get value from the current row result using
         * SQLiteColumnExtractor trait specialization.
         * @tparam T type of value
         * @param index the zero based (not 1 based) get index
         * @return the result value
         */
        template <SQLiteColumnValue T>
        T get_value(std::size_t index) const {
            validate_get_index(index);
            return SQLiteColumnExtractor<T>::extract(stmt_.get(),
                                                     static_cast<int>(index));
        }

        /*!
         * @brief Get value from the current row result using
         * SQLiteColumnExtractor trait specialization. The
         * current get index will be incremented by one if the
         * get succeeds
         * @tparam T type of value
         * @param val the value to get
         * @return the query itself
         */
        template <SQLiteColumnValue T>
        query& operator>>(T& val) {
            val = get_value<T>(current_get_index_);
            current_get_index_ += 1;
            return *this;
        }

        /*!
         * @brief Reset the current get index to @p i.
         * @param i the new zero based (not 1 based) get index.
         * @return the query itself
         */
        query& operator>>(index i) noexcept {
            current_get_index_ = i.idx;
            return *this;
        }

        /*!
         * @brief Get the count of columns returned by the
         * prepared statement using sqlite3_column_count
         * @return number of columns in the table, or zero if
         * the statement is not select or the query did not
         * return any columns
         */
        RAD_EXPORT_DECL std::size_t columns_count() const noexcept;

        /*!
         * @brief Get the size in bytes of column at @p index
         * using sqlite3_column_bytes.
         * @param index The 0 based column index.
         * @return The size in bytes of column at @p index.
         */
        RAD_EXPORT_DECL std::size_t
        column_size(std::size_t index) const noexcept;

        /*!
         * @brief Get the type of column at @p index using
         * sqlite3_column_type.
         * @param index The 0 based column index.
         * @return The type of column at @p index.
         */
        RAD_EXPORT_DECL enum column_type
        column_type(std::size_t index) const noexcept;

        /*!
         * @brief Get the name of column at @p index using
         * sqlite3_column_name.
         * @param index The 0 based column index.
         * @return The column name.
         */
        std::string column_name(std::size_t index) const {
            return std::string{column_name_view(index)};
        }

        /*!
         * @brief Get the name of column at @p index using
         * sqlite3_column_name.
         * @param index The 0 based column index.
         * @return View of the column name which is valid as
         * long as the query object is not mutated.
         */
        RAD_EXPORT_DECL std::string_view
        column_name_view(std::size_t index) const noexcept;

    private:
        RAD_EXPORT_DECL void
        validate_get_index(std::size_t index,
                           std::error_code& ec) const noexcept;

        void validate_get_index(std::size_t index) const {
            std::error_code ec;
            validate_get_index(index, ec);
            check_and_throw(ec, "");
        }

        ref<database> db_;
        native_handle_type stmt_;
        uint32_t current_bind_index_ = 0; // sqlite uses 1 based bind index
        uint32_t current_get_index_ = 0;  // sqlite uses 0 based get index
        uint16_t cols_count_ = 0;         // number of columns in  the current
                                          // table, max columns = 32767
        bool has_rows_ = false;           // whether the query execution
                                          // resulted in any result
    };

    /*!
     * @brief A sqlite row is a reference to a query which can be used to
     * only extract values from the current row. This class is usually used
     * with select_range to provide a range based for loop
     */
    class row {
    public:
        /*!
         * @brief Construct a row referencing a query.
         * @param q The query to exctract values from.
         */
        row(query& q) : q_{q} {
        }

        /*!
         * @brief Get the count of columns returned by the
         * prepared statement using sqlite3_column_count.
         * @return number of columns in the table, or zero if
         * the statement is not select or the query did not
         * return any columns.
         */
        std::size_t columns_count() const noexcept {
            return q().columns_count();
        }

        /*!
         * @brief Get the size in bytes of column at @p index
         * using sqlite3_column_bytes.
         * @param index The 0 based column index.
         * @return The size in bytes of column at @p index.
         */
        std::size_t column_size(std::size_t index) const noexcept {
            return q().column_size(index);
        }

        /*!
         * @brief Get the type of column at @p index using
         * sqlite3_column_type.
         * @param index The 0 based column index.
         * @return The type of column at @p index.
         */
        sqlite::column_type column_type(std::size_t index) const noexcept {
            return q().column_type(index);
        }

        /*!
         * @brief Get the name of column at @p index using
         * sqlite3_column_name.
         * @param index The 0 based column index.
         * @return The column name.
         */
        std::string column_name(std::size_t index) const {
            return q().column_name(index);
        }

        /*!
         * @brief Get the name of column at @p index using
         * sqlite3_column_name.
         * @param index The 0 based column index.
         * @return View of the column name which is valid as
         * long as the query object is not mutated.
         */
        std::string_view column_name_view(std::size_t index) const noexcept {
            return q().column_name_view(index);
        }

        /*!
         * @brief Check if column at @p index has integeral
         * type.
         * @param index The 0 based column index.
         * @return True if the column is integral, otherwise
         * false.
         */
        bool is_integer(std::size_t index) const noexcept {
            return column_type(index) == column_type::integeral;
        }

        /*!
         * @brief Check if column at @p index has floating point
         * type.
         * @param index The 0 based column index.
         * @return True if the column is floating point,
         * otherwise false.
         */
        bool is_float(std::size_t index) const noexcept {
            return column_type(index) == column_type::floating_point;
        }

        /*!
         * @brief Check if column at @p index has string type.
         * @param index The 0 based column index.
         * @return True if the column is string, otherwise
         * false.
         */
        bool is_string(std::size_t index) const noexcept {
            return column_type(index) == column_type::text;
        }

        /*!
         * @brief Check if column at @p index has blob type.
         * @param index The 0 based column index.
         * @return True if the column is blob, otherwise false.
         */
        bool is_blob(std::size_t index) const noexcept {
            return column_type(index) == column_type::blob;
        }

        /*!
         * @brief Check if column at @p index has null type.
         * @param index The 0 based column index.
         * @return True if the column is null, otherwise false.
         */
        bool is_null(std::size_t index) const noexcept {
            return column_type(index) == column_type::null;
        }

        /*!
         * @brief Reset the current get index to @p i.
         * @param i the new zero based (not 1 based) get index.
         * @return the row itself.
         */
        row& operator>>(index i) {
            q() >> i;
            return *this;
        }

        /*!
         * @brief Get value from the current row result using
         * SQLiteColumnExtractor trait specialization. The
         * current get index will be incremented by one if the
         * get succeeds.
         * @tparam T type of value
         * @param val the value to get
         * @return the row itself
         */
        template <SQLiteColumnValue T>
        row& operator>>(T& val) {
            q() >> val;
            return *this;
        }

    private:
        friend class select_range;

        ref<query> q_;

        query& q() {
            return *q_;
        }
        const query& q() const {
            return *q_;
        }
    };

    /*!
     * @brief A sqlite select_range is an adapter that uses a reference to a
     * query and provides a range based for loop using the query's next()
     * method
     */
    class select_range {
        struct end_mark {};

        class iterator {
            static constexpr int sqlite3_row_flag = 100; // SQLITE_ROW
        public:
            iterator(query& q, bool finished) : q_{q}, finished_{finished} {
            }

            iterator& operator++() {
                finished_ = !q_->next();
                return *this;
            }

            row operator*() noexcept {
                return row{q_};
            }

            bool operator==(end_mark) const noexcept {
                return finished_;
            }

        private:
            friend select_range;

            ref<query> q_;
            bool finished_;
        };

    public:
        select_range(query& q, bool finished) : begin_iter_{q, finished} {
        }

        iterator begin() const {
            return begin_iter_;
        }

        end_mark end() const {
            return {};
        }

    private:
        iterator begin_iter_;
    };

    inline select_range query::select(std::error_code& ec) & noexcept {
        bool finished = !next(ec);
        return select_range{*this, finished};
    }

    inline select_range query::select() & {
        bool finished = !next();
        return select_range{*this, finished};
    }

    class transaction;

    /// Tag type used to create in memory sqlite databases.
    struct in_memory_database_t {};

    /// used to create in memory sqlite databases.
    inline constexpr in_memory_database_t in_memory_database;

    /*!
     * @brief SQLite database connection.
     */
    class database : public trackable {
        friend class query;

    public:
        using native_handle_type = db_handle_type;

        /*!
         * @brief Default Constructor of the database connection
         * results in non opened database
         */
        database() = default;

        /*!
         * @brief Construct and open the database by a call to
         * open method
         * @param path the path of sqlite database
         * @param flags either read, write, or read_write_create
         * @throws On failure throws an exception of type
         * sqlite::exception
         */
        database(zstring_view path, flags flags = flags::read_write_create) {
            open(path, flags);
        }

        /*!
         * @brief Construct and open an in memory database using
         * sqlite
         * ":memory:" name the result database is both readable
         * and writable
         */
        database(in_memory_database_t) : database(":memory:") {
        }

        /*!
         * @brief Open a new sqlite database connection using
         * sqlite3_open_v2
         * @param path the path of sqlite database
         * @param flags either read, write, or read_write_create
         * @param ec to report errors if any
         */
        RAD_EXPORT_DECL void open(zstring_view path, flags flags,
                                  std::error_code& ec) noexcept;

        /*!
         * @brief Open a new sqlite database connection using
         * sqlite3_open_v2
         * @param path the path of sqlite database
         * @param flags either read, write, or read_write_create
         * @throws On failure throws an exception of type
         * sqlite::exception
         */
        RAD_EXPORT_DECL void open(zstring_view path,
                                  flags flags = flags::read_write_create);

        /*!
         * @brief Open a new sqlite in memory database
         * connection using
         * ":memory:" name the result database is both readable
         * and writable
         * @param pass in_memory_database
         * @param ec to report errors if any
         */
        void open(in_memory_database_t, std::error_code& ec) noexcept {
            open(":memory:", flags::read_write_create, ec);
        }

        /*!
         * @brief Open a new sqlite in memory database
         * connection using
         * ":memory:" name the result database is both readable
         * and writable
         * @param pass in_memory_database
         * @throws On failure throws an exception of type
         * sqlite::exception
         */
        void open(in_memory_database_t) {
            open(":memory:");
        }

        /*!
         * @brief Closes the database connection if open
         */
        void close() noexcept {
            db_.reset();
        }

        /*!
         * @brief Access the native sqlite handle used by the
         * implementation
         * @return the native handle which has the type of
         * native_handle_type defined by the implementation
         */
        native_handle_type& native_handle() noexcept {
            return db_;
        }

        /*!
         * @brief Access the native sqlite handle used by the
         * implementation
         * @return the native handle which has the type of
         * native_handle_type defined by the implementation
         */
        const native_handle_type& native_handle() const noexcept {
            return db_;
        }

        /*!
         * @brief Count the number of rows contained in a table
         * using query 'SELECT COUNT(*)' which may be slow
         * especially for large tables because the sqlite
         * library does a full table scan to count rows
         * @param table_name the name of the table to count rows
         * in. Table names are not validated
         * @param ec to report errors if any
         * @return The number of rows in the table
         */
        size_t count_rows(std::string_view table_name, std::error_code& ec) {
            constexpr std::string_view sql_cmd = "SELECT COUNT(*) FROM ";
            auto q = make_query(sql_cmd + table_name, ec);
            if (ec) {
                return 0;
            }
            q.execute(ec);
            if (ec) {
                return 0;
            }
            return q.get_value<std::size_t>(0);
        }

        /*!
         * @brief Count the number of rows contained in a table
         * using query 'SELECT COUNT(*)' which may be slow
         * especially for large tables because the sqlite
         * library does a full table scan to count rows
         * @param table_name the name of the table to count rows
         * in. Table names are not validated
         * @return The number of rows in the table
         * @throws If failed an exception of type
         * sqlite::exception is thrown
         */
        size_t count_rows(std::string_view table_name) {
            std::error_code ec;
            size_t n = count_rows(table_name, ec);
            throw_if_error(ec);
            return n;
        }

        /*!
         * @brief Get the number of rows affected by the most
         * recently completed INSERT, UPDATE or DELETE statement
         * @param ec to report errors if any
         * @return The number of rows inserted, modified or
         * deleted by the most recent statement
         */
        RAD_EXPORT_DECL size_t changed_rows(std::error_code& ec) noexcept;

        /*!
         * @brief Get the number of rows affected by the most
         * recently completed INSERT, UPDATE or DELETE statement
         * @return The number of rows inserted, modified or
         * deleted by the most recent statement
         * @throws If failed throws an exception of type
         * sqlite::exception
         */
        size_t changed_rows() {
            std::error_code ec;
            size_t n = changed_rows(ec);
            detail::throw_sqlite_exception(*this, ec);
            return n;
        }

        /*!
         * @brief Get the number of rows affected by the all
         * completed INSERT, UPDATE or DELETE statements since
         * the database connection was opened
         * @param ec to report errors if any
         * @return The number of rows inserted, modified or
         * deleted since the database connection was opened
         */
        RAD_EXPORT_DECL size_t total_changed_rows(std::error_code& ec) noexcept;

        /*!
         * @brief Get the number of rows affected by the all
         * completed INSERT, UPDATE or DELETE statements since
         * the database connection was opened
         * @return The number of rows inserted, modified or
         * deleted since the database connection was opened
         * @throws If failed throws an exception of type
         * sqlite::exception
         */
        size_t total_changed_rows() {
            std::error_code ec;
            size_t n = total_changed_rows(ec);
            detail::throw_sqlite_exception(*this, ec);
            return n;
        }

        /*!
         * @brief Get rowid of the last inserted row by a
         * successfull INSERT statement using
         * sqlite3_last_insert_rowid
         * @param ec to report errors if any
         * @return rowid of the last inserted row
         */
        RAD_EXPORT_DECL int64_t last_insert_rowid(std::error_code& ec) noexcept;

        /*!
         * @brief Get rowid of the last inserted row by a
         * successfull INSERT statement using
         * sqlite3_last_insert_rowid
         * @return rowid of the last inserted row
         * @throws If failed throws an exception of type
         * sqlite::exception
         */
        int64_t last_insert_rowid() {
            std::error_code ec;
            int64_t n = last_insert_rowid(ec);
            throw_if_error(ec);
            return n;
        }

        /*!
         * @brief Prepare a qurey using the statement. If the
         * query string contains multiple statements only the
         * first one will be used to prepare the query. To
         * execute multiple statements without obtaining the
         * resulting rows use execute() instead
         * @param query_statement the query statement text
         * @param ec to report errors if any
         * @return Prepared query object which won't take an
         * effect until it is executed
         */
        query make_query(std::string_view query_statement,
                         std::error_code& ec) noexcept {
            query q{*this};
            q.prepare(*this, query_statement, ec);
            return q;
        }

        /*!
         * @brief Prepare a qurey using the statement. If the
         * query string contains multiple statements only the
         * first one will be used to prepare the query. To
         * execute multiple statements without obtaining the
         * resulting rows use execute() instead
         * @param query_statement the query statement text
         * @return Prepared query object which won't take an
         * effect until it is executed
         * @throws If failed throws an exception of type
         * sqlite::exception
         */
        query make_query(std::string_view query_statement) {
            return query{*this, query_statement};
        }

        /*!
         * @brief Prepare a qurey using the statement text, then
         * bind the passed values to the query using
         * SQLiteBinder trait. If the query string contains
         * multiple statements only the first one will be used
         * to prepare the query. To execute multiple statements
         * without obtaining the resulting rows use execute()
         * instead.
         * @param query_statement the query statement text
         * @param ...args values to bind to the prepared
         * statement
         * @return Prepared query object which won't take an
         * effect until it is executed
         * @throws If failed throws an exception of type
         * sqlite::exception
         */
        template <class... Args>
        query make_query(std::string_view query_statement,
                         const Args&... args) {
            return query{*this, query_statement, args...};
        }

        /*!
         * @brief Execute one or multiple statements separated
         * by semicolons and discard the results of each one. If
         * an error occurs the function returns without
         * attempting to execute the remaining statements.
         * @param statements one or multiple sql statements
         * separated by semicolons to execute in order
         * @param ec to report errors if any
         * @return count of statments executed successfully
         */
        RAD_EXPORT_DECL size_t execute(std::string_view statements,
                                       std::error_code& ec) noexcept;

        /*!
         * @brief Execute one or multiple statements separated
         * by semicolons and discard the results of each one. If
         * an error occurs the function returns without
         * attempting to execute the remaining statements.
         * @param statements one or multiple sql statements
         * separated by semicolons to execute in order
         * @return count of statments executed successfully
         */
        size_t execute(std::string_view statements) {
            std::error_code ec;
            size_t n = execute(statements, ec);
            throw_if_error(ec);
            return n;
        }

        /*!
         * @brief Check whether the database is open
         * @return true if open otherwise false
         */
        bool is_open() const noexcept {
            return static_cast<bool>(db_);
        }

        /*!
         * @brief Check whether the database is open
         * @return true if open otherwise false
         */
        explicit operator bool() const noexcept {
            return is_open();
        }

        /*!
         * @brief Starts a transaction on the database
         * @return transaction object which can be used to
         * rollback or commit the transaction
         */
        transaction make_transaction();

        /*!
         * @brief Add a sqlite defined application function
         * using sqlite3_create_function_v2.
         * @param name the name of the function which must not
         * exceed 255 utf8 bytes
         * @param f the function to define. Its return type must
         * be void or a sqlite value compatible type or
         * convertible to one. The parameters types must be
         * extractable from sqlite values. The max number of
         * parameters count is 127
         * @param ec to report errors if any
         */
        template <SQLiteFunction Fn>
        void define_fn(zstring_view name, Fn f, std::error_code& ec) {
            using Ret = typename function_traits<Fn>::result_type;
            using args_tuple = typename function_traits<Fn>::args_tuple;
            constexpr std::size_t args_n = function_traits<Fn>::args_n;
            // the max length of name is 255 bytes in utf8
            // and max count of args is 127
            constexpr std::size_t max_args_n = 127;
            static_assert(args_n <= max_args_n, "the max count of sqlite "
                                                "function arguments "
                                                "can't exceed 127");
            ec.clear();
            Fn* fn_ptr = new (std::nothrow) Fn{std::move(f)};
            if (!fn_ptr) {
                ec = std::make_error_code(std::errc::not_enough_memory);
                return;
            }
            auto destroy_fn = [](void* p) { delete static_cast<Fn*>(p); };
            create_sql_fn(
                name, static_cast<int>(args_n), fn_ptr,
                detail::sqlite_fn_wrapper<Fn, Ret, args_tuple, args_n>,
                destroy_fn, ec);
        }

        /*!
         * @brief Add a sqlite defined application function
         * using sqlite3_create_function_v2.
         * @param name the name of the function which must not
         * exceed 255 utf8 bytes
         * @param f the function to define. Its return type must
         * be void or a sqlite value compatible type or
         * convertible to one. The parameters types must be
         * extractable from sqlite values. The max number of
         * parameters count is 127
         */
        template <SQLiteFunction Fn>
        void define_fn(zstring_view name, Fn f) {
            std::error_code ec;
            define_fn(name, std::move(f), ec);
            throw_if_error(ec);
        }

    private:
        RAD_EXPORT_DECL const char* last_error_msg() const noexcept;

        using sqlite_fn_t = void (*)(sqlite3_context*, int, sqlite3_value**);
        using sqlite_destroy_t = void (*)(void*);

        RAD_EXPORT_DECL void create_sql_fn(zstring_view name, int args_n,
                                           void* appdata, sqlite_fn_t func,
                                           sqlite_destroy_t destroy,
                                           std::error_code& ec) noexcept;

        void throw_if_error(const std::error_code& ec) {
            if (ec) {
                throw std::system_error(ec, last_error_msg());
            }
        }

        friend void detail::throw_sqlite_exception(database&,
                                                   const std::error_code&);

        native_handle_type db_;
    };

    inline query::query(database& db, std::string_view query_statement)
        : db_{db} {
        prepare(db, query_statement);
    }

    template <class... Args>
    inline query::query(database& db, std::string_view query_statement,
                        const Args&... args)
        : db_{db} {
        prepare(db, query_statement, args...);
    }

    inline size_t
    query::execute_and_return_changes(std::error_code& ec) noexcept {
        ec.clear();
        execute(ec);
        if (ec) {
            return 0;
        }
        return db_->changed_rows(ec);
    }

    /*!
     * @brief SQLite database transaction.
     * To execute queries inside a transaction:
     *
     * Construct a transaction using the open database reference.
     *
     * Execute queries on the database.
     *
     * call commit() method to commit the changes made by the queries,
     * and call rollback() to discard the changes.
     *
     * If the transaction object is destroyed before commit, it is roll
     * backed in the destructor.
     */
    class transaction {
        ref<database> db_;
        bool owns_lock = true;

    public:
        /*!
         * @brief Construct and begin a transaction using BEGIN
         * TRANSACTION query.
         * @param db The database to make a transaction for.
         */
        transaction(database& db) : db_{db} {
            // begin the transaction
            db.make_query("BEGIN TRANSACTION;").execute();
        }

        /*!
         * @brief Move construct from @p other.
         * After move other does not hold a transaction.
         * @param other The other transaction object.
         */
        transaction(transaction&& other) noexcept
            : db_{other.db_}, owns_lock{std::exchange(other.owns_lock, false)} {
        }

        /*!
         * @brief Move assign from @p other.
         * Any pending transaction is rolled back.
         * After move other does not hold a transaction.
         * @param other The other transaction object.
         * @return Reference to self.
         */
        transaction& operator=(transaction&& other) noexcept {
            if (std::addressof(other) == this) {
                return *this;
            }
            rollback();
            db_ = other.db_;
            owns_lock = std::exchange(other.owns_lock, false);
            return *this;
        }

        /*!
         * @brief Commit a transaction if there is one.
         * @param ec Set to indicate errors, if any.
         */
        void commit(std::error_code& ec) noexcept {
            ec.clear();
            if (owns_lock && db_->is_open()) {
                auto q = db_->make_query("COMMIT;", ec);
                if (ec) {
                    return;
                }
                q.execute(ec);
                if (!ec) {
                    owns_lock = false;
                }
            }
        }

        /*!
         * @brief Commit a transaction if there is one.
         */
        void commit() {
            if (owns_lock) {
                db_->make_query("COMMIT;").execute();
                owns_lock = false;
            }
        }

        /*!
         * @brief Roll back a transaction if there is one.
         */
        void rollback() noexcept {
            if (owns_lock && db_->is_open()) {
                std::error_code ec;
                auto q = db_->make_query("ROLLBACK;", ec);
                assert(!ec && "the transaction rollback "
                              "failed !");
                if (ec) {
                    return;
                }
                q.execute(ec);
                assert(!ec && "the transaction rollback "
                              "failed !");
            }
            owns_lock = false;
        }

        ~transaction() {
            rollback();
        }
    };

    inline transaction database::make_transaction() {
        return {*this};
    }

    namespace detail {
        inline void throw_sqlite_exception(database& db,
                                           const std::error_code& ec) {
            db.throw_if_error(ec);
        }
    } // namespace detail

}; // namespace RAD_LIB_NAMESPACE::sqlite
