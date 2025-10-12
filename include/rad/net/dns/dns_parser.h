#pragma once
#include <rad/async/executor.h>
#include <rad/async/io_executor.h>
#include <rad/buffer.h>
#include <rad/dynamic_buffer.h>
#include <rad/libbase.h>
#include <rad/net/types.h>
#include <rad/stack_allocator.h>
#include <rad/string.h>
#include <rad/threading/synchronized_value.h>

#include <chrono>

namespace RAD_LIB_NAMESPACE::net::dns {
    namespace details = RAD_LIB_NAMESPACE::detail;

    /*!
     * @brief The kind of query in the DNS message.
     */
    enum class query_opcode : uint16_t {
        /// A standard query (QUERY)
        standard = 0,
        /// An inverse query (IQUERY)
        inverse = 1,
        /// A server status request (STATUS)
        server_status,
    };

    /*!
     * @brief The DNS Response code (RCODE).
     */
    enum class response_code : uint16_t {
        /// No error condition
        no_error = 0,
        /*!
         * @brief Format error - The name server was unable to interpret the
         * query.
         */
        format_error = 1,
        /*!
         * @brief Server failure - The name server was unable to process this
         * query due to a problem with the name server.
         */
        server_failure = 2,
        /*!
         * @brief Name Error - Meaningful only for responses from an
         * authoritative name server, this code signifies that the domain name
         * referenced in the query does not exist.
         */
        name_error = 3,
        /*!
         * @brief Not Implemented - The name server does not support the
         * requested kind of query.
         */
        not_implemented = 4,
        /*!
         * @brief Refused - The name server refuses to perform the specified
         * operation for policy reasons.
         */
        refused = 5,
    };

    /*!
     * @brief Get a const reference to the DNS error category.
     * @return A const reference to the DNS error category.
     */
    RAD_EXPORT_DECL const std::error_category& dns_category() noexcept;

    /*!
     * @brief Make `std::error_code` with DNS error category.
     * @param c The DNS response code (RCODE).
     * @return A `std::error_code` with DNS error category.
     */
    inline std::error_code make_error(response_code c) noexcept {
        return std::error_code{static_cast<int>(c), dns_category()};
    }

    /*!
     * @brief The type of DNS message, either a query or a response.
     */
    enum class message_type : uint16_t {
        /// A query message.
        query = 0,
        /// A response message.
        response = 1,
    };

    /*!
     * @brief The TYPE and QTYPE fields that are used in questions and
     * resource records.
     */
    enum class query_type : std::uint16_t {
        invalid = 0,
        /// A a host address (ipv4)
        host = 1,
        /// A a host address (ipv4)
        ipv4 = 1,
        /// AAAA record type specific to the Internet class
        /// that stores a single IPv6 address. (RFC3596)
        ipv6 = 28,
        /// NS an authoritative name server
        name_server = 2,
        /// MD a mail destination (Obsolete - use MX)
        mail_destination = 3,
        /// MF a mail forwarder (Obsolete - use MX)
        mail_forwarder = 4,
        /// CNAME the canonical name for an alias
        canonical_name = 5,
        /// SOA marks the start of a zone of authority
        start_of_authority = 6,
        /// MB a mailbox domain name (EXPERIMENTAL)
        mailbox = 7,
        /// MG a mail group member (EXPERIMENTAL)
        mail_group = 8,
        /// MR a mail rename domain name (EXPERIMENTAL)
        mail_rename = 9,
        /// NULL a null RR (EXPERIMENTAL)
        null = 10,
        /// WKS a well known service description
        well_known_service = 11,
        /// PTR a domain name pointer
        domain_pointer = 12,
        /// HINFO host information
        host_info = 13,
        /// MINFO mailbox or mail list information
        mail_info = 14,
        /// MX mail exchange
        mail_exchange = 15,
        /// TXT text strings
        text_strings = 16,
        /// For EDNS record the OPT RR has RR type 41. (RFC6891)
        opt = 41,
        /// AXFR A request for a transfer of an entire zone (QTYPE only)
        axfr = 252,
        /// MAILB A request for mailbox-related records (MB, MG or MR)
        /// (QTYPE only)
        mailbox_related = 253,
        /// MAILA A request for mail agent RRs (Obsolete - see MX)
        /// (QTYPE only)
        mail_agents = 254,
        /// * A request for all records (QTYPE only)
        all = 255,
    };

    /*!
     * @brief The CLASS and QCLASS fields that are used in questions and
     * resource records.
     */
    enum class dns_class : std::uint16_t {
        /// Reserved (RFC6895).
        reserved = 0,
        /// IN the Internet
        internet = 1,
        /// CN the CSNET class (Obsolete - used only for examples in
        /// some
        /// obsolete
        /// RFCs)
        csnet = 2,
        /// CH the CHAOS class
        chaos = 3,
        /// HS Hesiod [Dyer 87]
        hesiod = 1,
        /// NONE (RFC2136)
        none = 254,
        /// * any class (QTYPE only)
        any = 255,
    };

    /*!
     * @brief The size of encoded DNS Header.
     */
    inline constexpr uint32_t dns_header_size = 12;
    /*!
     * @brief The max size of DNS message over UDP without using EDNS.
     */
    inline constexpr uint32_t max_udp_no_edns = 512;
    /*!
     * @brief The max size of a single label of a domain name.
     */
    inline constexpr uint32_t max_domain_label = 63;
    /*!
     * @brief The max size of a domain name wihtout separating dots.
     */
    inline constexpr uint32_t max_domain_string = 253;
    /*!
     * @brief The max size of a domain name including separating dots.
     */
    inline constexpr uint32_t max_formatted_domain = 255;
    /*!
     * @brief The size of the fixed part of DNS QUESTION.
     */
    inline constexpr uint32_t fixed_question_section_size = 4;
    /*!
     * @brief The max size of DNS QUESTION including QNAME.
     */
    inline constexpr uint32_t max_question_section_size =
        fixed_question_section_size + max_formatted_domain;
    /*!
     * @brief The size of EDNS header including the root domain
     * and excluding the RDATA.
     * It is 10 bytes for the record and 1 byte for the root domain.
     */
    inline constexpr uint32_t edns_header_size = 11;

    /*!
     * @brief Decoded DNS header.
     */
    struct dns_header {
        /*!
         * @brief (ID) A 16 bit identifier assigned by the program that
         * generates any kind of query.  This identifier is copied
         * the corresponding reply and can be used by the requester
         * to match up replies to outstanding queries.
         */
        uint16_t id = 0;
        /*!
         * @brief (QDCOUNT) An unsigned 16 bit integer specifying the number of
         * entries in the question section.
         */
        uint16_t questions = 0;
        /*!
         * @brief (ANCOUNT) An unsigned 16 bit integer specifying the number of
         * resource records in the answer section.
         */
        uint16_t answers = 0;
        /*!
         * @brief (NSCOUNT) An unsigned 16 bit integer specifying the number of
         * name server resource records in the authority records section.
         */
        uint16_t name_servers = 0;
        /*!
         * @brief (ARCOUNT) An unsigned 16 bit integer specifying the number of
         * resource records in the additional records section.
         */
        uint16_t additional_records = 0;
        /*!
         * @brief (RCODE) Response code - this 4 bit field is set as part of
         * responses.
         */
        dns::response_code response_code = dns::response_code::no_error;
        /*!
         * @brief (OPCODE) A four bit field that specifies kind of query in this
         * message.
         */
        query_opcode opcode = query_opcode::standard;
        /*!
         * @brief (QR) A one bit field that specifies whether this message is a
         * query (0), or a response (1).
         */
        bool is_response = false;
        /*!
         * @brief (AA) Authoritative Answer - this bit is valid in responses,
         * and specifies that the responding name server is an authority for the
         * domain name in question section.
         */
        bool authoritative_answer = false;
        /*!
         * @brief (TC) TrunCation - specifies that this message was truncated
         * due to length greater than that permitted on the transmission
         * channel.
         */
        bool truncated = false;
        /*!
         * @brief (RD) Recursion Desired - this bit may be set in a query and is
         * copied into the response.  If RD is set, it directs the name server
         * to pursue the query recursively. Recursive query support is optional.
         */
        bool recursion_desired = false;
        /*!
         * @brief (RA) Recursion Available - this be is set or cleared in a
         * response, and denotes whether recursive query support is available in
         * the name server.
         */
        bool recursion_available = false;
    };

