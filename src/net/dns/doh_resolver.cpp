#include <rad/match.h>
#include <rad/net/dns/doh_resolver.h>
#include <rad/net/http/http_parser.h>
#include <rad/net/socket_options.h>
#include <rad/system_error.h>

using namespace RAD_LIB_NAMESPACE;
using namespace net;
using namespace dns;
using namespace doh;

namespace {
    std::error_code make_unreachable_host_error() {
#ifdef _WIN32
        constexpr int host_not_found_code = 11001;
        const auto& cat = system_category();
#else
        constexpr int host_not_found_code =
            static_cast<int>(std::errc::host_unreachable);
        const auto& cat = std::generic_category();
#endif // _WIN32
        return std::error_code{host_not_found_code, cat};
    }
} // namespace

namespace RAD_LIB_NAMESPACE::net::doh {
    dns_config config;
}

void doh::resolver::reset() {
    retries_ = 0;
    results_.clear();
    cname_.clear();
    query_message_.clear();
    response_buff_.clear();
    response_rbuf_ = {};
    response_.clear();
    chunked_body_.clear();
    requested_name_.clear();
    results_ec_.clear();
}

void doh::resolver::make_query(std::string_view name, std::error_code& ec) {
    std::vector<uint8_t> query_payload;
    question_offset_ =
        make_dns_query(dynamic_buffer(query_payload), name, want_ipv4(), 0, ec);
    if (ec) {
        return;
    }

    std::string len_str = std::to_string(query_payload.size());
    http::request_view req;
    req.method = http::verb::post;
    req.version = http::version::v1_1;
    req.target = current_server_->path;
    req.headers.reserve(3);
    req.headers.insert(http::field::host, current_server_->host);
    req.headers.insert(http::field::accept, "application/dns-message");
    req.headers.insert(http::field::content_type, "application/dns-message");
    req.headers.insert(http::field::content_length, len_str);

    query_message_.clear();
    req.serialize(dynamic_buffer(query_message_));
    question_offset_ += static_cast<uint16_t>(query_message_.size());
    query_message_.insert(query_message_.end(), query_payload.begin(),
                          query_payload.end());

    requested_name_ = name;
}

bool doh::resolver::start_coro(dns::detail::handler_base& handler,
                               std::vector<endpoint>& cache_results) {
    if (config.enable_cache &&
        config.caches.find(requested_name_, requested_family_, requested_port_,
                           cache_results)) {
        return true;
    }
    results_handler_ = &handler;
    current_server_ = config.select_a_server();
    start_connection();
    return false;
}

void doh::resolver::try_fallback_server() {
    std::error_code ec;
    current_server_ = config.try_another_server(current_server_);
    make_query(requested_name_, ec);
    if (ec) {
        return notify_failure(ec);
    }
    return start_connection();
}

void doh::resolver::notify_success() {
    results_ec_.clear();
    current_server_->broken = false;

    auto handler = std::exchange(results_handler_, nullptr);
    handler->invoke_resolver(std::error_code{}, std::move(results_),
                             std::move(cname_));
}

void doh::resolver::notify_failure(std::error_code ec) {
    if (!ec) {
        ec = make_unreachable_host_error();
    }
    results_ec_ = ec;
    is_connected_ = false;
    if (!results_.empty()) {
        return notify_success();
    }
    current_server_->broken = true;
    retries_ += 1;
    if (retries_ < config.servers.size()) {
        return try_fallback_server();
    }
    auto handler = std::exchange(results_handler_, nullptr);
    handler->invoke_resolver(ec, std::move(results_), std::move(cname_));
}

void doh::resolver::fire_timeout_timer() {
    timer_state_->assign(false);
    timeout_timer_.expires_after(config.timeout.load());
    timeout_timer_.async_wait(
        [this, state = timer_state_](const std::error_code& ec) {
            auto [lock, stop_flag] = state->lock_guard();
            if (!ec && !stop_flag) {
                transport_.next_layer().cancel();
            }
        });
}

void doh::resolver::cancel_timeout_timer() noexcept {
    auto [lock, stop_flag] = timer_state_->lock_guard();
    stop_flag = true;
    timeout_timer_.cancel();
}

