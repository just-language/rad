#include <rad/net/http2/http2_client.h>
#include <rad/coro/when_all.h>
#include <rad/net/url/url.h>
#include <rad/coro/execute_timeout.h>

#include "http2_debug.h"

using namespace RAD_LIB_NAMESPACE;
using namespace net;
using namespace http2;

namespace {
    void copy_end_stream_flag_from_headers_to_continuation(
        const std::optional<frame_header>& last_header,
        frame_header& new_header) noexcept {
        if (!last_header.has_value()) {
            return;
        }
        if (last_header->type != frame_type::headers &&
            last_header->type != frame_type::continuation) {
            return;
        }
        if (new_header.type != frame_type::continuation) {
            return;
        }
        const std::uint8_t set_end_stream =
            last_header->flags & END_STREAM_FLAG;
        new_header.flags |= set_end_stream;
    }

    constexpr std::array<uint8_t, 10> valid_frames_flags = {
        PADDED_FLAG | END_STREAM_FLAG, // data
        PRIORITY_FLAG | PADDED_FLAG | END_HEADERS_FLAG |
            END_STREAM_FLAG,            // headers
        0,                              // priority
        0,                              // rst
        SETTINGS_ACK_FLAG,              // settings,
        PADDED_FLAG | END_HEADERS_FLAG, // push promise
        PING_ACK_FLAG,                  // ping
        0,                              // goaway
        0,                              // window_update
        END_HEADERS_FLAG,               // continuation
    };

    const std::error_code* get_http2_error(const std::error_code& ec1,
                                           const std::error_code& ec2) {
        const auto& h2_cat = http2_category();
        if (ec1 && std::addressof(ec1.category()) == std::addressof(h2_cat)) {
            return std::addressof(ec1);
        }
        else if (ec2 &&
                 std::addressof(ec2.category()) == std::addressof(h2_cat)) {
            return std::addressof(ec2);
        }
        return nullptr;
    }

    std::string get_url_path(std::string_view url, verb method,
                             std::string_view host, std::string_view scheme) {
        net::url req_url{url};
        if (req_url.host_view() != host) {
            throw std::system_error{make_error(http::error::bad_target)};
        }
        if (req_url.scheme() != scheme) {
            throw std::system_error{make_error(http::error::bad_scheme)};
        }
        std::string path;
        std::error_code ec;
        http::make_request_target(req_url, method, false, path, ec);
        if (ec) {
            throw std::system_error{ec};
        }
        return path;
    }

    std::string get_url_path(const net::url& req_url, verb method,
                             std::string_view scheme) {
        if (req_url.scheme() != scheme) {
            throw std::system_error{make_error(http::error::bad_scheme)};
        }
        std::string path;
        std::error_code ec;
        http::make_request_target(req_url, method, false, path, ec);
        if (ec) {
            throw std::system_error{ec};
        }
        return path;
    }
} // namespace

client::client(timer_executor& ex, std::string_view host,
               ssl::stream<tcp::socket>&& transport)
    : transport_{std::move(transport)}, idle_timer_{ex} {
    if (std::addressof(ex.as_any_executor()) !=
        std::addressof(transport_.executor().as_any_executor())) {
        http2_printf("(http2) The provided timer executor is not the same as "
                     "the provided ssl stream executor\n");
        throw std::system_error{
            std::make_error_code(std::errc::invalid_argument)};
    }
    std::string_view selected_alpn =
        transport_.ssl_engine().get_alpn_protocol();
    if (selected_alpn != alpn_value) {
        http2_printf(
            "(http2) The provided ssl stream didn't negotiate 'h2' protocol"
            " using ALPN but negotiated '%s' !\n",
            std::string{selected_alpn}.c_str());
        throw std::system_error{make_error(error::http_1_1_required)};
    }
    hencoder_.set_host(host);
    is_ssl_ = true;
}

client::client(timer_executor& ex, ssl::context_base& ctx,
               std::string_view host, tcp::socket&& transport)
    : transport_{ctx, std::move(transport)}, idle_timer_{ex} {
    if (std::addressof(ex.as_any_executor()) !=
        std::addressof(transport_.executor().as_any_executor())) {
        http2_printf("(http2) The provided timer executor is not the same as "
                     "the provided ssl stream executor\n");
        throw std::system_error{
            std::make_error_code(std::errc::invalid_argument)};
    }
    is_ssl_ = false;
}

task<> client::handshake() {
    if (stopped_) {
        if (connection_ec_) {
            throw std::system_error{connection_ec_};
        }
        else {
            throw std::system_error{
                std::make_error_code(std::errc::connection_aborted)};
        }
    }
    if (last_header_.has_value()) {
        throw std::system_error{
            std::make_error_code(std::errc::already_connected)};
    }
    auto cancel_fn = [&] {
        http2_printf(
            "(http2) Handshake is taking too long! canceling it ...\n");
        transport_.next_layer().cancel();
    };
    co_await execute_timeout(idle_timer_.executor(), handshake_timeout_,
                             do_handshake(), cancel_fn);
}

task<> client::do_handshake() {
    auto stop_on_exit = scope_exit([&] { stopped_ = true; });
    // http2 connection preface
    write_buff_.clear();
    {
        settings_frame sframe =
            make_settings_frame(get_connect_config(), endpoint_config{});
        sframe.write_to_buffer(dynamic_buffer(write_buff_));
        window_update_frame wframe;
        wframe.window_size_increment = default_client_window_size;
        wframe.write_to_buffer(dynamic_buffer(write_buff_));
        pending_ack_settings_.emplace_back(std::move(sframe));
    }
    if (is_ssl_) {
        co_await transport_.async_write(std::array<const_buffer, 2>{
            buffer(connection_preface), buffer(write_buff_)});
    }
    else {
        co_await transport_.next_layer().async_write(
            std::array<const_buffer, 2>{buffer(connection_preface),
                                        buffer(write_buff_)});
    }

    last_recv_time_ = std::chrono::steady_clock::now();
    init_read_buffers_and_hpack_tables();
    // wait for server SETTINGS
    co_await receive_and_parse_frames(connection_ec_, connection_ec_,
                                      received_connection_settings_);
    if (connection_ec_) {
        throw std::system_error{connection_ec_};
    }

    // send back SETTINGS ACK
    if (!send_queue_empty()) {
        co_await process_send_queue(connection_ec_);
        if (connection_ec_) {
            throw std::system_error{connection_ec_};
        }
    }

    // wait for SETTINGS ACK
    if (!received_connection_settings_ack_) {
        co_await receive_and_parse_frames(connection_ec_, connection_ec_,
                                          received_connection_settings_ack_);
        if (connection_ec_) {
            throw std::system_error{connection_ec_};
        }
    }

    // the connection is established
    // the host was previously set
    hencoder_.set_scheme(is_ssl_ ? "https" : "http");
    stop_on_exit.release();
    http2_printf("(http2) client completed handshaked with '%s'\n",
                 std::string(hencoder_.host()).c_str());
}

void client::max_headers_size(std::uint32_t n) {
    if (self_max_header_list_size_ == n) {
        return;
    }
    settings_frame sframe;
    sframe.settings_values.emplace_back(
        setting_value{settings_id::max_header_list_size, n});
    update_settings(std::move(sframe));
}

void client::update_settings(settings_frame sframe) {
    if (stopped_ || sframe.settings_values.empty()) {
        return;
    }
    if (pending_send_settings_ == sframe) {
        return;
    }
    else if (!pending_send_settings_.has_value() &&
             !pending_ack_settings_.empty()) {
        if (pending_ack_settings_.back() == sframe) {
            return;
        }
    }
    if (!pending_send_settings_.has_value() ||
        pending_send_settings_->settings_values.empty()) {
        pending_send_settings_.emplace(std::move(sframe));
    }
    else {
        auto& current_settings = pending_send_settings_->settings_values;
        for (const auto& [sid, sval] : sframe.settings_values) {
            auto it =
                std::find_if(current_settings.begin(), current_settings.end(),
                             [&](const auto& siv) { return siv.id == sid; });
            if (it != current_settings.end()) {
                it->value = sval;
            }
            else {
                current_settings.emplace_back(setting_value{sid, sval});
            }
        }
    }
    write_event_.set();
}

