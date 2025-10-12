#include <rad/async/io_loop.h>
#include <rad/async/timer.h>
#include <rad/coro/execute.h>
#include <rad/coro/execute_timeout.h>
#include <rad/coro/forked_task.h>
#include <rad/coro/scoped_wait.h>
#include <rad/coro/spawn.h>
#include <rad/coro/task.h>
#include <rad/coro/this_coro.h>
#include <rad/coro/when_all.h>
#include <rad/unittest/unittest.h>

#include <iostream>

using namespace RAD_LIB_NAMESPACE;
using namespace unittest;

namespace {

    struct test_task_t {};

    struct coro_class_test_t;

    struct coro_class_test_t {};

    class coro_test_allocator {
    public:
        using value_type = uint8_t;
        using pointer_type = uint8_t*;

        pointer_type allocate(std::size_t n) {
            void* p = ::operator new(n);
            ++allocations_n;
            return static_cast<pointer_type>(p);
        }

        void deallocate(pointer_type p, std::size_t n) {
            ::operator delete(p);
            --allocations_n;
        }

        static std::size_t allocations() noexcept {
            return allocations_n;
        }

    private:
        inline static std::size_t allocations_n = 0;
    };

    task<> test_free_await_task1() {
        throw std::system_error(std::make_error_code(std::errc::interrupted),
                                "task<>");
        co_return;
    }

    forked_task test_free_task1() {
        bool caught = false, same_ex = false;
        ;

        try {
            co_await test_free_await_task1();
        }
        catch (const std::system_error& ex) {
            caught = true;
            same_ex =
                ex.code() == std::make_error_code(std::errc::interrupted) &&
                std::string_view(ex.what()).find("task<>") !=
                    std::string_view::npos;
        }

        if (!caught) {
            printf("[!] exception thrown by task<> coroutine was not "
                   "caught !\n");
            throw std::runtime_error(""); // terminate
        }

        if (!same_ex) {
            printf("[!] caught exception is not the same as the one "
                   "thrown "
                   "by task<> "
                   "coroutine !\n");
            throw std::runtime_error(""); // terminate
        }
    }

    task<int> test_free_await_task2(std::allocator_arg_t,
                                    const coro_test_allocator& alloc,
                                    std::string param) {
        if (alloc.allocations() != 2) {
            printf("[!] the allocations was %zu instead of 2 !\n",
                   alloc.allocations());
            // terminate because not caught by forked_task caller
            // throw std::runtime_error("");
        }
        co_return 10;
    }

    forked_task test_free_task2(std::allocator_arg_t,
                                const coro_test_allocator& alloc,
                                std::string param) {
        if (alloc.allocations() != 1) {
            printf("[!] the allocations was %zu instead of 1 !\n",
                   alloc.allocations());
            // throw std::runtime_error(""); // terminate
        }

        int ret = co_await test_free_await_task2(std::allocator_arg, alloc,
                                                 std::move(param));
        if (ret != 10) {
            printf("[!] task<int> returned wrong value !\n");
            throw std::runtime_error(""); // terminate
        }
    }

    task<float>
    test_free_await_task3(const use_allocator<coro_test_allocator>& alloc,
                          int i) {
        if (alloc.get_allocator().allocations() != 2) {
            printf("[!] the allocations was %zu instead of 2 !\n",
                   alloc.get_allocator().allocations());
            // terminate because not caught by forked_task caller
            // throw std::runtime_error("");
        }
        co_return 1.3f;
    }

    forked_task test_free_task3(const use_allocator<coro_test_allocator>& alloc,
                                int i) {
        if (alloc.get_allocator().allocations() != 1) {
            printf("[!] the allocations was %zu instead of 1 !\n",
                   alloc.get_allocator().allocations());
            // throw std::runtime_error(""); // terminate
        }

        float ret = co_await test_free_await_task3(alloc, i);
        if (ret != 1.3f) {
            printf("[!] task<float> returned wrong value !\n");
            throw std::runtime_error(""); // terminate
        }
    }

