#pragma once
#include <rad/async/io_executor.h>
#include <rad/io/windows/iocp.h>
#include <rad/net/socket_options.h>
#include <rad/buffer.h>
#include <rad/net/types.h>
#include <rad/os_types.h>

namespace RAD_LIB_NAMESPACE::net::detail {
    namespace details = RAD_LIB_NAMESPACE::detail;

    class async_socket_impl : public trackable {
        using io_op = io::detail::io_op;
        template <class OriginalOp>
        using replaced_op_t = details::replaced_op_t<OriginalOp>;

        template <class SocketType>
        struct accept_socket_storage_t;

        template <class Endpoint>
        struct accept_endpoint_storage_t;

        struct awaiter_common; // provide await_ready() for all
                               // awaiters

        struct connect_awaiter;
        struct write_awaiter;
        struct read_awaiter;
        struct sendto_awaiter;

        template <class SocketStorage, class EndpointStorage>
        struct accept_awaiter;
        template <class Endpoint>
        struct recvfrom_awaiter;
        template <class Protocol, class EndpointRange>
        struct connect_range_awaiter;

        friend write_awaiter;
        friend read_awaiter;
        friend sendto_awaiter;
        friend connect_awaiter;

        template <class Handler, class Alloc>
        struct connect_op;
        template <class Handler, class Alloc>
        struct write_op;
        template <class Handler, class Alloc>
        struct read_op;
        template <class Endpoint, class Handler, class Alloc>
        struct recvfrom_op;
        template <class Protocol, class EndpointRange, class Handler,
                  class Alloc>
        struct connect_range_op;

        template <class OldOp>
        using connect_success_op = details::ec_success_op<OldOp>;
        template <class OldOp>
        using connect_failure_op = details::ec_failure_op<OldOp>;

        template <class OldOp, class Endpoint>
        struct connect_r_success_op final : replaced_op_t<OldOp> {
            using base = replaced_op_t<OldOp>;
            using handler_t = typename OldOp::handler_t;

            handler_t handler;
            Endpoint addr;

            template <class Alloc>
            connect_r_success_op(handler_t&& handler, const Endpoint& addr,
                                 any_executor& ex, const Alloc& alloc)
                : base(ex, alloc), handler{std::move(handler)}, addr{addr} {
            }

            virtual void invoke_operation() override {
                details::invoke_reused_op_handler(this, std::error_code{},
                                                  Endpoint{addr});
            }
        };

        template <class OldOp, class Endpoint>
        struct connect_r_failure_op final : replaced_op_t<OldOp> {
            using base = replaced_op_t<OldOp>;
            using handler_t = typename OldOp::handler_t;

            handler_t handler;
            std::error_code ec;

            template <class Alloc>
            connect_r_failure_op(handler_t&& handler, const std::error_code& ec,
                                 any_executor& ex, const Alloc& alloc)
                : base(ex, alloc), handler{std::move(handler)}, ec{ec} {
            }

            virtual void invoke_operation() override {
                details::invoke_reused_op_handler(this, std::error_code{ec},
                                                  Endpoint{});
            }
        };

        template <class OldOp>
        using rw_success_op = details::rw_success_op<OldOp>;

        template <class OldOp>
        using rw_failure_op = details::rw_failure_op<OldOp>;

        template <typename, typename>
        friend struct accept_awaiter;
        template <typename>
        friend struct recvfrom_awaiter;
        template <typename, typename>
        friend struct connect_range_awaiter;

        template <typename, typename>
        friend struct connect_op;
        template <typename, typename>
        friend struct write_op;
        template <typename, typename>
        friend struct read_op;
        template <typename, typename, typename>
        friend struct recvfrom_op;
        template <typename, typename, typename, typename>
        friend struct connect_range_op;

    public:
        using native_fd_type = socket_fd_t;
        using native_handle_type = socket_handle;
        using size_type = std::size_t;
        using executor_type = io_executor;

        template <class AllocatorTypes>
        static constexpr std::size_t write_allocator_size() noexcept {
            if constexpr (!(AllocatorTypes::op_type & op_alloc_type::write)) {
                return 0;
            }
            else {
                using handler = handler_allocator_size_calculator<
                    AllocatorTypes::max_handler_size>;
                return sizeof(write_op<handler, stateful_null_allocator>);
            }
        }

        template <class AllocatorTypes>
        static constexpr std::size_t read_allocator_size() noexcept {
            if constexpr (!(AllocatorTypes::op_type & op_alloc_type::read)) {
                return 0;
            }
            else {
                using handler = handler_allocator_size_calculator<
                    AllocatorTypes::max_handler_size>;
                return sizeof(read_op<handler, stateful_null_allocator>);
            }
        }

        template <class AllocatorTypes>
        static constexpr std::size_t sendto_allocator_size() noexcept {
            if constexpr (!(AllocatorTypes::op_type & op_alloc_type::sendto)) {
                return 0;
            }
            else {
                using handler = handler_allocator_size_calculator<
                    AllocatorTypes::max_handler_size>;
                return sizeof(write_op<handler, stateful_null_allocator>);
            }
        }

        template <class AllocatorTypes>
        static constexpr std::size_t recvfrom_allocator_size() noexcept {
            if constexpr (!(AllocatorTypes::op_type &
                            op_alloc_type::recvfrom)) {
                return 0;
            }
            else {
                using handler = handler_allocator_size_calculator<
                    AllocatorTypes::max_handler_size>;
                return sizeof(
                    recvfrom_op<typename AllocatorTypes::endpoint_type, handler,
                                stateful_null_allocator>);
            }
        }

