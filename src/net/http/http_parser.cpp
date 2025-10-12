#include <rad/match.h>
#include <rad/net/http/http_parser.h>
#include <rad/views/enumerate.h>

using namespace RAD_LIB_NAMESPACE;
using namespace net;
using namespace http;
using namespace std::string_view_literals;

namespace {
    bool is_http_sp(char ch) {
        return ch == ' ';
    }

    bool is_http_ws(char ch) {
        return is_http_sp(ch) || ch == '\t';
    }

    bool is_http_digit(char ch) {
        return ch >= '0' && ch <= '9';
    }

    bool is_http_hex_digit(char ch) {
        return is_http_digit(ch) || (ch >= 'a' && ch <= 'f') ||
               (ch >= 'A' && ch <= 'F');
    }

    bool is_http_tchar(char ch) {
        return (ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'z') ||
               (ch >= 'A' && ch <= 'Z') || ch == '!' || ch == '#' ||
               ch == '$' || ch == '%' || ch == '&' || ch == '\'' || ch == '*' ||
               ch == '+' || ch == '-' || ch == '.' || ch == '^' || ch == '_' ||
               ch == '`' || ch == '|' || ch == '~';
    }

    // http visible chars
    bool is_http_vchar(char ch) {
        return ch >= ' ' && ch <= '~';
    }

    bool is_http_obs_text(uint8_t ch) {
        // obs-text       = %x80-FF
        return ch >= 0x80 && ch <= 0xff;
    }

    bool is_http_reason_phrase(char ch) {
        // reason-phrase  = 1*( HTAB / SP / VCHAR / obs-text )
        return ch == '\t' || ch == ' ' || is_http_vchar(ch) ||
               is_http_obs_text(ch);
    }

    bool is_field_vchar(char ch) {
        // field-vchar    = VCHAR / obs-text
        // http obs-text is obselete.
        return is_http_vchar(ch) || is_http_obs_text(ch);
    }

    bool is_field_content(std::string_view previous, char ch) {
        // field-content  = field-vchar[1 * (SP / HTAB / field-vchar)
        // field-vchar]
        if (is_field_vchar(ch)) {
            return true;
        }
        if (previous.empty()) {
            return false;
        }
        return is_http_sp(ch) || ch == '\t';
    }

    verb parse_http_method(ring_consumer& input, std::error_code& ec) {
        // shortest method is 3 (GET, PUT) and longest method is 7
        // (OPTIONS, CONNECT)
        const auto http_verbs = http::detail::get_http_verbs();
        std::array<char, 7> method_buff;
        const std::size_t method_len = input.peek(buffer(method_buff));
        assert(method_len > 0);
        const std::string_view method_str{method_buff.data(), method_len};
        // find exact match
        auto it = std::find_if(std::begin(http_verbs), std::end(http_verbs),
                               [method_str](std::string_view v) {
                                   return method_str.starts_with(v);
                               });
        if (it != std::end(http_verbs)) {
            // found a complete method
            input.consume(it->size());
            return string_to_verb(*it);
        }
        // check if it is a method prefix
        it = std::find_if(std::begin(http_verbs), std::end(http_verbs),
                          [method_str](std::string_view v) {
                              return v.starts_with(method_str);
                          });
        if (it == std::end(http_verbs)) {
            ec = make_error(error::bad_method);
        }
        return verb::invalid;
    }

    bool parse_http_slash(ring_consumer& input, std::error_code& ec) {
        ec.clear();
        constexpr auto HTTPSlash = "HTTP/"sv;
        std::array<char, HTTPSlash.size()> read_buff;
        const std::size_t read_len = input.peek(buffer(read_buff));
        assert(read_len > 0);
        const std::string_view http_slash_str{read_buff.data(), read_len};
        if (http_slash_str.size() < HTTPSlash.size()) {
            if (!HTTPSlash.starts_with(http_slash_str)) {
                ec = make_error(error::bad_version);
            }
            return false;
        }
        if (http_slash_str != HTTPSlash) {
            ec = make_error(error::bad_version);
            return false;
        }
        input.consume(read_len);
        return true;
    }

    version parse_http_version(ring_consumer& input, std::error_code& ec) {
        // version: 1.1 or 1.0
        ec.clear();
        std::array<char, 3> version_buff;
        const std::size_t version_len = input.peek(buffer(version_buff));
        assert(version_len > 0);
        std::string_view version_str{version_buff.data(), version_len};
        if (version_str[0] != '1') {
            ec = make_error(error::bad_version);
            return version::invalid;
        }
        if (version_str.size() >= 2 && version_str[1] != '.') {
            ec = make_error(error::bad_version);
            return version::invalid;
        }
        if (version_str.size() < 3) {
            return version::invalid;
        }
        assert(version_str.size() == 3);
        const version v = string_to_version(version_str);
        if (v == version::invalid) {
            ec = make_error(error::bad_version);
            return v;
        }
        input.consume(version_len);
        return v;
    }