    /*!
     * @brief DNS header encoded in wire format.
     */
    struct dns_header_raw {
        dns_header_raw() = default;

        dns_header_raw(const dns_header& header) {
            id(header.id);
            response(header.is_response);
            opcode(header.opcode);
            authoritative_answer(header.authoritative_answer);
            truncation(header.truncated);
            recursion_desired(header.recursion_desired);
            recursion_available(header.recursion_available &&
                                header.is_response);
            response_code(header.response_code);
            questions_count(header.questions);
            answers_count(header.answers);
            name_server_count(header.name_servers);
            additional_records_count(header.additional_records);
        }

        dns_header decode() const noexcept {
            dns_header header;
            header.id = id();
            header.is_response = response();
            header.opcode = opcode();
            header.authoritative_answer = authoritative_answer();
            header.truncated = truncation();
            header.recursion_desired = recursion_desired();
            header.recursion_available = recursion_available();
            header.response_code = response_code();
            header.questions = questions_count();
            header.answers = answers_count();
            header.name_servers = name_server_count();
            header.additional_records = additional_records_count();
            return header;
        }

        uint16_t id() const noexcept {
            return *reinterpret_cast<const beu16*>(buff.data());
        }

        void id(uint16_t id) noexcept {
            *reinterpret_cast<beu16*>(buff.data()) = id;
        }

        bool response() const noexcept {
            return bits::check<7>(buff[2]);
        }

        void response(bool is_res) noexcept {
            if (is_res) {
                bits::set<7>(buff[2]);
            }
            else {
                bits::clear<7>(buff[2]);
            }
        }

        bool query() const noexcept {
            return !response();
        }

        void query(bool is_query) noexcept {
            response(!is_query);
        }

        query_opcode opcode() const noexcept {
            return static_cast<query_opcode>(bits::extract<3, 6>(buff[2]));
        }

        void opcode(query_opcode type) noexcept {
            buff[2] |= bits::extract<3, 6>(static_cast<uint8_t>(type));
        }

        bool authoritative_answer() const noexcept {
            return bits::check<2>(buff[2]);
        }

        void authoritative_answer(bool on) noexcept {
            bits::assign<2>(buff[2], on);
        }

        bool truncation() const noexcept {
            return bits::check<1>(buff[2]);
        }

        void truncation(bool on) noexcept {
            bits::assign<1>(buff[2], on);
        }

        bool recursion_desired() const noexcept {
            return bits::check<0>(buff[2]);
        }

        void recursion_desired(bool on) noexcept {
            bits::assign<0>(buff[2], on);
        }

        bool recursion_available() const noexcept {
            return bits::check<7>(buff[3]);
        }

        void recursion_available(bool on) noexcept {
            bits::assign<7>(buff[3], on);
        }

        dns::response_code response_code() const noexcept {
            uint8_t rcode = buff[3] & 0b00001111;
            return static_cast<dns::response_code>(rcode);
        }

        void response_code(dns::response_code rcode) noexcept {
            buff[3] |= static_cast<uint8_t>(rcode) & 0b00001111;
        }

        uint16_t questions_count() const noexcept {
            return *reinterpret_cast<const beu16*>(&buff[4]);
        }

        void questions_count(uint16_t count) noexcept {
            *reinterpret_cast<beu16*>(&buff[4]) = count;
        }

        uint16_t answers_count() const noexcept {
            return *reinterpret_cast<const beu16*>(&buff[6]);
        }

        void answers_count(uint16_t count) noexcept {
            *reinterpret_cast<beu16*>(&buff[6]) = count;
        }

        uint16_t name_server_count() const noexcept {
            return *reinterpret_cast<const beu16*>(&buff[8]);
        }

        void name_server_count(uint16_t count) noexcept {
            *reinterpret_cast<beu16*>(&buff[8]) = count;
        }

        uint16_t additional_records_count() const noexcept {
            return *reinterpret_cast<const beu16*>(&buff[10]);
        }

        void additional_records_count(uint16_t count) noexcept {
            *reinterpret_cast<beu16*>(&buff[10]) = count;
        }

    private:
        std::array<uint8_t, 12> buff = {};
    };

    /*!
     * @brief Decoded DNS Question section.
     */
    struct question_section {
        /*!
         * @brief (QTYPE) a two octet code which specifies the type of the
         * query.
         */
        query_type type = query_type::invalid;
        /*!
         * @brief (QCLASS) a two octet code that specifies the class of the
         * query.
         */
        dns_class qclass = dns_class::reserved;
    };

    /*!
     * @brief DNS Question section encoded in wire format after QNAME field.
     */
    struct question_section_raw {
        beu16 qtype;
        beu16 qclass;

        question_section_raw() = default;

        question_section_raw(const question_section& qs) {
            qtype = static_cast<uint16_t>(qs.type);
            qclass = static_cast<uint16_t>(qs.qclass);
        }

        question_section decode() const noexcept {
            question_section qs;
            qs.type = static_cast<query_type>(static_cast<uint16_t>(qtype));
            qs.qclass = static_cast<dns_class>(static_cast<uint16_t>(qclass));
            return qs;
        }

        /*!
         * @brief Check if the question type matches the a type.
         * @param type The type to match the question type with.
         * @return true if the two type are the same, and false
         * otherwise.
         */
        bool is_type(query_type type) const noexcept {
            return static_cast<uint16_t>(type) == qtype;
        }

        /*!
         * @brief Check if the question class matches a class.
         * @param c The class to match the question class with.
         * @return true if the two classes are the same, and
         * false otherwise.
         */
        bool is_class(dns_class c) const noexcept {
            return static_cast<uint16_t>(c) == qclass;
        }

        /*!
         * @brief Check if the question type is ipv4
         * @return true if the question type is ipv4, and false
         * otherwise.
         */
        bool is_ipv4() const noexcept {
            return is_type(query_type::ipv4);
        }

        /*!
         * @brief Check if the question type is ipv6
         * @return true if the question type is ipv6, and false
         * otherwise.
         */
        bool is_ipv6() const noexcept {
            return is_type(query_type::ipv6);
        }

        /*!
         * @brief Set the question type to ipv4.
         */
        void set_ipv4() {
            qtype = static_cast<uint16_t>(query_type::ipv4);
        }

        /*!
         * @brief Set the question type to ipv6.
         */
        void set_ipv6() {
            qtype = static_cast<uint16_t>(query_type::ipv6);
        }
    };

    /*!
     * @brief Decoded DNS resource record (RR)
     */
    struct resource_record {
        /// The record TYPE.
        query_type type = query_type::invalid;
        /// The record CLASS.
        dns_class record_class = dns_class::reserved;
        /*!
         * @brief the time interval that the resource record may
         * be cached before the source of the information should
         * again be consulted.
         */
        std::chrono::seconds cache_time_inerval = {};
        /*!
         * @brief The record RDLENGTH that specifies the length of following
         * RDATA.
         */
        uint16_t data_length = 0;
    };

    /*!
     * @brief DNS Resource record (RR) encoded in wire format after NAME
     * field and not including RDATA. It contains TYPE, CLASS, TTL and
     * RDLENGTH.
     */
    struct resource_record_raw {
        resource_record_raw() = default;

