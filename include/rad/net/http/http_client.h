#pragma once
#include <rad/async/timer.h>
#include <rad/coro/task.h>
#include <rad/net/dns/doh_resolver.h>
#include <rad/net/http/http_parser.h>
#include <rad/net/socket_options.h>
#include <rad/net/ssl/stream.h>
#include <rad/net/tcp.h>
#include <rad/net/url/url.h>
#include <rad/ring_buffer_consumer.h>

#include <memory>

namespace RAD_LIB_NAMESPACE::net::http {
    /*!
     * @brief Async HTTP 1.1 client to perform HTTP 1.1 requests and retrieve
     * responses. Resolve operation if required are performed using DOH
     * resolver. To disable DOH resolver call set_use_doh_resolver() and
     * pass false. If DOH resolver is used and fails, the client falls back
     * to system resolver.
     *
     * Only one outstanding request may exist at a time.
     *
     * The client is not thread safe and may not be accessed by more than one
     * thread concurrently, so if the used io timer executor runs on multiple
     * threads it should be wrapped in a strand, and this strand executor should
     * be used instead.
     *
     * The client tries to keep the connection to the server open as long as
     * possible to save the cost of TCP connection establishment, but if a
     * request is made to a new host, the previous connection, if open, is
     * closed and a new connection is made to the new host.
     *
     * Redirections are followed by default if the response contains a Location
     * header, and when many successive redirections are encountered an error is
     * raised.
     *
     * It is possible to disable automatic redirections and set the maximum
     * allowed successive redirections using `follow_redirection` and
     * `max_redirections` methods.
     *
     * To detect and close idle connections, a timeout timer is used with a
     * configurable idle timeout to close the connection if this timeout passes
     * without any data received from the peer.
     *
     * To configure the timeout, use `timeout`. To disable the timeout use
     * `disable_timeout` or call `timeout` with very large value.
     */
    class http_client {
    public:
        /*!
         * @brief The type of executor used by the http client.
         * Typically this is io_executor.
         */
        using executor_type = io_executor;

        /*!
         * @brief Construct the http client with an executor and
         * ssl context.
         * @param ex The executor used by the underlying socket
         * and ssl stream. This executor must be both timer and
         * io executor. Since http client is not thread safe,
         * the executor must be sequential like strand or
         * io_loop running on single thread.
         * @param ctx The ssl context used by the underlying ssl
         * stream.
         */
        http_client(IoTimerExecutor auto& ex, ssl::context_base& ctx)
            : resolver_{ex, ctx}, stream_{ctx, ex} {
            // to make sure resizing will not invalidate
            // existing buffer
            buffer_storage_.resize(read_buffer_size);
            rbuf_ = ring_consumer_producer(buffer(buffer_storage_));
        }

        /*!
         * @brief Perform an async HTTP request and retrieve the
         * result. Note that the async operation will not start
         * until the returned awaitable is awaited.
         *
         * @param url The target url.
         * @param method The HTTP request method.
         * @param hdrs The HTTP headers to include in the
         * request.
         *
         * If the Host header is in the headers, it must have the same
         * value as the url host, otherwise the request will be malformed.
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
        RAD_EXPORT_DECL task<> request(std::string_view url, verb v,
                                       headers hdrs, const_buffer body,
                                       response& res, dynamic_buffer res_buff);

        /*!
         * @brief Perform an async HTTP request and retrieve the
         * result. Note that the async operation will not start
         * until the returned awaitable is awaited.
         *
         * @param url The target url.
         * @param method The HTTP request method.
         * @param hdrs The HTTP headers to include in the
         * request.
         *
         * If the Host header is in the headers, it must have the same
         * value as the url host, otherwise the request will be malformed.
         *
         * If the Content-Length is in the headers, it must have the same value
         * as the passed @p body size, otherwise the request will be malformed.
         * @param body An optional request body.
         * @param res This is where the result http response
         * will be stored. If the response contains any body it
         * will be stored in the body of this response..
         * @return An awaitable that is when awaited will start
         * the async operation.
         */
        task<> request(std::string_view url, verb v, headers hdrs,
                       const_buffer body, response& res) {
            co_await request(url, v, std::move(hdrs), body, res,
                             dynamic_buffer(res.body));
        }

