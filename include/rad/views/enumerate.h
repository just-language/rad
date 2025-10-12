#pragma once
#include <rad/views/views_base.h>

#include <tuple>

namespace RAD_LIB_NAMESPACE {

    namespace views::detail {

        template <class Iter>
        struct enumerate_iterator {
            size_t index = 0;
            Iter iter;
            using deref_type = decltype(*iter);

            enumerate_iterator(Iter it) : iter{std::move(it)} {
            }

            decltype(auto) operator*() {
                return std::pair<size_t, deref_type>{index, *iter};
            }

            enumerate_iterator& operator++() {
                ++index;
                ++iter;
                return *this;
            }

            enumerate_iterator& operator--() {
                --index;
                --iter;
                return *this;
            }

            template <class T>
            bool operator==(const enumerate_iterator<T>& it) const {
                return iter == it.iter;
            }

            template <class T>
            bool operator!=(const enumerate_iterator<T>& it) const {
                return iter != it.iter;
            }
        };

        template <class Container>
        struct enumerate_struct : private views_base::range_base<Container> {
            using base = views_base::range_base<Container>;
            using container_type = typename base::container_type;
            using param_type = typename base::param_type;

            enumerate_struct(Container&& container)
                : base(std::forward<Container>(container)) {
            }

            decltype(auto) begin() {
                return enumerate_iterator{base::begin()};
            }

            decltype(auto) begin() const {
                return enumerate_iterator{base::begin()};
            }

            decltype(auto) end() {
                return enumerate_iterator{base::end()};
            }

            decltype(auto) end() const {
                return enumerate_iterator{base::end()};
            }
        };

        struct enumerate_with_pipe {
            constexpr enumerate_with_pipe() = default;

            template <class Range>
            friend auto operator|(Range&& range, enumerate_with_pipe) noexcept;
        };
    } // namespace views::detail

    template <class Container>
    decltype(auto) enumerate(Container&& C) noexcept {
        return views::detail::enumerate_struct<Container>(
            std::forward<Container>(C));
    }

    template <class T, std::size_t N>
    decltype(auto) enumerate(T (&&a)[N]) noexcept {
        using array_type = decltype(std::forward<decltype(a)>(a));
        return views::detail::enumerate_struct<array_type>(
            std::forward<decltype(a)>(a));
    }

    constexpr auto enumerate() noexcept {
        return views::detail::enumerate_with_pipe{};
    }

    namespace views::detail {
        template <class Range>
        auto operator|(Range&& range, enumerate_with_pipe) noexcept {
            return enumerate(std::forward<Range>(range));
        }
    } // namespace views::detail

}; // namespace RAD_LIB_NAMESPACE