        resource_record_raw(const resource_record& rr) {
            type(rr.type);
            record_class(rr.record_class);
            ttl(rr.cache_time_inerval);
            data_length(rr.data_length);
        }

        resource_record decode() const noexcept {
            resource_record rr;
            rr.type = type();
            rr.record_class = record_class();
            rr.cache_time_inerval = ttl();
            rr.data_length = data_length();
            return rr;
        }

        /*!
         * @brief Get the type of data in RDATA field.
         * @return The type of data in RDATA field.
         */
        query_type type() const noexcept {
            beu16 type_be;
            std::memcpy(&type_be, data.data(), sizeof(type_be));
            return static_cast<query_type>(static_cast<uint16_t>(type_be));
        };

        /*!
         * @brief Set the type of data in RDATA field.
         * @param t The type of data in RDATA field.
         */
        void type(query_type t) noexcept {
            beu16 class_be = static_cast<uint16_t>(t);
            std::memcpy(data.data(), &class_be, sizeof(class_be));
        };

        /*!
         * @brief Get the class of data in RDATA field.
         * @return The class of data in RDATA field.
         */
        dns_class record_class() const noexcept {
            beu16 class_be;
            std::memcpy(&class_be, data.data() + sizeof(std::uint16_t),
                        sizeof(class_be));
            return static_cast<dns_class>(static_cast<uint16_t>(class_be));
        };

        /*!
         * @brief Set the class of data in RDATA field.
         * @param c The class of data in RDATA field.
         */
        void record_class(dns_class c) noexcept {
            beu16 class_be = static_cast<uint16_t>(c);
            std::memcpy(data.data() + sizeof(std::uint16_t), &class_be,
                        sizeof(class_be));
        };

        /*!
         * @brief Get the time interval in seconds that the
         * resource record may be cached before it should be
         * discarded.
         * @return The time interval in seconds.
         */
        std::chrono::seconds ttl() const noexcept {
            beu32 ttl_be;
            std::memcpy(&ttl_be, data.data() + sizeof(std::uint16_t) * 2,
                        sizeof(ttl_be));
            return std::chrono::seconds{static_cast<uint32_t>(ttl_be)};
        }

        /*!
         * @brief Set the time interval in seconds that the
         * resource record may be cached before it should be
         * discarded.
         * @param t The time interval in seconds.
         */
        void ttl(std::chrono::seconds t) noexcept {
            beu16 ttl_be = static_cast<uint32_t>(t.count());
            std::memcpy(data.data() + sizeof(std::uint16_t) * 2, &ttl_be,
                        sizeof(ttl_be));
        }

        /*!
         * @brief Get the length in bytes of the following RDATA
         * field.
         * @return The length in bytes of the following RDATA
         * field.
         */
        uint16_t data_length() const noexcept {
            beu16 len_be;
            std::memcpy(&len_be,
                        data.data() +
                            (sizeof(std::uint16_t) * 2 + sizeof(std::uint32_t)),
                        sizeof(len_be));
            return len_be;
        }

        /*!
         * @brief Set the length in bytes of the following RDATA
         * field.
         * @param l The length in bytes of the following RDATA
         * field.
         */
        void data_length(uint16_t l) noexcept {
            beu16 len_be = l;
            std::memcpy(data.data() +
                            (sizeof(std::uint16_t) * 2 + sizeof(std::uint32_t)),
                        &len_be, sizeof(len_be));
        }

    private:
        std::array<uint8_t, 10> data = {};
    };

    /*!
     * @brief Start of autority SOA RDATA format.
     */
    struct start_of_authority {
        /*!
         * @brief The domain name of the name server that was the original or
         * primary source of data for this zone.
         */
        std::string primary_name;
        /*!
         * @brief A domain name which specifies the mailbox of the person
         * responsible for this zone.
         */
        std::string responsible_name;
        /*!
         * @brief The unsigned 32 bit version number of the original copy of the
         * zone.
         */
        std::uint32_t serial = 0;
        /*!
         * @brief A 32 bit time interval before the zone should be refreshed.
         */
        std::chrono::seconds referesh = {};
        /*!
         * @brief A 32 bit time interval that should elapse before a failed
         * refresh should be retried.
         */
        std::chrono::seconds retry = {};
        /*!
         * @brief A 32 bit time interval that should elapse before a failed
         * refresh should be retried.
         */
        std::chrono::seconds expire = {};
        /*!
         * @brief A 32 bit time value that specifies the upper limit on the time
         * interval that can elapse before the zone is no longer authoritative.
         */
        std::chrono::seconds minimum = {};
    };

    class start_of_authority_raw {
    public:
        start_of_authority_raw() = default;

        start_of_authority_raw(const start_of_authority& soa) {
            serial(soa.serial);
            refresh(soa.referesh);
            retry(soa.retry);
            expire(soa.expire);
            minimum(soa.minimum);
        }

        start_of_authority decoded() const noexcept {
            start_of_authority soa;
            soa.serial = serial();
            soa.referesh = refresh();
            soa.retry = retry();
            soa.expire = expire();
            soa.minimum = minimum();
            return soa;
        }

        std::uint32_t serial() const noexcept {
            return data_[0];
        }

        void serial(std::uint32_t v) {
            data_[0] = v;
        }

        std::chrono::seconds refresh() const noexcept {
            return std::chrono::seconds{static_cast<uint32_t>(data_[1])};
        }

        void refresh(std::chrono::seconds v) {
            data_[1] = static_cast<uint32_t>(v.count());
        }

        std::chrono::seconds retry() const noexcept {
            return std::chrono::seconds{static_cast<uint32_t>(data_[2])};
        }

        void retry(std::chrono::seconds v) {
            data_[2] = static_cast<uint32_t>(v.count());
        }

        std::chrono::seconds expire() const noexcept {
            return std::chrono::seconds{static_cast<uint32_t>(data_[3])};
        }

        void expire(std::chrono::seconds v) {
            data_[3] = static_cast<uint32_t>(v.count());
        }

        std::chrono::seconds minimum() const noexcept {
            return std::chrono::seconds{static_cast<uint32_t>(data_[4])};
        }

        void minimum(std::chrono::seconds v) {
            data_[4] = static_cast<uint32_t>(v.count());
        }

    private:
        std::array<beu32, 5> data_ = {};
    };

    /*!
     * @brief Host information HINFO RDATA format.
     */
    struct host_info {
        /// A string which specifies the CPU type.
        std::string_view cpu;
        /// A string which specifies the operating system type.
        std::string_view os;
    };

    /*!
     * @brief Mail information MINFO RDATA format.
     */
    struct mail_info {
        /*!
         * @brief A domain name which specifies a mailbox which is responsible
         * for the mailing list or mailbox.
         */
        std::string responsible_mail;
        /*!
         * @brief A domain name which specifies a mailbox which is to receive
         * error messages related to the mailing list or mailbox specified by
         * the owner of the MINFO RR.
         */
        std::string error_mail;
    };

    /*!
     * @brief Mail exchange MX RDATA format.
     */
    struct mail_exchange {
        /*!
         * @brief A domain name which specifies a host willing to act as a mail
         * exchange for the owner name.
         */
        std::string exchange_host;
        /*!
         * @brief A 16 bit integer which specifies the preference given to this
         * RR among others at the same owner.  Lower values are preferred.
         */
        uint16_t preference = 0;
    };

    static_assert(sizeof(dns_header_raw) == 12,
                  "size of dns_header must be 12");
    static_assert(sizeof(question_section_raw) == 4,
                  "size of fixed dns question must be 4");
    static_assert(sizeof(resource_record_raw) == 10,
                  "size of fixed dns resource record must be 10");
    static_assert(sizeof(start_of_authority_raw) == 20,
                  "size of fixed SOA record must be 20");

