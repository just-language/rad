#pragma once
#include <rad/async/timer.h>
#include <rad/net/dns/dns_parser.h>
#include <rad/net/socket_options.h>
#include <rad/net/tcp.h>
#include <rad/net/udp.h>
#include <rad/threading/synchronized_value.h>

#include <atomic>
#include <memory>

namespace RAD_LIB_NAMESPACE::net::ares {
    namespace details = RAD_LIB_NAMESPACE::detail;

    /*!
     * @brief The async resolver config.
     */
    struct resolver_config {
        /*!
         * @brief The DNS server.
         */
        struct server_state {
            /// The IP address of the server.
            endpoint address;
            /// Whether this server supports EDNS or not.
            mutable std::atomic<bool> supports_edns = true;
            /// Whether this server is broken or not.
            mutable std::atomic<bool> broken = false;
        };

        /*!
         * @brief The default request timeout.
         * Initial value is 5 seconds.
         */
        std::atomic<std::chrono::milliseconds> timeout =
            std::chrono::milliseconds(5000);

        /*!
         * @brief The supported DOH servers.
         * Supported servers are (in order):
         *
         * cloudflare-dns.com (1.1.1.1) at port 53.
         *
         * dns.google (8.8.8.8) at port 53'.
         *
         * dns.google (8.8.4.4) at port 53'.
         */
        const std::array<server_state, 3> servers = {
            server_state{ipv4_endpoint{{1, 1, 1, 1}, 53}, true, false},
            server_state{ipv4_endpoint{{8, 8, 8, 8}, 53}, true, false},
            server_state{ipv4_endpoint{{8, 8, 4, 4}, 53}, true, false},
        };

        /*!
         * @brief The DNS cache which stores previous results.
         * Cache will not be used if enable_cache is false.
         */
        dns::cache_storage caches;

        /*!
         * @brief Always use TCP and never use UDP.
         * Initial value is false.
         */
        std::atomic<bool> always_tcp = false;

        /*!
         * @brief Don't switch to TCP if UDP fails.
         * Initial value is false.
         */
        std::atomic<bool> no_tcp = false;

        /*!
         * @brief Use EDNS extension to increase UDP DNS message size.
         * Initial value is false.
         */
        std::atomic<bool> use_edns = false;

        /*!
         * @brief The max EDNS UDP DNS message size if ENDS is used.
         * Initial value is 4096.
         */
        std::atomic<uint16_t> max_udp_size = 4096;

        /*!
         * @brief The max TCP DNS message size.
         * Initial value is 4096.
         */
        std::atomic<uint32_t> max_tcp_size = 4096;

        /*!
         * @brief Accept truncated responses when using UDP.
         * Initial value is false.
         */
        std::atomic<bool> ignore_truncation = false;

        /*!
         * @brief Enable caching.
         * Initial value is true.
         */
        std::atomic<bool> enable_cache = true;

        /*!
         * @brief Try to select a server that is not broken.
         * If all servers are broken, the first server is returned.
         * @return A const pointer to one of the config servers.
         */
        const server_state* select_a_server() const noexcept {
            for (const auto& srv : servers) {
                if (!srv.broken) {
                    return &srv;
                }
            }
            return &servers[0];
        }

        /*!
         * @brief Try to select a server that is not broken and is not
         * @p failed_srv.
         * @param failed_srv The last failed server.
         * @return A const pointer to one of the config servers.
         */
        const server_state*
        try_another_server(const server_state* failed_srv) const noexcept {
            for (const auto& srv : servers) {
                if (failed_srv == std::addressof(srv)) {
                    continue;
                }
                if (!srv.broken) {
                    return &srv;
                }
            }
            // all servers are broken!
            for (const auto& srv : servers) {
                if (failed_srv == std::addressof(srv)) {
                    continue;
                }
                return &srv;
            }
            return nullptr;
        }
    };

    /*!
     * @brief The global async resolver config.
     * Adjust the config settings before starting a resolve operation.
     */
    RAD_EXPORT_DECL extern resolver_config config;

    /*!
     * @brief An async resolver that uses udp and tcp protocols to send
     * and receive messages encoded in dns wire format to get the ip
     * addresses associated with a domain name. The messages sent and
     * received by this resolver are in cleartext and not encrypted, to have
     * encryption use the DOH resolver instead. The resolver derives its
     * settings from the global resolvers config when starting a resolve
     * operation.
     */
    class resolver : public trackable {
        template <typename, bool>
        friend struct dns::detail::resolver_awaiter;

    public:
        using executor_type = udp::socket::executor_type;

        /*!
         * @brief Construct a resolver and attach it to an
         * executor that implements both io_executor and
         * timer_executor. Note this executor must execute the
         * handlers serially, so it must be an strand or an
         * executor running in only one thread
         * @param ex the executor to attach the resolver to
         */
        resolver(IoTimerExecutor auto& ex)
            : udp_sock_(ex), tcp_sock_(ex), timeout_timer_(ex) {
        }

