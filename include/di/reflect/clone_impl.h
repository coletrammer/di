#pragma once

#include "di/reflect/reflect.h"
#include "di/util/clone.h"

namespace di::util::detail {
template<concepts::ReflectableToFields T>
constexpr auto tag_invoke(types::Tag<clone>, T const& object) -> T {
    auto result = T {};
    vocab::tuple_for_each(
        [&](auto field) {
            field.get(result) = clone(field.get(object));
        },
        reflect(object));
    return result;
}
}
