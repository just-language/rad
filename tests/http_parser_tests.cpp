#include <rad/net/http/http_parser.h>
#include <rad/unittest/unittest.h>

#include <array>
#include <format>
#include <functional>
#include <iostream>

using namespace RAD_LIB_NAMESPACE;
using namespace net;
using namespace http;
using namespace unittest;
using namespace std::string_view_literals;

namespace {
    using headers_t = std::vector<std::pair<std::string, std::string>>;

    using chunk_extensions_t = std::vector<std::pair<std::string, std::string>>;

    struct request_line_entry {
        verb method = verb::invalid;
        version v = version::v1_1;
        std::string target;
    };

    struct response_line_entry {
        uint32_t status = 0;
        version v = version::v1_1;
        std::string reason;
    };

    void insert_headers_fn(headers& output, std::string_view name,
                           std::string_view value) {
        output.insert(name, value);
    }

    void check_equal_headers(const headers_t& h1, const headers& h2,
                             const std::string test_i) {
        assert_eq(h1.size(), h2.size(),
                  ("parsed headers count mismatch (" + test_i + ")").c_str());
        for (const auto& [f, v] : h1) {
            const std::size_t h1_count =
                std::count_if(h1.begin(), h1.end(), [&f](const auto& p) {
                    return iequal(p.first, f);
                });
            const std::size_t h2_count = h2.count(f);
            assert_eq(h1_count, h2_count,
                      ("parsed headers count of header '" + f + "' mismatch (" +
                       test_i + ")")
                          .c_str());
            auto r = h2.equal_range(f);
            assert_eq(r.size(), h1_count,
                      ("parsed headers count of header '" + f + "' mismatch (" +
                       test_i + ")")
                          .c_str());
            auto& front = r.front();
            bool found_value = false;
            for (const auto& rf : r) {
                if (!iequal(rf.first, front.first)) {
                    throw_test_error("equal_range() returned unequal "
                                     "fields");
                }
                if (rf.second == v) {
                    found_value = true;
                }
            }
            assert_true(found_value, ("parsed headers value of header '" + f +
                                      "' mismatch (" + test_i + ")")
                                         .c_str());
        }
    }

    bool insert_chunk_extension(chunk_extensions_t& output,
                                std::string_view name, std::string_view value) {
        output.emplace_back(name, value);
        return true;
    }

    // Valid test cases
    std::vector<std::pair<std::string, request_line_entry>>
        valid_request_line_cases = {
            // Basic GET requests
            {"GET / HTTP/1.1\r\n", {verb::get, version::v1_1, "/"}},
            {"GET /index.html HTTP/1.1\r\n",
             {verb::get, version::v1_1, "/index.html"}},
            {"GET /path/to/resource HTTP/1.1\r\n",
             {verb::get, version::v1_1, "/path/to/resource"}},

            {"GET / HTTP/1.0\r\n", {verb::get, version::v1_0, "/"}},
            {"GET /index.html HTTP/1.0\r\n",
             {verb::get, version::v1_0, "/index.html"}},
            {"GET /path/to/resource HTTP/1.0\r\n",
             {verb::get, version::v1_0, "/path/to/resource"}},

            // GET with query parameters
            {"GET /search?q=test HTTP/1.1\r\n",
             {verb::get, version::v1_1, "/search?q=test"}},
            {"GET /api/users?id=123&name=john HTTP/1.1\r\n",
             {verb::get, version::v1_1, "/api/users?id=123&name=john"}},

            // Different methods
            {"POST /api/users HTTP/1.1\r\n",
             {verb::post, version::v1_1, "/api/users"}},
            {"PUT /api/users/123 HTTP/1.1\r\n",
             {verb::put, version::v1_1, "/api/users/123"}},
            {"HEAD /resource HTTP/1.1\r\n",
             {verb::head, version::v1_1, "/resource"}},
            {"DELETE /api/users/123 HTTP/1.1\r\n",
             {verb::delete_, version::v1_1, "/api/users/123"}},
            {"OPTIONS /resource HTTP/1.1\r\n",
             {verb::options, version::v1_1, "/resource"}},
            {"CONNECT example.com:443 HTTP/1.1\r\n",
             {verb::connect, version::v1_1, "example.com:443"}},

            // HTTP/1.0
            {"GET / HTTP/1.0\r\n", {verb::get, version::v1_0, "/"}},

            // Special targets
            {"OPTIONS * HTTP/1.1\r\n", {verb::options, version::v1_1, "*"}},
            {"GET /path%20with%20spaces HTTP/1.1\r\n",
             {verb::get, version::v1_1, "/path%20with%20spaces"}},
            {"GET /path-with-special-chars-._~!$&'()*+,;=:@ HTTP/1.1\r\n",
             {verb::get, version::v1_1,
              "/path-with-special-chars-._~!$&'()*+,;=:@"}},

            // Long paths
            {"GET "
             "/very/long/path/with/many/segments/and/"
             "parameters?key1=value1&key2=value2 HTTP/1.1\r\n",
             {verb::get, version::v1_1,
              "/very/long/path/with/many/segments/and/"
              "parameters?key1=value1&key2=value2"}},

            {"GET http://example.com/ HTTP/1.1\r\n",
             {verb::get, version::v1_1,
              "http://example.com/"}}, // Absolute allowed to proxy
            {"GET path/without/leading/slash HTTP/1.1\r\n",
             {verb::get, version::v1_1,
              "path/without/leading/slash"}}, // Error on url parsing

            {"OPTIONS /path HTTP/1.1\r\n",
             {verb::options, version::v1_1,
              "/path"}}, // OPTIONS with path instead of asterisk
    };

    // Invalid test cases
    std::vector<std::pair<std::string, request_line_entry>>
        invalid_request_line_cases = {
            // Malformed versions
            {"GET / HTTP/1.2\r\n", {verb::invalid, version::invalid, ""}},
            {"GET / HTTP/2.0\r\n", {verb::invalid, version::invalid, ""}},
            {"GET / HTTP/1.1\n",
             {verb::invalid, version::invalid, ""}}, // Missing CR
            {"GET / HTTP/1.1\r",
             {verb::invalid, version::invalid, ""}}, // Missing LF
            {"GET / HTTP/1.1",
             {verb::invalid, version::invalid, ""}}, // Missing CRLF

            // Invalid methods
            {"GETT / HTTP/1.1\r\n", {verb::invalid, version::invalid, ""}},
            {"get / HTTP/1.1\r\n",
             {verb::invalid, version::invalid, ""}}, // Lowercase method
            {"Get / HTTP/1.1\r\n",
             {verb::invalid, version::invalid, ""}}, // Mixed case method
            {"gEt / HTTP/1.1\r\n",
             {verb::invalid, version::invalid, ""}}, // Weird case method
            {"POSTT / HTTP/1.1\r\n", {verb::invalid, version::invalid, ""}},
            {"", {verb::invalid, version::invalid, ""}}, // Empty string

            // Missing components
            {"GET HTTP/1.1\r\n",
             {verb::invalid, version::invalid, ""}}, // Missing target
            {"/ HTTP/1.1\r\n",
             {verb::invalid, version::invalid, ""}}, // Missing method
            {"GET /\r\n",
             {verb::invalid, version::invalid, ""}}, // Missing version

            // Invalid targets
            {"GET  HTTP/1.1\r\n",
             {verb::invalid, version::invalid, ""}}, // Empty target

            // Invalid characters
            {"GET /path with space HTTP/1.1\r\n",
             {verb::invalid, version::invalid, ""}}, // Space in target
            {"GET /path\twith\ttab HTTP/1.1\r\n",
             {verb::invalid, version::invalid, ""}}, // Tab in target
            {"GET /path\nwith\nnewline HTTP/1.1\r\n",
             {verb::invalid, version::invalid, ""}}, // Newline in target
            {"GET /path\rwith\rcarriage return HTTP/1.1\r\n",
             {verb::invalid, version::invalid, ""}}, // CR in target

            // Extra spaces
            {"GET  /  HTTP/1.1\r\n",
             {verb::invalid, version::invalid, ""}}, // Extra spaces
            {"GET / HTTP/1.1 \r\n",
             {verb::invalid, version::invalid,
              ""}}, // Extra space after version

            // Wrong order
            {"HTTP/1.1 GET /\r\n",
             {verb::invalid, version::invalid, ""}}, // Wrong component order

            // Invalid CONNECT targets
            {"CONNECT /path HTTP/1.1\r\n",
             {verb::invalid, version::invalid,
              ""}}, // CONNECT with path instead of authority
            {"CONNECT example.com HTTP/1.1\r\n",
             {verb::invalid, version::invalid, ""}}, // CONNECT without port

            // Invalid non OPTIONS targets (non-asterisk)
            {"GET * HTTP/1.1\r\n",
             {verb::invalid, version::invalid, ""}}, // non-asterisk
    };