    struct coro_class_test_derived : public coro_class_test_t {
    public:
        void do_test(coro_test_allocator& alloc) {
            coro_member1();
            coro_member2(std::allocator_arg, alloc, "param1");
        }

    private:
        forked_task coro_member1() {
            co_return;
        }

        forked_task coro_member2(std::allocator_arg_t,
                                 coro_test_allocator& alloc,
                                 std::string param1) {
            if (alloc.allocations() != 1) {
                printf("[!] the allocations was %zu "
                       "instead of 1 "
                       "!\n",
                       alloc.allocations());
                // terminate
                // throw std::runtime_error("");
            }

            auto ret = co_await coro_member3(use_allocator(alloc), 5);
            if (ret != "task<std::string>(5)") {
                printf("[!] task<std::string> returned "
                       "wrong "
                       "value !\n");
                throw std::runtime_error(""); // terminate
            }
        }

        task<std::string>
        coro_member3(const use_allocator<coro_test_allocator>& alloc, int i) {
            if (alloc.get_allocator().allocations() != 2) {
                printf("[!] the allocations was %zu "
                       "instead of 2 "
                       "!\n",
                       alloc.get_allocator().allocations());
                // throw std::runtime_error(""); //
                // terminate
            }
            co_return "task<std::string>(" + std::to_string(i) + ")";
        }
    };

    struct coro_counter_derived : public task_counter {
        using base = task_counter;

        coro_counter_derived(async_phaser& c) : task_counter(c) {
        }

        void do_tests(coro_test_allocator& alloc) {
            coro_member1();
            coro_member2(std::allocator_arg, alloc, "param1");
            if (base::wait_tasks().value() != 0) {
                throw std::runtime_error("forked_task ref counting after "
                                         "completion "
                                         "was not "
                                         "0 !");
            }
        }

    private:
        forked_task coro_member1() {
            if (base::wait_tasks().value() != 1) {
                printf("[!] forked_task ref counting is "
                       "not "
                       "working "
                       "properly !\n");
                throw std::runtime_error(""); // terminate
            }
            co_return;
        }

        forked_task coro_member2(std::allocator_arg_t,
                                 coro_test_allocator& alloc,
                                 std::string param1) {
            if (alloc.allocations() != 1) {
                printf("[!] the allocations was %zu "
                       "instead of 1 "
                       "!\n",
                       alloc.allocations());
                // throw std::runtime_error(""); //
                // terminate
            }
            if (base::wait_tasks().value() != 1) {
                printf("[!] forked_task ref counting was "
                       "%zu "
                       "instead "
                       "of 1 !\n",
                       base::wait_tasks().value());
                throw std::runtime_error(""); // terminate
            }
            coro_member3(use_allocator(alloc), 7);
            co_return;
        }

        forked_task
        coro_member3(const use_allocator<coro_test_allocator>& alloc, int i) {
            if (alloc.get_allocator().allocations() != 2) {
                printf("[!] the allocations was %zu "
                       "instead of 2 "
                       "!\n",
                       alloc.get_allocator().allocations());
                // throw std::runtime_error(""); //
                // terminate
            }
            if (base::wait_tasks().value() != 2) {
                printf("[!] forked_task ref counting was "
                       "%zu "
                       "instead "
                       "of 2 !\n",
                       base::wait_tasks().value());
                throw std::runtime_error(""); // terminate
            }
            co_return;
        }
    };

} // namespace