void doh::resolver::start_connection() {
    if (is_connected_ && last_server_ == current_server_) {
        using_existing_connection_ = true;
        return send_http_request();
    }

    std::error_code ec;
    transport_.reopen(tcp{current_server_->address.family()}, ec);
    if (ec) {
        return notify_failure(ec);
    }
    transport_.next_layer().set_option(socket_options::tcp_nodelay{true}, ec);
    if (ec) {
        return notify_failure(ec);
    }

    transport_.next_layer().async_connect(
        current_server_->address, [this](const std::error_code& ec) {
            if (ec) {
                return notify_failure(ec);
            }
            fire_timeout_timer();
            transport_.set_hostname(current_server_->host);
            transport_.async_handshake(ssl::handshake_type::client,
                                       [this](const std::error_code& ec) {
                                           cancel_timeout_timer();
                                           if (ec) {
                                               return notify_failure(ec);
                                           }
                                           is_connected_ = true;
                                           last_server_ = current_server_;
                                           send_http_request();
                                       });
        });
}

void doh::resolver::send_http_request() {
    transport_.async_write(
        buffer(query_message_),
        [this](const std::error_code& ec, std::size_t transferred) {
            if (ec) {
                bool was_using_existing_connection =
                    std::exchange(using_existing_connection_, false);
                if (was_using_existing_connection) {
                    is_connected_ = false;
                    return start_connection();
                }
                return notify_failure(ec);
            }
            read_http_response();
        });
}

void doh::resolver::read_http_response() {
    chunked_body_.clear();
    response_buff_.resize(config.max_reponse_size.load());
    response_rbuf_ = ring_consumer_producer{buffer(response_buff_)};
    consumed_response_size_ = 0;
    response_.clear();
    parser_.emplace<http::response_incremental_parser>(response_);
    fire_timeout_timer();
    transport_.async_read_some(
        buffer(response_buff_),
        [this](const std::error_code& ec, std::size_t transferred) {
            cancel_timeout_timer();
            continue_read_response(ec, transferred);
        });
}

void doh::resolver::continue_read_response(std::error_code ec,
                                           std::size_t transferred) {
    if (ec) {
        return notify_failure(ec);
    }
    // assert(transferred > 0);
    consumed_response_size_ += transferred;
    response_rbuf_.commit(transferred);
    auto& res_parser = get_response_parser();
    res_parser.parse(response_rbuf_, ec);
    if (ec) {
        return notify_failure(ec);
    }
    if (res_parser.done()) {
        return validate_http_response();
    }
    assert(!res_parser.has_error());
    if (consumed_response_size_ >= response_rbuf_.capacity()) {
        ec = http::make_error(http::error::too_large_message);
        return notify_failure(ec);
    }
    fire_timeout_timer();
    transport_.async_read_some(
        response_rbuf_.available_space(),
        [this](const std::error_code& ec, std::size_t transferred) {
            cancel_timeout_timer();
            continue_read_response(ec, transferred);
        });
}

void doh::resolver::validate_http_response() {
    if (response_.is_informational()) {
        if (response_.status_code() ==
            http::response_status::switching_protocols) {
            return notify_failure(make_unreachable_host_error());
        }
        // don't clear the buffer since it may contain the next response
        consumed_response_size_ = 0;
        response_.clear();
        parser_.emplace<http::response_incremental_parser>(response_);
        return continue_read_response({}, 0);
    }
    if (response_.status != 200) {
        return notify_failure(make_unreachable_host_error());
    }
    auto body_size = http::determine_message_body_length(
        http::verb::post, response_.status_code(), response_.version,
        response_.get_content_length(), response_.get_transfer_encoding());

    match(
        body_size,
        [&](std::uint64_t len) {
            read_with_content_len(static_cast<std::size_t>(len));
        },
        [&](http::chunked_body) { read_chunked(); },
        [&](http::body_until_eof) { read_until_eof(); },
        [&](http::bad_message_body) {
            return notify_failure(make_unreachable_host_error());
        });
}

void doh::resolver::read_with_content_len(std::size_t length) {
    response_rbuf_.linearize();
    assert(response_rbuf_.is_linearized());
    if (length < response_rbuf_.size()) {
        // The server sent more data after the body!
        // treat this an error because it violates HTTP
        return notify_failure(http::make_error(http::error::unexpected_body));
    }
    if (length == response_rbuf_.size()) {
        // all body was ready
        return process_answer(response_rbuf_.available_buffers()[0]);
    }
    if (length > response_rbuf_.space()) {
        return notify_failure(http::make_error(http::error::too_large_message));
    }
    response_rbuf_.move_to_start();
    length -= response_rbuf_.size();
    fire_timeout_timer();
    transport_.async_read(
        response_rbuf_.available_space(length),
        [this](const std::error_code& ec, std::size_t transferred) {
            cancel_timeout_timer();
            after_read_with_content_len(ec, transferred);
        });
}

