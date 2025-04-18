#pragma once

#include "di/container/view/filter_view.h"
#include "di/function/curry_back.h"
#include "di/function/tag_invoke.h"
#include "di/util/forward.h"

namespace di::container::view {
namespace detail {
    struct FilterFunction;

    template<typename Con, typename Pred>
    concept CustomFilter = concepts::TagInvocable<FilterFunction, Con, Pred>;

    template<typename Con, typename Pred>
    concept ViewFilter = requires(Con&& container, Pred&& predicate) {
        FilterView { util::forward<Con>(container), util::forward<Pred>(predicate) };
    };

    struct FilterFunction {
        template<concepts::ViewableContainer Con, typename Pred>
        requires(CustomFilter<Con, Pred> || ViewFilter<Con, Pred>)
        constexpr auto operator()(Con&& container, Pred&& predicate) const -> concepts::View auto {
            if constexpr (CustomFilter<Con, Pred>) {
                return function::tag_invoke(*this, util::forward<Con>(container), util::forward<Pred>(predicate));
            } else {
                return FilterView { util::forward<Con>(container), util::forward<Pred>(predicate) };
            }
        }
    };
}

constexpr inline auto filter = function::curry_back(detail::FilterFunction {}, meta::c_<2ZU>);
}

namespace di {
using view::filter;
}
