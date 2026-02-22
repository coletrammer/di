#pragma once

#include "di/container/string/encoding.h"
#include "di/container/string/string_impl.h"
#include "di/format/concepts/formattable.h"
#include "di/format/format_string_impl.h"
#include "di/format/make_format_args.h"
#include "di/format/vformat_encoded.h"

namespace di::fmt {
namespace detail {
    template<concepts::Encoding Enc>
    struct FormatEncodedFunction {
        template<concepts::Formattable... Args>
        constexpr auto operator()(fmt::FormatStringImpl<Enc, Args...> format, Args&&... args) const {
            return vformat_encoded<Enc>(format, fmt::make_format_args<FormatContext<Enc>>(args...));
        }
    };
}

template<concepts::Encoding Enc>
constexpr inline auto format_encoded = detail::FormatEncodedFunction<Enc> {};
}