namespace {
    [[maybe_unused]] void compile_time_tests(any_executor& ex) {
        auto task1 = []() -> task<int> { co_return 0; };

        auto task2 = []() -> task<float> { co_return 0.f; };

        auto task3 = []() -> task<double> { co_return 0.; };

        auto task4 = []() -> task<> { co_return; };

        using result_t = std::tuple<int, float, double, std::monostate>;
        static_assert(
            std::is_same_v<decltype(when_all(task1(), task2(), task3(),
                                             task4()))::value_type,
                           result_t>);
        static_assert(std::is_same_v<decltype((task1() && task2() && task3() &&
                                               task4()))::value_type,
                                     result_t>);
        static_assert(
            std::is_same_v<decltype((task1() && task2() &&
                                     when_all(task3(), task4())))::value_type,
                           result_t>);
        static_assert(std::is_same_v<decltype(when_all(task1(), task2()) &&
                                              task3() && task4())::value_type,
                                     result_t>);
        static_assert(
            std::is_same_v<decltype(when_all(task1(), task2()) &&
                                    when_all(task3(), task4()))::value_type,
                           result_t>);

        auto some_task = []() -> task<> { co_return; };
        auto some_task2 = []() -> task<int> { co_return 0; };

        spawn(ex, some_task());
        spawn(ex, some_task2());
        spawn(ex, some_task() && some_task2());

        spawn(ex, some_task(), redirect_error([](std::exception_ptr) {}));
        spawn(ex, some_task2(), redirect_error([](std::exception_ptr) {}));
        spawn(ex, some_task() && some_task2(),
              redirect_error([](std::exception_ptr) {}));

        spawn(ex, some_task(), ignore_errors);
        spawn(ex, some_task(), terminate_on_error);
        spawn(ex, some_task2(), [](std::exception_ptr) {});
        spawn(ex, some_task() && some_task2(), [](std::exception_ptr) {});

        spawn(ex, some_task(), [] {});
        spawn(ex, some_task2(), [](int) {});
        spawn(ex, some_task() && some_task2(),
              [](std::tuple<std::monostate, int>) {});

        spawn(ex, some_task(), [] {}, [](std::exception_ptr) {});
        spawn(ex, some_task2(), [](int) {}, [](std::exception_ptr) {});
    }

    template <std::size_t N>
    task<std::string> chained_coro(std::array<bool, N>& chain_state,
                                   size_t index) {
        for (size_t i = 0; i < index; ++i) {
            TEST_EQ_MSG(chain_state.at(i), true,
                        "chain(" + std::to_string(i + 1) +
                            ") is not running !");
        }
        for (size_t i = index; i < chain_state.size(); ++i) {
            TEST_EQ_MSG(chain_state.at(i), false,
                        "chain(" + std::to_string(i + 1) + ") is running !");
        }

        chain_state.at(index) = true;
        ++index;
        if (index == chain_state.size()) {
            co_return "chain(" + std::to_string(index) + ")";
        }
        else {
            co_return "chain(" + std::to_string(index) + ")->" +
                co_await chained_coro(chain_state, index);
        }
    }

    task<> test_chained_tasks() {
        constexpr std::size_t N = 4;
        std::array<bool, N> chains_state = {{false, false, false, false}};
        std::string result = co_await chained_coro(chains_state, 0);
        std::string expected_result;
        for (auto i : range(N - 1)) {
            expected_result += "chain(" + std::to_string(i + 1) + ")->";
        }
        expected_result += "chain(" + std::to_string(N) + ")";
        TEST_EQ(result, expected_result, "produced result", "expected result");
    }

    task<int> concurrent_coro(any_executor& ex, std::vector<int>& out,
                              int coro_index, int coros_count) {
        out.push_back(coro_index);
        co_await this_coro::yield(ex);
        out.push_back(coro_index + coros_count);
        co_return coro_index* coro_index;
    }

    template <class AwaitableFn>
    task<> do_test_when_all(AwaitableFn a, std::vector<int>& out) {
        out.clear();
        auto r = co_await a();
        const std::vector<int> expected_out = {1, 2, 3, 4, 5, 6, 7, 8};
        const std::tuple<int, int, int, int> expected_r = {1, 4, 9, 16};
        TEST_EQ(r, expected_r, "produced results", "expected results");
        TEST_EQ(out, expected_out, "produced output", "expected output");
    }