    std::vector<std::pair<std::string, response_line_entry>>
    create_status_line_test_cases() {
        std::vector<std::pair<std::string, response_line_entry>> test_cases;

        // Valid test cases
        test_cases.push_back(
            {"HTTP/1.1 200 OK\r\n", {200, version::v1_1, "OK"}});
        test_cases.push_back(
            {"HTTP/1.1 404 Not Found\r\n", {404, version::v1_1, "Not Found"}});
        test_cases.push_back({"HTTP/1.1 500 Internal Server Error\r\n",
                              {500, version::v1_1, "Internal Server Error"}});
        test_cases.push_back(
            {"HTTP/1.0 200 OK\r\n", {200, version::v1_0, "OK"}});
        test_cases.push_back(
            {"HTTP/1.1 201 Created\r\n", {201, version::v1_1, "Created"}});
        test_cases.push_back({"HTTP/1.1 200 \r\n",
                              {200, version::v1_1, ""}}); // Empty reason phrase
        test_cases.push_back({"HTTP/1.1 302 Found Temporarily\r\n",
                              {302, version::v1_1, "Found Temporarily"}});
        test_cases.push_back(
            {"HTTP/1.1 100 Continue\r\n", {100, version::v1_1, "Continue"}});
        test_cases.push_back(
            {"HTTP/1.1 200 OK \r\n",
             {200, version::v1_1, "OK "}}); // Reason phrase with trailing space
        test_cases.push_back({"HTTP/1.1 204 No Content\r\n",
                              {204, version::v1_1, "No Content"}});

        // Invalid test cases
        test_cases.push_back({"HTTP/1.2 200 OK\r\n",
                              {0, version::invalid, ""}}); // Invalid version
        test_cases.push_back({"http/1.1 200 OK\r\n",
                              {0, version::invalid, ""}}); // Lowercase protocol
        test_cases.push_back(
            {"HTTP/1.1 200OK\r\n",
             {0, version::invalid, ""}}); // No space after code
        test_cases.push_back(
            {"HTTP/1.1  200 OK\r\n",
             {0, version::invalid, ""}}); // Multiple spaces after version
        test_cases.push_back(
            {"HTTP/1.1 20 OK\r\n",
             {0, version::invalid, ""}}); // Status code not three digits
        test_cases.push_back(
            {"HTTP/1.1 2000 OK\r\n",
             {0, version::invalid, ""}}); // Status code too long
        test_cases.push_back(
            {"HTTP/1.1 200 O\rK\r\n",
             {0, version::invalid, ""}}); // Reason phrase contains CR
        test_cases.push_back(
            {"HTTP/1.1 200 OK\n",
             {0, version::invalid, ""}}); // Missing CR, only LF
        test_cases.push_back(
            {"HTTP/1.1 200 OK\r",
             {0, version::invalid, ""}}); // Missing LF, only CR
        test_cases.push_back(
            {"HTTP/1.1 200 OK", {0, version::invalid, ""}}); // No CRLF
        test_cases.push_back(
            {"HTTP/1.1 2A0 OK\r\n",
             {0, version::invalid, ""}}); // Status code with non-digit
        test_cases.push_back(
            {"HTTP/1.1 200\r\n",
             {0, version::invalid, ""}}); // No space after code
        test_cases.push_back({"HTTP/ 200 OK\r\n",
                              {0, version::invalid, ""}}); // No version number
        test_cases.push_back(
            {"HTTP/1.a 200 OK\r\n",
             {0, version::invalid, ""}}); // Invalid version format
        test_cases.push_back(
            {"HTTPS/1.1 200 OK\r\n",
             {0, version::invalid, ""}}); // Invalid protocol name

        return test_cases;
    }

    void test_request_line_parse_valid() {
        const auto& test_cases = valid_request_line_cases;

        for (const auto& [line, entry] : test_cases) {
            parse_request_line_context ctx;
            std::error_code ec;
            auto rbuf = ring_consumer(buffer(line));
            parse_request_line(rbuf, ctx, ec);
            if (ctx.need_more()) {
                assert_true(!ec, "http request line parsing is not "
                                 "complete but error is set");
                ec = std::make_error_code(
                    std::errc::resource_unavailable_try_again);
            }
            assert_true(!ec, ("failed to parse a valid request line (" + line +
                              "): " + ec.message())
                                 .c_str());
            assert_eq(entry.v, ctx.version,
                      ("parsed request line version mismatch (" + line + ")")
                          .c_str());
            assert_eq(
                entry.method, ctx.method,
                ("parsed request line method mismatch (" + line + ")").c_str());
            assert_eq(
                entry.target, ctx.target,
                ("parsed request line target mismatch (" + line + ")").c_str());
        }

        for (const auto& [line, entry] : test_cases) {
            if (line.empty()) {
                continue;
            }
            std::vector<char> line_buff(line.size());
            for (std::size_t step = 1; step < line.size(); ++step) {
                parse_request_line_context ctx;
                auto rbuf = ring_consumer_producer(buffer(line_buff));
                std::string_view line_view = line;
                std::error_code ec;
                while (!line_view.empty() && !ec) {
                    const std::size_t commit_size =
                        std::min(line_view.size(), step);
                    rbuf.put_data(buffer(line_view.substr(0, commit_size)));
                    line_view.remove_prefix(commit_size);
                    parse_request_line(rbuf, ctx, ec);
                }
                if (ctx.need_more()) {
                    assert_true(!ec, "http status line parsing is not "
                                     "complete but error is set");
                    ec = std::make_error_code(
                        std::errc::resource_unavailable_try_again);
                }
                assert_true(!ec, ("failed to parse a valid request line (" +
                                  line + "): " + ec.message())
                                     .c_str());
                assert_eq(
                    entry.v, ctx.version,
                    ("parsed request line version mismatch (" + line + ")")
                        .c_str());
                assert_eq(entry.method, ctx.method,
                          ("parsed request line method mismatch (" + line + ")")
                              .c_str());
                assert_eq(entry.target, ctx.target,
                          ("parsed request line target mismatch (" + line + ")")
                              .c_str());
            }
        }
    }

    void test_request_line_parse_invalid() {
        const auto& test_cases = invalid_request_line_cases;
        for (const auto& [line, entry] : test_cases) {
            parse_request_line_context ctx;
            std::error_code ec;
            auto rbuf = ring_consumer(buffer(line));
            parse_request_line(rbuf, ctx, ec);
            if (ctx.need_more()) {
                assert_true(!ec, "http request line parsing is not "
                                 "complete but error is set");
                ec = std::make_error_code(
                    std::errc::resource_unavailable_try_again);
            }
            assert_false(
                !ec, ("parsed an invalid request line (" + line + ")").c_str());
        }

        for (const auto& [line, entry] : test_cases) {
            if (line.empty()) {
                continue;
            }
            std::vector<char> line_buff(line.size());
            for (std::size_t step = 1; step < line.size(); ++step) {
                parse_request_line_context ctx;
                auto rbuf = ring_consumer_producer(buffer(line_buff));
                std::string_view line_view = line;
                std::error_code ec;
                while (!line_view.empty() && !ec) {
                    const std::size_t commit_size =
                        std::min(line_view.size(), step);
                    rbuf.put_data(buffer(line_view.substr(0, commit_size)));
                    line_view.remove_prefix(commit_size);
                    parse_request_line(rbuf, ctx, ec);
                }
                if (ctx.need_more()) {
                    assert_true(!ec, "http status line parsing is not "
                                     "complete but error is set");
                    ec = std::make_error_code(
                        std::errc::resource_unavailable_try_again);
                }
                assert_false(
                    !ec,
                    ("parsed an invalid request line (" + line + ")").c_str());
            }
        }
    }

