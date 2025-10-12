#pragma once
#include <rad/async/async_event.h>
#include <rad/async/async_wait_queue.h>
#include <rad/async/timer.h>
#include <rad/coro/task.h>
#include <rad/net/http2/http2_parser.h>
#include <rad/net/ssl/stream.h>
#include <rad/net/tcp.h>
#include <rad/ring_buffer_consumer.h>
#include <rad/stack_list.h>

namespace RAD_LIB_NAMESPACE::net::http2 {
    /*!
     * @brief Async HTTP 2 client to perform HTTP 2 requests and retrieve
     * responses. Resolve operation if required are performed using system
     * resolver. If the use of system resolver is not desired, the user can
     * resolve the domain beforehand and use the result endpoints to connect to
     * the server.
     *
     * Multiple outstanding request may exist at a time, which are carried on
     * http 2 streams.
     *
     * All urls used in requests must have the same host the client is already
     * connected to, because the whole point of http 2 is to use a single
     * connection for multiple parallel requests.
     *
     * The client is not thread safe and may not be accessed by more than one
     * thread concurrently, so if the used io timer executor runs on multiple
     * threads it should be wrapped in a strand, and this strand executor should
     * be used instead.
     *
     * Connection is driven when a request is awaited, so the user must keep
     * sending requests to prevent the connection from becoming idle, and
     * to receive and act on http 2 incoming SETTINGS, PING and data as they
     * arrive.
     *
     * Instead, the user may await `drive` method to keep the connection driven
     * and send ACK for incoming SETTINGS and PING, and send PING frames to
     * detect if the connection is broken or not.
     *
     * Note that, even if `drive` is awaited, the server may decide to close
     * the connection any way if the client does not send a request in a
     * specified time window, or if it wants to close the connection for any
     * reason.
     *
     * The client tries to keep the connection to the server open as long as
     * possible to save the cost of TCP connection establishment, ssl handshake
     * and http2 handshake.
     *
     * Redirections are followed by default if the response contains a Location
     * header, and when many successive redirections are encountered an error is
     * raised.
     *
     * Redirections that lead to a host different from the already connected to,
     * will not be followed and the 3XX response will be returned back to the
     * user.
     *
     * It is possible to disable automatic redirections and set the maximum
     * allowed successive redirections using `follow_redirection` and
     * `max_redirections` methods.
     *
     * To prevent handshake from hanging if the peer is not responsive,
     * a handshake timeout is used to cancel the handshake if this timeout
     * passes before the handshake completes.
     *
     * To detect and close idle connections, a timeout timer is used with a
     * configurable idle timeout to close the connection if this timeout passes
     * without any data received from the peer.
     *
     * To configure the timeouts, use `timeouts`. To disable the timeouts use
     * `disable_timeouts` or call `timeouts` with very large values.
     *
     * When the connection is idle for half of the configured idle timeout
     * a PING frame is sent to the peer to detect if the connection is broken,
     * and if no PING ACK is received before the second half of idle timeout
     * passes the connection is considered broken.
     */
    class client {
        static constexpr std::int32_t default_client_window_size =
            max_flow_control_window_size / 2;
        static constexpr std::int32_t min_allowed_window_size = 16 * 1024;

        enum class stream_state : uint8_t {
            idle,
            open,
            half_closed_local,
            half_closed_remote,
            closed,
        };

        struct stream : public stack_double_list_node {
            client& cl;
            std::string path;
            const headers& hdrs;
            response& res;
            const_buffer body;
            dynamic_buffer res_buff;
            std::error_code rst_ec;
            std::uint32_t id = 0;
            std::int32_t window_size = 0;
            std::uint64_t received_data = 0;
            std::uint32_t received_continuations = 0;
            verb method = verb::invalid;
            stream_state state = stream_state::idle;
            bool wants_headers = true;
            bool received_final_res = false;
            bool received_trailers = false;

            stream(client& cl, verb method, const headers& hdrs,
                   const_buffer body, response& res, dynamic_buffer res_buff)
                : cl{cl}, hdrs{hdrs}, res{res}, body{body}, res_buff{res_buff},
                  method{method} {
            }

            bool is_idle() const noexcept {
                return state == stream_state::idle;
            }

            bool is_open() const noexcept {
                return state == stream_state::open;
            }

            bool is_half_closed_local() const noexcept {
                return state == stream_state::half_closed_local;
            }

            bool is_half_closed_remote() const noexcept {
                return state == stream_state::half_closed_remote;
            }

            bool is_closed() const noexcept {
                return state == stream_state::closed;
            }