    // returns -1 on incomplete status with ec cleared
    int32_t parse_http_status(ring_consumer& input, std::error_code& ec) {
        ec.clear();
        // status is 3 digits
        std::array<char, 3> status_buff;
        const std::size_t status_len = input.peek(buffer(status_buff));
        assert(status_len > 0);
        if (status_buff[0] == '0') {
            ec = make_error(error::bad_status);
            return -1;
        }
        if (status_len < 3) {
            for (std::size_t i = 0; i < status_len; ++i) {
                if (!(status_buff[i] >= '0' && status_buff[i] <= '9')) {
                    ec = make_error(error::bad_status);
                    break;
                }
            }
            return -1;
        }
        uint32_t status_code =
            to_uint32(std::string_view{status_buff.data(), status_len}, 10, ec);
        if (ec) {
            ec = make_error(error::bad_status);
            return -1;
        }
        input.consume(status_len);
        return static_cast<int32_t>(status_code);
    }

    bool validate_target(std::string_view text, size_t& n, std::string& target,
                         std::error_code& ec) {
        for (auto ch : text) {
            n += 1;
            if (ch == ' ') {
                return true;
            }
            if (!is_http_vchar(ch)) {
                n -= 1;
                ec = make_error(error::bad_target);
                return false;
            }
            target.push_back(ch);
        }
        return false;
    }

    bool parse_target_and_sp(ring_consumer& input, std::string& target,
                             std::error_code& ec) {
        auto buffs = input.available_buffers();
        auto text1 = buffs[0].to_string_view();
        auto text2 = buffs[1].to_string_view();
        size_t consumed = 0;
        bool finished = validate_target(text1, consumed, target, ec);
        if (ec) {
            return false;
        }
        if (!finished) {
            finished = validate_target(text2, consumed, target, ec);
        }
        if (ec) {
            return false;
        }
        input.consume(consumed);
        return finished;
    }

    bool validate_reason_phrase(std::string_view text, size_t& n,
                                std::string& phrase, std::error_code& ec) {
        for (auto ch : text) {
            n += 1;
            if (!is_http_reason_phrase(ch)) {
                if (ch != '\r') {
                    n -= 1;
                    ec = make_error(error::bad_reason);
                    return false;
                }
                return true;
            }
            phrase.push_back(ch);
        }
        return false;
    }

    bool parse_reason_phrase_and_cr(ring_consumer& input, std::string& phrase,
                                    std::error_code& ec) {
        // field-name = token
        // token	  = 1*tchar
        auto buffs = input.available_buffers();
        auto text1 = buffs[0].to_string_view();
        auto text2 = buffs[1].to_string_view();
        size_t consumed = 0;
        bool finished = validate_reason_phrase(text1, consumed, phrase, ec);
        if (ec) {
            return false;
        }
        if (!finished) {
            finished = validate_reason_phrase(text2, consumed, phrase, ec);
        }
        if (ec) {
            return false;
        }
        input.consume(consumed);
        return finished;
    }

    void validate_target(verb method, std::string_view target,
                         std::error_code& ec) {
        if (method != verb::connect) {
            return;
        }
        const std::size_t colon_pos = target.find(':');
        if (colon_pos == std::string_view::npos || colon_pos == 0 ||
            colon_pos == target.size()) {
            ec = make_error(error::bad_target);
            return;
        }
        std::string_view host_part = target.substr(0, colon_pos);
        std::string_view port_part = target.substr(colon_pos + 1);
        to_uint16(port_part, 10, ec);
        if (ec) {
            ec = make_error(error::bad_target);
            return;
        }
        if (host_part.front() == '-' || host_part.back() == '-') {
            ec = make_error(error::bad_target);
            return;
        }
    }

    bool validate_field_name(std::string_view text, size_t& n,
                             std::string& name, std::error_code& ec) {
        for (auto ch : text) {
            n += 1;
            if (!is_http_tchar(ch)) {
                if (ch != ':') {
                    n -= 1;
                    ec = make_error(error::bad_field);
                    return false;
                }
                return true;
            }
            name.push_back(ch);
        }
        return false;
    }

    bool validate_field_value(std::string_view text, size_t& n,
                              std::string& value, std::error_code& ec) {
        // field-value    = *field-content
        // note that a space is also a field-content
        for (auto ch : text) {
            if (!is_field_content(value, ch)) {
                if (ch != '\r') {
                    ec = make_error(error::bad_value);
                    return false;
                }
                return true;
            }
            n += 1;
            value.push_back(ch);
        }
        return false;
    }

    bool parse_field_name_and_colon(ring_consumer& input, std::string& name,
                                    std::error_code& ec) {
        // field-name = token
        // token	  = 1*tchar
        auto buffs = input.available_buffers();
        auto text1 = buffs[0].to_string_view();
        auto text2 = buffs[1].to_string_view();
        size_t consumed = 0;
        bool finished = validate_field_name(text1, consumed, name, ec);
        if (ec) {
            return false;
        }
        if (!finished) {
            finished = validate_field_name(text2, consumed, name, ec);
        }
        if (ec) {
            return false;
        }
        input.consume(consumed);
        return finished;
    }