    task<> test_when_all(any_executor& ex) {
        std::vector<int> out;
        const int count = 4;

        auto produce_coros1 = [&] {
            return (concurrent_coro(ex, out, 1, count) &&
                    concurrent_coro(ex, out, 2, count) &&
                    concurrent_coro(ex, out, 3, count) &&
                    concurrent_coro(ex, out, 4, count));
        };

        auto produce_coros2 = [&] {
            return when_all(concurrent_coro(ex, out, 1, count),
                            concurrent_coro(ex, out, 2, count),
                            concurrent_coro(ex, out, 3, count),
                            concurrent_coro(ex, out, 4, count));
        };

        auto produce_coros3 = [&] {
            return (when_all(concurrent_coro(ex, out, 1, count),
                             concurrent_coro(ex, out, 2, count)) &&
                    concurrent_coro(ex, out, 3, count) &&
                    concurrent_coro(ex, out, 4, count));
        };

        auto produce_coros4 = [&] {
            return when_all(concurrent_coro(ex, out, 1, count),
                            concurrent_coro(ex, out, 2, count)) &&
                   when_all(concurrent_coro(ex, out, 3, count),
                            concurrent_coro(ex, out, 4, count));
        };

        auto produce_coros5 = [&] {
            return (concurrent_coro(ex, out, 1, count) &&
                    concurrent_coro(ex, out, 2, count) &&
                    when_all(concurrent_coro(ex, out, 3, count),
                             concurrent_coro(ex, out, 4, count)));
        };

        auto produce_coros6 = [&] {
            return when_all(concurrent_coro(ex, out, 1, count),
                            concurrent_coro(ex, out, 2, count),
                            concurrent_coro(ex, out, 3, count),
                            concurrent_coro(ex, out, 4, count));
        };

        co_await do_test_when_all(produce_coros1, out);
        co_await do_test_when_all(produce_coros2, out);
        co_await do_test_when_all(produce_coros3, out);
        co_await do_test_when_all(produce_coros4, out);
        co_await do_test_when_all(produce_coros5, out);
        co_await do_test_when_all(produce_coros6, out);

        int n = 0;
        auto throwing_task = [&n]() -> task<> {
            n += 5;
            if (n == 10) {
                throw std::system_error{
                    std::make_error_code(std::errc::interrupted)};
            }
            else if (n > 10) {
                throw std::runtime_error{"catch this !"};
            }
            co_return;
        };
        TEST_THROW_EX(co_await when_all(throwing_task(), throwing_task(),
                                        throwing_task(), throwing_task()),
                      std::system_error,
                      "when_all didn't propagate the exception");
        TEST_EQ(n, 20, "n", "20");
    }

    task<> spawned_task(int inc, int& n) {
        n += inc;
        co_return;
    }

    task<> test_scoped_spawn(io_loop& ex) {
        int count = 0;

        // auto task1 = [&](int n) -> task<> { count += n; co_return; };

        int result = co_await scoped_spawn(ex, [&](auto& s) {
            s.spawn(spawned_task(1, count));
            s.spawn(spawned_task(2, count));
            s.spawn(spawned_task(3, count));
            return 3;
        });
        TEST_EQ(count, 6, "count", "6");
        TEST_EQ(result, 3, "result", "3");
        count = 0;
        result = co_await scoped_spawn(ex, [&](auto& s) -> task<int> {
            s.spawn(spawned_task(2, count));
            s.spawn(spawned_task(2, count));
            s.spawn(spawned_task(3, count));
            co_return 4;
        });
        TEST_EQ(count, 7, "count", "7");
        TEST_EQ(result, 4, "result", "4");
        count = 0;
        auto throwing_fn = [&](auto& s) {
            s.spawn(spawned_task(2, count));
            s.spawn(spawned_task(3, count));
            s.spawn(spawned_task(5, count));
            throw std::runtime_error{""};
        };
        TEST_THROW_EX(co_await scoped_spawn(ex, throwing_fn), std::exception,
                      "scoped_spawn didn't propagate the exception");
        TEST_EQ(count, 10, "count", "10");
        count = 0;
        auto throwing_coro = [&](auto& s) -> task<> {
            s.spawn(spawned_task(3, count));
            s.spawn(spawned_task(4, count));
            s.spawn(spawned_task(6, count));
            throw std::runtime_error{""};
            co_return;
        };
        TEST_THROW_EX(co_await scoped_spawn(ex, throwing_coro), std::exception,
                      "scoped_spawn didn't propagate the exception");
        TEST_EQ(count, 13, "count", "13");
    }
} // namespace