    /*!
     * @brief The type of the resource record.
     */
    enum class resource_record_type {
        /// Answer resource record.
        answer,
        /// Authority resource record.
        authority,
        /// Additional resource record.
        additional,
    };

    /*!
     * @brief The handler interface used by DNS parse functions
     * to received parsed DNS message components as they get parsed.
     *
     * If any handler method returns false for the given parsed DNS
     * component, the parsing fails with error `std::errc::interrupted`.
     */
    struct RAD_EXPORT_VTABLE dns_parse_handler {

        RAD_EXPORT_DECL virtual ~dns_parse_handler();

        /*!
         * @brief Called after parsing of the DNS header.
         * @param header The decoded DNS header.
         * @return True to continue parsing, and false to interrupt parsing.
         */
        RAD_EXPORT_DECL virtual bool on_header(const dns_header& header);

        /*!
         * @brief Called on each parsed DNS question.
         * @param name The domain name (QNAME) of the question.
         * @param qestion The decoded fixed part of the question
         * which follows QNAME.
         * @return True to continue parsing, and false to interrupt parsing.
         */
        RAD_EXPORT_DECL virtual bool
        on_question(std::string_view name, const question_section& qestion);

        /*!
         * @brief Called on each parsed DNS resource record of type A (IPv4)
         * with the parsed IPv4 from RDATA.
         * @param rtype The type of this resource record, which indicates if
         * it is an answer, authority or additional resource record.
         * @param name The domain name (NAME) of the resource record.
         * @param record The decoded fixed part of the resource record
         * which follows NAME.
         * @param ip The IPv4 32 bit address contained in the RDATA of this
         * resource record.
         * @return True to continue parsing, and false to interrupt parsing.
         */
        RAD_EXPORT_DECL virtual bool
        on_ipv4_resource_record(resource_record_type rtype,
                                std::string_view name,
                                const resource_record& record, ipv4 ip);

        /*!
         * @brief Called on each parsed DNS resource record of type AAAA (IPv6)
         * with the parsed IPv6 from RDATA.
         * @param rtype The type of this resource record, which indicates if
         * it is an answer, authority or additional resource record.
         * @param name The domain name (NAME) of the resource record.
         * @param record The decoded fixed part of the resource record
         * which follows NAME.
         * @param ip The IPv6 128 bit address contained in the RDATA of this
         * resource record.
         * @return True to continue parsing, and false to interrupt parsing.
         */
        RAD_EXPORT_DECL virtual bool
        on_ipv6_resource_record(resource_record_type rtype,
                                std::string_view name,
                                const resource_record& record, ipv6 ip);

        /*!
         * @brief Called on each parsed DNS resource record of type CNAME
         * with the parsed canonical domain name from RDATA.
         * @param rtype The type of this resource record, which indicates if
         * it is an answer, authority or additional resource record.
         * @param name The domain name (NAME) of the resource record.
         * @param record The decoded fixed part of the resource record
         * which follows NAME.
         * @param cname The canonical domain name contained in the RDATA of this
         * resource record.
         * @return True to continue parsing, and false to interrupt parsing.
         */
        RAD_EXPORT_DECL virtual bool on_canonical_name_record(
            resource_record_type rtype, std::string_view name,
            const resource_record& record, std::string cname);

        /*!
         * @brief Called on each parsed DNS resource record of type SOA
         * (start of authority) with the parsed authority data from RDATA.
         * @param rtype The type of this resource record, which indicates if
         * it is an answer, authority or additional resource record.
         * @param name The domain name (NAME) of the resource record.
         * @param record The decoded fixed part of the resource record
         * which follows NAME.
         * @param soa The authority data contained in the RDATA of this
         * resource record.
         * @return True to continue parsing, and false to interrupt parsing.
         */
        RAD_EXPORT_DECL virtual bool on_start_of_authority_record(
            resource_record_type rtype, std::string_view name,
            const resource_record& record, start_of_authority& soa);

        /*!
         * @brief Called on each parsed DNS resource record of type HINFO
         * (host information) with the parsed host information from RDATA.
         * @param rtype The type of this resource record, which indicates if
         * it is an answer, authority or additional resource record.
         * @param name The domain name (NAME) of the resource record.
         * @param record The decoded fixed part of the resource record
         * which follows NAME.
         * @param hinfo The host information contained in the RDATA of this
         * resource record.
         * @return True to continue parsing, and false to interrupt parsing.
         */
        RAD_EXPORT_DECL virtual bool
        on_host_info_record(resource_record_type rtype, std::string_view name,
                            const resource_record& record,
                            const host_info& hinfo);

        /*!
         * @brief Called on each parsed DNS resource record of type TXT
         * (texts) with the parsed texts from RDATA.
         * @param rtype The type of this resource record, which indicates if
         * it is an answer, authority or additional resource record.
         * @param name The domain name (NAME) of the resource record.
         * @param record The decoded fixed part of the resource record
         * which follows NAME.
         * @param texts The texts contained in the RDATA of this
         * resource record.
         * @return True to continue parsing, and false to interrupt parsing.
         */
        RAD_EXPORT_DECL virtual bool
        on_texts_record(resource_record_type rtype, std::string_view name,
                        const resource_record& record,
                        std::span<std::string_view> texts);

        /*!
         * @brief Called on each parsed DNS resource record of type MINFO
         * (mail information) with the parsed mail information from RDATA.
         * @param rtype The type of this resource record, which indicates if
         * it is an answer, authority or additional resource record.
         * @param name The domain name (NAME) of the resource record.
         * @param record The decoded fixed part of the resource record
         * which follows NAME.
         * @param minfo The mail information contained in the RDATA of this
         * resource record.
         * @return True to continue parsing, and false to interrupt parsing.
         */
        RAD_EXPORT_DECL virtual bool
        on_mail_info_record(resource_record_type rtype, std::string_view name,
                            const resource_record& record, mail_info& minfo);

        /*!
         * @brief Called on each parsed DNS resource record of type MX
         * (mail exchange) with the parsed mail exchange from RDATA.
         * @param rtype The type of this resource record, which indicates if
         * it is an answer, authority or additional resource record.
         * @param name The domain name (NAME) of the resource record.
         * @param record The decoded fixed part of the resource record
         * which follows NAME.
         * @param mailx The mail exchange contained in the RDATA of this
         * resource record.
         * @return True to continue parsing, and false to interrupt parsing.
         */
        RAD_EXPORT_DECL virtual bool on_mail_exchange_record(
            resource_record_type rtype, std::string_view name,
            const resource_record& record, mail_exchange& mailx);

        /*!
         * @brief Called on each parsed DNS resource record whose RDATA
         * is defined to be a domain name, other than CNAME resource records.
         * @param rtype The type of this resource record, which indicates if
         * it is an answer, authority or additional resource record.
         * @param name The domain name (NAME) of the resource record.
         * @param record The decoded fixed part of the resource record
         * which follows NAME.
         * @param domain_name The domain name contained in the RDATA of this
         * resource record.
         * @return True to continue parsing, and false to interrupt parsing.
         */
        RAD_EXPORT_DECL virtual bool on_other_domain_name_records(
            resource_record_type rtype, std::string_view name,
            const resource_record& record, std::string domain_name);

        /*!
         * @brief Called on each parsed DNS resource record of type NULL, WKS
         * (well known service) or any other type not defined in the original
         * DNS RFC, with the raw resource record RDATA.
         * @param rtype The type of this resource record, which indicates if
         * it is an answer, authority or additional resource record.
         * @param name The domain name (NAME) of the resource record.
         * @param record The decoded fixed part of the resource record
         * which follows NAME.
         * @param rdata The raw resource record RDATA.
         * @return True to continue parsing, and false to interrupt parsing.
         */
        RAD_EXPORT_DECL virtual bool on_opaque_resource_record(
            resource_record_type rtype, std::string_view name,
            const resource_record& record, const_buffer rdata);
    };