        /*!
         * @brief Destroy the resolver and cancel all pending
         * resolve operations. Note that is undefined behavior
         * to destroy the resolver while it has an outstanding
         * async operation.
         */
        ~resolver() {
            cancel();
        }

    private:
        template <class Protocol, class Service>
        void configure(std::string_view host, Protocol protocol,
                       Service&& service, std::error_code& ec) noexcept {
            if (handler_ != nullptr) {
                ec = std::make_error_code(std::errc::operation_in_progress);
                return;
            }

            reset();

            get_family(protocol);
            get_port(std::forward<Service>(service), ec);

            if (!ec) {
                current_server = config.select_a_server();
                make_dns_query(host, ec);
            }
        }

    public:
        /*!
         * @brief Start an async resolve operation and call the
         * handler when the operation is done. Only one resolve
         * operation may be active at a time. If caching is
         * enabled in the config the global cache is used
         * firstly before going to the network and if a result
         * was found in the cache the handler is posted without
         * doing network operations.
         * @tparam Service The type of service, either numerical
         * type or a type convertible to std::string_view
         * @tparam Protocol The type of protocol
         * @tparam Handler The type of the handler
         * @tparam Alloc The type of the allocator
         * @param host the domain name to resolve to ip
         * addresses
         * @param service this is either the numeric port number
         * in host byte order, or a uri scheme that is used to
         * get the port number. This port number is not used to
         * resolve the host ip addresses but it is appened to
         * the result endpoints after resolving has been done.
         * @param protocol this is used to get the family of the
         * resolved ip addresses. This family is not used to
         * resolve the host ip addresses but it is appened to
         * the result.
         * @param handler a handler to invoke when the operation
         * is done and will be passed an error_code that
         * determines whether the operation has succeeded or
         * failed and the resolved endpoints and optionally the
         * canonical name. The handler must be either copyable
         * or movable and the following expression must be
         * valid: handler(std::error_code{},
         * std::vector<endpoint>{}) or
         * handler(std::error_code{}, std::vector<endpoint>{},
         * std::string{})
         * @param alloc An allocator used to allocate the
         * operation. The size passed to its allocate method is
         * in bytes and the alignment of the allocator must be
         * suitable for std::max_align_t
         */
        template <class Service, class Protocol,
                  dns::detail::ResolverHandler Handler,
                  HandlerAllocator Alloc = default_io_allocator>
        void async_resolve(std::string_view host, Service&& service,
                           Protocol protocol, Handler&& handler,
                           const Alloc& alloc = Alloc()) {
            using namespace dns::detail;

            std::error_code ec;
            configure(host, protocol, std::forward<Service>(service), ec);

            if (ec) {
                return post(
                    any_ex(),
                    [handler = std::forward<Handler>(handler), ec]() mutable {
                        if constexpr (resolver_handler1<Handler>) {
                            handler(ec, std::vector<endpoint>{});
                        }
                        else {
                            handler(ec, std::vector<endpoint>{}, std::string{});
                        }
                    },
                    alloc);
            }

            std::vector<endpoint> cache_results;
            if (config.enable_cache &&
                config.caches.find(host, requested_family_, requested_port_,
                                   cache_results)) {
                return post(
                    any_ex(),
                    [handler = std::forward<Handler>(handler),
                     results = std::move(cache_results)]() mutable {
                        if constexpr (resolver_handler1<Handler>) {
                            handler(std::error_code{}, std::move(results));
                        }
                        else {
                            handler(std::error_code{}, std::move(results),
                                    std::string{});
                        }
                    },
                    alloc);
            }

            using ctx_t = resolver_handler<std::remove_cvref_t<Handler>, Alloc,
                                           resolver_handler2<Handler>>;
            handler_ = details::allocate_op<ctx_t>(
                alloc, std::forward<Handler>(handler));

            if (!config.always_tcp) {
                send_udp_packet();
            }
            else {
                start_tcp_connection();
            }
        }

