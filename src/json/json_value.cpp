#include <rad/json/error.h>
#include <rad/json/value.h>
#include <rad/match.h>
#include <rad/utf.h>

#include <cassert>
#include <charconv>
#include <span>
#include <algorithm>
#include <system_error>

using namespace rad;
using namespace json;

array::array(std::size_t count, const value& v) : values_(count, v) {
}

array::array(std::size_t count) : values_(count) {
}

array::array(std::initializer_list<value> values) : values_(values) {
}

value& array::at(std::size_t pos) & {
    if (pos >= values_.size()) {
        throw std::system_error{make_error(error::out_of_range)};
    }
    return values_[pos];
}

const value& array::at(std::size_t pos) const& {
    if (pos >= values_.size()) {
        throw std::system_error{make_error(error::out_of_range)};
    }
    return values_[pos];
}

value&& array::at(std::size_t pos) && {
    if (pos >= values_.size()) {
        throw std::system_error{make_error(error::out_of_range)};
    }
    return std::move(values_[pos]);
}

value& array::operator[](std::size_t pos) & {
    return values_[pos];
}

const value& array::operator[](std::size_t pos) const& {
    return values_[pos];
}

value&& array::operator[](std::size_t pos) && {
    return std::move(values_[pos]);
}

value& array::front() & noexcept {
    return values_.front();
}

const value& array::front() const& noexcept {
    return values_.front();
}

value&& array::front() && noexcept {
    return std::move(values_.front());
}

value& array::back() & noexcept {
    return values_.back();
}

const value& array::back() const& noexcept {
    return values_.back();
}

value&& array::back() && noexcept {
    return std::move(values_.back());
}

value* array::if_contains(std::size_t pos) noexcept {
    if (pos < values_.size()) {
        return &values_[pos];
    }
    return nullptr;
}

const value* array::if_contains(std::size_t pos) const noexcept {
    if (pos < values_.size()) {
        return &values_[pos];
    }
    return nullptr;
}

void array::clear() noexcept {
    values_.clear();
}

void array::reserve(std::size_t count) {
    values_.reserve(count);
}

void array::resize(std::size_t count) {
    values_.resize(count);
}

void array::resize(std::size_t count, const value& v) {
    values_.resize(count, v);
}

void array::shrink_to_fit() {
    values_.shrink_to_fit();
}

auto array::erase(const_iterator pos) noexcept -> iterator {
    return values_.erase(pos);
}

auto array::erase(const_iterator first, const_iterator last) noexcept
    -> iterator {
    return values_.erase(first, last);
}

void array::push_back(const value& v) {
    values_.push_back(v);
}

void array::push_back(value&& v) {
    values_.push_back(std::move(v));
}

void array::pop_back() noexcept {
    values_.pop_back();
}

auto array::insert(const_iterator pos, const value& v) -> iterator {
    return values_.insert(pos, v);
}

auto array::insert(const_iterator pos, value&& v) -> iterator {
    return values_.insert(pos, std::move(v));
}

auto array::insert(const_iterator pos, std::size_t count, const value& v)
    -> iterator {
    return values_.insert(pos, count, v);
}

auto array::insert(const_iterator pos, std::initializer_list<value> values)
    -> iterator {
    return values_.insert(pos, values);
}

std::ostream& array::serialize_to_ostream(std::ostream& os) const {
    os << "[";
    if (!values_.empty()) {
        os << " ";
        auto sub_values =
            std::span{&*values_.begin(), &*std::prev(values_.end())};
        for (const auto& v : sub_values) {
            os << v << ", ";
        }
        const auto& v = values_.back();
        os << v << " ";
    }
    return os << "]";
}

object::object(std::size_t min_capacity) {
    keys_values_.reserve(min_capacity);
}

object::object(std::initializer_list<std::pair<std::string_view, value>> init,
               std::size_t min_capacity) {
    keys_values_.reserve(std::max(init.size(), min_capacity));
    for (const auto& [k, v] : init) {
        emplace(k, v);
    }
}

auto object::find(std::string_view key) noexcept -> iterator {
    auto it =
        std::find_if(keys_values_.begin(), keys_values_.end(),
                     [&](const key_value_pair& p) { return p.key() == key; });
    return it;
}

auto object::find(std::string_view key) const noexcept -> const_iterator {
    auto it =
        std::find_if(keys_values_.begin(), keys_values_.end(),
                     [&](const key_value_pair& p) { return p.key() == key; });
    return it;
}

value& object::at(std::string_view key) & {
    auto it = find(key);
    if (it == keys_values_.end()) {
        throw std::system_error{make_error(error::unknown_name),
                                std::string{key}};
    }
    return it->value();
}

const value& object::at(std::string_view key) const& {
    auto it = find(key);
    if (it == keys_values_.end()) {
        throw std::system_error{make_error(error::unknown_name),
                                std::string{key}};
    }
    return it->value();
}

