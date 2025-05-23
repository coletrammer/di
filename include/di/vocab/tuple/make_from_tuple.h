#pragma once

#include "di/function/pipeline.h"
#include "di/vocab/tuple/apply.h"
#include "di/vocab/tuple/tuple_like.h"

namespace di::vocab {
namespace detail {
    template<typename T>
    struct MakeFromTupleFunction {
        template<concepts::TupleLike Tup>
        constexpr auto operator()(Tup&& tuple) const -> T
        requires(requires {
            apply(
                []<typename... Args>(Args&&... args) {
                    return T(util::forward<Args>(args)...);
                },
                util::forward<Tup>(tuple));
        })
        {
            return apply(
                []<typename... Args>(Args&&... args) {
                    return T(util::forward<Args>(args)...);
                },
                util::forward<Tup>(tuple));
        }
    };
}

template<typename T>
constexpr inline auto make_from_tuple = detail::MakeFromTupleFunction<T> {};
}

namespace di {
using vocab::make_from_tuple;
}