    bool consume_ows(ring_consumer& input) {
        while (!input.empty()) {
            char ch = 0;
            input.peek(buffer(&ch, sizeof(ch)));
            if (is_http_ws(ch)) {
                input.consume(1);
                continue;
            }
            return true;
        }
        return false;
    }

    bool parse_field_value(ring_consumer& input, std::string& value,
                           std::error_code& ec) {
        auto buffs = input.available_buffers();
        auto text1 = buffs[0].to_string_view();
        auto text2 = buffs[1].to_string_view();
        size_t consumed = 0;
        bool finished = validate_field_value(text1, consumed, value, ec);
        if (ec) {
            return false;
        }
        if (!finished) {
            finished = validate_field_value(text2, consumed, value, ec);
        }
        if (ec) {
            return false;
        }
        input.consume(consumed);
        return finished;
    }

    bool validate_ext_token_name_value(std::string_view text, size_t& n,
                                       std::string& name) {
        // chunk-ext-name = token
        // token          = 1*tchar
        for (auto ch : text) {
            if (!is_http_tchar(ch)) {
                return true;
            }
            n += 1;
            name.push_back(ch);
        }
        return false;
    }

    bool parse_ext_token_name_value(ring_consumer& input, std::string& name) {
        auto buffs = input.available_buffers();
        auto text1 = buffs[0].to_string_view();
        auto text2 = buffs[1].to_string_view();
        size_t consumed = 0;
        bool finished = validate_ext_token_name_value(text1, consumed, name);
        if (!finished) {
            finished = validate_ext_token_name_value(text2, consumed, name);
        }
        input.consume(consumed);
        return finished;
    }

    bool validate_qdtext(std::string_view text, size_t& n, std::string& out,
                         std::error_code& ec) {
        // quoted-string  = DQUOTE *( qdtext / quoted-pair ) DQUOTE
        // qdtext         = HTAB / SP / %x21 / %x23-5B / %x5D-7E /
        // obs-text
        for (char ch : text) {
            if (ch == '"') {
                n += 1;
                return true;
            }
            if (ch == '\t' || ch == ' ' || ch == '\x21' ||
                (ch >= '\x23' && ch <= '\x5B') ||
                (ch >= '\x5D' && ch <= '\x7E') || is_http_obs_text(ch)) {
                out += ch;
                n += 1;
                continue;
            }
            ec = make_error(error::bad_chunk_extension);
            return true;
        }
        return false;
    }

    bool parse_qdtext(ring_consumer& input, std::string& out,
                      std::error_code& ec) {
        auto buffs = input.available_buffers();
        auto text1 = buffs[0].to_string_view();
        auto text2 = buffs[1].to_string_view();
        size_t consumed = 0;
        bool finished = validate_qdtext(text1, consumed, out, ec);
        if (ec) {
            return false;
        }
        if (!finished) {
            finished = validate_qdtext(text2, consumed, out, ec);
        }
        if (ec) {
            return false;
        }
        input.consume(consumed);
        return finished;
    }
} // namespace

void http::parse_transfer_encoding(std::string_view transfer_encoding,
                                   std::vector<std::string_view>& encodings) {
    /*
     * TE                 = #t-codings
     * t-codings          = "trailers" / ( transfer-coding [ weight ] )
     * transfer-coding    = token *( OWS ";" OWS transfer-parameter )
     * transfer-parameter = token BWS "=" BWS ( token / quoted-string )
     * weight = OWS ";" OWS "q=" qvalue
     * qvalue = ( "0" [ "." 0*3DIGIT ] ) / ( "1" [ "." 0*3("0") ] )
     */
    // Transfer-Encoding, unlike TE, does not have wieghts
    for (auto tcoding : transfer_encoding | split(",")) {
        while (!tcoding.empty() && is_http_ws(tcoding.front())) {
            tcoding.remove_prefix(1);
        }
        while (!tcoding.empty() && is_http_ws(tcoding.back())) {
            tcoding.remove_suffix(1);
        }
        if (tcoding.empty()) {
            continue;
        }
        encodings.emplace_back(tcoding);
    }
}

