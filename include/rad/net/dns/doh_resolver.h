#pragma once
#include <rad/async/timer.h>
#include <rad/net/dns/dns_parser.h>
#include <rad/net/http/http_parser.h>
#include <rad/net/ssl/stream.h>
#include <rad/net/tcp.h>
#include <rad/net/udp.h>
#include <rad/threading/synchronized_value.h>

#include <atomic>
#include <memory>

namespace RAD_LIB_NAMESPACE::net::doh {
    namespace details = RAD_LIB_NAMESPACE::detail;

    /*!
     * @brief The DOH resolver config.
     */
    struct dns_config {
        /*!
         * @brief The DOH server.
         */
        struct server_state {
            /// The IP address of the server.
            endpoint address;
            /// The host of the server to use as a value for HTTP Host header.
            std::string host;
            /// The target of the HTTP request line.
            std::string path;
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
         * cloudflare-dns.com (1.1.1.1) at port 443 and path '/dns-query'.
         *
         * dns.google (8.8.8.8) at port 443 and path '/dns-query'.
         *
         * dns.google (8.8.4.4) at port 443 and path '/dns-query'.
         */
        const std::array<server_state, 3> servers = {
            server_state{ipv4_endpoint{{1, 1, 1, 1}, 443}, "cloudflare-dns.com",
                         "/dns-query", false},
            server_state{ipv4_endpoint{{8, 8, 8, 8}, 443}, "dns.google",
                         "/dns-query", false},
            server_state{ipv4_endpoint{{8, 8, 4, 4}, 443}, "dns.google",
                         "/dns-query", false},
        };

        /*!
         * @brief The DOH cache which stores previous results.
         * Cache will not be used if enable_cache is false.
         */
        dns::cache_storage caches;

        /*!
         * @brief The max acceptable http response size.
         * Response messages exceeding this size will be treated as error.
         */
        std::atomic<uint16_t> max_reponse_size = 4 * 1024;

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
     * @brief The global async DOH resolver config.
     * Adjust the config settings before starting a resolve operation.
     */
    RAD_EXPORT_DECL extern dns_config config;

    /*!
     * @brief A DOH async resolver that uses http over ssl to send
     * and receive messages encoded in dns wire format to get the ip
     * addresses associated with a domain name. The messages sent and
     * received by this resolver are encrypted. The resolver derives its
     * settings from the global resolvers config when starting a resolve
     * operation.
     */
    class resolver {
        template <typename, bool>
        friend struct dns::detail::resolver_awaiter;

    public:
        using executor_type = ssl::stream<tcp::socket>::executor_type;

