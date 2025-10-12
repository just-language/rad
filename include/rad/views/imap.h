#pragma once
#include <rad/views/views_base.h>

namespace RAD_LIB_NAMESPACE {
    namespace views::detail {
        template <class Range, class Fn>
        class imap_struct : private views_base::range_base<Range> {
        public:
            using base = views_base::range_base<Range>;

            template <class Iter, class IterFn>
            class iterator {
                template <typename, typename>
                friend class iterator;

            public:
                iterator(Iter iter, IterFn fn)
                    : iter{std::move(iter)}, fn{std::move(fn)} {
                }

                iterator& operator++() {
                    ++iter;
                    return *this;
                }

                iterator operator++(int) {
                    iterator old_iter{*this};
                    iter++;
                    return old_iter;
                }

                iterator& operator--() {
                    --iter;
                    return *this;
                }

                iterator operator--(int) {
                    iterator old_iter{*this};
                    iter--;
                    return old_iter;
                }

                decltype(auto) operator*() {
                    return fn(*iter);
                }

                template <class Iter2, class Fn2>
                bool operator==(const iterator<Iter2, Fn2>& it) {
                    return iter == it.iter;
                }

                template <class Iter2, class Fn2>
                bool operator!=(const iterator<Iter2, Fn2>& it) {
                    return iter != it.iter;
                }

                template <class Iter2, class Fn2>
                auto operator-(const iterator<Iter2, Fn2>& it) {
                    return iter - it.iter;
                }

            private:
                Iter iter;
                IterFn fn;
            };

            imap_struct(Range&& range, Fn&& fn)
                : base(std::forward<Range>(range)), fn(std::forward<Fn>(fn)) {
            }

            decltype(auto) begin() {
                return iterator<decltype(base::begin()), Fn>{base::begin(), fn};
            }

            decltype(auto) end() {
                return iterator<decltype(base::end()), Fn>{base::end(), fn};
            }

            decltype(auto) rbegin() {
                return iterator<decltype(base::rbegin()), Fn>{base::rbegin(),
                                                              fn};
            }

            decltype(auto) rend() {
                return iterator<decltype(base::rend()), Fn>{base::rend(), fn};
            }

        private:
            Fn fn;
        };

        template <class Fn>
        class imap_with_pipe {
        public:
            imap_with_pipe(Fn&& fn) : fn{std::forward<Fn>(fn)} {
            }

            template <class Range, class Fn>
            friend decltype(auto) operator|(Range&& range,
                                            imap_with_pipe<Fn> fn_holder);

            template <class T, std::size_t N, class Fn>
            friend decltype(auto) operator|(T (&&arr)[N],
                                            imap_with_pipe<Fn> fn_holder);

        private:
            Fn fn;
        };

    } // namespace views::detail

    template <class Range, class Fn>
    decltype(auto) imap(Range&& range, Fn&& fn) {
        return views::detail::imap_struct<Range, Fn>{std::forward<Range>(range),
                                                     std::forward<Fn>(fn)};
    }

    template <class T, std::size_t N, class Fn>
    decltype(auto) imap(T (&&arr)[N], Fn&& fn) {
        return views::detail::imap_struct{std::forward<decltype(arr)>(arr),
                                          std::forward<Fn>(fn)};
    }

    template <class Fn>
    decltype(auto) imap(Fn&& fn) {
        return views::detail::imap_with_pipe{std::forward<Fn>(fn)};
    }

    namespace views::detail {
        template <class Range, class Fn>
        decltype(auto) operator|(Range&& range, imap_with_pipe<Fn> fn_holder) {
            return imap(std::forward<Range>(range),
                        std::forward<Fn>(fn_holder.fn));
        }

        template <class T, std::size_t N, class Fn>
        decltype(auto) operator|(T (&&arr)[N], imap_with_pipe<Fn> fn_holder) {
            return imap(std::forward<decltype(arr)>(arr),
                        std::forward<Fn>(fn_holder.fn));
        }
    } // namespace views::detail

} // namespace RAD_LIB_NAMESPACE
