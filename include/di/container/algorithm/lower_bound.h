#pragma once

#include "di/container/concepts/prelude.h"
#include "di/container/iterator/prelude.h"
#include "di/container/meta/prelude.h"

namespace di::container {
namespace detail {
    struct LowerBoundFunction {
        template<concepts::ForwardIterator It, concepts::SentinelFor<It> Sent, typename T,
                 typename Proj = function::Identity,
                 concepts::IndirectStrictWeakOrder<T const*, meta::Projected<It, Proj>> Comp = function::Compare>
        constexpr auto operator()(It first, Sent last, T const& needle, Comp comp = {}, Proj proj = {}) const -> It {
            auto const distance = container::distance(first, last);
            return lower_bound_with_size(util::move(first), needle, util::ref(comp), util::ref(proj), distance);
        }

        template<concepts::ForwardContainer Con, typename T, typename Proj = function::Identity,
                 concepts::IndirectStrictWeakOrder<T const*, meta::Projected<meta::ContainerIterator<Con>, Proj>> Comp =
                     function::Compare>
        constexpr auto operator()(Con&& container, T const& needle, Comp comp = {}, Proj proj = {}) const
            -> meta::BorrowedIterator<Con> {
            auto const distance = container::distance(container);
            return lower_bound_with_size(container::begin(container), needle, util::ref(comp), util::ref(proj),
                                         distance);
        }

    private:
        friend struct EqualRangeFunction;

        template<typename It, typename T, typename Proj, typename Comp,
                 typename SSizeType = meta::IteratorSSizeType<It>>
        constexpr static auto lower_bound_with_size(It first, T const& needle, Comp comp, Proj proj,
                                                    meta::TypeIdentity<SSizeType> n) -> It {
            while (n != 0) {
                SSizeType left_length = n >> 1;
                auto* mid = container::next(first, left_length);
                if (function::invoke(comp, needle, function::invoke(proj, *mid)) <= 0) {
                    // needle is less than or equal to every element in the range [mid, last).
                    n = left_length;
                } else {
                    // needle is greater than every element in the range [first, mid].
                    n -= left_length + 1;
                    first = ++mid;
                }
            }
            return first;
        }
    };
}

constexpr inline auto lower_bound = detail::LowerBoundFunction {};
}

namespace di {
using container::lower_bound;
}