void client::apply_local_settings(const settings_frame& sframe) {
    for (const auto& [id, val] : sframe.settings_values) {
        if (id == settings_id::header_table_size) {
            http2_printf("(http2) Changing  self header_table_size "
                         "from (%d) to (%d)\n",
                         (int)hdecoder_.table().max_size(), (int)val);
            hdecoder_.set_max_table_size(val);
        }
        else if (id == settings_id::initial_window_size) {
            http2_printf("(http2) Changing  self "
                         "self streams_initial_window_size from "
                         "(%d) to (%d)\n",
                         (int)self_streams_initial_window_size_, (int)val);
            self_streams_initial_window_size_ = val;
        }
        else if (id == settings_id::max_frame_size) {
            http2_printf("(http2) Changing  self max_payload_size "
                         "from (%" PRIu32 ") to (%" PRIu32 ")\n",
                         self_max_payload_size_, val);
            self_max_payload_size_ = val;
        }
        else if (id == settings_id::max_header_list_size) {
            http2_printf("(http2) Changing  self "
                         "max_header_list_size from (%" PRIu32 ") to (%" PRIu32
                         ")\n",
                         self_max_header_list_size_, val);
            self_max_header_list_size_ = val;
        }
    }
}

void client::init_read_buffers_and_hpack_tables() {
    read_buff_.resize(std::numeric_limits<uint16_t>::max());
    rbuf_ = ring_consumer_producer{buffer(read_buff_)};
    hencoder_.set_max_table_size(default_header_table_size);
    hdecoder_.set_max_table_size(default_header_table_size);
}

void client::close_open_stream(stream& s, const std::error_code& ec,
                               bool send_rst) noexcept {
    http2_printf("(http2) Closing stream (%d) with error: %d => %s\n", s.id,
                 ec.value(), ec.message().c_str());
    closed_a_stream_ = true;
    s.rst_ec = ec;
    // any unsent body will be discarded
    s.state = stream_state::closed;
    open_streams_.erase(s);
    if (send_rst) {
        pending_rst_streams_.push_back(
            std::pair{s.id, static_cast<error>(ec.value())});
    }
    // wake the write if it is waiting on write_event_
    write_event_.set();
}

void client::on_stream_received_headers(stream& s,
                                        std::uint8_t flags) noexcept {
    const bool is_end_stream = (flags & END_STREAM_FLAG) == END_STREAM_FLAG;
    const bool is_end_headers = (flags & END_HEADERS_FLAG) == END_HEADERS_FLAG;
    // if END_STREAM is set but not END_HEADERS, then
    // END_STREAM will be copied and set for next CONTINUATION frames
    if (is_end_stream && is_end_headers) {
        s.recv_ES();
    }
    // if END_HEADERS is not set, wait for the continuation
    // frames before closing the stream
    if (s.is_closed()) {
        close_open_stream(s, {}, false);
    }
}

void client::close_streams(const std::error_code& ec) noexcept {
    assert(ec);
    if (!connection_ec_) {
        connection_ec_ = ec;
    }
    stop();
    if (idle_streams_.empty() && open_streams_.empty()) {
        return;
    }
    auto close_idle_or_open_stream = [&](stream& s) {
        // if stream is aready closed don't set ec for it
        if (s.is_closed()) {
            return;
        }
        s.state = stream_state::closed;
        if (!s.rst_ec) {
            s.rst_ec = ec;
        }
    };
    for (auto& s : idle_streams_) {
        close_idle_or_open_stream(s);
    }
    for (auto& s : open_streams_) {
        close_idle_or_open_stream(s);
    }
    idle_streams_.clear();
    open_streams_.clear();
}

uint32_t client::make_new_stream_id() noexcept {
    if (last_client_id_ == 0) {
        last_client_id_ = 1;
        return 1;
    }
    std::uint32_t old_local_id = last_client_id_;
    last_client_id_ += 2;
    if (last_client_id_ > max_stream_id || last_client_id_ < old_local_id) {
        http2_printf("(http2) Exhausted stream id !\n");
        if (!to_send_goaway_ec_.has_value()) {
            to_send_goaway_ec_ = error::no_error;
        }
        return 0;
    }
    return last_client_id_;
}

void client::stop() noexcept {
    if (stopped_) {
        return;
    }
    stopped_ = true;
    transport_.next_layer().close();
    write_event_.set();
    drive_event_.set();
}

task<> client::drive() {
    if (!last_header_.has_value()) {
        throw std::system_error{std::make_error_code(std::errc::not_connected)};
    }
    if (stopped_) {
        if (connection_ec_) {
            throw std::system_error{connection_ec_};
        }
        throw std::system_error{
            std::make_error_code(std::errc::connection_aborted)};
    }

    while (!stopped_) {
        if (driving_the_connection_) {
            // wait...
            co_await drive_event_;
            continue;
        }
        driving_the_connection_ = true;
        drive_event_.reset();
        co_await drive_until_one_stream_is_closed();
    }
}

task<> client::ping() {
    if (!last_header_.has_value()) {
        throw std::system_error{std::make_error_code(std::errc::not_connected)};
    }
    if (stopped_) {
        if (connection_ec_) {
            throw std::system_error{connection_ec_};
        }
        throw std::system_error{
            std::make_error_code(std::errc::connection_aborted)};
    }

    const uint64_t ping_opaque_data = last_ping_opaque_data_++;
    http2_printf("(http2) Sending PING opaque data(%" PRIu64 "), ACK(false)\n",
                 ping_opaque_data);
    pending_pings_.push_back(std::pair{ping_opaque_data, false});
    wanted_pings_acks_.push_back(ping_opaque_data);
    write_event_.set();

    auto got_ping_ack = [&] {
        return std::find(wanted_pings_acks_.begin(), wanted_pings_acks_.end(),
                         ping_opaque_data) == wanted_pings_acks_.end();
    };

    while (!stopped_ && !got_ping_ack()) {
        if (driving_the_connection_) {
            // wait...
            co_await drive_event_;
            continue;
        }
        driving_the_connection_ = true;
        drive_event_.reset();
        co_await drive_until_one_stream_is_closed();
    }
    if (stopped_) {
        if (connection_ec_) {
            throw std::system_error{connection_ec_};
        }
        throw std::system_error{
            std::make_error_code(std::errc::connection_aborted)};
    }
}

task<> client::async_close(http2::error error) {
    if (!last_header_.has_value()) {
        throw std::system_error{std::make_error_code(std::errc::not_connected)};
    }
    if (stopped_ || to_send_goaway_ec_.has_value()) {
        co_return;
    }

    to_send_goaway_ec_ = error;
    write_event_.set();
    while (!stopped_) {
        if (driving_the_connection_) {
            // wait...
            co_await drive_event_;
            continue;
        }
        driving_the_connection_ = true;
        drive_event_.reset();
        auto on_exit = scope_exit([&] { stop(); });
        co_await process_send_queue(connection_ec_);
    }
}

task<> client::connect(std::string_view url) {
    net::url req_url{url};
    bool is_ssl = false;
    if (req_url.scheme() == "https") {
        is_ssl = true;
    }
    else if (req_url.scheme() != "http") {
        throw std::system_error{http::make_error(http::error::bad_scheme)};
    }
    uint16_t port = req_url.port();
    // http and https have default ports
    assert(port != 0);
    if (port == 0) {
        throw std::system_error{http::make_error(http::error::bad_scheme)};
    }
    if (req_url.is_host_ipv4() || req_url.is_host_ipv6()) {
        co_await connect(req_url.hostname(), req_url.make_endpoint(), is_ssl);
    }
    else {
        tcp::resolver sys_resolver{transport_.executor()};
        auto results = co_await sys_resolver.async_resolve(req_url.host_view(),
                                                           port, tcp::ipv4());
        co_await connect(req_url.host_view(), results, is_ssl);
    }
}

task<> client::connect(std::string_view host, uint16_t port, bool is_ssl) {
    tcp::resolver sys_resolver{transport_.executor()};
    auto results = co_await sys_resolver.async_resolve(host, port, tcp::ipv4());
    co_await connect(host, results, is_ssl);
}

task<> client::connect(std::string_view host, const tcp::endpoint_type& epoint,
                       bool is_ssl) {
    co_await connect(host, std::span{&epoint, 1}, is_ssl);
}

task<> client::connect(std::string_view host,
                       std::span<const tcp::endpoint_type> epoints,
                       bool is_ssl) {
    if (stopped_) {
        if (connection_ec_) {
            throw std::system_error{connection_ec_};
        }
        else {
            throw std::system_error{
                std::make_error_code(std::errc::connection_aborted)};
        }
    }
    if (last_header_.has_value()) {
        throw std::system_error{
            std::make_error_code(std::errc::already_connected)};
    }
    auto cancel_fn = [&] {
        http2_printf(
            "(http2) Handshake is taking too long! canceling it ...\n");
        transport_.next_layer().cancel();
    };
    co_await execute_timeout(idle_timer_.executor(), handshake_timeout_,
                             do_connect(host, epoints, is_ssl), cancel_fn);
}

