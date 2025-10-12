#include <rad/json/serializer.h>
#include <rad/match.h>

#include <array>
#include <limits>
#include <cmath>
#include <cassert>
#include <charconv>

using namespace rad;
using namespace json;

namespace {
    bool write_symbol(char*& buff, std::size_t& n, uint8_t& quotes, char s) {
        if (n == 0) {
            return false;
        }
        buff[0] = s;
        quotes += 1;
        buff += 1;
        n -= 1;
        return true;
    }

    char escape_char(char ch) {
        if (ch == '\t') {
            return 't';
        }
        if (ch == '\r') {
            return 'r';
        }
        if (ch == '\n') {
            return 'n';
        }
        if (ch == '\b') {
            return 'b';
        }
        if (ch == '\f') {
            return 'f';
        }
        return ch;
    }

    std::string serialize_value(serializer& sr) {
        constexpr std::size_t step_increase = 4096;
        std::string result;
        while (!sr.done()) {
            const std::size_t old_size = result.size();
            result.resize(old_size + step_increase);
            auto serialized = sr.read(result.data() + old_size, step_increase);
            result.resize(old_size + serialized.size());
        }
        return result;
    }
} // namespace

auto serializer::make_int64_value_entry(std::int64_t i) -> int64_value_entry {
    constexpr std::size_t max_digits =
        std::numeric_limits<std::int64_t>::digits10;
    std::array<char, max_digits * 2> buff;
    auto res = std::to_chars(buff.data(), buff.data() + buff.size(), i);
    assert(res.ec == std::errc{});
    if (res.ec != std::errc{}) {
        temp_text_ = "0";
    }
    else {
        temp_text_ = std::string_view(buff.data(), res.ptr);
    }
    return int64_value_entry{temp_text_};
}

auto serializer::make_uint64_value_entry(std::uint64_t u)
    -> uint64_value_entry {
    constexpr std::size_t max_digits =
        std::numeric_limits<std::uint64_t>::digits10;
    std::array<char, max_digits * 2> buff;
    auto res = std::to_chars(buff.data(), buff.data() + buff.size(), u);
    assert(res.ec == std::errc{});
    if (res.ec != std::errc{}) {
        temp_text_ = "0";
    }
    else {
        temp_text_ = std::string_view(buff.data(), res.ptr);
    }
    return uint64_value_entry{temp_text_};
}

auto serializer::make_double_value_entry(double d) -> double_value_entry {
    temp_text_.clear();
    if (std::isnan(d)) {
        if (opts_.allow_infinity_and_nan) {
            temp_text_ = "NaN";
        }
        else {
            temp_text_ = "0";
        }
    }
    else if (std::isinf(d)) {
        if (opts_.allow_infinity_and_nan) {
            if (d < 0) {
                temp_text_ = "-";
            }
            temp_text_ += "Infinity";
        }
        else {
            temp_text_ = "0";
        }
    }
    else {
        constexpr std::size_t max_double_digits =
            std::numeric_limits<double>::max_digits10;
        std::array<char, max_double_digits * 2> buff;
        auto res = std::to_chars(buff.data(), buff.data() + buff.size(), d);
        if (res.ec != std::errc{}) {
            temp_text_ = "0";
        }
        else {
            temp_text_.append(buff.data(), res.ptr);
        }
    }
    return double_value_entry{temp_text_};
}

auto serializer::make_item_type_from_value(const value& v) -> item_type {
    if (v.is_null()) {
        return null_value_entry{};
    }
    if (v.is_bool()) {
        return bool_value_entry{v.as_bool()};
    }
    if (v.is_int64()) {
        return make_int64_value_entry(v.as_int64());
    }
    if (v.is_uint64()) {
        return make_uint64_value_entry(v.as_uint64());
    }
    if (v.is_double()) {
        return make_double_value_entry(v.as_double());
    }
    if (v.is_string()) {
        std::string_view s = v.as_string();
        return string_value_entry{s};
    }
    if (v.is_array()) {
        const array& a = v.as_array();
        if (a.empty()) {
            return empty_array_entry{};
        }
        return array_value_entry{a.begin(), a.end()};
    }
    if (v.is_object()) {
        const object& o = v.as_object();
        if (o.empty()) {
            return empty_object_entry{};
        }
        return object_value_entry{o.begin(), o.end()};
    }
    assert(false);
    return bool_value_entry{v.as_bool()};
}

