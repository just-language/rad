#pragma once
#include <rad/trackable.h>
#include <rad/async/io_executor.h>
#include <rad/buffer.h>
#include <rad/function_view.h>
#include <rad/io/posix/async_file_impl.h>
#include <rad/net/types.h>
#ifdef RAD_HAS_IO_URING
#include <sys/socket.h>
#endif // RAD_HAS_IO_URING

namespace RAD_LIB_NAMESPACE::net::detail {
    // detail refers to net::detail
    namespace details = RAD_LIB_NAMESPACE::detail;

    struct dummy_protocol {
        using endpoint_type = endpoint;

        dummy_protocol(address_family af) {
        }

        address_family family() const noexcept {
            return address_family::unspecified;
        }

        socket_type type() const noexcept {
            return socket_type::raw;
        }

        protocol_type protocol() const noexcept {
            return protocol_type::auto_protocol;
        }
    };

    class async_socket_impl : public trackable {
        using io_op = io::detail::io_op;
        using descriptor_data_deleter = io::detail::descriptor_data_deleter;
        using descriptor_data_ptr = io::detail::descriptor_data_ptr;
        using op_alloc_fn = function_view<io_op*()>;

        struct write_op_base;
        struct read_op_base;

        template <class SocketType>
        struct accept_socket_storage_t;
        template <class Endpoint>
        struct accept_endpoint_storage_t;

        struct awaiter_common;
        template <class SocketStorage, class EndpointStorage>
        struct accept_awaiter;
        template <class Endpoint>
        struct connect_awaiter;
        template <class EndpointRange>
        struct connect_range_awaiter;
        // template<class Buffers>
        struct write_awaiter;
        // template<class Buffers>
        struct read_awaiter;
        template <class Endpoint>
        struct sendto_awaiter;
        template <class Endpoint>
        struct recvfrom_awaiter;

        template <class Endpoint, class Handler, class Alloc>
        struct connect_op;
        template <class Handler, class Alloc>
        struct write_op;
        template <class Handler, class Alloc>
        struct read_op;
        template <class Endpoint, class Handler, class Alloc>
        struct sendto_op;
        template <class Endpoint, class Handler, class Alloc>
        struct recvfrom_op;
        template <class EndpointRange, class Handler, class Alloc>
        struct connect_range_op;

        template <typename>
        friend struct connect_awaiter;
        // template<typename>
        friend struct write_awaiter;
        // template<typename>
        friend struct read_awaiter;
        template <typename>
        friend struct sendto_awaiter;
        template <typename, typename>
        friend struct accept_awaiter;
        template <typename>
        friend struct recvfrom_awaiter;
        template <typename>
        friend struct connect_range_awaiter;

        template <typename, typename, typename>
        friend struct connect_op;
        template <typename, typename>
        friend struct write_op;
        template <typename, typename>
        friend struct read_op;
        template <typename, typename, typename>
        friend struct sendto_op;
        template <typename, typename, typename>
        friend struct recvfrom_op;
        template <typename, typename, typename>
        friend struct connect_range_op;

    public:
        using native_fd_type = socket_fd_t;
        using native_handle_type = os::handle;
        using size_type = std::size_t;
        using executor_type = io_executor;

        template <class AllocatorTypes>
        static constexpr std::size_t write_allocator_size() noexcept {
            if constexpr (!(AllocatorTypes::op_type & op_alloc_type::write)) {
                return 0;
            }
            else {
                return sizeof(write_op<handler_allocator_size_calculator<
                                           AllocatorTypes::max_handler_size>,
                                       stateful_null_allocator>);
            }
        }

        template <class AllocatorTypes>
        static constexpr std::size_t read_allocator_size() noexcept {
            if constexpr (!(AllocatorTypes::op_type & op_alloc_type::read)) {
                return 0;
            }
            else {
                return sizeof(read_op<handler_allocator_size_calculator<
                                          AllocatorTypes::max_handler_size>,
                                      stateful_null_allocator>);
            }
        }

        template <class AllocatorTypes>
        static constexpr std::size_t sendto_allocator_size() noexcept {
            if constexpr (!(AllocatorTypes::op_type & op_alloc_type::sendto)) {
                return 0;
            }
            else {
                return sizeof(sendto_op<typename AllocatorTypes::endpoint_type,
                                        handler_allocator_size_calculator<
                                            AllocatorTypes::max_handler_size>,
                                        stateful_null_allocator>);
            }
        }

        template <class AllocatorTypes>
        static constexpr std::size_t recvfrom_allocator_size() noexcept {
            if constexpr (!(AllocatorTypes::op_type &
                            op_alloc_type::recvfrom)) {
                return 0;
            }
            else {
                return sizeof(
                    recvfrom_op<typename AllocatorTypes::endpoint_type,
                                handler_allocator_size_calculator<
                                    AllocatorTypes::max_handler_size>,
                                stateful_null_allocator>);
            }
        }

        template <class AllocatorTypes>
        static constexpr std::size_t connect_allocator_size() noexcept {
            if constexpr (!(AllocatorTypes::op_type & op_alloc_type::connect)) {
                return 0;
            }
            else {
                return sizeof(connect_op<typename AllocatorTypes::endpoint_type,
                                         handler_allocator_size_calculator<
                                             AllocatorTypes::max_handler_size>,
                                         stateful_null_allocator>);
            }
        }

        template <class AllocatorTypes>
        static constexpr std::size_t connect_range_allocator_size() noexcept {
            if constexpr (!(AllocatorTypes::op_type &
                            op_alloc_type::connect_range)) {
                return 0;
            }
            else {
                return sizeof(
                    connect_range_op<typename AllocatorTypes::endpoints_range,
                                     handler_allocator_size_calculator<
                                         AllocatorTypes::max_handler_size>,
                                     stateful_null_allocator>);
            }
        }

        async_socket_impl(executor_type& ex) noexcept
            : data_{nullptr, descriptor_data_deleter{ex}} {
        }

        async_socket_impl(executor_type& ex, socket_fd_t sock_fd)
            : data_{nullptr, descriptor_data_deleter{ex}} {
            init_by_fd(sock_fd);
        }

        async_socket_impl(executor_type& ex,
                          native_handle_type& sock_fd) noexcept
            : data_{nullptr, descriptor_data_deleter{ex}} {
            init_by_handle(sock_fd);
        }

        async_socket_impl(async_socket_impl&&) noexcept = default;

        async_socket_impl& operator=(async_socket_impl&&) noexcept = default;

        ~async_socket_impl() {
            close();
        }

        executor_type& executor() noexcept {
            return *data_.get_deleter().ex;
        }

        const executor_type& executor() const noexcept {
            return *data_.get_deleter().ex;
        }

        void rebind_to_executor(io_executor& ex) {
            data_.get_deleter().ex = &ex;
        }

        native_fd_type native_fd() const noexcept {
            return native_handle().get();
        }

        native_handle_type& native_handle() noexcept {
            return data_ != nullptr ? data_->handle : dummy_file_handle;
        }

        const native_handle_type& native_handle() const noexcept {
            return data_ != nullptr ? data_->handle : dummy_file_handle;
        }

        bool is_open() const noexcept {
            return static_cast<bool>(native_handle());
        }

        void cancel() noexcept {
            if (!data_) {
                return;
            }
            data_->cancel(any_ex());
        }

        RAD_EXPORT_DECL void open(address_family af, socket_type type,
                                  protocol_type proto,
                                  std::error_code& ec) noexcept;

        void close() noexcept {
            data_.reset();
        }

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
            // #define SOMAXCONN	4096
            return 4096;
        }