        /*!
         * @brief Perform an async HTTP request and retrieve the
         * result. Note that the async operation will not start
         * until the returned awaitable is awaited.
         *
         * @param url The target url.
         * @param method The HTTP request method.
         * @param hdrs The HTTP headers to include in the
         * request.
         *
         * If the Host header is in the headers, it must have the same
         * value as the url host, otherwise the request will be malformed.
         *
         * If the Content-Length is in the headers, it must have the same value
         * as the passed @p body size, otherwise the request will be malformed.
         * @param body An optional request body.
         * @param res_buff The response body will be appended to
         * this dynamic buffer.
         * @return An awaitable that is when awaited will start
         * the async operation.
         * The result of the awaitable is a
         * response struct that contains the response status and
         * headers but not the body.
         */
        task<response> request(std::string_view url, verb v, headers hdrs,
                               const_buffer body, dynamic_buffer res_buff) {
            response res;
            co_await request(url, v, std::move(hdrs), std::move(body), res,
                             res_buff);
            co_return std::move(res);
        }

        /*!
         * @brief Perform an async HTTP request and retrieve the
         * result. Note that the async operation will not start
         * until the returned awaitable is awaited.
         *
         * @param url The target url.
         * @param method The HTTP request method.
         * @param hdrs The HTTP headers to include in the
         * request.
         *
         * If the Host header is in the headers, it must have the same
         * value as the url host, otherwise the request will be malformed.
         *
         * If the Content-Length is in the headers, it must have the same value
         * as the passed @p body size, otherwise the request will be malformed.
         * @param body An optional request body.
         * @param res_buff The response body will be appended to
         * this dynamic buffer.
         * @return An awaitable that is when awaited will start
         * the async operation.
         * The result of the awaitable is a
         * response struct that contains the response status,
         * headers and the body.
         */
        task<response> request(std::string_view url, verb v, headers hdrs = {},
                               const_buffer body = {}) {
            response res;
            co_await request(url, v, std::move(hdrs), std::move(body), res,
                             dynamic_buffer(res.body));
            co_return std::move(res);
        }

        /*!
         * @brief Cancel any pending requests.
         * New requests can be made after call to cancel.
         */
        void cancel() noexcept {
            canceled_ = true;
            resolver_.cancel();
            stream_.next_layer().cancel();
            server_keepalive_ = {};
            last_transfer_time_ = {};
            connected_host_.clear();
        }

        /*!
         * @brief Get the host this client is connected to.
         * If the client is not connected, an empty string is returned.
         * @return The host this client is connected to.
         */
        std::string_view host() const noexcept {
            return connected_host_;
        }

        /*!
         * @brief Get the timeout. The default is 30 seconds.
         * @return The timeout in seconds.
         */
        std::chrono::seconds timeout() const noexcept {
            return timeout_secs_;
        }

        /*!
         * @brief Set the timeout.
         * @param time The timeout duration.
         */
        template <class Rep, class Period>
        void timeout(std::chrono::duration<Rep, Period> time) noexcept {
            using namespace std::chrono;
            timeout_secs_ = duration_cast<seconds>(time);
            if (timeout_secs_ < 1s) {
                timeout_secs_ = 1s;
            }
        }

        /*!
         * @brief Restore the default timeout (30 seconds)
         */
        void set_default_timeout() noexcept {
            timeout_secs_ = default_timeout;
        }

