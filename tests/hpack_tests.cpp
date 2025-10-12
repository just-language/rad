#include <rad/net/http2/hpack.h>
#include <rad/unittest/unittest.h>

#include <format>
#include <iostream>

using namespace RAD_LIB_NAMESPACE;
using namespace unittest;
namespace http2 = net::http2;
namespace http = net::http;

namespace {
    struct hpack_test_case {
        bool indexed = false;
        bool huffman = false;
        bool never_indexed = false;
        std::vector<uint8_t> encoded;
        http::headers decoded;
        http::headers table;

        bool equal_table(
            const ring_buffer<std::pair<std::string, std::string>>& t) const {
            return table.size() == t.size() &&
                   std::equal(t.begin(), t.end(), table.begin());
        }
    };

    std::vector<hpack_test_case> build_test_cases1() {
        std::vector<hpack_test_case> cases;

        // case1
        {
            hpack_test_case c;
            c.indexed = true;
            c.huffman = false;
            c.never_indexed = false;
            c.encoded = std::vector<uint8_t>{
                {0x40, 0x0a, 0x63, 0x75, 0x73, 0x74, 0x6f, 0x6d, 0x2d,
                 0x6b, 0x65, 0x79, 0x0d, 0x63, 0x75, 0x73, 0x74, 0x6f,
                 0x6d, 0x2d, 0x68, 0x65, 0x61, 0x64, 0x65, 0x72}};
            c.decoded.insert("custom-key", "custom-header");
            c.table.insert("custom-key", "custom-header");

            cases.emplace_back(std::move(c));
        }

        // case2
        {
            hpack_test_case c;
            c.indexed = false;
            c.huffman = false;
            c.never_indexed = false;
            c.encoded = std::vector<uint8_t>{{0x04, 0x0c, 0x2f, 0x73, 0x61,
                                              0x6d, 0x70, 0x6c, 0x65, 0x2f,
                                              0x70, 0x61, 0x74, 0x68}};
            c.decoded.insert(":path", "/sample/path");

            cases.emplace_back(std::move(c));
        }

        // case3
        {
            hpack_test_case c;
            c.indexed = false;
            c.huffman = false;
            c.never_indexed = true;
            c.encoded = std::vector<uint8_t>{
                {0x10, 0x08, 0x70, 0x61, 0x73, 0x73, 0x77, 0x6f, 0x72, 0x64,
                 0x06, 0x73, 0x65, 0x63, 0x72, 0x65, 0x74}};
            c.decoded.insert("password", "secret");

            cases.emplace_back(std::move(c));
        }

        // case4
        {
            hpack_test_case c;
            c.indexed = false;
            c.huffman = false;
            c.never_indexed = false;
            c.encoded = std::vector<uint8_t>{0x82};
            c.decoded.insert(":method", "GET");

            cases.emplace_back(std::move(c));
        }

        return cases;
    }