            void send_H() noexcept {
                assert(state == stream_state::idle);
                state = stream_state::open;
            }

            void send_ES() noexcept {
                assert(state == stream_state::open ||
                       state == stream_state::half_closed_remote);
                state = state == stream_state::open
                            ? stream_state::half_closed_local
                            : stream_state::closed;
            }

            void recv_ES() noexcept {
                assert(state == stream_state::open ||
                       state == stream_state::half_closed_local);
                state = state == stream_state::open
                            ? stream_state::half_closed_remote
                            : stream_state::closed;
            }

            void send_R() noexcept {
                state = stream_state::closed;
            }

            void recv_R() noexcept {
                state = stream_state::closed;
            }

            void restart() noexcept {
                res.clear(false);
                rst_ec.clear();
                id = 0;
                state = stream_state::idle;
                wants_headers = true;
                received_final_res = false;
                received_trailers = false;
            }

            std::optional<std::uint64_t> get_content_length() const noexcept {
                auto content_length_value = res.get_content_length();
                if (!content_length_value.empty()) {
                    std::error_code ec;
                    std::uint64_t content_length =
                        to_uint64(content_length_value, 10, ec);
                    if (!ec) {
                        return content_length;
                    }
                }
                return std::nullopt;
            }
        };

        struct parser_frame_header_stage {};

        struct parser_frame_payload_stage {
            size_t total_size = 0;
            size_t min_read_size = 0;
            size_t consumed_size = 0;
            uint8_t padding = 0;
        };

    public:
        /*!
         * @brief Construct the http 2 client with an executor and
         * ssl context.
         * @param ex The executor used by the underlying socket,
         * ssl stream and timer. This executor must be both timer and
         * io executor. Since http 2 client is not thread safe,
         * the executor must be sequential like strand or
         * io_loop running on single thread.
         * @param ctx The ssl context used by the underlying ssl
         * stream.
         */
        client(IoTimerExecutor auto& ex, ssl::context_base& ctx)
            : transport_{ctx, ex}, idle_timer_{ex} {
        }

        /*!
         * @brief Construct the http 2 client with a timer executor and
         * ssl stream.
         *
         * After construction, the client must do http 2 handshake
         * as fast as possible using `handshake` mehtod before the server
         * handshake times out.
         * @param ex The executor used by the underlying timer.
         * This executor must be be the same executor used by the ssl
         * stream. Since http 2 client is not thread safe,
         * the executor must be sequential like strand or
         * io_loop running on single thread.
         * @param host The host of the peer endpoint.
         * It is needed to use it in the ':authority' (Host) header
         * when sending requests.
         * @param transport The ssl stream to use as a transport.
         * The stream must be open and have done the ssl handshake and
         * negotiated the 'h2' ALPN protocol identifier,
         * and didn't write or read any prior data.
         */
        RAD_EXPORT_DECL client(timer_executor& ex, std::string_view host,
                               ssl::stream<tcp::socket>&& transport);

        /*!
         * @brief Construct the http 2 client with a timer executor and
         * tcp socket to use HTTP 2 over non encrypted transport.
         *
         * After construction, the client must do http 2 handshake
         * as fast as possible using `handshake` mehtod before the server
         * handshake times out.
         * @param ex The executor used by the underlying timer.
         * This executor must be be the same executor used by the tcp
         * socket. Since http 2 client is not thread safe,
         * the executor must be sequential like strand or
         * io_loop running on single thread.
         * @param ctx The ssl context for the underlying ssl stream.
         * @param host The host of the peer endpoint.
         * It is needed to use it in the ':authority' (Host) header
         * when sending requests.
         * @param transport The tcp socket to use as a transport.
         * The socket must be connected to a peer that supports HTTP 2.
         */
        RAD_EXPORT_DECL client(timer_executor& ex, ssl::context_base& ctx,
                               std::string_view host, tcp::socket&& transport);

        /*!
         * @brief Get the current local endpoint SETTINGS.
         * @return The current local endpoint SETTINGS.
         */
        endpoint_config get_local_config() const noexcept {
            endpoint_config cfg;
            cfg.header_table_size =
                static_cast<uint32_t>(hdecoder_.table().max_size());
            cfg.initial_window_size = self_streams_initial_window_size_;
            cfg.max_frame_size = self_max_payload_size_;
            cfg.max_header_list_size = self_max_header_list_size_;
            cfg.enable_push = false;
            return cfg;
        }