void doh::resolver::after_read_with_content_len(std::error_code ec,
                                                std::size_t transferred) {
    if (ec) {
        return notify_failure(ec);
    }
    response_rbuf_.commit(transferred);
    assert(response_rbuf_.is_linearized());
    assert(!response_rbuf_.empty());
    process_answer(response_rbuf_.available_buffers()[0]);
}

void doh::resolver::read_chunked() {
    consumed_response_size_ = 0;
    parser_.emplace<http::chunks_incremental_parser>();
    if (!response_rbuf_.empty()) {
        auto& p = get_chunks_parser();
        std::error_code ec;
        consumed_response_size_ +=
            p.parse(response_rbuf_, dynamic_buffer(chunked_body_), nullptr,
                    nullptr, ec);
        if (ec) {
            return notify_failure(ec);
        }
        assert(!p.has_error());
        if (p.done()) {
            return process_answer(buffer(chunked_body_));
        }
        if (consumed_response_size_ >= response_buff_.capacity()) {
            return notify_failure(
                http::make_error(http::error::too_large_message));
        }
    }
    fire_timeout_timer();
    transport_.async_read_some(
        response_rbuf_.available_space(),
        [this](const std::error_code& ec, std::size_t transferred) {
            cancel_timeout_timer();
            continue_read_chunked(ec, transferred);
        });
}

void doh::resolver::continue_read_chunked(std::error_code ec,
                                          std::size_t transferred) {
    if (ec) {
        return notify_failure(ec);
    }
    assert(transferred > 0);
    response_rbuf_.commit(transferred);
    auto& p = get_chunks_parser();
    consumed_response_size_ += p.parse(
        response_rbuf_, dynamic_buffer(chunked_body_), nullptr, nullptr, ec);
    if (ec) {
        return notify_failure(ec);
    }
    assert(!p.has_error());
    if (p.done()) {
        return process_answer(buffer(chunked_body_));
    }
    if (consumed_response_size_ >= response_buff_.capacity()) {
        return notify_failure(http::make_error(http::error::too_large_message));
    }

    fire_timeout_timer();
    transport_.async_read_some(
        response_rbuf_.available_space(),
        [this](const std::error_code& ec, std::size_t transferred) {
            cancel_timeout_timer();
            continue_read_chunked(ec, transferred);
        });
}

void doh::resolver::read_until_eof() {
    response_rbuf_.linearize();
    response_rbuf_.move_to_start();
    continue_read_until_eof(std::error_code{}, 0);
}

void doh::resolver::continue_read_until_eof(const std::error_code& ec,
                                            std::size_t transferred) {
    response_rbuf_.commit(transferred);
    const auto eof_ec = io::detail::make_eof_error_code();
    if (ec == eof_ec) {
        assert(transferred == 0);
        assert(response_rbuf_.is_linearized());
        assert(!response_rbuf_.empty());
        return process_answer(response_rbuf_.available_buffers()[0]);
    }
    if (ec) {
        return notify_failure(ec);
    }
    // assert(transferred > 0);
    if (response_rbuf_.full()) {
        return notify_failure(http::make_error(http::error::too_large_message));
    }
    fire_timeout_timer();
    transport_.async_read_some(
        response_rbuf_.available_space(),
        [this](const std::error_code& ec, std::size_t transferred) {
            cancel_timeout_timer();
            continue_read_until_eof(ec, transferred);
        });
}

void doh::resolver::process_answer(const_buffer buff) {
    if (buff.size() < sizeof(dns_header_raw)) {
        return notify_failure(make_unreachable_host_error());
    }
    auto header = buff.data_as<const dns_header_raw>();
    if (header->response_code() != response_code::no_error ||
        header->questions_count() != 1 || header->answers_count() == 0 ||
        !header->response() || header->id() != 0) {
        return notify_failure(make_unreachable_host_error());
    }

    // can't tolerate truncation on tcp
    if (header->truncation()) {
        return notify_failure(make_unreachable_host_error());
    }

    cache_storage* cache_ptr = config.enable_cache ? &config.caches : nullptr;
    dns_ip_answers_handler answers_handler{requested_name_, results_,
                                           requested_port_, cache_ptr};
    std::error_code ec;
    parse_dns_message(buff, answers_handler, ec);
    if (ec) {
        return notify_failure(make_unreachable_host_error());
    }
    cname_ = answers_handler.take_canonicnal_name();

    if (still_ipv6()) {
        requested_family_ = address_family::ipv6;
        auto question = reinterpret_cast<question_section_raw*>(
            &query_message_[question_offset_]);
        question->set_ipv6();
        return send_http_request();
    }

    notify_success();
}