    /*!
     * @brief Map a service name (http, https, ...) to its default numerical
     * port (80, 443, ...)
     * @param service The service string. If this string is the stringified
     * port number, then its converted to uint16_t and the result is
     * returned.
     * @param ec Cleared on success, and set to error on failure.
     * @return The port of the service.
     */
    RAD_EXPORT_DECL uint16_t service_to_port(std::string_view service,
                                             std::error_code& ec);

    /*!
     * @brief Encode a domain name in DNS wire format.
     * @param query_storage The encoded name will be appended to this
     * dynamic buffer.
     * @param name The domain name to encode.
     * @param ec Cleared on success, and set to error on failure.
     * @return The count of written bytes in @p query_storage.
     */
    RAD_EXPORT_DECL std::size_t encode_name(dynamic_buffer query_storage,
                                            std::string_view name,
                                            std::error_code& ec);

    /*!
     * @brief Encode a DNS query and append the wire format result to @p
     * query_storage
     * @param query_storage The output buffer where encoded DNS query will
     * be appended to.
     * @param name The queried host name to encode.
     * @param want_ipv4 Whether ipv4 or ipv6 result is wanted. If true it is
     * ipv4, and if false it is ipv6.
     * @param edns_size Whether to use EDNS extension to set the max udp
     * size. If not desired set to 0.
     * @param ec Cleared on success, and set to error on failure.
     * @return The offset to the start of the fixed part of DNS Question
     * Section which follows QNAME. This offset is relative to the start of
     * data in @p query_storage. On failure 0 is returned.
     */
    RAD_EXPORT_DECL uint16_t make_dns_query(dynamic_buffer query_storage,
                                            std::string_view name,
                                            bool want_ipv4, uint16_t edns_size,
                                            std::error_code& ec);

    /*!
     * @brief Parse DNS encoded characters string from @p input buffer.
     * @param input The input DNS buffer cotaining The characters string.
     * @param out The output characters string.
     * This output view points to @p input.
     * On error, this output view is set to empty string view.
     * @param ec Set to indicate error occured, if any.
     * @return The count of bytes consumed from @p input.
     */
    RAD_EXPORT_DECL std::size_t
    parse_characters_string(const_buffer input, std::string_view& out,
                            std::error_code& ec) noexcept;

    /*!
     * @brief Parse DNS encoded domain name from @p input buffer.
     * @param input The input DNS buffer cotaining The domain name.
     * @param whole_message The whole DNS message starting from the header.
     * It is needed to decompress the domain name, if it is compressed.
     * @param out The output string where parsed domain name will be appended.
     * Existing content in this output string is not cleared.
     * @param ec Set to indicate error occured, if any.
     * @return The count of bytes consumed from @p input.
     * The count may be less than size of the appended string to @p out if
     * compression is used.
     */
    RAD_EXPORT_DECL std::size_t parse_domain_name(const_buffer input,
                                                  const_buffer whole_message,
                                                  std::string& out,
                                                  std::error_code& ec);

    /*!
     * @brief Parse DNS QUESTION including the question domain name QNAME from
     * @p input buffer.
     * @param input The input DNS buffer cotaining the QUESTION section.
     * @param whole_message The whole DNS message starting from the header.
     * It is needed to decompress domain names, if they are compressed.
     * @param qname The output string where parsed QNAME will be appended.
     * Existing content in this output string is cleared.
     * @param handler The DNS parse handler to call its `on_question` method
     * after successful parsing.
     * @param ec Set to indicate error occured, if any.
     * @return The count of bytes consumed from @p input.
     */
    RAD_EXPORT_DECL std::size_t parse_question(const_buffer input,
                                               const_buffer whole_message,
                                               std::string& qname,
                                               dns_parse_handler& handler,
                                               std::error_code& ec);

    /*!
     * @brief Parse DNS RDATA of TXT (texts) resource record from @p rdata
     * buffer.
     * @param rtype The type of this resource record, which indicates if it is
     * an answer, authority or additional resource record.
     * @param rdata The RDATA buffer following the resource record.
     * @param name The domain name (NAME) of the resource record.
     * @param rr The decoded fixed part of the resource record which follows
     * NAME.
     * @param handler The DNS parse handler to call its `on_texts_record` method
     * after successful parsing.
     * @param ec Set to indicate error occured, if any.
     * @return The count of bytes consumed from @p rdata.
     * For successful parsing, the count of bytes returned will be equal to size
     * of @p rdata.
     */
    RAD_EXPORT_DECL std::size_t
    parse_texts_rdata(resource_record_type rtype, const_buffer rdata,
                      std::string_view name, const resource_record& rr,
                      dns_parse_handler& handler, std::error_code& ec);

    /*!
     * @brief Parse DNS RDATA of HINFO (host information) resource record from
     * @p rdata buffer.
     * @param rtype The type of this resource record, which indicates if it is
     * an answer, authority or additional resource record.
     * @param rdata The RDATA buffer following the resource record.
     * @param name The domain name (NAME) of the resource record.
     * @param rr The decoded fixed part of the resource record which follows
     * NAME.
     * @param handler The DNS parse handler to call its `on_host_info_record`
     * method after successful parsing.
     * @param ec Set to indicate error occured, if any.
     * @return The count of bytes consumed from @p rdata.
     * For successful parsing, the count of bytes returned will be equal to size
     * of @p rdata.
     */
    RAD_EXPORT_DECL std::size_t
    parse_host_info_rdata(resource_record_type rtype, const_buffer rdata,
                          std::string_view name, const resource_record& rr,
                          dns_parse_handler& handler, std::error_code& ec);

    /*!
     * @brief Parse DNS RDATA of MX (mail exchange) resource record from
     * @p rdata buffer.
     * @param rtype The type of this resource record, which indicates if it is
     * an answer, authority or additional resource record.
     * @param rdata The RDATA buffer following the resource record.
     * @param whole_message The whole DNS message starting from the header.
     * It is needed to decompress domain names, if they are compressed.
     * @param name The domain name (NAME) of the resource record.
     * @param rr The decoded fixed part of the resource record which follows
     * NAME.
     * @param handler The DNS parse handler to call its
     * `on_mail_exchange_record` method after successful parsing.
     * @param ec Set to indicate error occured, if any.
     * @return The count of bytes consumed from @p rdata.
     * For successful parsing, the count of bytes returned will be equal to size
     * of @p rdata.
     */
    RAD_EXPORT_DECL std::size_t
    parse_mail_exchange_rdata(resource_record_type rtype, const_buffer rdata,
                              const_buffer whole_message, std::string_view name,
                              const resource_record& rr,
                              dns_parse_handler& handler, std::error_code& ec);

    /*!
     * @brief Parse DNS RDATA of MINFO (mail information) resource record from
     * @p rdata buffer.
     * @param rtype The type of this resource record, which indicates if it is
     * an answer, authority or additional resource record.
     * @param rdata The RDATA buffer following the resource record.
     * @param whole_message The whole DNS message starting from the header.
     * It is needed to decompress domain names, if they are compressed.
     * @param name The domain name (NAME) of the resource record.
     * @param rr The decoded fixed part of the resource record which follows
     * NAME.
     * @param handler The DNS parse handler to call its
     * `on_mail_info_record` method after successful parsing.
     * @param ec Set to indicate error occured, if any.
     * @return The count of bytes consumed from @p rdata.
     * For successful parsing, the count of bytes returned will be equal to size
     * of @p rdata.
     */
    RAD_EXPORT_DECL std::size_t
    parse_mail_info_rdata(resource_record_type rtype, const_buffer rdata,
                          const_buffer whole_message, std::string_view name,
                          const resource_record& rr, dns_parse_handler& handler,
                          std::error_code& ec);

