#pragma once
#include <rad/views/views_base.h>

namespace RAD_LIB_NAMESPACE {

    namespace views::detail {
        template <class Range, class Fn>
        class takewhile_struct : private views_base::range_base<Range> {
        public:
            using base = views_base::range_base<Range>;

            struct end_mark {};

            template <class Iter, class EndIter>
            class iterator {
            public:
                iterator(Iter iter, EndIter end_iter, Fn fn)
                    : iter{std::move(iter)}, end_iter{std::move(end_iter)},
                      fn{std::move(fn)} {
                    if (!this->fn(*iter)) {
                        iter = end_iter;
                    }
                }

                iterator& operator++() {
                    ++iter;
                    if (!fn(*iter)) {
                        iter = end_iter;
                    }
                    return *this;
                }

                iterator operator++(int) {
                    iterator old_iter{*this};
                    operator++();
                    return old_iter;
                }

                decltype(auto) operator*() {
                    return *iter;
                }

                bool operator!=(end_mark) {
                    return iter != end_iter;
                }

            private:
                Iter iter;
                EndIter end_iter;
                Fn fn;
            };

            takewhile_struct(Range&& range, Fn fn)
                : base(std::forward<Range>(range)), fn{std::move(fn)} {
            }

            decltype(auto) begin() {
                return iterator{base::begin(), base::end(), fn};
            }

            decltype(auto) begin() const {
                return iterator{base::begin(), base::end(), fn};
            }

            end_mark end() const {
                return end_mark{};
            }

        private:
            Fn fn;
        };

        template <class Fn>
        class takewhile_with_pipe {
        public:
            takewhile_with_pipe(Fn fn) : fn{std::move(fn)} {
            }

            template <class Range, class Pred>
            friend decltype(auto) operator|(Range&& range,
                                            takewhile_with_pipe<Pred>&& pred);

        private:
            Fn fn;
        };

    } // namespace views::detail

    template <class Range, class Fn>
    decltype(auto) takewhile(Range&& range, Fn&& fn) {
        return views::detail::takewhile_struct<Range, Fn>{
            std::forward<Range>(range), std::forward<Fn>(fn)};
    }

    template <class T, std::size_t N, class Fn>
    decltype(auto) takewhile(T (&&arr)[N], Fn&& fn) {
        return views::detail::takewhile_struct{std::forward<decltype(arr)>(arr),
                                               std::forward<Fn>(fn)};
    }

    template <class Fn>
    decltype(auto) takewhile(Fn&& fn) {
        return views::detail::takewhile_with_pipe{std::forward<Fn>(fn)};
    }

    namespace views::detail {
        template <class Range, class Pred>
        decltype(auto) operator|(Range&& range,
                                 takewhile_with_pipe<Pred>&& pred) {
            return takewhile(std::forward<Range>(range),
                             std::forward<Pred>(pred.fn));
        }
    } // namespace views::detail

} // namespace RAD_LIB_NAMESPACE
