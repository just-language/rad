#pragma once
#include <rad/json/json.h>
#include <rad/net/http/http_parser.h>
#include <rad/unittest/unittest.h>

#include <random>

namespace {
    using namespace RAD_LIB_NAMESPACE;
    using namespace unittest;

    std::string make_random_string(std::size_t i,
                                   std::default_random_engine& reng) {
        std::uniform_int_distribution<std::size_t> len_dist{1, 50};
        std::uniform_int_distribution<int> chars_dist{0, (('z' - 'a' + 1) * 2) +
                                                             9};

        std::string s(len_dist(reng), '\0');
        for (char& ch : s) {
            int c = chars_dist(reng);
            if (c >= 0 && c <= 9) {
                ch = static_cast<char>(c + '0');
            }
            else if (c >= 10 && c < 36) {
                ch = static_cast<char>(c - 10 + 'a');
            }
            else if (c >= 36 && c < 36 + 26) {
                ch = static_cast<char>(c - 36 + 'A');
            }
            else {
                c = 'l';
            }
        }
        return s;
    }

    json::array make_random_array(std::size_t nth,
                                  std::default_random_engine& reng,
                                  std::size_t depth);

    json::object make_random_object(std::size_t nth,
                                    std::default_random_engine& reng,
                                    std::size_t depth);

    json::value make_random_json_value(std::size_t nth,
                                       std::default_random_engine& reng,
                                       std::size_t depth) {
        constexpr std::int64_t max_javascript_int =
            std::int64_t{9007199254740991};
        constexpr std::int64_t min_javascript_int =
            std::int64_t{-9007199254740991};

        if (nth % 7 == 0) {
            return make_random_object(nth, reng, depth);
        }
        else if (nth % 6 == 0) {
            return make_random_array(nth, reng, depth);
        }
        else if (nth % 5 == 0) {
            return nullptr;
        }
        else if (nth % 4 == 0) {
            return nth % 2 == 0;
        }
        else if (nth % 3 == 0) {
            return make_random_string(nth, reng);
        }
        else if (nth % 2 == 0) {
            return std::uniform_int_distribution<std::int64_t>{
                min_javascript_int, max_javascript_int}(reng);
        }
        else {
            return std::uniform_int_distribution<std::int64_t>{
                min_javascript_int, max_javascript_int}(reng);
        }
    }

    json::array make_random_array(std::size_t nth,
                                  std::default_random_engine& reng,
                                  std::size_t depth) {
        if (nth == 0 || depth > 3) {
            return {};
        }
        std::size_t count = (nth % 9);
        json::array a;
        for (auto i : range(count)) {
            a.emplace_back(
                make_random_json_value(i ^ nth + 53, reng, depth + 1));
        }
        return a;
    }

    json::object make_random_object(std::size_t nth,
                                    std::default_random_engine& reng,
                                    std::size_t depth) {
        json::object o;
        if (depth > 3) {
            return o;
        }
        std::size_t count = (nth % 9);
        do {
            if (count > 0) {
                count -= 1;
            }
            o.emplace(make_random_string(nth ^ reng(), reng),
                      make_random_json_value(nth ^ reng(), reng, depth + 1));
        } while (count != 0);
        return o;
    }

    void validate_response_body(std::string_view res_body_text,
                                const json::object& body_object,
                                const net::http::headers& hdrs) {
        const auto res_object = json::parse(res_body_text);
        const auto& parsed_body = res_object.at("parsedBody").as_object();
        assert_eq(parsed_body, body_object,
                  "The JSON body is not equal to parsedBody in response");
    }
} // namespace