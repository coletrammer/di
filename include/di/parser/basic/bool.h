#pragma once

#include "di/container/algorithm/equal.h"
#include "di/container/string/string_view.h"
#include "di/math/intcmp/checked.h"
#include "di/math/numeric_limits.h"
#include "di/math/to_unsigned.h"
#include "di/meta/language.h"
#include "di/parser/basic/match_one_or_more.h"
#include "di/parser/combinator/optional.h"
#include "di/parser/combinator/sequence.h"
#include "di/parser/integral_set.h"
#include "di/parser/make_error.h"
#include "di/parser/meta/parser_context_result.h"
#include "di/parser/parser_base.h"
#include "di/types/char.h"
#include "di/util/get.h"
#include "di/vocab/expected/unexpected.h"
#include "di/vocab/tuple/make_tuple.h"

namespace di::parser {
namespace detail {
    struct BooleanFunction {
        constexpr static auto operator()() {
            using namespace di::string_view_literals;

            return match_one_or_more('t'_m || 'r'_m || 'u'_m || 'e'_m || 'f'_m || 'a'_m || 'l'_m || 's'_m)
                       << []<concepts::ParserContext Context>(Context& context,
                                                              auto result) -> meta::ParserContextResult<bool, Context> {
                if (container::equal(result, "true"_sv)) {
                    return true;
                }
                if (container::equal(result, "false"_sv)) {
                    return false;
                }
                return vocab::Unexpected(parser::make_error(context));
            };
        }
    };
}

constexpr inline auto boolean = detail::BooleanFunction {};

namespace detail {
    constexpr auto tag_invoke(types::Tag<create_parser_in_place>, InPlaceType<bool>) {
        return boolean();
    }
}
}
