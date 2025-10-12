#include <rad/async/executor.h>
#include <rad/coro/task.h>
#include <rad/databases/sqlite.h>
#include <rad/unittest/unittest.h>

#include <filesystem>
#include <iostream>

using namespace RAD_LIB_NAMESPACE;
using namespace unittest;

namespace {
    void test_insert_select() {
        using namespace sqlite;

        database db{in_memory_database};

        db.make_query("CREATE TABLE test_table (ID INTEGER, USIZE INTEGER, "
                      "PSIZE "
                      "INTEGER, UNAME TEXT, PASS TEXT)")
            .execute();

        {
            std::vector<std::pair<std::string, std::string>> data{
                {"uname1", "pass1"},
                {"uname2", "pass2"},
                {"uname3", "pass3"},
            };

            for (auto i : range(data.size())) {
                auto q = db.make_query(
                    "INSERT INTO test_table VALUES(?, ?, ?, ?, "
                    "?);",
                    i, data[i].first.size() + i, data[i].second.size() + i,
                    data[i].first, data[i].second);
                q.execute();
            }
        }

        size_t i = 0;
        constexpr size_t uname_size = 6;
        constexpr size_t pass_size = 5;
        for (auto q = db.make_query("SELECT * FROM test_table");
             auto row : q.select()) {
            size_t id, usize, psize;
            std::string_view uname, pass;

            row >> id >> usize >> psize >> uname >> pass;
            if (id != i) {
                throw std::runtime_error("fetched id doesn't match expected");
            }
            ++i;

            if (usize != uname_size + id) {
                throw std::runtime_error(
                    "fetched usize doesn't match expected");
            }

            if (psize != pass_size + id) {
                throw std::runtime_error(
                    "fetched psize doesn't match expected");
            }

            std::string expected_uname = "uname" + std::to_string(i);
            std::string expected_pass = "pass" + std::to_string(i);

            if (uname != expected_uname) {
                throw std::runtime_error(
                    "fetched uname doesn't match expected, "
                    "fetched: " +
                    uname);
            }

            if (pass != expected_pass) {
                throw std::runtime_error("fetched pass doesn't match expected");
            }
        }

        int scale = 1;
        db.define_fn("mulby", [&scale](int i) { return i * scale; });

        for (int i : range(10)) {
            scale = i + 1;
            int expected = 5 * scale;
            auto q = db.make_query("SELECT mulby(5)");
            assert_true(q.next(),
                        "sqlite defined function didn't return a result");
            int got = q.get_value<int>(0);
            assert_eq(got, expected,
                      "sqlite defined function didn't match the "
                      "expected result");
        }

        db.define_fn("concatstr",
                     [](std::string_view str1, std::string_view str2) {
                         return str1 + "-" + str2;
                     });
        for (int i : range(10)) {
            std::string str = "str(" + std::to_string(i) + ")";
            std::string expected = str + "-" + str;
            auto q = db.make_query("SELECT concatstr(?, ?)", str, str);
            assert_true(q.next(),
                        "sqlite defined function didn't return a result");
            std::string_view got = q.get_value<std::string_view>(0);
            assert_eq(got, expected,
                      "sqlite defined function didn't match the "
                      "expected result");
        }

        bool was_executed = false;
        db.define_fn("mark_execution",
                     [&was_executed]() { was_executed = true; });
        {
            auto q = db.make_query("SELECT mark_execution()");
            q.execute();
            // assert_false(q.next(), "void sqlite defined function
            // returned a result");
            assert_true(was_executed,
                        "sqlite defined function was not executed");
        }
    }
} // namespace

namespace tests_fn {
    bool do_sqlite_tests() {
        try {
            test_insert_select();
        }
        catch (const std::exception& ex) {
            std::cout << "[!] sqlite tests failed ! " << ex.what() << "\n";
            return false;
        }

        std::cout << "[*] sqlite tests passed\n";
        return true;
    }
} // namespace tests_fn