        template <class AllocatorTypes>
        static constexpr std::size_t connect_allocator_size() noexcept {
            if constexpr (!(AllocatorTypes::op_type & op_alloc_type::connect)) {
                return 0;
            }
            else {
                using handler = handler_allocator_size_calculator<
                    AllocatorTypes::max_handler_size>;
                return sizeof(connect_op<handler, stateful_null_allocator>);
            }
        }

        template <class AllocatorTypes>
        static constexpr std::size_t connect_range_allocator_size() noexcept {
            if constexpr (!(AllocatorTypes::op_type &
                            op_alloc_type::connect_range)) {
                return 0;
            }
            else {
                using handler = handler_allocator_size_calculator<
                    AllocatorTypes::max_handler_size>;
                return sizeof(
                    connect_range_op<typename AllocatorTypes::protocol_type,
                                     typename AllocatorTypes::endpoints_range,
                                     handler, stateful_null_allocator>);
            }
        }

        static constexpr uint8_t max_sync_in_ops_completions = 2;

        async_socket_impl(executor_type& ex) noexcept : ex_{ex} {
        }

        async_socket_impl(executor_type& ex, socket_fd_t sock_fd) noexcept
            : ex_{ex}, sock_fd_{sock_fd} {
            io::detail::descriptor_data unused;
            std::error_code ec;
            ex.attach_handle(native_handle(), unused, ec);
            check_and_throw(ec, "");
        }

        async_socket_impl(executor_type& ex,
                          native_handle_type& sock_fd) noexcept
            : ex_{ex} {
            io::detail::descriptor_data unused;
            std::error_code ec;
            ex.attach_handle(sock_fd, unused, ec);
            check_and_throw(ec, "");
            sock_fd_ = std::move(sock_fd);
        }

        // ex must be a strand whose inner executor is the same
        // as the current executor
        void rebind_to_executor(executor_type& ex) noexcept {
            ex_ = ex;
        }

        executor_type& executor() noexcept {
            return ex_;
        }

        const executor_type& executor() const noexcept {
            return ex_;
        }

        native_fd_type native_fd() const noexcept {
            return native_handle().get();
        }

        native_handle_type& native_handle() noexcept {
            return sock_fd_;
        }

        const native_handle_type& native_handle() const noexcept {
            return sock_fd_;
        }

        bool is_open() const noexcept {
            return static_cast<bool>(native_handle());
        }

        void close() noexcept {
            sock_fd_.reset();
        }

        RAD_EXPORT_DECL void open(address_family af, socket_type type,
                                  protocol_type proto,
                                  std::error_code& ec) noexcept;

        RAD_EXPORT_DECL void cancel() noexcept;

        RAD_EXPORT_DECL void shutdown(socket_shutdown how,
                                      std::error_code& ec) noexcept;

        RAD_EXPORT_DECL void set_option(socket_option_level level,
                                        socket_option_name optname,
                                        const void* optdata, socklen_t optlen,
                                        std::error_code& ec) noexcept;

        RAD_EXPORT_DECL void get_option(socket_option_level level,
                                        socket_option_name optname,
                                        void* optdata, socklen_t& optlen,
                                        std::error_code& ec) const noexcept;

        RAD_EXPORT_DECL void local_endpoint(void* address, socklen_t& size,
                                            std::error_code& ec) const noexcept;

        RAD_EXPORT_DECL void
        remote_endpoint(void* address, socklen_t& size,
                        std::error_code& ec) const noexcept;

        static constexpr int max_listen_backlog() noexcept {
            // #define SOMAXCONN       0x7fffffff
            return 0x7fffffff;
        }

        RAD_EXPORT_DECL void listen(uint32_t backlog,
                                    std::error_code& ec) noexcept;

        RAD_EXPORT_DECL void bind(const void* address, socklen_t size,
                                  std::error_code& ec) noexcept;

        RAD_EXPORT_DECL native_handle_type accept(void* address,
                                                  socklen_t& size,
                                                  std::error_code& ec) noexcept;

        RAD_EXPORT_DECL void connect(const void* address, socklen_t size,
                                     std::error_code& ec) noexcept;

        RAD_EXPORT_DECL std::size_t send(const const_buffer* buffs,
                                         std::size_t n, transfer_flags flags,
                                         std::error_code& ec) noexcept;

        RAD_EXPORT_DECL std::size_t send_to(const const_buffer* buffs,
                                            std::size_t n, transfer_flags flags,
                                            const void* address, socklen_t size,
                                            std::error_code& ec) noexcept;

        RAD_EXPORT_DECL std::size_t receive(const mutable_buffer* buffs,
                                            std::size_t n, bool not_zero,
                                            transfer_flags flags,
                                            std::error_code& ec) noexcept;

        RAD_EXPORT_DECL std::size_t receive_from(const mutable_buffer* buffs,
                                                 std::size_t n, bool not_zero,
                                                 transfer_flags flags,
                                                 void* address, socklen_t& size,
                                                 std::error_code& ec) noexcept;

        template <class Protocol>
        auto async_accept(std::error_code& ec = no_ec) {
            make_error_if_closed(ec, "async_accept");

            using s_storage_t =
                accept_socket_storage_t<typename Protocol::socket>;
            using e_storage_t =
                accept_endpoint_storage_t<typename Protocol::endpoint_type>;
            using awaiter_t = accept_awaiter<s_storage_t, e_storage_t>;

            return awaiter_t{*this, s_storage_t{executor(), bound_af_},
                             e_storage_t{}, ec};
        }

        template <class EndpointType, class SocketType>
        auto async_accept_s(SocketType& s, std::error_code& ec = no_ec) {
            make_error_if_closed(ec, "async_accept");

            using s_storage_t = accept_socket_storage_t<SocketType&>;
            using e_storage_t = accept_endpoint_storage_t<EndpointType>;
            using awaiter_t = accept_awaiter<s_storage_t, e_storage_t>;

            return awaiter_t{*this, s_storage_t{s, bound_af_}, e_storage_t{},
                             ec};
        }