task<> client::do_connect(std::string_view host,
                          std::span<const tcp::endpoint_type> epoints,
                          bool is_ssl) {
    // tcp connection
    co_await transport_.next_layer().async_connect(epoints);

    // ssl handshake
    if (is_ssl) {
        transport_.set_hostname(host);
        const auto http2_alpn_protos = std::array<std::string_view, 1>{
            alpn_value,
        };
        transport_.ssl_engine().set_alpn_protocols(http2_alpn_protos);
        co_await transport_.async_handshake(ssl::handshake_type::client);
        std::string_view selected_alpn =
            transport_.ssl_engine().get_alpn_protocol();
        if (selected_alpn != http2_alpn_protos.front()) {
            http2_printf("(http2) failed to negotiate 'h2' protocol"
                         " using ALPN but negotiated '%s' !\n",
                         std::string{selected_alpn}.c_str());
            throw std::system_error{make_error(error::http_1_1_required)};
        }
        http2_printf("(http2) negotiated '%s' protocol using ALPN\n",
                     std::string{selected_alpn}.c_str());
    }

    auto stop_on_exit = scope_exit([&] { stopped_ = true; });

    // must be set so other methods use the ssl stream
    is_ssl_ = is_ssl;

    // http2 connection preface
    write_buff_.clear();
    {
        const settings_frame sframe =
            make_settings_frame(get_connect_config(), endpoint_config{});
        sframe.write_to_buffer(dynamic_buffer(write_buff_));
        window_update_frame wframe;
        wframe.window_size_increment = default_client_window_size;
        wframe.write_to_buffer(dynamic_buffer(write_buff_));
        pending_ack_settings_.emplace_back(std::move(sframe));
    }
    if (is_ssl) {
        co_await transport_.async_write(std::array<const_buffer, 2>{
            buffer(connection_preface), buffer(write_buff_)});
    }
    else {
        co_await transport_.next_layer().async_write(
            std::array<const_buffer, 2>{buffer(connection_preface),
                                        buffer(write_buff_)});
    }

    last_recv_time_ = std::chrono::steady_clock::now();
    init_read_buffers_and_hpack_tables();
    // wait for server SETTINGS
    co_await receive_and_parse_frames(connection_ec_, connection_ec_,
                                      received_connection_settings_);
    if (connection_ec_) {
        throw std::system_error{connection_ec_};
    }

    // send back SETTINGS ACK
    if (!send_queue_empty()) {
        co_await process_send_queue(connection_ec_);
        if (connection_ec_) {
            throw std::system_error{connection_ec_};
        }
    }

    // wait for SETTINGS ACK
    co_await receive_and_parse_frames(connection_ec_, connection_ec_,
                                      received_connection_settings_ack_);
    if (connection_ec_) {
        throw std::system_error{connection_ec_};
    }

    // the connection is established
    hencoder_.set_host(host);
    hencoder_.set_scheme(is_ssl ? "https" : "http");
    stop_on_exit.release();
    http2_printf("(http2) client connected to '%s'\n",
                 std::string(host).c_str());
}

task<> client::request(std::string_view url, verb method, headers hdrs,
                       const_buffer body, response& res,
                       dynamic_buffer res_buff) {
    if (!last_header_.has_value()) {
        throw std::system_error{std::make_error_code(std::errc::not_connected)};
    }
    if (stopped_) {
        if (connection_ec_) {
            throw std::system_error{connection_ec_};
        }
        throw std::system_error{
            std::make_error_code(std::errc::connection_aborted)};
    }

    stream s{*this, method, hdrs, body, res, res_buff};
    s.path = get_url_path(url, method, hencoder_.host(), hencoder_.scheme());

    // on exception the stream may not be removed from the list.
    // for example of many requests are awaited using when_all
    // and one of them fails with exception and cancels other requests
    // the next streams will throw on co_await drive_connection_until()
    // and will not have a chance to be removed from the list
    auto on_exit = scope_exit([&] {
        if (s.is_idle()) {
            idle_streams_.erase(s);
        }
        else if (!s.is_closed()) {
            open_streams_.erase(s);
        }
    });

    idle_streams_.push_back(s);
    write_event_.set();

    const size_t original_res_body_size = res_buff.size();
    const_buffer original_body = body;
    uint32_t redirects_count = 0;
    std::string base_url_str{url};
    while (1) {
        while (!stopped_ && !s.is_closed()) {
            if (driving_the_connection_) {
                // wait...
                co_await drive_event_;
                continue;
            }
            driving_the_connection_ = true;
            drive_event_.reset();
            co_await drive_until_one_stream_is_closed();
        }

        if (s.rst_ec) {
            throw std::system_error{s.rst_ec};
        }
        if (stopped_) {
            if (connection_ec_) {
                throw std::system_error{connection_ec_};
            }
            throw std::system_error{
                std::make_error_code(std::errc::connection_aborted)};
        }

        if (!follow_redirections_ || !s.res.is_redirect() ||
            redirects_count >= max_redirections_) {
            break;
        }
        auto it = s.res.headers.find(http::field::location);
        if (it == s.res.headers.end()) {
            break;
        }

        net::url base_url{base_url_str};
        net::url redirect_url{base_url, it->second};
        if (redirect_url.host_view() != hencoder_.host()) {
            break;
        }

        http2_printf("(http2) stream(%d) is redirected from '%s' to '%s'\n",
                     (int)s.id, base_url_str.c_str(), it->second.c_str());

        s.method = http::change_method_on_redirect(method, s.res.status_code());
        s.path = get_url_path(redirect_url, method, hencoder_.scheme());
        s.res_buff.resize(original_res_body_size);
        base_url_str = redirect_url.href();
        s.restart();
        s.body = original_body;
        s.state = stream_state::idle;
        idle_streams_.push_back(s);
        redirects_count += 1;
    }
    if (require_ok_response_ && !s.res.is_ok()) {
        throw std::system_error{
            http::make_error(http::error::bad_response_code)};
    }
    http2_printf("(http2) stream(%d) has finished\n", (int)s.id);
}

task<> client::drive_until_one_stream_is_closed() {
    std::error_code write_ec, read_ec;

    auto on_exit = scope_exit([&] {
        driving_the_connection_ = false;
        drive_event_.set();
        if (write_ec) {
            close_streams(write_ec);
        }
        else if (read_ec) {
            close_streams(read_ec);
        }
        else if (received_goaway_) {
            close_streams(make_error(error::cancel));
        }
    });

    if (stopped_) {
        co_return;
    }

    closed_a_stream_ = false;
    co_await (process_rquests_and_bodies(write_ec, read_ec, closed_a_stream_) &&
              receive_and_parse_frames(read_ec, write_ec, closed_a_stream_) &&
              timeout_detector(read_ec, closed_a_stream_));

    const std::error_code* h2error = get_http2_error(write_ec, read_ec);
    if (h2error != nullptr && !received_goaway_ &&
        !to_send_goaway_ec_.has_value()) {
        to_send_goaway_ec_ = static_cast<error>(h2error->value());
        co_await process_send_queue(write_ec);
        connection_ec_ = *h2error;
    }
}

task<> client::process_rquests_and_bodies(std::error_code& ec,
                                          std::error_code& read_ec,
                                          const bool& stop_flag) {
    auto on_exit = scope_exit([&] {
        if (!read_ec) {
            read_ec = ec;
        }
        write_event_.set();
        idle_timer_.cancel();
    });
    write_event_.reset();
    while (!stopped_ && !received_goaway_ && !ec && !stop_flag) {
        if (!send_queue_empty()) {
            co_await process_send_queue(ec);
            continue;
        }
        else if (can_open_new_streams()) {
            co_await send_one_stream_request(ec);
            continue;
        }
        else if (has_local_open_streams()) {
            co_await send_streams_bodies_until(ec);
        }
        else {
            // nothing to write now, wait...
            co_await write_event_;
            write_event_.reset();
        }
    }
}