        /*!
         * @brief construct a DOH resolver and attach it to an
         * executor that implements both io_executor and
         * timer_executor. Note this executor must execute the
         * handlers serially, so it must be an strand or an
         * executor running in only one thread
         * @param ex the executor to attach the resolver to
         * @param ctx the ssl context used to create ssl streams
         * to encrypt the DNS messages
         */
        resolver(IoTimerExecutor auto& ex, ssl::context_base& ctx)
            : transport_(ctx, ex), timeout_timer_(ex) {
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
        void do_configure(std::string_view host, Protocol protocol,
                          Service&& service, std::error_code& ec) noexcept {
            if (results_handler_ != nullptr) {
                ec = std::make_error_code(std::errc::operation_in_progress);
                return;
            }

            reset();

            get_family(protocol);
            get_port(std::forward<Service>(service), ec);

            if (!ec) {
                current_server_ = config.select_a_server();
                make_query(host, ec);
            }
        }

        template <class Protocol, class Service>
        void configure(std::string_view host, Protocol protocol,
                       Service&& service, std::error_code& ec) {
            std::error_code cfg_ec;
            do_configure(host, protocol, std::forward<Service>(service),
                         cfg_ec);

            if (cfg_ec) {
                if (use_exceptions(ec)) {
                    throw std::system_error(ec, "async_resolve");
                }
                else {
                    ec = cfg_ec;
                }
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
         * get the port number. This port number is used to
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

            using ctx_t =
                resolver_handler<Handler, Alloc, resolver_handler2<Handler>>;
            results_handler_ = details::allocate_op<ctx_t>(
                alloc, std::forward<Handler>(handler));
            start_connection();
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
            transport_.next_layer().cancel();
        }

        /*!
         * @brief Get a reference to the executor used by the
         * resolver casted to io executor
         * @return a reference to the executor used by the
         * resolver casted to io executor
         */
        executor_type& executor() noexcept {
            return transport_.executor();
        }

        /*!
         * @brief Get a reference to the executor used by the
         * resolver casted to io executor
         * @return a reference to the executor used by the
         * resolver casted to io executor
         */
        const executor_type& executor() const noexcept {
            return transport_.executor();
        }

        /*!
         * @brief Get a reference to the executor used by the
         * resolver casted to io executor
         * @return a reference to the executor used by the
         * resolver casted to io executor
         */
        executor_type& io_executor() noexcept {
            return transport_.executor();
        }

        /*!
         * @brief Get a reference to the executor used by the
         * resolver casted to io executor
         * @return a reference to the executor used by the
         * resolver casted to io executor
         */
        const executor_type& io_executor() const noexcept {
            return transport_.executor();
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
        RAD_EXPORT_DECL void reset();

        template <class T>
        void get_port(T&& p, std::error_code& ec) noexcept {
            if constexpr (std::is_integral_v<std::decay_t<T>>) {
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

        void get_family(tcp p) {
            requested_family_ = p.family();
        }

        void get_family(udp p) {
            requested_family_ = p.family();
        }

        RAD_EXPORT_DECL bool start_coro(dns::detail::handler_base& handler,
                                        std::vector<endpoint>& cache_results);

        RAD_EXPORT_DECL void make_query(std::string_view name,
                                        std::error_code& ec);

        RAD_EXPORT_DECL void start_connection();

        void send_http_request();

        void read_http_response();

        void continue_read_response(std::error_code ec,
                                    std::size_t transferred);

        void validate_http_response();

        void read_with_content_len(std::size_t length);

        void after_read_with_content_len(std::error_code ec,
                                         std::size_t transferred);

        void read_chunked();

        void continue_read_chunked(std::error_code ec, std::size_t transferred);

        void read_until_eof();

        void continue_read_until_eof(const std::error_code& ec,
                                     std::size_t transferred);

        void fire_timeout_timer();

        RAD_EXPORT_DECL void cancel_timeout_timer() noexcept;

        bool want_ipv4() const noexcept {
            return requested_family_ == address_family::ipv4 ||
                   requested_family_ == address_family::unspecified;
        }

        bool still_ipv6() const noexcept {
            return requested_family_ == address_family::unspecified;
        }

        void try_fallback_server();

        void notify_success();

        void notify_failure(std::error_code ec);

        void process_answer(const_buffer buffer);

        any_executor& any_ex() noexcept {
            return io_executor().as_any_executor();
        }

        http::response_incremental_parser& get_response_parser() {
            return std::get<http::response_incremental_parser>(parser_);
        }

        http::chunks_incremental_parser& get_chunks_parser() {
            return std::get<http::chunks_incremental_parser>(parser_);
        }

        dns::detail::handler_base* results_handler_ = nullptr;
        const dns_config::server_state* current_server_ = nullptr;
        const dns_config::server_state* last_server_ = nullptr;
        uint8_t retries_ = 0;
        bool is_connected_ = false;
        bool using_existing_connection_ = false;

        std::shared_ptr<sync_value<bool>> timer_state_ =
            std::make_shared<sync_value<bool>>(false);
        ssl::stream<tcp::socket> transport_;
        timer timeout_timer_;
        mutex timer_lock_;

        std::string requested_name_;
        std::vector<endpoint> results_;
        std::string cname_;
        std::error_code results_ec_;
        std::vector<uint8_t> query_message_;

        std::vector<uint8_t> response_buff_;
        std::vector<uint8_t> chunked_body_;
        ring_consumer_producer response_rbuf_;
        std::size_t consumed_response_size_ = 0;

        http::response response_;
        std::variant<std::monostate, http::response_incremental_parser,
                     http::chunks_incremental_parser>
            parser_;

        address_family requested_family_;
        uint16_t question_offset_ = 0;
        uint16_t requested_port_ = 0;
    };
} // namespace RAD_LIB_NAMESPACE::net::doh