    /*!
     * @brief Parse DNS RDATA of SOA (start of authority) resource record from
     * @p rdata buffer.
     * @param rtype The type of this resource record, which indicates if it is
     * an answer, authority or additional resource record.
     * @param rdata The RDATA buffer following the resource record.
     * @param whole_message The whole DNS message starting from the header.
     * It is needed to decompress domain names, if they are compressed.
     * @param name The domain name (NAME) of the resource record.
     * @param rr The decoded fixed part of the resource record which follows
     * NAME.
     * @param handler The DNS parse handler to call its
     * `on_start_of_authority_record` method after successful parsing.
     * @param ec Set to indicate error occured, if any.
     * @return The count of bytes consumed from @p rdata.
     * For successful parsing, the count of bytes returned will be equal to size
     * of @p rdata.
     */
    RAD_EXPORT_DECL std::size_t parse_start_of_authority_rdata(
        resource_record_type rtype, const_buffer rdata,
        const_buffer whole_message, std::string_view name,
        const resource_record& rr, dns_parse_handler& handler,
        std::error_code& ec);

    /*!
     * @brief Parse DNS RDATA of A (IPv4) resource record from
     * @p rdata buffer.
     * @param rtype The type of this resource record, which indicates if it is
     * an answer, authority or additional resource record.
     * @param rdata The RDATA buffer following the resource record.
     * @param name The domain name (NAME) of the resource record.
     * @param rr The decoded fixed part of the resource record which follows
     * NAME.
     * @param handler The DNS parse handler to call its
     * `on_ipv4_resource_record` method after successful parsing.
     * @param ec Set to indicate error occured, if any.
     * @return The count of bytes consumed from @p rdata.
     * For successful parsing, the count of bytes returned will be equal to size
     * of @p rdata.
     */
    RAD_EXPORT_DECL std::size_t
    parse_ipv4_rdata(resource_record_type rtype, const_buffer rdata,
                     std::string_view name, const resource_record& rr,
                     dns_parse_handler& handler, std::error_code& ec);

    /*!
     * @brief Parse DNS RDATA of AAAA (IPv6) resource record from
     * @p rdata buffer.
     * @param rtype The type of this resource record, which indicates if it is
     * an answer, authority or additional resource record.
     * @param rdata The RDATA buffer following the resource record.
     * @param name The domain name (NAME) of the resource record.
     * @param rr The decoded fixed part of the resource record which follows
     * NAME.
     * @param handler The DNS parse handler to call its
     * `on_ipv6_resource_record` method after successful parsing.
     * @param ec Set to indicate error occured, if any.
     * @return The count of bytes consumed from @p rdata.
     * For successful parsing, the count of bytes returned will be equal to size
     * of @p rdata.
     */
    RAD_EXPORT_DECL std::size_t
    parse_ipv6_rdata(resource_record_type rtype, const_buffer rdata,
                     std::string_view name, const resource_record& rr,
                     dns_parse_handler& handler, std::error_code& ec);

    /*!
     * @brief Parse DNS RDATA that contains a domain name from @p rdata buffer.
     * @param rtype The type of this resource record, which indicates if it is
     * an answer, authority or additional resource record.
     * @param rdata The RDATA buffer following the resource record.
     * @param whole_message The whole DNS message starting from the header.
     * It is needed to decompress the domain name, if it is compressed.
     * @param record_name The domain name (NAME) of the resource record.
     * @param rr The decoded fixed part of the resource record which follows
     * NAME.
     * @param handler The DNS parse handler to call its
     * `on_canonical_name_record` or `on_other_domain_name_records` method after
     * successful parsing.
     * @param ec Set to indicate error occured, if any.
     * @return The count of bytes consumed from @p rdata.
     * For successful parsing, the count of bytes returned will be equal to size
     * of @p rdata.
     */
    RAD_EXPORT_DECL std::size_t
    parse_domain_name_rdata(resource_record_type rtype, const_buffer rdata,
                            const_buffer whole_message,
                            std::string_view record_name,
                            const resource_record& rr,
                            dns_parse_handler& handler, std::error_code& ec);

    /*!
     * @brief Parse one DNS resource record (RR) including record domain name
     * (NAME) from @p input buffer.
     * @param rtype The type of this resource record, which indicates if it is
     * an answer, authority or additional resource record.
     * @param input The input DNS buffer cotaining the resource record.
     * @param whole_message The whole DNS message starting from the header.
     * It is needed to decompress domain names, if they are compressed.
     * @param record_name The output string where parsed NAME will be appended.
     * Existing content in this output string is cleared.
     * @param handler The DNS parse handler to notify it with the parsed record
     * after successful parsing.
     * @param ec Set to indicate error occured, if any.
     * @return The count of bytes consumed from @p input.
     */
    RAD_EXPORT_DECL std::size_t
    parse_resource_record(resource_record_type rtype, const_buffer input,
                          const_buffer whole_message, std::string& record_name,
                          dns_parse_handler& handler, std::error_code& ec);

    /*!
     * @brief Parse multiple DNS resource records (RR) including records domain
     * name (NAME) from @p input buffer.
     * @param rtype The type of these resource records, which indicates if they
     * are answer, authority or additional resource records.
     * @param records_count The count of resource records to parse.
     * @param input The input DNS buffer cotaining the resource records.
     * @param whole_message The whole DNS message starting from the header.
     * It is needed to decompress domain names, if they are compressed.
     * @param record_name The output string where each parsed NAME will be
     * appended. Existing content in this output string is cleared. Reusing this
     * string for several records saves allocation costs. After return, the
     * content of this output string is cleared.
     * @param handler The DNS parse handler to notify it with each parsed record
     * after each successful parsing.
     * @param ec Set to indicate error occured, if any.
     * @return The count of bytes consumed from @p input.
     */
    RAD_EXPORT_DECL std::size_t
    parse_resource_records(resource_record_type rtype, uint16_t records_count,
                           const_buffer input, const_buffer whole_message,
                           std::string& record_name, dns_parse_handler& handler,
                           std::error_code& ec);

    /*!
     * @brief Parse a DNS message from @p input buffer.
     * The message includes the DNS HEADER, QUESTION sections and resource
     * records (RRs) including answer, authority and additional resource
     * records.
     * @param input The input DNS buffer cotaining the message.
     * The buffer must contain a complete message, and any trailing data after
     * the complete message is treated as an error.
     * @param handler The DNS parse handler to notify it with the parsed header,
     * questions and resource records.
     * @param ec Set to indicate error occured, if any.
     * @return The count of bytes consumed from @p input.
     * For successful parsing, the count of bytes returned will be equal to size
     * of @p input.
     */
    RAD_EXPORT_DECL std::size_t parse_dns_message(const_buffer input,
                                                  dns_parse_handler& handler,
                                                  std::error_code& ec);

    /*!
     * @brief Check if two domain names are equal using case insensetive
     * comparison, and taking account of prefix wildcard, if any.
     * @param domain1 The first domain name.
     * @param domain2 The second domain name.
     * @return True if the two domains are equal, otherwise false.
     */
    RAD_EXPORT_DECL bool compare_two_domains(std::string_view domain1,
                                             std::string_view domain2) noexcept;

    /*!
     * @brief Cached DNS CNAME result.
     */
    class cache_canonical_name_entry {
    public:
        /*!
         * @brief Construct a cache canonical name entry.
         * @param alias The alias domain name.
         * @param cname The canonical domain name.
         * @param time The time (in seconds) to cache the result
         * for.
         */
        cache_canonical_name_entry(std::string_view alias,
                                   std::string_view cname,
                                   std::chrono::seconds time) noexcept
            : expiry_time{std::chrono::steady_clock::now() + time},
              alias_{alias}, cname_{cname} {
        }