        RAD_EXPORT_DECL void listen(int backlog, std::error_code& ec) noexcept;

        RAD_EXPORT_DECL void bind(const void* address, socklen_t size,
                                  std::error_code& ec) noexcept;

        // sync operations

        RAD_EXPORT_DECL native_handle_type accept(void* address,
                                                  socklen_t& size,
                                                  std::error_code& ec) noexcept;

        RAD_EXPORT_DECL void connect(const void* address, socklen_t size,
                                     std::error_code& ec) noexcept;

        std::size_t send(const const_buffer* buffs, std::size_t n,
                         transfer_flags flags, std::error_code& ec) noexcept {
            return send_to(buffs, n, flags, nullptr, 0, ec);
        }

        RAD_EXPORT_DECL std::size_t send_to(const const_buffer* buffs,
                                            std::size_t n, transfer_flags flags,
                                            const void* address, socklen_t size,
                                            std::error_code& ec) noexcept;

        std::size_t receive(const mutable_buffer* buffs, std::size_t n,
                            bool not_zero, transfer_flags flags,
                            std::error_code& ec) noexcept {
            socklen_t addr_size = 0;
            return receive_from(buffs, n, not_zero, flags, nullptr, addr_size,
                                ec);
        }

        RAD_EXPORT_DECL std::size_t receive_from(const mutable_buffer* buffs,
                                                 std::size_t n, bool not_zero,
                                                 transfer_flags flags,
                                                 void* address, socklen_t& size,
                                                 std::error_code& ec) noexcept;

        // async operations

        template <class Protocol>
        auto async_accept(std::error_code& ec = no_ec) {
            make_error_if_closed(ec, "async_accept");

            using s_storage_t =
                accept_socket_storage_t<typename Protocol::socket>;
            using e_storage_t =
                accept_endpoint_storage_t<typename Protocol::endpoint_type>;
            using awaiter_t = accept_awaiter<s_storage_t, e_storage_t>;

            return awaiter_t{*this, s_storage_t{executor()}, e_storage_t{}, ec};
        }

        template <class EndpointType, class SocketType>
        auto async_accept_s(SocketType& s, std::error_code& ec = no_ec) {
            make_error_if_closed(ec, "async_accept");

            using s_storage_t = accept_socket_storage_t<SocketType&>;
            using e_storage_t = accept_endpoint_storage_t<EndpointType>;
            using awaiter_t = accept_awaiter<s_storage_t, e_storage_t>;

            return awaiter_t{*this, s_storage_t{s}, e_storage_t{}, ec};
        }

        template <class SocketType, class EndpointType>
        auto async_accept_e(EndpointType& e, std::error_code& ec = no_ec) {
            make_error_if_closed(ec, "async_accept");

            using s_storage_t = accept_socket_storage_t<SocketType>;
            using e_storage_t = accept_endpoint_storage_t<EndpointType&>;
            using awaiter_t = accept_awaiter<s_storage_t, e_storage_t>;

            return awaiter_t{*this, s_storage_t{executor()}, e_storage_t{e},
                             ec};
        }

        template <class SocketType, class EndpointType>
        auto async_accept_se(SocketType& s, EndpointType& e,
                             std::error_code& ec = no_ec) {
            make_error_if_closed(ec, "async_accept");

            using s_storage_t = accept_socket_storage_t<SocketType&>;
            using e_storage_t = accept_endpoint_storage_t<EndpointType&>;
            using awaiter_t = accept_awaiter<s_storage_t, e_storage_t>;

            return awaiter_t{*this, s_storage_t{s}, e_storage_t{e}, ec};
        }

        template <class Protocol>
        connect_awaiter<typename Protocol::endpoint_type>
        async_connect(const typename Protocol::endpoint_type& peer_address,
                      std::error_code& ec) {
            if (!is_open()) {
                Protocol protocol{peer_address.family()};
                std::error_code open_ec;
                open(protocol.family(), protocol.type(), protocol.protocol(),
                     open_ec);
                if (open_ec) {
                    report_error(ec, open_ec, "async_connect");
                }
            }
            return connect_awaiter<typename Protocol::endpoint_type>{
                *this, peer_address, ec};
        }

        template <class Protocol, class EndpointRange>
        connect_range_awaiter<EndpointRange>
        async_connect(const EndpointRange& endpoints, std::error_code& ec) {
            if (!std::empty(endpoints) && !is_open()) {
                // try with first endpoint family
                Protocol protocol{std::begin(endpoints)->family()};
                std::error_code open_ec;
                open(protocol.family(), protocol.type(), protocol.protocol(),
                     open_ec);
                if (open_ec) {
                    report_error(ec, open_ec, "async_connect");
                }
            }
            return {*this, endpoints, ec};
        }

        template <class Buffers>
        write_awaiter async_write(const Buffers& buffers, transfer_flags flags,
                                  std::error_code& ec);

        template <class Buffers>
        read_awaiter async_read(const Buffers& buffers, bool not_zero,
                                transfer_flags flags, std::error_code& ec);

        template <class Buffers, class Endpoint>
        sendto_awaiter<Endpoint>
        async_send_to(const Buffers& buffers, transfer_flags flags,
                      const Endpoint& receiver, std::error_code& ec) {
            make_error_if_closed(ec, "async_send_to");
            return {*this, buffers, flags, receiver, ec};
        }

        template <class Buffers, class Endpoint>
        recvfrom_awaiter<Endpoint>
        async_receive_from(const Buffers& buffers, transfer_flags flags,
                           Endpoint& sender, std::error_code& ec) {
            make_error_if_closed(ec, "async_receive_from");
            return {*this, buffers, flags, sender, ec};
        }

        template <class Protocol, class Handler, class Alloc>
        void async_connect(const typename Protocol::endpoint_type& peer_address,
                           Handler&& handler, const Alloc& alloc) {
            if (!is_open()) {
                Protocol protocol{peer_address.family()};
                std::error_code open_ec;
                open(protocol.family(), protocol.type(), protocol.protocol(),
                     open_ec);
                if (open_ec) {
                    return post_early(
                        [ec = open_ec, handler = std::forward<Handler>(
                                           handler)]() mutable { handler(ec); },
                        alloc);
                }
            }

            using op_t = connect_op<typename Protocol::endpoint_type,
                                    std::remove_cvref_t<Handler>, Alloc>;
            auto alloc_fn = [&]() -> op_t* {
                return details::allocate_op<op_t>(
                    alloc, *this, peer_address, std::forward<Handler>(handler));
            };
            auto result = async_connect(peer_address.address(),
                                        peer_address.size(), alloc_fn);
            if (result.is_pending()) {
                return;
            }
            // op was not allocated
            post_early(
                [ec = result.error(), handler = std::forward<Handler>(
                                          handler)]() mutable { handler(ec); },
                alloc);
        }

