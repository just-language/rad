
## Rad Coroutines

The `task<T>` makes it easy to start and chain coroutines:

```
#include <rad/coro/task.h>

task<int> coro1() {
    co_return 1;
}

task<int> coro2() {
    co_return 2;
}

task<int> coro3() {
    co_return (co_await coro1()) +
        (co_await coro2());
}

task<int> coro4() {
    auto t = coro3();
    // t hasn't started yet!
    co_return co_await t; // start to now 
}

// void task
task<> coro5() {
    int res = co_await coro4();
    std::cout << res << std::endl;
}
```

To wait for multiple coroutines:

```
#include <rad/coro/task.h>
#include <rad/coro/when_all.h>

// coro1, coro2, coro3, coro4, coro5 are defined above

task<> wait_for_them_all() {
    auto wait_all_awaitable = when_all(coro1(),
        coro2(), coro3(), coro4());
    // wait_all_awaitable hasn't started yet
    // no coroutine has started yet

    // start the coroutines now
    // the awaitable will not resume until
    // all coroutines have finished
    auto [n1, n2, n3, n4] = co_await wait_all_awaitable;

    // use && operators
    // it will return an awaitable like wait_all_awaitable
    std::tie(n1, n2) = co_await (coro1() && coro2());

    // void is tranformed to std::monostate
    auto [res1, res2] = co_await (coro4() && coro5());
    // res2 is monostate

    // use vector of tasks
    std::vector<task<int>> tasks;
    tasks.emplace_back(coro1());
    tasks.emplace_back(coro2());
    tasks.emplace_back(coro3());
    tasks.emplace_back(coro4());
    // start all the tasks and wait for them
    co_await when_all(std::move(tasks));
}
```

To spawn an awaitable from sync function:

```
#include <rad/coro/spawn.h>
#include <rad/async/io_loop.h>

using namespace rad;

task<int> coro1() {
    std::cout << "coro1 is running!\n";
    co_return 0;
}

task<int> coro2() {
    std::cout << "coro2 is running!\n";
    co_return 1;
}

task<> coro3() {
    throw std::runtime_error{ "catch this!" };
}

task<bool> coro4(bool b) {
    if (b) {
        std::cout << "coro4 is returning true!\n";
        return true;
    }
    else {
        throw std::runtime_error{ "catch this from coro4!" };
        return false;
    }
}

int main() {
    io_loop loop;
    std::allocator<std::uint8_t> alloc;

    // the coroutine result is discarded
    // if an exception is thrown std::terminate is called
    spawn(loop, coro1());

    // same as the above with custom allocator
    spawn(loop, coro1(), alloc);
    
    // the coroutine result is passed to handler
    // if an exception is thrown std::terminate is called
    spawn(loop, coro2(), [](int res) {});

    // same as the above with custom allocator
    spawn(loop, coro2(), [](int res) {}, alloc);

    // the coroutine result is discarded
    // if an exception is thrown the handler
    // will be called with non empty exception_ptr
    spawn(loop, coro3(), [](std::exception_ptr e) {});

    // same as the above with custom allocator
    spawn(loop, coro3(), [](std::exception_ptr e) {}, alloc);

    // the coroutine result is passed to handler
    // if an exception is thrown the handler
    // will be called with non empty exception_ptr
    spawn(loop, coro4(true), [](bool res) {},
        [](std::exception_ptr e) {});

    // same as the above with custom allocator
    spawn(loop, coro4(false), [](bool res) {},
        [](std::exception_ptr e) {}, alloc);

    // run until all posted work is done
    loop.run();
    std::cout << "all coroutines has finished!\n";
    return 0;
}
```

To wait for the spawned coroutines:

```
#include <rad/coro/task.h>
#include <rad/coro/spawn.h>
#include <rad/coro/scoped_wait.h>
#include <rad/async/io_loop.h>

using namespace rad;

void spawn_coros(spawner<io_loop>& s, bool throw_it) {
    // all overloads of free spawn are available
    // but the executor is not passed because
    // the spawner was constructed with the executor
    s.spawn(coro1());
    s.spawn(coro2(), [](int res) {});
    if (throw_it) {
        throw std::runtime_error{ "you wanted this!" };
    }
    s.spawn(coro3(), [](std::exception_ptr e) {});
}

task<> spawn_and_manual_wait(spawner<io_loop>& s) {
    spawn_coros(s, false);
    co_await s.async_wait();

    spawn_coros(s, true);
    // can't wait because the exception was thrown!
    // execution will not reach here
    co_await s.async_wait();
}

task<> spawn_auto_wait(spawner<io_loop>& s) {
    co_await wait_on(s, [&] {
        spawn_coros(s, false);
    });
    // all spawned coroutines has finished
    co_await wait_on(s, [&] {
        spawn_coros(s, true);
    });
    // all spawned coroutines has finished
    // the exception was rethrown after
    // wait so execution will not reach here!
}

task<> spawn_auto_wait(io_loop& loop) {
    co_await scoped_wait<spawner<io_loop>>(
        loop,
        [](spawner<io_loop>& s) {
            spawn_coros(s, false);
        }
    );
    // all spawned coroutines has finished
    co_await scoped_wait<spawner<io_loop>>(
        loop,
        [](spawner<io_loop>& s) {
            spawn_coros(s, true);
        }
    );
    // all spawned coroutines has finished
    // the exception was rethrown after
    // wait so execution will not reach here!
}
```