    std::vector<hpack_test_case> build_test_cases2() {
        std::vector<hpack_test_case> cases;

        // case 1
        {
            hpack_test_case c;
            c.indexed = true;
            c.huffman = false;
            c.never_indexed = false;
            c.encoded = std::vector<uint8_t>{
                {0x82, 0x86, 0x84, 0x41, 0x0f, 0x77, 0x77, 0x77, 0x2e, 0x65,
                 0x78, 0x61, 0x6d, 0x70, 0x6c, 0x65, 0x2e, 0x63, 0x6f, 0x6d}};
            c.decoded.insert(":method", "GET");
            c.decoded.insert(":scheme", "http");
            c.decoded.insert(":path", "/");
            c.decoded.insert(":authority", "www.example.com");

            c.table.insert(":authority", "www.example.com");

            cases.emplace_back(std::move(c));
        }

        // case 2
        {
            hpack_test_case c;
            c.indexed = true;
            c.huffman = false;
            c.never_indexed = false;
            c.encoded = std::vector<uint8_t>{{0x82, 0x86, 0x84, 0xbe, 0x58,
                                              0x08, 0x6e, 0x6f, 0x2d, 0x63,
                                              0x61, 0x63, 0x68, 0x65}};
            c.decoded.insert(":method", "GET");
            c.decoded.insert(":scheme", "http");
            c.decoded.insert(":path", "/");
            c.decoded.insert(":authority", "www.example.com");
            c.decoded.insert("cache-control", "no-cache");

            c.table.insert(":authority", "www.example.com");
            c.table.insert("cache-control", "no-cache");

            cases.emplace_back(std::move(c));
        }

        // case 3
        {
            hpack_test_case c;
            c.indexed = true;
            c.huffman = false;
            c.never_indexed = false;
            c.encoded = std::vector<uint8_t>{
                {0x82, 0x87, 0x85, 0xbf, 0x40, 0x0a, 0x63, 0x75, 0x73, 0x74,
                 0x6f, 0x6d, 0x2d, 0x6b, 0x65, 0x79, 0x0c, 0x63, 0x75, 0x73,
                 0x74, 0x6f, 0x6d, 0x2d, 0x76, 0x61, 0x6c, 0x75, 0x65}};
            c.decoded.insert(":method", "GET");
            c.decoded.insert(":scheme", "https");
            c.decoded.insert(":path", "/index.html");
            c.decoded.insert(":authority", "www.example.com");
            c.decoded.insert("custom-key", "custom-value");

            c.table.insert(":authority", "www.example.com");
            c.table.insert("cache-control", "no-cache");
            c.table.insert("custom-key", "custom-value");

            cases.emplace_back(std::move(c));
        }

        return cases;
    }

    std::vector<hpack_test_case> build_test_cases3() {
        std::vector<hpack_test_case> cases;

        // case 1
        {
            hpack_test_case c;
            c.indexed = true;
            c.huffman = true;
            c.never_indexed = false;
            c.encoded = std::vector<uint8_t>{
                {0x82, 0x86, 0x84, 0x41, 0x8c, 0xf1, 0xe3, 0xc2, 0xe5, 0xf2,
                 0x3a, 0x6b, 0xa0, 0xab, 0x90, 0xf4, 0xff}};
            c.decoded.insert(":method", "GET");
            c.decoded.insert(":scheme", "http");
            c.decoded.insert(":path", "/");
            c.decoded.insert(":authority", "www.example.com");

            c.table.insert(":authority", "www.example.com");

            cases.emplace_back(std::move(c));
        }

        // case 2
        {
            hpack_test_case c;
            c.indexed = true;
            c.huffman = true;
            c.never_indexed = false;
            c.encoded =
                std::vector<uint8_t>{{0x82, 0x86, 0x84, 0xbe, 0x58, 0x86, 0xa8,
                                      0xeb, 0x10, 0x64, 0x9c, 0xbf}};
            c.decoded.insert(":method", "GET");
            c.decoded.insert(":scheme", "http");
            c.decoded.insert(":path", "/");
            c.decoded.insert(":authority", "www.example.com");
            c.decoded.insert("cache-control", "no-cache");

            c.table.insert(":authority", "www.example.com");
            c.table.insert("cache-control", "no-cache");

            cases.emplace_back(std::move(c));
        }

        // case 3
        {
            hpack_test_case c;
            c.indexed = true;
            c.huffman = true;
            c.never_indexed = false;
            c.encoded = std::vector<uint8_t>{
                {0x82, 0x87, 0x85, 0xbf, 0x40, 0x88, 0x25, 0xa8,
                 0x49, 0xe9, 0x5b, 0xa9, 0x7d, 0x7f, 0x89, 0x25,
                 0xa8, 0x49, 0xe9, 0x5b, 0xb8, 0xe8, 0xb4, 0xbf}};
            c.decoded.insert(":method", "GET");
            c.decoded.insert(":scheme", "https");
            c.decoded.insert(":path", "/index.html");
            c.decoded.insert(":authority", "www.example.com");
            c.decoded.insert("custom-key", "custom-value");

            c.table.insert(":authority", "www.example.com");
            c.table.insert("cache-control", "no-cache");
            c.table.insert("custom-key", "custom-value");

            cases.emplace_back(std::move(c));
        }

        return cases;
    }