std::size_t serializer::write_piece_of_string(char* buff, std::size_t n,
                                              uint32_t& written,
                                              std::string_view text,
                                              bool pop_stack) {
    assert(written < text.size());
    const std::string_view part = text.substr(written, n);
    const std::size_t part_n = part.size();
    std::copy(part.data(), part.data() + part_n, buff);
    written += static_cast<uint32_t>(part_n);
    assert(written <= text.size());
    if (pop_stack && written == text.size()) {
        stack_.pop_back();
    }
    return part_n;
}

std::size_t serializer::write_piece_of_string(char* buff, std::size_t n,
                                              std::string_view& text,
                                              bool pop_stack) {
    assert(!text.empty());
    const std::string_view part = text.substr(0, n);
    const std::size_t part_n = part.size();
    std::copy(part.data(), part.data() + part_n, buff);
    text.remove_prefix(part_n);
    if (pop_stack && text.empty()) {
        stack_.pop_back();
    }
    return part_n;
}

std::size_t serializer::write_escaped_string(char* buff, std::size_t n,
                                             std::string_view& text,
                                             uint32_t& escape_len) {
    const std::size_t total_size = n;
    while (!text.empty() && n != 0) {
        char ch = text.front();

        if (ch == '\\' || ch == '\t' || ch == '\r' || ch == '\n' ||
            ch == '\b' || ch == '\f' || ch == '"' ||
            (ch == '/' && opts_.escape_solidus)) {
            if (escape_len == 0) {
                buff[0] = '\\';
                buff += 1;
                n -= 1;
                if (n > 0) {
                    buff[0] = escape_char(ch);
                    buff += 1;
                    n -= 1;
                    text.remove_prefix(1);
                    continue;
                }
                else {
                    escape_len = 1;
                    return total_size - n;
                }
            }
            else {
                escape_len = 0;
                buff[0] = escape_char(ch);
                buff += 1;
                n -= 1;
                text.remove_prefix(1);
                continue;
            }
        }
        else if (ch >= 0 && ch <= 0x1f) {
            // \u00XX
            if (escape_len == 0) {
                escape_len = 6;
            }
            std::string hex_str{"\\u0000"};
            uint32_t ch_val = ch;
            auto res =
                std::to_chars(hex_str.data() + 4,
                              hex_str.data() + hex_str.size(), ch_val, 16);
            assert(res.ec == std::errc{});
            std::ignore = res;
            uint32_t written_escape = 6 - escape_len;
            const std::size_t written =
                write_piece_of_string(buff, n, written_escape, hex_str, false);
            n -= written;
            buff += written;
            escape_len = 6 - written_escape;
            if (escape_len == 0) {
                text.remove_prefix(1);
            }
            continue;
        }
        else {
            // printable ascii char (ch > 0x1f && ch <= 127) or utf8
            // byte
            buff[0] = ch;
            buff += 1;
            n -= 1;
            text.remove_prefix(1);
            continue;
        }
    }
    return total_size - n;
}

std::size_t serializer::write_quoted_string(char* buff, std::size_t n,
                                            string_value_entry& e) {
    const std::size_t saved_n = n;
    if (e.quotes == 0) {
        buff[0] = '"';
        buff += 1;
        e.quotes += 1;
        n -= 1;
        if (n == 0) {
            return 1;
        }
    }

    const std::size_t written =
        write_escaped_string(buff, n, e.s, e.escape_len);
    assert(written <= n);
    buff += written;
    n -= written;
    if (n == 0) {
        return saved_n - n;
    }

    buff[0] = '"';
    n -= 1;
    stack_.pop_back();
    return saved_n - n;
}

