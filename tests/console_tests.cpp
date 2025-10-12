#include <rad/coro/task.h>
#include <rad/io/console.h>

#include <iostream>

#include "tests.h"

using namespace LIB_NAMESPACE;
using namespace io;

namespace {
    void test_console_write(console& c) {
        std::string_view console_message =
            "[Message Text] writing to the console\n";
        c.write(buffer(console_message));
    }

    void test_console_read(console& c) {
        std::cout << "[*] enter some text up to 20 characters in the "
                     "console: ";
        std::string input(20, '\0');
        std::size_t n = c.read(buffer(input));
        input.resize(n);
        std::cout << "[*] you entered(" << n << "): " << input;
    }

    inline forked_task test_console_async_read(console& c) {
        std::cout << "[*] enter some text up to 20 characters in the "
                     "console: ";
        std::string input(20, '\0');
        std::size_t n = co_await c.async_read(buffer(input));
        input.resize(n);
        std::cout << "[*] you entered (" << n << "): " << input;
    }
} // namespace

bool do_console_tests() {
    try {
        io_loop loop;
        console c{loop};

        test_console_write(c);
        test_console_read(c);
        test_console_async_read(c);

        loop.run();
    }
    catch (const std::exception& ex) {
        std::cout << "[!] conosole tests failed ! " << ex.what() << "\n";
        return false;
    }

    std::cout << "[*] console tests passed\n";
    return true;
}