        /*!
         * @brief Get the current remote endpoint SETTINGS.
         * @return The current remote endpoint SETTINGS.
         */
        endpoint_config get_remote_config() const noexcept {
            endpoint_config cfg;
            cfg.header_table_size =
                static_cast<uint32_t>(hencoder_.table().max_size());
            cfg.initial_window_size = peer_streams_initial_window_size_;
            cfg.max_frame_size = peer_max_payload_size_;
            cfg.max_concurrent_streams = peer_max_concurrent_streams_;
            cfg.max_header_list_size = peer_max_header_list_size_;
            cfg.enable_push = false;
            return cfg;
        }

        /*!
         * @brief Get a reference to the underlying ssl stream transport.
         * @return A reference to the underlying ssl stream transport.
         */
        ssl::stream<tcp::socket>& transport() noexcept {
            return transport_;
        }

        /*!
         * @brief Get a const reference to the underlying ssl stream transport.
         * @return A const reference to the underlying ssl stream transport.
         */
        const ssl::stream<tcp::socket>& transport() const noexcept {
            return transport_;
        }

        /*!
         * @brief Do the HTTP 2 handshake by sending the connection
         * preface and exchanging initial SETTINGS.
         *
         * The underlying transport must be open and connected before
         * doing the HTTP 2 handshake.
         *
         * This method is only used when the http 2 client is constructed
         * using an already open tcp socket or ssl stream.
         * @return An awaitable that is when awaited will start the handshake
         * process.
         */
        RAD_EXPORT_DECL task<> handshake();

        /*!
         * @brief Connect to a host given by a url that provides
         * the scheme to use, the host to resolve and connect to
         * and the port which if not explicitly given in the url,
         * will be 443 for 'https' scheme, and 80 for 'http' scheme'.
         *
         * After TCP connection, the ssl handshake is done if the scheme
         * is 'https', then the HTTP 2 handshake is done.
         * @param url The url specifying the scheme, host and port.
         *
         * Other parts of the url are ignored and not used.
         * The url must include the scheme which must be either 'http'
         * or 'https' and must include a host to resolve and connect to
         * and may include a port to use a port different from the default
         * scheme port.
         *
         * Example of a valid connect url: 'https://example.com'.
         * @return An awaitable that is when awaited will start the connection
         * process.
         */
        RAD_EXPORT_DECL task<> connect(std::string_view url);

        /*!
         * @brief Resolve a host and connect to the result ip address with the
         * given port.
         *
         * After TCP connection, the ssl handshake is done if the scheme
         * is @p is_ssl is true, then the HTTP 2 handshake is done.
         * @param url The url specifying the scheme, host and port.
         * @param host The host name to resolve and connect to the result ip.
         * This is a host name not a url (i.e. example.com).
         * @param port The port of the target host to connect to.
         * The port must be non zero.
         * @param is_ssl True if HTTP 2 is to be used over encrypted TLS,
         * and false to use HTTP 2 over TCP.
         * @return An awaitable that is when awaited will start the connection
         * process.
         */
        RAD_EXPORT_DECL task<> connect(std::string_view host, uint16_t port,
                                       bool is_ssl);

        /*!
         * @brief Connect to a specified IP endpoint and use the given host
         * for subsequent HTTP 2 requests.
         *
         * After TCP connection, the ssl handshake is done if the scheme
         * is @p is_ssl is true, then the HTTP 2 handshake is done.
         * @param host The host of the peer endpoint.
         * It is needed to use it in the ':authority' (Host) header
         * when sending requests.
         * @param epoint The IP address of the peer endpoint.
         * @param is_ssl True if HTTP 2 is to be used over encrypted TLS,
         * and false to use HTTP 2 over TCP.
         * @return An awaitable that is when awaited will start the connection
         * process.
         */
        RAD_EXPORT_DECL task<> connect(std::string_view host,
                                       const tcp::endpoint_type& epoint,
                                       bool is_ssl);

        /*!
         * @brief Connect to one of specified IP endpoints range and use the
         * given host for subsequent HTTP 2 requests.
         *
         * After TCP connection, the ssl handshake is done if the scheme
         * is @p is_ssl is true, then the HTTP 2 handshake is done.
         * @param host The host of the peer endpoint.
         * It is needed to use it in the ':authority' (Host) header
         * when sending requests.
         * @param epoints The IP addresses of the peer endpoint.
         *
         * The client will try to connect to each address in order,
         * and if a connection attempt succeeds, the client proceeds
         * to ssl handshake and HTTP 2 handshake.
         *
         * If all connection attempts fail, an exception is thrown.
         * @param is_ssl True if HTTP 2 is to be used over encrypted TLS,
         * and false to use HTTP 2 over TCP.
         * @return An awaitable that is when awaited will start the connection
         * process.
         */
        RAD_EXPORT_DECL task<>
        connect(std::string_view host,
                std::span<const tcp::endpoint_type> epoints, bool is_ssl);