value&& object::at(std::string_view key) && {
    auto it = find(key);
    if (it == keys_values_.end()) {
        throw std::system_error{make_error(error::unknown_name)};
    }
    return std::move(it->value());
}

void object::clear() noexcept {
    keys_values_.clear();
}

auto object::erase(const_iterator pos) noexcept -> iterator {
    return keys_values_.erase(pos);
}

std::size_t object::erase(std::string_view key) noexcept {
    auto it = find(key);
    if (it != end()) {
        keys_values_.erase(it);
        return 1;
    }
    return 0;
}

value* object::if_contains(std::string_view key) noexcept {
    auto it = find(key);
    if (it != end()) {
        return std::addressof(it->value());
    }
    return nullptr;
}

const value* object::if_contains(std::string_view key) const noexcept {
    auto it = find(key);
    if (it != end()) {
        return std::addressof(it->value());
    }
    return nullptr;
}

void object::insert(
    std::initializer_list<std::pair<std::string_view, value>> init) {
    if (init.size() == 0) {
        return;
    }
    keys_values_.reserve(keys_values_.size() + init.size());
    for (const auto& [k, v] : init) {
        emplace(k, v);
    }
}

value& object::operator[](std::string_view key) {
    auto it = find(key);
    if (it == end()) {
        it = keys_values_.insert(it, key_value_pair{key, nullptr});
    }
    return it->value();
}

void object::reserve(std::size_t cap) {
    keys_values_.reserve(cap);
}

auto object::stable_erase(const_iterator pos) noexcept -> iterator {
    return erase(pos);
}

std::size_t object::stable_erase(std::string_view key) noexcept {
    return erase(key);
}

bool object::equal(const object& other) const noexcept {
    if (size() != other.size()) {
        return false;
    }
    for (const auto& k_v : keys_values_) {
        auto it = other.find(k_v.key());
        if (it == other.end() || it->value() != k_v.value()) {
            return false;
        }
    }
    return true;
}

std::ostream& object::serialize_to_ostream(std::ostream& os) const {
    os << "{";
    if (!keys_values_.empty()) {
        os << " ";
        auto sub_keys_values =
            std::span{&*keys_values_.begin(), &*std::prev(keys_values_.end())};
        for (const auto& k_v : sub_keys_values) {
            os << "\"" << k_v.key() << "\": " << k_v.value() << ", ";
        }
        const auto& k_v = keys_values_.back();
        os << "\"" << k_v.key() << "\": " << k_v.value() << " ";
    }
    return os << "}";
}

bool& value::as_bool() {
    if (auto* p = std::get_if<bool>(&storage_)) {
        return *p;
    }
    throw std::system_error{make_error(error::not_bool)};
}

bool value::as_bool() const {
    if (auto* p = std::get_if<bool>(&storage_)) {
        return *p;
    }
    throw std::system_error{make_error(error::not_bool)};
}

std::int64_t& value::as_int64() {
    if (auto* p = std::get_if<std::int64_t>(&storage_)) {
        return *p;
    }
    throw std::system_error{make_error(error::not_int64)};
}

std::int64_t value::as_int64() const {
    if (auto* p = std::get_if<std::int64_t>(&storage_)) {
        return *p;
    }
    throw std::system_error{make_error(error::not_int64)};
}

std::uint64_t& value::as_uint64() {
    if (auto* p = std::get_if<std::uint64_t>(&storage_)) {
        return *p;
    }
    throw std::system_error{make_error(error::not_uint64)};
}

std::uint64_t value::as_uint64() const {
    if (auto* p = std::get_if<std::uint64_t>(&storage_)) {
        return *p;
    }
    throw std::system_error{make_error(error::not_uint64)};
}

double& value::as_double() {
    if (auto* p = std::get_if<double>(&storage_)) {
        return *p;
    }
    throw std::system_error{make_error(error::not_double)};
}

double value::as_double() const {
    if (auto* p = std::get_if<double>(&storage_)) {
        return *p;
    }
    throw std::system_error{make_error(error::not_double)};
}

std::string& value::as_string() & {
    if (auto* p = std::get_if<std::string>(&storage_)) {
        return *p;
    }
    throw std::system_error{make_error(error::not_string)};
}

const std::string& value::as_string() const& {
    if (auto* p = std::get_if<std::string>(&storage_)) {
        return *p;
    }
    throw std::system_error{make_error(error::not_string)};
}

std::string&& value::as_string() && {
    if (auto* p = std::get_if<std::string>(&storage_)) {
        return std::move(*p);
    }
    throw std::system_error{make_error(error::not_string)};
}

object& value::as_object() & {
    if (auto* p = std::get_if<object>(&storage_)) {
        return *p;
    }
    throw std::system_error{make_error(error::not_object)};
}

const object& value::as_object() const& {
    if (auto* p = std::get_if<object>(&storage_)) {
        return *p;
    }
    throw std::system_error{make_error(error::not_object)};
}