        template <class SocketType, class EndpointType>
        auto async_accept_e(EndpointType& e, std::error_code& ec = no_ec) {
            make_error_if_closed(ec, "async_accept");

            using s_storage_t = accept_socket_storage_t<SocketType>;
            using e_storage_t = accept_endpoint_storage_t<EndpointType&>;
            using awaiter_t = accept_awaiter<s_storage_t, e_storage_t>;

            return awaiter_t{*this, s_storage_t{executor(), bound_af_},
                             e_storage_t{e}, ec};
        }

        template <class SocketType, class EndpointType>
        auto async_accept_se(SocketType& s, EndpointType& e,
                             std::error_code& ec = no_ec) {
            make_error_if_closed(ec, "async_accept");

            using s_storage_t = accept_socket_storage_t<SocketType&>;
            using e_storage_t = accept_endpoint_storage_t<EndpointType&>;
            using awaiter_t = accept_awaiter<s_storage_t, e_storage_t>;

            return awaiter_t{*this, s_storage_t{s, bound_af_}, e_storage_t{e},
                             ec};
        }

        template <class Protocol>
        connect_awaiter
        async_connect(const typename Protocol::endpoint_type& peer_address,
                      std::error_code& ec);

        template <class Protocol, class EndpointRange>
        connect_range_awaiter<Protocol, EndpointRange>
        async_connect(const EndpointRange& endpoints, std::error_code& ec) {
            return {*this, endpoints, ec};
        }

        template <class Buffers>
        write_awaiter async_write(const Buffers& buffers, transfer_flags flags,
                                  std::error_code& ec);

        template <class Buffers>
        read_awaiter async_read(const Buffers& buffers, bool not_zero,
                                transfer_flags flags, std::error_code& ec);

        template <class Buffers, class Endpoint>
        sendto_awaiter
        async_send_to(const Buffers& buffers, transfer_flags flags,
                      const Endpoint& receiver, std::error_code& ec);

        template <class Endpoint>
        recvfrom_awaiter<Endpoint>
        async_receive_from(const mutable_buffer* buffs, std::size_t n,
                           bool not_zero, transfer_flags flags,
                           Endpoint& sender, std::error_code& ec) {
            make_error_if_closed(ec, "async_receive_from");
            return {*this,  buffs, static_cast<uint32_t>(n), flags, not_zero,
                    sender, ec};
        }

        template <class Buffers, class Endpoint>
        recvfrom_awaiter<Endpoint>
        async_receive_from(const Buffers& buffers, transfer_flags flags,
                           Endpoint& sender, std::error_code& ec) {
            make_error_if_closed(ec, "async_receive_from");
            auto [buffs, n] = extract_buffers<false>(buffers);
            return {*this, buffs, static_cast<uint32_t>(n), flags, sender, ec};
        }

        template <class Protocol, class Handler, class Alloc>
        void async_connect(const typename Protocol::endpoint_type& peer_address,
                           Handler&& handler, const Alloc& alloc) {
            if (!is_open() || open_af_ != peer_address.family()) {
                Protocol protocol{peer_address.family()};
                std::error_code open_ec;
                open(protocol.family(), protocol.type(), protocol.protocol(),
                     open_ec);
                if (open_ec) {
                    return post(
                        any_ex(),
                        [ec = open_ec, handler = std::forward<Handler>(
                                           handler)]() mutable { handler(ec); },
                        alloc);
                }
            }

            using op_t = connect_op<Handler, Alloc>;
            op_t* op = details::allocate_op<op_t>(
                alloc, *this, std::forward<Handler>(handler));
            auto result = do_async_connect(peer_address.address(),
                                           peer_address.size(), *op);
            if (result.is_pending()) {
                return;
            }

            post_sync_connect(result, op);
        }

        template <class Protocol, class EndpointRange, class Handler,
                  class Alloc>
        void async_connect_r(EndpointRange&& endpoints, Handler&& handler,
                             const Alloc& alloc) {
            using endpoint_type = typename Protocol::endpoint_type;

            if (std::empty(endpoints)) {
                post(
                    any_ex(),
                    [handler = std::forward<Handler>(handler)]() mutable {
                        handler(std::make_error_code(
                                    std::errc::destination_address_required),
                                endpoint_type{});
                    },
                    alloc);
                return;
            }

            using op_t =
                connect_range_op<Protocol, EndpointRange, Handler, Alloc>;
            op_t* op = details::allocate_op<op_t>(
                alloc, *this, std::forward<Handler>(handler),
                std::forward<EndpointRange>(endpoints));
            auto result = op->start_connect_op(
                std::make_error_code(std::errc::destination_address_required));
            if (result.is_pending()) {
                return;
            }

            post_sync_connect_r<endpoint_type>(result, op);
        }

        template <class Buffers, class Handler, class Alloc>
        void async_write(const Buffers& buffers, Handler&& handler,
                         transfer_flags flags, const Alloc& alloc) {
            if (!is_open()) {
                post(
                    any_ex(),
                    [handler = std::forward<Handler>(handler)]() mutable {
                        handler(std::make_error_code(
                                    std::errc::bad_file_descriptor),
                                0);
                    },
                    alloc);
                return;
            }

            auto [buffs, n] = extract_buffers<true>(buffers);

            using op_t = write_op<Handler, Alloc>;
            op_t* op = details::allocate_op<op_t>(
                alloc, *this, std::forward<Handler>(handler));

            auto result =
                do_async_write(buffs, static_cast<uint32_t>(n), flags, *op);
            if (result.is_pending()) {
                return;
            }

            post_sync_rw(result, op);
        }

