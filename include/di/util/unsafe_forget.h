#pragma once

#include "di/meta/operations.h"
#include "di/util/construct_at.h"
#include "di/util/move.h"

namespace di::util::detail {
template<typename T>
struct Union {
    Union() {}
    ~Union() {}

    union {
        T value;
    };
};

struct UnsafeForget {
    template<concepts::MoveConstructible T>
    static void operator()(T value) {
        auto slot = Union<T> {};
        di::construct_at(&slot.value, di::move(value));
    }
};
}

namespace di {
/// @brief Steal ownership of an object and do not its destructor
///
/// You should literally never use this function. Code which requires it
/// is not doing type erasure correctly.
constexpr inline auto unsafe_forget = util::detail::UnsafeForget {};
}
