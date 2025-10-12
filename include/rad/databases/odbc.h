#pragma once
#include <rad/libbase.h>
#include <rad/string.h>

#include <array>
#include <chrono>
#include <memory>
#include <optional>
#include <span>
#include <vector>

namespace RAD_LIB_NAMESPACE::odbc {
    using wide_char_t = char16_t;
    using wide_string_view = std::basic_string_view<wide_char_t>;
    using wide_string = std::basic_string<wide_char_t>;

    template <class T>
    inline void to_wide_string(std::string_view from,
                               std::basic_string<T>& to) {
        static_assert(std::is_same_v<T, wide_char_t>,
                      "only wide_char_t is supported");
        if constexpr (sizeof(wide_char_t) == sizeof(char16_t)) {
            to_u16string(from, to);
        }
        else if constexpr (sizeof(wide_char_t) == sizeof(char32_t)) {
            to_u32string(from, to);
        }
    }

    namespace detail {
        struct env_handle_deleter {
            using pointer = void*;
            RAD_EXPORT_DECL void operator()(pointer ptr) const noexcept;
        };

        struct db_handle_deleter {
            bool connected = false;

            using pointer = void*;
            RAD_EXPORT_DECL void operator()(pointer ptr) const noexcept;
        };

        struct stmt_handle_deleter {
            using pointer = void*;
            RAD_EXPORT_DECL void operator()(pointer ptr) const noexcept;
        };
    } // namespace detail

    /*!
     * @brief ODBC Connection data source in UTF-8.
     */
    struct data_source_utf8 {
        std::string driver;
        std::string description;
    };

    /*!
     * @brief ODBC Connection data source.
     */
    struct data_source {
        wide_string driver;
        wide_string description;

        data_source() = default;

        data_source(const wide_string& driver, const wide_string& description)
            : driver{driver}, description{description} {
        }

        data_source_utf8 to_utf8() const {
            return {to_string(driver), to_string(description)};
        }
    };

    /*!
     * @brief ODBC Connection pooling.
     */
    enum class connection_pooling {
        none,
        per_driver,
        per_environment,
        driver_aware,
    };

    /*!
     * @brief ODBC Connection pooling match.
     */
    enum class connection_pooling_match {
        strict,
        relaxed,
    };

    /*!
     * @brief ODBC environment version.
     */
    enum class environment_version {
        v2 = 2,
        v3 = 3,
        v3_8 = 380,
    };

    /*!
     * @brief ODBC driver output string.
     */
    enum class driver_output_string {
        not_null_terminated,
        null_terminated,
    };

    /*!
     * @brief Set the global connection pooling.
     *
     * The default behavior is to disable connection pooling.
     *
     * This function must be called before the desired environments are created
     * to take effect.
     * @param pooling The connection pooling.
     */
    RAD_EXPORT_DECL void set_connection_pooling(connection_pooling pooling);

    /*!
     * @brief ODBC SQL environment.
     */
    class environment {
    public:
        /*!
         * @brief The type of the wrapper handle.
         */
        using native_handle_type =
            std::unique_ptr<void, detail::env_handle_deleter>;

        /*!
         * @brief Create ODBC SQL environment.
         */
        RAD_EXPORT_DECL environment();

        /*!
         * @brief Get a reference to the wrapper handler.
         * @return A reference to the wrapper handler.
         */
        native_handle_type& native_handle() noexcept {
            return handle_;
        }

        /*!
         * @brief Get a const reference to the wrapper handler.
         * @return A const reference to the wrapper handler.
         */
        const native_handle_type& native_handle() const noexcept {
            return handle_;
        }

        /*!
         * @brief Get the user and system data sources from the environment.
         * @param data_sources The user and system data sources from the
         * environment.
         */
        void sources(std::vector<data_source>& data_sources) const {
            list_sources(data_sources, true, true);
        }

        /*!
         * @brief Get the user data sources from the environment.
         * @param data_sources The user data sources from the
         * environment.
         */
        void user_sources(std::vector<data_source>& data_sources) const {
            list_sources(data_sources, true, false);
        }

        /*!
         * @brief Get the system data sources from the environment.
         * @param data_sources system data sources from the
         * environment.
         */
        void system_sources(std::vector<data_source>& data_sources) const {
            list_sources(data_sources, false, true);
        }

