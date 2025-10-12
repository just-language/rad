#pragma once
#include <rad/async/executor.h>
#include <rad/async/io_executor.h>
#include <rad/buffer.h>
#include <rad/io/linux/async_waitable_timer.h>
#include <rad/io/linux/epoll.h>
#include <rad/stack_forward_list.h>
#include <rad/stack_list.h>
#include <rad/threading/synchronized_value.h>

#include <atomic>
#include <mutex>
#include <thread>

namespace RAD_LIB_NAMESPACE {
    class io_loop;
}

namespace RAD_LIB_NAMESPACE::io::detail {

    struct io_loop_impl {
        RAD_EXPORT_DECL io_loop_impl(uint32_t threads_num);

        RAD_EXPORT_DECL ~io_loop_impl();

        RAD_EXPORT_DECL void interrupt() noexcept;

        RAD_EXPORT_DECL void reset_interrupter() noexcept;

        void attach_handle(os::handle& fd, descriptor_data& data,
                           std::error_code& ec) noexcept {
            reactor_fd.attach_writable_handle(fd, &data, ec);
        }

        void attach_handle(os::handle& fd, descriptor_data& data) {
            std::error_code ec;
            attach_handle(fd, data, ec);
            check_and_throw(ec, __func__);
        }

        void set_timer_timeout(std::chrono::steady_clock::duration timeout) {
            timer_.set(timeout);
        }

        RAD_EXPORT_DECL void on_child_fork(std::error_code& ec) noexcept;

        epoll reactor_fd;
        os::handle interrupter;
        sync_value<stack_forward_list<io_op>> pending_ops;
        rad::detail::async_waitable_timer timer_ = {reactor_fd};
    };
} // namespace RAD_LIB_NAMESPACE::io::detail
