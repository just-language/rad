#pragma once
#include <rad/views/views_base.h>

namespace RAD_LIB_NAMESPACE {
    namespace views::detail {
        template <class Range, class Fn>
        class dropwhile_struct : private views_base::range_base<Range> {
        public:
            using base = views_base::range_base<Range>;

            template <class Iter>
            class iterator {
            public:
                template <class EndIter>
                iterator(Iter iter, EndIter end_iter, Fn fn)
                    : iter{std::move(iter)}, fn{std::move(fn)} {
                    if (!IsEndIter) {
                        advance_first_time(std::move(end_iter));
                    }
                }

                iterator(Iter end_iter, Fn fn)
                    : iter{std::move(end_iter)}, fn{std::move(fn)} {
                }

                iterator& operator++() {
                    ++iter;
                    return *this;
                }

                iterator operator++(int) {
                    iterator old_iter{*this};
                    operator++();
                    return old_iter;
                }

                iterator& operator--() {
                    --iter;
                    return *this;
                }

                iterator operator--(int) {
                    iterator old_iter{*this};
                    operator--();
                    return old_iter;
                }

                decltype(auto) operator*() {
                    return *iter;
                }

                template <class Iter2>
                bool operator!=(const iterator<Iter2>& other) {
                    return iter != other.iter;
                }

                template <class Iter2>
                bool operator==(const iterator<Iter2>& other) {
                    return iter == other.iter;
                }

            private:
                template <class EndIter>
                void advance_first_time(EndIter end_iter) {
                    while (iter != end_iter && fn(*iter)) {
                        ++iter;
                    }
                }

                Iter iter;
                Fn fn;
            };

            dropwhile_struct(Range&& range, Fn fn)
                : base(std::forward<Range>(range)), fn{std::move(fn)} {
            }

            decltype(auto) begin() {
                return iterator{base::begin(), base::end(), fn};
            }

            decltype(auto) begin() const {
                return iterator{base::begin(), base::end(), fn};
            }

            decltype(auto) end() {
                return iterator{base::end(), fn};
            }

            decltype(auto) end() const {
                return iterator{base::end(), fn};
            }

            decltype(auto) rbegin() {
                return iterator{base::rbegin(), base::rend(), fn};
            }

            decltype(auto) rbegin() const {
                return iterator{base::rbegin(), base::rend(), fn};
            }

            decltype(auto) rend() {
                return iterator{base::rend(), fn};
            }

            decltype(auto) rend() const {
                return iterator{base::rend(), fn};
            }

        private:
            Fn fn;
        };

        template <class Fn>
        class dropwhile_with_pipe {
        public:
            dropwhile_with_pipe(Fn fn) : fn{std::move(fn)} {
            }

            template <class Range, class Pred>
            friend decltype(auto) operator|(Range&& range,
                                            dropwhile_with_pipe<Pred>&& pred);

        private:
            Fn fn;
        };

    } // namespace views::detail

    template <class Range, class Fn>
    decltype(auto) dropwhile(Range&& range, Fn&& fn) {
        return views::detail::dropwhile_struct<Range, Fn> {
            std::forward<Range>(range), std::forward<Fn>(fn);
        };
    }

    template <class T, std::size_t N, class Fn>
    decltype(auto) dropwhile(T (&&arr)[N], Fn&& fn) {
        return views::detail::dropwhile_struct{std::forward<decltype(arr)>(arr),
                                               std::forward<Fn>(fn)};
    }

    template <class Fn>
    decltype(auto) dropwhile(Fn&& fn) {
        return views::detail::dropwhile_with_pipe{std::forward<Fn>(fn)};
    }

    namespace views::detail {
        template <class Range, class Pred>
        decltype(auto) operator|(Range&& range,
                                 dropwhile_with_pipe<Pred>&& pred) {
            return dropwhile(std::forward<Range>(range),
                             std::forward<Pred>(pred.fn));
        }
    } // namespace views::detail

} // namespace RAD_LIB_NAMESPACE