        template <class Buffers, class Handler, class Alloc>
        void async_read(const Buffers& buffers, Handler&& handler,
                        bool not_zero, transfer_flags flags,
                        const Alloc& alloc) {
            if (!is_open()) {
                post(
                    any_ex(),
                    [handler = std::forward<Handler>(handler)]() mutable {
                        handler(std::make_error_code(
                                    std::errc::bad_file_descriptor),
                                0);
                    },
                    alloc);
                return;
            }
            auto [buffs, n] = extract_buffers<false>(buffers);
            using op_t = read_op<Handler, Alloc>;
            op_t* op = details::allocate_op<op_t>(
                alloc, *this, std::forward<Handler>(handler), not_zero);
            auto result = do_async_read(buffs, static_cast<uint32_t>(n), flags,
                                        not_zero, *op);
            if (result.is_pending()) {
                return;
            }
            post_sync_rw(result, op);
        }

        template <class Buffers, class Endpoint, class Handler, class Alloc>
        void async_send_to(const Buffers& buffers, Handler&& handler,
                           const Endpoint& reciever, transfer_flags flags,
                           const Alloc& alloc) {
            if (!is_open()) {
                post(
                    any_ex(),
                    [handler = std::forward<Handler>(handler)]() mutable {
                        handler(std::make_error_code(
                                    std::errc::bad_file_descriptor),
                                0);
                    },
                    alloc);
                return;
            }

            auto [buffs, n] = extract_buffers<true>(buffers);

            using op_t = write_op<Handler, Alloc>;
            op_t* op = details::allocate_op<op_t>(
                alloc, *this, std::forward<Handler>(handler));

            auto result =
                do_async_sendto(buffs, static_cast<uint32_t>(n), flags,
                                reciever.address(), reciever.size(), *op);
            if (result.is_pending()) {
                return;
            }

            post_sync_rw(result, op);
        }

        template <class Buffers, class Endpoint, class Handler, class Alloc>
        void async_receive_from(const Buffers& buffers, Handler&& handler,
                                Endpoint& sender, transfer_flags flags,
                                const Alloc& alloc) {
            if (!is_open()) {
                post(
                    any_ex(),
                    [handler = std::forward<Handler>(handler)]() mutable {
                        handler(std::make_error_code(
                                    std::errc::bad_file_descriptor),
                                0);
                    },
                    alloc);
                return;
            }
            auto [buffs, n] = extract_buffers<false>(buffers);
            using op_t = recvfrom_op<Endpoint, Handler, Alloc>;
            op_t* op = details::allocate_op<op_t>(
                alloc, *this, std::forward<Handler>(handler), sender);
            auto result =
                do_async_recvfrom(buffs, static_cast<uint32_t>(n), flags,
                                  sender.address(), &op->size, *op);
            if (result.is_pending()) {
                return;
            }
            if (!result.has_error()) {
                sender.resize(op->size);
            }
            post_sync_rw(result, op);
        }

    private:
        RAD_EXPORT_DECL async_result
        do_async_accept(native_handle_type& new_sock, void* addrs,
                        DWORD one_addr_size, io_op& op) noexcept;

        RAD_EXPORT_DECL async_result do_async_connect(const void* address,
                                                      socklen_t size,
                                                      io_op& op) noexcept;

        RAD_EXPORT_DECL async_result do_async_write(const const_buffer* buffs,
                                                    uint32_t n,
                                                    transfer_flags flags,
                                                    io_op& op) noexcept;

        RAD_EXPORT_DECL async_result do_async_sendto(
            const const_buffer* buffs, uint32_t n, transfer_flags flags,
            const void* address, socklen_t size, io_op& op) noexcept;

        RAD_EXPORT_DECL async_result do_async_read(const mutable_buffer* buffs,
                                                   uint32_t n,
                                                   transfer_flags flags,
                                                   bool not_zero,
                                                   io_op& op) noexcept;

        RAD_EXPORT_DECL async_result do_async_recvfrom(
            const mutable_buffer* buffs, uint32_t n, transfer_flags flags,
            void* address, socklen_t* size, io_op& op) noexcept;

        RAD_EXPORT_DECL std::pair<const uint8_t*, socklen_t>
        get_accept_result(io_op& op, uint8_t* addrs, DWORD one_addr_size,
                          std::error_code& ec) noexcept;

        RAD_EXPORT_DECL std::error_code get_connect_result(io_op& op) noexcept;

        RAD_EXPORT_DECL std::size_t
        get_write_result(io_op& op, std::error_code& ec) noexcept;

        RAD_EXPORT_DECL std::size_t
        get_read_result(io_op& op, bool not_zero, std::error_code& ec) noexcept;

        // returns a span for the remote address
        RAD_EXPORT_DECL static std::pair<const uint8_t*, socklen_t>
        parse_accept_addrs(uint8_t* addrs, DWORD one_addr_size) noexcept;

        void report_error(std::error_code& out_ec, const std::error_code& in_ec,
                          const char* msg) {
            if (use_exceptions(out_ec)) {
                throw std::system_error(in_ec, msg);
            }
            else {
                out_ec = in_ec;
            }
        }

        void make_error_if_closed(std::error_code& ec, const char* msg) {
            if (!is_open()) {
                report_error(
                    ec, std::make_error_code(std::errc::bad_file_descriptor),
                    msg);
            }
        }

        template <class OldOp>
        void post_sync_connect(const async_result& result, OldOp* old_op) {
            details::async_op_base* op_ptr;
            if (!result.has_error()) {
                using op_t = connect_success_op<OldOp>;
                op_ptr = details::reuse_op<op_t>(old_op, any_ex());
            }
            else {
                using op_t = connect_failure_op<OldOp>;
                op_ptr =
                    details::reuse_op<op_t>(old_op, result.error(), any_ex());
            }
            ex_->as_any_executor().post(*op_ptr);
        }