    std::vector<hpack_test_case> build_test_cases4() {
        std::vector<hpack_test_case> cases;

        // case 1
        {
            hpack_test_case c;
            c.indexed = true;
            c.huffman = false;
            c.never_indexed = false;
            c.encoded = std::vector<uint8_t>{
                {0x48, 0x03, 0x33, 0x30, 0x32, 0x58, 0x07, 0x70, 0x72, 0x69,
                 0x76, 0x61, 0x74, 0x65, 0x61, 0x1d, 0x4d, 0x6f, 0x6e, 0x2c,
                 0x20, 0x32, 0x31, 0x20, 0x4f, 0x63, 0x74, 0x20, 0x32, 0x30,
                 0x31, 0x33, 0x20, 0x32, 0x30, 0x3a, 0x31, 0x33, 0x3a, 0x32,
                 0x31, 0x20, 0x47, 0x4d, 0x54, 0x6e, 0x17, 0x68, 0x74, 0x74,
                 0x70, 0x73, 0x3a, 0x2f, 0x2f, 0x77, 0x77, 0x77, 0x2e, 0x65,
                 0x78, 0x61, 0x6d, 0x70, 0x6c, 0x65, 0x2e, 0x63, 0x6f, 0x6d}};
            c.decoded.insert(":status", "302");
            c.decoded.insert("cache-control", "private");
            c.decoded.insert("date", "Mon, 21 Oct 2013 20:13:21 GMT");
            c.decoded.insert("location", "https://www.example.com");

            c.table.insert(":status", "302");
            c.table.insert("cache-control", "private");
            c.table.insert("date", "Mon, 21 Oct 2013 20:13:21 GMT");
            c.table.insert("location", "https://www.example.com");

            cases.emplace_back(std::move(c));
        }

        // case2
        {
            hpack_test_case c;
            c.indexed = true;
            c.huffman = false;
            c.never_indexed = false;
            c.encoded = std::vector<uint8_t>{
                {0x48, 0x03, 0x33, 0x30, 0x37, 0xc1, 0xc0, 0xbf}};
            c.decoded.insert(":status", "307");
            c.decoded.insert("cache-control", "private");
            c.decoded.insert("date", "Mon, 21 Oct 2013 20:13:21 GMT");
            c.decoded.insert("location", "https://www.example.com");

            c.table.insert("cache-control", "private");
            c.table.insert("date", "Mon, 21 Oct 2013 20:13:21 GMT");
            c.table.insert("location", "https://www.example.com");
            c.table.insert(":status", "307");

            cases.emplace_back(std::move(c));
        }

        // case3
        {
            hpack_test_case c;
            c.indexed = true;
            c.huffman = false;
            c.never_indexed = false;
            c.encoded = std::vector<uint8_t>{
                {0x88, 0xc1, 0x61, 0x1d, 0x4d, 0x6f, 0x6e, 0x2c, 0x20, 0x32,
                 0x31, 0x20, 0x4f, 0x63, 0x74, 0x20, 0x32, 0x30, 0x31, 0x33,
                 0x20, 0x32, 0x30, 0x3a, 0x31, 0x33, 0x3a, 0x32, 0x32, 0x20,
                 0x47, 0x4d, 0x54, 0xc0, 0x5a, 0x04, 0x67, 0x7a, 0x69, 0x70,
                 0x77, 0x38, 0x66, 0x6f, 0x6f, 0x3d, 0x41, 0x53, 0x44, 0x4a,
                 0x4b, 0x48, 0x51, 0x4b, 0x42, 0x5a, 0x58, 0x4f, 0x51, 0x57,
                 0x45, 0x4f, 0x50, 0x49, 0x55, 0x41, 0x58, 0x51, 0x57, 0x45,
                 0x4f, 0x49, 0x55, 0x3b, 0x20, 0x6d, 0x61, 0x78, 0x2d, 0x61,
                 0x67, 0x65, 0x3d, 0x33, 0x36, 0x30, 0x30, 0x3b, 0x20, 0x76,
                 0x65, 0x72, 0x73, 0x69, 0x6f, 0x6e, 0x3d, 0x31}};
            c.decoded.insert(":status", "200");
            c.decoded.insert("cache-control", "private");
            c.decoded.insert("date", "Mon, 21 Oct 2013 20:13:22 GMT");
            c.decoded.insert("location", "https://www.example.com");
            c.decoded.insert("content-encoding", "gzip");
            c.decoded.insert("set-cookie", "foo=ASDJKHQKBZXOQWEOPIUAXQWEOIU; "
                                           "max-age=3600; version=1");

            c.table.insert("date", "Mon, 21 Oct 2013 20:13:22 GMT");
            c.table.insert("content-encoding", "gzip");
            c.table.insert("set-cookie", "foo=ASDJKHQKBZXOQWEOPIUAXQWEOIU; "
                                         "max-age=3600; version=1");

            cases.emplace_back(std::move(c));
        }

        return cases;
    }

