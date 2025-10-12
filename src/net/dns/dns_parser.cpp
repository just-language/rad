#include <rad/net/dns/dns_parser.h>
#include <rad/net/idna/idna.h>
#ifdef _WIN32
#include <WinSock2.h>
#elif __unix__
#include <netdb.h>
#endif // _WIN32

namespace {
#ifdef _WIN32
    constexpr DWORD host_not_found_code = 11001L;
#elif __unix__
    constexpr int host_not_found_code =
        static_cast<int>(std::errc::host_unreachable);
#endif // _WIN32
} // namespace

using namespace RAD_LIB_NAMESPACE;
using namespace net;
using namespace dns;

namespace {
    struct name_reader {
    public:
        name_reader(const_buffer buff) : buff(buff) {
        }

        uint8_t read_byte() {
            uint8_t b = *buff.data_as<const uint8_t>();
            ++consumed;
            buff += 1;
            return b;
        }

        bool discard(uint8_t n) {
            if (buff.size() <= n) {
                return false;
            }
            consumed += n;
            buff += n;
            return true;
        }

        bool get(uint8_t n, char* ptr) {
            if (buff.size() <= n) {
                return false;
            }
            memcpy(ptr, buff.data(), n);
            consumed += n;
            buff += n;
            return true;
        }

        std::size_t total_consumed() {
            return consumed;
        }

        bool has_data() {
            return !buff.empty();
        }

        uint16_t current_pos(const_buffer whole_message) const noexcept {
            assert(whole_message.size() <=
                   std::numeric_limits<std::uint16_t>::max());
            return static_cast<uint16_t>(
                buff.data_as<const uint8_t>() -
                whole_message.data_as<const uint8_t>());
        }

    private:
        const_buffer buff;
        uint32_t consumed = 0;
    };

    constexpr uint8_t offset_mask = 0b11000000;

    uint16_t extract_offset(uint8_t byte1, uint8_t byte2) {
        byte1 &= ~offset_mask;
        uint16_t offset = byte2;
        offset |= uint16_t{byte1} << 8;
        return offset;
    }

    enum class rdata_type {
        domain_name,
        chars_strings,
        two_chars_strings,
        two_domain_names,
        i16_domain_name,
        opaque,
        ipv4,
        ipv6,
    };

    rdata_type get_rdata_type(query_type q) {
        switch (q) {
        case query_type::invalid:
            return rdata_type::opaque;
        case query_type::ipv4:
            return rdata_type::ipv4;
        case query_type::ipv6:
            return rdata_type::ipv6;
        case query_type::name_server:
            return rdata_type::domain_name;
        case query_type::mail_destination:
            return rdata_type::domain_name;
        case query_type::mail_forwarder:
            return rdata_type::domain_name;
        case query_type::canonical_name:
            return rdata_type::domain_name;
        case query_type::start_of_authority:
            return rdata_type::opaque;
        case query_type::mailbox:
            return rdata_type::domain_name;
        case query_type::mail_group:
            return rdata_type::domain_name;
        case query_type::mail_rename:
            return rdata_type::domain_name;
        case query_type::null:
            return rdata_type::opaque;
        case query_type::well_known_service:
            return rdata_type::opaque;
        case query_type::domain_pointer:
            return rdata_type::domain_name;
        case query_type::host_info:
            return rdata_type::two_chars_strings;
        case query_type::mail_info:
            return rdata_type::two_domain_names;
        case query_type::mail_exchange:
            return rdata_type::i16_domain_name;
        case query_type::text_strings:
            return rdata_type::chars_strings;
        default:
            return rdata_type::opaque;
        }
    }

    struct dns_error_category_t : std::error_category {
        const char* name() const noexcept override {
            return "dns";
        }

        std::string message(int c) const override {
            response_code rc = static_cast<response_code>(c);
            switch (rc) {
            case response_code::no_error:
                return "";
            case response_code::format_error:
                return "Format error";
            case response_code::server_failure:
                return "Server failure";
            case response_code::name_error:
                return "Name Error";
            case response_code::not_implemented:
                return "Not Implemented";
            case response_code::refused:
                return "Refused";
            default:
                return "";
            }
        }
    };

    const dns_error_category_t dns_error_category_inst;
} // namespace

const std::error_category& dns::dns_category() noexcept {
    return dns_error_category_inst;
}

dns_parse_handler::~dns_parse_handler() {
}

bool dns_parse_handler::on_header(const dns_header& header) {
    std::ignore = header;
    return true;
}

