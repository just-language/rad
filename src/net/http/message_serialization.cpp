#include <rad/net/http/http_parser.h>

using namespace rad;
using namespace net;
using namespace http;

namespace {
    using namespace std::string_view_literals;
    constexpr auto HTTPSlash = "HTTP/"sv;
    constexpr auto ColonSP = ": "sv;
    constexpr auto SP = " "sv;

    void copy_into_span(std::span<char>& out, const char* p,
                        std::size_t n) noexcept {
        assert(out.size() >= n);
        std::copy(p, p + n, out.data());
        out = out.subspan(n);
    }

    void serialize_headers(std::span<char>& out,
                           headers_iterator& headers) noexcept {
        headers.return_to_begin();
        while (1) {
            auto next_header = headers.next();
            if (!next_header.has_value()) {
                return;
            }
            const auto& [f, v] = *next_header;
            copy_into_span(out, f.data(), f.size());
            copy_into_span(out, ColonSP.data(), ColonSP.size());
            copy_into_span(out, v.data(), v.size());
            copy_into_span(out, CRLF.data(), CRLF.size());
        }
    }

    void insert_headers_and_body(std::span<char>& out,
                                 headers_iterator& headers,
                                 std::string_view body) {
        // insert the headers
        serialize_headers(out, headers);
        // final CRLF terminating the headers
        copy_into_span(out, CRLF.data(), CRLF.size());
        // insert the body if there is one
        copy_into_span(out, body.data(), body.size());
    }
} // namespace

void http::serialize_request(verb method, std::string_view path, version ver,
                             headers_iterator& headers,
                             std::size_t headers_serialized_size,
                             std::string_view body, dynamic_buffer out) {
    const std::string_view verb_str = verb_to_string(method);
    const std::string_view version_str = version_to_string(ver);

    // calculate required size
    size_t size = verb_str.size() + 1 + path.size() + 1 + HTTPSlash.size() +
                  version_str.size() + CRLF.size() * 2;
    size += headers_serialized_size;
    size += body.size();

    // reserve space for the serialized request
    const std::size_t out_old_size = out.size();
    auto out_span = out.prepare(size).to_span<char>();

    // insert the request line
    // insert the http verb and space
    copy_into_span(out_span, verb_str.data(), verb_str.size());
    copy_into_span(out_span, SP.data(), SP.size());
    // insert the path and space
    copy_into_span(out_span, path.data(), path.size());
    copy_into_span(out_span, SP.data(), SP.size());
    // insert the HTTP/1.x and CRLF
    copy_into_span(out_span, HTTPSlash.data(), HTTPSlash.size());
    copy_into_span(out_span, version_str.data(), version_str.size());
    copy_into_span(out_span, CRLF.data(), CRLF.size());

    // insert the headers and body
    insert_headers_and_body(out_span, headers, body);
    // to ensure no excess space
    out.resize(out_old_size + size - out_span.size());
}

void http::serialize_response(uint32_t status, version ver,
                              std::string_view reason_phrase,
                              headers_iterator& headers,
                              std::size_t headers_serialized_size,
                              std::string_view body, dynamic_buffer out) {
    const std::string_view version_str = version_to_string(ver);
    std::array<char, std::numeric_limits<uint32_t>::digits10 + 1>
        status_code_buff;
    auto res = std::to_chars(status_code_buff.data(),
                             status_code_buff.data() + status_code_buff.size(),
                             status);
    if (res.ec != std::errc{}) {
        return;
    }
    const std::string_view status_code_str{status_code_buff.data(), res.ptr};

    // status-line = HTTP-version SP status-code SP [ reason-phrase ]
    size_t size = HTTPSlash.size() + version_str.size() + 1 +
                  status_code_str.size() + 1 + CRLF.size() * 2;
    if (!reason_phrase.empty()) {
        size += 1 + reason_phrase.size();
    }
    size += headers_serialized_size;
    size += body.size();

    // reserve space for the serialized request
    const std::size_t out_old_size = out.size();
    auto out_span = out.prepare(size).to_span<char>();

    // insert the status line
    copy_into_span(out_span, HTTPSlash.data(), HTTPSlash.size());
    copy_into_span(out_span, version_str.data(), version_str.size());
    copy_into_span(out_span, SP.data(), SP.size());
    copy_into_span(out_span, status_code_str.data(), status_code_str.size());
    copy_into_span(out_span, SP.data(), SP.size());
    copy_into_span(out_span, reason_phrase.data(), reason_phrase.size());
    copy_into_span(out_span, CRLF.data(), CRLF.size());
    // insert the headers and body
    insert_headers_and_body(out_span, headers, body);
    // to ensure no excess space
    out.resize(out_old_size + size - out_span.size());
}