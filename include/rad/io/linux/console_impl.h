#pragma once
#include <rad/async/io_loop.h>
#include <rad/buffer.h>

namespace RAD_LIB_NAMESPACE::io::detail {
    namespace details = RAD_LIB_NAMESPACE::detail;

    struct console_read_handler_base : public details::async_op_base {};

    class console_impl {
    public:
        using executor_type = io_loop;
        using strand_type = strand<io_loop>;

        console_impl(executor_type& loop) noexcept : loop_{loop} {
        }

        console_impl(strand_type& st) noexcept
            : loop_{st.executor()}, st_{&st} {
        }

        executor_type& executor() noexcept {
            return *loop_;
        }

        const executor_type& executor() const noexcept {
            return *loop_;
        }

        pointer<strand_type> strand_executor() noexcept {
            return st_;
        }

        pointer<const strand_type> strand_executor() const noexcept {
            return st_;
        }

        void open() {
        }

        std::size_t write(const_buffer buff, std::error_code& ec) noexcept {
            return 0;
        }

        std::size_t read(mutable_buffer buff, std::error_code& ec) noexcept {
            return 0;
        }

        async_result async_read(mutable_buffer buff,
                                console_read_handler_base* op) {
        }

    private:
        ref<io_loop> loop_;
        pointer<strand_type> st_ = nullptr;
    };
} // namespace RAD_LIB_NAMESPACE::io::detail