bool dns_parse_handler::on_question(std::string_view name,
                                    const question_section& qestion) {
    std::ignore = qestion;
    return true;
}

bool dns_parse_handler::on_ipv4_resource_record(resource_record_type rtype,
                                                std::string_view name,
                                                const resource_record& record,
                                                ipv4 ip) {
    std::ignore = rtype;
    std::ignore = name;
    std::ignore = record;
    std::ignore = ip;
    return true;
}

bool dns_parse_handler::on_ipv6_resource_record(resource_record_type rtype,
                                                std::string_view name,
                                                const resource_record& record,
                                                ipv6 ip) {
    std::ignore = rtype;
    std::ignore = name;
    std::ignore = record;
    std::ignore = ip;
    return true;
}

bool dns_parse_handler::on_canonical_name_record(resource_record_type rtype,
                                                 std::string_view name,
                                                 const resource_record& record,
                                                 std::string cname) {
    std::ignore = rtype;
    std::ignore = name;
    std::ignore = record;
    std::ignore = cname;
    return true;
}

bool dns_parse_handler::on_start_of_authority_record(
    resource_record_type rtype, std::string_view name,
    const resource_record& record, start_of_authority& soa) {
    std::ignore = rtype;
    std::ignore = name;
    std::ignore = record;
    std::ignore = soa;
    return true;
}

bool dns_parse_handler::on_host_info_record(resource_record_type rtype,
                                            std::string_view name,
                                            const resource_record& record,
                                            const host_info& hinfo) {
    std::ignore = rtype;
    std::ignore = name;
    std::ignore = record;
    std::ignore = hinfo;
    return true;
}

bool dns_parse_handler::on_texts_record(resource_record_type rtype,
                                        std::string_view name,
                                        const resource_record& record,
                                        std::span<std::string_view> texts) {
    std::ignore = rtype;
    std::ignore = name;
    std::ignore = record;
    std::ignore = texts;
    return true;
}

bool dns_parse_handler::on_mail_info_record(resource_record_type rtype,
                                            std::string_view name,
                                            const resource_record& record,
                                            mail_info& minfo) {
    std::ignore = rtype;
    std::ignore = name;
    std::ignore = record;
    std::ignore = minfo;
    return true;
}

bool dns_parse_handler::on_mail_exchange_record(resource_record_type rtype,
                                                std::string_view name,
                                                const resource_record& record,
                                                mail_exchange& mailx) {
    std::ignore = rtype;
    std::ignore = name;
    std::ignore = record;
    std::ignore = mailx;
    return true;
}

bool dns_parse_handler::on_other_domain_name_records(
    resource_record_type rtype, std::string_view name,
    const resource_record& record, std::string domain_name) {
    std::ignore = rtype;
    std::ignore = name;
    std::ignore = record;
    std::ignore = domain_name;
    return true;
}

bool dns_parse_handler::on_opaque_resource_record(resource_record_type rtype,
                                                  std::string_view name,
                                                  const resource_record& record,
                                                  const_buffer rdata) {
    std::ignore = rtype;
    std::ignore = name;
    std::ignore = record;
    std::ignore = rdata;
    return true;
}

uint16_t dns::service_to_port(std::string_view service, std::error_code& ec) {
    bool is_numeric = true;
    if (service.size() > 4) {
        is_numeric = false;
    }
    else {
        for (auto ch : service) {
            if (ch < '0' || ch > '9') {
                is_numeric = false;
                break;
            }
        }
    }

    if (is_numeric) {
        return to_uint16(service, 10, ec);
    }

    std::string zservice = std::string(service);

    servent* srv = ::getservbyname(zservice.c_str(), nullptr);
    if (srv == nullptr) {
        ec.assign(host_not_found_code, system_category());
        return 0;
    }

    return *reinterpret_cast<beu16*>(&srv->s_port);
}