        /*!
         * @brief Async resolve a host. Note that the resolve
         * operation will not start until the returned awaitable
         * is awaited. Only one resolve operation may be active
         * at a time. If caching is enabled in the config the
         * global cache is used firstly before going to the
         * network and if a result was found in the cache the
         * handler is posted without doing network operations.
         * @tparam Service The type of service, either numerical
         * type or a type convertible to std::string_view
         * @tparam Protocol The type of protocol
         * @param host the domain name to resolve to ip
         * addresses
         * @param service this is either the numeric port number
         * in host byte order, or a uri scheme that is used to
         * get the port number. This port number is not used to
         * resolve the host ip addresses but it is appened to
         * the result endpoints after resolving has been done.
         * @param protocol this is used to get the family of the
         * resolved ip addresses. This family is not used to
         * resolve the host ip addresses but it is appened to
         * the result.
         * @param ec if set to no_ec then errors are reported
         * via exceptions, otherwise errors are reported via @p
         * ec.
         * @return an awaitable that is when awaited will start
         * the resolve operation and result in a vector of
         * endpoints associated with host.
         */
        template <class Service, class Protocol>
        auto async_resolve(std::string_view host, Service&& service,
                           Protocol protocol, std::error_code& ec = no_ec) {
            using namespace dns::detail;
            configure(host, protocol, std::forward<Service>(service), ec);
            return resolver_awaiter{no_cname_t{}, *this, ec};
        }

        /*!
         * @brief Async resolve a host and get its canonical
         * name. Note that the resolve operation will not start
         * until the returned awaitable is awaited. Only one
         * resolve operation may be active at a time. If caching
         * is enabled in the config the global cache is used
         * firstly before going to the network and if a result
         * was found in the cache the handler is posted without
         * doing network operations.
         * @tparam Service The type of service, either numerical
         * type or a type convertible to std::string_view
         * @tparam Protocol The type of protocol
         * @param host the domain name to resolve to ip
         * addresses
         * @param service this is either the numeric port number
         * in host byte order, or a uri scheme that is used to
         * get the port number. This port number is used to
         * resolve the host ip addresses but it is appened to
         * the result endpoints after resolving has been done.
         * @param protocol this is used to get the family of the
         * resolved ip addresses. This family is not used to
         * resolve the host ip addresses but it is appened to
         * the result.
         * @param ec if set to no_ec then errors are reported
         * via exceptions, otherwise errors are reported via @p
         * ec.
         * @return an awaitable that is when awaited will start
         * the resolve operation and result in a pair whost
         * first element is a vector of endpoints associated
         * with host, and the second is a string canonical name.
         */
        template <class Service, class Protocol>
        auto async_resolve_cname(std::string_view host, Service&& service,
                                 Protocol protocol,
                                 std::error_code& ec = no_ec) {
            using namespace dns::detail;
            configure(host, protocol, std::forward<Service>(service), ec);
            return resolver_awaiter{hold_cname_t{}, *this, ec};
        }

        /*!
         * @brief Cancel pending async resolve operations.
         * Canceled operations are passed an error_code that
         * indicates cancelation. Note that not all operations
         * can be canceled. Operations that have completed and
         * are scheduled for invocation can no longer be
         * canceled.
         */
        void cancel() noexcept {
            cancel_timeout_timer();
            udp_sock_.cancel();
            tcp_sock_.close();
        }

        /*!
         * @brief Get a reference to the executor used by the
         * resolver casted to io executor
         * @return a reference to the executor used by the
         * resolver casted to io executor
         */
        executor_type& io_executor() noexcept {
            return udp_sock_.executor();
        }

        /*!
         * @brief Get a reference to the executor used by the
         * resolver casted to io executor
         * @return a reference to the executor used by the
         * resolver casted to io executor
         */
        const executor_type& io_executor() const noexcept {
            return udp_sock_.executor();
        }

        /*!
         * @brief Get a reference to the executor used by the
         * resolver casted to timer executor
         * @return a reference to the executor used by the
         * resolver casted to timer executor
         */
        class timer_executor& timer_exutor() noexcept {
            return timeout_timer_.executor();
        }

        /*!
         * @brief Get a reference to the executor used by the
         * resolver casted to timer executor
         * @return a reference to the executor used by the
         * resolver casted to timer executor
         */
        const class timer_executor& timer_exutor() const noexcept {
            return timeout_timer_.executor();
        }

    private:
        RAD_EXPORT_DECL bool start_coro(dns::detail::handler_base& h,
                                        std::vector<endpoint>& cache_results);

        bool open_udp_socket();

        bool open_tcp_socket();

        void try_fallback_server();

        void switch_to_tcp();

        RAD_EXPORT_DECL void cancel_timeout_timer() noexcept;

        void fire_timeout_timer();

        void reset() noexcept {
            using_tcp_ = config.always_tcp;
            results_.clear();
            cname_.clear();
            results_ec_.clear();
            retries_ = 0;
        }

        void notify_failure(const std::error_code& ec);

        void notify_success();

        const_buffer udp_query_buffer() const noexcept;

        const_buffer tcp_query_buffer() const noexcept;

        bool want_ipv4() const noexcept {
            return requested_family_ == address_family::ipv4 ||
                   requested_family_ == address_family::unspecified;
        }

        bool still_ipv6() const noexcept {
            return requested_family_ == address_family::unspecified;
        }