std::size_t http::parse_request_line(ring_consumer& input,
                                     parse_request_line_context& ctx,
                                     std::error_code& ec) noexcept {
    /*
    request-line   = method SP request-target SP HTTP-version
    */
    using parse_stage = parse_request_line_context::parse_stage;
    ec.clear();
    if (ctx.done()) {
        return 0;
    }
    if (ctx.error()) {
        ec = make_error(ctx.last_ec);
        return 0;
    }
    const std::size_t input_size = input.size();
    while (!input.empty() && !ec) {
        if (ctx.stage == parse_stage::method) {
            const verb method = parse_http_method(input, ec);
            if (ec || method == verb::invalid) {
                break;
            }
            ctx.method = method;
            ctx.stage = parse_stage::sp;
            ctx.next_stage = parse_stage::target_start;
            continue;
        }
        else if (ctx.stage == parse_stage::sp) {
            char sp_ch = 0;
            input.peek(buffer(&sp_ch, 1));
            if (!is_http_sp(sp_ch)) {
                ec = make_error(error::bad_method);
                break;
            }
            input.consume(1);
            ctx.stage = ctx.next_stage;
            continue;
        }
        else if (ctx.stage == parse_stage::target_start) {
            char ch = 0;
            input.peek(buffer(&ch, sizeof(ch)));
            if ((ch == '*' && ctx.method != verb::options) ||
                (ch == '/' && ctx.method == verb::connect) ||
                !is_http_vchar(ch)) {
                ec = make_error(error::bad_target);
                break;
            }
            input.consume(1);
            ctx.target += ch;
            ctx.stage = ch == '*' ? parse_stage::sp : parse_stage::target;
            ctx.next_stage = parse_stage::http_slash;
            continue;
        }
        else if (ctx.stage == parse_stage::target) {
            const bool got_target = parse_target_and_sp(input, ctx.target, ec);
            if (!got_target || ec) {
                break;
            }
            validate_target(ctx.method, ctx.target, ec);
            if (ec) {
                break;
            }
            ctx.stage = parse_stage::http_slash;
            continue;
        }
        else if (ctx.stage == parse_stage::http_slash) {
            const bool got_http_slash = parse_http_slash(input, ec);
            if (!got_http_slash || ec) {
                break;
            }
            ctx.stage = parse_stage::version;
            continue;
        }
        else if (ctx.stage == parse_stage::version) {
            const version v = parse_http_version(input, ec);
            if (ec || v == version::invalid) {
                break;
            }
            ctx.version = v;
            ctx.stage = parse_stage::cr;
            continue;
        }
        else if (ctx.stage == parse_stage::cr || ctx.stage == parse_stage::lf) {
            const char expected_char =
                ctx.stage == parse_stage::cr ? '\r' : '\n';
            char cr_or_lf = 0;
            input.peek(buffer(&cr_or_lf, sizeof(cr_or_lf)));
            if (cr_or_lf != expected_char) {
                ec = make_error(error::bad_line_ending);
                break;
            }
            input.consume(1);
            ctx.stage = ctx.stage == parse_stage::cr ? parse_stage::lf
                                                     : parse_stage::done;
            continue;
        }
        else if (ctx.stage == parse_stage::done ||
                 ctx.stage == parse_stage::error) {
            break;
        }
    }

    if (ec) {
        ctx.stage = parse_stage::error;
        ctx.last_ec = static_cast<error>(ec.value());
    }

    return input_size - input.size();
}

std::size_t http::parse_status_line(ring_consumer& input,
                                    parse_status_line_context& ctx,
                                    std::error_code& ec) noexcept {
    // status-line = HTTP-version SP status-code SP [ reason-phrase ]
    // reason-phrase  = 1*( HTAB / SP / VCHAR / obs-text )
    using parse_stage = parse_status_line_context::parse_stage;
    const std::size_t input_size = input.size();
    ec.clear();
    if (ctx.stage == parse_stage::done) {
        return 0;
    }
    if (ctx.stage == parse_stage::error) {
        ec = make_error(ctx.last_ec);
        return 0;
    }
    while (!input.empty() && !ec) {
        if (ctx.stage == parse_stage::http_slash) {
            const bool got_http_slash = parse_http_slash(input, ec);
            if (!got_http_slash || ec) {
                break;
            }
            ctx.stage = parse_stage::version;
            continue;
        }
        else if (ctx.stage == parse_stage::version) {
            const version v = parse_http_version(input, ec);
            if (ec || v == version::invalid) {
                break;
            }
            ctx.version = v;
            ctx.stage = parse_stage::sp;
            ctx.next_stage = parse_stage::status;
            continue;
        }
        else if (ctx.stage == parse_stage::sp) {
            char sp_ch = 0;
            input.peek(buffer(&sp_ch, 1));
            if (!is_http_sp(sp_ch)) {
                error e = ctx.next_stage == parse_stage::status
                              ? error::bad_version
                              : error::bad_status;
                ec = make_error(e);
                break;
            }
            input.consume(1);
            ctx.stage = ctx.next_stage;
            continue;
        }
        else if (ctx.stage == parse_stage::status) {
            const int32_t status = parse_http_status(input, ec);
            if (ec || status < 0) {
                break;
            }
            ctx.status = static_cast<uint32_t>(status);
            ctx.stage = parse_stage::sp;
            ctx.next_stage = parse_stage::reason_start_or_cr;
            continue;
        }
        else if (ctx.stage == parse_stage::reason_start_or_cr) {
            char rp_or_cr = 0;
            input.peek(buffer(&rp_or_cr, sizeof(rp_or_cr)));
            if (is_http_reason_phrase(rp_or_cr)) {
                input.consume(1);
                ctx.reason += rp_or_cr;
                ctx.stage = parse_stage::reason;
                continue;
            }
            if (rp_or_cr == '\r') {
                input.consume(1);
                ctx.stage = parse_stage::lf;
                continue;
            }
            ec = make_error(error::bad_line_ending);
            break;
        }
        else if (ctx.stage == parse_stage::reason) {
            const bool got_reason =
                parse_reason_phrase_and_cr(input, ctx.reason, ec);
            if (ec || !got_reason) {
                break;
            }
            ctx.stage = parse_stage::lf;
            continue;
        }
        else if (ctx.stage == parse_stage::lf) {
            char ch = 0;
            input.peek(buffer(&ch, sizeof(ch)));
            if (ch != '\n') {
                ec = make_error(error::bad_line_ending);
                break;
            }
            input.consume(1);
            ctx.stage = parse_stage::done;
            break;
        }
        else if (ctx.stage == parse_stage::done ||
                 ctx.stage == parse_stage::error) {
            break;
        }
    }

    if (ec) {
        ctx.stage = parse_stage::error;
        ctx.last_ec = static_cast<error>(ec.value());
    }
    return input_size - input.size();
}