std::size_t dns::encode_name(dynamic_buffer query_storage,
                             std::string_view name, std::error_code& ec) {
    ec.clear();
    if (name.size() == 1 && name.front() == '.') {
        query_storage.push_back('\0');
        return 1;
    }
    std::string owned_name{name};
    const bool res = idna::domain_to_ascii(owned_name, false, true, true, false,
                                           false, true, false, ec);
    if (!res || ec) {
        return 0;
    }
    name = owned_name;
    if (name.empty() || (name.size() == 1 && name.front() == '.')) {
        query_storage.push_back('\0');
        return 1;
    }
    if (name.back() == '.') {
        name.remove_suffix(1);
    }
    constexpr uint32_t max_label_size = 63;

    auto make_bad_host_error = [&]() {
        ec.assign(host_not_found_code, system_category());
        return 0;
    };

    std::size_t consumed = 0;

    if (name.size() > max_formatted_domain) {
        return make_bad_host_error();
    }

    uint32_t labels_count = 0;
    for (auto label : name | split(".")) {
        if (label.empty() || label.size() > max_label_size) {
            return make_bad_host_error();
        }
        ++labels_count;
        query_storage.push_back(static_cast<uint8_t>(label.size()));
        query_storage.insert(label.data(), label.size());
        consumed += label.size() + 1;
    }

    if (labels_count < 1) {
        return make_bad_host_error();
    }

    query_storage.push_back('\0');
    ++consumed;
    return consumed;
}

uint16_t dns::make_dns_query(dynamic_buffer query_storage,
                             std::string_view name, bool ipv4,
                             uint16_t edns_size, std::error_code& ec) {
    ec.clear();

    dns_header_raw header;
    header.query(true);
    header.opcode(query_opcode::standard);
    header.recursion_desired(true);
    header.questions_count(1);
    if (edns_size) {
        header.additional_records_count(1);
    }

    query_storage.push_back(header);

    encode_name(query_storage, name, ec);
    if (ec) {
        return 0;
    }

    uint16_t qoffset = static_cast<uint16_t>(query_storage.size());

    question_section_raw qsection;
    qsection.qclass = static_cast<uint16_t>(dns_class::internet);
    ipv4 ? qsection.set_ipv4() : qsection.set_ipv6();

    query_storage.push_back(qsection);

    return qoffset;
}

std::size_t dns::parse_characters_string(const_buffer input,
                                         std::string_view& out,
                                         std::error_code& ec) noexcept {
    ec.clear();
    out = {};
    if (input.empty()) {
        ec = make_error(response_code::format_error);
        return 0;
    }
    auto view = input.to_string_view();
    std::size_t len = static_cast<uint8_t>(view.front());
    view.remove_prefix(1);
    if (len > view.size()) {
        ec = make_error(response_code::format_error);
        return 0;
    }
    out = view.substr(0, len);
    return len + 1;
}

std::size_t dns::parse_domain_name(const_buffer input,
                                   const_buffer whole_message, std::string& out,
                                   std::error_code& ec) {
    ec.clear();
    name_reader rd(input);
    char label_buff[max_domain_label + 1];

    while (rd.has_data()) {
        uint8_t label_len = rd.read_byte();
        if (!label_len) {
            if (!out.empty() && out.back() == '.') {
                out.pop_back();
            }
            return rd.total_consumed();
        }
        else if ((label_len & offset_mask) == offset_mask) {
            if (!rd.has_data()) {
                break;
            }
            uint8_t next_byte = rd.read_byte();
            uint16_t offset = extract_offset(label_len, next_byte);
            if (offset >= whole_message.size()) {
                break;
            }
            if (offset > rd.current_pos(whole_message)) {
                break;
            }
            size_t ptr_len = parse_domain_name(whole_message + offset,
                                               whole_message, out, ec);
            if (!ptr_len || ec) {
                break;
            }
            return rd.total_consumed();
        }
        else if (label_len > max_domain_label ||
                 rd.total_consumed() > max_formatted_domain) {
            break;
        }
        else if (!rd.get(label_len, label_buff)) {
            break;
        }
        label_buff[label_len] = '.';
        out.insert(out.end(), label_buff, label_buff + label_len + 1);
    }

    ec = make_error(response_code::format_error);
    return 0;
}

std::size_t dns::parse_question(const_buffer input, const_buffer whole_message,
                                std::string& qname, dns_parse_handler& handler,
                                std::error_code& ec) {
    /*
                                1  1  1  1  1  1
    0  1  2  3  4  5  6  7  8  9  0  1  2  3  4  5
    +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
    |                                               |
    /                     QNAME                     /
    /                                               /
    +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
    |                     QTYPE                     |
    +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
    |                     QCLASS                    |
    +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
    */
    ec.clear();
    // 1 byte for QNAME, 2 bytes for QTYPE and 2 bytes for QCLASS
    const std::size_t min_question_size = 1 + 2 * sizeof(uint16_t);
    if (input.size() < min_question_size) {
        ec = make_error(response_code::format_error);
        return 0;
    }
    qname.clear();
    std::size_t parsed_n = parse_domain_name(input, whole_message, qname, ec);
    if (ec || parsed_n == 0) {
        return 0;
    }
    input += parsed_n;
    if (input.size() < sizeof(question_section_raw)) {
        ec = make_error(response_code::format_error);
        return 0;
    }

    question_section qs;
    {
        question_section_raw qs_raw;
        std::memcpy(&qs_raw, input.data(), sizeof(qs_raw));
        qs = qs_raw.decode();
    }

    if (!handler.on_question(qname, qs)) {
        ec = std::make_error_code(std::errc::interrupted);
        return 0;
    }
    return parsed_n + sizeof(question_section_raw);
}