        /*!
         * @brief Get the host this client is connected to.
         * If the client is not connected, an empty string is returned.
         * @return The host this client is connected to.
         */
        std::string_view host() const noexcept {
            return hencoder_.host();
        }

        /*!
         * @brief Drive the connection to send requests, receive responses,
         * send ACK for incoming SETTINGS and PING, and send PING frames to
         * detect if the connection is broken or not.
         *
         * The client must be connected and completed the HTTP 2 handshake prior
         * to driving the connection.
         * @return An awaitable that is when awaited will drive the connection.
         * The awaitable will complete when the client is stopped.
         */
        RAD_EXPORT_DECL task<> drive();

        /*!
         * @brief Perform an async HTTP request and retrieve the
         * result. Note that the async operation will not start
         * until the returned awaitable is awaited.
         *
         * The client must be connected and completed the HTTP 2 handshake prior
         * to sending a request.
         * @param url The target url.
         * The url host must be the same host this client is connected to,
         * otherwise an exception is thrown.
         * @param method The HTTP request method.
         * @param hdrs The HTTP headers to include in the
         * request.
         *
         * If the Host header is in the headers, it must have the same
         * value as the current host, otherwise the request will be malformed.
         *
         * If the Content-Length is in the headers, it must have the same value
         * as the passed @p body size, otherwise the request will be malformed.
         * @param body An optional request body.
         * @param res This is where the result http response
         * will be stored. If the response contains any body it
         * will not be stored in the body of this response but
         * will be appended to @p res_buff instead.
         * @param res_buff The response body will be appended to
         * this dynamic buffer.
         * @return An awaitable that is when awaited will start
         * the async operation.
         */
        RAD_EXPORT_DECL task<> request(std::string_view url, verb method,
                                       headers hdrs, const_buffer body,
                                       response& res, dynamic_buffer res_buff);

        /*!
         * @brief Perform an async HTTP request and retrieve the
         * result. Note that the async operation will not start
         * until the returned awaitable is awaited.
         *
         * The client must be connected and completed the HTTP 2 handshake prior
         * to sending a request.
         * @param url The target url.
         * The url host must be the same host this client is connected to,
         * otherwise an exception is thrown.
         * @param method The HTTP request method.
         * @param hdrs The HTTP headers to include in the
         * request.
         *
         * If the Host header is in the headers, it must have the same
         * value as the current host, otherwise the request will be malformed.
         *
         * If the Content-Length is in the headers, it must have the same value
         * as the passed @p body size, otherwise the request will be malformed.
         * @param body An optional request body.
         * @param res This is where the result http response
         * will be stored. If the response contains any body it
         * will be stored in the body of this response.
         * @return An awaitable that is when awaited will start
         * the async operation.
         */
        task<> request(std::string_view url, verb v, headers hdrs,
                       const_buffer body, response& res) {
            return request(url, v, std::move(hdrs), body, res,
                           dynamic_buffer(res.body));
        }

        /*!
         * @brief Perform an async HTTP request and retrieve the
         * result. Note that the async operation will not start
         * until the returned awaitable is awaited.
         *
         * The client must be connected and completed the HTTP 2 handshake prior
         * to sending a request.
         * @param url The target url.
         * The url host must be the same host this client is connected to,
         * otherwise an exception is thrown.
         * @param method The HTTP request method.
         * @param hdrs The HTTP headers to include in the
         * request.
         *
         * If the Host header is in the headers, it must have the same
         * value as the current host, otherwise the request will be malformed.
         *
         * If the Content-Length is in the headers, it must have the same value
         * as the passed @p body size, otherwise the request will be malformed.
         * @param body An optional request body.
         * @param res_buff The response body will be appended to
         * this dynamic buffer.
         * @return An awaitable that is when awaited will start
         * the async operation. The result of the awaitable is a
         * response struct that contains the response status and
         * headers but not the body.
         */
        task<response> request(std::string_view url, verb v, headers hdrs,
                               const_buffer body, dynamic_buffer res_buff) {
            response res;
            co_await request(url, v, std::move(hdrs), body, res, res_buff);
            co_return std::move(res);
        }