    std::vector<hpack_test_case> build_test_cases5() {
        std::vector<hpack_test_case> cases;

        // case 1
        {
            hpack_test_case c;
            c.indexed = true;
            c.huffman = true;
            c.never_indexed = false;
            c.encoded = std::vector<uint8_t>{
                {0x48, 0x82, 0x64, 0x02, 0x58, 0x85, 0xae, 0xc3, 0x77,
                 0x1a, 0x4b, 0x61, 0x96, 0xd0, 0x7a, 0xbe, 0x94, 0x10,
                 0x54, 0xd4, 0x44, 0xa8, 0x20, 0x05, 0x95, 0x04, 0x0b,
                 0x81, 0x66, 0xe0, 0x82, 0xa6, 0x2d, 0x1b, 0xff, 0x6e,
                 0x91, 0x9d, 0x29, 0xad, 0x17, 0x18, 0x63, 0xc7, 0x8f,
                 0x0b, 0x97, 0xc8, 0xe9, 0xae, 0x82, 0xae, 0x43, 0xd3}};
            c.decoded.insert(":status", "302");
            c.decoded.insert("cache-control", "private");
            c.decoded.insert("date", "Mon, 21 Oct 2013 20:13:21 GMT");
            c.decoded.insert("location", "https://www.example.com");

            c.table.insert(":status", "302");
            c.table.insert("cache-control", "private");
            c.table.insert("date", "Mon, 21 Oct 2013 20:13:21 GMT");
            c.table.insert("location", "https://www.example.com");

            cases.emplace_back(std::move(c));
        }

        // case2
        {
            hpack_test_case c;
            c.indexed = true;
            c.huffman = true;
            c.never_indexed = false;
            c.encoded = std::vector<uint8_t>{
                {0x48, 0x83, 0x64, 0x0e, 0xff, 0xc1, 0xc0, 0xbf}};
            c.decoded.insert(":status", "307");
            c.decoded.insert("cache-control", "private");
            c.decoded.insert("date", "Mon, 21 Oct 2013 20:13:21 GMT");
            c.decoded.insert("location", "https://www.example.com");

            c.table.insert("cache-control", "private");
            c.table.insert("date", "Mon, 21 Oct 2013 20:13:21 GMT");
            c.table.insert("location", "https://www.example.com");
            c.table.insert(":status", "307");

            cases.emplace_back(std::move(c));
        }

        // case3
        {
            hpack_test_case c;
            c.indexed = true;
            c.huffman = true;
            c.never_indexed = false;
            c.encoded = std::vector<uint8_t>{
                {0x88, 0xc1, 0x61, 0x96, 0xd0, 0x7a, 0xbe, 0x94, 0x10, 0x54,
                 0xd4, 0x44, 0xa8, 0x20, 0x05, 0x95, 0x04, 0x0b, 0x81, 0x66,
                 0xe0, 0x84, 0xa6, 0x2d, 0x1b, 0xff, 0xc0, 0x5a, 0x83, 0x9b,
                 0xd9, 0xab, 0x77, 0xad, 0x94, 0xe7, 0x82, 0x1d, 0xd7, 0xf2,
                 0xe6, 0xc7, 0xb3, 0x35, 0xdf, 0xdf, 0xcd, 0x5b, 0x39, 0x60,
                 0xd5, 0xaf, 0x27, 0x08, 0x7f, 0x36, 0x72, 0xc1, 0xab, 0x27,
                 0x0f, 0xb5, 0x29, 0x1f, 0x95, 0x87, 0x31, 0x60, 0x65, 0xc0,
                 0x03, 0xed, 0x4e, 0xe5, 0xb1, 0x06, 0x3d, 0x50, 0x07}};
            c.decoded.insert(":status", "200");
            c.decoded.insert("cache-control", "private");
            c.decoded.insert("date", "Mon, 21 Oct 2013 20:13:22 GMT");
            c.decoded.insert("location", "https://www.example.com");
            c.decoded.insert("content-encoding", "gzip");
            c.decoded.insert("set-cookie", "foo=ASDJKHQKBZXOQWEOPIUAXQWEOIU; "
                                           "max-age=3600; version=1");

            c.table.insert("date", "Mon, 21 Oct 2013 20:13:22 GMT");
            c.table.insert("content-encoding", "gzip");
            c.table.insert("set-cookie", "foo=ASDJKHQKBZXOQWEOPIUAXQWEOIU; "
                                         "max-age=3600; version=1");

            cases.emplace_back(std::move(c));
        }

        return cases;
    }

