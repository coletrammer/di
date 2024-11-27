#pragma once

#include <di/container/algorithm/in_out_result.h>
#include <di/container/concepts/prelude.h>
#include <di/container/iterator/prelude.h>
#include <di/container/meta/prelude.h>
#include <di/util/addressof.h>
#include <di/util/construct_at.h>

namespace di::container {
namespace detail {
    struct UninitializedCopyNFunction {
        template<concepts::InputIterator It, concepts::UninitForwardIterator Out,
                 concepts::UninitSentinelFor<Out> OutSent>
        requires(concepts::ConstructibleFrom<meta::IteratorValue<Out>, meta::IteratorReference<It>>)
        constexpr auto operator()(It in, meta::IteratorSSizeType<It> n, Out out, OutSent out_last) const
            -> InOutResult<It, Out> {
            for (; n > 0 && out != out_last; --n, ++in, ++out) {
                util::construct_at(util::addressof(*out), *in);
            }
            return { util::move(in), util::move(out) };
        }
    };
}

constexpr inline auto uninitialized_copy_n = detail::UninitializedCopyNFunction {};
}

namespace di {
using container::uninitialized_copy_n;
}