std::size_t dns::parse_texts_rdata(resource_record_type rtype,
                                   const_buffer rdata, std::string_view name,
                                   const resource_record& rr,
                                   dns_parse_handler& handler,
                                   std::error_code& ec) {
    /*
    +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
    /                   TXT-DATA                    /
    +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
    TXT-DATA	One or more <character-string>s.
    */
    if (rdata.empty()) {
        ec = make_error(response_code::format_error);
        return 0;
    }
    std::size_t parsed_n = 0;
    std::vector<std::string_view> texts;
    while (!rdata.empty()) {
        std::string_view text;
        std::size_t n = parse_characters_string(rdata, text, ec);
        if (ec) {
            return parsed_n;
        }
        texts.emplace_back(text);
        parsed_n += n;
        rdata += n;
    }
    if (!handler.on_texts_record(rtype, name, rr, texts)) {
        ec = std::make_error_code(std::errc::interrupted);
    }
    return parsed_n;
}

std::size_t
dns::parse_host_info_rdata(resource_record_type rtype, const_buffer rdata,
                           std::string_view name, const resource_record& rr,
                           dns_parse_handler& handler, std::error_code& ec) {
    /*
    +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
    /                      CPU                      /
    +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
    /                       OS                      /
    +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
    CPU	A <character-string> which specifies the CPU type.
    OS	A <character-string> which specifies the operating system type.
    */
    const std::size_t min_size = 2;
    const std::size_t rdata_size = rdata.size();
    if (rdata.size() < min_size) {
        ec = make_error(response_code::format_error);
        return 0;
    }
    host_info hinfo;
    std::size_t parsed_n = parse_characters_string(rdata, hinfo.cpu, ec);
    if (ec) {
        return 0;
    }
    rdata += parsed_n;
    parsed_n = parse_characters_string(rdata, hinfo.os, ec);
    if (ec) {
        return parsed_n;
    }
    rdata += parsed_n;
    if (!rdata.empty()) {
        ec = make_error(response_code::format_error);
        return parsed_n;
    }
    if (!handler.on_host_info_record(rtype, name, rr, hinfo)) {
        ec = std::make_error_code(std::errc::interrupted);
    }
    return rdata_size - rdata.size();
}

std::size_t dns::parse_mail_exchange_rdata(
    resource_record_type rtype, const_buffer rdata, const_buffer whole_message,
    std::string_view name, const resource_record& rr,
    dns_parse_handler& handler, std::error_code& ec) {
    /*
    +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
    |                  PREFERENCE                   |
    +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
    /                   EXCHANGE                    /
    /                                               /
    +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
    */
    const std::size_t min_size = sizeof(uint16_t) + 1;
    const std::size_t rdata_size = rdata.size();
    if (rdata.size() < min_size) {
        ec = make_error(response_code::format_error);
        return 0;
    }
    beu16 preference_be;
    std::memcpy(&preference_be, rdata.data(), sizeof(preference_be));
    rdata += sizeof(uint16_t);
    mail_exchange mailx;
    mailx.preference = preference_be;
    std::size_t parsed_n =
        parse_domain_name(rdata, whole_message, mailx.exchange_host, ec);
    if (ec) {
        return 0;
    }
    rdata += parsed_n;
    if (!rdata.empty()) {
        ec = make_error(response_code::format_error);
        return 0;
    }
    if (!handler.on_mail_exchange_record(rtype, name, rr, mailx)) {
        ec = std::make_error_code(std::errc::interrupted);
        return parsed_n;
    }
    return rdata_size - rdata.size();
}

