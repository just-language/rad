#pragma once
#include <rad/libbase.h>

#include <iterator>

namespace RAD_LIB_NAMESPACE::views_base {

    template <class Container>
    struct range_base {
        using container_type = Container;
        using param_type = Container;
        container_type container;

        range_base(range_base&&) = default;

        range_base(const range_base&) = delete;

        range_base& operator=(range_base&&) = default;

        range_base& operator=(const range_base&) = delete;

        template <bool IsArray = std::is_array_v<Container>,
                  std::enable_if_t<!IsArray, int> = 0>
        range_base(Container&& cont)
            : container{std::forward<Container>(cont)} {
        }

        template <bool IsArray = std::is_array_v<Container>,
                  std::enable_if_t<IsArray, int> = 0>
        range_base(Container&& cont) {
            std::move(std::begin(cont), std::end(cont), std::begin(container));
        }

        decltype(auto) get_begin() {
            return std::begin(container);
        }

        decltype(auto) get_begin() const {
            return std::begin(container);
        }

        decltype(auto) get_rbegin() {
            return std::rbegin(container);
        }

        decltype(auto) get_rbegin() const {
            return std::rbegin(container);
        }

        decltype(auto) get_end() {
            return std::end(container);
        }

        decltype(auto) get_end() const {
            return std::end(container);
        }

        decltype(auto) get_rend() {
            return std::rend(container);
        }

        decltype(auto) get_rend() const {
            return std::rend(container);
        }

        decltype(auto) begin() {
            return get_begin();
        }

        decltype(auto) begin() const {
            return get_begin();
        }

        decltype(auto) end() {
            return get_end();
        }

        decltype(auto) end() const {
            return get_end();
        }

        decltype(auto) rbegin() {
            return get_rbegin();
        }

        decltype(auto) rbegin() const {
            return get_rbegin();
        }

        decltype(auto) rend() {
            return get_rend();
        }

        decltype(auto) rend() const {
            return get_rend();
        }
    };

    template <class Container>
    struct range_base<Container&> {
        using container_type = Container*;
        using param_type = Container&;
        container_type container;

        range_base(param_type cont) noexcept : container{&cont} {
        }

        range_base(range_base&&) = default;

        range_base(const range_base&) = delete;

        range_base& operator=(range_base&&) = default;

        range_base& operator=(const range_base&) = delete;

        decltype(auto) get_begin() {
            return std::begin(*container);
        }

        decltype(auto) get_begin() const {
            return std::begin(*container);
        }

        decltype(auto) get_rbegin() {
            return std::rbegin(*container);
        }

        decltype(auto) get_rbegin() const {
            return std::rbegin(*container);
        }

        decltype(auto) get_end() {
            return std::end(*container);
        }

        decltype(auto) get_end() const {
            return std::end(*container);
        }

        decltype(auto) get_rend() {
            return std::rend(*container);
        }

        decltype(auto) get_rend() const {
            return std::rend(*container);
        }

        decltype(auto) begin() {
            return get_begin();
        }

        decltype(auto) begin() const {
            return get_begin();
        }

        decltype(auto) end() {
            return get_end();
        }

        decltype(auto) end() const {
            return get_end();
        }

        decltype(auto) rbegin() {
            return get_rbegin();
        }

        decltype(auto) rbegin() const {
            return get_rbegin();
        }

        decltype(auto) rend() {
            return get_rend();
        }

        decltype(auto) rend() const {
            return get_rend();
        }
    };

    template <class Container>
    struct range_base<const Container&> {
        using container_type = const Container*;
        using param_type = const Container&;
        container_type container;

        range_base(param_type cont) : container{&cont} {
        }

        range_base(range_base&&) = default;

        range_base(const range_base&) = delete;

        range_base& operator=(range_base&&) = default;

        range_base& operator=(const range_base&) = delete;

        decltype(auto) get_begin() {
            return std::begin(*container);
        }

        decltype(auto) get_begin() const {
            return std::begin(*container);
        }

        decltype(auto) get_rbegin() {
            return std::rbegin(*container);
        }

        decltype(auto) get_rbegin() const {
            return std::rbegin(*container);
        }

        decltype(auto) get_end() {
            return std::end(*container);
        }

        decltype(auto) get_end() const {
            return std::end(*container);
        }

        decltype(auto) get_rend() {
            return std::rend(*container);
        }

        decltype(auto) get_rend() const {
            return std::rend(*container);
        }

        decltype(auto) begin() {
            return get_begin();
        }

        decltype(auto) begin() const {
            return get_begin();
        }

        decltype(auto) end() {
            return get_end();
        }

        decltype(auto) end() const {
            return get_end();
        }

        decltype(auto) rbegin() {
            return get_rbegin();
        }

        decltype(auto) rbegin() const {
            return get_rbegin();
        }

        decltype(auto) rend() {
            return get_rend();
        }

        decltype(auto) rend() const {
            return get_rend();
        }
    };

    template <class Container>
    struct range_base<Container&&> {
        using container_type = Container;
        using param_type = Container;
        Container container;

        range_base(range_base&&) = default;

        range_base(const range_base&) = delete;

        range_base& operator=(range_base&&) = default;

        range_base& operator=(const range_base&) = delete;

        template <bool IsArray = std::is_array_v<Container>,
                  std::enable_if_t<!IsArray, int> = 0>
        range_base(Container&& cont)
            : container{std::forward<Container>(cont)} {
        }

        template <bool IsArray = std::is_array_v<Container>,
                  std::enable_if_t<IsArray, int> = 0>
        range_base(Container&& cont) {
            std::move(std::begin(cont), std::end(cont), std::begin(container));
        }

        decltype(auto) get_begin() {
            return std::begin(container);
        }

        decltype(auto) get_begin() const {
            return std::begin(container);
        }

        decltype(auto) get_rbegin() {
            return std::rbegin(container);
        }

        decltype(auto) get_rbegin() const {
            return std::rbegin(container);
        }

        decltype(auto) get_end() {
            return std::end(container);
        }

        decltype(auto) get_end() const {
            return std::end(container);
        }

        decltype(auto) get_rend() {
            return std::rend(container);
        }

        decltype(auto) get_rend() const {
            return std::rend(container);
        }

        decltype(auto) begin() {
            return get_begin();
        }

        decltype(auto) begin() const {
            return get_begin();
        }

        decltype(auto) end() {
            return get_end();
        }

        decltype(auto) end() const {
            return get_end();
        }

        decltype(auto) rbegin() {
            return get_rbegin();
        }

        decltype(auto) rbegin() const {
            return get_rbegin();
        }

        decltype(auto) rend() {
            return get_rend();
        }

        decltype(auto) rend() const {
            return get_rend();
        }
    };

} // namespace RAD_LIB_NAMESPACE::views_base