    void test_status_line_parse() {
        const auto test_cases = create_status_line_test_cases();

        for (const auto& [line, entry] : test_cases) {
            parse_status_line_context ctx;
            std::error_code ec;
            auto rbuf = ring_consumer(buffer(line));
            parse_status_line(rbuf, ctx, ec);
            if (ctx.need_more()) {
                assert_true(!ec, "http status line parsing is not "
                                 "complete but error is set");
                ec = std::make_error_code(
                    std::errc::resource_unavailable_try_again);
            }
            if (entry.v == version::invalid) {
                assert_false(
                    !ec,
                    ("parsed an invalid status line (" + line + ")").c_str());
                continue;
            }
            assert_true(
                !ec,
                ("failed to parse a valid status line (" + line + ")").c_str());
            assert_eq(
                entry.v, ctx.version,
                ("parsed status line version mismatch (" + line + ")").c_str());
            assert_eq(
                entry.status, ctx.status,
                ("parsed status line status mismatch (" + line + ")").c_str());
            assert_eq(
                entry.reason, ctx.reason,
                ("parsed status line reason mismatch (" + line + ")").c_str());
        }

        for (const auto& [line, entry] : test_cases) {
            if (line.empty()) {
                continue;
            }
            std::vector<char> line_buff(line.size());
            for (std::size_t step = 1; step < line.size(); ++step) {
                parse_status_line_context ctx;
                auto rbuf = ring_consumer_producer(buffer(line_buff));
                std::string_view line_view = line;
                std::error_code ec;
                while (!line_view.empty() && !ec) {
                    const std::size_t commit_size =
                        std::min(line_view.size(), step);
                    rbuf.put_data(buffer(line_view.substr(0, commit_size)));
                    line_view.remove_prefix(commit_size);
                    parse_status_line(rbuf, ctx, ec);
                }
                if (ctx.need_more()) {
                    assert_true(!ec, "http status line parsing is not "
                                     "complete but error is set");
                    ec = std::make_error_code(
                        std::errc::resource_unavailable_try_again);
                }
                if (entry.v == version::invalid) {
                    assert_false(
                        !ec, ("parsed an invalid status line (" + line + ")")
                                 .c_str());
                    continue;
                }
                assert_true(
                    !ec, ("failed to parse a valid status line (" + line + ")")
                             .c_str());
                assert_eq(entry.v, ctx.version,
                          ("parsed status line version mismatch (" + line + ")")
                              .c_str());
                assert_eq(entry.status, ctx.status,
                          ("parsed status line status mismatch (" + line + ")")
                              .c_str());
                assert_eq(entry.reason, ctx.reason,
                          ("parsed status line reason mismatch (" + line + ")")
                              .c_str());
            }
        }
    }

    std::vector<std::pair<std::string, headers_t>> valid_header_test_cases = {
        // Basic single headers
        {"Content-Type: text/html\r\nContent-Length: "
         "123\r\nConnection: "
         "close\r\n\r\n",
         {{"Content-Type", "text/html"},
          {"Content-Length", "123"},
          {"Connection", "close"}}},

        // Headers with different capitalization
        {"Content-Type: application/json\r\nUSER-AGENT: "
         "MyBrowser/1.0\r\n\r\n",
         {{"Content-Type", "application/json"},
          {"USER-AGENT", "MyBrowser/1.0"}}},

        // Headers with whitespace
        {"Content-Type:   text/html  \r\nAccept:  application/json "
         "\r\n\r\n",
         {{"Content-Type", "text/html"}, {"Accept", "application/json"}}},

        // Headers with special characters in values
        {"Set-Cookie: session=abc123; Secure; "
         "HttpOnly\r\nX-API-Version: "
         "v2.1\r\n\r\n",
         {{"Set-Cookie", "session=abc123; Secure; HttpOnly"},
          {"X-API-Version", "v2.1"}}},

        // Multiple Set-Cookie headers (should not be combined)
        {"Set-Cookie: session=abc123; Secure\r\nSet-Cookie: "
         "theme=dark; "
         "Path=/\r\n\r\n",
         {{"Set-Cookie", "session=abc123; Secure"},
          {"Set-Cookie", "theme=dark; Path=/"}}},

        // Headers with standard special characters in values
        {"X-Custom-Header: "
         "value-with-dashes_and_underscores\r\nX-Another: "
         "!@#$%^&*()\r\n\r\n",
         {{"X-Custom-Header", "value-with-dashes_and_underscores"},
          {"X-Another", "!@#$%^&*()"}}},

        // Headers with quoted values
        {"ETag: \"abc123\"\r\nIf-Match: \"xyz789\"\r\n\r\n",
         {{"ETag", "\"abc123\""}, {"If-Match", "\"xyz789\""}}},

        // Headers with colons in values
        {"X-Timestamp: 2023-10-15T14:30:00Z\r\nVia: 1.1 "
         "proxy.example.com:8080\r\n\r\n",
         {{"X-Timestamp", "2023-10-15T14:30:00Z"},
          {"Via", "1.1 proxy.example.com:8080"}}},

        // Headers with commas in values (not interpreted as separators)
        {"Cache-Control: max-age=3600, public\r\nAccept: text/html, "
         "application/json\r\n\r\n",
         {{"Cache-Control", "max-age=3600, public"},
          {"Accept", "text/html, application/json"}}},

        // Empty header values
        {"X-Custom-Header:\r\nX-Another: value\r\n\r\n",
         {{"X-Custom-Header", ""}, {"X-Another", "value"}}},

        // Headers with various spacing patterns
        {"Header-With-Space: value \r\nNo-Space:value\r\n\r\n",
         {{"Header-With-Space", "value"}, {"No-Space", "value"}}},

        // Standard headers with typical values
        {"Host: example.com\r\nAccept-Language: en-US, "
         "en;q=0.5\r\nAccept-Encoding: gzip, deflate\r\n\r\n",
         {{"Host", "example.com"},
          {"Accept-Language", "en-US, en;q=0.5"},
          {"Accept-Encoding", "gzip, deflate"}}},

        // Authorization header
        {"Authorization: Basic dGVzdDp0ZXN0\r\nWWW-Authenticate: Basic "
         "realm=\"example\"\r\n\r\n",
         {{"Authorization", "Basic dGVzdDp0ZXN0"},
          {"WWW-Authenticate", "Basic realm=\"example\""}}},

        // Date header
        {"Date: Wed, 21 Oct 2015 07:28:00 GMT\r\nExpires: Thu, 22 Oct "
         "2015 "
         "07:28:00 GMT\r\n\r\n",
         {{"Date", "Wed, 21 Oct 2015 07:28:00 GMT"},
          {"Expires", "Thu, 22 Oct 2015 07:28:00 GMT"}}},

        // Multiple instances of the same header (should not be
        // combined)
        {"Warning: 199 - Example Warning\r\nWarning: 299 - Another "
         "Warning\r\n\r\n",
         {{"Warning", "199 - Example Warning"},
          {"Warning", "299 - Another Warning"}}},

        // Header with multiple colons in name
        {"Valid:Header: value\r\n\r\n", {{"Valid", "Header: value"}}},

        // Header with obs-text
        {"X-Header: value\xFF\xFE\xFD\r\n\r\n",
         {{"X-Header", "value\xFF\xFE\xFD"}}},

        // Header with empty value
        {"X-Header:\r\nX-Valid: value\r\n\r\n",
         {{"X-Header", ""}, {"X-Valid", "value"}}},

        // Header with hash
        {"X-Header#With-Hash: value\r\n\r\n",
         {{"X-Header#With-Hash", "value"}}},

        // Empty headers block
        {"\r\n", {}}};

