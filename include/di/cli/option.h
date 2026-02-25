#pragma once

#include "di/cli/value_type.h"
#include "di/cli/zsh.h"
#include "di/container/prelude.h"
#include "di/container/string/fixed_string_to_utf8_string_view.h"
#include "di/container/string/prelude.h"
#include "di/container/string/string_view.h"
#include "di/container/view/transform.h"
#include "di/format/prelude.h"
#include "di/function/prelude.h"
#include "di/meta/language.h"
#include "di/parser/prelude.h"
#include "di/reflect/reflect.h"
#include "di/vocab/optional/optional_forward_declaration.h"
#include "di/vocab/prelude.h"
#include "di/vocab/tuple/tuple_for_each.h"

namespace di::cli::detail {
class Option {
private:
    using Parse = Result<> (*)(void*, Optional<TransparentStringView>);
    using GetValues = Vector<Tuple<String, StringView>> (*)();
    using DefaultValue = String (*)();

    template<auto member>
    constexpr static auto concrete_parse(void* output_untyped, Optional<TransparentStringView> input) -> Result<> {
        using Base = meta::MemberPointerClass<decltype(member)>;
        using Value = meta::MemberPointerValue<decltype(member)>;

        auto output = static_cast<Base*>(output_untyped);
        if constexpr (concepts::SameAs<Value, bool>) {
            if (!input) {
                (*output).*member = true;
                return {};
            }
            (*output).*member = DI_TRY(parser::parse<Value>(*input));
            return {};
        } else {
            DI_ASSERT(input);
            if constexpr (concepts::Optional<Value>) {
                (*output).*member = DI_TRY(parser::parse<meta::OptionalValue<Value>>(*input));
            } else {
                (*output).*member = DI_TRY(parser::parse<Value>(*input));
            }
            return {};
        }
    }

public:
    Option() = default;

    template<auto member>
    constexpr explicit Option(Constexpr<member>, Optional<char> short_name = {},
                              Optional<TransparentStringView> long_name = {}, StringView description = {},
                              bool required = false, Optional<StringView> value_name = {},
                              Optional<ValueType> value_type = {}, bool is_help = false)
        : m_parse(concrete_parse<member>)
        , m_get_values(detail::concrete_get_values<meta::MemberPointerValue<decltype(member)>>)
        , m_default_value(detail::concrete_default_value<member>)
        , m_short_name(short_name)
        , m_long_name(long_name)
        , m_description(description)
        , m_value_name(value_name)
        , m_value_type(value_type.value_or(default_value_type<meta::MemberPointerValue<decltype(member)>>))
        , m_required(required)
        , m_is_help(is_help)
        , m_boolean(di::SameAs<meta::MemberPointerValue<decltype(member)>, bool>) {}

    constexpr auto parse(void* base, Optional<TransparentStringView> input) const {
        DI_ASSERT(m_parse);
        return m_parse(base, input);
    }
    constexpr auto short_name() const { return m_short_name; }
    constexpr auto long_name() const { return m_long_name; }
    constexpr auto description() const { return m_description; }
    constexpr auto required() const { return m_required; }
    constexpr auto boolean() const { return m_boolean; }
    constexpr auto is_help() const { return m_is_help; }

    constexpr auto long_display_name() const {
        auto result = di::TransparentString {};
        result += di::single('-');
        result += di::single('-');
        result += m_long_name.value();
        return result;
    }

    constexpr auto short_display_name() const {
        auto result = di::TransparentString {};
        result += di::single('-');
        result += di::single(m_short_name.value());
        return result;
    }

    constexpr auto display_name() const {
        if (long_name()) {
            return long_display_name();
        }
        if (short_name()) {
            return short_display_name();
        }
        return di::TransparentString {};
    }

    constexpr auto value_name() const -> String {
        if (m_value_name) {
            return m_value_name.value().to_owned();
        }
        if (!m_long_name) {
            return "VALUE"_s;
        }
        auto result = String {};
        for (auto c : m_long_name.value()) {
            if (c == '-') {
                result.push_back(U'_');
            } else if (c >= 'a' && c <= 'z') {
                result.push_back(c & ~0x20);
            } else {
                result.push_back(c);
            }
        }
        return result;
    }

    constexpr auto default_value() const -> String { return m_default_value(); }
    constexpr auto value_type() const -> ValueType { return m_value_type; }
    constexpr auto values() const -> Vector<Tuple<String, StringView>> { return m_get_values(); }

    constexpr auto zsh_completion_specs() const -> Vector<String> {
        auto prefixes = Vector<TransparentString> {};
        if (short_name()) {
            prefixes.push_back(short_display_name());
        }
        if (long_name()) {
            prefixes.push_back(long_display_name());
        }

        return prefixes | transform([&](TransparentString const& prefix) {
                   auto value_char = boolean() ? ""_sv : prefix.starts_with("--"_tsv) ? "="_sv : "+"_sv;
                   auto value_spec = ""_s;
                   if (!boolean()) {
                       auto values = this->values();
                       value_spec =
                           format(":{}:{}"_sv, value_name(), zsh::value_completions(m_value_type, values.span()));
                   }
                   return format("{}{}[{}]{}"_sv, prefix, value_char, zsh::escape_description(description()),
                                 value_spec);
               }) |
               di::to<Vector>();
    }

private:
    Parse m_parse { nullptr };
    GetValues m_get_values { nullptr };
    DefaultValue m_default_value { nullptr };
    Optional<char> m_short_name;
    Optional<TransparentStringView> m_long_name;
    StringView m_description;
    Optional<StringView> m_value_name;
    ValueType m_value_type { ValueType::Unknown };
    bool m_required { false };
    bool m_is_help { false };
    bool m_boolean { false };
};
}