std::size_t
dns::parse_mail_info_rdata(resource_record_type rtype, const_buffer rdata,
                           const_buffer whole_message, std::string_view name,
                           const resource_record& rr,
                           dns_parse_handler& handler, std::error_code& ec) {
    /*
    +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
    /                    RMAILBX                    /
    +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
    /                    EMAILBX                    /
    +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
    */
    const std::size_t min_size = 2;
    const std::size_t rdata_size = rdata.size();
    if (rdata.size() < min_size) {
        ec = make_error(response_code::format_error);
        return 0;
    }
    mail_info minfo;
    std::size_t parsed_n =
        parse_domain_name(rdata, whole_message, minfo.responsible_mail, ec);
    if (ec) {
        return 0;
    }
    rdata += parsed_n;
    parsed_n = parse_domain_name(rdata, whole_message, minfo.error_mail, ec);
    if (ec) {
        return 0;
    }
    rdata += parsed_n;
    if (!rdata.empty()) {
        ec = make_error(response_code::format_error);
        return 0;
    }
    if (!handler.on_mail_info_record(rtype, name, rr, minfo)) {
        ec = std::make_error_code(std::errc::interrupted);
        return parsed_n;
    }
    return rdata_size - rdata.size();
}

std::size_t dns::parse_start_of_authority_rdata(
    resource_record_type rtype, const_buffer rdata, const_buffer whole_message,
    std::string_view name, const resource_record& rr,
    dns_parse_handler& handler, std::error_code& ec) {
    /*
    +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
    /                     MNAME                     /
    /                                               /
    +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
    /                     RNAME                     /
    +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
    |                    SERIAL                     |
    |                                               |
    +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
    |                    REFRESH                    |
    |                                               |
    +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
    |                     RETRY                     |
    |                                               |
    +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
    |                    EXPIRE                     |
    |                                               |
    +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
    |                    MINIMUM                    |
    |                                               |
    +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
    */
    ec.clear();
    const std::size_t min_size = 2 + sizeof(uint32_t) * 5;
    const std::size_t rdata_size = rdata.size();
    if (rdata.size() < min_size) {
        ec = make_error(response_code::format_error);
        return 0;
    }
    std::string mname;
    std::size_t parsed_n = parse_domain_name(rdata, whole_message, mname, ec);
    if (ec) {
        return 0;
    }
    rdata += parsed_n;
    std::string rname;
    parsed_n = parse_domain_name(rdata, whole_message, rname, ec);
    if (ec) {
        return 0;
    }
    rdata += parsed_n;
    if (rdata.size() != sizeof(start_of_authority_raw)) {
        ec = make_error(response_code::format_error);
        return 0;
    }
    start_of_authority soa;
    {
        start_of_authority_raw soa_raw;
        std::memcpy(&soa_raw, rdata.data(), sizeof(soa_raw));
        soa = soa_raw.decoded();
    }
    rdata += sizeof(start_of_authority_raw);
    soa.primary_name = std::move(mname);
    soa.responsible_name = std::move(rname);
    if (!handler.on_start_of_authority_record(rtype, name, rr, soa)) {
        ec = std::make_error_code(std::errc::interrupted);
        return parsed_n;
    }
    return rdata_size - rdata.size();
}

std::size_t dns::parse_ipv4_rdata(resource_record_type rtype,
                                  const_buffer rdata, std::string_view name,
                                  const resource_record& rr,
                                  dns_parse_handler& handler,
                                  std::error_code& ec) {
    ec.clear();
    /*
    +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
    |                    ADDRESS                    |
    +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
    ADDRESS	A 32 bit Internet address.
    */
    if (rr.data_length != sizeof(uint32_t)) {
        ec = make_error(response_code::format_error);
        return 0;
    }
    beu32 ipv4_be;
    std::memcpy(&ipv4_be, rdata.data(), sizeof(ipv4_be));
    ipv4 ip{static_cast<uint32_t>(ipv4_be)};
    if (!handler.on_ipv4_resource_record(rtype, name, rr, ip)) {
        ec = std::make_error_code(std::errc::interrupted);
        return 0;
    }
    return sizeof(uint32_t);
}

std::size_t dns::parse_ipv6_rdata(resource_record_type rtype,
                                  const_buffer rdata, std::string_view name,
                                  const resource_record& rr,
                                  dns_parse_handler& handler,
                                  std::error_code& ec) {
    ec.clear();
    /*
    RFC3596:
    A 128 bit IPv6 address is encoded in the data portion of an AAAA
    resource record in network byte order (high-order byte first).
    */
    if (rr.data_length != 16) {
        ec = make_error(response_code::format_error);
        return 0;
    }

    ipv6::bytes_type ipv6_bytes;
    std::memcpy(ipv6_bytes.data(), rdata.data(), ipv6_bytes.size());
    ipv6 ip{ipv6_bytes};
    if (!handler.on_ipv6_resource_record(rtype, name, rr, ip)) {
        ec = std::make_error_code(std::errc::interrupted);
        return 0;
    }
    return ipv6_bytes.size();
}

