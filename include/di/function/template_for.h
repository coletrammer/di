#pragma once

#include "di/function/invoke.h"
#include "di/meta/algorithm.h"
#include "di/types/prelude.h"

namespace di::function {
template<size_t count, typename F>
constexpr void template_for(F&& function) {
    (void) []<size_t... indices>(meta::ListV<indices...>, F & function) {
        (void) (function::invoke(function, c_<indices>), ...);
    }
    (meta::MakeIndexSequence<count> {}, function);
}
}