        template <class Endpoint, class OldOp>
        void post_sync_connect_r(const async_result& result, OldOp* old_op) {
            details::async_op_base* op_ptr;
            if (!result.has_error()) {
                old_op->sync_success = true;
                op_ptr = old_op;
            }
            else {
                using op_t = connect_r_failure_op<OldOp, Endpoint>;
                op_ptr = details::reuse_op<op_t>(
                    old_op, std::error_code{result.error()}, any_ex());
            }
            ex_->as_any_executor().post(*op_ptr);
        }

        template <class OldOp>
        void post_sync_rw(const async_result& result, OldOp* old_op) {
            details::async_op_base* op_ptr;
            if (!result.has_error()) {
                using op_t = rw_success_op<OldOp>;
                op_ptr = details::reuse_op<op_t>(old_op, result.transferred(),
                                                 any_ex());
            }
            else {
                using op_t = rw_failure_op<OldOp>;
                op_ptr =
                    details::reuse_op<op_t>(old_op, result.error(), any_ex());
            }
            ex_->as_any_executor().post(*op_ptr);
        }

        any_executor& any_ex() noexcept {
            return ex_->as_any_executor();
        }

        ref<executor_type> ex_;
        native_handle_type sock_fd_;
        address_family open_af_ = address_family::unspecified;
        address_family bound_af_ = address_family::unspecified;
        bool skip_iocp_on_success_ = false;
        uint8_t sync_completed_in_ops_ = 0;
    };

    // stores socket object
    template <class SocketType>
    struct async_socket_impl::accept_socket_storage_t {
        static constexpr bool holds_ref = false;
        using protocol_type = typename SocketType::protocol_type;

        accept_socket_storage_t(executor_type& ex, address_family af)
            : sock{ex, protocol_type{af}} {
        }

        auto& native_handle() noexcept {
            return sock.native_handle();
        }

        template <class Opt>
        void set_option(const Opt& opt, std::error_code& ec) {
            sock.set_option(opt, ec);
        }

        SocketType sock;
    };

    // stores reference to socket
    template <class SocketType>
    struct async_socket_impl::accept_socket_storage_t<SocketType&> {
        static constexpr bool holds_ref = true;
        using protocol_type = typename SocketType::protocol_type;

        accept_socket_storage_t(SocketType& s, address_family af) noexcept
            : s_{s} {
            if (!s_->is_open()) {
                s_->open(protocol_type{af});
            }
        }

        auto& native_handle() noexcept {
            return s_->native_handle();
        }

        template <class Opt>
        void set_option(const Opt& opt, std::error_code& ec) {
            s_->set_option(opt, ec);
        }

        ref<SocketType> s_;
    };

    template <class Endpoint>
    struct async_socket_impl::accept_endpoint_storage_t {
        static constexpr bool holds_ref = false;
        static constexpr DWORD address_size = Endpoint::max_size();

        void set_address(const void* addr, socklen_t len) noexcept {
            address.set_address(addr, len);
        }

        Endpoint address;
    };

    template <class Endpoint>
    struct async_socket_impl::accept_endpoint_storage_t<Endpoint&> {
        static constexpr bool holds_ref = true;
        static constexpr DWORD address_size = Endpoint::max_size();

        accept_endpoint_storage_t(Endpoint& address) noexcept
            : address{address} {
        }

        void set_address(const void* addr, socklen_t len) noexcept {
            address.set_address(addr, len);
        }

        Endpoint& address;
    };

    struct async_socket_impl::awaiter_common : public noncopyable,
                                               public error_storage,
                                               public io_op {
        ref<async_socket_impl> impl;
        std::coroutine_handle<> waiter;

        awaiter_common(async_socket_impl& impl, std::error_code& ec,
                       details::async_op_type type) noexcept
            : error_storage(ec), io_op(type), impl{impl} {
        }

        bool await_ready() const noexcept {
            return has_error();
        }

        any_executor& associated_executor() const noexcept override {
            return impl->any_ex();
        }
    };

    template <class SocketStorage, class EndpointStorage>
    struct [[nodiscard]] async_socket_impl::accept_awaiter final
        : async_socket_impl::awaiter_common {
        static constexpr bool socket_ref = SocketStorage::holds_ref;
        static constexpr bool epoint_ref = EndpointStorage::holds_ref;
        static constexpr DWORD one_addr_size =
            EndpointStorage::address_size + 16;
        static constexpr DWORD addrs_buff_size = one_addr_size * 2;

        using base = async_socket_impl::awaiter_common;

        std::array<uint8_t, addrs_buff_size> addrs_buffer;
        SocketStorage socket_storage;
        EndpointStorage endpoint_storage;
        bool completed = false;

        accept_awaiter(async_socket_impl& impl, SocketStorage&& s_storage,
                       EndpointStorage&& e_storage,
                       std::error_code& ec) noexcept
            : base(impl, ec, details::async_op_type::accept),
              socket_storage{std::move(s_storage)},
              endpoint_storage{std::move(e_storage)} {
        }

        bool await_suspend(std::coroutine_handle<> coro) {
            waiter = coro;
            auto result = impl->do_async_accept(socket_storage.native_handle(),
                                                addrs_buffer.data(),
                                                one_addr_size, *this);
            if (result.is_pending()) {
                return true;
            }

            store(result.error());
            if (!result.has_error()) {
                auto [addr, len] = async_socket_impl::parse_accept_addrs(
                    addrs_buffer.data(), one_addr_size);
                endpoint_storage.set_address(addr, len);
                auto acceptor_fd = impl->native_fd();
                std::error_code ec;
                socket_storage.set_option(
                    socket_options::update_accept_context{acceptor_fd}, ec);
                store(ec);
            }
            if (!has_error()) {
                impl->sync_completed_in_ops_ += 1;
                if (impl->sync_completed_in_ops_ >
                    async_socket_impl::max_sync_in_ops_completions) {
                    impl->sync_completed_in_ops_ = 0;
                    // so that invoke_operation don't use
                    // get_accept_result!
                    completed = true;
                    auto& ex = associated_executor();
                    ex.post(*this);
                    // the awaiable may have been destructed!
                    return true;
                }
            }
            return false;
        }

        auto await_resume() {
            raise("async_accept");

            if constexpr (!socket_ref && !epoint_ref) {
                return std::make_pair(std::move(socket_storage.sock),
                                      endpoint_storage.address);
            }
            else if constexpr (socket_ref && !epoint_ref) {
                return endpoint_storage.address;
            }
            else if constexpr (!socket_ref && !epoint_ref) {
                return;
            }
            else if constexpr (!socket_ref && epoint_ref) {
                return std::move(socket_storage.sock);
            }
        }

        void invoke_operation() override {
            if (!completed) {
                std::error_code ec;
                auto [addr, len] = impl->get_accept_result(
                    *this, addrs_buffer.data(), one_addr_size, ec);
                store(ec);
                if (!ec) {
                    endpoint_storage.set_address(addr, len);
                    auto acceptor_fd = impl->native_fd();
                    socket_storage.set_option(
                        socket_options::update_accept_context{acceptor_fd}, ec);
                }
            }
            waiter.resume();
        }
    };

