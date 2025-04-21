#pragma once

#include "di/container/concepts/input_container.h"
#include "di/container/string/constant_string.h"
#include "di/container/string/string_view.h"
#include "di/format/concepts/formattable.h"
#include "di/format/formatter.h"
#include "di/format/make_format_args.h"
#include "di/format/vpresent_encoded_context.h"
#include "di/meta/util.h"
#include "di/vocab/variant/prelude.h"

namespace di::format {
template<concepts::Formattable... Types, concepts::Encoding Enc>
constexpr auto tag_invoke(types::Tag<formatter_in_place>, InPlaceType<Variant<Types...>>, FormatParseContext<Enc>&) {
    auto do_output = [](concepts::FormatContext auto& context,
                        concepts::DecaySameAs<Variant<Types...>> auto&& variant) -> Result<void> {
        return di::visit(
            [&](auto&& value) -> Result<void> {
                return vpresent_encoded_context<meta::Encoding<decltype(context)>>(
                    u8"{}"_sv, format::make_format_args<decltype(context)>(value), context, true);
            },
            variant);
    };
    return Result<decltype(do_output)>(util::move(do_output));
}

template<concepts::Formattable T, concepts::Formattable E, concepts::Encoding Enc>
constexpr auto tag_invoke(types::Tag<formatter_in_place>, InPlaceType<Expected<T, E>>, FormatParseContext<Enc>&) {
    auto do_output = [](concepts::FormatContext auto& context,
                        concepts::DecaySameAs<Expected<T, E>> auto&& expected) -> Result<void> {
        if (expected.has_value()) {
            return vpresent_encoded_context<meta::Encoding<decltype(context)>>(
                u8"{}"_sv, format::make_format_args<decltype(context)>(expected.value()), context, true);
        }

        (void) context.output(U'U');
        (void) context.output(U'n');
        (void) context.output(U'e');
        (void) context.output(U'x');
        (void) context.output(U'p');
        (void) context.output(U'e');
        (void) context.output(U'c');
        (void) context.output(U't');
        (void) context.output(U'e');
        (void) context.output(U'd');
        (void) context.output(U'(');
        auto result = vpresent_encoded_context<meta::Encoding<decltype(context)>>(
            u8"{}"_sv, format::make_format_args<decltype(context)>(expected.error()), context, true);
        (void) context.output(U')');
        return result;
    };
    return Result<decltype(do_output)>(util::move(do_output));
}
}
