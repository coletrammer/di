#pragma once

#include "di/container/string/string.h"
#include "di/format/concepts/formattable.h"
#include "di/format/format.h"

namespace di::fmt {
namespace detail {
    struct ToStringFunction {
        constexpr auto operator()(concepts::Formattable auto&& value) const { return format(u8"{}"_sv, value); }
    };
}

constexpr inline auto to_string = detail::ToStringFunction {};
}

namespace di {
using fmt::to_string;
}