    struct [[nodiscard]] async_socket_impl::connect_awaiter final
        : awaiter_common {
        const void* address;
        socklen_t size;

        connect_awaiter(async_socket_impl& impl, const void* addr,
                        socklen_t size, std::error_code& ec) noexcept
            : awaiter_common(impl, ec, details::async_op_type::connect),
              address{addr}, size{size} {
        }

        RAD_EXPORT_DECL bool await_suspend(std::coroutine_handle<> coro);

        void await_resume() const {
            raise("async_connect");
        }

        RAD_EXPORT_DECL void invoke_operation() override;
    };

    struct [[nodiscard]] async_socket_impl::write_awaiter : awaiter_common {
        struct count_flags {
            uint32_t count;
            transfer_flags flags;

            count_flags(uint32_t n, transfer_flags f) : count{n}, flags{f} {
            }
        };

        const const_buffer* buffs;
        union {
            count_flags nf;
            std::size_t transferred;
        };

        write_awaiter(async_socket_impl& impl, const const_buffer* buffs,
                      uint32_t n, transfer_flags f,
                      std::error_code& ec) noexcept
            : awaiter_common(impl, ec, details::async_op_type::write),
              buffs{buffs}, nf{n, f} {
            if (has_error()) {
                transferred = 0;
            }
        }

        RAD_EXPORT_DECL bool await_suspend(std::coroutine_handle<> coro);

        std::size_t await_resume() const {
            raise("async_write");
            return transferred;
        }

        RAD_EXPORT_DECL void invoke_operation() override;
    };

    struct [[nodiscard]] async_socket_impl::read_awaiter : awaiter_common {
        struct count_not_zero {
            bool not_zero;
            uint32_t count;

            count_not_zero(bool b, uint32_t n) : not_zero{b}, count{n} {
            }
        };

        const mutable_buffer* buffs;
        union {
            count_not_zero bn;
            std::size_t transferred;
        };
        transfer_flags flags;

        read_awaiter(async_socket_impl& impl, const mutable_buffer* buffs,
                     uint32_t n, transfer_flags flags, bool not_zero,
                     std::error_code& ec) noexcept
            : awaiter_common(impl, ec, details::async_op_type::read),
              buffs{buffs}, bn{not_zero, n}, flags{flags} {
            if (has_error()) {
                transferred = 0;
            }
        }

        RAD_EXPORT_DECL bool await_suspend(std::coroutine_handle<> coro);

        std::size_t await_resume() const {
            raise("async_read");
            return transferred;
        }

        RAD_EXPORT_DECL void invoke_operation() override;
    };

    struct [[nodiscard]] async_socket_impl::sendto_awaiter final
        : public write_awaiter {
        const void* address;
        socklen_t address_size;

        sendto_awaiter(async_socket_impl& impl, const const_buffer* buffs,
                       uint32_t n, transfer_flags f, const void* addr,
                       socklen_t size, std::error_code& ec) noexcept
            : write_awaiter(impl, buffs, n, f, ec), address{addr},
              address_size{size} {
        }

        RAD_EXPORT_DECL bool await_suspend(std::coroutine_handle<> coro);

        std::size_t await_resume() const {
            raise("async_sendto");
            return transferred;
        }
    };

    template <class Endpoint>
    struct [[nodiscard]] async_socket_impl::recvfrom_awaiter final
        : public async_socket_impl::read_awaiter {
        using base = async_socket_impl::read_awaiter;
        Endpoint& sender;
        socklen_t size = Endpoint::max_size();

        recvfrom_awaiter(async_socket_impl& impl, const mutable_buffer* buffs,
                         uint32_t n, transfer_flags flags, Endpoint& sender,
                         std::error_code& ec) noexcept
            : base(impl, buffs, n, flags, false, ec), sender{sender} {
        }

        bool await_suspend(std::coroutine_handle<> coro) {
            waiter = coro;
            auto result = impl->do_async_recvfrom(
                buffs, bn.count, flags, sender.address(), &size, *this);
            if (result.is_pending()) {
                return true;
            }
            transferred = result.transferred();
            store(result.error());
            if (!has_error()) {
                impl->sync_completed_in_ops_ += 1;
                if (impl->sync_completed_in_ops_ >
                    async_socket_impl::max_sync_in_ops_completions) {
                    impl->sync_completed_in_ops_ = 0;
                    // so that read_awaiter::invoke_operation don't use
                    // get_read_result!
                    assert(buffs != nullptr);
                    buffs = nullptr;
                    auto& ex = associated_executor();
                    ex.post(*this);
                    // the awaiable may have been destructed!
                    return true;
                }
            }
            return false;
        }

        std::size_t await_resume() const {
            if (!has_error() ||
                error() == std::make_error_code(std::errc::message_size)) {
                sender.resize(size);
            }
            raise("async_receive_from");
            return transferred;
        }
    };