        /*!
         * @brief Perform an async HTTP request and retrieve the
         * result. Note that the async operation will not start
         * until the returned awaitable is awaited.
         *
         * The client must be connected and completed the HTTP 2 handshake prior
         * to sending a request.
         * @param url The target url.
         * The url host must be the same host this client is connected to,
         * otherwise an exception is thrown.
         * @param method The HTTP request method.
         * @param hdrs The HTTP headers to include in the
         * request.
         *
         * If the Host header is in the headers, it must have the same
         * value as the current host, otherwise the request will be malformed.
         *
         * If the Content-Length is in the headers, it must have the same value
         * as the passed @p body size, otherwise the request will be malformed.
         * @param body An optional request body.
         * @return An awaitable that is when awaited will start
         * the async operation. The result of the awaitable is a
         * response struct that contains the response status,
         * headers and body.
         */
        task<response> request(std::string_view url, verb v, headers hdrs = {},
                               const_buffer body = {}) {
            response res;
            co_await request(url, v, std::move(hdrs), body, res,
                             dynamic_buffer(res.body));
            co_return std::move(res);
        }

        /*!
         * @brief Send a PING frame to the peer endpoint.
         * The client must be connected and completed the HTTP 2 handshake prior
         * to sending PING.
         * @return An awaitable that is when awaited will start
         * the async operation.
         */
        RAD_EXPORT_DECL task<> ping();

        /*!
         * @brief Send a GOAWAY frame with the specified HTTP 2 error code.
         *
         * The client must be connected and completed the HTTP 2 handshake prior
         * to sending a GOAWAY frame.
         *
         * If the client has already stopped, nothing will be sent since the
         * connection may have been broken.
         * @param error The GOAWAY frame HTTP 2 error code.
         * @return An awaitable that is when awaited will start
         * the async operation.
         */
        RAD_EXPORT_DECL task<> async_close(http2::error error);

        /*!
         * @brief Stop any ongoing request and close the connection with the
         * peer endpoint.
         */
        RAD_EXPORT_DECL void stop() noexcept;

        /*!
         * @brief Get the total sent bytes since the client has connected.
         * Used for testing only.
         * @return The total sent bytes since the client has connected.
         */
        std::size_t total_sent_bytes() const noexcept {
            return total_sent_;
        }

        /*!
         * @brief Get the total received bytes since the client has connected.
         * Used for testing only.
         * @return The total received bytes since the client has connected.
         */
        std::size_t total_received_bytes() const noexcept {
            return total_received_;
        }

        /*!
         * @brief Get the current timeout settings.
         * @return The current timeout settings.
         */
        const endpoint_timeout timeouts() const noexcept {
            endpoint_timeout t;
            t.handshake_timeout = handshake_timeout_;
            t.idle_timeout = idle_timeout_;
            t.keep_alive_pings = keep_alive_pings_;
            return t;
        }

        /*!
         * @brief Set the handshake and idle timeouts and whether to send
         * PING for idle connection or not.
         * @param timeouts The timeout settings.
         */
        void timeouts(const endpoint_timeout& timeouts) noexcept {
            handshake_timeout_ = timeouts.handshake_timeout;
            idle_timeout_ = timeouts.idle_timeout;
            keep_alive_pings_ = timeouts.keep_alive_pings;
        }

        /*!
         * @brief Disable timeouts.
         * If the connection becomes idle, it will never timeout.
         */
        void disable_timeouts() noexcept {
            using namespace std::chrono;
            handshake_timeout_ = milliseconds::max();
            idle_timeout_ = milliseconds::max();
        }

        /*!
         * @brief Check if automatic redirection is enabled.
         * @return True if automatic redirection is enabled, otherwise false.
         */
        bool follow_redirection() const noexcept {
            return follow_redirections_;
        }

        /*!
         * @brief Set whether to enable automatic redirection or not.
         * @param follow True to enable automatic redirection, and false to
         * diable it.
         */
        void follow_redirection(bool follow) noexcept {
            follow_redirections_ = follow;
        }

        /*!
         * @brief Get the maximum allowed successive redirections.
         * @return The maximum allowed successive redirections.
         */
        uint32_t max_redirections() const noexcept {
            return max_redirections_;
        }

        /*!
         * @brief Set the maximum allowed successive redirections.
         * @param n The maximum allowed successive redirections.
         */
        void max_redirections(uint32_t n) noexcept {
            max_redirections_ = n;
        }

        /*!
         * @brief Get the maximum allowed successive CONTINUATION
         * frames the client may receive before raising a connection error.
         *
         * Limiting successive CONTINUATION frames is necessary to protect
         * against CONTINUATION frames flood.
         *
         * Default value is 8.
         * @return The maximum allowed successive CONTINUATION frames.
         */
        std::uint32_t max_continuation_frames() const noexcept {
            return self_max_continuation_frames_;
        }