std::size_t dns::parse_domain_name_rdata(
    resource_record_type rtype, const_buffer rdata, const_buffer whole_message,
    std::string_view record_name, const resource_record& rr,
    dns_parse_handler& handler, std::error_code& ec) {
    ec.clear();
    std::string domain_name;
    std::size_t parsed_n =
        parse_domain_name(rdata, whole_message, domain_name, ec);
    if (ec) {
        return 0;
    }
    rdata += parsed_n;
    if (!rdata.empty()) {
        ec = make_error(response_code::format_error);
        return 0;
    }
    bool handler_result = true;
    if (rr.type == query_type::canonical_name) {
        handler_result = handler.on_canonical_name_record(
            rtype, record_name, rr, std::move(domain_name));
    }
    else {
        handler_result = handler.on_other_domain_name_records(
            rtype, record_name, rr, std::move(domain_name));
    }
    if (!handler_result) {
        ec = std::make_error_code(std::errc::interrupted);
        return 0;
    }
    return parsed_n;
}

std::size_t
dns::parse_resource_record(resource_record_type rtype, const_buffer input,
                           const_buffer whole_message, std::string& record_name,
                           dns_parse_handler& handler, std::error_code& ec) {
    /*
                                                                    1  1  1
    1  1 1 0  1  2  3  4  5  6  7  8  9  0  1  2  3  4  5
    +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
    |                                               |
    /                                               /
    /                      NAME                     /
    |                                               |
    +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
    |                      TYPE                     |
    +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
    |                     CLASS                     |
    +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
    |                      TTL                      |
    |                                               |
    +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
    |                   RDLENGTH                    |
    +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--|
    /                     RDATA                     /
    /                                               /
    +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
    */
    ec.clear();
    record_name.clear();
    constexpr std::size_t min_record_size = 1 + sizeof(resource_record_raw);
    if (input.size() < min_record_size) {
        ec = make_error(response_code::format_error);
        return 0;
    }
    std::size_t parsed_n =
        parse_domain_name(input, whole_message, record_name, ec);
    if (ec) {
        return 0;
    }
    input += parsed_n;
    if (input.size() < sizeof(resource_record_raw)) {
        ec = make_error(response_code::format_error);
        return 0;
    }

    resource_record rr;
    {
        resource_record_raw rr_raw;
        std::memcpy(&rr_raw, input.data(), sizeof(rr_raw));
        rr = rr_raw.decode();
    }
    parsed_n += sizeof(resource_record_raw);
    input += sizeof(resource_record_raw);
    if (rr.data_length > 0) {
        if (input.size() < rr.data_length) {
            ec = make_error(response_code::format_error);
            return 0;
        }
    }
    std::size_t parsed_rdata_n = 0;
    if (rr.type == query_type::ipv4) {
        parsed_rdata_n =
            parse_ipv4_rdata(rtype, input.sub_buffer(0, rr.data_length),
                             record_name, rr, handler, ec);
    }
    else if (rr.type == query_type::ipv6) {
        parsed_rdata_n =
            parse_ipv6_rdata(rtype, input.sub_buffer(0, rr.data_length),
                             record_name, rr, handler, ec);
    }
    else if (rr.type == query_type::start_of_authority) {
        parsed_rdata_n = parse_start_of_authority_rdata(
            rtype, input.sub_buffer(0, rr.data_length), whole_message,
            record_name, rr, handler, ec);
    }
    else if (rr.type == query_type::host_info) {
        parsed_rdata_n =
            parse_host_info_rdata(rtype, input.sub_buffer(0, rr.data_length),
                                  record_name, rr, handler, ec);
    }
    else if (rr.type == query_type::text_strings) {
        parsed_rdata_n =
            parse_texts_rdata(rtype, input.sub_buffer(0, rr.data_length),
                              record_name, rr, handler, ec);
    }
    else if (rr.type == query_type::mail_info) {
        parsed_rdata_n =
            parse_mail_info_rdata(rtype, input.sub_buffer(0, rr.data_length),
                                  whole_message, record_name, rr, handler, ec);
    }
    else if (rr.type == query_type::mail_exchange) {
        parsed_rdata_n = parse_mail_exchange_rdata(
            rtype, input.sub_buffer(0, rr.data_length), whole_message,
            record_name, rr, handler, ec);
    }
    else if (get_rdata_type(rr.type) == rdata_type::domain_name) {
        parsed_rdata_n = parse_domain_name_rdata(
            rtype, input.sub_buffer(0, rr.data_length), whole_message,
            record_name, rr, handler, ec);
    }
    else {
        parsed_rdata_n = rr.data_length;
        if (!handler.on_opaque_resource_record(
                rtype, record_name, rr, input.sub_buffer(0, rr.data_length))) {
            ec = std::make_error_code(std::errc::interrupted);
        }
    }
    if (ec) {
        return 0;
    }
    return parsed_n + parsed_rdata_n;
}