        template <class Protocol, class EndpointRange, class Handler,
                  class Alloc>
        void async_connect_r(EndpointRange&& endpoints, Handler&& handler,
                             const Alloc& alloc) {
            using endpoint_type = typename Protocol::endpoint_type;

            if (std::empty(endpoints)) {
                post_early(
                    [handler = std::forward<Handler>(handler)]() mutable {
                        handler(std::make_error_code(
                                    std::errc::destination_address_required),
                                endpoint_type{});
                    },
                    alloc);
                return;
            }

            if (!is_open()) {
                Protocol protocol{std::begin(endpoints)->family()};
                std::error_code open_ec;
                open(protocol.family(), protocol.type(), protocol.protocol(),
                     open_ec);
                if (open_ec) {
                    return post_early(
                        [ec = open_ec,
                         handler = std::forward<Handler>(handler)]() mutable {
                            handler(ec, endpoint_type{});
                        },
                        alloc);
                }
            }

            using op_t = connect_range_op<std::remove_cvref_t<EndpointRange>,
                                          std::remove_cvref_t<Handler>, Alloc>;
            op_t* op = details::allocate_op<op_t>(
                alloc, *this, std::forward<Handler>(handler),
                std::forward<EndpointRange>(endpoints));
            auto result = op->start_connect_op(
                std::make_error_code(std::errc::destination_address_required));
            if (result.is_pending()) {
                return;
            }

            post_sync(op);
        }

        template <class Buffers, class Handler, class Alloc>
        void async_write(const Buffers& buffers, Handler&& handler,
                         transfer_flags flags, const Alloc& alloc) {
            if (!is_open()) {
                post_early(
                    [handler = std::forward<Handler>(handler)]() mutable {
                        handler(std::make_error_code(
                                    std::errc::bad_file_descriptor),
                                0);
                    },
                    alloc);
                return;
            }

            using op_t = write_op<std::remove_cvref_t<Handler>, Alloc>;

            io::iovec_buffers cloned_buffs{buffers};
            auto alloc_fn = [&]() -> op_t* {
                return rad::detail::allocate_op<op_t>(
                    alloc, *this, std::move(cloned_buffs),
                    std::forward<Handler>(handler));
            };

            auto result = async_send(cloned_buffs, flags, alloc_fn);
            if (result.is_pending()) {
                return;
            }
            // op was not allocated
            post_early([ec = result.error(), n = result.transferred(),
                        handler = std::forward<Handler>(
                            handler)]() mutable { handler(ec, n); },
                       alloc);
        }

        template <class Buffers, class Handler, class Alloc>
        void async_read(const Buffers& buffers, Handler&& handler,
                        bool not_zero, transfer_flags flags,
                        const Alloc& alloc) {
            if (!is_open()) {
                post_early(
                    [handler = std::forward<Handler>(handler)]() mutable {
                        handler(std::make_error_code(
                                    std::errc::bad_file_descriptor),
                                0);
                    },
                    alloc);
                return;
            }

            using op_t = read_op<std::remove_cvref_t<Handler>, Alloc>;

            io::iovec_buffers cloned_buffs{buffers};
            auto alloc_fn = [&]() -> op_t* {
                return rad::detail::allocate_op<op_t>(
                    alloc, *this, std::move(cloned_buffs),
                    std::forward<Handler>(handler), not_zero);
            };

            auto result = async_recv(cloned_buffs, flags, not_zero, alloc_fn);
            if (result.is_pending()) {
                return;
            }
            // op was not allocated
            post_early([ec = result.error(), n = result.transferred(),
                        handler = std::forward<Handler>(
                            handler)]() mutable { handler(ec, n); },
                       alloc);
        }

        template <class Buffers, class Endpoint, class Handler, class Alloc>
        void async_send_to(const Buffers& buffers, Handler&& handler,
                           const Endpoint& reciever, transfer_flags flags,
                           const Alloc& alloc) {
            if (!is_open()) {
                post_early(
                    [handler = std::forward<Handler>(handler)]() mutable {
                        handler(std::make_error_code(
                                    std::errc::bad_file_descriptor),
                                0);
                    },
                    alloc);
                return;
            }

            using op_t =
                sendto_op<Endpoint, std::remove_cvref_t<Handler>, Alloc>;

            io::iovec_buffers cloned_buffs{buffers};
            auto alloc_fn = [&]() -> op_t* {
                return rad::detail::allocate_op<op_t>(
                    alloc, *this, std::move(cloned_buffs),
                    std::forward<Handler>(handler), reciever);
            };

            auto result = async_sendto(cloned_buffs, reciever.address(),
                                       reciever.size(), flags, alloc_fn);
            if (result.is_pending()) {
                return;
            }
            // op was not allocated
            post_early([ec = result.error(), n = result.transferred(),
                        handler = std::forward<Handler>(
                            handler)]() mutable { handler(ec, n); },
                       alloc);
        }

        template <class Buffers, class Endpoint, class Handler, class Alloc>
        void async_receive_from(const Buffers& buffers, Handler&& handler,
                                Endpoint& sender, transfer_flags flags,
                                const Alloc& alloc) {
            if (!is_open()) {
                post_early(
                    [handler = std::forward<Handler>(handler)]() mutable {
                        handler(std::make_error_code(
                                    std::errc::bad_file_descriptor),
                                0);
                    },
                    alloc);
                return;
            }

            using op_t =
                recvfrom_op<Endpoint, std::remove_cvref_t<Handler>, Alloc>;

            io::iovec_buffers cloned_buffs{buffers};
            auto alloc_fn = [&]() -> op_t* {
                return rad::detail::allocate_op<op_t>(
                    alloc, *this, std::move(cloned_buffs),
                    std::forward<Handler>(handler), sender, false);
            };

            socklen_t size = Endpoint::max_size();
            auto result = async_recvfrom(cloned_buffs, sender.address(), size,
                                         flags, false, alloc_fn);
            if (result.is_pending()) {
                return;
            }

            // op was not allocated
            if (!result.has_error()) {
                sender.resize(size);
            }

            post_early([ec = result.error(), n = result.transferred(),
                        handler = std::forward<Handler>(
                            handler)]() mutable { handler(ec, n); },
                       alloc);
        }

    private:
        // sock_ is invalid and data_ is nullptr
        void init_by_fd(int fd) {
            assert(data_ == nullptr);
            std::error_code ec;
            auto data = io::detail::attach_fd_to_executor(
                fd, *data_.get_deleter().ex, ec);
            check_and_throw(ec, __func__);
            data_ = std::move(data);
        }

        void init_by_handle(native_handle_type& handle) {
            int fd = handle.get();
            init_by_fd(fd);
            handle.release();
        }

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

        template <class Handler, class Alloc>
        void post_early(Handler&& handler, const Alloc& alloc) {
            post(any_ex(), std::forward<Handler>(handler), alloc);
        }

        template <class Op>
        void post_sync(Op* op) {
            any_ex().post(*op);
        }

        async_result make_pending(std::size_t n = 0) noexcept {
            any_ex().add_work();
            return async_result::pending(n);
        }

        RAD_EXPORT_DECL async_result async_accept(
            void* address, socklen_t* size, op_alloc_fn alloc_fn) noexcept;

        // if the result is success then the result contains the
        // new accepted socket fd
        RAD_EXPORT_DECL async_result perform_accept(void* address,
                                                    socklen_t* size) noexcept;

        RAD_EXPORT_DECL async_result async_connect(
            const void* addr, socklen_t len, op_alloc_fn alloc_fn) noexcept;

        RAD_EXPORT_DECL async_result perform_connect(const void* addr,
                                                     socklen_t len) noexcept;

        // returns 0 on success, otherwise an error value
        RAD_EXPORT_DECL int get_connect_result() noexcept;

        RAD_EXPORT_DECL async_result
        async_sendto(io::iovec_buffers& buffs, const void* addr,
                     socklen_t addr_len, transfer_flags flags,
                     function_view<write_op_base*()> alloc_fn) noexcept;

        RAD_EXPORT_DECL async_result perform_sendto(
            bool first_time, io::iovec_buffers& buffs, transfer_flags flags,
            const void* addr, socklen_t addr_len) noexcept;