    void test_hpack_requests() {
        auto req_cases = build_test_cases1();
        for (const auto& c : req_cases) {
            http2::hpack_decoder decoder;
            http2::hpack_encoder encoder;

            decoder.set_max_table_size(4096);
            encoder.set_max_table_size(4096);

            std::error_code ec;
            http2::headers decoded;

            decoder.decode(buffer(c.encoded), decoded, ec);
            assert_true(!ec, "hpack failed to decode");

            std::vector<uint8_t> encoded;
            encoder.encode(c.decoded, dynamic_buffer(encoded), c.indexed,
                           c.huffman, c.never_indexed);

            assert_eq(decoded, c.decoded, "decoded requests don't match");
            assert_eq(encoded, c.encoded, "encoded requests don't match");
            assert_true(c.equal_table(decoder.table().headers()),
                        "decoder dynamic table don't match");
            assert_true(c.equal_table(encoder.table().headers()),
                        "encoder dynamic table don't match");
        }
    }

    void test_hpack_requests_no_huffman() {
        http2::hpack_encoder encoder;
        http2::hpack_decoder decoder;

        encoder.set_max_table_size(4096);
        decoder.set_max_table_size(4096);

        auto req_cases = build_test_cases2();
        size_t test_i = 0;

        for (const auto& c : req_cases) {
            test_i += 1;

            std::error_code ec;
            http2::headers decoded;

            decoder.decode(buffer(c.encoded), decoded, ec);
            assert_true(
                !ec,
                std::format("hpack failed to decode ({})", test_i).c_str());

            assert_eq(decoded, c.decoded,
                      std::format("decoded requests don't match ({})", test_i)
                          .c_str());
            assert_true(
                c.equal_table(decoder.table().headers()),
                std::format("decoder dynamic table don't match ({})", test_i)
                    .c_str());

            std::vector<uint8_t> encoded;
            encoder.encode(c.decoded, dynamic_buffer(encoded), c.indexed,
                           c.huffman, c.never_indexed);
            assert_eq(encoded, c.encoded,
                      std::format("encoded requests don't match ({})", test_i)
                          .c_str());
            assert_true(
                c.equal_table(encoder.table().headers()),
                std::format("encoder dynamic table don't match ({})", test_i)
                    .c_str());
        }
    }