task<size_t> client::send_one_stream_request(std::error_code& ec) {
    if (idle_streams_.empty()) {
        co_return 0;
    }

    auto& s = idle_streams_.front();
    const std::uint32_t new_id = make_new_stream_id();
    if (new_id == 0) {
        // exhausted stream id
        co_return 0;
    }
    assert(new_id != 0);
    make_headers_continuations_frames(new_id, s.method, s.path, s.hdrs,
                                      s.body.empty(), peer_max_payload_size_,
                                      hencoder_, write_buff_, frames_buffs_);
    assert(!frames_buffs_.empty());
    if (frames_buffs_.empty()) {
        ec = make_error(error::compression_error);
        co_return 0;
    }

    s.id = new_id;
    s.window_size = self_streams_initial_window_size_;
    // transition from idle to open state by (send H)
    // if HEADERS has END_STREAM flag set, then transition to half closed
    // local.
    s.send_H();
    const uint32_t body_size = static_cast<uint32_t>(s.body.size());
    if (body_size == 0) {
        s.send_ES();
    }
    idle_streams_.erase(s);
    open_streams_.push_back(s);

    size_t sent_n = 0;
    if (is_ssl_) {
        sent_n += co_await transport_.async_write(frames_buffs_, ec);
    }
    else {
        sent_n +=
            co_await transport_.next_layer().async_write(frames_buffs_, ec);
    }
    if (ec) {
        http2_printf("(http2) Failed to open a new stream: %d ! "
                     "async_write error: "
                     "%d => %s\n",
                     (int)new_id, ec.value(), ec.message().c_str());
        // close this stream if it is not closed yet
        auto it = open_streams_.find(
            [new_id](const stream& s) { return s.id == new_id; });
        if (it != open_streams_.end()) {
            close_open_stream(*it, ec, false);
        }
        co_return 0;
    }

    http2_printf(
        "(http2) Sent %d bytes of frame HEADERS with id (%d) remaining "
        "body %d bytes\n",
        (int)sent_n, (int)new_id, (int)body_size);

    total_sent_ += sent_n;
    co_return sent_n;
}

task<size_t> client::send_streams_bodies_until(std::error_code& ec) {
    data_frames_.clear();
    const size_t total_size = collect_data_frames(data_frames_);
    std::ignore = total_size;
    if (data_frames_.empty()) {
        co_return 0;
    }
    make_frames_buffers(data_frames_, write_buff_, frames_buffs_);
    assert(!frames_buffs_.empty() && !write_buff_.empty());
    if (frames_buffs_.empty()) {
        co_return 0;
    }
    size_t sent_size = 0;
    if (is_ssl_) {
        sent_size = co_await transport_.async_write(frames_buffs_, ec);
    }
    else {
        sent_size =
            co_await transport_.next_layer().async_write(frames_buffs_, ec);
    }
    if (ec) {
        http2_printf("(http2) Failed to write DATA frames !\n");
        co_return 0;
    }
    // half close or close streams with empty bodies
    for (auto it = open_streams_.begin(); it != open_streams_.end();) {
        auto& s = *it;
        // s may be removed from the list, so advance to the next item
        // before erase
        ++it;
        if (s.is_half_closed_local() || s.is_closed() || !s.body.empty()) {
            continue;
        }
        s.send_ES();
        if (s.is_closed()) {
            close_open_stream(s, {}, false);
        }
    }
    http2_printf("(http2) Sent %zu bytes of DATA frames\n", sent_size);
    total_sent_ += sent_size;
    co_return sent_size;
}

bool client::send_queue_empty() const noexcept {
    auto all_streams_windows_are_large = [&] {
        for (auto& s : open_streams_) {
            if (s.is_closed() || s.is_half_closed_remote()) {
                continue;
            }
            if (s.window_size <= min_allowed_window_size) {
                return false;
            }
        }
        return true;
    };
    return pending_pings_.empty() && pending_rst_streams_.empty() &&
           !to_send_goaway_ec_.has_value() &&
           !pending_send_settings_.has_value() && pending_settings_acks_ == 0 &&
           window_size_ > min_allowed_window_size &&
           all_streams_windows_are_large();
}

bool client::has_local_open_streams() const noexcept {
    for (const auto& s : open_streams_) {
        if (!s.is_half_closed_local() && !s.is_closed()) {
            assert(!s.body.empty());
            return true;
        }
    }
    return false;
}

bool client::has_remote_open_streams() const noexcept {
    if (!idle_streams_.empty() || !received_connection_settings_ ||
        !received_connection_settings_ack_ || !wanted_pings_acks_.empty() ||
        !pending_ack_settings_.empty()) {
        return true;
    }
    for (const auto& s : open_streams_) {
        if (!s.is_half_closed_remote() && !s.is_closed()) {
            return true;
        }
    }
    for (const auto& [opaque_data, is_ack] : pending_pings_) {
        std::ignore = opaque_data;
        if (is_ack) {
            return true;
        }
    }
    return false;
}

size_t client::collect_data_frames(
    std::vector<std::pair<frame_header, const_buffer>>& data_frames) {
    data_frames.clear();
    size_t max_chunk_size =
        std::min(peer_max_payload_size_, uint32_t{8 * 1024});
    size_t total_size = 0;
    size_t available_frames = 100;
    while (available_frames > 0 && total_size < 16 * 1024) {
        size_t collected_frames = 0;
        for (auto& s : open_streams_) {
            if (s.is_half_closed_local() || s.is_closed() || s.body.empty()) {
                continue;
            }
            size_t write_size = std::min(max_chunk_size, s.body.size());
            frame_header dfh;
            dfh.type = frame_type::data;
            dfh.stream_id = s.id;
            dfh.length = static_cast<uint32_t>(write_size);
            if (write_size == s.body.size()) {
                dfh.flags = END_STREAM_FLAG;
            }
            data_frames.emplace_back(dfh, s.body.sub_buffer(0, write_size));
            s.body += write_size;
            total_size += write_size;
            collected_frames += 1;
            available_frames -= 1;
            if (available_frames == 0 ||
                total_size >= min_allowed_window_size) {
                break;
            }
        }
        if (collected_frames == 0) {
            break;
        }
    }
    return total_size;
}

void client::prepare_pending_send_queue() {
    if (to_send_goaway_ec_.has_value()) {
        goaway_frame gframe;
        gframe.last_stream_id = last_client_id_;
        gframe.error_code = *to_send_goaway_ec_;
        gframe.debug_data = make_error(*to_send_goaway_ec_).message();
        write_buff_.clear();
        gframe.write_to_buffer(dynamic_buffer(write_buff_));
        http2_printf("(http2) Sending GOAWAY frame with error "
                     "code = %d, debug "
                     "data = '%s' ...\n",
                     (int)gframe.error_code, gframe.debug_data.c_str());
        return;
    }
    settings_frame sframe;
    sframe.set_ack();
    const size_t ping_frames_size =
        ping_frame::needed_write_size() * pending_pings_.size();
    const size_t rst_frames_size =
        rst_stream_frame::needed_write_size() * pending_rst_streams_.size();
    const size_t frame_ack_size =
        pending_settings_acks_ * sframe.needed_write_size();
    size_t window_update_frame_size =
        window_size_ <= min_allowed_window_size
            ? window_update_frame::needed_write_size()
            : 0;
    for (auto& s : open_streams_) {
        if (s.is_closed() || s.is_half_closed_remote()) {
            continue;
        }
        if (s.window_size <= min_allowed_window_size) {
            window_update_frame_size +=
                window_update_frame::needed_write_size();
        }
    }
    size_t settings_frames_size =
        pending_send_settings_.has_value()
            ? pending_send_settings_->needed_write_size()
            : 0;

    write_buff_.clear();
    write_buff_.reserve(ping_frames_size + rst_frames_size + frame_ack_size +
                        window_update_frame_size + settings_frames_size);

    while (pending_settings_acks_ > 0) {
        sframe.write_to_buffer(dynamic_buffer(write_buff_));
        pending_settings_acks_ -= 1;
        http2_printf("(http2) Sending SETTINGS ACK ...\n");
    }

    if (pending_send_settings_.has_value()) {
        pending_send_settings_->write_to_buffer(dynamic_buffer(write_buff_));
        pending_ack_settings_.emplace_back(std::move(*pending_send_settings_));
        pending_send_settings_ = std::nullopt;
    }

    auto insert_window_update_frame = [&](uint32_t stream_id,
                                          int32_t& window_space) {
        window_update_frame wframe;
        wframe.stream_id = stream_id;
        wframe.window_size_increment =
            default_client_window_size - window_space;
        window_space = default_client_window_size;
        wframe.write_to_buffer(dynamic_buffer(write_buff_));
        http2_printf(
            "(http2) Sending WINDOW_UPDATE (%d) on stream (%d) current "
            "window size (%d) ...\n",
            (int)wframe.window_size_increment, (int)stream_id,
            (int)window_space);
    };

    if (window_size_ <= min_allowed_window_size) {
        insert_window_update_frame(0, window_size_);
    }
    for (auto& s : open_streams_) {
        if (s.is_closed() || s.is_half_closed_remote()) {
            continue;
        }
        if (s.window_size <= min_allowed_window_size) {
            insert_window_update_frame(s.id, s.window_size);
        }
    }

    for (const auto& [opaque_data, is_ack] : pending_pings_) {
        ping_frame pf;
        pf.opaque_data = opaque_data;
        if (is_ack) {
            pf.set_ack();
        }
        pf.write_to_buffer(dynamic_buffer(write_buff_));
        http2_printf("(http2) Sending PING opaque data (%" PRIu64
                     ") ACK (%s) ...\n",
                     opaque_data, is_ack ? "true" : "false");
    }
    pending_pings_.clear();
    for (const auto& [id, err] : pending_rst_streams_) {
        rst_stream_frame rsf;
        rsf.stream_id = id;
        rsf.error_code = static_cast<uint32_t>(err);
        rsf.write_to_buffer(dynamic_buffer(write_buff_));
        http2_printf("(http2) Sending RST_STREAM error (%d) stream id "
                     "(%d) ...\n",
                     (int)err, (int)id);
    }
    pending_rst_streams_.clear();
}

