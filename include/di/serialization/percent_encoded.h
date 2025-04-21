#pragma once

#include "di/container/string/string_view.h"
#include "di/format/prelude.h"
#include "di/parser/basic/match_zero_or_more.h"
#include "di/parser/prelude.h"
#include "di/vocab/error/result.h"

namespace di::serialization {
/// @brief Helper class for performing percent encoding
///
/// @tparam String The underlying string used to hold the raw data
///
/// The scheme is defined in this [RFC](https://datatracker.ietf.org/doc/html/rfc3986#section-2.1)
template<typename String = di::TransparentString>
requires(concepts::SameAs<meta::Encoding<String>, container::string::TransparentEncoding>)
class PercentEncoded {
public:
    using IsAtom = void;

    PercentEncoded() = default;

    constexpr static auto from_raw_data(String string) -> PercentEncoded { return PercentEncoded(di::move(string)); }

    constexpr auto underlying_string() const& -> String const& { return m_string; }
    constexpr auto underlying_string() && -> String&& { return di::move(m_string); }

    auto operator==(PercentEncoded const& other) const -> bool = default;

private:
    constexpr explicit PercentEncoded(String string) : m_string(di::move(string)) {}

    constexpr static auto hex_digit(u8 value) -> c32 {
        if (value >= 10) {
            return c32('A' + (value - 10));
        }
        return c32('0' + value);
    }

    constexpr static auto hex_value(char value) -> di::Optional<u8> {
        if (('0'_mc - '9'_mc)(value)) {
            return value - '0';
        }
        if (('A'_mc - 'F'_mc)(value)) {
            return u8(10u + (value - 'A'));
        }
        if (('a'_mc - 'f'_mc)(value)) {
            return u8(10u + (value - 'a'));
        }
        return {};
    }

    template<concepts::Encoding Enc>
    constexpr friend auto tag_invoke(types::Tag<format::formatter_in_place>, InPlaceType<PercentEncoded>,
                                     FormatParseContext<Enc>&) {
        auto do_output = [](concepts::FormatContext auto& context,
                            concepts::DecaySameAs<PercentEncoded> auto&& percent_encoded) -> Result<> {
            constexpr auto allowed =
                ('0'_mc - '9'_mc || 'a'_mc - 'z'_mc || 'A'_mc - 'Z'_mc || '-'_mc || '.'_mc || '_'_mc || '~'_mc);

            for (auto ch : percent_encoded.m_string) {
                if (allowed(ch)) {
                    (void) context.output(ch);
                } else {
                    (void) context.output(U'%');
                    (void) context.output(hex_digit(u8(ch) >> 4));
                    (void) context.output(hex_digit(u8(ch) & 0xFu));
                }
            }
            return {};
        };
        return Result<decltype(do_output)>(di::move(do_output));
    }

    constexpr friend auto tag_invoke(types::Tag<parser::create_parser_in_place>, InPlaceType<PercentEncoded>) {
        // Only accept characters which are valid ASCII.
        constexpr auto valid_base64 = ('\0'_m - '\x7f'_m);

        return (parser::match_zero_or_more(valid_base64)) <<
                   []<typename Context>(
                       Context& context,
                       concepts::CopyConstructible auto results) -> meta::ParserContextResult<PercentEncoded, Context> {
            auto result = PercentEncoded {};
            auto expected_chars = 0;
            auto accumulator = u8(0);
            for (auto ch : results) {
                if (expected_chars > 0) {
                    accumulator <<= 4;
                    auto value = hex_value(ch);
                    if (!value) {
                        return Unexpected(context.make_error());
                    }
                    accumulator |= value.value();

                    if (--expected_chars == 0) {
                        result.m_string.push_back(char(accumulator));
                    }
                    continue;
                }
                if (ch == '%') {
                    expected_chars = 2;
                    accumulator = 0;
                    continue;
                }
                result.m_string.push_back(ch);
            }
            if (expected_chars > 0) {
                return Unexpected(context.make_error());
            }
            return result;
        };
    }

    String m_string;
};
}

namespace di {
inline namespace literals {
    inline namespace percent_encoded_literals {
        constexpr auto operator""_percent_encoded(char const* data, usize size) -> serialization::PercentEncoded<> {
            auto view = di::TransparentStringView { data, size };
            return parser::parse_unchecked<serialization::PercentEncoded<>>(view);
        }
    }
}
}

namespace di {
using serialization::PercentEncoded;

using PercentEncodedView = PercentEncoded<di::TransparentStringView>;
}

#if !defined(DI_NO_GLOBALS) && !defined(DI_NO_GLOBAL_PERCENT_ENCODED_LITERALS)
using namespace di::literals::percent_encoded_literals;
#endif
