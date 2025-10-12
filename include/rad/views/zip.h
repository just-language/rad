#pragma once
#include <rad/views/views_base.h>

#include <tuple>

namespace RAD_LIB_NAMESPACE {

    namespace views::detail {
        template <typename... T>
        class zip_struct {
        public:
            template <class... Iter>
            class iterator {
            public:
                iterator(std::tuple<Iter...> iters) : iters{std::move(iters)} {
                }

                iterator& operator++() {
                    auto increment = [](auto&&... elems) { (++elems, ...); };
                    std::apply(increment, iters);
                    return *this;
                }

                iterator operator++(int) {
                    iterator old_iter(*this);
                    operator++();
                    return old_iter;
                }

                iterator& operator--() {
                    auto decrement = [](auto&&... elems) { (++elems, ...); };
                    std::apply(decrement, iters);
                    return *this;
                }

                iterator operator--(int) {
                    iterator old_iter{*this};
                    operator--();
                    return *this;
                }

                auto operator*() {
                    auto dereference = [](auto&&... elems) {
                        return std::tuple<decltype(*elems)...>(
                            std::forward<decltype(*elems)>(*elems)...);
                    };
                    return std::apply(dereference, iters);
                }

                template <class... It>
                bool operator!=(const iterator<It...>& other) const {
                    return iters != other.iters;
                }

            private:
                std::tuple<Iter...> iters;
            };

            zip_struct(T&&... ranges)
                : stored_ranges{std::forward<T>(ranges)...} {
            }

            decltype(auto) begin() {
                auto generate_iter = [](auto&&... containers) {
                    return iterator<decltype(std::begin(containers))...>{
                        std::make_tuple(std::begin(containers)...)};
                };
                return std::apply(generate_iter, stored_ranges);
            }

            decltype(auto) begin() const {
                auto generate_iter = [](auto&&... containers) {
                    return iterator<decltype(std::begin(containers))...>{
                        std::make_tuple(std::begin(containers)...)};
                };
                return std::apply(generate_iter, stored_ranges);
            }

            decltype(auto) end() {
                auto generate_iter = [](auto&&... containers) {
                    return iterator<decltype(std::end(containers))...>{
                        std::make_tuple(std::end(containers)...)};
                };
                return std::apply(generate_iter, stored_ranges);
            }

            decltype(auto) end() const {
                auto generate_iter = [](auto&&... containers) {
                    return iterator<decltype(std::end(containers))...>{
                        std::make_tuple(std::end(containers)...)};
                };
                return std::apply(generate_iter, stored_ranges);
            }

            decltype(auto) rbegin() {
                auto generate_iter = [](auto&&... containers) {
                    return iterator<decltype(std::rbegin(containers))...>{
                        std::make_tuple(std::rbegin(containers)...)};
                };
                return std::apply(generate_iter, stored_ranges);
            }

            decltype(auto) rbegin() const {
                auto generate_iter = [](auto&&... containers) {
                    return iterator<decltype(std::rbegin(containers))...>{
                        std::make_tuple(std::rbegin(containers)...)};
                };
                return std::apply(generate_iter, stored_ranges);
            }

            decltype(auto) rend() {
                auto generate_iter = [](auto&&... containers) {
                    return iterator<decltype(std::rend(containers))...>{
                        std::make_tuple(std::rend(containers)...)};
                };
                return std::apply(generate_iter, stored_ranges);
            }

            decltype(auto) rend() const {
                auto generate_iter = [](auto&&... containers) {
                    return iterator<decltype(std::rend(containers))...>{
                        std::make_tuple(std::rend(containers)...)};
                };
                return std::apply(generate_iter, stored_ranges);
            }

        private:
            std::tuple<views_base::range_base<T>...> stored_ranges;
        };
    } // namespace views::detail

    // ranges must have the same length.
    template <typename... T>
    auto zip(T&&... ranges) {
        return views::detail::zip_struct<T...>{std::forward<T>(ranges)...};
    }

} // namespace RAD_LIB_NAMESPACE