std::size_t
dns::parse_resource_records(resource_record_type rtype, uint16_t records_count,
                            const_buffer input, const_buffer whole_message,
                            std::string& record_name,
                            dns_parse_handler& handler, std::error_code& ec) {
    ec.clear();
    record_name.clear();
    std::size_t parsed_n = 0;
    for (uint16_t i = 0; i < records_count; ++i) {
        std::size_t n = parse_resource_record(rtype, input, whole_message,
                                              record_name, handler, ec);
        if (ec) {
            break;
        }
        input += n;
        parsed_n += n;
    }
    record_name.clear();
    return parsed_n;
}

std::size_t dns::parse_dns_message(const_buffer input,
                                   dns_parse_handler& handler,
                                   std::error_code& ec) {
    /*
    +---------------------+
    |        Header       |
    +---------------------+
    |       Question      | the question for the name server
    +---------------------+
    |        Answer       | RRs answering the question
    +---------------------+
    |      Authority      | RRs pointing toward an authority
    +---------------------+
    |      Additional     | RRs holding additional information
    +---------------------+
    */
    ec.clear();
    const const_buffer whole_message = input;

    // parse the headers
    if (input.size() < sizeof(dns_header_raw) ||
        input.size() > std::numeric_limits<uint16_t>::max()) {
        ec = make_error(response_code::format_error);
        return 0;
    }
    dns_header header;
    {
        dns_header_raw header_raw;
        std::memcpy(&header_raw, input.data(), sizeof(header_raw));
        input += sizeof(header_raw);
        header = header_raw.decode();
    }
    if (!handler.on_header(header)) {
        ec = std::make_error_code(std::errc::interrupted);
        return 0;
    }

    std::string domain_name;
    // parse questions
    for (uint16_t i = 0; i < header.questions; ++i) {
        input += parse_question(input, whole_message, domain_name, handler, ec);
        if (ec) {
            return whole_message.size() - input.size();
        }
    }
    // parse answers
    input +=
        parse_resource_records(resource_record_type::answer, header.answers,
                               input, whole_message, domain_name, handler, ec);
    if (ec) {
        return whole_message.size() - input.size();
    }
    // parse authority
    input += parse_resource_records(resource_record_type::authority,
                                    header.name_servers, input, whole_message,
                                    domain_name, handler, ec);
    if (ec) {
        return whole_message.size() - input.size();
    }
    // parse additional records
    input += parse_resource_records(resource_record_type::additional,
                                    header.additional_records, input,
                                    whole_message, domain_name, handler, ec);
    // the message must be fully consumed
    if (!input.empty()) {
        ec = make_error(response_code::format_error);
    }
    return whole_message.size() - input.size();
}

bool dns::compare_two_domains(std::string_view domain1,
                              std::string_view domain2) noexcept {
    if (domain1.empty() || domain2.empty()) {
        return false;
    }
    const bool wildcard1 = domain1[0] == '*';
    const bool wildcard2 = domain2[0] == '*';
    if (wildcard1 == wildcard2) {
        return iequal(domain1, domain2);
    }
    if (domain1.size() == 1 || domain2.size() == 1) {
        return false;
    }
    if (domain2[0] == '*') {
        std::swap(domain1, domain2);
    }
    size_t first_dot_idx = domain2.find('.');
    if (first_dot_idx == std::string_view::npos ||
        first_dot_idx + 1 >= domain2.size()) {
        return false;
    }
    return iequal(domain1.substr(1), domain2.substr(first_dot_idx + 1));
}

