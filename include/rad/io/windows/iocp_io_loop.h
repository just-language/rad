#pragma once
#include <rad/async/io_executor.h>
#include <rad/io/windows/async_waitable_timer.h>
#include <rad/io/windows/iocp.h>
#include <rad/net/types.h>

namespace RAD_LIB_NAMESPACE {
    class io_loop;
}

namespace RAD_LIB_NAMESPACE::io::detail {
    namespace details = RAD_LIB_NAMESPACE::detail;

    struct io_loop_impl {
        static constexpr uintptr_t notify_key = 0;
        static constexpr uintptr_t io_ctx_key = 1;

        io_loop_impl(uint32_t threads_num) {
            iocp_port.create(threads_num);
        }

        void interrupt() noexcept {
            std::error_code ec;
            iocp::completion_result interrupt_packet{0, notify_key, nullptr};
            iocp_port.post(interrupt_packet, ec);
            ((void)ec);
        }

        void
        set_timer_timeout(details::async_waitable_timer::duration timeout) {
            timer.set(timeout);
        }

        void attach_handle(net::socket_handle& sock,
                           std::error_code& ec) noexcept {
            iocp_port.add_handle(sock, io_ctx_key, ec);
        }

        void attach_handle(os::file_handle& file,
                           std::error_code& ec) noexcept {
            iocp_port.add_handle(file, io_ctx_key, ec);
        }

        iocp::io_port iocp_port;
        details::async_waitable_timer timer;
    };

}; // namespace RAD_LIB_NAMESPACE::io::detail