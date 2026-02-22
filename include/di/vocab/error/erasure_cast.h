#pragma once

#include "di/container/algorithm/copy.h"
#include "di/meta/language.h"
#include "di/meta/trivial.h"
#include "di/types/byte.h"
#include "di/util/addressof.h"
#include "di/util/bit_cast.h"
#include "di/util/compile_time_fail.h"
#include "di/util/unsafe_forget.h"

namespace di::vocab::detail {
template<concepts::TriviallyRelocatable To, concepts::TriviallyRelocatable From>
requires(sizeof(To) == sizeof(From) && concepts::Trivial<From>)
constexpr auto erasure_cast(From const& from) -> To {
    return bit_cast<To>(from);
}

template<concepts::TriviallyRelocatable To, concepts::TriviallyRelocatable From>
requires(sizeof(To) == sizeof(From) && concepts::RValueReference<From &&>)
constexpr auto erasure_cast(From&& from) -> To {
    if constexpr (requires { bit_cast<To>(from); }) {
        return bit_cast<To>(from);
    } else {
        // This seems safe...
        di::byte bytes[sizeof(From)];
        di::copy((byte const*) di::addressof(from), (byte const*) di::addressof(from) + sizeof(From), bytes);
        unsafe_forget(di::forward<From>(from));
        return di::bit_cast<To>(bytes);
    }
}
}
