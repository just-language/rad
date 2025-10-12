#pragma once
#include <rad/views/views_base.h>

namespace RAD_LIB_NAMESPACE {
    namespace views::detail {
        template <class Range>
        class take_struct : private views_base::range_base<Range> {
            using base = views_base::range_base<Range>;

        public:
            struct end_mark {};

            template <class Iter, class EndIter>
            class iterator {
            public:
                iterator(Iter iter, EndIter end_iter, std::size_t count)
                    : iter{std::move(iter)}, end_iter{std::move(end_iter)},
                      count{count} {
                }

                iterator& operator++() {
                    if (!--count) {
                        iter = end_iter;
                        return *this;
                    }
                    ++iter;
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

                bool operator!=(end_mark) const {
                    return iter != end_iter;
                }

                bool operator==(end_mark) const {
                    return iter == end_iter;
                }

            private:
                Iter iter;
                EndIter end_iter;
                std::size_t count;
            };

            take_struct(Range&& range, std::size_t count)
                : base(std::forward<Range>(range)), count{count} {
            }

            decltype(auto) begin() {
                return iterator{base::begin(), base::end(), count};
            }

            decltype(auto) begin() const {
                return iterator{base::begin(), base::end(), count};
            }

            end_mark end() const noexcept {
                return end_mark{};
            }

        private:
            std::size_t count;
        };

        class take_with_pipe {
        public:
            take_with_pipe(std::size_t count) : count{count} {
            }

            template <class Range>
            friend decltype(auto) operator|(Range&& range,
                                            take_with_pipe to_take);

        private:
            std::size_t count;
        };

    } // namespace views::detail

    template <class Range>
    decltype(auto) take(Range&& range, std::size_t count) {
        return views::detail::take_struct<Range>{std::forward<Range>(range),
                                                 count};
    }

    template <class T, std::size_t N>
    decltype(auto) take(T (&&arr)[N], std::size_t count) {
        return views::detail::take_struct{std::forward<decltype(arr)>(arr),
                                          count};
    }

    decltype(auto) take(std::size_t count) {
        return views::detail::take_with_pipe{count};
    }

    namespace views::detail {
        template <class Range>
        decltype(auto) operator|(Range&& range, take_with_pipe to_take) {
            return take(std::forward<Range>(range), to_take.count);
        }
    } // namespace views::detail

} // namespace RAD_LIB_NAMESPACE
