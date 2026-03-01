#pragma once

#include "di/platform/compiler.h"
#include "di/util/forward.h"
#include "di/util/std_new.h"

#ifndef DI_NO_USE_STD
#include <memory>
#else
namespace std {
#ifdef DI_GCC
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdangling-pointer"
#endif
template<typename T, typename... Args>
constexpr auto construct_at(T* location, Args&&... args) -> T* {
    return ::new (const_cast<void*>(static_cast<void const volatile*>(location))) T(di::util::forward<Args>(args)...);
}
}
#ifdef DI_GCC
#pragma GCC diagnostic pop
#endif
#endif

namespace di::util {
namespace detail {
    struct ConstructAtFunction {
        template<typename T, typename... Args>
        constexpr auto operator()(T* location, Args&&... args) const -> T* requires(requires {
            std::construct_at(location, util::forward<Args>(args)...);
        }) { return std::construct_at(location, util::forward<Args>(args)...); }
    };
}

constexpr inline auto construct_at = detail::ConstructAtFunction {};
}

namespace di {
using util::construct_at;
}
