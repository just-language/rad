#include <rad/net/dns/ares_resolver.h>
#include <rad/random.h> // to generate random id
#include <rad/system_error.h>

using namespace RAD_LIB_NAMESPACE;
using namespace net;
using namespace dns;
using namespace ares;

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

namespace RAD_LIB_NAMESPACE::net::ares {
    resolver_config config;
}

bool ares::resolver::start_coro(dns::detail::handler_base& h,
                                std::vector<endpoint>& cache_results) {
    if (config.enable_cache &&
        config.caches.find(requested_name_, requested_family_, requested_port_,
                           cache_results)) {
        return true;
    }

    assert(handler_ == nullptr);

    handler_ = &h;
    current_server = config.select_a_server();
    if (config.always_tcp) {
        start_tcp_connection();
    }
    else {
        send_udp_packet();
    }

    return false;
}

bool ares::resolver::open_udp_socket() {
    if (udp_sock_.is_open() &&
        last_sock_family == current_server->address.family()) {
        return true;
    }

    std::error_code ec;
    udp_sock_.open(current_server->address.family(), ec);
    if (!ec) {
        last_sock_family = current_server->address.family();
        return true;
    }
    else {
        return false;
    }
}

bool ares::resolver::open_tcp_socket() {
    std::error_code ec;
    tcp_sock_.open(tcp{current_server->address.family()}, ec);
    if (!ec) {
        tcp_sock_.set_option(socket_options::tcp_nodelay{true}, ec);
        ec.clear();
    }
    return !ec;
}

void ares::resolver::try_fallback_server() {
    current_server = config.try_another_server(current_server);
    if (using_tcp_) {
        start_tcp_connection();
    }
    else {
        send_udp_packet();
    }
}

void ares::resolver::switch_to_tcp() {
    if (config.no_tcp) {
        return notify_failure(make_unreachable_host_error());
    }
    using_tcp_ = true;
    current_server = config.select_a_server();
    start_tcp_connection();
}

void ares::resolver::make_dns_query(std::string_view name,
                                    std::error_code& ec) {
    auto insert_into_storage = [this](const auto& value) {
        auto first = reinterpret_cast<const uint8_t*>(&value);
        auto last = first + sizeof(value);
        query_storage_.insert(query_storage_.end(), first, last);
    };

    query_storage_.clear();
    // reserve space for tcp length word
    query_storage_.push_back('0');
    query_storage_.push_back('0');
    // insert the dns header
    dns_header_raw header{};
    query_id_ = static_cast<uint16_t>(
        random_uint(std::numeric_limits<uint16_t>::max(),
                    std::numeric_limits<uint32_t>::max()));
    header.id(query_id_);
    header.query(true);
    header.opcode(query_opcode::standard);
    header.recursion_desired(true);
    header.questions_count(1);
    if (config.use_edns) {
        header.additional_records_count(1);
    }

    insert_into_storage(header);
    // insert QNAME
    encode_name(dynamic_buffer(query_storage_), name, ec);
    if (ec) {
        return;
    }
    // insert the fixed question section
    question_section_raw qsection;
    qsection.qclass = static_cast<uint16_t>(dns_class::internet);
    if (want_ipv4()) {
        qsection.set_ipv4();
    }
    else {
        qsection.set_ipv6();
    }
    // store the question offset to fast switch from ipv4 to ipv6
    question_offset_ = static_cast<uint16_t>(query_storage_.size());
    insert_into_storage(qsection);

    // the edns header will be sent only if edns is enabled in both config
    // and server
    query_storage_.push_back(0); // root domain
    resource_record_raw edns_header;
    edns_header.type(query_type::opt);
    edns_header.record_class(
        static_cast<dns_class>(config.max_udp_size.load()));
    edns_header.ttl({});
    edns_header.data_length(0);

    insert_into_storage(edns_header);

    // first two bytes are the tcp length and used only for tcp
    // transmission, edns is used only over udp
    uint16_t tcp_size = static_cast<uint16_t>(
        query_storage_.size() - sizeof(uint16_t) - edns_header_size);
    // store the tcp length word in the start of the storage
    *reinterpret_cast<beu16*>(query_storage_.data()) = tcp_size;

    requested_name_ = name;
}