        RAD_EXPORT_DECL async_result
        async_recvfrom(io::iovec_buffers& buffs, void* addr,
                       socklen_t& addr_len, transfer_flags flags, bool not_zero,
                       function_view<read_op_base*()> alloc_fn) noexcept;

        RAD_EXPORT_DECL async_result perform_recvfrom(io::iovec_buffers& buffs,
                                                      transfer_flags flags,
                                                      void* addr,
                                                      socklen_t* addr_len,
                                                      bool not_zero) noexcept;

        async_result
        async_send(io::iovec_buffers& buffs, transfer_flags flags,
                   function_view<write_op_base*()> alloc_fn) noexcept {
            return async_sendto(buffs, nullptr, 0, flags, alloc_fn);
        }

        async_result perform_send(io::iovec_buffers& buffs,
                                  transfer_flags flags) noexcept {
            return perform_sendto(false, buffs, flags, nullptr, 0);
        }

        async_result
        async_recv(io::iovec_buffers& buffs, transfer_flags flags,
                   bool not_zero,
                   function_view<read_op_base*()> alloc_fn) noexcept {
            socklen_t addr_len = 0;
            return async_recvfrom(buffs, nullptr, addr_len, flags, not_zero,
                                  alloc_fn);
        }

        async_result perform_recv(io::iovec_buffers& buffs,
                                  transfer_flags flags,
                                  bool not_zero) noexcept {
            return perform_recvfrom(buffs, flags, nullptr, nullptr, not_zero);
        }

#ifdef __linux__
        RAD_EXPORT_DECL static bool accept_should_retry(int ec) noexcept;
#endif // __linux__

        any_executor& any_ex() noexcept {
            return executor().as_any_executor();
        }

