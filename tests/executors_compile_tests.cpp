#include <rad/async/io_loop.h>
#include <rad/async/strand.h>
#include <rad/coro/spawn.h>
#include <rad/coro/task.h>
#include <rad/threading/thread_pool.h>
#include <rad/unittest/unittest.h>

#include <iostream>

using namespace RAD_LIB_NAMESPACE;

namespace {
    static_assert(Executor<io_loop>);
    static_assert(TimerExecutor<io_loop>);
    static_assert(IoExecutor<io_loop>);
    static_assert(IoTimerExecutor<io_loop>);

    static_assert(Executor<thread_pool>);
    static_assert(TimerExecutor<thread_pool>);

    static_assert(Executor<strand<io_loop>>);
    static_assert(TimerExecutor<strand<io_loop>>);
    static_assert(IoExecutor<strand<io_loop>>);
    static_assert(IoTimerExecutor<strand<io_loop>>);

    static_assert(ProxyTimerExecutor<strand<io_loop>>);
    static_assert(ProxyIoExecutor<strand<io_loop>>);
    static_assert(ProxyIoTimerExecutor<strand<io_loop>>);

    static_assert(Executor<strand<thread_pool>>);
    static_assert(TimerExecutor<strand<thread_pool>>);

    static_assert(ProxyTimerExecutor<strand<thread_pool>>);

    struct dummy_op final : detail::async_op_base {
        any_executor& ex;

        dummy_op(any_executor& ex) : ex{ex} {
        }

        void invoke_operation() override {
        }

        any_executor& associated_executor() const noexcept override {
            return ex;
        }
    };
} // namespace