        /*!
         * @brief Get the user and system data sources from the environment.
         * @return The user and system data sources from the
         * environment.
         */
        std::vector<data_source> sources() const {
            std::vector<data_source> data_sources;
            sources(data_sources);
            return data_sources;
        }

        /*!
         * @brief Get the user data sources from the environment.
         * @return The user data sources from the
         * environment.
         */
        std::vector<data_source> user_sources() const {
            std::vector<data_source> data_sources;
            user_sources(data_sources);
            return data_sources;
        }

        /*!
         * @brief Get the system data sources from the environment.
         * @return The system data sources from the
         * environment.
         */
        std::vector<data_source> system_sources() const {
            std::vector<data_source> data_sources;
            system_sources(data_sources);
            return data_sources;
        }

        /*!
         * @brief Set the connection pooling on this environment.
         *
         * The default behavior is to disable connection pooling.
         * @param pooling The connection pooling.
         */
        RAD_EXPORT_DECL void set_connection_pooling(connection_pooling pooling);

        /*!
         * @brief Set the connection pooling match behavior on this environment.
         *
         * The default behavior is strict matching.
         * @param pooling_match The connection pooling match behavior.
         */
        RAD_EXPORT_DECL void
        set_connection_pooling_match(connection_pooling_match pooling_match);

        /*!
         * @brief Set the ODBC version behavior.
         * @param v The ODBC version behavior.
         */
        RAD_EXPORT_DECL void set_version(environment_version v);

        /*!
         * @brief Set how the driver returns string data.
         *
         * The default is null terminated string.
         * @param s How the driver returns string data.
         */
        RAD_EXPORT_DECL void set_driver_output_string(driver_output_string s);

    private:
        RAD_EXPORT_DECL void list_sources(std::vector<data_source>& sources,
                                          bool user, bool system) const;

        void throw_if_error(short ret, const char* msg) const;

        native_handle_type handle_;
    };

    class database;

    /*!
     * @brief The type of SQL_C_GUID.
     */
    struct guid_t {
        uint32_t data1 = 0;
        uint16_t data2 = 0;
        uint16_t data3 = 0;
        uint8_t data4[8];
    };

    /*!
     * @brief The type of SQL_C_TYPE_TIME.
     */
    struct time_t {
        uint16_t hour = 0;
        uint16_t minute = 0;
        uint16_t second = 0;
    };

    /*!
     * @brief The type of SQL_C_TYPE_DATE.
     */
    struct date_t {
        int16_t year = 0;
        uint16_t month = 0;
        uint16_t day = 0;
    };

    /*!
     * @brief The type of SQL_C_TYPE_TIMESTAMP.
     */
    struct timestamp_t {
        int16_t year = 0;
        uint16_t month = 0;
        uint16_t day = 0;

        uint16_t hour = 0;
        uint16_t minute = 0;
        uint16_t second = 0;

        uint32_t fraction = 0;
    };

    class statement;

    template <class T>
    struct ColumnExtractor;

    template <class T>
    struct Binder;

    template <class T>
    concept ColumnValue = requires(statement& stmt, std::size_t col) {
        typename ColumnExtractor<T>;
        { ColumnExtractor<T>::extract(stmt, col) } -> std::same_as<T>;
    };

    template <class T>
    concept Bindable =
        requires(statement& stmt, std::size_t col, const T& value, bool copy) {
            typename Binder<T>;
            Binder<T>::bind(stmt, col, value, copy);
        };

    struct index {
        std::size_t idx;
        index(std::size_t i) : idx{i} {
        }
    };

    template <class T>
    class as_view {
    public:
        using value_type = T;

        as_view(const T& val) noexcept : val{val} {
        }

        const T& value() const noexcept {
            return val;
        }

    private:
        const T& val;
    };

    class select_range;

    /*!
     * @brief ODBC SQL query prepared statement.
     *
     * Use `database.prepare()` method to make a prepared statement.
     */
    class statement {
        struct bound_param {
            uint16_t index = 0; // 0 based
            short sql_type = 0;
            short c_type = 0;
            size_t sql_size = 0;
            intptr_t size = 0;
            short scale = 0;
            union {
                int32_t i32_val;
                uint32_t u32_val;
                int64_t i64_val;
                uint64_t u64_val;
                double dval;
                guid_t guid_val;
                time_t time_val;
                date_t date_val;
                timestamp_t timestamp_val;
                uint8_t data_bytes[28];
            };
            std::span<const uint8_t> view_data;
            std::vector<uint8_t> owned_data;

