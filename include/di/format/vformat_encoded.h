#pragma once

#include "di/container/allocator/allocator.h"
#include "di/container/string/string_impl.h"
#include "di/container/string/string_view_impl.h"
#include "di/format/vformat_encoded_context.h"
#include "di/function/monad/monad_try.h"
#include "di/meta/core.h"
#include "di/meta/vocab.h"
#include "di/util/move.h"

namespace di::fmt {
namespace detail {
    template<concepts::Encoding Enc>
    struct VFormatEncodedFunction {
        using View = container::string::StringViewImpl<Enc>;
        using Str = container::string::StringImpl<Enc>;

        using R = meta::Conditional<concepts::FallibleAllocator<DefaultAllocator>, Result<Str>, Str>;

        template<concepts::FormatArg Arg>
        constexpr auto operator()(View format, FormatArgs<Arg> args) const -> R {
            auto context = FormatContext<Enc> {};
            if constexpr (concepts::FallibleAllocator<DefaultAllocator>) {
                DI_TRY(vformat_encoded_context<Enc>(format, util::move(args), context));
            } else {
                DI_ASSERT(vformat_encoded_context<Enc>(format, util::move(args), context));
            }
            return util::move(context).output();
        }
    };
}

template<concepts::Encoding Enc>
constexpr inline auto vformat_encoded = detail::VFormatEncodedFunction<Enc> {};
}
