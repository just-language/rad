#pragma once
#include <rad/views/views_base.h>

#include <iterator>

namespace RAD_LIB_NAMESPACE {

    namespace views::detail {
        template <class Container>
        struct reversed_range_struct
            : private views_base::range_base<Container> {
            using base = views_base::range_base<Container>;
            using container_type = typename base::container_type;
            using param_type = typename base::param_type;

            reversed_range_struct(Container&& container)
                : base(std::forward<Container>(container)) {
            }

            decltype(auto) begin() {
                return base::get_rbegin();
            }

            decltype(auto) end() {
                return base::get_rend();
            }
        };

        struct reversed_range_with_pipe {
            template <class Range>
            friend auto operator|(Range&& range,
                                  reversed_range_with_pipe) noexcept;

            template <class T, std::size_t N>
            friend auto operator|(T (&&arr)[N], reversed_range_with_pipe);
        };

    } // namespace views::detail

    template <class Container>
    auto reversed(Container&& C) {
        using container_type = decltype(std::forward<Container>(C));
        return views::detail::reversed_range_struct<container_type>{
            std::forward<Container>(C)};
    }

    template <class T, std::size_t N>
    decltype(auto) reversed(T (&&arr)[N]) {
        using array_type = decltype(std::forward<decltype(arr)>(arr));
        return views::detail::reversed_range_struct<array_type>(
            std::forward<decltype(arr)>(arr));
    }

    constexpr auto reversed() noexcept {
        return views::detail::reversed_range_with_pipe{};
    }

    namespace views::detail {
        template <class Range>
        auto operator|(Range&& range, reversed_range_with_pipe) noexcept {
            return reversed(std::forward<Range>(range));
        }

        template <class T, std::size_t N>
        auto operator|(T (&&arr)[N], reversed_range_with_pipe) {
            return reversed(std::forward<decltype(arr)>(arr));
        }

    } // namespace views::detail

}; // namespace RAD_LIB_NAMESPACE