std::size_t serializer::write_quoted_key(char* buff, std::size_t n,
                                         key_entry& e) {
    const std::size_t saved_n = n;
    if (e.quotes == 0) {
        if (!write_symbol(buff, n, e.quotes, '"')) {
            return saved_n - n;
        }
    }

    const std::size_t written =
        write_escaped_string(buff, n, e.s, e.escape_len);
    assert(written <= n);
    buff += written;
    n -= written;
    if (n == 0) {
        return saved_n - n;
    }

    if (e.quotes == 1) {
        if (!write_symbol(buff, n, e.quotes, '"')) {
            return saved_n - n;
        }
    }
    if (e.quotes == 2) {
        if (!write_symbol(buff, n, e.quotes, ':')) {
            return saved_n - n;
        }
    }
    if (write_symbol(buff, n, e.quotes, ' ')) {
        stack_.pop_back();
    }
    return saved_n - n;
}

std::size_t serializer::serialize_object(char* buff, std::size_t n,
                                         object_value_entry& e) {
    const std::size_t saved_n = n;
    if (!e.first_bracket) {
        buff[0] = '{';
        buff += 1;
        n -= 1;
        e.first_bracket = true;
        auto current = e.current;
        std::advance(e.current, 1);
        if (e.current != e.end) {
            stack_.emplace_back(comma_space_entry{});
        }
        stack_.emplace_back(make_item_type_from_value(current->value()));
        stack_.emplace_back(key_entry{current->key()});
        return 1;
    }
    if (e.current == e.end) {
        buff[0] = '}';
        stack_.pop_back();
        return 1;
    }

    auto current = e.current;
    std::advance(e.current, 1);
    if (e.current != e.end) {
        stack_.emplace_back(comma_space_entry{});
    }
    stack_.emplace_back(make_item_type_from_value(current->value()));
    stack_.emplace_back(key_entry{current->key()});
    return saved_n - n;
}

std::size_t serializer::serialize_array(char* buff, std::size_t n,
                                        array_value_entry& e) {
    const std::size_t saved_n = n;
    if (!e.first_bracket) {
        buff[0] = '[';
        buff += 1;
        n -= 1;
        e.first_bracket = true;
        auto current = e.current;
        std::advance(e.current, 1);
        if (e.current != e.end) {
            stack_.emplace_back(comma_space_entry{});
        }
        stack_.emplace_back(make_item_type_from_value(*current));
        return 1;
    }
    if (e.current == e.end) {
        buff[0] = ']';
        stack_.pop_back();
        return 1;
    }

    auto current = e.current;
    std::advance(e.current, 1);
    if (e.current != e.end) {
        stack_.emplace_back(comma_space_entry{});
    }
    stack_.emplace_back(make_item_type_from_value(*current));
    return saved_n - n;
}

serializer::serializer(const serialize_options& opts) : opts_{opts} {
    stack_.emplace_back(null_value_entry{});
}

void serializer::reset(const value& v) {
    stack_.clear();
    stack_.emplace_back(make_item_type_from_value(v));
}

void serializer::reset(const object& v) {
    stack_.clear();
    if (v.empty()) {
        stack_.emplace_back(empty_object_entry{});
    }
    else {
        stack_.emplace_back(object_value_entry{v.begin(), v.end()});
    }
}

void serializer::reset(const array& v) {
    stack_.clear();
    if (v.empty()) {
        stack_.emplace_back(empty_array_entry{});
    }
    else {
        stack_.emplace_back(array_value_entry{v.begin(), v.end()});
    }
}

void serializer::reset(std::string_view v) {
    stack_.clear();
    stack_.emplace_back(string_value_entry{v});
}

void serializer::reset(std::nullptr_t) {
    stack_.clear();
    stack_.emplace_back(null_value_entry{});
}

void serializer::reset(bool v) {
    stack_.clear();
    stack_.emplace_back(bool_value_entry{v});
}

void serializer::reset(std::int64_t v) {
    stack_.clear();
    stack_.emplace_back(make_int64_value_entry(v));
}