task<> client::process_send_queue(std::error_code& ec) {
    prepare_pending_send_queue();
    if (write_buff_.empty()) {
        co_return;
    }
    if (is_ssl_) {
        co_await transport_.async_write(buffer(write_buff_), ec);
    }
    else {
        co_await transport_.next_layer().async_write(buffer(write_buff_), ec);
    }
    if (ec) {
        http2_printf("(http2) Failed to send frames to the server !\n");
    }
    if (to_send_goaway_ec_.has_value()) {
        ec = make_error(*to_send_goaway_ec_);
        // wake the reader
        transport_.next_layer().cancel();
    }
}

task<> client::timeout_detector(std::error_code& ec, const bool& stop_flag) {
    using namespace std::chrono;
    while (!stopped_ && !received_goaway_ && !ec && !stop_flag) {
        std::error_code cancel_ec;
        co_await idle_timer_.wait_for(
            keep_alive_pings_ ? idle_timeout_ / 2 : idle_timeout_, cancel_ec);
        if (cancel_ec) {
            co_return;
        }
        if (stopped_ || ec || stop_flag) {
            break;
        }
        const auto elapsed = steady_clock::now() - last_recv_time_;
        if (elapsed > idle_timeout_) {
            http2_printf("(http2) The connection has been "
                         "idle for too long !\n");
            http2_printf(
                "(http2) Closing the http2 connection due to timeout !\n");
            ec = make_error(error::settings_timeout);
            transport_.next_layer().cancel();
            co_return;
        }
        else if (elapsed > idle_timeout_ / 2 && keep_alive_pings_) {
            const uint64_t ping_opaque_data = last_ping_opaque_data_++;
            http2_printf("(http2) The connection has been "
                         "idle for %dms !\n",
                         (int)duration_cast<milliseconds>(elapsed).count());
            http2_printf("(http2) Sending PING opaque data(%" PRIu64
                         "), ACK(false)\n",
                         ping_opaque_data);
            pending_pings_.push_back(std::pair{ping_opaque_data, false});
            wanted_pings_acks_.push_back(ping_opaque_data);
            last_recv_time_ = steady_clock::now();
            write_event_.set();
        }
    }
}

task<> client::receive_and_parse_frames(std::error_code& ec,
                                        std::error_code& write_ec,
                                        const bool& stop_flag) {
    size_t total_read = 0;
    auto on_exit = scope_exit([&] {
        if (!write_ec) {
            write_ec = ec;
        }
        write_event_.set();
        idle_timer_.cancel();
    });
    while (!stopped_ && !received_goaway_ && !stop_flag && !ec &&
           has_remote_open_streams()) {
        const bool need_reading = rbuf_.empty() || rbuf_exhausted_;
        if (need_reading) {
            assert(!rbuf_.full());
            if (rbuf_.full()) {
                ec = make_error(error::flow_control_error);
                co_return;
            }

            http2_printf("(http2) Reading http2 data into "
                         "available space (%d)...\n",
                         (int)rbuf_.space());
            size_t n = is_ssl_
                           ? co_await transport_.async_read_some(
                                 rbuf_.available_space(), ec)
                           : co_await transport_.next_layer().async_read_some(
                                 rbuf_.available_space(), ec);
            if (ec) {
                http2_printf("(http2) Failed to read from the "
                             "transport with error: "
                             "(%d) => %s !\n",
                             ec.value(), ec.message().c_str());
                co_return;
            }

            assert(n > 0);
            rbuf_.commit(n);
            rbuf_exhausted_ = false;
            total_read += n;
            total_received_ += n;
            last_recv_time_ = std::chrono::steady_clock::now();
            http2_printf("(http2) Read (%d) bytes of http2 data, "
                         "total read: %d\n",
                         (int)n, (int)total_read);
            std::ignore = total_read;
        }

        parse_frames(ec);
        if (ec) {
            http2_printf("(http2) parse_frames() failed !!!\n");
            co_return;
        }
    }
}

void client::parse_frames(std::error_code& ec) {
    bool has_zero_payload = false;
    while (!stopped_ && (!rbuf_.empty() || has_zero_payload) && !ec) {
        has_zero_payload = false;
        if (std::holds_alternative<parser_frame_header_stage>(parser_stage_)) {
            if (rbuf_.size() < frame_header::needed_write_size()) {
                rbuf_exhausted_ = true;
                return;
            }
            frame_header_buffer fh_buff;
            size_t n = rbuf_.write_to(fh_buff.get_buffer());
            assert(n == frame_header::needed_write_size());
            if (n != frame_header::needed_write_size()) {
                ec = make_error(error::protocol_error);
                return;
            }
            frame_header new_header = fh_buff.get_frame_header();
            if (!validate_frame_header(new_header, ec)) {
                assert(ec);
                return;
            }
            // ensure only supported flags are present
            new_header.flags &=
                valid_frames_flags[static_cast<uint32_t>(new_header.type)];
            // if the new frame is CONTINUATION, copy the END_STREAM
            // flag from the previous HEADERS or CONTINUATION frame
            // to end the stream on if END_HEADERS is set and
            // END_STREAM was set in the first HEADERS frame
            copy_end_stream_flag_from_headers_to_continuation(last_header_,
                                                              new_header);
            last_header_ = new_header;
            /*
            printf("(http2) Parsed a new frame header with id (%d),
            type (%d), flags
            (%d == %d) and length (%d)\n",
            (int)last_header_->stream_id, (int)new_header.type,
            (int)last_header_->flags, (int)new_header.flags,
            (int)last_header_->length);
            */
            has_zero_payload = last_header_->length == 0;
            parser_stage_ = parser_frame_payload_stage{
                last_header_->length, last_header_->min_payload_size()};
            continue;
        }
        else if (parser_frame_payload_stage* payload =
                     std::get_if<parser_frame_payload_stage>(&parser_stage_)) {
            // parse zero-length payloads (HEADERS and DATA) because
            // they may contain END_STREAM flag.
            assert(payload->consumed_size <= payload->total_size);
            if (rbuf_.size() < payload->min_read_size) {
                return;
            }
            process_frame_payload(*payload, ec);
            if (ec) {
                return;
            }
        }
    }
}

void client::process_frame_payload(parser_frame_payload_stage& payload,
                                   std::error_code& ec) {
    switch (last_header_->type) {
    case frame_type::data:
        return process_data_frame_payload(payload, ec);
    case frame_type::headers:
        return process_headers_frame_payload(payload, ec);
    case frame_type::priority:
        return process_priority_frame_payload(payload);
    case frame_type::rst_stream:
        return process_rst_frame_payload(payload);
    case frame_type::settings:
        return process_settings_frame_payload(payload, ec);
    case frame_type::push_promise:
        return;
    case frame_type::ping:
        return process_ping_frame_payload(payload);
    case frame_type::goaway:
        return process_goaway_frame_payload(payload, ec);
    case frame_type::window_update:
        return process_window_update_frame_payload(payload, ec);
    case frame_type::continuation:
        return process_continuation_frame_payload(payload, ec);
    default:
        return;
    }
}

