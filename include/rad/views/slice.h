#pragma once
#include <rad/views/views_base.h>

namespace RAD_LIB_NAMESPACE {

    namespace detail {
        template <class Ptr>
        struct slice_container {
            Ptr p;
            std::size_t size;

            slice_container(Ptr p, std::size_t size) noexcept
                : p{p}, size{size} {
            }

            Ptr begin() noexcept {
                return p;
            }

            Ptr end() noexcept {
                return p + size;
            }

            const Ptr begin() const noexcept {
                return p;
            }

            const Ptr end() const noexcept {
                return p + size;
            }
        };

        template <class Container>
        struct slice_range_struct : public views_base::range_base<Container> {
            using base = views_base::range_base<Container>;

            std::size_t from;
            std::size_t to;

            slice_range_struct(Container&& c, std::size_t from, std::size_t to)
                : base(std::forward<Container>(c)), from{from}, to{to} {
            }

            decltype(auto) begin() {
                return base::begin() + from;
            }

            decltype(auto) end() {
                return base::begin() + to;
            }

            decltype(auto) begin() const {
                return base::begin() + from;
            }

            decltype(auto) end() const {
                return base::begin() + to;
            }
        };

        struct slice_with_pipe_1_pos {
            std::size_t to;

            constexpr slice_with_pipe_1_pos(std::size_t to) noexcept : to{to} {
            }

            template <class Range>
            friend auto operator|(Range&& range, slice_with_pipe_1_pos slicer);
        };

        struct slice_with_pipe_2_pos {
            std::size_t from;
            std::size_t to;

            constexpr slice_with_pipe_2_pos(std::size_t from,
                                            std::size_t to) noexcept
                : from{from}, to{to} {
            }

            template <class Range>
            friend auto operator|(Range&& range, slice_with_pipe_2_pos slicer);
        };

    } // namespace detail

    // iterate from (from pos) to (to pos)
    template <class Range>
    auto slice(Range&& range, std::size_t from, std::size_t to) {
        using namespace detail;

        if constexpr (std::is_pointer_v<std::remove_cvref_t<Range>>) {
            return slice_range_struct{
                slice_container<std::remove_cvref_t<Range>>{range, to}, from,
                to};
        }
        else {
            return slice_range_struct<Range>(std::forward<Range>(range), from,
                                             to);
        }
    }

    // iterate from begin to begin + to
    template <class Range>
    auto slice(Range&& range, std::size_t to) {
        return slice(std::forward<Range>(range), 0, to);
    }

    template <class T, std::size_t N>
    auto slice(T (&&arr)[N], std::size_t from, std::size_t to) {
        using array_type = decltype(std::forward<decltype(arr)>(arr));
        return detail::slice_range_struct<array_type>(
            std::forward<decltype(arr)>(arr), from, to);
    }

    template <class T, std::size_t N>
    auto slice(T (&&arr)[N], std::size_t to) {
        using array_type = decltype(std::forward<decltype(arr)>(arr));
        return detail::slice_range_struct<array_type>(
            std::forward<decltype(arr)>(arr), 0, to);
    }

    // iterate from begin to begin + to
    constexpr auto slice(std::size_t to) noexcept {
        return detail::slice_with_pipe_1_pos{to};
    }

    constexpr auto slice(std::size_t from, std::size_t to) noexcept {
        return detail::slice_with_pipe_2_pos{from, to};
    }

    namespace detail {
        template <class Range>
        auto operator|(Range&& range, slice_with_pipe_1_pos slicer) {
            return slice(std::forward<Range>(range), slicer.to);
        }

        template <class Range>
        auto operator|(Range&& range, slice_with_pipe_2_pos slicer) {
            return slice(std::forward<Range>(range), slicer.from, slicer.to);
        }

    } // namespace detail

}; // namespace RAD_LIB_NAMESPACE