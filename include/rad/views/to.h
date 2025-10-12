#pragma once
#include <rad/views/views_base.h>

namespace RAD_LIB_NAMESPACE {

    template <class NewRange, class Range>
    NewRange to(Range&& range) {
        return NewRange{std::begin(std::forward<Range>(range)),
                        std::end(std::forward<Range>(range))};
    }

    namespace views::detail {
        template <class NewRange>
        struct to_struct {
            template <class Range>
            friend NewRange operator|(Range&& range, to_struct) {
                return to<NewRange>(std::forward<Range>(range));
            }
        };
    } // namespace views::detail

    template <class NewRange>
    auto to() -> views::detail::to_struct<NewRange> {
        return views::detail::to_struct<NewRange>{};
    }

} // namespace RAD_LIB_NAMESPACE