        /*!
         * @brief Set the maximum allowed successive CONTINUATION
         * frames the client may receive before raising a connection error.
         *
         * Limiting successive CONTINUATION frames is necessary to protect
         * against CONTINUATION frames flood.
         *
         * Default value is 8.
         * @param n The maximum allowed successive CONTINUATION frames.
         */
        void max_continuation_frames(std::uint32_t n) noexcept {
            self_max_continuation_frames_ = n;
        }

        /*!
         * @brief Get the maximum allowed body size.
         * By default the maximum body size is the max value of `std::uint64_t`.
         * @return The maximum allowed body size.
         */
        std::uint64_t max_body_size() const noexcept {
            return max_body_size_;
        }

        /*!
         * @brief Set the maximum allowed body size.
         * @param n The maximum allowed body size.
         */
        void max_body_size(std::uint64_t n) noexcept {
            max_body_size_ = n;
        }

        /*!
         * @brief Check if final non OK responses are treated as error or not.
         *
         * Non OK responses are accepted and returned to the user by default.
         * @return True if non OK responses are accepted and not treated as
         * error, otherwise fale.
         */
        bool accept_non_ok_responses() const noexcept {
            return !require_ok_response_;
        }

        /*!
         * @brief Decide whether to accept non OK responses or
         * not. If this option is disabled, a non OK response
         * will be treaded as an error. OK responses are those
         * with status in the range (200 - 299). By default non
         * OK responses are accepted.
         * @param yes True to accept non OK responses, and false
         * to treat them as error.
         */
        void accept_non_ok_responses(bool yes) noexcept {
            require_ok_response_ = !yes;
        }

        /*!
         * @brief Get the maximum allowed response headers size.
         * The size corresponds to the decompressed headers.
         * By default it is 16 KB.
         * @return The maximum allowed response headers size.
         */
        std::uint32_t max_headers_size() const noexcept {
            return self_max_header_list_size_;
        }

        /*!
         * @brief Set the maximum allowed response headers size.
         * The size corresponds to the decompressed headers.
         *
         * The passed value will not take effect until it is ACKed by the peer.
         * @param n The maximum allowed response headers size.
         */
        RAD_EXPORT_DECL void max_headers_size(std::uint32_t n);

    private:
        endpoint_config get_connect_config() const noexcept {
            endpoint_config cfg;
            cfg.header_table_size = default_header_table_size;
            cfg.initial_window_size = default_client_window_size;
            cfg.max_frame_size = min_payload_size;
            cfg.max_header_list_size = 16 * 1024;
            cfg.enable_push = false;
            return cfg;
        }

        void update_settings(settings_frame sframe);

        void apply_local_settings(const settings_frame& sframe);

        void init_read_buffers_and_hpack_tables();

        task<> do_handshake();

        task<> do_connect(std::string_view host,
                          std::span<const tcp::endpoint_type> epoints,
                          bool is_ssl);

        task<> drive_until_one_stream_is_closed();

        task<> process_rquests_and_bodies(std::error_code& ec,
                                          std::error_code& read_ec,
                                          const bool& stop_flag);

        // Check if a new stream HEADERS frame can be sent to
        // the peer. This will return true if idle_streams_ is
        // not empty and open_streams_ is less than
        // max_concurrent_streams_. Initially
        // max_concurrent_streams_ will be its max value.
        bool can_open_new_streams() const noexcept {
            return !idle_streams_.empty() &&
                   open_streams_.size() < peer_max_concurrent_streams_;
        }

        bool send_queue_empty() const noexcept;

        bool has_local_open_streams() const noexcept;

        bool has_remote_open_streams() const noexcept;

        task<size_t> send_one_stream_request(std::error_code& ec);

        task<size_t> send_streams_bodies_until(std::error_code& ec);

        task<> timeout_detector(std::error_code& ec, const bool& stop_flag);

        task<> receive_and_parse_frames(std::error_code& ec,
                                        std::error_code& write_ec,
                                        const bool& stop_flag);

        size_t collect_data_frames(
            std::vector<std::pair<frame_header, const_buffer>>& data_frames);

        void prepare_pending_send_queue();

        task<> process_send_queue(std::error_code& ec);

        task<size_t> send_ping_acks(std::error_code& ec);

        task<size_t> send_settings_ack(std::error_code& ec);

        task<size_t> send_window_update(std::error_code& ec);

        task<> send_streams_requests(std::error_code& ec);

        task<> send_streams_bodies(std::error_code& ec);

        task<> frames_reader(std::error_code& ec);

        void on_received_setting(const setting_value& sframe,
                                 std::error_code& ec);