        // on destruction sock_ should be destroyed before data_
        descriptor_data_ptr data_;
        // os::handle sock_;
        address_family bound_af_ = address_family::unspecified;
        inline static native_handle_type dummy_file_handle;
    };

    struct async_socket_impl::write_op_base : io::detail::io_op {
        std::size_t transferred = 0;
        transfer_flags flags = {};

        write_op_base() : io::detail::io_op(rad::detail::async_op_type::write) {
        }

        write_op_base(transfer_flags flags) noexcept : write_op_base() {
            this->flags = flags;
        }

    protected:
        ~write_op_base() = default;
    };

    struct async_socket_impl::read_op_base : io::detail::io_op {
        std::size_t transferred = 0;
        transfer_flags flags = {};
        bool not_zero = false;

        read_op_base() : io::detail::io_op(rad::detail::async_op_type::read) {
        }

        read_op_base(transfer_flags flags, bool not_zero) noexcept
            : read_op_base() {
            this->flags = flags;
            this->not_zero = not_zero;
        }

    protected:
        ~read_op_base() = default;
    };

    // stores socket object
    template <class SocketType>
    struct async_socket_impl::accept_socket_storage_t {
        static constexpr bool holds_ref = false;

        accept_socket_storage_t(io_executor& ex) : sock{ex} {
        }

        void set_fd(socket_fd_t fd) {
            sock = SocketType{sock.executor(), fd};
        }

        SocketType sock;
    };

    // stores reference to socket
    template <class SocketType>
    struct async_socket_impl::accept_socket_storage_t<SocketType&> {
        static constexpr bool holds_ref = true;

        accept_socket_storage_t(SocketType& s) noexcept : sock{s} {
        }

        void set_fd(socket_fd_t fd) {
            *sock = SocketType{sock->executor(), fd};
        }

        ref<SocketType> sock;
    };

    // stores endpoint object
    template <class Endpoint>
    struct async_socket_impl::accept_endpoint_storage_t {
        static constexpr bool holds_ref = false;
        static constexpr socklen_t address_size = Endpoint::max_size();

        void* address_ptr() noexcept {
            return address.address();
        }

        void resize(socklen_t len) noexcept {
            address.resize(len);
        }

        Endpoint address;
    };

    // stores reference to endpoint
    template <class Endpoint>
    struct async_socket_impl::accept_endpoint_storage_t<Endpoint&> {
        static constexpr bool holds_ref = true;
        static constexpr socklen_t address_size = Endpoint::max_size();

        accept_endpoint_storage_t(Endpoint& address) noexcept
            : address{address} {
        }

        void* address_ptr() noexcept {
            return address.address();
        }

        void resize(socklen_t len) noexcept {
            address.resize(len);
        }

        Endpoint& address;
    };

    // awaiters declarations

    struct async_socket_impl::awaiter_common : noncopyable, error_storage {
        ref<async_socket_impl> impl;
        std::coroutine_handle<> waiter;

        awaiter_common(async_socket_impl& impl, std::error_code& ec) noexcept
            : error_storage(ec), impl{impl} {
        }

        bool await_ready() const noexcept {
            return has_error();
        }
    };

    template <class SocketStorage, class EndpointStorage>
    struct [[nodiscard]] async_socket_impl::accept_awaiter final
        : async_socket_impl::awaiter_common,
          io::detail::io_op {
        static constexpr bool socket_ref = SocketStorage::holds_ref;
        static constexpr bool epoint_ref = EndpointStorage::holds_ref;

        using base = async_socket_impl::awaiter_common;

        SocketStorage socket_storage;
        EndpointStorage endpoint_storage;
        socklen_t endpoint_size = EndpointStorage::address_size;

        accept_awaiter(async_socket_impl& impl, SocketStorage&& s_storage,
                       EndpointStorage&& e_storage,
                       std::error_code& ec) noexcept
            : base(impl, ec), socket_storage{std::move(s_storage)},
              endpoint_storage{std::move(e_storage)} {
        }

        bool await_suspend(std::coroutine_handle<> coro) {
            waiter = coro;
            socklen_t size = EndpointStorage::address_size;
            auto result = impl->async_accept(endpoint_storage.address_ptr(),
                                             &size, [this] { return this; });
            if (result.is_pending()) {
                return true;
            }

            if (!result.has_error()) {
                socket_storage.set_fd(static_cast<socket_fd_t>(result.fd()));
                endpoint_storage.resize(size);
            }
            store(result.error());
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
            else if constexpr (socket_ref && epoint_ref) {
                return;
            }
            else if constexpr (!socket_ref && epoint_ref) {
                return std::move(socket_storage.sock);
            }
        }

        bool perform() noexcept override {
            // the lock is held
            assert(!canceled);
            socklen_t size = EndpointStorage::address_size;
            auto result =
                impl->perform_accept(endpoint_storage.address_ptr(), &size);

            if (result.is_pending()) {
                return false;
            }

            if (!result.has_error()) {
                socket_storage.set_fd(result.fd());
                endpoint_storage.resize(size);
            }
            store(result.error());
            return true;
        }

        void invoke_operation() override {
            if (canceled) {
                store(std::make_error_code(std::errc::operation_canceled));
            }
            waiter.resume();
        }

        any_executor& associated_executor() const noexcept override {
            return impl->any_ex();
        }

#ifdef RAD_HAS_IO_URING
        void submit(io::detail::descriptor_data_inner_t& inner,
                    std::error_code& ec) noexcept override {
            endpoint_size = EndpointStorage::address_size;
            descriptor->uring_backend->submit_accept(
                *descriptor, inner, impl->native_fd(),
                endpoint_storage.address_ptr(), &endpoint_size, ec);
        }

        bool complete(const std::error_code& ec, int result) noexcept override {
            store(ec);
            if (!canceled &&
                async_socket_impl::accept_should_retry(ec.value())) {
                return false;
            }
            if (result <= 0) {
                return true;
            }
            socket_storage.set_fd(result);
            endpoint_storage.resize(endpoint_size);
            return true;
        }
#endif // RAD_HAS_IO_URING
    };

    template <class Endpoint>
    struct [[nodiscard]] RAD_EXPORT_VTABLE
        async_socket_impl::connect_awaiter final
        : async_socket_impl::awaiter_common,
          io::detail::io_op {
        Endpoint endpoint;

        connect_awaiter(async_socket_impl& impl, const Endpoint& endpoint,
                        std::error_code& ec) noexcept
            : awaiter_common(impl, ec), endpoint{endpoint} {
        }

        bool await_suspend(std::coroutine_handle<> coro) {
            waiter = coro;
            auto result = impl->async_connect(
                endpoint.address(), endpoint.size(), [this] { return this; });
            if (result.is_pending()) {
                return true;
            }
            store(result.error());
            return false;
        }

        void await_resume() const {
            raise("async_connect");
        }

        bool perform() noexcept override {
            assert(!canceled);
            store(os::make_system_error(impl->get_connect_result()));
            return true;
        }

        void invoke_operation() override {
            if (canceled) {
                store(std::make_error_code(std::errc::operation_canceled));
            }
            waiter.resume();
        }

        any_executor& associated_executor() const noexcept override {
            return impl->any_ex();
        }

#ifdef RAD_HAS_IO_URING
        void submit(io::detail::descriptor_data_inner_t& inner,
                    std::error_code& ec) noexcept override {
            descriptor->uring_backend->submit_connect(
                *descriptor, inner, impl->native_fd(), endpoint.address(),
                endpoint.size(), ec);
        }

        bool complete(const std::error_code& ec, int result) noexcept override {
            store(ec);
            std::ignore = result;
            return true;
        }
#endif // RAD_HAS_IO_URING
    };

    struct [[nodiscard]] RAD_EXPORT_VTABLE
        async_socket_impl::write_awaiter final
        : async_socket_impl::awaiter_common,
          async_socket_impl::write_op_base {
        io::iovec_buffers buffers;
#ifdef RAD_HAS_IO_URING
        msghdr msg{};
#endif // RAD_HAS_IO_URING

        template <class B>
        write_awaiter(async_socket_impl& impl, const B& buffs,
                      transfer_flags flags, std::error_code& ec) noexcept
            : awaiter_common(impl, ec), write_op_base(flags), buffers{buffs} {
        }

        RAD_EXPORT_DECL ~write_awaiter();

        RAD_EXPORT_DECL bool await_suspend(std::coroutine_handle<> coro);

        RAD_EXPORT_DECL std::size_t await_resume() const;

        RAD_EXPORT_DECL bool perform() noexcept override;

        RAD_EXPORT_DECL void invoke_operation() override;

        RAD_EXPORT_DECL any_executor&
        associated_executor() const noexcept override;

#ifdef RAD_HAS_IO_URING
        RAD_EXPORT_DECL void submit(io::detail::descriptor_data_inner_t& inner,
                                    std::error_code& ec) noexcept override;

        RAD_EXPORT_DECL bool complete(const std::error_code& ec,
                                      int result) noexcept override;
#endif // RAD_HAS_IO_URING
    };

    template <class Endpoint>
    struct [[nodiscard]] async_socket_impl::sendto_awaiter final
        : async_socket_impl::awaiter_common,
          async_socket_impl::write_op_base {
        io::iovec_buffers buffers;
#ifdef RAD_HAS_IO_URING
        msghdr msg{};
#endif // RAD_HAS_IO_URING
        Endpoint receiver;

        template <class B>
        sendto_awaiter(async_socket_impl& impl, const B& buffs,
                       transfer_flags flags, const Endpoint& receiver,
                       std::error_code& ec) noexcept
            : awaiter_common(impl, ec), write_op_base(flags), buffers{buffs},
              receiver{receiver} {
        }

        bool await_suspend(std::coroutine_handle<> coro) {
            waiter = coro;
            auto result =
                impl->async_sendto(buffers, receiver.address(), receiver.size(),
                                   flags, [this] { return this; });
            if (result.is_pending()) {
                return true;
            }
            transferred = result.transferred();
            store(result.error());
            return false;
        }

        std::size_t await_resume() const {
            raise("async_sendto");
            return has_error() ? 0 : transferred;
        }

        bool perform() noexcept override {
            // the lock is held
            assert(!canceled);
            auto result = impl->perform_sendto(
                false, buffers, flags, receiver.address(), receiver.size());
            transferred += result.transferred();
            if (result.is_pending()) {
                return false;
            }
            store(result.error());
            return true;
        }

        void invoke_operation() override {
            if (canceled) {
                store(std::make_error_code(std::errc::operation_canceled));
            }
            waiter.resume();
        }

        any_executor& associated_executor() const noexcept override {
            return impl->any_ex();
        }

#ifdef RAD_HAS_IO_URING
        void submit(io::detail::descriptor_data_inner_t& inner,
                    std::error_code& ec) noexcept override {
            std::size_t buffs_count = buffers.get_count();
            io::iovec_buff* buffs_ptr = buffers.get_buffers();
            if (buffs_count == 1) {
                descriptor->uring_backend->submit_sendto(
                    *descriptor, inner, impl->native_fd(), buffs_ptr->iov_base,
                    buffs_ptr->iov_len, receiver.address(), receiver.size(),
                    ec);
            }
            else {
                msg.msg_name = receiver.address();
                msg.msg_namelen = receiver.size();
                msg.msg_iov = reinterpret_cast<iovec*>(buffs_ptr);
                msg.msg_iovlen = buffs_count;
                descriptor->uring_backend->submit_sendmsg(
                    *descriptor, inner, impl->native_fd(), &msg, ec);
            }
        }

        bool complete(const std::error_code& ec, int result) noexcept override {
            buffers.advance(static_cast<std::size_t>(result));
            transferred += static_cast<std::size_t>(result);
            store(ec);
            if (ec || canceled || buffers.get_count() == 0) {
                return true;
            }
            return false;
        }
#endif // RAD_HAS_IO_URING
    };

    struct [[nodiscard]] RAD_EXPORT_VTABLE async_socket_impl::read_awaiter final
        : async_socket_impl::awaiter_common,
          async_socket_impl::read_op_base {
        io::iovec_buffers buffers;
#ifdef RAD_HAS_IO_URING
        msghdr msg{};
#endif // RAD_HAS_IO_URING

        template <class B>
        read_awaiter(async_socket_impl& impl, const B& buffs,
                     transfer_flags flags, bool not_zero,
                     std::error_code& ec) noexcept
            : awaiter_common(impl, ec), read_op_base(flags, not_zero),
              buffers{buffs} {
        }

        RAD_EXPORT_DECL ~read_awaiter();

        RAD_EXPORT_DECL bool await_suspend(std::coroutine_handle<> coro);

        RAD_EXPORT_DECL std::size_t await_resume() const;

        RAD_EXPORT_DECL bool perform() noexcept override;

        RAD_EXPORT_DECL void invoke_operation() override;

        RAD_EXPORT_DECL any_executor&
        associated_executor() const noexcept override;

#ifdef RAD_HAS_IO_URING
        RAD_EXPORT_DECL void submit(io::detail::descriptor_data_inner_t& inner,
                                    std::error_code& ec) noexcept override;

        RAD_EXPORT_DECL bool complete(const std::error_code& ec,
                                      int result) noexcept override;
#endif // RAD_HAS_IO_URING
    };

    template <class Endpoint>
    struct [[nodiscard]] async_socket_impl::recvfrom_awaiter final
        : async_socket_impl::awaiter_common,
          async_socket_impl::read_op_base {
        io::iovec_buffers buffers;
#ifdef RAD_HAS_IO_URING
        msghdr msg{};
#endif // RAD_HAS_IO_URING
        Endpoint& sender;

        template <class B>
        recvfrom_awaiter(async_socket_impl& impl, const B& buffs,
                         transfer_flags flags, Endpoint& sender,
                         std::error_code& ec) noexcept
            : awaiter_common(impl, ec), read_op_base(flags, false),
              buffers{buffs}, sender{sender} {
        }

        bool await_suspend(std::coroutine_handle<> coro) {
            waiter = coro;
            socklen_t size = Endpoint::max_size();
            auto result =
                impl->async_recvfrom(buffers, sender.address(), size, flags,
                                     not_zero, [this] { return this; });
            if (result.is_pending()) {
                return true;
            }
            transferred = result.transferred();
            if (!result.has_error()) {
                sender.resize(size);
            }
            store(result.error());
            return false;
        }

        std::size_t await_resume() const {
            raise("async_receive_from");
            return has_error() ? 0 : transferred;
        }

        bool perform() noexcept override {
            // the lock is held
            assert(!canceled);
            socklen_t size = Endpoint::max_size();
            auto result = impl->perform_recvfrom(
                buffers, flags, sender.address(), &size, not_zero);
            transferred += result.transferred();
            if (result.is_pending()) {
                return false;
            }
            if (!result.has_error()) {
                sender.resize(size);
            }
            store(result.error());
            return true;
        }

        void invoke_operation() override {
            if (canceled) {
                store(std::make_error_code(std::errc::operation_canceled));
            }
            waiter.resume();
        }

        any_executor& associated_executor() const noexcept override {
            return impl->any_ex();
        }

#ifdef RAD_HAS_IO_URING
        void submit(io::detail::descriptor_data_inner_t& inner,
                    std::error_code& ec) noexcept {
            std::size_t buffs_count = buffers.get_count();
            io::iovec_buff* buffs_ptr = buffers.get_buffers();
            msg.msg_name = sender.address();
            msg.msg_namelen = Endpoint::max_size();
            msg.msg_iov = reinterpret_cast<iovec*>(buffs_ptr);
            msg.msg_iovlen = buffs_count;
            descriptor->uring_backend->submit_recvmsg(
                *descriptor, inner, impl->native_fd(), &msg, ec);
        }

        bool complete(const std::error_code& ec, int result) noexcept override {
            if (not_zero && result == 0 && !ec) {
                store(io::detail::make_eof_error_code());
                return true;
            }
            buffers.advance(static_cast<std::size_t>(result));
            transferred += static_cast<std::size_t>(result);
            store(ec);
            bool read_all = flags & transfer_flags::wait_all;
            bool is_stream = not_zero;
            if (ec || canceled || !is_stream || buffers.get_count() == 0 ||
                (!read_all && transferred > 0)) {
                auto sender_size = msg.msg_namelen;
                if (sender_size > Endpoint::max_size()) {
                    sender_size = Endpoint::max_size();
                }
                sender.resize(sender_size);
                return true;
            }
            return false;
        }
#endif // RAD_HAS_IO_URING
    };

    template <class EndpointRange>
    struct [[nodiscard]] async_socket_impl::connect_range_awaiter final
        : async_socket_impl::awaiter_common,
          io::detail::io_op {
        using iterator_type =
            std::decay_t<decltype(std::begin(std::declval<EndpointRange>()))>;
        using endpoint_type =
            std::decay_t<decltype(*std::declval<iterator_type>())>;

        const EndpointRange& endpoints;
        iterator_type current_addr = std::begin(endpoints);

        connect_range_awaiter(async_socket_impl& impl,
                              const EndpointRange& endpoints,
                              std::error_code& ec) noexcept
            : awaiter_common(impl, ec), endpoints{endpoints} {
        }

        // check if there is no more endpoints
        bool finished() const noexcept {
            return current_addr == std::end(endpoints);
        }

        // returns true if there is a pending operation,
        // otherwise false
        bool start_connect_op(const std::error_code& init_ec) noexcept {
            auto result = async_result::failed(init_ec);
            auto alloc_fn = [this] { return this; };

            for (; current_addr != std::end(endpoints); ++current_addr) {
                result = impl->async_connect(current_addr->address(),
                                             current_addr->size(), alloc_fn);
                if (!result.has_error()) {
                    break;
                }
            }

            // store the error of last try which may be
            // init_error if no addresses were provided
            store(result.error());
            return result.is_pending();
        }

        bool await_suspend(std::coroutine_handle<> coro) {
            waiter = coro;
            // if there is no endpoints initially then fail
            // with destination_address_required
            return start_connect_op(
                std::make_error_code(std::errc::destination_address_required));
        }

        endpoint_type await_resume() const {
            raise("async_connect");
            return finished() ? endpoint_type{} : *current_addr;
        }

        bool perform() noexcept override {
            // the lock is held
            assert(!canceled);
            store(os::make_system_error(impl->get_connect_result()));
            if (!has_error()) {
                return true;
            }
            // start from the next address
            ++current_addr;
            if (!finished() && start_connect_op(error())) {
                return false;
            }

            return true;
        }

        void invoke_operation() override {
            if (canceled) {
                store(std::make_error_code(std::errc::operation_canceled));
            }
            waiter.resume();
        }

        any_executor& associated_executor() const noexcept override {
            return impl->any_ex();
        }

#ifdef RAD_HAS_IO_URING
        RAD_EXPORT_DECL void submit(io::detail::descriptor_data_inner_t& inner,
                                    std::error_code& ec) noexcept override {
            assert(!finished());
            descriptor->uring_backend->submit_connect(
                *descriptor, inner, impl->native_fd(), current_addr->address(),
                current_addr->size(), ec);
        }

        bool complete(const std::error_code& ec, int result) noexcept override {
            store(ec);
            if (!ec || canceled) {
                return true;
            }
            ++current_addr;
            if (finished()) {
                return true;
            }
            return false;
        }
#endif // RAD_HAS_IO_URING
    };

    // handlers operations

    template <class Endpoint, class Handler, class Alloc>
    struct async_socket_impl::connect_op final : io::detail::io_op,
                                                 allocator_storage<Alloc> {
        using alloc_base = allocator_storage<Alloc>;

        ref<async_socket_impl> impl;
        Handler handler;
        Endpoint endpoint;
        std::error_code ec;

        template <class H>
        connect_op(async_socket_impl& impl, const Endpoint& endpoint,
                   H&& handler, const Alloc& alloc) noexcept
            : alloc_base(alloc), impl{impl}, endpoint{endpoint},
              handler{std::forward<H>(handler)} {
        }

        bool perform() noexcept override {
            assert(!canceled);
            ec = os::make_system_error(impl->get_connect_result());
            return true;
        }

        void invoke_operation() override {
            if (canceled) {
                ec = std::make_error_code(std::errc::operation_canceled);
            }
            // copy ec to stack
            details::invoke_handler(this, std::error_code{ec});
        }

        any_executor& associated_executor() const noexcept override {
            return impl->any_ex();
        }

#ifdef RAD_HAS_IO_URING
        void submit(io::detail::descriptor_data_inner_t& inner,
                    std::error_code& ec) noexcept override {
            descriptor->uring_backend->submit_connect(
                *descriptor, inner, impl->native_fd(), endpoint.address(),
                endpoint.size(), ec);
        }

        bool complete(const std::error_code& ec, int result) noexcept override {
            this->ec = ec;
            std::ignore = result;
            return true;
        }
#endif // RAD_HAS_IO_URING
    };

    template <class Handler, class Alloc>
    struct async_socket_impl::write_op final : async_socket_impl::write_op_base,
                                               allocator_storage<Alloc> {
        using alloc_base = allocator_storage<Alloc>;

        ref<async_socket_impl> impl;
        io::iovec_buffers buffers;
        Handler handler;
        std::error_code ec;
#ifdef RAD_HAS_IO_URING
        msghdr msg{};
#endif // RAD_HAS_IO_URING

        template <class H>
        write_op(async_socket_impl& impl, io::iovec_buffers&& buffs,
                 H&& handler, const Alloc& alloc) noexcept
            : alloc_base(alloc), impl{impl}, buffers{std::move(buffs)},
              handler{std::forward<H>(handler)} {
        }

        bool perform() noexcept override {
            assert(!canceled);
            auto result = impl->perform_send(buffers, flags);
            transferred += result.transferred();
            if (result.is_pending()) {
                return false;
            }
            ec = result.error();
            return true;
        }

        void invoke_operation() override {
            if (canceled) {
                ec = std::make_error_code(std::errc::operation_canceled);
            }
            // copy ec and transferred to stack
            details::invoke_handler(this, std::error_code{ec},
                                    std::size_t{transferred});
        }

        any_executor& associated_executor() const noexcept override {
            return impl->any_ex();
        }

#ifdef RAD_HAS_IO_URING
        void submit(io::detail::descriptor_data_inner_t& inner,
                    std::error_code& ec) noexcept override {
            std::size_t buffs_count = buffers.get_count();
            io::iovec_buff* buffs_ptr = buffers.get_buffers();
            if (buffs_count == 1) {
                descriptor->uring_backend->submit_send(
                    *descriptor, inner, impl->native_fd(), buffs_ptr->iov_base,
                    buffs_ptr->iov_len, ec);
            }
            else {
                msg.msg_name = nullptr;
                msg.msg_namelen = 0;
                msg.msg_iov = reinterpret_cast<iovec*>(buffs_ptr);
                msg.msg_iovlen = buffs_count;
                descriptor->uring_backend->submit_sendmsg(
                    *descriptor, inner, impl->native_fd(), &msg, ec);
            }
        }

        bool complete(const std::error_code& ec, int result) noexcept override {
            buffers.advance(static_cast<std::size_t>(result));
            transferred += static_cast<std::size_t>(result);
            this->ec = ec;
            if (ec || canceled || buffers.get_count() == 0) {
                return true;
            }
            return false;
        }
#endif // RAD_HAS_IO_URING
    };

    template <class Endpoint, class Handler, class Alloc>
    struct async_socket_impl::sendto_op final
        : async_socket_impl::write_op_base,
          allocator_storage<Alloc> {
        using alloc_base = allocator_storage<Alloc>;

        ref<async_socket_impl> impl;
        io::iovec_buffers buffers;
        Handler handler;
        Endpoint receiver;
        std::error_code ec;
#ifdef RAD_HAS_IO_URING
        msghdr msg{};
#endif // RAD_HAS_IO_URING

        template <class H>
        sendto_op(async_socket_impl& impl, io::iovec_buffers&& buffs,
                  H&& handler, const Endpoint& receiver,
                  const Alloc& alloc) noexcept
            : alloc_base(alloc), impl{impl}, buffers{std::move(buffs)},
              handler{std::forward<H>(handler)}, receiver{receiver} {
        }

        bool perform() noexcept override {
            assert(!canceled);
            auto result = impl->perform_sendto(
                false, buffers, flags, receiver.address(), receiver.size());
            transferred += result.transferred();
            if (result.is_pending()) {
                return false;
            }
            ec = result.error();
            return true;
        }

        void invoke_operation() override {
            if (canceled) {
                ec = std::make_error_code(std::errc::operation_canceled);
            }
            // copy ec and transferred to stack
            details::invoke_handler(this, std::error_code{ec},
                                    std::size_t{transferred});
        }

        any_executor& associated_executor() const noexcept override {
            return impl->any_ex();
        }

#ifdef RAD_HAS_IO_URING
        void submit(io::detail::descriptor_data_inner_t& inner,
                    std::error_code& ec) noexcept override {
            std::size_t buffs_count = buffers.get_count();
            io::iovec_buff* buffs_ptr = buffers.get_buffers();
            if (buffs_count == 1) {
                descriptor->uring_backend->submit_sendto(
                    *descriptor, inner, impl->native_fd(), buffs_ptr->iov_base,
                    buffs_ptr->iov_len, receiver.address(), receiver.size(),
                    ec);
            }
            else {
                msg.msg_name = receiver.address();
                msg.msg_namelen = receiver.size();
                msg.msg_iov = reinterpret_cast<iovec*>(buffs_ptr);
                msg.msg_iovlen = buffs_count;
                descriptor->uring_backend->submit_sendmsg(
                    *descriptor, inner, impl->native_fd(), &msg, ec);
            }
        }

        bool complete(const std::error_code& ec, int result) noexcept override {
            buffers.advance(static_cast<std::size_t>(result));
            transferred += static_cast<std::size_t>(result);
            this->ec = ec;
            if (ec || canceled || buffers.get_count() == 0) {
                return true;
            }
            return false;
        }
#endif // RAD_HAS_IO_URING
    };

    template <class Handler, class Alloc>
    struct async_socket_impl::read_op final : async_socket_impl::read_op_base,
                                              allocator_storage<Alloc> {
        using alloc_base = allocator_storage<Alloc>;

        ref<async_socket_impl> impl;
        io::iovec_buffers buffers;
        Handler handler;
        std::error_code ec;
#ifdef RAD_HAS_IO_URING
        msghdr msg{};
#endif // RAD_HAS_IO_URING

        template <class H>
        read_op(async_socket_impl& impl, io::iovec_buffers&& buffs, H&& handler,
                bool not_zero, const Alloc& alloc) noexcept
            : alloc_base(alloc), impl{impl}, buffers{std::move(buffs)},
              handler{std::forward<H>(handler)} {
            this->not_zero = not_zero;
        }

        bool perform() noexcept override {
            assert(!canceled);
            auto result = impl->perform_recv(buffers, flags, not_zero);
            transferred += result.transferred();
            if (result.is_pending()) {
                return false;
            }
            ec = result.error();
            return true;
        }

        void invoke_operation() override {
            if (canceled) {
                ec = std::make_error_code(std::errc::operation_canceled);
            }
            // copy ec and transferred to stack
            details::invoke_handler(this, std::error_code{ec},
                                    std::size_t{transferred});
        }

        any_executor& associated_executor() const noexcept override {
            return impl->any_ex();
        }

#ifdef RAD_HAS_IO_URING
        void submit(io::detail::descriptor_data_inner_t& inner,
                    std::error_code& ec) noexcept override {
            std::size_t buffs_count = buffers.get_count();
            io::iovec_buff* buffs_ptr = buffers.get_buffers();
            if (buffs_count == 0) {
                descriptor->uring_backend->submit_recv(
                    *descriptor, inner, impl->native_fd(), nullptr, 0, ec);
            }
            if (buffs_count == 1) {
                descriptor->uring_backend->submit_recv(
                    *descriptor, inner, impl->native_fd(), buffs_ptr->iov_base,
                    buffs_ptr->iov_len, ec);
            }
            else {
                msg.msg_name = nullptr;
                msg.msg_namelen = 0;
                msg.msg_iov = reinterpret_cast<iovec*>(buffs_ptr);
                msg.msg_iovlen = buffs_count;
                descriptor->uring_backend->submit_recvmsg(
                    *descriptor, inner, impl->native_fd(), &msg, ec);
            }
        }

        bool complete(const std::error_code& ec, int result) noexcept override {
            if (not_zero && result == 0 && !ec) {
                this->ec = io::detail::make_eof_error_code();
                return true;
            }
            buffers.advance(static_cast<std::size_t>(result));
            transferred += static_cast<std::size_t>(result);
            this->ec = ec;
            bool read_all = flags & transfer_flags::wait_all;
            bool is_stream = not_zero;
            if (ec || canceled || !is_stream || buffers.get_count() == 0 ||
                (!read_all && transferred > 0)) {
                return true;
            }
            return false;
        }
#endif // RAD_HAS_IO_URING
    };

    template <class Endpoint, class Handler, class Alloc>
    struct async_socket_impl::recvfrom_op final
        : async_socket_impl::read_op_base,
          allocator_storage<Alloc> {
        using alloc_base = allocator_storage<Alloc>;

        ref<async_socket_impl> impl;
        io::iovec_buffers buffers;
        Handler handler;
        Endpoint& sender;
        std::error_code ec;
#ifdef RAD_HAS_IO_URING
        msghdr msg{};
#endif // RAD_HAS_IO_URING

        template <class H>
        recvfrom_op(async_socket_impl& impl, io::iovec_buffers&& buffs,
                    H&& handler, Endpoint& sender, bool not_zero,
                    Alloc& alloc) noexcept
            : alloc_base(alloc), impl{impl}, buffers{std::move(buffs)},
              handler{std::forward<H>(handler)}, sender{sender} {
            this->not_zero = not_zero;
        }

        bool perform() noexcept override {
            assert(!canceled);
            socklen_t size = Endpoint::max_size();
            auto result = impl->perform_recvfrom(
                buffers, flags, sender.address(), &size, not_zero);
            transferred += result.transferred();
            if (result.is_pending()) {
                return false;
            }
            if (!result.has_error()) {
                sender.resize(size);
            }
            ec = result.error();
            return true;
        }

        void invoke_operation() override {
            if (canceled) {
                ec = std::make_error_code(std::errc::operation_canceled);
            }
            // copy ec and transferred to stack
            details::invoke_handler(this, std::error_code{ec},
                                    std::size_t{transferred});
        }

        any_executor& associated_executor() const noexcept override {
            return impl->any_ex();
        }

#ifdef RAD_HAS_IO_URING
        void submit(io::detail::descriptor_data_inner_t& inner,
                    std::error_code& ec) noexcept {
            std::size_t buffs_count = buffers.get_count();
            io::iovec_buff* buffs_ptr = buffers.get_buffers();
            msg.msg_name = sender.address();
            msg.msg_namelen = Endpoint::max_size();
            msg.msg_iov = reinterpret_cast<iovec*>(buffs_ptr);
            msg.msg_iovlen = buffs_count;
            descriptor->uring_backend->submit_recvmsg(
                *descriptor, inner, impl->native_fd(), &msg, ec);
        }

        bool complete(const std::error_code& ec, int result) noexcept override {
            if (not_zero && result == 0 && !ec) {
                this->ec = io::detail::make_eof_error_code();
                return true;
            }
            buffers.advance(static_cast<std::size_t>(result));
            transferred += static_cast<std::size_t>(result);
            this->ec = ec;
            bool read_all = flags & transfer_flags::wait_all;
            bool is_stream = not_zero;
            if (ec || canceled || !is_stream || buffers.get_count() == 0 ||
                (!read_all && transferred > 0)) {
                auto sender_size = msg.msg_namelen;
                if (sender_size > Endpoint::max_size()) {
                    sender_size = Endpoint::max_size();
                }
                sender.resize(sender_size);
                return true;
            }
            return false;
        }
#endif // RAD_HAS_IO_URING
    };

    template <class EndpointRange, class Handler, class Alloc>
    struct async_socket_impl::connect_range_op final
        : io::detail::io_op,
          allocator_storage<Alloc> {
        using alloc_base = allocator_storage<Alloc>;
        using iterator_type =
            std::decay_t<decltype(std::begin(std::declval<EndpointRange>()))>;
        using endpoint_type =
            std::decay_t<decltype(*std::declval<iterator_type>())>;

        ref<async_socket_impl> impl;
        Handler handler;
        EndpointRange endpoints;
        iterator_type current_addr = std::begin(endpoints);
        std::error_code ec;

        template <class H, class ERange>
        connect_range_op(async_socket_impl& impl, H&& handler,
                         ERange&& endpoints, const Alloc& alloc)
            : alloc_base(alloc), impl{impl}, handler{std::forward<H>(handler)},
              endpoints{std::forward<ERange>(endpoints)} {
        }

        bool finished() const noexcept {
            return current_addr == std::end(endpoints);
        }

        async_result start_connect_op(const std::error_code& init_ec) {
            async_result result = async_result::failed(init_ec);
            auto alloc_fn = [this] { return this; };

            for (; current_addr != std::end(endpoints); ++current_addr) {
                result = impl->async_connect(current_addr->address(),
                                             current_addr->size(), alloc_fn);
                if (!result.has_error()) {
                    break;
                }
            }

            ec = result.error();
            return result;
        }

        bool perform() noexcept override {
            assert(!canceled);
            ec = os::make_system_error(impl->get_connect_result());
            if (!ec) {
                return true;
            }
            // consume the last tried address
            ++current_addr;
            auto result = start_connect_op(ec);
            return !result.is_pending();
        }

        void invoke_operation() override {
            if (canceled) {
                ec = std::make_error_code(std::errc::operation_canceled);
            }
            // copy ec and endpoint to stack
            endpoint_type epoint;
            if (!ec) {
                assert(!finished());
                epoint = *current_addr;
            }
            details::invoke_handler(this, std::error_code{ec}, epoint);
        }

        any_executor& associated_executor() const noexcept override {
            return impl->any_ex();
        }

#ifdef RAD_HAS_IO_URING
        void submit(io::detail::descriptor_data_inner_t& inner,
                    std::error_code& ec) noexcept override {
            assert(!finished());
            descriptor->uring_backend->submit_connect(
                *descriptor, inner, impl->native_fd(), current_addr->address(),
                current_addr->size(), ec);
        }

        bool complete(const std::error_code& ec, int result) noexcept override {
            this->ec = ec;
            if (!ec || canceled) {
                return true;
            }
            ++current_addr;
            if (finished()) {
                return true;
            }
            return false;
        }
#endif // RAD_HAS_IO_URING
    };

    // methods implementaions

    template <class Buffers>
    auto async_socket_impl::async_write(const Buffers& buffers,
                                        transfer_flags flags,
                                        std::error_code& ec) -> write_awaiter {
        make_error_if_closed(ec, "async_write");
        return {*this, buffers, flags, ec};
    }

    template <class Buffers>
    auto async_socket_impl::async_read(const Buffers& buffers, bool not_zero,
                                       transfer_flags flags,
                                       std::error_code& ec) -> read_awaiter {
        make_error_if_closed(ec, "async_read");
        return {*this, buffers, flags, not_zero, ec};
    }

} // namespace RAD_LIB_NAMESPACE::net::detail