std::size_t http::parse_headers(
    ring_consumer& input, parse_headers_context& ctx,
    function_view<void(std::string_view, std::string_view)> on_header,
    std::error_code& ec) noexcept {
    using parse_stage = parse_headers_context::parse_stage;
    ec.clear();
    if (ctx.stage == parse_stage::error) {
        ec = make_error(ctx.last_ec);
        return 0;
    }
    if (ctx.stage == parse_stage::done) {
        return 0;
    }
    // field-line   = field-name ":" OWS field-value OWS
    const std::size_t total_size = input.size();
    while (!input.empty() && !ec) {
        if (ctx.stage == parse_stage::name_and_colon) {
            if (ctx.name.empty()) {
                char ch = 0;
                input.peek(buffer(&ch, 1));
                if (ch == '\r') {
                    input.consume(1);
                    ctx.stage = parse_stage::terminating_lf;
                    continue;
                }
            }
            const bool get_name =
                parse_field_name_and_colon(input, ctx.name, ec);
            if (ec || !get_name) {
                break;
            }
            if (ctx.name.empty()) {
                // header name is not allowed to be empty
                ec = make_error(error::bad_field);
                break;
            }
            const auto duplicates = std::array{
                std::pair{&ctx.got_host, field::host},
                std::pair{&ctx.got_connection, field::connection},
                std::pair{&ctx.got_upgrade, field::upgrade},
                std::pair{&ctx.got_transfer_encoding, field::transfer_encoding},
            };
            bool has_duplicate = false;
            for (const auto& [pflag, field_name] : duplicates) {
                if (iequal(ctx.name, field_to_string(field_name))) {
                    if (*pflag) {
                        has_duplicate = true;
                    }
                    *pflag = true;
                    break;
                }
            }
            if (has_duplicate) {
                ec = make_error(error::bad_field);
                break;
            }
            ctx.stage = parse_stage::ows;
            ctx.next_stage = parse_stage::value;
        }
        else if (ctx.stage == parse_stage::ows) {
            if (!consume_ows(input)) {
                break;
            }
            if (ctx.next_stage == parse_stage::cr && !ctx.name.empty()) {
                on_header(ctx.name, ctx.value);
                ctx.name.clear();
                ctx.value.clear();
            }
            ctx.stage = ctx.next_stage;
            continue;
        }
        else if (ctx.stage == parse_stage::value) {
            const bool got_value = parse_field_value(input, ctx.value, ec);
            if (ec || !got_value) {
                break;
            }
            /*
             * OWS occurring before the first non-whitespace octet
             * of the field line value, or after the last
             * non-whitespace octet of the field line value, is
             * excluded by parsers when extracting the field line
             * value from a field line.
             */
            while (!ctx.value.empty() && is_http_ws(ctx.value.back())) {
                ctx.value.pop_back();
            }
            // reject multiple content-length with different values
            if (iequal(ctx.name, field_to_string(field::content_length))) {
                std::uint64_t content_length = to_uint64(ctx.value, 10, ec);
                if (ec) {
                    ec = make_error(error::bad_content_length);
                    break;
                }
                if (!ctx.first_content_length.has_value()) {
                    ctx.first_content_length = content_length;
                }
                else {
                    if (*ctx.first_content_length != content_length) {
                        ec = make_error(error::bad_content_length);
                        break;
                    }
                    // skip duplicate content length
                    ctx.name.clear();
                    ctx.value.clear();
                }
            }
            ctx.stage = parse_stage::ows;
            ctx.next_stage = parse_stage::cr;
        }
        else if (ctx.stage == parse_stage::cr || ctx.stage == parse_stage::lf ||
                 ctx.stage == parse_stage::terminating_lf) {
            const char expected_ch = ctx.stage == parse_stage::cr ? '\r' : '\n';
            char ch = 0;
            input.peek(buffer(&ch, 1));
            if (ch != expected_ch) {
                ec = make_error(error::bad_line_ending);
                break;
            }
            input.consume(1);
            ctx.stage = ctx.stage == parse_stage::cr ? parse_stage::lf
                        : ctx.stage == parse_stage::terminating_lf
                            ? parse_stage::done
                            : parse_stage::name_and_colon;
        }
        else if (ctx.stage == parse_stage::done ||
                 ctx.stage == parse_stage::error) {
            break;
        }
    }
    if (ec) {
        ctx.stage = parse_stage::error;
        ctx.last_ec = static_cast<error>(ec.value());
    }
    return total_size - input.size();
}