            bound_param() {
            }

            void clear() noexcept {
                view_data = {};
                owned_data.clear();
            }
        };

        struct bound_column {
            enum bound_column_flags : uint8_t {
                none = 0,
                variable_len = 1 << 0,
                nullable = 1 << 1,
                result = 1 << 2,
                null = 1 << 3,
                converted_to_string = 1 << 4,
                converted_to_wstring = 1 << 5,
                bound = 1 << 6,
            };

            uint16_t index = 0; // 0 based
            int16_t c_type = 0;
            size_t c_len = 0;
            uint8_t flags = bound_column_flags::none;

            std::vector<uint8_t> blob;
            std::vector<uint8_t> get_data_buff;
            wide_string converted_wstring;
            std::string converted_string;

            union {
                int64_t i64_val;
                uint64_t u64_val;
                double dval;
                guid_t guid_val;
                time_t time_val;
                date_t date_val;
                timestamp_t timestamp_val;

                uint8_t data_bytes[28];
            };

            union {
                intptr_t blob_size = 0;
                intptr_t null_indicator;
            };

            bound_column() {
            }

            bool is_bound() const noexcept {
                return flags & bound_column_flags::bound;
            }

            bool is_variable_len() const noexcept {
                return flags & bound_column_flags::variable_len;
            }

            bool is_nullable() const noexcept {
                return flags & bound_column_flags::nullable;
            }

            bool has_result() const noexcept {
                return flags & bound_column_flags::result;
            }

            bool is_null() const noexcept {
                return flags & bound_column_flags::null;
            }

            bool has_string() const noexcept {
                return flags & bound_column_flags::converted_to_string;
            }

            bool has_wstring() const noexcept {
                return flags & bound_column_flags::converted_to_wstring;
            }

            void set_bound() noexcept {
                flags |= bound_column_flags::bound;
            }

            void set_variable_len() noexcept {
                flags |= bound_column_flags::variable_len;
            }

            void set_has_result() noexcept {
                flags |= bound_column_flags::result;
            }

            void set_null() noexcept {
                flags |= bound_column_flags::null;
            }

            void clear() noexcept;

            std::span<const uint8_t> get_blob_data() const;

            std::string_view get_string_view_data() const;

            wide_string_view get_wstring_view_data() const;

            void convert_to_string();

            void convert_to_wstring();
        };

        struct result_column {
            uint16_t index = 0; // 0 based
            short sql_type = 0;
            size_t sql_size = 0;
            short scale = 0;
            short c_type = 0;
            intptr_t c_size = 0;
            wide_string name;
        };

        friend class database;

    public:
        /*!
         * @brief The type of the wrapper handle.
         */
        using native_handle_type =
            std::unique_ptr<void, detail::stmt_handle_deleter>;

        RAD_EXPORT_DECL statement(native_handle_type handle,
                                  uint32_t get_data_exts);

        /*!
         * @brief Get a reference to the wrapper handler.
         * @return A reference to the wrapper handler.
         */
        native_handle_type& native_handle() noexcept {
            return handle_;
        }

        /*!
         * @brief Get a const reference to the wrapper handler.
         * @return A const reference to the wrapper handler.
         */
        const native_handle_type& native_handle() const noexcept {
            return handle_;
        }

        /*!
         * @brief Get the count of columns returned by the
         * prepared statement using `SQLNumResultCols`.
         * @return number of columns in the table, or zero if
         * the statement is not select or the query did not
         * return any columns
         */
        RAD_EXPORT_DECL std::size_t columns_count() const;

        /*!
         * @brief return the count of bind parameters in the
         * prepared statement using `SQLNumParams`.
         * @return the result of `SQLNumParams`.
         * which may be 0 if no bind parameters.
         */
        RAD_EXPORT_DECL std::size_t bind_params_count();

