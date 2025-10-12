#pragma once
#include <rad/async/io_executor.h>
#include <rad/buffer.h>
#include <rad/threading/thread_pool.h>

namespace RAD_LIB_NAMESPACE::io::detail {
    namespace details = RAD_LIB_NAMESPACE::detail;

    class console_impl;

    struct console_read_handler_base : public details::async_op_base {
        void store_results(const std::error_code& ec, std::size_t n) noexcept {
            was_read = n;
            result_ec = ec;
        }

        std::size_t was_read = 0;
        std::error_code result_ec;
    };

    template <class Handler, class Alloc>
    struct console_read_handler : public console_read_handler_base,
                                  public allocator_storage<Alloc> {
        using alloc_base = allocator_storage<Alloc>;
        using handler_type = std::decay_t<Handler>;

        handler_type handler;
        any_executor& ex;

        console_read_handler(Handler&& handler, any_executor& ex,
                             const Alloc& alloc)
            : alloc_base(alloc), handler{std::forward<Handler>(handler)},
              ex{ex} {
        }

        virtual void invoke_operation() override {
            invoke_handler(this, result_ec, was_read);
        }

        virtual any_executor& associated_executor() const noexcept override {
            return ex;
        }
    };

    struct console_read_awaiter : public console_read_handler_base {
        using handle_type = std::coroutine_handle<>;

        console_read_awaiter(console_impl& impl, mutable_buffer buff,
                             std::error_code* ec_ptr = nullptr) noexcept
            : impl{impl}, buff{buff}, ec_ptr{ec_ptr} {
        }

        constexpr bool await_ready() const noexcept {
            return false;
        }

        bool await_suspend(handle_type coro);

        std::size_t await_resume() {
            if (ec_ptr) {
                *ec_ptr = result_ec;
            }
            else {
                check_and_throw(result_ec, "async_read");
            }
            return was_read;
        }

        virtual void invoke_operation() override {
            waiting_coro.resume();
        }

        virtual any_executor& associated_executor() const noexcept override;

        console_impl& impl;
        mutable_buffer buff;
        std::error_code* ec_ptr = nullptr;
        handle_type waiting_coro;
    };

    class console_impl {
    public:
        using awaiter = console_read_awaiter;
        using executor_type = io_executor;

        console_impl(executor_type& ex) noexcept : ex_{ex} {
        }

        executor_type& executor() noexcept {
            return ex_;
        }

        const executor_type& executor() const noexcept {
            return ex_;
        }

        void open();

        std::size_t write(const_buffer buff, std::error_code& ec) noexcept {
            if (!has_console(outcon)) {
                return 0;
            }

            if (!is_valid_console(outcon)) {
                ec = std::make_error_code(std::errc::bad_file_descriptor);
                return 0;
            }

            return do_write(buff, ec);
        }

        std::size_t read(mutable_buffer buff, std::error_code& ec) noexcept {
            if (!has_console(incon)) {
                return 0;
            }
            if (!is_valid_console(incon)) {
                ec = std::make_error_code(std::errc::bad_file_descriptor);
                return 0;
            }
            return do_read(buff, ec);
        }

        async_result async_read(mutable_buffer buff,
                                console_read_handler_base* op) {
            if (!has_console(incon)) {
                return async_result::success(0);
            }

            if (!is_valid_console(incon)) {
                return async_result::failed(
                    std::make_error_code(std::errc::bad_file_descriptor));
            }

            do_async_read(buff, op);
            return async_result::pending();
        }

        any_executor& any_ex() noexcept {
            return ex_->as_any_executor();
        }

    private:
        std::size_t do_write(const_buffer buff, std::error_code& ec) noexcept;

        std::size_t do_read(mutable_buffer buff, std::error_code& ec) noexcept;

        void do_async_read(mutable_buffer buff, console_read_handler_base* op);

        static bool has_console(void* c) noexcept {
            return c != nullptr;
        }

        static bool is_valid_console(void* h) noexcept {
            return h != (void*)((intptr_t)-1);
        }

        ref<io_executor> ex_;
        thread_pool reader_thd_;
        mutex inlock;
        mutex outlock;
        mutex errlock;
        void* incon = nullptr;
        void* outcon = nullptr;
        void* errcon = nullptr;
    };

    inline bool console_read_awaiter::await_suspend(handle_type coro) {
        waiting_coro = coro;

        auto result = impl.async_read(buff, this);
        if (result.is_pending()) {
            return true;
        }

        store_results(result.error(), result.transferred());
        return false;
    }

    inline any_executor&
    console_read_awaiter::associated_executor() const noexcept {
        return impl.any_ex();
    }
} // namespace RAD_LIB_NAMESPACE::io::detail