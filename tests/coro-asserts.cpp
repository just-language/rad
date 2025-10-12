#include <cassert>
#include <coroutine>
#include <exception>
#include <iostream>
#include <optional>
#include <variant>

class resume_waiter {
public:
    resume_waiter(std::coroutine_handle<> waiter) noexcept : waiter_{waiter} {
    }

    constexpr bool await_ready() const noexcept {
        return false;
    }

    std::coroutine_handle<>
    await_suspend(std::coroutine_handle<> suspended) noexcept {
        // the following assert is not met!
        assert(suspended.done() &&
               "the coroutine must be in final_suspend at this "
               "point");
        ((void)suspended);
        return waiter_;
    }

    void await_resume() const noexcept {
        assert(false && "execution should nenver reach here");
    }

private:
    std::coroutine_handle<> waiter_;
};

struct task_result {
    std::optional<std::exception_ptr> ex_ptr;

    void unhandled_exception() noexcept {
        ex_ptr = std::current_exception();
    }

    constexpr void return_void() const noexcept {
    }

    void get_result() const {
        if (ex_ptr) {
            std::rethrow_exception(*ex_ptr);
        }
    }

    std::variant<std::monostate, std::exception_ptr>
    result_or_error() const noexcept {
        if (ex_ptr) {
            return *ex_ptr;
        }
        return {};
    }
};

class vtask;

class task_promise_base : public task_result {
    friend class vtask;

public:
    vtask get_return_object() noexcept;

    constexpr std::suspend_always initial_suspend() noexcept {
        return {};
    }

    resume_waiter final_suspend() noexcept {
        return resume_waiter{waiter_};
    }

private:
    std::coroutine_handle<> waiter_;
};

class vtask {
    friend class task_promise_base;

    using promise_t = task_promise_base;
    using handle = std::coroutine_handle<promise_t>;

    // used by promise get_return_object
    vtask(handle coro) noexcept : this_coro_{coro} {
    }

public:
    vtask(vtask&& other) noexcept
        : this_coro_{std::exchange(other.this_coro_, nullptr)} {
    }

    ~vtask() {
        if (this_coro_) {
            this_coro_.destroy();
        }
    }

    bool done() const noexcept {
        assert(this_coro_ != nullptr);
        return this_coro_.done();
    }

    // vtask always starts suspended (don't use await_* methods
    // directly !)
    constexpr bool await_ready() const noexcept {
        return false;
    }

    handle await_suspend(std::coroutine_handle<> waiter) {
        assert(this_coro_ != nullptr);
        this_promise().waiter_ = waiter;
        return this_coro_;
    }

    void await_resume() {
        assert(this_coro_ != nullptr);
        return this_promise().get_result();
    }

    bool is_valid() const noexcept {
        return this_coro_ != nullptr;
    }

private:
    promise_t& this_promise() const noexcept {
        assert(this_coro_ != nullptr);
        return this_coro_.promise();
    }

    handle this_coro_;
};

vtask task_promise_base::get_return_object() noexcept {
    return vtask{std::coroutine_handle<task_promise_base>::from_promise(*this)};
}

class forked_task_promise_base;

class forked_task {
    friend class forked_task_promise_base;

    forked_task() = default;
};

class forked_task_promise_base {
public:
    forked_task get_return_object() {
        return {};
    }

    constexpr std::suspend_never initial_suspend() noexcept {
        return {};
    }

    constexpr std::suspend_never final_suspend() noexcept {
        return {};
    }

    void unhandled_exception() noexcept {
#ifndef NDEBUG
        try {
            std::rethrow_exception(std::current_exception());
        }
        catch (const std::exception& ex) {
            printf("[!!] an exception was thrown and not "
                   "caught "
                   "from forked_task : %s\n",
                   ex.what());
        }
        catch (...) {
            printf("[!!] an exception was thrown and not "
                   "caught "
                   "from forked_task !\n");
        }
#endif // !NDEBUG
        std::terminate();
    }

    constexpr void return_void() const noexcept {
    }
};

namespace std {
    template <class... Args>
    struct coroutine_traits<vtask, Args...> {
        using promise_type = task_promise_base;
    };

    template <class... Args>
    struct coroutine_traits<forked_task, Args...> {
        using promise_type = forked_task_promise_base;
    };
} // namespace std

vtask simple_task(int i) {
    std::cout << "[*] simple_task(" << i << ") is running\n";
    co_return;
}

forked_task run_tasks() {
    for (int i = 0; i < 10; ++i) {
        co_await simple_task(i);
    }
}

namespace tests_fn {
    bool do_coro_asserts() {
        run_tasks();
        return true;
    }
} // namespace tests_fn