    void test_hpack_requests_with_huffman() {
        http2::hpack_encoder encoder;
        http2::hpack_decoder decoder;

        encoder.set_max_table_size(4096);
        decoder.set_max_table_size(4096);

        auto req_cases = build_test_cases3();
        size_t test_i = 0;

        for (const auto& c : req_cases) {
            test_i += 1;

            std::error_code ec;
            http2::headers decoded;

            decoder.decode(buffer(c.encoded), decoded, ec);
            assert_true(
                !ec,
                std::format("hpack failed to decode ({})", test_i).c_str());

            assert_eq(decoded, c.decoded,
                      std::format("decoded requests don't match ({})", test_i)
                          .c_str());
            assert_true(
                c.equal_table(decoder.table().headers()),
                std::format("decoder dynamic table don't match ({})", test_i)
                    .c_str());

            std::vector<uint8_t> encoded;
            encoder.encode(c.decoded, dynamic_buffer(encoded), c.indexed,
                           c.huffman, c.never_indexed);
            assert_eq(encoded, c.encoded,
                      std::format("encoded requests don't match ({})", test_i)
                          .c_str());
            assert_true(
                c.equal_table(encoder.table().headers()),
                std::format("encoder dynamic table don't match ({})", test_i)
                    .c_str());
        }
    }

    void test_hpack_responses_no_huffman() {
        http2::hpack_encoder encoder;
        http2::hpack_decoder decoder;

        encoder.set_max_table_size(256);
        decoder.set_max_table_size(256);

        auto req_cases = build_test_cases4();
        size_t test_i = 0;

        for (const auto& c : req_cases) {
            test_i += 1;

            std::error_code ec;
            http2::headers decoded;

            decoder.decode(buffer(c.encoded), decoded, ec);
            assert_true(
                !ec,
                std::format("hpack failed to decode ({})", test_i).c_str());

            assert_eq(decoded, c.decoded,
                      std::format("decoded responses don't match ({})", test_i)
                          .c_str());
            assert_true(
                c.equal_table(decoder.table().headers()),
                std::format("decoder dynamic table don't match ({})", test_i)
                    .c_str());

            std::vector<uint8_t> encoded;
            encoder.encode(c.decoded, dynamic_buffer(encoded), c.indexed,
                           c.huffman, c.never_indexed);
            assert_eq(encoded, c.encoded,
                      std::format("encoded responses don't match ({})", test_i)
                          .c_str());
            assert_true(
                c.equal_table(encoder.table().headers()),
                std::format("encoder dynamic table don't match ({})", test_i)
                    .c_str());
        }
    }

    void test_hpack_responses_with_huffman() {
        http2::hpack_encoder encoder;
        http2::hpack_decoder decoder;

        encoder.set_max_table_size(256);
        decoder.set_max_table_size(256);

        auto req_cases = build_test_cases5();
        size_t test_i = 0;

        for (const auto& c : req_cases) {
            test_i += 1;

            std::error_code ec;
            http2::headers decoded;

            decoder.decode(buffer(c.encoded), decoded, ec);
            assert_true(
                !ec,
                std::format("hpack failed to decode ({})", test_i).c_str());

            assert_eq(decoded, c.decoded,
                      std::format("decoded responses don't match ({})", test_i)
                          .c_str());
            assert_true(
                c.equal_table(decoder.table().headers()),
                std::format("decoder dynamic table don't match ({})", test_i)
                    .c_str());

            std::vector<uint8_t> encoded;
            encoder.encode(c.decoded, dynamic_buffer(encoded), c.indexed,
                           c.huffman, c.never_indexed);
            assert_eq(encoded, c.encoded,
                      std::format("encoded responses don't match ({})", test_i)
                          .c_str());
            assert_true(
                c.equal_table(encoder.table().headers()),
                std::format("encoder dynamic table don't match ({})", test_i)
                    .c_str());
        }
    }
} // namespace

namespace tests_fn {
    bool do_hpack_tests() {
        try {
            test_hpack_requests();
            test_hpack_requests_no_huffman();
            test_hpack_requests_with_huffman();
            test_hpack_responses_no_huffman();
            test_hpack_responses_with_huffman();
        }
        catch (const exception& ex) {
            std::cout << "[!] hpack tests failed ! " << ex.what() << "\n";
            return false;
        }
        std::cout << "[*] hpack tests passed\n";
        return true;
    }
} // namespace tests_fn