        /*!
         * @brief Disable the timeout setting.
         */
        void disable_timeout() noexcept {
            using namespace std::chrono;
            timeout_secs_ = seconds::max();
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
        uint8_t max_redirections() const noexcept {
            return max_redirections_n_;
        }

        /*!
         * @brief Set the maximum allowed successive redirections.
         * @param n The maximum allowed successive redirections.
         */
        void max_redirections(uint8_t n) noexcept {
            max_redirections_n_ = n;
        }

        /*!
         * @brief Get the maximum allowed body size.
         * By default the maximum body size is the max value of `std::size_t`.
         * @return The maximum allowed body size.
         */
        std::size_t max_body_size() const noexcept {
            return max_body_size_;
        }

        /*!
         * @brief Set the maximum allowed body size.
         * @param n The maximum allowed body size.
         */
        void max_body_size(std::size_t n) noexcept {
            max_body_size_ = n;
        }

        /*!
         * @brief Set whether to use DOH resolver when resolving
         * hosts or not. If set to true, the host resolve is
         * first attempted using DOH and on failure will switch
         * back to the os resolver.
         * @param on True to try first with DOH resolver, and
         * false to use the system resolver directly.
         */
        void set_use_doh_resolver(bool on) noexcept {
            use_doh_resolver_ = on;
        }

        /*!
         * @brief Get a reference to the executor used by the
         * client casted to io executor
         * @return a reference to the executor used by the
         * client casted to io executor
         */
        io_executor& executor() noexcept {
            return resolver_.executor();
        }

        /*!
         * @brief Get a reference to the executor used by the
         * client casted to io executor
         * @return a reference to the executor used by the
         * client casted to io executor
         */
        const io_executor& executor() const noexcept {
            return resolver_.executor();
        }

        /*!
         * @brief Get a reference to the executor used by the
         * client casted to timer executor
         * @return a reference to the executor used by the
         * client casted to timer executor
         */
        class timer_executor& timer_executor() noexcept {
            return resolver_.timer_exutor();
        }

        /*!
         * @brief Get a reference to the executor used by the
         * client casted to timer executor
         * @return a reference to the executor used by the
         * client casted to timer executor
         */
        const class timer_executor& timer_executor() const noexcept {
            return resolver_.timer_exutor();
        }

    private:
        static constexpr std::size_t read_buffer_size = 1024 * 16;
        static constexpr std::size_t max_line_len = 51;

        task<std::pair<bool, url>> connect_to_host(std::string_view url_string);

        task<> send_request(const url& req_url, verb v, const headers& hdrs,
                            const_buffer body, std::error_code& ec);

        // reads in read_buff_ and parses the response into res
        // and returns the ready body which may be empty
        task<> read_http_response(response& res, const_buffer pending_body);

        struct redirect_location {
            verb method = verb::get;
            std::string url;
        };

        // on error an exception is thrown. returns non empty
        // url string if there is a redirection
        task<std::optional<redirect_location>>
        try_request(std::string_view url, response& res, verb v,
                    const headers& hdrs, const_buffer body,
                    dynamic_buffer res_buff);

        task<std::size_t> read_some();

        task<std::size_t> read_some(std::error_code& ec);

        task<> read_all(mutable_buffer buff);

        task<> read_with_content_length(std::size_t content_length,
                                        dynamic_buffer output);

        task<> read_chunked(dynamic_buffer output);

        task<> read_until_eof(std::size_t consumed_size, dynamic_buffer output);

        bool get_keep_alive_settings(const headers& hdrs) noexcept;

        bool is_still_connected() const noexcept;

        static constexpr auto default_timeout = std::chrono::seconds(30);

        doh::resolver resolver_;
        ssl::stream<tcp::socket> stream_;
        std::chrono::seconds timeout_secs_ = default_timeout;
        std::chrono::seconds server_keepalive_ = {};
        std::chrono::steady_clock::time_point last_transfer_time_ = {};
        std::vector<uint8_t> buffer_storage_;
        ring_consumer_producer rbuf_;
        std::size_t max_body_size_ = std::numeric_limits<std::size_t>::max();
        uint8_t max_redirections_n_ = 20;
        bool require_ok_response_ = false;
        bool follow_redirections_ = true;
        bool use_doh_resolver_ = true;
        bool canceled_ = false;
        bool is_ssl_ = false;
        bool expects_100_continue_ = false;
        std::string connected_host_;
    };
} // namespace RAD_LIB_NAMESPACE::net::http