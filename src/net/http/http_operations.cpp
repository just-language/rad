#include <rad/net/http/http_parser.h>
#include <rad/net/url/url.h>

using namespace rad;
using namespace net;
using namespace http;

namespace {}

void http::make_request_target(const url& req_url, verb method, bool to_proxy,
                               std::string& target, std::error_code& ec) {
    ec.clear();
    target.clear();
    if (req_url.pathname() == "*") {
        /*
         * asterisk-form  = "*"
         * When a client wishes to request OPTIONS for the server as a
         * whole, as opposed to a specific named resource of that
         * server, the client MUST send only "*" (%x2A) as the
         * request-target. OPTIONS * HTTP/1.1
         */
        if (method != verb::options) {
            ec = make_error(error::bad_target);
            return;
        }
        target = '*';
        return;
    }
    else if (method == verb::connect) {
        /*
         * authority-form = uri-host ":" port
         * When making a CONNECT request to establish a tunnel through
         * one or more proxies, a client MUST send only the host and
         * port of the tunnel destination as the request-target. CONNECT
         * www.example.com:80 HTTP/1.1
         */
        std::string_view url_host = req_url.hostname();
        if (url_host.empty()) {
            ec = make_error(error::bad_target);
            return;
        }
        uint16_t port_num = req_url.port();
        if (port_num == 0) {
            ec = make_error(error::bad_target);
            return;
        }
        std::string port_str = std::to_string(port_num);
        target.reserve(url_host.size() + 1 + port_str.size());
        target.append(url_host);
        target += ':';
        target.append(port_str);
        return;
    }
    else if (!to_proxy) {
        /*
         * origin-form    = absolute-path [ "?" query ]
         * When making a request directly to an origin server, other
         * than a CONNECT or server-wide OPTIONS request (as detailed
         * below), a client MUST send only the absolute path and query
         * components of the target URI as the request-target. If the
         * target URI's path component is empty, the client MUST send
         * "/" as the path within the origin-form of request-target. GET
         * /where?q=now HTTP/1.1
         */
        target.reserve(std::min(req_url.pathname().size(), size_t{1}) +
                       req_url.query().size() +
                       static_cast<size_t>(!req_url.query().empty()));
        if (req_url.pathname().empty()) {
            target += '/';
        }
        else {
            target += req_url.pathname();
        }
        if (!req_url.query().empty()) {
            target += '?';
            target += req_url.query();
        }
    }
    else {
        /*
         * When making a request to a proxy, other than a CONNECT or
         * server-wide OPTIONS request (as detailed below), a client
         * MUST send the target URI in absolute-form as the
         * request-target. absolute-form  = absolute-URI GET
         * http://www.example.org/pub/WWW/TheProject.html HTTP/1.1
         */
        target = req_url.href();
    }
}

message_body_size http::determine_message_body_length(
    verb req_method, std::optional<response_status> status, version ver,
    std::string_view content_length, std::string_view transfer_encoding) {
    /*
     * Any response to a HEAD request and any response with a 1xx
     * (Informational), 204 (No Content), or 304 (Not Modified) status code
     * is always terminated by the first empty line after the header fields,
     * regardless of the header fields present in the message, and thus
     * cannot contain a message body or trailer section.
     */
    if (req_method == verb::head ||
        (status.has_value() && (status == response_status::no_content ||
                                status == response_status::not_modified))) {
        return std::size_t{0};
    }
    /*
     * If a message is received with both a Transfer-Encoding and a
     * Content-Length header field, the Transfer-Encoding overrides the
     * Content-Length. Such a message might indicate an attempt to perform
     * request smuggling (Section 11.2) or response splitting (Section 11.1)
     * and ought to be handled as an error. An intermediary that chooses to
     * forward the message MUST first remove the received Content-Length
     * field and process the Transfer-Encoding (as described below) prior to
     * forwarding the message downstream.
     */
    if (!content_length.empty() && !transfer_encoding.empty()) {
        content_length = {};
    }
    /*
     * If a Transfer-Encoding header field is present and the chunked
     * transfer coding (Section 7.1) is the final encoding, the message body
     * length is determined by reading and decoding the chunked data until
     * the transfer coding indicates the data is complete.
     */
    if (!transfer_encoding.empty()) {
        std::vector<std::string_view> encodings;
        parse_transfer_encoding(transfer_encoding, encodings);
        if (!encodings.empty() && iequal(encodings.back(), "chunked")) {
            return chunked_body{};
        }
        /*
         * If a Transfer-Encoding header field is present in a response
         * and the chunked transfer coding is not the final encoding,
         * the message body length is determined by reading the
         * connection until it is closed by the server.
         */
        if (status.has_value()) {
            return body_until_eof{};
        }
        /*
         * If a Transfer-Encoding header field is present in a request
         * and the chunked transfer coding is not the final encoding,
         * the message body length cannot be determined reliably; the
         * server MUST respond with the 400 (Bad Request) status code
         * and then close the connection.
         */
        return bad_message_body{};
    }
    if (!content_length.empty()) {
        std::error_code content_len_ec;
        std::uint64_t clen = to_uint64(content_length, 10, content_len_ec);
        /*
         * If a message is received without Transfer-Encoding and with
         * an invalid Content-Length header field, then the message
         * framing is invalid and the recipient MUST treat it as an
         * unrecoverable error.
         */
        if (content_len_ec) {
            return bad_message_body{};
        }
        /*
         * If a valid Content-Length header field is present without
         * Transfer-Encoding, its decimal value defines the expected
         * message body length in octets.
         */
        return clen;
    }
    /*
     * If this is a request message and none of the above are true, then the
     * message body length is zero (no message body is present).
     */
    if (!status.has_value()) {
        return std::uint64_t{0};
    }
    /*
     * Otherwise, this is a response message without a declared message body
     * length, so the message body length is determined by the number of
     * octets received prior to the server closing the connection.
     */
    return body_until_eof{};
}