    template <class Protocol, class EndpointRange>
    struct [[nodiscard]] async_socket_impl::connect_range_awaiter final
        : public error_storage,
          public io::detail::io_op {
        using iterator_type =
            std::decay_t<decltype(std::begin(std::declval<EndpointRange>()))>;
        using endpoint_type =
            std::decay_t<decltype(*std::declval<iterator_type>())>;

        ref<async_socket_impl> impl;
        const EndpointRange& endpoints;
        iterator_type current_addr = std::begin(endpoints);
        std::coroutine_handle<> waiter;

        connect_range_awaiter(async_socket_impl& impl,
                              const EndpointRange& endpoints,
                              std::error_code& ec) noexcept
            : error_storage(ec),
              io::detail::io_op(details::async_op_type::connect_range),
              impl{impl}, endpoints{endpoints} {
        }

        // check if there is no more endpoints
        bool finished() const noexcept {
            return current_addr == std::end(endpoints);
        }

        // returns true if there is a pending operation,
        // otherwise false
        bool start_connect_op(const std::error_code& init_ec) noexcept {
            // if there is no endpoints initially then fail
            // with destination_address_required
            auto result = async_result::failed(init_ec);

            for (; current_addr != std::end(endpoints); ++current_addr) {
                if (!impl->is_open() ||
                    impl->open_af_ != current_addr->family()) {
                    Protocol protocol{current_addr->family()};
                    std::error_code open_ec;
                    impl->open(protocol.family(), protocol.type(),
                               protocol.protocol(), open_ec);
                    if (open_ec) {
                        result = async_result::failed(open_ec);
                        continue;
                    }
                }
                result = impl->do_async_connect(current_addr->address(),
                                                current_addr->size(), *this);
                if (!result.has_error()) {
                    break; // this address will be
                           // consumed if the
                           // operation has failed
                }
            }

            store(result.error());
            return result.is_pending();
        }

        bool await_ready() const noexcept {
            return has_error();
        }

        bool await_suspend(std::coroutine_handle<> coro) {
            waiter = coro;
            return start_connect_op(
                std::make_error_code(std::errc::destination_address_required));
        }

        endpoint_type await_resume() const {
            raise("async_connect");
            return finished() ? endpoint_type{} : *current_addr;
        }

        void invoke_operation() override {
            // collect result after async operation is
            // finished. if the operation was canceled,
            // don't continue trying

            constexpr int wsa_operation_aborted = 995; // WSA_OPERATION_ABORTED

            store(impl->get_connect_result(*this));

            // consume the last tried address
            if (has_error()) {
                ++current_addr;
            }

            if (has_error() && error().value() != wsa_operation_aborted &&
                !finished() && start_connect_op(error())) {
                return;
            }

            waiter.resume();
        }

        any_executor& associated_executor() const noexcept override {
            return impl->any_ex();
        }
    };

    template <class Handler, class Alloc>
    struct async_socket_impl::connect_op final : public io::detail::io_op,
                                                 allocator_storage<Alloc> {
        using alloc_base = allocator_storage<Alloc>;
        using handler_t = std::remove_cvref_t<Handler>;

        ref<async_socket_impl> impl;
        handler_t handler;

        template <class H>
        connect_op(async_socket_impl& impl, H&& handler,
                   const Alloc& alloc) noexcept
            : io::detail::io_op(details::async_op_type::connect),
              alloc_base(alloc), impl{impl}, handler{std::forward<H>(handler)} {
        }

        virtual void invoke_operation() override {
            std::error_code ec = impl->get_connect_result(*this);
            details::invoke_handler(this, ec);
        }

        virtual any_executor& associated_executor() const noexcept override {
            return impl->any_ex();
        }
    };

    template <class Handler, class Alloc>
    struct async_socket_impl::write_op final : public io::detail::io_op,
                                               allocator_storage<Alloc> {
        using alloc_base = allocator_storage<Alloc>;
        using handler_t = std::remove_cvref_t<Handler>;

        ref<async_socket_impl> impl;
        handler_t handler;

        template <class H>
        write_op(async_socket_impl& impl, H&& handler,
                 const Alloc& alloc) noexcept
            : io::detail::io_op(details::async_op_type::write),
              alloc_base(alloc), impl{impl}, handler{std::forward<H>(handler)} {
        }

        virtual void invoke_operation() override {
            std::error_code ec;
            auto transferred = impl->get_write_result(*this, ec);
            details::invoke_handler(this, ec, transferred);
        }

        virtual any_executor& associated_executor() const noexcept override {
            return impl->any_ex();
        }
    };

    template <class Handler, class Alloc>
    struct async_socket_impl::read_op final : public io::detail::io_op,
                                              allocator_storage<Alloc> {
        using alloc_base = allocator_storage<Alloc>;
        using handler_t = std::remove_cvref_t<Handler>;

        ref<async_socket_impl> impl;
        handler_t handler;
        bool not_zero;

        template <class H>
        read_op(async_socket_impl& impl, H&& handler, bool not_zero,
                Alloc& alloc) noexcept
            : io::detail::io_op(details::async_op_type::read),
              alloc_base(alloc), impl{impl}, handler{std::forward<H>(handler)},
              not_zero{not_zero} {
        }

        virtual void invoke_operation() override {
            std::error_code ec;
            auto transferred = impl->get_read_result(*this, not_zero, ec);
            details::invoke_handler(this, ec, transferred);
        }

        virtual any_executor& associated_executor() const noexcept override {
            return impl->any_ex();
        }
    };

