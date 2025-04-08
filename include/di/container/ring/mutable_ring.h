#pragma once

#include "di/container/ring/constant_ring.h"
#include "di/container/vector/mutable_vector.h"

namespace di::concepts::detail {
template<typename T>
concept MutableRing = MutableVector<T> && ConstantRing<T> && requires(T& lvalue, usize n) {
    { lvalue.assume_head(n) } -> LanguageVoid;
    { lvalue.assume_tail(n) } -> LanguageVoid;
};
}

namespace di::meta::detail {
template<concepts::detail::MutableVector T, typename Value = void>
using RingAllocResult =
    meta::LikeExpected<decltype(util::declval<T&>().reserve_from_nothing(util::declval<size_t>())), Value>;
}