object&& value::as_object() && {
    if (auto* p = std::get_if<object>(&storage_)) {
        return std::move(*p);
    }
    throw std::system_error{make_error(error::not_object)};
}

array& value::as_array() & {
    if (auto* p = std::get_if<array>(&storage_)) {
        return *p;
    }
    throw std::system_error{make_error(error::not_array)};
}

const array& value::as_array() const& {
    if (auto* p = std::get_if<array>(&storage_)) {
        return *p;
    }
    throw std::system_error{make_error(error::not_array)};
}

array&& value::as_array() && {
    if (auto* p = std::get_if<array>(&storage_)) {
        return std::move(*p);
    }
    throw std::system_error{make_error(error::not_array)};
}

bool value::equal(const value& other) const noexcept {
    // convert int64_t to uint64_t if positive
    if (const std::int64_t* self_i = std::get_if<std::int64_t>(&storage_)) {
        if (*self_i >= 0) {
            if (const std::uint64_t* other_u =
                    std::get_if<std::uint64_t>(&other.storage_)) {
                return static_cast<std::uint64_t>(*self_i) == *other_u;
            }
        }
    }
    else if (const std::int64_t* other_i =
                 std::get_if<std::int64_t>(&other.storage_)) {
        if (*other_i >= 0) {
            if (const std::uint64_t* self_u =
                    std::get_if<std::uint64_t>(&storage_)) {
                return static_cast<std::uint64_t>(*other_i) == *self_u;
            }
        }
    }

    return storage_ == other.storage_;
}

std::ostream& value::serialize_to_ostream(std::ostream& os) const {
    return std::visit([&os](const auto& v) -> std::ostream& { return os << v; },
                      storage_);
}

void value_stack::push_bool(bool b) {
    assert(!expects_string_or_key());
    stack_.emplace_back(std::in_place_type<value>, b);
}

void value_stack::push_int64(std::int64_t i) {
    assert(!expects_string_or_key());
    stack_.emplace_back(std::in_place_type<value>, i);
}

void value_stack::push_uint64(std::uint64_t u) {
    assert(!expects_string_or_key());
    stack_.emplace_back(std::in_place_type<value>, u);
}

void value_stack::push_double(double d) {
    assert(!expects_string_or_key());
    stack_.emplace_back(std::in_place_type<value>, d);
}

void value_stack::push_null() {
    assert(!expects_string_or_key());
    stack_.emplace_back(std::in_place_type<value>, nullptr);
}

void value_stack::push_chars(std::string_view s) {
    has_pending_chars_ = true;
    stack_chars_ += s;
}

void value_stack::push_string(std::string_view s) {
    std::string jstr;
    if (expects_string_or_key()) {
        jstr += stack_chars_;
        stack_chars_.clear();
        has_pending_chars_ = false;
    }
    jstr += s;
    stack_.emplace_back(std::in_place_type<value>, std::move(jstr));
}

void value_stack::push_key(std::string_view k) {
    std::string jstr;
    if (expects_string_or_key()) {
        jstr += stack_chars_;
        stack_chars_.clear();
        has_pending_chars_ = false;
    }
    jstr += k;
    stack_.emplace_back(std::in_place_type<std::string>, std::move(jstr));
}

void value_stack::push_array(std::size_t n) {
    assert(!expects_string_or_key());
    assert(stack_.size() >= n);
    array a;
    a.reserve(n);
    std::size_t start_pos = stack_.size() - n;
    for (std::size_t i = 0; i < n; ++i) {
        a.push_back(std::move(std::get<value>(stack_[start_pos])));
        start_pos += 1;
    }
    stack_.erase(std::prev(stack_.end(), n), stack_.end());
    stack_.emplace_back(std::in_place_type<value>, std::move(a));
}

void value_stack::push_object(std::size_t n) {
    assert(!expects_string_or_key());
    assert(stack_.size() >= n * 2);
    object o;
    o.reserve(n);
    std::size_t start_pos = stack_.size() - n * 2;
    for (std::size_t i = 0; i < n; ++i) {
        std::string key = std::move(std::get<std::string>(stack_[start_pos]));
        start_pos += 1;
        value v = std::move(std::get<value>(stack_[start_pos]));
        start_pos += 1;
        o.insert_or_assign(key, std::move(v));
    }
    stack_.erase(std::prev(stack_.end(), n * 2), stack_.end());
    stack_.emplace_back(std::in_place_type<value>, std::move(o));
}

value value_stack::release() noexcept {
    assert(!expects_string_or_key());
    assert(stack_.size() == 1);
    assert(std::holds_alternative<value>(stack_.front()));

    value v = std::move(std::get<value>(stack_.front()));
    stack_.pop_back();
    return v;
}

void value_stack::reset() noexcept {
    stack_chars_.clear();
    stack_.clear();
    has_pending_chars_ = false;
}