        /*!
         * @brief Clear set binidings if any using
         * `SQLFreeStmt`.
         */
        RAD_EXPORT_DECL void clear_bindings();

        /*!
         * @brief Get the name of column at @p index.
         * @param index The 0 based column index.
         * @return The column name.
         */
        std::string column_name(std::size_t i) const {
            return to_string(result_cols_.at(0).name);
        }

        /*!
         * @brief Execute a query using `SQLExecute`. For select
         * queries use next() or select() instead
         */
        RAD_EXPORT_DECL void execute();

        /*!
         * @brief Execute a prepared SELECT query and return a
         * lazy range of the results.
         * @return A range of results which may be empty in case
         * of no results.
         */
        select_range select() &;

        RAD_EXPORT_DECL void execute_select();

        /*!
         * @brief Step to the next row for select queries using
         * `SQLFetch`.
         * @return true if fetched a new row, otherwise false if
         * finished.
         */
        RAD_EXPORT_DECL bool next();

        bool is_null(size_t i) const {
            return cols_.at(i).is_null();
        }

        RAD_EXPORT_DECL std::optional<int64_t> get_i64_value(size_t i);

        RAD_EXPORT_DECL std::optional<double> get_double_value(size_t i);

        RAD_EXPORT_DECL std::optional<std::string_view>
        get_string_value(size_t i);

        RAD_EXPORT_DECL std::optional<wide_string_view>
        get_wstring_value(size_t i);

        RAD_EXPORT_DECL std::optional<std::span<const uint8_t>>
        get_binary_value(size_t i);

        RAD_EXPORT_DECL std::optional<time_t> get_time_value(size_t i);

        RAD_EXPORT_DECL std::optional<date_t> get_date_value(size_t i);

        RAD_EXPORT_DECL std::optional<timestamp_t>
        get_timestamp_value(size_t i);

        /*!
         * @brief Get value from the current row result using
         * `ColumnExtractor` trait specialization.
         * @tparam T type of value
         * @param index the zero based (not 1 based) get index
         * @return the result value
         */
        template <ColumnValue T>
        T get_value(std::size_t index) {
            return ColumnExtractor<T>::extract(*this, index);
        }

        /*!
         * @brief Get value from the current row result using
         * `ColumnExtractor` trait specialization. The
         * current get index will be incremented by one if the
         * get succeeds
         * @tparam T type of value
         * @param val the value to get
         * @return the query itself
         */
        template <ColumnValue T>
        statement& operator>>(T& val) {
            val = get_value<T>(current_get_index_);
            current_get_index_ += 1;
            return *this;
        }

        /*!
         * @brief Reset the current get index to @p i.
         * @param i the new zero based (not 1 based) get index.
         * @return the query itself
         */
        statement& operator>>(index i) noexcept {
            current_get_index_ = i.idx;
            return *this;
        }

        /*!
         * @brief Bind a value to a prepared statement using
         * `Binder` specialization.
         * @param index the zero based index (not 1 based) of
         * the sql parameter.
         * @param val the value to bind to the parameter
         * @param copy whether to make a copy of the data buffer
         * or not. Typically this is only used if the data is
         * string or binary buffer. If data is not copied then
         * the referenced buffer must be valid until bindings
         * are cleared, the same param is bound to another value
         * or the query is closed
         */
        template <Bindable T>
        void bind(std::size_t index, const T& value, bool copy = true) {
            Binder<T>::bind(*this, index, value, copy);
        }

        /*!
         * @brief Bind a value to a prepared statement using
         * `Binder` trait specialization. The current bind
         * index will be incremented by one if the bind succeeds
         * @param val the value to bind
         * @return the query itself
         */
        template <Bindable T>
        statement& operator<<(const T& value) {
            Binder<T>::bind(*this, current_bind_index_, value, true);
            current_bind_index_ += 1;
            return *this;
        }

        /*!
         * @brief Reset the current bind index to i
         * @param i the new zero based (not 1 based) bind index
         * @return the query itself
         */
        statement& operator<<(index i) noexcept {
            current_bind_index_ = i.idx;
            return *this;
        }

        std::span<const result_column> result_columns() const {
            return result_cols_;
        }

        RAD_EXPORT_DECL void bind_int32(size_t i, int32_t val);

        RAD_EXPORT_DECL void bind_uint32(size_t i, uint32_t val);