void cache_storage::add(std::string_view hostname, const endpoint& address,
                        std::chrono::seconds time) {
    if (hostname.empty() || hostname.size() > 255 || !address.is_valid() ||
        time.count() <= 0) {
        return;
    }
    auto caches = caches_.synchronize();
    auto it = std::find_if(caches->addresses.begin(), caches->addresses.end(),
                           [&](const cache_ip_entry& entry) {
                               return entry.matches(hostname, address.family());
                           });
    if (it != caches->addresses.end()) {
        *it = cache_ip_entry{hostname, address, time};
        return;
    }
    caches->addresses.emplace_back(hostname, address, time);
}

void cache_storage::add(std::string_view alias, std::string_view cname,
                        std::chrono::seconds time) {
    if (alias.empty() || alias.size() > 255 || cname.empty() ||
        cname.size() > 255 || time.count() <= 0) {
        return;
    }
    auto caches = caches_.synchronize();
    auto& cnames = caches->cnames;
    auto it = std::find_if(cnames.begin(), cnames.end(),
                           [&](const cache_canonical_name_entry& entry) {
                               return entry.matches(alias);
                           });
    if (it != cnames.end()) {
        *it = cache_canonical_name_entry{alias, cname, time};
        return;
    }
    cnames.emplace_back(alias, cname, time);
}

bool cache_storage::find(std::string_view hostname, address_family af,
                         uint16_t port, std::vector<endpoint>& results) {
    bool found = false;
    auto now = std::chrono::steady_clock::now();
    auto caches = caches_.synchronize();
    while (!hostname.empty()) {
        auto& addresses = caches->addresses;
        for (auto it = addresses.begin(); it != addresses.end();) {
            if (it->expired(now)) {
                it = addresses.erase(it);
                continue;
            }
            if (!found && it->matches(hostname, af)) {
                results.emplace_back(it->to_endpoint(port));
                found = true;
                break;
            }
            it = std::next(it);
        }
        if (!found) {
            auto cname_result = find_cname(now, caches->cnames, hostname);
            if (cname_result != nullptr) {
                hostname = cname_result->canonical_name();
                continue;
            }
        }
        break;
    }
    return found;
}

void cache_storage::clear() noexcept {
    auto caches = caches_.synchronize();
    caches->addresses.clear();
    caches->cnames.clear();
}

void cache_storage::clear_expired() noexcept {
    auto caches = caches_.synchronize();
    auto now = std::chrono::steady_clock::now();
    std::erase_if(caches->addresses, [now](const cache_ip_entry& entry) {
        return entry.expired(now);
    });
    std::erase_if(caches->cnames,
                  [now](const cache_canonical_name_entry& entry) {
                      return entry.expired(now);
                  });
}

const cache_canonical_name_entry*
cache_storage::find_cname(std::chrono::steady_clock::time_point now,
                          std::vector<cache_canonical_name_entry>& cnames,
                          std::string_view host) {
    const cache_canonical_name_entry* result = nullptr;
    for (auto it = cnames.begin(); it != cnames.end();) {
        if (it->expired(now)) {
            it = cnames.erase(it);
            continue;
        }
        if (it->matches(host)) {
            result = std::addressof(*it);
        }
        it = std::next(it);
    }
    return result;
}

dns_ip_answers_handler::~dns_ip_answers_handler() {
}

bool dns_ip_answers_handler::on_ipv4_resource_record(
    resource_record_type rtype, std::string_view name,
    const resource_record& record, ipv4 ip) {
    if (cache_ != nullptr) {
        cache_->add(name, endpoint{ip, port_}, record.cache_time_inerval);
    }
    if (rtype != resource_record_type::answer ||
        !compare_two_domains(host_, name)) {
        return true;
    }
    results_.emplace_back(ip, port_);
    return true;
}

bool dns_ip_answers_handler::on_ipv6_resource_record(
    resource_record_type rtype, std::string_view name,
    const resource_record& record, ipv6 ip) {
    if (cache_ != nullptr) {
        cache_->add(name, endpoint{ip, port_}, record.cache_time_inerval);
    }
    if (rtype != resource_record_type::answer ||
        !compare_two_domains(host_, name)) {
        return true;
    }
    results_.emplace_back(ip, port_);
    return true;
}

bool dns_ip_answers_handler::on_canonical_name_record(
    resource_record_type rtype, std::string_view name,
    const resource_record& record, std::string cname) {
    if (cache_ != nullptr) {
        cache_->add(name, cname, record.cache_time_inerval);
    }
    if (rtype != resource_record_type::answer ||
        !compare_two_domains(host_, name)) {
        return true;
    }
    cname_ = std::move(cname);
    host_ = cname_;
    return true;
}

dns::detail::handler_base::~handler_base() {
}