void serializer::reset(std::uint64_t v) {
    stack_.clear();
    stack_.emplace_back(make_uint64_value_entry(v));
}

void serializer::reset(double v) {
    stack_.clear();
    stack_.emplace_back(make_double_value_entry(v));
}

std::size_t serializer::read_into(char* buff, std::size_t n) {
    if (done() || n == 0) {
        return 0;
    }
    const std::size_t saved_n = n;
    while (n > 0 && !done()) {
        std::size_t written = match(
            stack_.back(),
            [&](null_value_entry& e) {
                return write_piece_of_string(buff, n, e.written, "null");
            },
            [&](bool_value_entry& e) {
                return write_piece_of_string(buff, n, e.written,
                                             e.b ? "true" : "false");
            },
            [&](int64_value_entry& e) {
                return write_piece_of_string(buff, n, e.text);
            },
            [&](uint64_value_entry& e) {
                return write_piece_of_string(buff, n, e.text);
            },
            [&](double_value_entry& e) {
                return write_piece_of_string(buff, n, e.text);
            },
            [&](string_value_entry& e) {
                return write_quoted_string(buff, n, e);
            },
            [&](key_entry& e) { return write_quoted_key(buff, n, e); },
            [&](comma_space_entry& e) {
                return write_piece_of_string(buff, n, e.written, ", ");
            },
            [&](empty_array_entry& e) {
                return write_piece_of_string(buff, n, e.written, "[]");
            },
            [&](empty_object_entry& e) {
                return write_piece_of_string(buff, n, e.written, "{}");
            },
            [&](array_value_entry& e) { return serialize_array(buff, n, e); },
            [&](object_value_entry& e) {
                return serialize_object(buff, n, e);
            });
        assert(written <= n);
        n -= written;
        buff += written;
    }
    return saved_n - n;
}

std::string json::serialize(const value& v, const serialize_options& opts) {
    serializer sr{opts};
    sr.reset(v);
    return serialize_value(sr);
}

std::string json::serialize(const object& o, const serialize_options& opts) {
    serializer sr{opts};
    sr.reset(o);
    return serialize_value(sr);
}

std::string json::serialize(const array& a, const serialize_options& opts) {
    serializer sr{opts};
    sr.reset(a);
    return serialize_value(sr);
}

std::string json::serialize(const std::string& s,
                            const serialize_options& opts) {
    serializer sr{opts};
    sr.reset(s);
    return serialize_value(sr);
}

std::string json::serialize(std::string_view s, const serialize_options& opts) {
    serializer sr{opts};
    sr.reset(s);
    return serialize_value(sr);
}

std::string json::serialize(const char* s, std::size_t n,
                            const serialize_options& opts) {
    serializer sr{opts};
    sr.reset(s, n);
    return serialize_value(sr);
}

std::string json::serialize(std::uint64_t u, const serialize_options& opts) {
    constexpr std::size_t max_digits =
        std::numeric_limits<std::uint64_t>::digits10;
    std::array<char, max_digits * 2> buff;
    auto res = std::to_chars(buff.data(), buff.data() + buff.size(), u);
    assert(res.ec == std::errc{});
    if (res.ec != std::errc{}) {
        return "0";
    }
    else {
        return std::string(buff.data(), res.ptr);
    }
}

std::string json::serialize(std::int64_t i, const serialize_options& opts) {
    constexpr std::size_t max_digits =
        std::numeric_limits<std::int64_t>::digits10;
    std::array<char, max_digits * 2> buff;
    auto res = std::to_chars(buff.data(), buff.data() + buff.size(), i);
    assert(res.ec == std::errc{});
    if (res.ec != std::errc{}) {
        return "0";
    }
    else {
        return std::string(buff.data(), res.ptr);
    }
}

std::string json::serialize(double d, const serialize_options& opts) {
    serializer sr{opts};
    sr.reset(d);
    return serialize_value(sr);
}

std::string json::serialize(bool b, const serialize_options& opts) {
    return b ? "true" : "false";
}

std::string json::serialize(std::nullptr_t, const serialize_options& opts) {
    return "null";
}