    std::vector<std::pair<std::string, headers_t>> valid_header_test_cases2 = {
        // Complex case with many headers including duplicates
        {"Content-Type: text/html; charset=utf-8\r\n"
         "Content-Length: 2048\r\n"
         "Connection: keep-alive\r\n"
         "Cache-Control: no-cache, no-store, must-revalidate\r\n"
         "Set-Cookie: session=abc123; Path=/; Secure; HttpOnly\r\n"
         "Set-Cookie: preferences=dark_theme; Path=/; Expires=Wed, 21 "
         "Oct 2025 "
         "07:28:00 GMT\r\n"
         "X-Powered-By: MyFramework/2.0\r\n"
         "X-Request-ID: 550e8400-e29b-41d4-a716-446655440000\r\n"
         "X-Forwarded-For: 192.168.1.1, 10.0.0.1\r\n"
         "Via: 1.1 proxy1.example.com, 1.1 proxy2.example.com\r\n"
         "Accept-Ranges: bytes\r\n"
         "ETag: \"737060cd8c284d8af7ad3082f209582d\"\r\n"
         "Last-Modified: Wed, 21 Oct 2015 07:28:00 GMT\r\n"
         "Vary: Accept-Encoding, User-Agent\r\n"
         "Strict-Transport-Security: max-age=31536000; "
         "includeSubDomains\r\n"
         "Content-Security-Policy: default-src 'self'; img-src *; "
         "media-src "
         "media1.com media2.com\r\n"
         "X-Content-Type-Options: nosniff\r\n"
         "X-Frame-Options: DENY\r\n"
         "X-XSS-Protection: 1; mode=block\r\n"
         "Referrer-Policy: no-referrer-when-downgrade\r\n"
         "Feature-Policy: geolocation 'self'; microphone 'none'\r\n"
         "Server: MyServer/2.0\r\n"
         "Date: Wed, 21 Oct 2015 07:28:00 GMT\r\n"
         "\r\n",
         {{"Content-Type", "text/html; charset=utf-8"},
          {"Content-Length", "2048"},
          {"Connection", "keep-alive"},
          {"Cache-Control", "no-cache, no-store, must-revalidate"},
          {"Set-Cookie", "session=abc123; Path=/; Secure; HttpOnly"},
          {"Set-Cookie",
           "preferences=dark_theme; Path=/; Expires=Wed, 21 Oct 2025 "
           "07:28:00 GMT"},
          {"X-Powered-By", "MyFramework/2.0"},
          {"X-Request-ID", "550e8400-e29b-41d4-a716-446655440000"},
          {"X-Forwarded-For", "192.168.1.1, 10.0.0.1"},
          {"Via", "1.1 proxy1.example.com, 1.1 proxy2.example.com"},
          {"Accept-Ranges", "bytes"},
          {"ETag", "\"737060cd8c284d8af7ad3082f209582d\""},
          {"Last-Modified", "Wed, 21 Oct 2015 07:28:00 GMT"},
          {"Vary", "Accept-Encoding, User-Agent"},
          {"Strict-Transport-Security", "max-age=31536000; includeSubDomains"},
          {"Content-Security-Policy",
           "default-src 'self'; img-src *; media-src media1.com "
           "media2.com"},
          {"X-Content-Type-Options", "nosniff"},
          {"X-Frame-Options", "DENY"},
          {"X-XSS-Protection", "1; mode=block"},
          {"Referrer-Policy", "no-referrer-when-downgrade"},
          {"Feature-Policy", "geolocation 'self'; microphone 'none'"},
          {"Server", "MyServer/2.0"},
          {"Date", "Wed, 21 Oct 2015 07:28:00 GMT"}}},

        // Headers with various spacing and edge cases
        {"Header-No-Space:value\r\n"
         "Header-With-Space: value\r\n"
         "Header-With-Multiple-Spaces:   value   \r\n"
         "Header-With-Tab:\tvalue\t\r\n"
         "Header-With-Colon: value:with:colons\r\n"
         "Header-With-Comma: value,with,commas\r\n"
         "Header-With-Semicolon: value;with;semicolons\r\n"
         "Header-With-Quotes: \"value with quotes\"\r\n"
         "Header-With-Brackets: [value with brackets]\r\n"
         "Header-With-Braces: {value with braces}\r\n"
         "Header-With-Parens: (value with parens)\r\n"
         "Header-With-Special-Chars: !@#$%^&*()_+-=[]{}|;:',./<>?\r\n"
         "X-Empty-Header:\r\n"
         "X-Numeric-Value: 1234567890\r\n"
         "X-Boolean-Value: true\r\n"
         "X-JSON-Like: {\"key\": \"value\", \"array\": [1, 2, 3]}\r\n"
         "X-Long-Value: This is a very long header value that spans "
         "multiple words "
         "and contains various characters to test parsing of lengthy "
         "header values "
         "in HTTP implementations\r\n"
         "\r\n",
         {{"Header-No-Space", "value"},
          {"Header-With-Space", "value"},
          {"Header-With-Multiple-Spaces", "value"},
          {"Header-With-Tab", "value"},
          {"Header-With-Colon", "value:with:colons"},
          {"Header-With-Comma", "value,with,commas"},
          {"Header-With-Semicolon", "value;with;semicolons"},
          {"Header-With-Quotes", "\"value with quotes\""},
          {"Header-With-Brackets", "[value with brackets]"},
          {"Header-With-Braces", "{value with braces}"},
          {"Header-With-Parens", "(value with parens)"},
          {"Header-With-Special-Chars", "!@#$%^&*()_+-=[]{}|;:',./<>?"},
          {"X-Empty-Header", ""},
          {"X-Numeric-Value", "1234567890"},
          {"X-Boolean-Value", "true"},
          {"X-JSON-Like", "{\"key\": \"value\", \"array\": [1, 2, 3]}"},
          {"X-Long-Value",
           "This is a very long header value that spans multiple "
           "words and contains various characters to test parsing "
           "of lengthy header values in HTTP implementations"}}},

        // Multiple duplicate headers of different types
        {"Warning: 199 - Example Warning 1\r\n"
         "Warning: 299 - Example Warning 2\r\n"
         "Warning: 399 - Example Warning 3\r\n"
         "Set-Cookie: session=abc123; Path=/\r\n"
         "Set-Cookie: preferences=dark; Path=/settings\r\n"
         "Set-Cookie: analytics=allowed; Path=/; Domain=example.com\r\n"
         "X-Custom-Header: first value\r\n"
         "X-Custom-Header: second value\r\n"
         "X-Custom-Header: third value\r\n"
         "Via: 1.0 proxy1.example.com\r\n"
         "Via: 1.1 proxy2.example.com\r\n"
         "X-Forwarded-For: 192.168.1.1\r\n"
         "X-Forwarded-For: 10.0.0.1\r\n"
         "Cache-Control: no-cache\r\n"
         "Cache-Control: no-store\r\n"
         "Content-Type: text/html\r\n"
         "Content-Type: charset=utf-8\r\n" // This would be invalid in
                                           // practice but
         // tests parsing
         "\r\n",
         {{"Warning", "199 - Example Warning 1"},
          {"Warning", "299 - Example Warning 2"},
          {"Warning", "399 - Example Warning 3"},
          {"Set-Cookie", "session=abc123; Path=/"},
          {"Set-Cookie", "preferences=dark; Path=/settings"},
          {"Set-Cookie", "analytics=allowed; Path=/; Domain=example.com"},
          {"X-Custom-Header", "first value"},
          {"X-Custom-Header", "second value"},
          {"X-Custom-Header", "third value"},
          {"Via", "1.0 proxy1.example.com"},
          {"Via", "1.1 proxy2.example.com"},
          {"X-Forwarded-For", "192.168.1.1"},
          {"X-Forwarded-For", "10.0.0.1"},
          {"Cache-Control", "no-cache"},
          {"Cache-Control", "no-store"},
          {"Content-Type", "text/html"},
          {"Content-Type", "charset=utf-8"}}},

        // Headers with various date formats and time values
        {"Date: Wed, 21 Oct 2015 07:28:00 GMT\r\n"
         "Expires: Thu, 22 Oct 2015 07:28:00 GMT\r\n"
         "Last-Modified: Tue, 15 Nov 1994 12:45:26 GMT\r\n"
         "If-Modified-Since: Sat, 29 Oct 1994 19:43:31 GMT\r\n"
         "Retry-After: Fri, 31 Dec 1999 23:59:59 GMT\r\n"
         "X-Expiry-Date: 2025-12-31T23:59:59Z\r\n"
         "X-Timestamp: 1445412480\r\n"
         "Age: 3600\r\n"
         "\r\n",
         {{"Date", "Wed, 21 Oct 2015 07:28:00 GMT"},
          {"Expires", "Thu, 22 Oct 2015 07:28:00 GMT"},
          {"Last-Modified", "Tue, 15 Nov 1994 12:45:26 GMT"},
          {"If-Modified-Since", "Sat, 29 Oct 1994 19:43:31 GMT"},
          {"Retry-After", "Fri, 31 Dec 1999 23:59:59 GMT"},
          {"X-Expiry-Date", "2025-12-31T23:59:59Z"},
          {"X-Timestamp", "1445412480"},
          {"Age", "3600"}}},

        // Headers with authentication and security tokens
        {"Authorization: Basic dGVzdDp0ZXN0\r\n"
         "WWW-Authenticate: Basic realm=\"example\"\r\n"
         "Proxy-Authorization: Bearer "
         "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9."
         "eyJzdWIiOiIxMjM0NTY3ODkwIiwibmFtZSI6IkpvaG4gRG9lIiwiaWF0IjoxN"
         "TE2MjM5MDIyf"
         "Q.SflKxwRJSMeKKF2QT4fwpMeJf36POk6yJV_adQssw5c\r\n"
         "X-API-Key: "
         "abc123def456ghi789jkl012mno345pqr678stu901vwx234yz5\r\n"
         "X-Auth-Token: 550e8400-e29b-41d4-a716-446655440000\r\n"
         "X-CSRF-Token: "
         "5a6f7g8h9i0j1k2l3m4n5o6p7q8r9s0t1u2v3w4x5y6z7a8b9c0d1e2f\r\n"
         "X-Requested-With: XMLHttpRequest\r\n"
         "\r\n",
         {{"Authorization", "Basic dGVzdDp0ZXN0"},
          {"WWW-Authenticate", "Basic realm=\"example\""},
          {"Proxy-Authorization",
           "Bearer "
           "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9."
           "eyJzdWIiOiIxMjM0NTY3ODkwIiwibmFtZSI6IkpvaG4gRG9lIiwiaWF0Ijo"
           "xNTE2MjM5MDI"
           "yfQ.SflKxwRJSMeKKF2QT4fwpMeJf36POk6yJV_adQssw5c"},
          {"X-API-Key", "abc123def456ghi789jkl012mno345pqr678stu901vwx234yz5"},
          {"X-Auth-Token", "550e8400-e29b-41d4-a716-446655440000"},
          {"X-CSRF-Token",
           "5a6f7g8h9i0j1k2l3m4n5o6p7q8r9s0t1u2v3w4x5y6z7a8b9c0d1e2f"},
          {"X-Requested-With", "XMLHttpRequest"}}}};

