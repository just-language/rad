#include <rad/cli.h>

#include <iostream>
#include <memory>
#include <vector>

using namespace RAD_LIB_NAMESPACE;
using namespace cli;

namespace {
    struct cmdline_test_entry {
        std::string line;
        std::vector<std::string_view> argv;
    };

    struct parser_test_entry_base {
        std::string line;
        std::vector<std::string_view> argv;

        virtual void init_parser(parser& p) = 0;

        virtual void validate_arguments(const parser& p) = 0;

        virtual ~parser_test_entry_base() = default;
    };

    void print_argv(std::span<const std::string_view> argv1,
                    std::span<std::string> argv2) {
        std::cout << "expected: ";
        for (auto it = argv1.begin(); it != std::prev(argv1.end()); ++it) {
            std::cout << '[' << *it << "], ";
        }
        std::cout << '[' << argv1.back() << "], but got: ";
        for (auto it = argv2.begin(); it != std::prev(argv2.end()); ++it) {
            std::cout << '[' << *it << "], ";
        }
        std::cout << '[' << argv2.back() << "]\n";
    }

    std::vector<cmdline_test_entry> make_split_test_entries() {
        std::vector<cmdline_test_entry> entries;

        // 1 - "abc" d e -> [abc] [d] [e]
        {
            auto& entry = entries.emplace_back();
            entry.line = R"#("abc" d e)#";
            entry.argv = {"abc", "d", "e"};
        }

        // 2 - a\\\b d"e f"g h -> [a\\\b] [de fg] [h]
        {
            auto& entry = entries.emplace_back();
            entry.line = R"#(a\\\b d"e f"g h)#";
            entry.argv = {R"(a\\\b)", "de fg", "h"};
        }

        // 3 - a\\\"b c d -> [a\"b] [c] [d]
        {
            auto& entry = entries.emplace_back();
            entry.line = R"#(a\\\"b c d)#";
            entry.argv = {R"(a\"b)", "c", "d"};
        }

        // 4 - a\\\\"b c" d e -> [a\\b c] [d] [e]
        {
            auto& entry = entries.emplace_back();
            entry.line = R"#(a\\\\"b c" d e)#";
            entry.argv = {R"(a\\b c)", "d", "e"};
        }

        // 5 - "ab\"c" "\\" d -> [ab"c] [\] [d]
        {
            auto& entry = entries.emplace_back();
            entry.line = R"#("ab\"c" "\\" d)#";
            entry.argv = {R"(ab"c)", "\\", "d"};
        }

        // 6 - a"b"" c d -> [ab" c d]
        {
            auto& entry = entries.emplace_back();
            entry.line = R"#(a"b"" c d)#";
            entry.argv = {R"(ab" c d)"};
        }

        // 7 - arg"1" arg2 arg3\\\ -> [arg1] [arg2] [arg3\\\]
        {
            auto& entry = entries.emplace_back();
            entry.line = R"#(arg"1" arg2 arg3\\\)#";
            entry.argv = {"arg1", "arg2", R"(arg3\\\)"};
        }

        // 8 - arg1\\arg2"arg"3 -> [arg1\\arg2arg3]
        {
            auto& entry = entries.emplace_back();
            entry.line = R"#(arg1\\arg2"arg"3)#";
            entry.argv = {R"(arg1\\arg2arg3)"};
        }

        // 9 - arg1\"a arg2\\" arg3 -> [arg1"a] [arg2\ arg3]
        {
            auto& entry = entries.emplace_back();
            entry.line = R"#(arg1\"a arg2\\" arg3)#";
            entry.argv = {R"(arg1"a)", R"(arg2\ arg3)"};
        }

        // 10 - "cut"into\\\\ pieces -> [cutinto\\\\] [pieces]
        {
            auto& entry = entries.emplace_back();
            entry.line = R"#("cut"into\\\\ pieces)#";
            entry.argv = {R"(cutinto\\\\)", "pieces"};
        }

        return entries;
    }

    std::vector<std::unique_ptr<parser_test_entry_base>>
    make_parser_test_entries() {
        std::vector<std::unique_ptr<parser_test_entry_base>> entries;

        // 1
        {
            // progname --one=1 --two 2 --three "3" --tt="33" -f 4 -abs 6

            struct test_entry1 : public parser_test_entry_base {
                std::string progname;
                int one = 0, two = 0, three = 0, thirty_three = 0, four = 0,
                    six = 0;

                test_entry1() {
                    line = "progname --one=1 --two 2 --three "
                           "\"3\" "
                           "--tt=\"33\" -f 4 -abs 6";
                    argv = {"progname", "--one=1", "--two",  "2", "-t",
                            "3",        "--tt=33", "--four", "4", "-a"};
                }

                virtual void init_parser(parser& p) override {
                    p.add_positional("progname", value(progname));
                    p.add_options()("one", value(one))("two", value(two))(
                        "three,t", value(three))("tt", value(thirty_three))(
                        "four,f", value(four))("s", value(six),
                                               true)("a")("b", true);
                }

                virtual void validate_arguments(const parser& p) override {
                    if (one != 1 || two != 2 || three != 3 ||
                        thirty_three != 33 || four != 4 || !p.has_option('a')) {
                        throw std::runtime_error("failed to parse arguments "
                                                 "(1)");
                    }
                    if (progname != "progname" ||
                        (p.has_option('s') && six != 6)) {
                        throw std::runtime_error("failed to parse arguments "
                                                 "(1)");
                    }

                    progname.clear();
                    one = 0, two = 0, three = 0, thirty_three = 0, four = 0,
                    six = 0;
                }
            };

            entries.push_back(
                std::unique_ptr<parser_test_entry_base>{new test_entry1()});
        }

        return entries;
    }

    void do_split_cmdline_tests() {
        auto entries = make_split_test_entries();

        int i = 0;

        for (const auto& entry : entries) {
            ++i;

            auto argv = split_winmain(entry.line);
            if (argv.size() != entry.argv.size() ||
                !std::equal(argv.begin(), argv.end(), entry.argv.begin())) {
                print_argv(entry.argv, argv);
                std::string msg = "failed to split test (" + std::to_string(i) +
                                  ") || " + entry.line + " ||";
                throw std::runtime_error(msg);
            }
        }
    }

    void do_parser_tests() {
        auto entries = make_parser_test_entries();

        parser p;

        for (auto& entry : entries) {
            p.reset();
            entry->init_parser(p);
            p.parse(entry->line);
            entry->validate_arguments(p);
            p.reset();
            entry->init_parser(p);
            p.parse(entry->argv);
            entry->validate_arguments(p);
        }
    }
} // namespace

namespace tests_fn {
    bool do_cli_tests() {
        try {
            do_split_cmdline_tests();
            do_parser_tests();
        }
        catch (const std::exception& ex) {
            std::cout << "[!] cli tests failed ! " << ex.what() << " !\n";
            return false;
        }

        std::cout << "[*] cli tests passed\n";
        return true;
    }
} // namespace tests_fn