    template <class Endpoint, class Handler, class Alloc>
    struct async_socket_impl::recvfrom_op final : public io::detail::io_op,
                                                  allocator_storage<Alloc> {
        using alloc_base = allocator_storage<Alloc>;
        using handler_t = std::remove_cvref_t<Handler>;

        ref<async_socket_impl> impl;
        handler_t handler;
        Endpoint& sender;
        socklen_t size = Endpoint::max_size();

        template <class H>
        recvfrom_op(async_socket_impl& impl, H&& handler, Endpoint& sender,
                    Alloc& alloc) noexcept
            : io::detail::io_op(details::async_op_type::recvfrom),
              alloc_base(alloc), impl{impl}, handler{std::forward<H>(handler)},
              sender{sender} {
        }

        void invoke_operation() override {
            std::error_code ec;
            auto transferred = impl->get_read_result(*this, false, ec);
            if (!ec || ec != std::make_error_code(std::errc::message_size)) {
                sender.resize(size);
            }

            details::invoke_handler(this, ec, transferred);
        }

        any_executor& associated_executor() const noexcept override {
            return impl->any_ex();
        }
    };

    template <class Protocol, class EndpointRange, class Handler, class Alloc>
    struct async_socket_impl::connect_range_op final
        : public io::detail::io_op,
          allocator_storage<Alloc> {
        using alloc_base = allocator_storage<Alloc>;
        using handler_t = std::remove_cvref_t<Handler>;
        using endpoints_t = std::remove_cvref_t<EndpointRange>;
        using iterator_type =
            std::decay_t<decltype(std::begin(std::declval<endpoints_t>()))>;
        using endpoint_type =
            std::decay_t<decltype(*std::declval<iterator_type>())>;

        ref<async_socket_impl> impl;
        handler_t handler;
        endpoints_t endpoints;
        iterator_type current_addr;
        bool sync_success = false;

        template <class H, class R>
        connect_range_op(async_socket_impl& impl, H&& handler, R&& endpoints,
                         Alloc& alloc)
            : io::detail::io_op(details::async_op_type::connect_range),
              alloc_base(alloc), impl{impl}, handler{std::forward<H>(handler)},
              endpoints{std::forward<R>(endpoints)},
              current_addr{std::begin(this->endpoints)} {
        }

        void notify(const std::error_code& ec, endpoint_type peer) {
            details::invoke_handler(this, ec, peer);
        }

        async_result start_connect_op(const std::error_code& init_ec) {
            async_result result = async_result::failed(init_ec);
            for (; current_addr != std::end(endpoints); ++current_addr) {
                if (!impl->is_open() ||
                    impl->open_af_ != current_addr->family()) {
                    Protocol protocol{current_addr->family()};
                    std::error_code open_ec;
                    impl->open(protocol.family(), protocol.type(),
                               protocol.protocol(), open_ec);
                    if (open_ec) {
                        result = async_result::failed(open_ec);
                        continue;
                    }
                }
                result = impl->do_async_connect(current_addr->address(),
                                                current_addr->size(), *this);
                if (!result.has_error()) {
                    return result;
                }
            }
            return result;
        }

        void invoke_operation() override {
            // finished sync
            if (sync_success) {
                notify(std::error_code{}, *current_addr);
            }

            std::error_code ec = impl->get_connect_result(*this);
            if (!ec) { // connected successfully
                return notify(ec, *current_addr);
            }

            // start try from the next address
            ++current_addr;
            auto result = start_connect_op(ec);

            if (result.is_pending()) {
                return;
            }

            notify(ec, current_addr != std::end(endpoints) ? *current_addr
                                                           : endpoint_type{});
        }

        any_executor& associated_executor() const noexcept override {
            return impl->any_ex();
        }
    };

    template <class Protocol>
    inline auto async_socket_impl::async_connect(
        const typename Protocol::endpoint_type& peer_address,
        std::error_code& ec) -> connect_awaiter {
        if (!is_open() || open_af_ != peer_address.family()) {
            Protocol protocol{peer_address.family()};
            std::error_code open_ec;
            open(protocol.family(), protocol.type(), protocol.protocol(),
                 open_ec);
            if (open_ec) {
                report_error(ec, open_ec, "async_connect");
            }
        }
        return {*this, peer_address.address(), peer_address.size(), ec};
    }

    template <class Buffers>
    auto async_socket_impl::async_write(const Buffers& buffers,
                                        transfer_flags flags,
                                        std::error_code& ec) -> write_awaiter {
        make_error_if_closed(ec, "async_write");
        auto [buffs, n] = extract_buffers<true>(buffers);
        return {*this, buffs, static_cast<uint32_t>(n), flags, ec};
    }

    template <class Buffers>
    auto async_socket_impl::async_read(const Buffers& buffers, bool not_zero,
                                       transfer_flags flags,
                                       std::error_code& ec) -> read_awaiter {
        make_error_if_closed(ec, "async_read");
        auto [buffs, n] = extract_buffers<false>(buffers);
        return {*this, buffs, static_cast<uint32_t>(n), flags, not_zero, ec};
    }

    template <class Buffers, class Endpoint>
    auto async_socket_impl::async_send_to(const Buffers& buffers,
                                          transfer_flags flags,
                                          const Endpoint& receiver,
                                          std::error_code& ec)
        -> sendto_awaiter {
        make_error_if_closed(ec, "async_send_to");
        auto [buffs, n] = extract_buffers<true>(buffers);
        return {*this,
                buffs,
                static_cast<uint32_t>(n),
                flags,
                receiver.address(),
                receiver.size(),
                ec};
    }
} // namespace RAD_LIB_NAMESPACE::net::detail