    void test_headers_parse_valid(
        const std::vector<std::pair<std::string, headers_t>>& test_cases) {
        std::size_t test_i = 0;
        for (const auto& [fields_lines, entry] : test_cases) {
            test_i += 1;
            std::string test_i_str = std::to_string(test_i);
            parse_headers_context ctx;
            std::error_code ec;
            auto rbuf = ring_consumer(buffer(fields_lines));
            headers output;
            parse_headers(rbuf, ctx,
                          std::bind_front(insert_headers_fn, std::ref(output)),
                          ec);
            if (ctx.need_more()) {
                assert_true(!ec, "http headers parsing is not complete "
                                 "but error is set");
                ec = std::make_error_code(
                    std::errc::resource_unavailable_try_again);
            }
            assert_true(!ec, ("failed to parse valid headers (" + test_i_str +
                              "): " + ec.message())
                                 .c_str());
            assert_eq(
                entry.size(), output.size(),
                ("parsed headers count mismatch (" + test_i_str + ")").c_str());
            check_equal_headers(entry, output, test_i_str);
        }

        test_i = 0;
        for (const auto& [fields_lines, entry] : test_cases) {
            test_i += 1;
            if (fields_lines.empty()) {
                continue;
            }
            std::string test_i_str = std::to_string(test_i);
            std::vector<char> lines_buff(fields_lines.size());
            for (std::size_t step = 1; step < fields_lines.size(); ++step) {
                parse_headers_context ctx;
                auto rbuf = ring_consumer_producer(buffer(lines_buff));
                std::string_view lines_view = fields_lines;
                std::error_code ec;
                headers output;
                while (!lines_view.empty() && !ec) {
                    const std::size_t commit_size =
                        std::min(lines_view.size(), step);
                    rbuf.put_data(buffer(lines_view.substr(0, commit_size)));
                    lines_view.remove_prefix(commit_size);
                    parse_headers(
                        rbuf, ctx,
                        std::bind_front(insert_headers_fn, std::ref(output)),
                        ec);
                }
                if (ctx.need_more()) {
                    assert_true(!ec, "http headers parsing is not "
                                     "complete but error is set");
                    ec = std::make_error_code(
                        std::errc::resource_unavailable_try_again);
                }
                assert_true(!ec, ("failed to parse valid headers (" +
                                  test_i_str + "): " + ec.message())
                                     .c_str());
                assert_eq(entry.size(), output.size(),
                          ("parsed headers count mismatch (" + test_i_str + ")")
                              .c_str());
                check_equal_headers(entry, output, test_i_str);
            }
        }
    }