        /*!
         * @brief Check if this cache entry has expired.
         * @param now The current clock time.
         * @return True if this cache entry has expired, and
         * false otherwise.
         */
        bool expired(std::chrono::steady_clock::time_point now) const noexcept {
            return now >= expiry_time;
        }

        /*!
         * @brief Check if this cache entry has expired.
         * @return true if this cache entry has expired, and
         * false otherwise.
         */
        bool expired() const noexcept {
            return std::chrono::steady_clock::now() >= expiry_time;
        }

        /*!
         * @brief Check if this cache entry matches the wanted
         * host and address family.
         * @param host The host to match with.
         * @param af The address family to match with.
         * @return true if the cache entry matches, and false
         * otherwise.
         */
        bool matches(std::string_view host) const noexcept {
            return compare_two_domains(alias_, host);
        }

        /*!
         * @brief Get a view to the canonical name.
         * @return A view to the canonical name.
         */
        std::string_view canonical_name() const noexcept {
            return cname_;
        }

        friend bool operator<(const cache_canonical_name_entry& lhs,
                              const cache_canonical_name_entry& rhs) noexcept {
            // don't include expiry_time in the comparison,
            // so a recent resolve ttl overrides the old one
            if (lhs.alias_ < rhs.alias_) {
                return true;
            }
            else if (lhs.alias_ > rhs.alias_) {
                return false;
            }
            return lhs.cname_ < rhs.cname_;
        }

    private:
        std::chrono::steady_clock::time_point expiry_time;
        std::string alias_;
        std::string cname_;
    };

    /*!
     * @brief Cached DNS IPv4 or IPv6 result.
     */
    class cache_ip_entry {
        struct address_storage {
            union {
                ipv4 v4addr;
                ipv6 v6addr;
            };
            bool is_v4;

            address_storage(const endpoint& address) noexcept
                : is_v4{address.is_v4()} {
                if (address.is_v4()) {
                    v4addr = address.as_ipv4().ip();
                }
                else {
                    v6addr = address.as_ipv6().ip();
                }
            }

            endpoint to_endpoint(uint16_t port) const noexcept {
                if (is_v4) {
                    return endpoint{v4addr, port};
                }
                else {
                    return endpoint{v6addr, port};
                }
            }

            friend bool operator<(const address_storage& lhs,
                                  const address_storage& rhs) noexcept {
                return lhs.to_endpoint(0) < rhs.to_endpoint(0);
            }
        };

    public:
        /*!
         * @brief Construct a cached DNS lookup result.
         * @param host The host associated with the endpoint
         * address.
         * @param address The resolved address of the host. The
         * port in the address is not cached.
         * @param time The time (in seconds) to cache the result
         * for.
         */
        cache_ip_entry(std::string_view host, const endpoint& address,
                       std::chrono::seconds time) noexcept
            : expiry_time{std::chrono::steady_clock::now() + time},
              hostname{host}, address{address} {
        }

        /*!
         * @brief Check if this cache entry has expired.
         * @param now The current clock time.
         * @return True if this cache entry has expired, and
         * false otherwise.
         */
        bool expired(std::chrono::steady_clock::time_point now) const noexcept {
            return now >= expiry_time;
        }

        /*!
         * @brief Check if this cache entry has expired.
         * @return true if this cache entry has expired, and
         * false otherwise.
         */
        bool expired() const noexcept {
            return std::chrono::steady_clock::now() >= expiry_time;
        }

        /*!
         * @brief Check if this cache entry matches the wanted
         * host and address family.
         * @param host The host to match with.
         * @param af The address family to match with.
         * @return true if the cache entry matches, and false
         * otherwise.
         */
        bool matches(std::string_view host, address_family af) const noexcept {
            address_family stored_af =
                address.is_v4 ? address_family::ipv4 : address_family::ipv6;
            return (af == stored_af || af == address_family::unspecified) &&
                   compare_two_domains(hostname, host);
        }

        /*!
         * @brief Make an endpoint using the cached address and
         * the provided port.
         * @param port The port to add to the endpoint.
         * @return An endpoint composed of the cached address
         * and the provided port.
         */
        endpoint to_endpoint(uint16_t port) const noexcept {
            return address.to_endpoint(port);
        }

        friend bool operator<(const cache_ip_entry& lhs,
                              const cache_ip_entry& rhs) noexcept {
            // don't include expiry_time in the comparison,
            // so a recent resolve ttl overrides the old one
            return lhs.hostname < rhs.hostname && lhs.address < rhs.address;
        }

    private:
        std::chrono::steady_clock::time_point expiry_time;
        std::string hostname;
        address_storage address;
    };

    /*!
     * @brief Thread safe cache storage to store and retieve DNS lookup
     * results.
     */
    class cache_storage {
    public:
        /*!
         * @brief Add an ip address lookup result to the caches.
         * @param hostname The queried host name.
         * @param address The resolved ip address. The port in
         * the address is not cached.
         * @param time  The time (in seconds) to cache the result for.
         */
        RAD_EXPORT_DECL void add(std::string_view hostname,
                                 const endpoint& address,
                                 std::chrono::seconds time);

        /*!
         * @brief Add a canonical name lookup result to the caches.
         * @param alias The alias host name.
         * @param cname The canonical name.
         * @param time The time (in seconds) to cache the result for.
         */
        RAD_EXPORT_DECL void add(std::string_view alias, std::string_view cname,
                                 std::chrono::seconds time);

        /*!
         * @brief Search for a cached result matching the host
         * name and the address family. Expired caches are not
         * returned in the results.
         * @param hostname The host name to search for.
         * @param af The address family to search for.
         * @param port The port to append to the search result
         * endpoints.
         * @param results The matched search results will be
         * appended to this vector.
         * @return true if matches were found and appnded to the
         * results, and false otherwise.
         */
        RAD_EXPORT_DECL bool find(std::string_view hostname, address_family af,
                                  uint16_t port,
                                  std::vector<endpoint>& results);

        /*!
         * @brief Clear all stored caches.
         */
        RAD_EXPORT_DECL void clear() noexcept;

        /*!
         * @brief Remove expired cache entries.
         */
        RAD_EXPORT_DECL void clear_expired() noexcept;

    private:
        static const cache_canonical_name_entry*
        find_cname(std::chrono::steady_clock::time_point now,
                   std::vector<cache_canonical_name_entry>& cnames,
                   std::string_view host);

        struct cache_store {
            std::vector<cache_ip_entry> addresses;
            std::vector<cache_canonical_name_entry> cnames;
        };

        sync_value<cache_store> caches_;
    };

    /*!
     * @brief DNS parse handler that handlers IPv4, IPv6 and canonical names
     * answer records and optionally insert them in the provided cache store.
     */
    class RAD_EXPORT_VTABLE dns_ip_answers_handler : public dns_parse_handler {
    public:
        /*!
         * @brief Construct the handler with requested host name, output
         * endpoint results, requested port and optional cache storage.
         * @param host The requested host name.
         * A copy of this name is not made, so it must be valid as long as it is
         * used by the handler.
         * @param results The output list where results will be appended.
         * This list must be valid as long as it is used by the handler.
         * @param port The requested port.
         * @param cache An optional pointer to a cache store to cache ip and
         * caononical names results as they are received.
         */
        dns_ip_answers_handler(std::string_view host,
                               std::vector<endpoint>& results,
                               std::uint16_t port,
                               cache_storage* cache = nullptr) noexcept
            : cache_{cache}, host_{host}, results_{results}, port_{port} {
        }

