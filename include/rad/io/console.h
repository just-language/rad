#pragma once
#ifdef _WIN32
#include <rad/io/windows/console_impl.h>
#elif __linux__
#include <rad/io/linux/console_impl.h>
#endif // _WIN32

namespace RAD_LIB_NAMESPACE::io {
    namespace details = RAD_LIB_NAMESPACE::detail;

    class console {
        using impl_type = detail::console_impl;
        using awaiter = impl_type::awaiter;

    public:
        using executor_type = impl_type::executor_type;

        console(executor_type& loop) : impl(loop) {
            impl.open();
        }

        executor_type& executor() noexcept {
            return impl.executor();
        }

        const executor_type& executor() const noexcept {
            return impl.executor();
        }

        std::size_t write(const_buffer buff, std::error_code& ec) noexcept {
            return impl.write(buff, ec);
        }

        std::size_t write(const_buffer buff) {
            std::error_code ec;
            auto n = write(buff, ec);
            check_and_throw(ec, __func__);
            return n;
        }

        std::size_t read(mutable_buffer buff, std::error_code& ec) noexcept {
            return impl.read(buff, ec);
        }

        std::size_t read(mutable_buffer buff) {
            std::error_code ec;
            auto n = read(buff, ec);
            check_and_throw(ec, __func__);
            return n;
        }

        template <ReadHandler Handler,
                  HandlerAllocator Alloc = default_io_allocator>
        void async_read(mutable_buffer buff, Handler&& handler,
                        const Alloc& alloc = Alloc()) {
            using op_t = detail::console_read_handler<Handler, Alloc>;
            auto op = details::allocate_op<op_t>(
                alloc, std::forward<Handler>(handler), impl.any_ex());
            auto result = impl.async_read(buff, op);
            if (result.is_pending()) {
                post_sync_rw(result, op);
            }
        }

        awaiter async_read(mutable_buffer buff) noexcept {
            return awaiter{impl, buff};
        }

        awaiter async_read(mutable_buffer buff, std::error_code& ec) noexcept {
            return awaiter{impl, buff, &ec};
        }

    private:
        template <class OldOp>
        void post_sync_rw(const async_result& result, OldOp* old_op) {
            details::async_op_base* op_ptr;
            auto& ex = impl.any_ex();
            if (!result.has_error()) {
                using op_t = details::rw_success_op<OldOp>;
                op_ptr =
                    details::reuse_op<op_t>(old_op, result.transferred(), ex);
            }
            else {
                using op_t = details::rw_failure_op<OldOp>;
                op_ptr = details::reuse_op<op_t>(old_op, result.error(), ex);
            }
            ex.post(*op_ptr);
        }

        impl_type impl;
    };
} // namespace RAD_LIB_NAMESPACE::io