void ares::resolver::notify_failure(const std::error_code& ec) {
    // if at least one endpoint was retrieved then it a success
    if (!results_.empty()) {
        return notify_success();
    }
    // if udp is being used and tcp is allowed, try again with tcp
    else if (!using_tcp_ && !config.no_tcp) {
        return switch_to_tcp();
    }
    // if there is still another server to retry, switch to it
    retries_ += 1;
    if (retries_ < config.servers.size()) {
        return try_fallback_server();
    }

    // invoke the handler directly
    auto handler2 = std::exchange(handler_, nullptr);
    handler2->invoke_resolver(ec, std::vector<endpoint>{}, std::string{});
}

void ares::resolver::notify_success() {
    // store the error code in case of strand is used and mark the current
    // server as not broken
    results_ec_.clear();
    current_server->broken = false;

    if (results_.empty()) {
        return notify_failure(make_unreachable_host_error());
    }

    // invoke the handler directly
    auto handler2 = std::exchange(handler_, nullptr);
    handler2->invoke_resolver(std::error_code{}, std::move(results_),
                              std::move(cname_));
}

const_buffer ares::resolver::udp_query_buffer() const noexcept {
    using namespace dns;

    // skip the tcp word length
    auto buff = buffer(query_storage_) + sizeof(uint16_t);

    auto& header = *buff.data_as<dns::dns_header_raw>();
    if (!current_server->supports_edns || !config.use_edns) {
        // if edns is turned off do not include it
        header.additional_records_count(0);
        buff -= edns_header_size;
    }
    else if (config.use_edns) {
        header.additional_records_count(1);
        request_edns().data_length(config.max_udp_size);
    }

    return buff;
}

const_buffer ares::resolver::tcp_query_buffer() const noexcept {
    // don't include the edns header as it is only used over udp
    request_header().additional_records_count(0);
    return buffer(query_storage_) - dns::edns_header_size;
}

void ares::resolver::fire_timeout_timer() {
    timer_state_->assign(false);

    timeout_timer_.expires_after(config.timeout.load());

    if (using_tcp_) {
        timeout_timer_.async_wait(
            [self = pointer<resolver>{this},
             state = timer_state_](const std::error_code& ec) {
                auto [lock, stop_flag] = state->lock_guard();
                if (ec || stop_flag) {
                    return;
                }
                self->tcp_sock_.cancel();
            });
    }
    else {
        timeout_timer_.async_wait(
            [self = pointer<resolver>{this},
             state = timer_state_](const std::error_code& ec) {
                auto [lock, stop_flag] = state->lock_guard();
                if (ec || stop_flag) {
                    return;
                }
                self->udp_sock_.cancel();
            });
    }
}

void ares::resolver::cancel_timeout_timer() noexcept {
    auto [lock, stop_flag] = timer_state_->lock_guard();
    stop_flag = true;
    timeout_timer_.cancel();
}

void ares::resolver::send_udp_packet() {
    using_tcp_ = false;

    if (!open_udp_socket()) {
        return notify_failure(make_unreachable_host_error());
    }

    udp_sock_.async_send_to(
        udp_query_buffer(), current_server->address,
        [this](const std::error_code& ec, std::size_t) {
            if (ec) {
                return notify_failure(ec);
            }
            read_udp_response();
        },
        static_buffer_allocator(io_alloc_buff_));
}