    const std::vector<std::pair<std::string, headers_t>>
        invalid_header_test_cases = {
            // Header with space before colon (invalid per RFC 7230)
            {
                "Invalid Header : value\r\n\r\n",
                {} // Should be rejected - space before colon
            },

            // Header without colon
            {
                "InvalidHeaderNoColon\r\n\r\n",
                {} // Should be rejected - missing colon
            },

            // Header with non-ASCII characters in name
            {
                "X-Header-With-\u00dcnicode: value\r\n\r\n",
                {} // Should be rejected - non-ASCII in header name
            },

            // Header with control characters in name (NUL character)
            {
                std::string{"X-Header-With-\x00-Control: value\r\n\r\n", 34},
                {} // Should be rejected - control character in name
            },

            // Header with newline in name
            {
                "X-Header-With-\n-Newline: value\r\n\r\n",
                {} // Should be rejected - newline in header name
            },

            // Header with carriage return in name
            {
                "X-Header-With-\r-Return: value\r\n\r\n",
                {} // Should be rejected - CR in header name
            },

            // Header with space in name (not around colon)
            {
                "X Header With Space: value\r\n\r\n",
                {} // Should be rejected - space in header name
            },

            // Header with invalid characters in name
            {
                "X-Header-With-@-Invalid: value\r\n\r\n",
                {} // Should be rejected - invalid character '@' in name
            },

            // Header with parentheses in name
            {
                "X-Header-With-(parens): value\r\n\r\n",
                {} // Should be rejected - parentheses in header name
            },

            // Empty header name
            {
                ": value\r\n\r\n", {} // Should be rejected - empty header name
            },

            // Header with only whitespace name
            {
                "   : value\r\n\r\n",
                {} // Should be rejected - whitespace-only name
            },

            // Missing CRLF terminator
            {
                "Content-Type: text/html", // Missing CRLF
                {} // Should be rejected - incomplete header block
            },

            // Invalid CRLF handling - LF instead of CRLF
            {
                "Content-Type: text/html\n\n",
                {} // Should be rejected - LF instead of CRLF
            },

            // Invalid CRLF handling - CR instead of CRLF
            {
                "Content-Type: text/html\r\r",
                {} // Should be rejected - CR instead of CRLF
            },

            // Header with obs-folded line (obsolete line folding)
            {
                "X-Long-Header: This is a very long header value that\r\n "
                "continues on the next line\r\n\r\n",
                {} // Should be rejected in strict parsing - obsolete line
                   // folding
            },

            // Header with null byte in value
            {
                "X-Header: value\x00with-null\r\n\r\n",
                {} // Should be rejected - NUL character in value
            },

            // Header with null byte in name
            {
                "X-Header\x00-With-Null: value\r\n\r\n",
                {} // Should be rejected - NUL character in name
            },

            // Duplicate Content-Length headers (invalid per RFC 7230)
            {
                "Content-Length: 100\r\nContent-Length: 200\r\n\r\n",
                {} // Should be rejected - duplicate Content-Length
            },

            // Duplicate Transfer-Encoding headers (invalid per RFC 7230)
            {
                "Transfer-Encoding: chunked\r\nTransfer-Encoding: "
                "gzip\r\n\r\n",
                {} // Should be rejected - duplicate Transfer-Encoding
            },

            // Duplicate Host headers (invalid per RFC 7230)
            {
                "Host: example.com\r\nHost: example.org\r\n\r\n",
                {} // Should be rejected - duplicate Host
            },

            // Header with invalid whitespace - space-only line
            {
                "X-Header: value\r\n \r\n\r\n",
                {} // Should be rejected - space-only line
            },

            // Header with invalid characters in value (vertical tab)
            {
                "X-Header: value\x0Bwith-vertical-tab\r\n\r\n",
                {} // Should be rejected - control character in value
            },

            // Header with invalid characters in value (bell character)
            {
                "X-Header: value\x07with-bell\r\n\r\n",
                {} // Should be rejected - control character in value
            },

            // Header with invalid characters in value (escape character)
            {
                "X-Header: value\x1Bwith-escape\r\n\r\n",
                {} // Should be rejected - control character in value
            },

            // Header with invalid characters in value (delete character)
            {
                "X-Header: value\x7Fwith-delete\r\n\r\n",
                {} // Should be rejected - control character in value
            },

            // Header with invalid characters in name (comma)
            {
                "X-Header,With-Comma: value\r\n\r\n",
                {} // Should be rejected - comma in header name
            },

            // Header with invalid characters in name (semicolon)
            {
                "X-Header;With-Semicolon: value\r\n\r\n",
                {} // Should be rejected - semicolon in header name
            },

            // Header with invalid characters in name (equals sign)
            {
                "X-Header=With-Equals: value\r\n\r\n",
                {} // Should be rejected - equals sign in header name
            },

            // Header with invalid characters in name (question mark)
            {
                "X-Header?With-Question: value\r\n\r\n",
                {} // Should be rejected - question mark in header name
            },

            // Header with invalid characters in name (backslash)
            {
                "X-Header\\With-Backslash: value\r\n\r\n",
                {} // Should be rejected - backslash in header name
            },

            // Header with invalid characters in name (quote)
            {
                "X-Header\"With-Quote: value\r\n\r\n",
                {} // Should be rejected - quote in header name
            },

            // Header with invalid characters in name (bracket)
            {
                "X-Header[With-Bracket: value\r\n\r\n",
                {} // Should be rejected - bracket in header name
            },

            // Header with invalid characters in name (brace)
            {
                "X-Header{With-Brace: value\r\n\r\n",
                {} // Should be rejected - brace in header name
            }};

    void test_headers_parse_invalid() {
        const auto& test_cases = invalid_header_test_cases;

        std::size_t test_i = 0;
        for (const auto& [fields_lines, entry] : test_cases) {
            test_i += 1;
            std::string test_i_str = std::to_string(test_i);
            parse_headers_context ctx;
            std::error_code ec;
            auto rbuf = ring_consumer(buffer(fields_lines));
            headers output;
            parse_headers(rbuf, ctx,
                          std::bind_front(insert_headers_fn, std::ref(output)),
                          ec);
            if (ctx.need_more()) {
                assert_true(!ec, "http headers parsing is not complete "
                                 "but error is set");
                ec = std::make_error_code(
                    std::errc::resource_unavailable_try_again);
            }
            assert_false(!ec, ("parsed and invalid headers (" + test_i_str +
                               "): " + ec.message())
                                  .c_str());
        }

        test_i = 0;
        for (const auto& [fields_lines, entry] : test_cases) {
            test_i += 1;
            if (fields_lines.empty()) {
                continue;
            }
            std::string test_i_str = std::to_string(test_i);
            std::vector<char> lines_buff(fields_lines.size());
            for (std::size_t step = 1; step < fields_lines.size(); ++step) {
                parse_headers_context ctx;
                auto rbuf = ring_consumer_producer(buffer(lines_buff));
                std::string_view lines_view = fields_lines;
                std::error_code ec;
                headers output;
                while (!lines_view.empty() && !ec) {
                    const std::size_t commit_size =
                        std::min(lines_view.size(), step);
                    rbuf.put_data(buffer(lines_view.substr(0, commit_size)));
                    lines_view.remove_prefix(commit_size);
                    parse_headers(
                        rbuf, ctx,
                        std::bind_front(insert_headers_fn, std::ref(output)),
                        ec);
                }
                if (ctx.need_more()) {
                    assert_true(!ec, "http headers parsing is not "
                                     "complete but error is set");
                    ec = std::make_error_code(
                        std::errc::resource_unavailable_try_again);
                }
                assert_false(!ec, ("parsed and invalid headers (" + test_i_str +
                                   "): " + ec.message())
                                      .c_str());
            }
        }
    }

    auto make_chunk_size_valid_cases() {
        std::vector<std::pair<std::string, std::uint64_t>> test_cases;
        test_cases.emplace_back("0", 0);
        test_cases.emplace_back("1", 1);
        test_cases.emplace_back("a", 10);
        test_cases.emplace_back("f", 15);
        test_cases.emplace_back("10", 16);
        test_cases.emplace_back("ff", 255);
        test_cases.emplace_back("100", 256);
        test_cases.emplace_back("fff", 4095);
        test_cases.emplace_back("1000", 4096);
        test_cases.emplace_back("ffff", 65535);
        test_cases.emplace_back("10000", 65536);
        test_cases.emplace_back("fffff", 1048575);
        test_cases.emplace_back("100000", 1048576);
        test_cases.emplace_back("ffffff", 16777215);
        test_cases.emplace_back("1000000", 16777216);
        test_cases.emplace_back("fffffff", 268435455);
        test_cases.emplace_back("10000000", 268435456);
        test_cases.emplace_back("ffffffff", 4294967295);
        return test_cases;
    }

    auto make_chunk_size_valid_64_cases() {
        std::vector<std::pair<std::string, std::uint64_t>> test_cases;
        if constexpr (std::numeric_limits<std::size_t>::max() >
                      std::numeric_limits<std::uint32_t>::max()) {
            test_cases.emplace_back("100000000", 4294967296);
            test_cases.emplace_back("7fffffffffffffff", 9223372036854775807ull);
            test_cases.emplace_back("8000000000000000", 9223372036854775808ull);
            test_cases.emplace_back("fffffffffffffffe",
                                    18446744073709551614ull);
            test_cases.emplace_back("ffffffffffffffff",
                                    18446744073709551615ull);
        }
        return test_cases;
    }

    auto make_chunk_size_invalid_32_cases() {
        std::vector<std::string> test_cases;
        if constexpr (std::numeric_limits<std::size_t>::max() ==
                      std::numeric_limits<std::uint32_t>::max()) {
            test_cases.emplace_back("100000000");
            test_cases.emplace_back("7fffffffffffffff");
            test_cases.emplace_back("8000000000000000");
            test_cases.emplace_back("fffffffffffffffe");
            test_cases.emplace_back("ffffffffffffffff");
        }
        return test_cases;
    }