bool client::validate_frame_header(const frame_header& header,
                                   std::error_code& ec) noexcept {
    if (!header.is_valid_frame(last_header_, self_max_payload_size_, false,
                               false, last_client_id_, last_server_id_)) {
        ec = make_error(error::protocol_error);
        return false;
    }

    if (header.stream_id == 0) {
        if (header.type == frame_type::settings) {
            http2_printf("(http2) Received SETTINGS frame with "
                         "length (%d)\n",
                         (int)header.length);
        }
        return true;
    }

    auto it = open_streams_.find(
        [&](const stream& s) { return s.id == header.stream_id; });
    if (it == open_streams_.end()) {
        if (header.may_be_sent_to_closed_stream()) {
            return true;
        }
        // connection error of type STREAM_CLOSED
        http2_printf("(http2) Received frame type: %d for closed "
                     "stream id: %d !\n",
                     (int)header.type, (int)header.stream_id);
        ec = make_error(error::stream_closed);
        return false;
    }

    if (it->is_half_closed_remote()) {
        if (header.may_be_sent_to_half_closed_remote_stream()) {
            return true;
        }
        // stream error of type STREAM_CLOSED
        http2_printf("(http2) Received frame type: %d for closed "
                     "stream id: %d !\n",
                     (int)header.type, (int)header.stream_id);
        close_open_stream(*it, make_error(error::stream_closed), true);
        return true;
    }

    if (it->wants_headers && header.type != frame_type::headers &&
        header.type != frame_type::rst_stream &&
        header.type != frame_type::window_update &&
        header.type != frame_type::priority) {
        http2_printf("(http2) The first received frame on stream (%d) is not "
                     "HEADERS but %d!\n",
                     (int)it->id, (int)header.type);
        ec = make_error(error::protocol_error);
        return false;
    }
    else if (!it->wants_headers && header.type == frame_type::headers) {
        // if received_final_res or received_trailers is true here,
        // then we were expecting a CONTINUATION! this path should never
        // be reached since it should have been rejected by
        // header.is_valid_frame()
        if (!it->received_final_res || it->received_trailers) {
            http2_printf("(http2) Received HEADERS frame again on "
                         "stream (%d) !\n",
                         (int)it->id);
            ec = make_error(error::protocol_error);
            return false;
        }
        // received the trailers which must contain END_STREAM flag.
        if ((header.flags & END_STREAM_FLAG) != END_STREAM_FLAG) {
            http2_printf("(http2) Received trailer HEADERS frame without "
                         "END_STREAM flag on stream (%d) !\n",
                         (int)it->id);
            ec = make_error(error::protocol_error);
            return false;
        }
        it->received_trailers = true;
    }
    if (it->wants_headers && header.type == frame_type::headers) {
        it->wants_headers = false;
    }
    if (header.type == frame_type::continuation) {
        it->received_continuations += 1;
        if (it->received_continuations > self_max_continuation_frames_) {
            http2_printf("(http2) Received too many CONTINUATION frames (%d) "
                         "on stream (%d) !\n",
                         (int)it->received_continuations, (int)it->id);
            ec = make_error(error::protocol_error);
            return false;
        }
    }
    else {
        it->received_continuations = 0;
    }

    const bool is_padded = (header.flags & PADDED_FLAG) == PADDED_FLAG;
    const bool end_stream = (header.flags & END_STREAM_FLAG) == END_STREAM_FLAG;
    const bool end_headers =
        (header.flags & END_HEADERS_FLAG) == END_HEADERS_FLAG;
    std::ignore = is_padded;
    std::ignore = end_stream;
    std::ignore = end_headers;

    if (header.type == frame_type::headers) {
        hpack_read_buff_.clear();
        http2_printf("(http2) Received HEADERS frame on stream (%d) with size "
                     "(%d), "
                     "padded? (%s), end stream? (%s), end headers? (%s)\n",
                     (int)header.stream_id, (int)header.length,
                     is_padded ? "true" : "false",
                     end_stream ? "true" : "false",
                     end_headers ? "true" : "false");
    }
    else if (header.type == frame_type::data) {
        it->received_data += header.length;
        if (it->received_data > max_body_size_) {
            http2_printf("(http2) Received DATA frames on stream (%d) with "
                         "size (%" PRIu64
                         ") larger than the maximum allowed body size (%" PRIu64
                         ")!\n",
                         (int)it->id, it->received_data, max_body_size_);
            close_open_stream(*it, make_error(error::frame_size_error), true);
            return true;
        }
        // check Content-Length
        if (auto content_length = it->get_content_length()) {
            if (it->received_data > *content_length) {
                http2_printf(
                    "(http2) Received DATA frames on stream (%d) with "
                    "size (%" PRIu64
                    ") larger than the response Content-Length (%" PRIu64
                    ")!\n",
                    (int)it->id, it->received_data, *content_length);
                close_open_stream(*it, make_error(error::frame_size_error),
                                  true);
                return true;
            }
        }
        // only DATA frames are subject to flow control.
        window_size_ -= header.length;
        it->window_size -= header.length;
        if (window_size_ <= min_allowed_window_size ||
            it->window_size <= min_allowed_window_size) {
            // wake the write if it is waiting on write_event_
            write_event_.set();
        }

        http2_printf("(http2) Received DATA frame on stream (%d) with "
                     "size (%d), "
                     "padded? (%s), end stream? (%s)\n",
                     (int)header.stream_id, (int)header.length,
                     is_padded ? "true" : "false",
                     end_stream ? "true" : "false");
    }

    return true;
}

void client::process_settings_frame_payload(parser_frame_payload_stage& payload,
                                            std::error_code& ec) {
    while (payload.consumed_size < payload.total_size) {
        assert(payload.total_size - payload.consumed_size >= 6);
        setting_value_buffer sbuff;
        if (rbuf_.size() < setting_value::needed_write_size()) {
            http2_printf("(http2) Not enough available buffers to "
                         "read SETTING_VALUE, "
                         "current size (%d) min (%d) !",
                         (int)rbuf_.size(),
                         (int)setting_value::needed_write_size());
            rbuf_exhausted_ = true;
            return;
        }
        payload.consumed_size += rbuf_.write_to(sbuff.get_buffer());
        on_received_setting(sbuff.get_setting_value(), ec);
        if (ec) {
            return;
        }
    }
    parser_stage_ = parser_frame_header_stage{};
    received_connection_settings_ = true;
    // respond to settings
    const bool is_ack =
        (last_header_->flags & SETTINGS_ACK_FLAG) == SETTINGS_ACK_FLAG;
    if (!is_ack) {
        pending_settings_acks_ += 1;
        // wake the writer if it is waiting on write_event_
        write_event_.set();
    }
    else {
        http2_printf("(http2) Received SETTINGS ACK\n");
        if (pending_ack_settings_.empty()) {
            http2_printf("(http2) Received SETTINGS ACK but no "
                         "settings are waiting for ACK!");
            ec = make_error(error::protocol_error);
            return;
        }
        received_connection_settings_ack_ = true;
        settings_frame oldest_sframe = std::move(pending_ack_settings_.front());
        pending_ack_settings_.erase(pending_ack_settings_.begin());
        apply_local_settings(oldest_sframe);
    }
}

void client::on_received_setting(const setting_value& svalue,
                                 std::error_code& ec) {
    if (svalue.id == settings_id::header_table_size) {
        http2_printf("(http2) Received SETTINGS_HEADER_TABLE_SIZE with "
                     "value: %d\n",
                     (int)svalue.value);
        hencoder_.set_max_table_size(svalue.value);
    }
    else if (svalue.id == settings_id::enable_push) {
        http2_printf("(http2) Received SETTINGS_ENABLE_PUSH with value: %d !\n",
                     (int)svalue.value);
        if (svalue.value != 0) {
            http2_printf("(http2) Received SETTINGS_ENABLE_PUSH "
                         "with value: %d other "
                         "than zero !\n",
                         (int)svalue.value);
            ec = make_error(error::protocol_error);
            return;
        }
    }
    else if (svalue.id == settings_id::max_concurrent_streams) {
        http2_printf("(http2) Received SETTINGS_MAX_CONCURRENT_STREAMS "
                     "with value: %d\n",
                     (int)svalue.value);
        const bool increased = svalue.value > peer_max_concurrent_streams_;
        peer_max_concurrent_streams_ = svalue.value;
        if (increased) {
            // wake the writer if it is waiting on write_event_
            write_event_.set();
        }
    }
    else if (svalue.id == settings_id::initial_window_size) {
        if (svalue.value > max_flow_control_window_size) {
            http2_printf("(http2) Received SETTINGS_INITIAL_WINDOW_SIZE with"
                         " value above the maximum flow-control window "
                         "size: %d !\n",
                         (int)svalue.value);
            ec = make_error(error::flow_control_error);
            return;
        }
        http2_printf("(http2) Received SETTINGS_INITIAL_WINDOW_SIZE "
                     "with value: %d\n",
                     (int)svalue.value);
    }
    else if (svalue.id == settings_id::max_frame_size) {
        if (svalue.value < min_payload_size ||
            svalue.value > max_payload_size) {
            http2_printf("(http2) Received SETTINGS_MAX_FRAME_SIZE "
                         "with invalid value: %d !\n",
                         (int)svalue.value);
            ec = make_error(error::protocol_error);
            return;
        }
        http2_printf(
            "(http2) Received SETTINGS_MAX_FRAME_SIZE with value: %d\n",
            (int)svalue.value);
        peer_max_payload_size_ = svalue.value;
    }
    else if (svalue.id == settings_id::max_header_list_size) {
        http2_printf("(http2) Received SETTINGS_MAX_HEADER_LIST_SIZE "
                     "with value: %d\n",
                     (int)svalue.value);
        peer_max_header_list_size_ = svalue.value;
    }
    else {
        http2_printf("(http2) Received unknown setting id: %d, with "
                     "value: %d !\n",
                     (int)svalue.id, (int)svalue.value);
    }
}

