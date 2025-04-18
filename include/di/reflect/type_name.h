#pragma once

#include "di/container/string/fixed_string.h"
#include "di/meta/core.h"
#include "di/reflect/enumerator.h"
#include "di/reflect/field.h"
#include "di/reflect/reflect.h"
#include "di/vocab/pointer/box.h"

namespace di::reflection {
namespace detail {
    template<typename T, typename M>
    struct TypeName {};

    template<typename T, typename M>
    requires(requires { M::name; })
    struct TypeName<T, M> {
        constexpr static auto name = M::name;
    };

    template<typename T, typename A, typename M>
    requires(requires { TypeName<T, meta::Reflect<T>>::name; })
    struct TypeName<di::Box<T, A>, M> {
        constexpr static auto subname = TypeName<T, meta::Reflect<T>>::name;
        constexpr static auto name = container::fixed_string_concat(FixedString("Box<"), subname, FixedString(">"));
    };
}

template<concepts::Reflectable T, typename M = Reflect<T>>
requires(requires { detail::TypeName<meta::RemoveCVRef<T>, M>::name; })
constexpr inline auto type_name = detail::TypeName<meta::RemoveCVRef<T>, M>::name;
}

namespace di {
using reflection::type_name;
}