std::size_t request_incremental_parser::parse(ring_consumer& input,
                                              std::error_code& ec) noexcept {
    ec.clear();
    if (done()) {
        return 0;
    }
    if (auto last_ec = last_error(); last_ec) {
        ec = last_ec;
        return 0;
    }

    auto on_header_fn = [this](std::string_view name, std::string_view value) {
        out_.headers.insert(name, value);
    };

    std::size_t parsed_n = 0;
    while (!input.empty() && !done() && !has_error() && !ec) {
        parsed_n += match(
            stage_,
            [&](parse_request_line_context& ctx) {
                const std::size_t n = parse_request_line(input, ctx, ec);
                if (ctx.done()) {
                    out_.method = ctx.method;
                    out_.version = ctx.version;
                    out_.target = std::move(ctx.target);
                    stage_ = parse_headers_context{};
                }
                return n;
            },
            [&](parse_headers_context& ctx) {
                const std::size_t n =
                    parse_headers(input, ctx, on_header_fn, ec);
                if (ctx.done()) {
                    stage_ = done_stage{};
                }
                return n;
            },
            [&](done_stage&) {
                assert(false);
                return std::size_t{0};
            },
            [&](error_stage&) {
                assert(false);
                return std::size_t{0};
            });
    }

    if (ec) {
        stage_ = error_stage{ec};
    }
    return parsed_n;
}

std::size_t response_incremental_parser::parse(ring_consumer& input,
                                               std::error_code& ec) noexcept {
    ec.clear();
    if (done()) {
        return 0;
    }
    if (auto last_ec = last_error(); last_ec) {
        ec = last_ec;
        return 0;
    }

    auto on_header_fn = [this](std::string_view name, std::string_view value) {
        out_.headers.insert(name, value);
    };

    std::size_t parsed_n = 0;
    while (!input.empty() && !done() && !has_error() && !ec) {
        parsed_n += match(
            stage_,
            [&](parse_status_line_context& ctx) {
                const std::size_t n = parse_status_line(input, ctx, ec);
                if (ctx.done()) {
                    out_.status = ctx.status;
                    out_.version = ctx.version;
                    out_.reason = std::move(ctx.reason);
                    stage_ = parse_headers_context{};
                }
                return n;
            },
            [&](parse_headers_context& ctx) {
                const std::size_t n =
                    parse_headers(input, ctx, on_header_fn, ec);
                if (ctx.done()) {
                    stage_ = done_stage{};
                }
                return n;
            },
            [&](done_stage&) {
                assert(false);
                return std::size_t{0};
            },
            [&](error_stage&) {
                assert(false);
                return std::size_t{0};
            });
    }

    if (ec) {
        stage_ = error_stage{ec};
    }
    return parsed_n;
}

std::size_t http::parse_chunk_size(ring_consumer& input,
                                   parse_chunk_size_context& ctx,
                                   std::error_code& ec) noexcept {
    // chunk-size = 1*HEXDIG
    using parse_stage = parse_chunk_size_context::parse_stage;
    ec.clear();
    if (ctx.stage == parse_stage::done) {
        return 0;
    }
    if (ctx.stage == parse_stage::error) {
        ec = make_error(error::bad_chunk);
        return 0;
    }

    const std::size_t total_size = input.size();
    while (!input.empty()) {
        if (ctx.stage == parse_stage::size) {
            char ch = 0;
            input.peek(buffer(&ch, sizeof(ch)));
            if (is_http_hex_digit(ch)) {
                if (ctx.hex_len >= ctx.hex_buff.size()) {
                    ec = make_error(error::bad_chunk);
                    break;
                }
                ctx.hex_buff[ctx.hex_len++] = ch;
                input.consume(1);
                continue;
            }
            else {
                if (ctx.hex_len == 0) {
                    // no hex numbers
                    ec = make_error(error::bad_chunk);
                    break;
                }
                ctx.chunk_size = to_size_t(
                    std::string_view{ctx.hex_buff.data(), ctx.hex_len}, 16, ec);
                if (ec) {
                    ec = make_error(error::bad_chunk);
                    break;
                }
                ctx.stage = parse_stage::done;
                break;
            }
        }
        else if (ctx.stage == parse_stage::done) {
            break;
        }
        else if (ctx.stage == parse_stage::error) {
            break;
        }
    }

    if (ec) {
        ctx.stage = parse_stage::error;
    }
    return total_size - input.size();
}