    const std::vector<std::string> invalid_chunk_size_cases = {
        "10000000000000000",                // 2^64 (1 followed by 16 zeros)
        "10000000000000001",                // 2^64 + 1
        "ffffffffffffffff0",                // UINT64_MAX * 16
        "100000000000000000",               // 2^68
        "ffffffffffffffffff",               // UINT64_MAX * 16 + 15
        "1000000000000000000",              // Even larger
        "123456789abcdef0123456789abcdef0", // Very large value
    };

    void test_chunk_size_valid_cases(
        const std::vector<std::pair<std::string, std::uint64_t>>& test_cases) {
        for (const auto& [line, num] : test_cases) {
            // add non hex char as terminator
            std::string mut_line = line + 'h';
            parse_chunk_size_context ctx;
            std::error_code ec;
            auto rbuf = ring_consumer(buffer(mut_line));
            parse_chunk_size(rbuf, ctx, ec);
            if (ctx.need_more()) {
                assert_true(!ec, "http chunk size parsing is not "
                                 "complete but error is set");
                ec = std::make_error_code(
                    std::errc::resource_unavailable_try_again);
            }
            assert_true(!ec, ("failed to parse a valid chunk size (" + line +
                              "): " + ec.message())
                                 .c_str());
            assert_eq(num, ctx.chunk_size,
                      ("parsed chunk size mismatch (" + line + ")").c_str());
        }

        for (const auto& [cline, num] : test_cases) {
            if (cline.empty()) {
                continue;
            }
            std::string line = cline + 'h';
            std::vector<char> line_buff(line.size());
            for (std::size_t step = 1; step < line.size(); ++step) {
                parse_chunk_size_context ctx;
                auto rbuf = ring_consumer_producer(buffer(line_buff));
                std::string_view line_view = line;
                std::error_code ec;
                while (!line_view.empty() && !ec) {
                    const std::size_t commit_size =
                        std::min(line_view.size(), step);
                    rbuf.put_data(buffer(line_view.substr(0, commit_size)));
                    line_view.remove_prefix(commit_size);
                    parse_chunk_size(rbuf, ctx, ec);
                }
                if (ctx.need_more()) {
                    assert_true(!ec, "http status line parsing is not "
                                     "complete but error is set");
                    ec = std::make_error_code(
                        std::errc::resource_unavailable_try_again);
                }
                assert_true(!ec, ("failed to parse a valid chunk size (" +
                                  line + "): " + ec.message())
                                     .c_str());
                assert_eq(
                    num, ctx.chunk_size,
                    ("parsed chunk size mismatch (" + line + ")").c_str());
            }
        }
    }

    void
    test_chunk_size_invalid_cases(const std::vector<std::string>& test_cases) {
        for (const auto& line : test_cases) {
            // add non hex char as terminator
            std::string mut_line = line + 'h';
            parse_chunk_size_context ctx;
            std::error_code ec;
            auto rbuf = ring_consumer(buffer(mut_line));
            parse_chunk_size(rbuf, ctx, ec);
            if (ctx.need_more()) {
                assert_true(!ec, "http chunk size parsing is not "
                                 "complete but error is set");
                ec = std::make_error_code(
                    std::errc::resource_unavailable_try_again);
            }
            assert_false(!ec, ("parsed an invalid chunk size (" + line +
                               "): " + ec.message())
                                  .c_str());
        }

        for (const auto& cline : test_cases) {
            if (cline.empty()) {
                continue;
            }
            std::string line = cline + 'h';
            std::vector<char> line_buff(line.size());
            for (std::size_t step = 1; step < line.size(); ++step) {
                parse_chunk_size_context ctx;
                auto rbuf = ring_consumer_producer(buffer(line_buff));
                std::string_view line_view = line;
                std::error_code ec;
                while (!line_view.empty() && !ec) {
                    const std::size_t commit_size =
                        std::min(line_view.size(), step);
                    rbuf.put_data(buffer(line_view.substr(0, commit_size)));
                    line_view.remove_prefix(commit_size);
                    parse_chunk_size(rbuf, ctx, ec);
                }
                if (ctx.need_more()) {
                    assert_true(!ec, "http status line parsing is not "
                                     "complete but error is set");
                    ec = std::make_error_code(
                        std::errc::resource_unavailable_try_again);
                }
                assert_false(!ec, ("parsed an invalid chunk size (" + line +
                                   "): " + ec.message())
                                      .c_str());
            }
        }
    }

    std::vector<std::pair<std::string, chunk_extensions_t>>
        valid_chunk_extensions_cases = {
            // Basic extensions with various leading whitespace patterns
            {";ext1=value1", {{"ext1", "value1"}}},
            {" ;ext1=value1", {{"ext1", "value1"}}},  // Leading space before ;
            {"\t;ext1=value1", {{"ext1", "value1"}}}, // Leading tab before ;
            {"  ;ext1=value1",
             {{"ext1", "value1"}}}, // Leading multiple spaces before ;
            {" \t ;ext1=value1",
             {{"ext1", "value1"}}}, // Mixed leading whitespace before ;

            // Extensions with whitespace in various positions
            {" ; ext1=value1", {{"ext1", "value1"}}},
            {" ; ext1 =value1", {{"ext1", "value1"}}},
            {" ; ext1= value1", {{"ext1", "value1"}}},

            // Multiple extensions with leading whitespace
            {";ext1=value1;ext2=value2",
             {{"ext1", "value1"}, {"ext2", "value2"}}},
            {" ;ext1=value1;ext2=value2",
             {{"ext1", "value1"}, {"ext2", "value2"}}},
            {" ; ext1=value1 ; ext2=value2",
             {{"ext1", "value1"}, {"ext2", "value2"}}},
            {" \t ; ext1=value1 \t ; \t ext2=value2",
             {{"ext1", "value1"}, {"ext2", "value2"}}},

            // Flag extensions with leading whitespace
            {" ;flag", {{"flag", ""}}},
            {" \t ;flag", {{"flag", ""}}},
            {" ; flag ; ext=value", {{"flag", ""}, {"ext", "value"}}},

            // Quoted values with leading whitespace
            {" ; name=\"value\"", {{"name", "value"}}},
            {" \t ; name = \"value\"", {{"name", "value"}}},
    };

    std::vector<std::string> invalid_chunk_extensions_cases = {
        // Trailing whitespace after final value (not followed by semicolon)
        "; ext1=value1 ",
        "; name=\"value\" ",
        "; flag ",
        " ; flag ", // Even with leading whitespace, trailing whitespace is
                    // invalid

        // Whitespace within tokens (not allowed)
        "; ex t1=value1", // Space in name
        "; ext1=val ue1", // Space in unquoted value

        // Missing semicolon (just whitespace)
        " ext1=value1", // Whitespace but no semicolon

        // Other malformed cases
        "; =value",          // Missing name
        "; ext1=",           // Empty value after equals
        "; name=\"unclosed", // Unclosed quotes
    };