        uint32_t make_new_stream_id() noexcept;

        void close_open_stream(stream& s, const std::error_code& ec,
                               bool send_rst) noexcept;

        // called after entire receive of HEADERS, CONTINUATION
        // and DATA frames since these frames may have
        // END_STREAM flag set
        void on_stream_received_headers(stream& s, std::uint8_t flags) noexcept;

        void on_stream_received_data(stream& s, std::uint8_t flags) noexcept {
            on_stream_received_headers(s, flags | END_HEADERS_FLAG);
        }

        bool handle_headers_block(stream& s, std::error_code& ec) noexcept;

        void close_streams(const std::error_code& ec) noexcept;

        void parse_frames(std::error_code& ec);

        bool validate_frame_header(const frame_header& header,
                                   std::error_code& ec) noexcept;

        void process_frame_payload(parser_frame_payload_stage& payload,
                                   std::error_code& ec);

        void process_settings_frame_payload(parser_frame_payload_stage& payload,
                                            std::error_code& ec);

        void process_data_frame_payload(parser_frame_payload_stage& payload,
                                        std::error_code& ec);

        void process_headers_frame_payload(parser_frame_payload_stage& payload,
                                           std::error_code& ec);

        void
        process_continuation_frame_payload(parser_frame_payload_stage& payload,
                                           std::error_code& ec);

        void
        process_priority_frame_payload(parser_frame_payload_stage& payload);

        void process_ping_frame_payload(parser_frame_payload_stage& payload);

        void process_rst_frame_payload(parser_frame_payload_stage& payload);

        void process_goaway_frame_payload(parser_frame_payload_stage& payload,
                                          std::error_code& ec);

        void
        process_window_update_frame_payload(parser_frame_payload_stage& payload,
                                            std::error_code& ec);

        bool read_padding_length(parser_frame_payload_stage& payload,
                                 std::error_code& ec, bool is_data) noexcept;

        bool read_priority(parser_frame_payload_stage& payload) noexcept;

        bool read_data_content(parser_frame_payload_stage& payload,
                               dynamic_buffer sink, bool is_data);

        bool read_padding_content(parser_frame_payload_stage& payload,
                                  std::error_code& ec, bool is_data);

        // client id is odd and starts at 1
        // it will be assigned 1 on the first frame opened
        std::uint32_t last_client_id_ = 0;
        // server id is even and starts at 2
        // it will be assigned 2 on the first push promise
        // received
        std::uint32_t last_server_id_ = 0;
        // PING opaque data counter starting from 1
        // and incremented for each PING sent.
        std::uint64_t last_ping_opaque_data_ = 1;
        // count of received SETTINGS that need to be ACKed.
        std::uint32_t pending_settings_acks_ = 0;
        // window size of the whole connection.
        std::int32_t window_size_ = default_client_window_size;
        // initial window size assigned to each opened stream by the client.
        std::uint32_t self_streams_initial_window_size_ =
            min_allowed_window_size;
        // initial window size assigned to each opened stream by the server.
        // currently unused since PUSH_PROMIST is  disabled.
        std::uint32_t peer_streams_initial_window_size_ =
            min_allowed_window_size;

        // configured max payload size. initially it is set to
        // 2^14
        uint32_t self_max_payload_size_ = 1ul << 14;
        // received max payload size. initially it is set to
        // 2^14
        uint32_t peer_max_payload_size_ = 1ul << 14;
        // negotiated max concurrent streams size
        uint32_t peer_max_concurrent_streams_ =
            std::numeric_limits<uint32_t>::max();
        // configured max headers size after decompression.
        // initially it is 16 KB
        // note that this is not set by the received
        // SETTINGS_MAX_HEADER_LIST_SIZE it is instead sent to
        // the server. the received value is the server side
        // advised value.
        uint32_t self_max_header_list_size_ =
            std::numeric_limits<uint32_t>::max();
        // received SETTINGS_MAX_HEADER_LIST_SIZE.
        uint32_t peer_max_header_list_size_ =
            std::numeric_limits<uint32_t>::max();
        // configured max continuation frames. initially it
        // is 8.
        uint32_t self_max_continuation_frames_ = 8;
        // max allowed successive redirections.
        uint32_t max_redirections_ = 20;
        // max allowed body size.
        std::uint64_t max_body_size_ =
            std::numeric_limits<std::uint64_t>::max();
        // the last connection error
        std::error_code connection_ec_;
        // when sending a GOAWAY frame set this to error value
        std::optional<error> to_send_goaway_ec_;

