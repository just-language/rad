#include <rad/async/io_loop.h>
#include <rad/async/strand.h>
#include <rad/async/work_guard.h>
#include <rad/coro/forked_task.h>
#include <rad/coro/spawn.h>
#include <rad/coro/task.h>
#include <rad/threading/thread.h>
#include <rad/threading/thread_pool.h>

#include <iostream>
#include <random>
#include <sstream>
#include <thread>

using namespace RAD_LIB_NAMESPACE;

namespace {
    class testor {
    public:
        testor(io_loop& loop) : wguard{loop}, st{loop} {
            start();
        }

        task<> do_test() {
            constexpr size_t n = 10000;

            for (auto i : range(n)) {
                post(st, [this, i] {
                    if (handler_index++ != i) {
                        throw std::runtime_error("wrong handler "
                                                 "order");
                    }

                    if (shared_flag != 0) {
                        throw std::runtime_error("the shared "
                                                 "flag is "
                                                 "accessed by "
                                                 "another "
                                                 "thread");
                    }

                    std::random_device rd;
                    std::default_random_engine eng(rd());
                    std::uniform_int_distribution<size_t> rng(1000, 1000000);

                    volatile size_t rnd_flag = rng(eng);
                    shared_flag = rnd_flag;

                    {
                        using namespace std::chrono;
                        std::this_thread::yield();
                        // std::this_thread::sleep_for(microseconds(1));
                    }

                    if (shared_flag != rnd_flag) {
                        throw std::runtime_error("the shared "
                                                 "flag was "
                                                 "altered by "
                                                 "another "
                                                 "thread");
                    }

                    shared_flag = 0;
                    if (handler_index == n) {
                        wguard.reset();
                    }
                });
            }

            co_await st.async_wait();
        }

        void start() {
            for (auto i : range(std::thread::hardware_concurrency() * 2)) {
                std::ignore = i;
                thds.emplace_back([&] {
                    try {
                        st.inner_executor().run();
                    }
                    catch (const std::exception& ex) {
                        std::stringstream ss;
                        ss << "[!] An Io "
                              "worker exited "
                              "with an "
                              "exception : "
                           << ex.what() << "\n";
                        error_msg = ss.str();
                    }
                });
            }
        }

        void wait() {
            for (auto& thd : thds) {
                if (thd.joinable()) {
                    thd.join();
                }
            }

            if (!error_msg.empty()) {
                throw std::runtime_error(error_msg);
            }
        }

    private:
        volatile size_t shared_flag = 0;
        size_t handler_index = 0;
        std::string error_msg;
        work_guard<io_loop> wguard;
        strand<io_loop> st;
        std::vector<scoped_thread> thds;
    };

    class timer_testor {
        using clock = std::chrono::steady_clock;
        using duration = clock::duration;
        using time_point = clock::time_point;

    public:
        void wait() {
            auto elapsed = clock::now() - start_time_;
            while (elapsed < delay_time_) {
                mutex mtx;
                std::unique_lock<mutex> lock{mtx};
                wait_on_timer();
                wait_cv_.wait(lock);
                elapsed = clock::now() - start_time_;
            }
        }

    private:
        rad::forked_task wait_on_timer() {
            using namespace std::chrono;
            constexpr auto wait_time = seconds{1};
            timer t{pool_};
            for (auto i : range(4)) {
                std::ignore = i;
                auto start_wait = clock::now();
                co_await t.wait_for(wait_time);
                std::cout
                    << "[*] timer waited on thread "
                       "pool for "
                    << duration_cast<seconds>(clock::now() - start_wait).count()
                    << "s\n";
            }
            wait_cv_.notify_all();
        }

        const time_point start_time_ = clock::now();
        const duration delay_time_ = std::chrono::seconds{1};
        thread_pool pool_ = thread_pool{1};
        condition_variable wait_cv_;
    };
} // namespace

namespace tests_fn {
    bool do_strand_tests() {
        using namespace std::chrono;

        auto start = steady_clock::now();

        try {
            std::exception_ptr ex_ptr;
            io_loop loop;
            {
                testor t{loop};
                spawn(loop, t.do_test(),
                      [&ex_ptr](std::exception_ptr e) { ex_ptr = e; });
                t.wait();
                if (ex_ptr) {
                    std::rethrow_exception(ex_ptr);
                }
            }
            timer_testor t;
            t.wait();
        }
        catch (const std::exception& ex) {
            std::cout << "[!] strand tests failed ! " << ex.what() << "\n";
            return false;
        }

        auto elapsed = steady_clock::now() - start;
        std::cout << "[*] strand tests passed ("
                  << duration_cast<seconds>(elapsed).count() << "s)\n";
        return true;
    }
} // namespace tests_fn