    void test_chunk_extensions_valid_cases() {
        const auto& test_cases = valid_chunk_extensions_cases;
        for (const auto& [eline, exts] : test_cases) {
            std::string line = eline + '\r';
            parse_chunk_extensions_context ctx;
            std::error_code ec;
            auto rbuf = ring_consumer(buffer(line));
            chunk_extensions_t output;
            parse_chunk_extensions(
                rbuf, ctx,
                std::bind_front(insert_chunk_extension, std::ref(output)), ec);
            if (ctx.need_more()) {
                assert_true(!ec, "http chunk extensions parsing is not "
                                 "complete but error is set");
                ec = std::make_error_code(
                    std::errc::resource_unavailable_try_again);
            }
            assert_true(!ec, ("failed to parse valid chunk extensions (" +
                              eline + "): " + ec.message())
                                 .c_str());
            assert_eq(exts.size(), output.size(),
                      ("parsed chunk extensions count mismatch (" + eline + ")")
                          .c_str());
            assert_eq(
                exts, output,
                ("parsed chunk extensions name value mismatch (" + eline + ")")
                    .c_str());
        }

        for (const auto& [eline, exts] : test_cases) {
            if (eline.empty()) {
                continue;
            }
            std::string line = eline + '\r';
            std::vector<char> lines_buff(line.size());
            for (std::size_t step = 1; step < line.size(); ++step) {
                parse_chunk_extensions_context ctx;
                auto rbuf = ring_consumer_producer(buffer(lines_buff));
                std::string_view line_view = line;
                std::error_code ec;
                chunk_extensions_t output;
                while (!line_view.empty() && !ec) {
                    const std::size_t commit_size =
                        std::min(line_view.size(), step);
                    rbuf.put_data(buffer(line_view.substr(0, commit_size)));
                    line_view.remove_prefix(commit_size);
                    parse_chunk_extensions(
                        rbuf, ctx,
                        std::bind_front(insert_chunk_extension,
                                        std::ref(output)),
                        ec);
                }
                if (ctx.need_more()) {
                    assert_true(!ec, "http chunk extensions parsing is "
                                     "not complete but error is set");
                    ec = std::make_error_code(
                        std::errc::resource_unavailable_try_again);
                }
                assert_true(!ec, ("failed to parse valid chunk "
                                  "extensions (" +
                                  eline + "): " + ec.message())
                                     .c_str());
                assert_eq(exts.size(), output.size(),
                          ("parsed chunk extensions count "
                           "mismatch (" +
                           eline + ")")
                              .c_str());
                assert_eq(exts, output,
                          ("parsed chunk extensions name value "
                           "mismatch (" +
                           eline + ")")
                              .c_str());
            }
        }
    }

    void test_chunk_extensions_invalid_cases() {
        const auto& test_cases = invalid_chunk_extensions_cases;
        for (const auto& eline : test_cases) {
            std::string line = eline + '\r';
            parse_chunk_extensions_context ctx;
            std::error_code ec;
            auto rbuf = ring_consumer(buffer(line));
            chunk_extensions_t output;
            parse_chunk_extensions(
                rbuf, ctx,
                std::bind_front(insert_chunk_extension, std::ref(output)), ec);
            if (ctx.need_more()) {
                assert_true(!ec, "http chunk extensions parsing is not "
                                 "complete but error is set");
                ec = std::make_error_code(
                    std::errc::resource_unavailable_try_again);
            }
            assert_false(!ec, ("parsed an invalid chunk extensions (" + eline +
                               "): " + ec.message())
                                  .c_str());
        }

        for (const auto& eline : test_cases) {
            if (eline.empty()) {
                continue;
            }
            std::string line = eline + '\r';
            std::vector<char> lines_buff(line.size());
            for (std::size_t step = 1; step < line.size(); ++step) {
                parse_chunk_extensions_context ctx;
                auto rbuf = ring_consumer_producer(buffer(lines_buff));
                std::string_view line_view = line;
                std::error_code ec;
                chunk_extensions_t output;
                while (!line_view.empty() && !ec) {
                    const std::size_t commit_size =
                        std::min(line_view.size(), step);
                    rbuf.put_data(buffer(line_view.substr(0, commit_size)));
                    line_view.remove_prefix(commit_size);
                    parse_chunk_extensions(
                        rbuf, ctx,
                        std::bind_front(insert_chunk_extension,
                                        std::ref(output)),
                        ec);
                }
                if (ctx.need_more()) {
                    assert_true(!ec, "http chunk extensions parsing is "
                                     "not complete but error is set");
                    ec = std::make_error_code(
                        std::errc::resource_unavailable_try_again);
                }
                assert_false(!ec, ("parsed an invalid chunk extensions (" +
                                   eline + "): " + ec.message())
                                      .c_str());
            }
        }
    }

    void test_trailing_crlf_valid_parse() {
        {
            auto rbuf = ring_consumer(buffer(CRLF));
            parse_chunk_trailing_crlf_ctx ctx;
            std::error_code ec;
            parse_chunk_trailing_crlf(rbuf, ctx, ec);
            if (ctx.need_more()) {
                assert_true(!ec, "http chunk crlf parsing is not "
                                 "complete but error is set");
                ec = std::make_error_code(
                    std::errc::resource_unavailable_try_again);
            }
            assert_true(
                !ec,
                ("failed to parse valid chunk crlf: " + ec.message()).c_str());
        }

        {
            parse_chunk_trailing_crlf_ctx ctx;
            std::array<char, 2> crlf_buff;
            auto rbuf = ring_consumer_producer(buffer(crlf_buff));
            rbuf.put_data(buffer(CRLF, 1));
            std::error_code ec;
            parse_chunk_trailing_crlf(rbuf, ctx, ec);
            assert_false(ctx.done(),
                         "http chunk crlf partial parsing is complete");
            assert_false(ctx.error(), "http chunk crlf partial parsing failed");
            assert_true(!ec, "http chunk crlf partial parsing failed");
            rbuf.put_data(buffer(CRLF.substr(1)));
            parse_chunk_trailing_crlf(rbuf, ctx, ec);
            assert_true(ctx.done(),
                        "http chunk crlf partial parsing is not complete");
            assert_false(ctx.error(), "http chunk crlf partial parsing failed");
            assert_true(!ec, "http chunk crlf partial parsing failed");
        }
    }

    const auto invalid_trailing_crlf_cases = std::array{
        "\r"sv, "\n"sv, "\r\r"sv, "\n\r"sv, "a"sv, "\rc"sv,
    };

    void test_trailing_crlf_invalid_parse() {
        for (const auto line : invalid_trailing_crlf_cases) {
            auto rbuf = ring_consumer(buffer(line));
            parse_chunk_trailing_crlf_ctx ctx;
            std::error_code ec;
            parse_chunk_trailing_crlf(rbuf, ctx, ec);
            if (ctx.need_more()) {
                assert_true(!ec, "http chunk crlf parsing is not "
                                 "complete but error is set");
                ec = std::make_error_code(
                    std::errc::resource_unavailable_try_again);
            }
            assert_false(!ec, "parsed invalid chunk crlf");
        }

        for (const auto& line : invalid_trailing_crlf_cases) {
            if (line.empty()) {
                continue;
            }
            std::vector<char> line_buff(line.size());
            for (std::size_t step = 1; step < line.size(); ++step) {
                parse_chunk_trailing_crlf_ctx ctx;
                auto rbuf = ring_consumer_producer(buffer(line_buff));
                std::string_view line_view = line;
                std::error_code ec;
                while (!line_view.empty() && !ec) {
                    const std::size_t commit_size =
                        std::min(line_view.size(), step);
                    rbuf.put_data(buffer(line_view.substr(0, commit_size)));
                    line_view.remove_prefix(commit_size);
                    parse_chunk_trailing_crlf(rbuf, ctx, ec);
                }
                if (ctx.need_more()) {
                    assert_true(!ec, "http chunk crlf parsing is not "
                                     "complete but error is set");
                    ec = std::make_error_code(
                        std::errc::resource_unavailable_try_again);
                }
                assert_false(!ec, "parsed invalid chunk crlf");
            }
        }
    }
} // namespace

namespace tests_fn {
    bool do_http_parser_tests() {
        try {
            test_request_line_parse_valid();
            test_request_line_parse_invalid();
            test_status_line_parse();
            test_headers_parse_valid(valid_header_test_cases);
            test_headers_parse_valid(valid_header_test_cases2);
            test_headers_parse_invalid();
            test_chunk_size_valid_cases(make_chunk_size_valid_cases());
            test_chunk_size_valid_cases(make_chunk_size_valid_64_cases());
            test_chunk_size_invalid_cases(make_chunk_size_invalid_32_cases());
            test_chunk_size_invalid_cases(invalid_chunk_size_cases);
            test_chunk_extensions_valid_cases();
            test_chunk_extensions_invalid_cases();
            test_trailing_crlf_valid_parse();
            test_trailing_crlf_invalid_parse();
        }
        catch (const exception& ex) {
            std::cout << "[!] http parser tests failed ! " << ex.detailed()
                      << "\n";
            return false;
        }
        catch (const std::exception& ex) {
            std::cout << "[!] http parser tests failed ! " << ex.what() << "\n";
            return false;
        }
        std::cout << "[*] http parser tests passed\n";
        return true;
    }
} // namespace tests_fn