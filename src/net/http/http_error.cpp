#include <rad/net/http/http_parser.h>

using namespace RAD_LIB_NAMESPACE;
using namespace net;
using namespace http;

namespace {
    class http_error_category_t : public std::error_category {
        const char* name() const noexcept override {
            return "http";
        }

        std::string message(int error_val) const override {
            error ec = static_cast<error>(error_val);
            switch (ec) {
            case error::no_error:
                return "";
            case error::end_of_stream:
                return "The end of the stream "
                       "was "
                       "reached.";
            case error::partial_message:
                return "The incoming message "
                       "is "
                       "incomplete.";
            case error::bad_line_ending:
                return "The line ending was "
                       "malformed.";
            case error::bad_method:
                return "The method is invalid.";
            case error::bad_target:
                return "The request-target is "
                       "invalid.";
            case error::bad_version:
                return "The HTTP-version is "
                       "invalid.";
            case error::bad_status:
                return "The status-code is "
                       "invalid.";
            case error::bad_reason:
                return "The reason-phrase is "
                       "invalid.";
            case error::bad_field:
                return "The field name is "
                       "invalid.";
            case error::bad_value:
                return "The field value is "
                       "invalid.";
            case error::bad_content_length:
                return "The Content-Length is "
                       "invalid.";
            case error::bad_transfer_encoding:
                return "The Transfer-Encoding "
                       "is "
                       "invalid.";
            case error::bad_chunk:
                return "The chunk syntax is "
                       "invalid.";
            case error::bad_chunk_extension:
                return "The chunk extension is "
                       "invalid.";
            case error::too_large_message:
                return "The message is too "
                       "large";
            case error::bad_response_code:
                return "Bad HTTP response code";
            case error::unexpected_body:
                return "Unexpected HTTP "
                       "response body";
            case error::bad_scheme:
                return "Request url scheme "
                       "must be "
                       "either http "
                       "or "
                       "https";
            case error::too_many_redirections:
                return "Too many redirections";
            default:
                return "";
            }
        }
    };

    const http_error_category_t http_error_category_inst;
} // namespace

const std::error_category& http::http_category() noexcept {
    return http_error_category_inst;
}