task<std::chrono::seconds> wait_on_timer(io_loop& loop,
                                         std::chrono::seconds time, int i) {
    using namespace std::chrono;
    timer t{loop};
    auto start_time = steady_clock::now();
    co_await t.wait_for(time);
    auto elapsed = steady_clock::now() - start_time;
    std::cout << "[*] coroutine " << i << " waited for "
              << duration_cast<seconds>(elapsed).count() << "\n";
    co_return duration_cast<seconds>(elapsed);
}

task<> wait_on_several_timers(io_loop& loop) {
    using namespace std::chrono;
    auto task2 = wait_on_timer(loop, seconds{7}, 2);
    auto task4 = wait_on_timer(loop, seconds{9}, 4);
    auto start_time = steady_clock::now();

    auto [t1, t2, t3, t4] =
        co_await when_all(wait_on_timer(loop, seconds{3}, 1), std::move(task2),
                          wait_on_timer(loop, seconds{5}, 3), std::move(task4));

    if (false) {
        // compile test
        std::tie(t1, t2, t3, t4) =
            co_await (wait_on_timer(loop, seconds{7}, 2) &&
                      wait_on_timer(loop, seconds{7}, 2) &&
                      wait_on_timer(loop, seconds{7}, 2) &&
                      wait_on_timer(loop, seconds{7}, 2));
        auto await1 = wait_on_timer(loop, seconds{7}, 2) &&
                      wait_on_timer(loop, seconds{7}, 2);
        auto await2 = wait_on_timer(loop, seconds{7}, 2) && std::move(await1);
        auto await3 = std::move(await1) && std::move(await2);
    }

    auto elapsed = steady_clock::now() - start_time;
    std::cout << "[*] wait_all waited for "
              << duration_cast<seconds>(elapsed).count() << "\n";
    std::cout << "[*] timer1: " << t1.count() << ", timer2: " << t2.count()
              << ", timer3: " << t3.count() << ", timer4: " << t4.count()
              << "\n";
}

task<> dummy_spawned_task() {
    co_return;
}

namespace tests_fn {
    bool do_coro_tests() {
        coro_test_allocator alloc;
        test_free_task1();
        test_free_task2(std::allocator_arg, alloc, "leading allocator");
        test_free_task3(use_allocator(alloc), 10);
        coro_class_test_derived cls_test;
        cls_test.do_test(alloc);
        io_loop loop;
        spawn(loop, dummy_spawned_task());

        int failed_tests = 0;
        auto ex_handler = [&failed_tests](std::string_view test_name,
                                          std::exception_ptr ex_ptr) {
            ++failed_tests;
            try {
                std::rethrow_exception(ex_ptr);
            }
            catch (const unittest::exception& ex) {
                std::cout << "[!] coroutines " << test_name << " test failed ! "
                          << ex.detailed() << "\n";
            }
        };

        any_executor& any_ex = loop;
        spawn(any_ex, test_when_all(loop),
              std::bind_front(ex_handler, "when_all"));
        spawn(any_ex, test_chained_tasks(),
              std::bind_front(ex_handler, "chained tasks"));
        spawn(any_ex, test_scoped_spawn(loop),
              std::bind_front(ex_handler, "scoped_spawn"));

        // co_spawn wait_on_several_timers(loop);
        // printf("--- running spawned coros\n");
        loop.run();
        // printf("--- runned spawned coros\n");
        async_phaser c{loop};
        coro_counter_derived cls2_test{c};
        cls2_test.do_tests(alloc);

        loop.run();

        if (!failed_tests) {
            printf("[*] coroutine tests passed\n");
        }
        return failed_tests == 0;
    }
} // namespace tests_fn