        template <class T>
        void get_port(T&& p, std::error_code& ec) noexcept {
            ec.clear();
            if constexpr (std::is_integral_v<T>) {
                requested_port_ = static_cast<uint16_t>(p);
            }
            else if constexpr (std::is_convertible_v<T, std::string_view>) {
                requested_port_ =
                    dns::service_to_port(static_cast<std::string_view>(p), ec);
            }
            else {
                static_assert(always_false<T>, "port must be either "
                                               "number or string");
            }
        }

        void get_family(tcp p) noexcept {
            requested_family_ = p.family();
        }

        void get_family(udp p) noexcept {
            requested_family_ = p.family();
        }

        RAD_EXPORT_DECL void make_dns_query(std::string_view name,
                                            std::error_code& ec);

        RAD_EXPORT_DECL void send_udp_packet();

        void read_udp_response();

        RAD_EXPORT_DECL void start_tcp_connection();

        void send_tcp_packet();

        void read_tcp_length_word();

        void read_tcp_data();

        void process_answer();

        dns::dns_header_raw& request_header() const noexcept {
            return *(buffer(query_storage_) + sizeof(uint16_t))
                        .data_as<dns::dns_header_raw>();
        }

        dns::question_section_raw& request_question() {
            return *reinterpret_cast<dns::question_section_raw*>(
                &query_storage_[question_offset_]);
        }

        dns::resource_record_raw& request_edns() const noexcept {
            // question offset + question (4) + root domain
            // (1)
            uint16_t edns_offset =
                question_offset_ + sizeof(dns::question_section_raw) + 1;
            return *(buffer(query_storage_) + edns_offset)
                        .data_as<dns::resource_record_raw>();
        }

        bool edns_was_used() const noexcept {
            return request_header().additional_records_count() == 1;
        }

        any_executor& any_ex() noexcept {
            return udp_sock_.executor().as_any_executor();
        }

        struct allocator_types {
            using ops = op_alloc_type;

            using write_buffers_type = const_buffer;
            using read_buffers_type = mutable_buffer;
            using endpoint_type = endpoint;

            static constexpr ops op_type = ops::read | ops::write |
                                           ops::sendto | ops::recvfrom |
                                           ops::connect;
            static constexpr std::size_t max_handler_size = sizeof(void*) * 3;
        };

        static constexpr std::size_t alloc_size =
            max_of({tcp::socket::max_allocator_size<allocator_types>(),
                    udp::socket::max_allocator_size<allocator_types>()});

        // the final handler
        dns::detail::handler_base* handler_ = nullptr;

        // if tcp is being used
        bool using_tcp_ = config.always_tcp;
        // the last address family used to open the udp socket
        address_family last_sock_family = address_family::unspecified;
        // the udp socket used to send and receive dns messages
        // over udp
        udp::socket udp_sock_;
        // the tcp socket used to send and receive dns messages
        // over tcp if neccessary
        tcp::socket tcp_sock_;

        // the allocator and storage used to allocate handlers
        // for io operations
        alignas(16) std::array<std::uint8_t, alloc_size> io_alloc_buff_;

        // the port that is to be put into the endpoints results
        uint16_t requested_port_ = 0;
        // request ipv4 records then ipv6 when unspecified, ipv4
        // only if ipv4 and ipv6 only when ipv6
        address_family requested_family_ = {};

        // this flag must be valid as long as the timer wait
        // handler is pending even after cancelation. so it may
        // outlive the resolver
        std::shared_ptr<sync_value<bool>> timer_state_ =
            std::make_shared<sync_value<bool>>(false);
        // used to cancel the query if async_recvfrom or
        // async_read takes too long
        timer timeout_timer_;

        // points to the current config used server
        const resolver_config::server_state* current_server = nullptr;

        // the generated query id for the current outstanding
        // query
        uint16_t query_id_ = 0;

        // holds the endpoints results after finish
        std::vector<endpoint> results_;

        // holds the cname after finish if it was requested
        std::string cname_;

        // holds the error code after the operation has finished
        std::error_code results_ec_;

        /*
        a stack buffer space used to store queries. it consists
        of : tcp word length of the whole message except the
        edns header the header and question consisting of
        encoded host name and question section the edns header

        for udp transport the buffer will start from header and
        may include edns header if edns is used for tcp
        transport the buffer used will start from tcp word
        length to the end of question not including the edns
        header
        */
        std::vector<uint8_t> query_storage_;

        // store the tcp word length during the tcp message read
        beu16 tcp_length_;
        // the buffer used to read the dns messages
        std::vector<uint8_t> read_buffer_;
        // used with async_receive_from to hold the address of
        // the remote sender server
        endpoint from_addr_;
        // an offset to the question section to the start of
        // query_storage
        uint16_t question_offset_ = 0;
        // the requested host name to resolve
        std::string requested_name_;

        uint8_t retries_ = 0;
    };

} // namespace RAD_LIB_NAMESPACE::net::ares