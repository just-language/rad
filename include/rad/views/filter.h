#pragma once
#include <rad/views/views_base.h>

namespace RAD_LIB_NAMESPACE {
    namespace views::detail {
        struct end_mark {};

        template <class Iter, class EndIter, class FilterFn>
        class filter_iterator {
        public:
            filter_iterator(Iter iter, EndIter end_iter, FilterFn fn)
                : iter{std::move(iter)}, end_iter{std::move(end_iter)},
                  fn{std::move(fn)} {
                while (this->iter != this->end_iter) {
                    if (this->fn(*(this->iter))) {
                        break;
                    }
                    ++(this->iter);
                }
            }

            filter_iterator& operator++() {
                while (iter != end_iter) {
                    ++iter;
                    if (fn(*iter)) {
                        break;
                    }
                }
                return *this;
            }

            filter_iterator operator++(int) {
                filter_iterator old_iter{*this};
                operator++();
                return old_iter;
            }

            filter_iterator& operator--() {
                while (iter != end_iter) {
                    --iter;
                    if (fn(*iter)) {
                        break;
                    }
                }
                return *this;
            }

            filter_iterator operator--(int) {
                filter_iterator old_iter{*this};
                operator--();
                return old_iter;
            }

            decltype(auto) operator*() {
                return *iter;
            }

            bool operator==(end_mark) const {
                return iter == end_iter;
            }

            bool operator!=(end_mark) const {
                return iter != end_iter;
            }

        private:
            Iter iter;
            EndIter end_iter;
            FilterFn fn;
        };

        template <class Range, class Fn>
        class filter_struct : private views_base::range_base<Range> {
        public:
            using base = views_base::range_base<Range>;

            filter_struct(Range&& range, Fn fn)
                : base(std::forward<Range>(range)), fn{std::move(fn)} {
            }

            decltype(auto) begin() {
                return filter_iterator{base::begin(), base::end(), fn};
            }

            decltype(auto) begin() const {
                return filter_iterator{base::begin(), base::end(), fn};
            }

            decltype(auto) rbegin() {
                return filter_iterator{base::rbegin(), base::rend(), fn};
            }

            decltype(auto) rbegin() const {
                return filter_iterator{base::rbegin(), base::rend(), fn};
            }

            constexpr auto end() {
                return end_mark{};
            }

            constexpr auto end() const {
                return end_mark{};
            }

            constexpr auto rend() {
                return end_mark{};
            }

            constexpr auto rend() const {
                return end_mark{};
            }

        private:
            Fn fn;
        };

        template <class Fn>
        class filter_with_pipe {
        public:
            filter_with_pipe(Fn fn) : fn{std::move(fn)} {
            }

            template <class Range, class FilterFn>
            friend decltype(auto) operator|(Range&& range,
                                            filter_with_pipe<FilterFn>&& fn);

        private:
            Fn fn;
        };

    } // namespace views::detail

    template <class Range, class Fn>
    decltype(auto) filter(Range&& range, Fn&& fn) {
        return views::detail::filter_struct<Range, Fn>{
            std::forward<Range>(range), std::forward<Fn>(fn)};
    }

    template <class T, std::size_t N, class Fn>
    decltype(auto) filter(T (&&arr)[N], Fn&& fn) {
        return views::detail::filter_struct{std::forward<decltype(arr)>(arr),
                                            std::forward<Fn>(fn)};
    }

    template <class Fn>
    decltype(auto) filter(Fn&& fn) {
        return views::detail::filter_with_pipe{std::forward<Fn>(fn)};
    }

    namespace views::detail {
        template <class Range, class FilterFn>
        decltype(auto) operator|(Range&& range,
                                 filter_with_pipe<FilterFn>&& fn) {
            return filter(std::forward<Range>(range),
                          std::forward<FilterFn>(fn.fn));
        }
    } // namespace views::detail

} // namespace RAD_LIB_NAMESPACE
