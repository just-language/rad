#pragma once
#include <liburing.h>
#include <rad/async/io_executor.h>
#include <rad/buffer.h>
#include <rad/io/linux/epoll.h>
#include <rad/stack_list.h>

namespace RAD_LIB_NAMESPACE::io {
    class io_uring_loop : noncopyable, public async_io_executor_backend {
        using op_t = rad::detail::async_op_base;
        using descriptor_data = detail::descriptor_data;
        using descriptor_data_inner_t = detail::descriptor_data_inner_t;

    public:
        io_uring_loop() noexcept {
            ring_.ring_fd = -1;
        }

        RAD_EXPORT_DECL ~io_uring_loop();

        RAD_EXPORT_DECL void init(epoll& e, bool standalone,
                                  std::error_code& ec) noexcept;

        RAD_EXPORT_DECL void close() noexcept;

        RAD_EXPORT_DECL void attach_descriptor(descriptor_data& data) noexcept;

        RAD_EXPORT_DECL void
        delete_descriptor_data(descriptor_data* p,
                               stack_forward_list<op_t>& ops) noexcept;

        RAD_EXPORT_DECL void
        submit_pending_operations(stack_forward_list<op_t>& completed,
                                  std::error_code& ec) noexcept;

        RAD_EXPORT_DECL std::size_t
        get_completions(stack_forward_list<op_t>& completed,
                        std::error_code& ec) noexcept;

        RAD_EXPORT_DECL void reset_eventfd() noexcept;

        RAD_EXPORT_DECL void
        cancel_and_wait(stack_forward_list<op_t>& completed) noexcept;

        RAD_EXPORT_DECL void
        cancel_descriptor(descriptor_data& p) noexcept override;

        RAD_EXPORT_DECL void interrupt() noexcept;

        RAD_EXPORT_DECL void submit_and_get(stack_forward_list<op_t>& completed,
                                            std::chrono::nanoseconds timeout,
                                            std::error_code& ec) noexcept;

        RAD_EXPORT_DECL void
        submit_writev(descriptor_data& d,
                      detail::descriptor_data_inner_t& inner, int fd,
                      const iovec_buff* iovecs, unsigned n_vecs,
                      uint64_t offset, std::error_code& ec) noexcept override;

        RAD_EXPORT_DECL void
        submit_readv(descriptor_data& d, detail::descriptor_data_inner_t& inner,
                     int fd, const iovec_buff* iovecs, unsigned n_vecs,
                     uint64_t offset, std::error_code& ec) noexcept override;

        RAD_EXPORT_DECL void
        submit_connect(descriptor_data& d,
                       detail::descriptor_data_inner_t& inner, int fd,
                       const void* addr, size_t addr_len,
                       std::error_code& ec) noexcept override;

        RAD_EXPORT_DECL void
        submit_accept(descriptor_data& d,
                      detail::descriptor_data_inner_t& inner, int fd,
                      void* addr, socklen_t* addr_len,
                      std::error_code& ec) noexcept override;

        RAD_EXPORT_DECL void submit_send(descriptor_data& d,
                                         detail::descriptor_data_inner_t& inner,
                                         int fd, const void* buff,
                                         std::size_t n,
                                         std::error_code& ec) noexcept override;

        RAD_EXPORT_DECL void
        submit_sendmsg(descriptor_data& d,
                       detail::descriptor_data_inner_t& inner, int fd,
                       msghdr* msg, std::error_code& ec) noexcept override;

        RAD_EXPORT_DECL void
        submit_sendto(descriptor_data& d,
                      detail::descriptor_data_inner_t& inner, int fd,
                      const void* buff, std::size_t n, const void* addr,
                      socklen_t addrlen, std::error_code& ec) noexcept override;

        RAD_EXPORT_DECL void submit_recv(descriptor_data& d,
                                         detail::descriptor_data_inner_t& inner,
                                         int fd, void* buff, std::size_t n,
                                         std::error_code& ec) noexcept override;

        RAD_EXPORT_DECL void
        submit_recvmsg(descriptor_data& d,
                       detail::descriptor_data_inner_t& inner, int fd,
                       msghdr* msg, std::error_code& ec) noexcept override;

    private:
        void cancel_descriptor_ops(descriptor_data& data,
                                   descriptor_data_inner_t& inner,
                                   stack_forward_list<op_t>& ops) noexcept;

        void cancel_ops_by_user_data(descriptor_data_inner_t& inner,
                                     void* user_data1, void* user_data2,
                                     stack_forward_list<op_t>& ops) noexcept;

        void
        submit_pending_sqes(stack_forward_list<op_t>& completed,
                            stack_forward_list<detail::io_op>& incompleted,
                            bool cancel_anyway, std::error_code& ec,
                            std::unique_lock<executor_mutex>& lock) noexcept;

        io_uring_sqe*
        try_get_sqe(stack_forward_list<op_t>& ops, std::error_code& ec,
                    std::unique_lock<executor_mutex>& lock) noexcept;

        std::size_t
        get_completions(stack_forward_list<op_t>& completed,
                        stack_forward_list<detail::io_op>& incompleted,
                        bool cancel_anyway, std::error_code& ec,
                        std::unique_lock<executor_mutex>& lock) noexcept;

        void handle_cqe(void* cqe_user_data, int res, bool cancel_anyway,
                        stack_forward_list<op_t>& completed,
                        stack_forward_list<detail::io_op>& incompleted,
                        std::unique_lock<executor_mutex>& lock) noexcept;

        void submit_if_batch_exceeded(
            descriptor_data& d, detail::descriptor_data_inner_t& inner,
            std::error_code& ec,
            std::unique_lock<executor_mutex>& lock) noexcept;

        static void submit_incompleted_operations(
            stack_forward_list<detail::io_op>& incompleted,
            stack_forward_list<op_t>& completed, bool cancel_anyway) noexcept;

        constexpr bool supports_no_drop() const noexcept {
            return (ring_.features & IORING_FEAT_NODROP) == IORING_FEAT_NODROP;
        }

        executor_mutex io_uring_lock_;
        struct io_uring ring_;
        os::file_handle event_handle_;
        std::size_t pending_sqes_ = 0;
        std::size_t pending_ops_;

        sync_value<stack_list<descriptor_data>> descriptors_;
        stack_forward_list<op_t> completions_;
        stack_forward_list<detail::io_op> incompleted_;

        descriptor_data* current_descriptor_ = nullptr;
        descriptor_data_inner_t* current_inner_ = nullptr;

        bool waiting_cqes_now_ = false;
    };
} // namespace RAD_LIB_NAMESPACE::io