        // set to true when rbuf_ is not empty but the existing
        // data is not enough to decode something.
        bool rbuf_exhausted_ = false;
        // set to true when the first SETTINGS frame is received
        // to stop the driver.
        bool received_connection_settings_ = false;
        // will be set to true when the first SETTINGS ACK is received
        // to stop the driver.
        bool received_connection_settings_ack_ = false;
        // set to true to use the ssl stream.
        bool is_ssl_ = false;
        // once stopped, the client can't start again.
        bool stopped_ = false;
        // set to true when a coroutine drives the connection to
        // prevent other coroutines from driving the connection.
        bool driving_the_connection_ = false;
        // set to true when a stream enter the closed state.
        bool closed_a_stream_ = false;
        // if true, follow the redirections
        bool follow_redirections_ = true;
        // if true send PING frames to detect connection lose.
        bool keep_alive_pings_ = true;
        // if true, treat receive of response other than 2XX as error.
        bool require_ok_response_ = false;
        // set to true when a GOAWAY frame is received.
        bool received_goaway_ = false;

        // either parsing a header or a payload
        std::variant<parser_frame_header_stage, parser_frame_payload_stage>
            parser_stage_;
        // the last received frame header from the peer
        // if no frames were received, this will be nullopt
        std::optional<frame_header> last_header_;

        // idle streams that have reserved stream id but are not
        // sent yet
        stack_list<stream> idle_streams_;
        // open and half closed (local or remote) streams
        // closed streams are not in the list
        stack_list<stream> open_streams_;

        // PING opaque data that need to be sent to the peer
        std::vector<std::pair<uint64_t, bool>> pending_pings_;
        // opaque data of sent PING that has not been ACKed yet
        std::vector<uint64_t> wanted_pings_acks_;
        // pending RST_STREAM frames that need to be sent to the
        // peer
        std::vector<std::pair<std::uint32_t, error>> pending_rst_streams_;
        // pending settings that need to be sent
        std::optional<settings_frame> pending_send_settings_;
        // pending settings that need until ACK is received
        std::vector<settings_frame> pending_ack_settings_;

        // prepared DATA frames. each pair consists of a frame
        // header and payload the payload is the same as
        // provided data since padding is not used
        std::vector<std::pair<frame_header, const_buffer>> data_frames_;

        // buffer used to write HEADERS, CONTINUATION, SETTINGS,
        // WINDOW_UPDATE and PING frames (headers and payloads).
        // also, used to write headers of DATA frames.
        std::vector<uint8_t> write_buff_;

        // a series of frame header buffer followed by its
        // payload buffer the header may be HEADERS or DATA the
        // payload is the buffer data since padding is not used
        std::vector<const_buffer> frames_buffs_;

        // buffer used to read incoming http2 data from the peer
        // the size of this buffer is not changed once allocated
        std::vector<uint8_t> read_buff_;
        // ring buffer view over read_buff_ to enable write and
        // read without moving the data
        ring_consumer_producer rbuf_;

        // buffer used to read payloads of HEADERS and
        // CONTINUATION frames
        std::vector<uint8_t> hpack_read_buff_;
        // the HPACK encoder used to encode outgoing requests
        // headers
        hpack_encoder hencoder_;
        // the HPACK decoder used to decode ingoing responses
        // headers
        hpack_decoder hdecoder_;

        // the io transport used to transmit and receive http 2
        // data. depending on is_ssl_, the ssl stream may be
        // used or the underlying tcp socket.
        ssl::stream<tcp::socket> transport_;
        std::chrono::steady_clock::time_point last_recv_time_ =
            std::chrono::steady_clock::now();
        // timeout for the whole handshake process (tcp + tls + connection
        // preface)
        std::chrono::milliseconds handshake_timeout_ = std::chrono::seconds{5};
        // timeout for idle connection (not per stream)
        std::chrono::milliseconds idle_timeout_ = std::chrono::seconds{30};

        // timer used to close the stream on time out
        timer idle_timer_;

        // event used by the write to wait until something to
        // write is available.
        async_event write_event_ =
            async_event{transport_.executor().as_any_executor()};
        // event used by the connection driver to prevent more
        // than one coroutine driving the connection at the same
        // time.
        async_event drive_event_ =
            async_event{transport_.executor().as_any_executor()};

        // the debug message in the received GOAWAY frame.
        std::string goaway_reason_;
        // total sent bytes since connection. for debugging and
        // testing.
        std::size_t total_sent_ = 0;
        // total received bytes since connection. for debugging
        // and testing.
        std::size_t total_received_ = 0;
    };
} // namespace RAD_LIB_NAMESPACE::net::http2