        RAD_EXPORT_DECL void bind_int64(size_t i, int64_t val);

        RAD_EXPORT_DECL void bind_uint64(size_t i, uint64_t val);

        RAD_EXPORT_DECL void bind_double(size_t i, double val);

        RAD_EXPORT_DECL void bind_time(size_t i, time_t val);

        RAD_EXPORT_DECL void bind_date(size_t i, date_t val);

        RAD_EXPORT_DECL void bind_timestamp(size_t i, const timestamp_t& val);

        RAD_EXPORT_DECL void bind_string(size_t i, std::string_view val,
                                         bool copy);

        RAD_EXPORT_DECL void bind_wstring(size_t i, wide_string_view val,
                                          bool copy);

        RAD_EXPORT_DECL void bind_binary(size_t i, std::span<const uint8_t> val,
                                         bool copy);

        RAD_EXPORT_DECL void bind_null(size_t i);

    private:
        void throw_if_error(short ret, const char* msg) const;

        bound_param describe_param(uint16_t index);

        result_column describe_column(uint16_t index0);

        void get_columns_descriptions();

        void bind_result_columns();

        void get_results_until(size_t i);

        void get_column_result(bound_column& col);

        bool get_blob_result(short index1, short c_type,
                             std::vector<uint8_t>& blob);

        void get_params_descriptions();

        void bind_param(bound_param& param, void* val_ptr);

        native_handle_type handle_;
        std::vector<result_column> result_cols_;
        std::vector<bound_column> cols_;
        std::vector<bound_param> bound_params_;
        uint32_t get_data_exts_ = 0;
        std::size_t current_bind_index_ = 0; // sql uses 1 based bind index
        std::size_t current_get_index_ = 0;  // sql uses 0 based get index
    };

    /*!
     * @brief A SQL row is a reference to a query which can be used to
     * only extract values from the current row. This class is usually used
     * with select_range to provide a range based for loop.
     */
    class row {
    public:
        /*!
         * @brief Construct a row referencing a query.
         * @param q The query to exctract values from.
         */
        row(statement& q) : stmt_{q} {
        }

        /*!
         * @brief Get the count of columns returned by the
         * prepared statement using sqlite3_column_count.
         * @return number of columns in the table, or zero if
         * the statement is not select or the query did not
         * return any columns.
         */
        std::size_t columns_count() const {
            return q().columns_count();
        }

        /*!
         * @brief Get the name of column at @p index.
         * @param index The 0 based column index.
         * @return The column name.
         */
        std::string column_name(std::size_t index) const {
            return q().column_name(index);
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
         * `ColumnExtractor` trait specialization. The
         * current get index will be incremented by one if the
         * get succeeds.
         * @tparam T type of value
         * @param val the value to get
         * @return the row itself
         */
        template <ColumnValue T>
        row& operator>>(T& val) {
            q() >> val;
            return *this;
        }

    private:
        statement& stmt_;

        statement& q() const noexcept {
            return stmt_;
        }
    };

    /*!
     * @brief A select_range is an adapter that uses a reference to a
     * query and provides a range based for loop using the query's next()
     * method.
     */
    class select_range {
        struct end_mark {};

        class iterator {
            static constexpr int sqlite3_row_flag = 100; // SQLITE_ROW
        public:
            iterator(statement& q, bool finished) : q_{q}, finished_{finished} {
            }

            iterator& operator++() {
                finished_ = !q_.next();
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

            statement& q_;
            bool finished_;
        };

