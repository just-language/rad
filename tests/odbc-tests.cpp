#include <rad/databases/odbc.h>

#include <iostream>

using namespace RAD_LIB_NAMESPACE;

namespace {
    void list_sources() {
        odbc::environment env;
        env.set_version(odbc::environment_version::v3_8);
        auto sources = env.sources();
        for (const auto& source : sources) {
            auto u8source = source.to_utf8();
            std::cout << "driver: " << u8source.driver
                      << ", description: " << u8source.description << "\n";
        }
        std::u16string path =
            uR"#(Driver={Microsoft Access Driver (*.mdb, *.accdb)};DBQ=D:\programs\projects\rad-build\Debug\database1.accdb;)#";
        odbc::database db{env};
        db.connect(path);
        auto stmt = db.prepare(u"SELECT * FROM Table1 WHERE ID > ?;");
        std::wstring long_text;
        for (auto i : range(100)) {
            std::ignore = i;
            long_text += L"Five Thousands ";
        }
        stmt << 1;
        // stmt.bind_int64(0, 2);
        // stmt.bind_wstring(0, long_text, false);
        auto select_result = stmt.select();
        size_t cols_count = stmt.columns_count();
        printf("query resulted in %zu columns\n", cols_count);
        for (const auto& col : stmt.result_columns()) {
            printf("%s (%zu) [%d]| ", to_string(col.name).c_str(), col.sql_size,
                   col.sql_type);
        }
        printf("\n");
        for (auto row : select_result) {
            size_t c1 = 0, c2 = 0;
            std::string_view c3, c4;
            std::string_view c5;
            row >> c1 >> c2 >> c3 >> c4 >> c5;
            printf("%zu | %zu | %s | %s |", c1, c2, c3.data(), c4.data());
            printf("\n");
        }
    }
} // namespace

namespace tests_fn {
    bool do_odbc_tests() {
        try {
            list_sources();
        }
        catch (const std::exception& ex) {
            std::cout << "[!] odbc tests failed ! " << ex.what() << "\n";
            return false;
        }
        return true;
    }
} // namespace tests_fn