std::size_t http::parse_chunk_extensions(ring_consumer& input,
                                         parse_chunk_extensions_context& ctx,
                                         chunk_extension_callback callback,
                                         std::error_code& ec) {
    /*
    chunk-ext      = *( BWS ";" BWS chunk-ext-name
                      [ BWS "=" BWS chunk-ext-val ] )
    */
    using parse_stage = parse_chunk_extensions_context::parse_stage;
    ec.clear();
    if (ctx.error()) {
        ec = make_error(error::bad_chunk_extension);
        return 0;
    }
    if (ctx.done()) {
        return 0;
    }
    const std::size_t total_size = input.size();
    while (!input.empty() && !ctx.done() && !ec) {
        if (ctx.stage == parse_stage::check) {
            char ch = 0;
            input.peek(buffer(&ch, sizeof(ch)));
            if (is_http_ws(ch)) {
                input.consume(1);
                ctx.stage = parse_stage::bws;
                ctx.next_stage = parse_stage::colon;
                continue;
            }
            else if (ch == ';') {
                input.consume(1);
                ctx.stage = parse_stage::bws;
                ctx.next_stage = parse_stage::name;
                continue;
            }
            else {
                ctx.stage = parse_stage::done;
                break;
            }
        }
        else if (ctx.stage == parse_stage::bws) {
            assert(ctx.next_stage != ctx.stage);
            if (consume_ows(input)) {
                ctx.stage = ctx.next_stage;
            }
            continue;
        }
        else if (ctx.stage == parse_stage::colon) {
            char ch = 0;
            input.peek(buffer(&ch, sizeof(ch)));
            if (ch != ';') {
                ec = make_error(error::bad_chunk_extension);
                break;
            }
            input.consume(1);
            ctx.stage = parse_stage::bws;
            ctx.next_stage = parse_stage::name;
            continue;
        }
        else if (ctx.stage == parse_stage::name) {
            const bool got_name = parse_ext_token_name_value(input, ctx.name);
            if (!got_name) {
                break;
            }
            // empty names are not allowed
            if (ctx.name.empty()) {
                ec = make_error(error::bad_chunk_extension);
                break;
            }
            // name ends when a non token char is found
            // this char is not consumed by
            // parse_ext_token_name_value
            assert(!input.empty());
            if (input.empty()) {
                ec = make_error(error::bad_chunk_extension);
                break;
            }
            char ch = 0;
            input.peek(buffer(&ch, sizeof(ch)));
            if (ch == '=') {
                // followed by =value
                input.consume(1);
                ctx.stage = parse_stage::bws;
                ctx.next_stage = parse_stage::value_start;
                continue;
            }
            else if (is_http_ws(ch)) {
                // may be followed by =value or ;name
                ctx.stage = parse_stage::bws;
                ctx.next_stage = parse_stage::eq_or_colon;
                continue;
            }
            else {
                // last name without value
                ctx.stage = parse_stage::done;
                if (callback && !callback(ctx.name, "")) {
                    ec = make_error(error::bad_chunk_extension);
                }
                ctx.name.clear();
                break;
            }
        }
        else if (ctx.stage == parse_stage::eq_or_colon) {
            char ch = 0;
            input.peek(buffer(&ch, sizeof(ch)));
            if (ch == '=') {
                input.consume(1);
                ctx.stage = parse_stage::bws;
                ctx.next_stage = parse_stage::value_start;
                continue;
            }
            else if (ch == ';') {
                input.consume(1);
                ctx.stage = parse_stage::bws;
                ctx.next_stage = parse_stage::name;
                if (callback && !callback(ctx.name, "")) {
                    ec = make_error(error::bad_chunk_extension);
                }
                ctx.name.clear();
                continue;
            }
            else {
                ec = make_error(error::bad_chunk_extension);
                break;
            }
        }
        else if (ctx.stage == parse_stage::value_start) {
            // chunk-ext-val  = token / quoted-string
            // token          = 1*tchar
            char ch = 0;
            input.peek(buffer(&ch, sizeof(ch)));
            if (ch == '"') {
                input.consume(1);
                ctx.stage = parse_stage::quoted_value;
                continue;
            }
            else if (is_http_tchar(ch)) {
                input.consume(1);
                ctx.value += ch;
                ctx.stage = parse_stage::token_value;
                continue;
            }
            else {
                ec = make_error(error::bad_chunk_extension);
                break;
            }
        }
        else if (ctx.stage == parse_stage::token_value) {
            const bool got_value = parse_ext_token_name_value(input, ctx.value);
            if (!got_value) {
                break;
            }
            // token value can't be empty here because it had 1 char
            // from value_start stage
            assert(!ctx.value.empty());
            ctx.stage = parse_stage::report_name_value;
            continue;
        }
        else if (ctx.stage == parse_stage::quoted_value) {
            // quoted-string  = DQUOTE *( qdtext / quoted-pair )
            // DQUOTE qdtext         = HTAB / SP / %x21 / %x23-5B /
            // %x5D-7E / obs-text quoted-pair    = "\" ( HTAB / SP /
            // VCHAR / obs-text ) not implemented yet!
            const bool got_qdtext = parse_qdtext(input, ctx.value, ec);
            if (!got_qdtext || ec) {
                break;
            }
            ctx.stage = parse_stage::report_name_value;
            continue;
        }
        else if (ctx.stage == parse_stage::report_name_value) {
            if (callback && !callback(ctx.name, ctx.value)) {
                ec = make_error(error::bad_chunk_extension);
                break;
            }
            ctx.name.clear();
            ctx.value.clear();
            ctx.stage = parse_stage::check;
            continue;
        }
        else {
            // done, error
            break;
        }
    }

    if (ec) {
        ctx.stage = parse_stage::error;
    }
    return total_size - input.size();
}