        RAD_EXPORT_DECL ~dns_ip_answers_handler();

        /*!
         * @brief Move the last canonicnal name, if any, from the handler.
         * The canonical name must not be moved before parse completion,
         * otherwise results will not be handled correctly.
         * @return The last canonicnal name, which can be empty if no canonical
         * name was received.
         */
        std::string take_canonicnal_name() noexcept {
            cache_ = nullptr;
            return std::move(cname_);
        }

        /*!
         * @brief Called on each parsed DNS resource record of type A (IPv4)
         * with the parsed IPv4 from RDATA.
         * @param rtype The type of this resource record, which indicates if
         * it is an answer, authority or additional resource record.
         * @param name The domain name (NAME) of the resource record.
         * @param record The decoded fixed part of the resource record
         * which follows NAME.
         * @param ip The IPv4 32 bit address contained in the RDATA of this
         * resource record.
         * @return True to continue parsing, and false to interrupt parsing.
         */
        RAD_EXPORT_DECL bool on_ipv4_resource_record(
            resource_record_type rtype, std::string_view name,
            const resource_record& record, ipv4 ip) override;

        /*!
         * @brief Called on each parsed DNS resource record of type AAAA (IPv6)
         * with the parsed IPv6 from RDATA.
         * @param rtype The type of this resource record, which indicates if
         * it is an answer, authority or additional resource record.
         * @param name The domain name (NAME) of the resource record.
         * @param record The decoded fixed part of the resource record
         * which follows NAME.
         * @param ip The IPv6 128 bit address contained in the RDATA of this
         * resource record.
         * @return True to continue parsing, and false to interrupt parsing.
         */
        RAD_EXPORT_DECL bool on_ipv6_resource_record(
            resource_record_type rtype, std::string_view name,
            const resource_record& record, ipv6 ip) override;

        /*!
         * @brief Called on each parsed DNS resource record of type CNAME
         * with the parsed canonical domain name from RDATA.
         * @param rtype The type of this resource record, which indicates if
         * it is an answer, authority or additional resource record.
         * @param name The domain name (NAME) of the resource record.
         * @param record The decoded fixed part of the resource record
         * which follows NAME.
         * @param cname The canonical domain name contained in the RDATA of this
         * resource record.
         * @return True to continue parsing, and false to interrupt parsing.
         */
        RAD_EXPORT_DECL bool on_canonical_name_record(
            resource_record_type rtype, std::string_view name,
            const resource_record& record, std::string cname) override;

    private:
        cache_storage* cache_ = nullptr;
        std::string_view host_;
        std::string cname_;
        std::vector<endpoint>& results_;
        std::uint16_t port_;
    };
} // namespace RAD_LIB_NAMESPACE::net::dns

namespace RAD_LIB_NAMESPACE::net::dns::detail {
    template <class Handler>
    concept resolver_handler1 = requires(Handler handler) {
        handler(std::error_code{}, std::vector<endpoint>{});
    };

    // handles cname
    template <class Handler>
    concept resolver_handler2 = requires(Handler handler) {
        handler(std::error_code{}, std::vector<endpoint>{}, std::string{});
    };

    template <class Handler>
    concept ResolverHandler =
        resolver_handler1<Handler> || resolver_handler2<Handler>;

    template <bool is_cname>
    struct handler_data_storage;

    template <>
    struct handler_data_storage<false> {
        std::error_code ec;
        std::vector<endpoint> results;

        template <class... Args>
        void store(const std::error_code& ec, std::vector<endpoint> results,
                   Args&&... args) noexcept {
            this->ec = ec;
            this->results = std::move(results);
        }

        template <class Ctx>
        void invoke(Ctx* ctx) {
            details::invoke_handler(ctx, std::error_code{ec},
                                    std::vector<endpoint>{std::move(results)});
        }
    };

    template <>
    struct handler_data_storage<true> {
        std::error_code ec;
        std::vector<endpoint> results;
        std::string cname;

        void store(const std::error_code& ec, std::vector<endpoint> results,
                   std::string cname) noexcept {
            this->ec = ec;
            this->results = std::move(results);
            this->cname = std::move(cname);
        }

        template <class Ctx>
        void invoke(Ctx* ctx) {
            details::invoke_handler(ctx, std::error_code{ec},
                                    std::vector<endpoint>{std::move(results)},
                                    std::string{std::move(cname)});
        }
    };

    template <bool is_cname>
    struct cname_holder;

    template <>
    struct cname_holder<false> {
        template <class... Args>
        void store(Args&&... args) const noexcept {
        }

        std::vector<endpoint> get_result(std::vector<endpoint>& results) {
            return std::move(results);
        }
    };

    template <>
    struct cname_holder<true> {
        std::string cname;

        void store(std::string cname) noexcept {
            this->cname = std::move(cname);
        }

        std::pair<std::vector<endpoint>, std::string>
        get_result(std::vector<endpoint>& results) {
            return std::pair(std::move(results), std::move(cname));
        }
    };

    struct hold_cname_t {
        static constexpr bool cname = true;
    };

    struct no_cname_t {
        static constexpr bool cname = false;
    };

    struct handler_base {
        bool canon_name;

        handler_base(bool cname) noexcept : canon_name{cname} {
        }

        RAD_EXPORT_DECL virtual ~handler_base() = 0;

        virtual void invoke_resolver(const std::error_code& ec,
                                     std::vector<endpoint> results,
                                     std::string cname) = 0;
    };

    template <class Handler, class Alloc, bool is_cname>
    struct resolver_handler final : public handler_base,
                                    allocator_storage<Alloc> {
        using alloc_base = allocator_storage<Alloc>;
        using handler_type = std::decay_t<Handler>;

        handler_type handler;

        resolver_handler(Handler&& handler, const Alloc& alloc)
            : handler_base(is_cname), alloc_base(alloc),
              handler{std::forward<Handler>(handler)} {
        }

        void invoke_resolver(const std::error_code& ec,
                             std::vector<endpoint> results,
                             std::string cname) override {
            if constexpr (is_cname) {
                details::invoke_handler(this, ec, std::move(results),
                                        std::move(cname));
            }
            else {
                details::invoke_handler(this, ec, std::move(results));
            }
        }
    };

    template <class Resolver, bool is_cname>
    struct [[nodiscard]] resolver_awaiter final : public handler_base,
                                                  error_storage,
                                                  cname_holder<is_cname> {
        using error_base = error_storage;
        using cname_base = cname_holder<is_cname>;
        using handle_type = std::coroutine_handle<>;

        template <class ResolverType, class CnameDeduce>
        resolver_awaiter(CnameDeduce, ResolverType& r,
                         std::error_code& ec = no_ec) noexcept
            : handler_base(is_cname), error_base(ec), r{r} {
        }

        void invoke_resolver(const std::error_code& ec,
                             std::vector<endpoint> results,
                             std::string cname) override {
            error_base::store(ec);
            this->results = std::move(results);
            cname_base::store(std::move(cname));

            awaiting_coro.resume();
        }

        bool await_ready() const noexcept {
            return error_base::has_error();
        }

        bool await_suspend(handle_type coro) {
            awaiting_coro = coro;
            bool finished = r.start_coro(*this, results);
            return !finished;
        }

        auto await_resume() {
            error_base::raise("async_resolve");
            return cname_base::get_result(results);
        }

    private:
        Resolver& r;
        handle_type awaiting_coro;
        std::vector<endpoint> results;
    };

    template <class ResolverType, class CnameDeduce>
    resolver_awaiter(CnameDeduce, ResolverType& r, std::error_code& ec)
        -> resolver_awaiter<ResolverType, CnameDeduce::cname>;
} // namespace RAD_LIB_NAMESPACE::net::dns::detail