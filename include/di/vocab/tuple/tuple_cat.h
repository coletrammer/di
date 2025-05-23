#pragma once

#include "di/function/overload.h"
#include "di/function/unpack.h"
#include "di/function/ycombinator.h"
#include "di/vocab/tuple/apply.h"
#include "di/vocab/tuple/tuple.h"

namespace di::vocab {
template<concepts::TupleLike... Tups>
constexpr auto tuple_cat(Tups&&... tuples) {
    auto helper = function::overload(
        [] {
            return Tuple<> {};
        },
        [](auto&& x) {
            return x;
        },
        []<typename X, typename Y>(X&& x, Y&& y) {
            return function::unpack<meta::MakeIndexSequence<meta::TupleSize<X>>>([&]<size_t... xs>(meta::ListV<xs...>) {
                return function::unpack<meta::MakeIndexSequence<meta::TupleSize<Y>>>(
                    [&]<size_t... ys>(meta::ListV<ys...>) {
                        return Tuple<meta::TupleElement<meta::RemoveCVRef<X>, xs>...,
                                     meta::TupleElement<meta::RemoveCVRef<Y>, ys>...> {
                            // NOLINTNEXTLINE(bugprone-use-after-move)
                            util::get<xs>(util::forward<X>(x))..., util::get<ys>(util::forward<Y>(y))...
                        };
                    });
            });
        },
        []<typename X, typename Y, typename... Rs>(X&& x, Y&& y, Rs&&... rest) {
            return tuple_cat(tuple_cat(util::forward<X>(x), util::forward<Y>(y)), util::forward<Rs>(rest)...);
        });
    return helper(util::forward<Tups>(tuples)...);
}
}

namespace di {
using vocab::tuple_cat;
}