void ares::resolver::read_udp_response() {
    auto header = udp_query_buffer().data_as<const dns_header_raw>();
    bool is_edns = header->additional_records_count() == 1;

    if (is_edns) {
        read_buffer_.resize(config.max_udp_size);
    }
    else {
        read_buffer_.resize(max_udp_no_edns);
    }

    fire_timeout_timer();

    udp_sock_.async_receive_from(
        buffer(read_buffer_), from_addr_,
        [this](const std::error_code& ec, std::size_t transferred) {
            cancel_timeout_timer();
            if (ec) {
                return notify_failure(ec);
            }
            // process the response
            read_buffer_.resize(transferred);
            process_answer();
        },
        static_buffer_allocator(io_alloc_buff_));
}

void ares::resolver::start_tcp_connection() {
    if (!open_tcp_socket()) {
        return notify_failure(make_unreachable_host_error());
    }

    tcp_sock_.async_connect(
        current_server->address,
        [this](const std::error_code& ec) {
            if (ec) {
                return notify_failure(ec);
            }
            send_tcp_packet();
        },
        static_buffer_allocator(io_alloc_buff_));
}

void ares::resolver::send_tcp_packet() {
    tcp_sock_.async_write(
        tcp_query_buffer(),
        [this](const std::error_code& ec, std::size_t) {
            if (ec) {
                return notify_failure(ec);
            }
            read_tcp_length_word();
        },
        static_buffer_allocator(io_alloc_buff_));
}

void ares::resolver::read_tcp_length_word() {
    fire_timeout_timer();

    tcp_sock_.async_read(
        buffer(&tcp_length_, sizeof(tcp_length_)),
        [this](const std::error_code& ec, std::size_t) {
            cancel_timeout_timer();
            if (ec) {
                return notify_failure(ec);
            }
            read_tcp_data();
        },
        static_buffer_allocator(io_alloc_buff_));
}

void ares::resolver::read_tcp_data() {
    if (tcp_length_ > config.max_tcp_size) {
        return notify_failure(make_unreachable_host_error());
    }

    read_buffer_.resize(tcp_length_);

    fire_timeout_timer();

    tcp_sock_.async_read(
        buffer(read_buffer_),
        [this](const std::error_code& ec, std::size_t) {
            cancel_timeout_timer();
            if (ec) {
                return notify_failure(ec);
            }
            process_answer();
        },
        static_buffer_allocator(io_alloc_buff_));
}

void ares::resolver::process_answer() {
    auto buff = buffer(read_buffer_);
    if (buff.size() < sizeof(dns_header_raw)) {
        return notify_failure(make_unreachable_host_error());
    }
    auto header = buff.data_as<const dns_header_raw>();
    // if the server doesn't support edns, restart without edns (edns is
    // only used with udp)
    if (edns_was_used()) {
        auto rcode = header->response_code();
        if (rcode != response_code::no_error &&
            (rcode == response_code::not_implemented ||
             rcode == response_code::format_error ||
             rcode == response_code::server_failure)) {
            current_server->supports_edns = false;
            return send_udp_packet();
        }
    }

    if (header->response_code() != response_code::no_error ||
        header->questions_count() != 1 || header->answers_count() == 0 ||
        !header->response() || header->id() != query_id_) {
        return notify_failure(make_unreachable_host_error());
    }

    // if the response is truncated and udp is used and truncation isn't
    // ignored switch to tcp if allowed
    if (header->truncation() && !config.ignore_truncation && !config.no_tcp &&
        !using_tcp_) {
        return notify_failure(make_unreachable_host_error());
    }

    cache_storage* cache_ptr = config.enable_cache ? &config.caches : nullptr;
    dns_ip_answers_handler answers_handler{requested_name_, results_,
                                           requested_port_, cache_ptr};
    std::error_code ec;
    parse_dns_message(buffer(read_buffer_), answers_handler, ec);
    if (ec) {
        return notify_failure(make_unreachable_host_error());
    }
    cname_ = answers_handler.take_canonicnal_name();

    if (still_ipv6()) {
        requested_family_ = address_family::ipv6;
        request_question().set_ipv6();
        if (using_tcp_) {
            return start_tcp_connection();
        }
        else {
            return send_udp_packet();
        }
    }

    notify_success();
}