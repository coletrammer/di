#pragma once

#include <di/concepts/same_as.h>
#include <di/meta/bool_constant.h>
#include <di/meta/constexpr.h>

namespace di::concepts {
template<typename T>
concept Clock = requires {
    typename T::Representation;
    typename T::Period;
    typename T::Duration;
    typename T::TimePoint;
    T::is_steady;
    typename Constexpr<T::is_steady>;
    { T::now() } -> SameAs<typename T::TimePoint>;
};
}