bool client::read_padding_length(parser_frame_payload_stage& payload,
                                 std::error_code& ec, bool is_data) noexcept {
    constexpr uint8_t PADDED_FLAG = 0x8;
    if ((last_header_->flags & PADDED_FLAG) != PADDED_FLAG ||
        payload.consumed_size >= 1) {
        return true;
    }
    if (rbuf_.empty()) {
        return false;
    }
    rbuf_.write_to(buffer(&payload.padding, 1));
    payload.consumed_size += 1;
    if (payload.padding >= payload.total_size) {
        http2_printf("(http2) %s frame on stream (%d) has padding size "
                     "more than "
                     "total size (%d > %d) !\n",
                     is_data ? "DATA" : "HEADERS", (int)last_header_->stream_id,
                     (int)payload.padding, (int)payload.total_size);
        ec = make_error(error::protocol_error);
        return false;
    }
    return true;
}

bool client::read_priority(parser_frame_payload_stage& payload) noexcept {
    constexpr uint8_t PADDED_FLAG = 0x8;
    constexpr uint8_t PRIORITY_FLAG = 0x20;
    const bool is_padded = (last_header_->flags & PADDED_FLAG) == PADDED_FLAG;
    const size_t priority_consumed_size = 5 + (is_padded ? 1 : 0);
    if ((last_header_->flags & PRIORITY_FLAG) != PRIORITY_FLAG ||
        payload.consumed_size >= priority_consumed_size) {
        return true;
    }
    if (rbuf_.size() < 5) {
        return false;
    }
    rbuf_.consume(5);
    payload.consumed_size += 5;
    return true;
}

bool client::read_data_content(parser_frame_payload_stage& payload,
                               dynamic_buffer sink, bool is_data) {
    constexpr uint8_t PADDED_FLAG = 0x8;
    constexpr uint8_t PRIORITY_FLAG = 0x20;
    const bool is_padded = (last_header_->flags & PADDED_FLAG) == PADDED_FLAG;
    const bool is_priority =
        (last_header_->flags & PRIORITY_FLAG) == PRIORITY_FLAG;
    // exclude the consumed padding length byte and padding buffer and
    // priority id and weight from data
    const size_t data_len = payload.total_size -
                            (is_padded ? 1 + payload.padding : 0) -
                            (is_priority ? 5 : 0);
    // exclude the consumed padding length byte and priority id and weight
    // from consumed
    size_t consumed_data =
        payload.consumed_size - (is_padded ? 1 : 0) - (is_priority ? 5 : 0);
    assert(consumed_data <= data_len);
    if (consumed_data == data_len) {
        return true;
    }
    if (rbuf_.empty()) {
        return false;
    }
    const size_t to_consume_n = rbuf_.write_to(sink, data_len - consumed_data);
    consumed_data += to_consume_n;
    payload.consumed_size += to_consume_n;
    http2_printf("(http2) Consumed %d bytes of %s frame on stream (%d) total "
                 "consumed (%d)\n",
                 (int)to_consume_n, is_data ? "DATA" : "HEADERS",
                 (int)last_header_->stream_id, (int)payload.consumed_size);
    return consumed_data == data_len;
}

bool client::read_padding_content(parser_frame_payload_stage& payload,
                                  std::error_code& ec, bool is_data) {
    constexpr uint8_t PADDED_FLAG = 0x8;
    constexpr uint8_t PRIORITY_FLAG = 0x20;
    const bool is_padded = (last_header_->flags & PADDED_FLAG) == PADDED_FLAG;
    const bool is_priority =
        (last_header_->flags & PRIORITY_FLAG) == PRIORITY_FLAG;
    // exclude the consumed padding length byte and padding buffer and
    // priority id and weight from data
    const size_t data_len = payload.total_size -
                            (is_padded ? 1 + payload.padding : 0) -
                            (is_priority ? 5 : 0);
    // exclude the consumed padding length byte and priority id and weight
    // from consumed
    const size_t consumed_data =
        payload.consumed_size - (is_padded ? 1 : 0) - (is_priority ? 5 : 0);
    // all data must be consumed first
    if (consumed_data < data_len) {
        return false;
    }
    // if there is no padding this frame is finished
    if (!is_padded || payload.padding == 0) {
        assert(payload.consumed_size == payload.total_size);
        return true;
    }
    if (rbuf_.empty()) {
        return false;
    }
    const size_t to_consume_n =
        std::min(payload.total_size - payload.consumed_size, rbuf_.size());
    if (to_consume_n > payload.padding) {
        ec = make_error(error::protocol_error);
        return false;
    }
    std::array<uint8_t, 256> padding_buff;
    rbuf_.write_to(buffer(padding_buff, to_consume_n));
    for (size_t i = 0; i < to_consume_n; ++i) {
        if (padding_buff[i] != 0) {
            http2_printf("(http2) %s frame on stream (%d) has non "
                         "zero padding bytes !\n",
                         is_data ? "DATA" : "HEADERS",
                         (int)last_header_->stream_id);
            ec = make_error(error::protocol_error);
            return false;
        }
    }
    payload.consumed_size += to_consume_n;
    return payload.consumed_size == payload.total_size;
}

void client::process_data_frame_payload(parser_frame_payload_stage& payload,
                                        std::error_code& ec) {
    // rbuf_ may be empty for 0 length DATA frames

    auto it = open_streams_.find(
        [this](const stream& s) { return s.id == last_header_->stream_id; });
    if (it == open_streams_.end()) {
        size_t consume_n =
            std::min(payload.total_size - payload.consumed_size, rbuf_.size());
        rbuf_.consume(consume_n);
        http2_printf("(http2) Discarded %d bytes of data frame on stream (%d) "
                     "(closed) total consumed (%d)\n",
                     (int)consume_n, (int)last_header_->stream_id,
                     (int)payload.consumed_size);
        payload.consumed_size += consume_n;
        if (payload.consumed_size == payload.total_size) {
            parser_stage_ = parser_frame_header_stage{};
        }
        return;
    }

    bool res = read_padding_length(payload, ec, true);
    if (!res || ec) {
        return;
    }
    res = read_data_content(payload, it->res_buff, true);
    if (!res) {
        return;
    }
    res = read_padding_content(payload, ec, true);
    if (!res || ec) {
        return;
    }

    on_stream_received_data(*it, last_header_->flags);
    parser_stage_ = parser_frame_header_stage{};
}

bool client::handle_headers_block(stream& s, std::error_code& ec) noexcept {
    const bool is_end_stream =
        (last_header_->flags & END_STREAM_FLAG) == END_STREAM_FLAG;
    headers trailers_headers;
    if (s.received_trailers) {
        hdecoder_.decode(buffer(hpack_read_buff_), trailers_headers, ec);
    }
    else {
        hdecoder_.decode(buffer(hpack_read_buff_), s.res, ec);
    }
    if (ec) {
        http2_printf("(http2) HPACK decoder failed !\n");
        ec = make_error(error::compression_error);
        return false;
    }
    const headers& decoded_headers =
        s.received_trailers ? trailers_headers : s.res.headers;
    if (!validate_headers(decoded_headers)) {
        http2_printf("(http2) Received malformed response on stream (%d)!\n",
                     (int)last_header_->stream_id);
        close_open_stream(s, make_error(error::protocol_error), true);
        return false;
    }
    if (get_header_list_size(decoded_headers) > self_max_header_list_size_) {
        http2_printf("(http2) Received response headers on stream (%d) "
                     "with size "
                     "greater than (%d)!\n",
                     (int)last_header_->stream_id,
                     (int)self_max_header_list_size_);
        close_open_stream(s, make_error(error::protocol_error), true);
        return false;
    }
    if (s.received_trailers) {
        // there is no response to check, it was only headers
        http2_printf("(http2) Received %d trailers headers on stream (%d)\n",
                     (int)trailers_headers.size(),
                     (int)last_header_->stream_id);
        return true;
    }
    if (s.res.is_informational()) {
        if (s.res.status_code() == http::response_status::switching_protocols) {
            // http2 does not support 101 switching protocols
            http2_printf("(http2) Received 101 Switching Protocols "
                         "on stream (%d)!\n",
                         (int)last_header_->stream_id);
            close_open_stream(s, make_error(error::protocol_error), true);
            return false;
        }
        if (s.received_final_res) {
            http2_printf("(http2) Received informational response "
                         "after final "
                         "response on stream (%d)!\n",
                         (int)last_header_->stream_id);
            close_open_stream(s, make_error(error::protocol_error), true);
            return false;
        }
        if (is_end_stream) {
            http2_printf("(http2) Received informational response "
                         "which ends stream (%d)!\n",
                         (int)last_header_->stream_id);
            close_open_stream(s, make_error(error::protocol_error), true);
            return false;
        }
        // wait again for the final response
        s.wants_headers = true;
        s.res.clear();
    }
    else {
        s.received_final_res = true;
    }
    return true;
}

