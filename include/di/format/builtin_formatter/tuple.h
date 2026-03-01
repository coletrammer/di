#pragma once

#include "di/container/concepts/input_container.h"
#include "di/container/string/constant_string.h"
#include "di/container/string/string_view.h"
#include "di/format/concepts/formattable.h"
#include "di/format/formatter.h"
#include "di/format/make_format_args.h"
#include "di/format/vformat_encoded_context.h"
#include "di/meta/util.h"
#include "di/vocab/tuple/prelude.h"

namespace di::fmt {
template<concepts::Formattable... Types, concepts::Encoding Enc>
constexpr auto tag_invoke(types::Tag<formatter_in_place>, InPlaceType<Tuple<Types...>>, FormatParseContext<Enc>&) {
    auto do_output = [](concepts::FormatContext auto& context,
                        concepts::DecaySameAs<Tuple<Types...>> auto&& tuple) -> Result<void> {
        context.output('(');
        context.output(' ');
        bool first = true;
        tuple_for_each(
            [&](auto&& value) {
                if (!first) {
                    context.output(',');
                    context.output(' ');
                }
                first = false;
                (void) vformat_encoded_context<meta::Encoding<decltype(context)>>(
                    u8"{}"_sv, fmt::make_format_args<decltype(context)>(value), context, true);
            },
            util::forward<decltype(tuple)>(tuple));

        context.output(' ');
        context.output(')');
        return {};
    };
    return Result<decltype(do_output)>(util::move(do_output));
}
}