std::size_t http::parse_chunk_data(ring_consumer& input,
                                   parse_chunk_data_context& ctx,
                                   dynamic_buffer out) {
    if (ctx.done()) {
        return 0;
    }
    std::size_t consumed = input.write_to(out, ctx.size);
    ctx.size -= consumed;
    return consumed;
}

std::size_t http::parse_chunk_trailing_crlf(ring_consumer& input,
                                            parse_chunk_trailing_crlf_ctx& ctx,
                                            std::error_code& ec) noexcept {
    if (ctx.error()) {
        ec = make_error(error::bad_line_ending);
        return 0;
    }
    ec.clear();
    if (ctx.done()) {
        return 0;
    }
    const std::size_t total_size = input.size();
    while (!ctx.done() && !ec && !input.empty()) {
        char ch = 0;
        input.peek(buffer(&ch, sizeof(ch)));
        if (ch != ctx.expected_char) {
            ctx.is_error = true;
            ec = make_error(error::bad_line_ending);
            break;
        }
        input.consume(1);
        if (ctx.expected_char == '\r') {
            ctx.expected_char = '\n';
        }
        else {
            ctx.is_done = true;
        }
    }
    return total_size - input.size();
}

std::size_t
chunks_incremental_parser::parse(ring_consumer& input, dynamic_buffer output,
                                 chunk_extension_callback extensions_cb,
                                 header_callback trailers_callback,
                                 std::error_code& ec) {
    /*
            chunked-body   = *chunk
               last-chunk
               trailer-section
               CRLF

            chunk          = chunk-size [ chunk-ext ] CRLF
               chunk-data CRLF
            chunk-size     = 1*HEXDIG
            last-chunk     = 1*("0") [ chunk-ext ] CRLF
            trailer-section   = *( field-line CRLF )
    */
    ec.clear();
    if (done()) {
        return 0;
    }
    if (auto last_ec = last_error(); last_ec) {
        ec = last_ec;
        return 0;
    }

    const std::size_t total_input_size = input.size();
    while (!input.empty() && !ec && need_more()) {
        match(
            stage_,
            [&](parse_chunk_size_context& ctx) {
                const std::size_t n = parse_chunk_size(input, ctx, ec);
                if (ctx.done()) {
                    chunk_size_ = ctx.chunk_size;
                    stage_ = parse_chunk_extensions_context{};
                }
                return n;
            },
            [&](parse_chunk_extensions_context& ctx) {
                const std::size_t n =
                    parse_chunk_extensions(input, ctx, extensions_cb, ec);
                if (ctx.done()) {
                    stage_ = parse_crlf_after_size_ctx{};
                }
                return n;
            },
            [&](parse_crlf_after_size_ctx& ctx) {
                const std::size_t n = parse_chunk_trailing_crlf(input, ctx, ec);
                if (ctx.done()) {
                    stage_ = parse_chunk_data_context{chunk_size_};
                }
                return n;
            },
            [&](parse_chunk_data_context& ctx) {
                const std::size_t n = parse_chunk_data(input, ctx, output);
                if (ctx.done()) {
                    if (chunk_size_ == 0) {
                        // there is no chunk-data CRLF
                        // either there is CRLF, or *(
                        // field-line CRLF ) both will be
                        // consumed by trailers parse
                        stage_ = parse_headers_context{};
                    }
                    else {
                        // parse CRLF following the data
                        stage_ = parse_chunk_trailing_crlf_ctx{};
                    }
                }
                return n;
            },
            [&](parse_chunk_trailing_crlf_ctx& ctx) {
                const std::size_t n = parse_chunk_trailing_crlf(input, ctx, ec);
                if (ctx.done()) {
                    assert(chunk_size_ != 0);
                    // parse the next chunk
                    stage_ = parse_chunk_size_context{};
                }
                return n;
            },
            [&](parse_headers_context& ctx) {
                // the headers parser will consume the CRLF after
                // body if there are trailers or not, so if done the
                // parse is done
                const std::size_t n =
                    parse_headers(input, ctx, trailers_callback, ec);
                if (ctx.done()) {
                    stage_ = done_stage{};
                }
                return n;
            },
            [&](done_stage&) {
                assert(false);
                return std::size_t{0};
            },
            [&](error_stage&) {
                assert(false);
                return std::size_t{0};
            });
    }

    if (ec) {
        stage_ = error_stage{ec};
    }
    return total_input_size - input.size();
}