void client::process_headers_frame_payload(parser_frame_payload_stage& payload,
                                           std::error_code& ec) {
    // rbuf_ may be empty for 0 length HEADERS frames
    auto it = open_streams_.find(
        [this](const stream& s) { return s.id == last_header_->stream_id; });
    if (it == open_streams_.end()) {
        size_t consume_n =
            std::min(payload.total_size - payload.consumed_size, rbuf_.size());
        rbuf_.consume(consume_n);
        payload.consumed_size += consume_n;
        if (payload.consumed_size == payload.total_size) {
            parser_stage_ = parser_frame_header_stage{};
        }
        return;
    }

    bool res = read_padding_length(payload, ec, false);
    if (!res || ec) {
        return;
    }
    res = read_priority(payload);
    if (!res) {
        return;
    }
    res = read_data_content(payload, dynamic_buffer(hpack_read_buff_), false);
    if (!res) {
        return;
    }
    res = read_padding_content(payload, ec, false);
    if (!res || ec) {
        return;
    }

    if ((last_header_->flags & END_HEADERS_FLAG) == END_HEADERS_FLAG) {
        if (!handle_headers_block(*it, ec)) {
            assert(ec);
            return;
        }
    }

    on_stream_received_headers(*it, last_header_->flags);
    parser_stage_ = parser_frame_header_stage{};
}

void client::process_continuation_frame_payload(
    parser_frame_payload_stage& payload, std::error_code& ec) {
    // rbuf_ may be empty for 0 length CONTINUATION frames

    auto it = open_streams_.find(
        [this](const stream& s) { return s.id == last_header_->stream_id; });
    if (it == open_streams_.end()) {
        size_t consume_n =
            std::min(payload.total_size - payload.consumed_size, rbuf_.size());
        rbuf_.consume(consume_n);
        payload.consumed_size += consume_n;
        if (payload.consumed_size == payload.total_size) {
            parser_stage_ = parser_frame_header_stage{};
        }
        return;
    }

    const bool is_end_headers =
        (last_header_->flags & END_HEADERS_FLAG) == END_HEADERS_FLAG;
    size_t consume_n =
        std::min(payload.total_size - payload.consumed_size, rbuf_.size());
    auto res_buff = dynamic_buffer(hpack_read_buff_).prepare(consume_n);
    rbuf_.write_to(res_buff);
    payload.consumed_size += consume_n;
    if (payload.consumed_size == payload.total_size && is_end_headers) {
        if (!handle_headers_block(*it, ec)) {
            assert(ec);
            return;
        }
    }

    if (payload.consumed_size == payload.total_size) {
        on_stream_received_headers(*it, last_header_->flags);
        parser_stage_ = parser_frame_header_stage{};
        return;
    }
}

void client::process_priority_frame_payload(
    parser_frame_payload_stage& payload) {
    if (rbuf_.empty()) {
        return;
    }
    assert(payload.consumed_size < payload.total_size);
    const size_t to_consume_n =
        std::min(payload.total_size - payload.consumed_size, rbuf_.size());
    rbuf_.consume(to_consume_n);
    payload.consumed_size += to_consume_n;
    if (payload.consumed_size == payload.total_size) {
        http2_printf("(http2) Received PRIORITY frame on stream (%d)\n",
                     (int)last_header_->stream_id);
        parser_stage_ = parser_frame_header_stage{};
    }
}

void client::process_ping_frame_payload(parser_frame_payload_stage& payload) {
    if (rbuf_.size() < payload.total_size) {
        rbuf_exhausted_ = true;
        http2_printf("(http2) Not enough available buffers to read PING frame, "
                     "current size (%d) min (%d) !",
                     (int)rbuf_.size(), (int)payload.total_size);
        return;
    }
    beu64 bopaque_data;
    rbuf_.write_to(buffer(&bopaque_data, sizeof(beu64)));
    uint64_t opaque_data = bopaque_data;
    const bool is_ack = (last_header_->flags & PING_ACK_FLAG) == PING_ACK_FLAG;
    http2_printf("(http2) Received PING opaque data(%" PRIu64 "), ACK(%s)\n",
                 opaque_data, is_ack ? "true" : "false");
    parser_stage_ = parser_frame_header_stage{};
    // respond to ping
    if (!is_ack) {
        pending_pings_.push_back(std::pair{opaque_data, true});
        // wake the writer if it is waiting on write_event_
        write_event_.set();
    }
    else {
        auto it = std::find(wanted_pings_acks_.begin(),
                            wanted_pings_acks_.end(), opaque_data);
        if (it != wanted_pings_acks_.end()) {
            wanted_pings_acks_.erase(it);
            closed_a_stream_ = true;
            // wake the writer if it is waiting on write_event_
            write_event_.set();
        }
    }
}

void client::process_rst_frame_payload(parser_frame_payload_stage& payload) {
    if (rbuf_.size() < payload.total_size) {
        rbuf_exhausted_ = true;
        http2_printf("(http2) Not enough available buffers to read RST_STREAM "
                     "frame, current size (%d) min (%d) !",
                     (int)rbuf_.size(), (int)payload.total_size);
        return;
    }
    beu32 berror_code;
    rbuf_.write_to(buffer(&berror_code, sizeof(beu32)));
    uint32_t error_code = berror_code;
    http2_printf("(http2) received RST_STREAM id (%d) error code (%d)\n",
                 (int)last_header_->stream_id, (int)error_code);
    parser_stage_ = parser_frame_header_stage{};
    auto it = open_streams_.find(
        [&](const stream& s) { return s.id == last_header_->stream_id; });
    if (it != open_streams_.end()) {
        close_open_stream(*it, make_error(static_cast<error>(error_code)),
                          false);
    }
}

void client::process_goaway_frame_payload(parser_frame_payload_stage& payload,
                                          std::error_code& ec) {
    if (rbuf_.size() < payload.total_size) {
        rbuf_exhausted_ = true;
        http2_printf("(http2) Not enough available buffers to read "
                     "GOAWAY frame, "
                     "current size (%d) min (%d) !",
                     (int)rbuf_.size(), (int)payload.total_size);
        return;
    }
    beu32 bid, berror_code;
    std::string debug_msg;
    rbuf_.write_to(buffer(&bid, sizeof(beu32)));
    rbuf_.write_to(buffer(&berror_code, sizeof(beu32)));
    payload.consumed_size += sizeof(uint32_t) * 2;
    if (payload.consumed_size < payload.total_size) {
        debug_msg.resize(payload.total_size - payload.consumed_size);
        rbuf_.write_to(buffer(debug_msg));
        payload.consumed_size = payload.total_size;
    }
    const uint32_t id = bid, error_code = berror_code;
    std::ignore = id;
    std::ignore = error_code;
    http2_printf(
        "(http2) received GOAWAY last id (%d) error code (%d) msg(%s)\n",
        (int)id, (int)error_code, debug_msg.c_str());
    parser_stage_ = parser_frame_header_stage{};
    goaway_reason_ = std::move(debug_msg);
    ec = make_error(static_cast<error>(error_code));
    received_goaway_ = true;
    closed_a_stream_ = true;
}

void client::process_window_update_frame_payload(
    parser_frame_payload_stage& payload, std::error_code& ec) {
    if (rbuf_.size() < payload.total_size) {
        rbuf_exhausted_ = true;
        http2_printf("(http2) Not enough available buffers to read "
                     "WINDOW_UPDATE "
                     "frame, current size (%d) min (%d) !",
                     (int)rbuf_.size(), (int)payload.total_size);
        return;
    }
    beu32 bsize;
    rbuf_.write_to(buffer(&bsize, sizeof(beu32)));
    uint32_t window_size = bsize;
    bits::clear<31>(window_size);
    if (window_size == 0) {
        http2_printf("received WINDOW_UPDATE with zero window "
                     "increment on stream (%d) !\n",
                     (int)last_header_->stream_id);
        if (last_header_->stream_id != 0) {
            auto it = open_streams_.find([this](const stream& s) {
                return s.id == last_header_->stream_id;
            });
            if (it != open_streams_.end()) {
                close_open_stream(*it, make_error(error::protocol_error), true);
            }
        }
        else {
            ec = make_error(error::protocol_error);
            return;
        }
    }
    http2_printf("(http2) received WINDOW_UPDATE on stream (%d) with window "
                 "increment (%d)\n",
                 (int)last_header_->stream_id, (int)window_size);
    parser_stage_ = parser_frame_header_stage{};
}