    public:
        select_range(statement& q, bool finished) : begin_iter_{q, finished} {
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

    inline select_range statement::select() & {
        execute();
        bool finished = !next();
        return select_range{*this, finished};
    }

    /*!
     * @brief ODBC database connection.
     */
    class database {
        friend class statement;

    public:
        /*!
         * @brief The type of the wrapper handle.
         */
        using native_handle_type =
            std::unique_ptr<void, detail::db_handle_deleter>;

        /*!
         * @brief Create a database.
         * The database is initially not connected.
         */
        database() {
            environment env;
            init(env);
        }

        /*!
         * @brief Create a database using an existing environment.
         * The database is initially not connected.
         * @param env An existing environment to create the database with.
         */
        database(environment& env) {
            init(env);
        }

        /*!
         * @brief Get a reference to the wrapper handler.
         * @return A reference to the wrapper handler.
         */
        native_handle_type& native_handle() noexcept {
            return handle_;
        }

        /*!
         * @brief Get a const reference to the wrapper handler.
         * @return A const reference to the wrapper handler.
         */
        const native_handle_type& native_handle() const noexcept {
            return handle_;
        }

        /*!
         * @brief Connect to a data source using a connection string.
         * @param conn_str The connection string.
         */
        RAD_EXPORT_DECL void connect(wide_string_view conn_str);

        /*!
         * @brief Connect to a data source using a connection string.
         * @param conn_str The connection string encoded in UTF-8.
         */
        void connect(std::string_view conn_str) {
            wide_string wconn_str;
            to_wide_string(conn_str, wconn_str);
            connect(wconn_str);
        }

        /*!
         * @brief Close the connection.
         */
        RAD_EXPORT_DECL void disconnect();

        /*!
         * @brief Prepare a qurey using the statement.
         * @param query_statement the query statement text
         * @return Prepared query object which won't take an
         * effect until it is executed
         */
        RAD_EXPORT_DECL statement prepare(wide_string_view query_statement);

        /*!
         * @brief Retrieve the GETDATA extensions using `SQLGetInfoW` with
         * `SQL_GETDATA_EXTENSIONS`.
         * @return The GETDATA extensions.
         */
        RAD_EXPORT_DECL uint32_t get_data_extensions() const;

    private:
        RAD_EXPORT_DECL void init(environment& env);

        void throw_if_error(short ret, const char* msg) const;

        native_handle_type handle_;
        uint32_t get_data_exts_ = 0;
    };

    template <std::integral T>
    struct ColumnExtractor<T> {
        static T extract(statement& stmt, std::size_t col) {
            return static_cast<T>(stmt.get_i64_value(col).value_or(0));
        }
    };

    template <std::floating_point T>
    struct ColumnExtractor<T> {
        static T extract(statement& stmt, std::size_t col) {
            return static_cast<T>(stmt.get_double_value(col).value_or(0));
        }
    };

    template <>
    struct ColumnExtractor<std::string_view> {
        static std::string_view extract(statement& stmt, std::size_t col) {
            return stmt.get_string_value(col).value_or("");
        }
    };

    template <>
    struct ColumnExtractor<std::string> {
        static std::string extract(statement& stmt, std::size_t col) {
            return std::string{
                ColumnExtractor<std::string_view>::extract(stmt, col)};
        }
    };

    template <>
    struct ColumnExtractor<std::span<const uint8_t>> {
        static std::span<const uint8_t> extract(statement& stmt,
                                                std::size_t col) {
            return stmt.get_binary_value(col).value_or(
                std::span<const uint8_t>{});
        }
    };

    template <>
    struct ColumnExtractor<std::vector<uint8_t>> {
        static std::vector<uint8_t> extract(statement& stmt, std::size_t col) {
            auto bytes =
                ColumnExtractor<std::span<const uint8_t>>::extract(stmt, col);
            return std::vector<uint8_t>{bytes.begin(), bytes.end()};
        }
    };

    template <>
    struct ColumnExtractor<time_t> {
        static time_t extract(statement& stmt, std::size_t col) {
            return stmt.get_time_value(col).value_or(time_t{});
        }
    };

    template <>
    struct ColumnExtractor<date_t> {
        static date_t extract(statement& stmt, std::size_t col) {
            return stmt.get_date_value(col).value_or(date_t{});
        }
    };

    template <>
    struct ColumnExtractor<timestamp_t> {
        static timestamp_t extract(statement& stmt, std::size_t col) {
            return stmt.get_timestamp_value(col).value_or(timestamp_t{});
        }
    };

    template <class Rep, class Period>
    struct ColumnExtractor<std::chrono::duration<Rep, Period>> {
        static std::chrono::duration<Rep, Period> extract(statement& stmt,
                                                          std::size_t col) {
            using namespace std::chrono;

            auto t = ColumnExtractor<time_t>::extract(stmt, col);
            uint64_t time_secs = t.second;
            time_secs += (uint64_t)t.minute * 60;
            time_secs += (uint64_t)t.hour * 60 * 60;

            using result_duration = std::chrono::duration<Rep, Period>;
            return duration_cast<result_duration>(
                seconds{static_cast<int64_t>(time_secs)});
        }
    };

    template <ColumnValue T>
    struct ColumnExtractor<std::optional<T>> {
        static std::optional<T> extract(statement& stmt, std::size_t col) {
            if (stmt.is_null(col)) {
                return std::nullopt;
            }
            return ColumnExtractor<T>::extract(stmt, col);
        }
    };

    template <std::signed_integral T>
    struct Binder<T> {
        static void bind(statement& stmt, std::size_t col, T value, bool copy) {
            std::ignore = copy;
            stmt.bind_int64(col, static_cast<int64_t>(value));
        }
    };

    template <std::unsigned_integral T>
    struct Binder<T> {
        static void bind(statement& stmt, std::size_t col, T value, bool copy) {
            std::ignore = copy;
            stmt.bind_uint64(col, static_cast<uint64_t>(value));
        }
    };

    template <std::floating_point T>
    struct Binder<T> {
        static void bind(statement& stmt, std::size_t col, T value, bool copy) {
            std::ignore = copy;
            stmt.bind_double(col, static_cast<double>(value));
        }
    };

    template <>
    struct Binder<std::nullptr_t> {
        static void bind(statement& stmt, std::size_t col, std::nullptr_t,
                         bool copy) {
            std::ignore = copy;
            stmt.bind_null(col);
        }
    };

    template <>
    struct Binder<time_t> {
        static void bind(statement& stmt, std::size_t col, time_t value,
                         bool copy) {
            std::ignore = copy;
            stmt.bind_time(col, value);
        }
    };

    template <>
    struct Binder<date_t> {
        static void bind(statement& stmt, std::size_t col, date_t value,
                         bool copy) {
            std::ignore = copy;
            stmt.bind_date(col, value);
        }
    };

    template <>
    struct Binder<timestamp_t> {
        static void bind(statement& stmt, std::size_t col,
                         const timestamp_t& value, bool copy) {
            std::ignore = copy;
            stmt.bind_timestamp(col, value);
        }
    };

    template <>
    struct Binder<std::string_view> {
        static void bind(statement& stmt, std::size_t col,
                         std::string_view value, bool copy) {
            stmt.bind_string(col, value, copy);
        }
    };

    template <>
    struct Binder<std::string> {
        static void bind(statement& stmt, std::size_t col,
                         const std::string& value, bool copy) {
            Binder<std::string_view>::bind(stmt, col, value, copy);
        }
    };

    template <>
    struct Binder<wide_string_view> {
        static void bind(statement& stmt, std::size_t col,
                         wide_string_view value, bool copy) {
            stmt.bind_wstring(col, value, copy);
        }
    };

    template <>
    struct Binder<wide_string> {
        static void bind(statement& stmt, std::size_t col,
                         const wide_string& value, bool copy) {
            Binder<wide_string_view>::bind(stmt, col, value, copy);
        }
    };

    template <>
    struct Binder<std::span<const uint8_t>> {
        static void bind(statement& stmt, std::size_t col,
                         std::span<const uint8_t> value, bool copy) {
            stmt.bind_binary(col, value, copy);
        }
    };

    template <>
    struct Binder<std::vector<uint8_t>> {
        static void bind(statement& stmt, std::size_t col,
                         const std::vector<uint8_t>& value, bool copy) {
            Binder<std::span<const uint8_t>>::bind(stmt, col, value, copy);
        }
    };

    template <Bindable T>
    struct Binder<std::optional<T>> {
        static void bind(statement& stmt, std::size_t col,
                         const std::optional<T>& value, bool copy) {
            if (value.has_value()) {
                Binder<T>::bind(stmt, col, *value, copy);
            }
            else {
                stmt.bind_null(col);
            }
        }
    };

    template <Bindable T>
    struct Binder<as_view<T>> {
        static void bind(statement& stmt, std::size_t col,
                         const as_view<T>& value, bool copy) {
            std::ignore = copy;
            Binder<T>::bind(stmt, col, value.value(), false);
        }
    };
} // namespace RAD_LIB_NAMESPACE::odbc