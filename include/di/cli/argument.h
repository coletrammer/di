#pragma once

#include "di/cli/value_type.h"
#include "di/cli/zsh.h"
#include "di/container/string/encoding.h"
#include "di/container/string/prelude.h"
#include "di/container/string/string_view.h"
#include "di/container/vector/mutable_vector.h"
#include "di/container/vector/prelude.h"
#include "di/parser/prelude.h"
#include "di/vocab/span/prelude.h"

namespace di::cli::detail {
class Argument {
private:
    using Parse = Result<> (*)(void*, Span<TransparentStringView>);
    using GetValues = Vector<Tuple<String, StringView>> (*)();
    using DefaultValue = String (*)();

    template<auto member>
    constexpr static auto concrete_variadic() -> bool {
        using Value = meta::MemberPointerValue<decltype(member)>;

        // Match any form of vector, but not a string-like type.
        return concepts::detail::MutableVector<Value> && !concepts::HasEncoding<Value>;
    }

    template<auto member>
    constexpr static auto concrete_parse(void* output_untyped, Span<TransparentStringView> input) -> Result<> {
        using Base = meta::MemberPointerClass<decltype(member)>;
        using Value = meta::MemberPointerValue<decltype(member)>;

        auto* output = static_cast<Base*>(output_untyped);
        if constexpr (concrete_variadic<member>()) {
            auto& vector = (*output).*member = Value();
            for (auto view : input) {
                if constexpr (concepts::Expected<meta::detail::VectorAllocResult<Value>>) {
                    DI_TRY(vector.emplace_back(DI_TRY(parser::parse<meta::ContainerValue<Value>>(view))));
                } else {
                    vector.emplace_back(DI_TRY(parser::parse<meta::ContainerValue<Value>>(view)));
                }
            }
            return {};
        } else {
            DI_ASSERT(input.size() == 1);
            if constexpr (concepts::Optional<Value>) {
                (*output).*member = DI_TRY(parser::parse<meta::OptionalValue<Value>>(input[0]));
            } else {
                (*output).*member = DI_TRY(parser::parse<Value>(input[0]));
            }
            return {};
        }
    }

public:
    Argument() = default;

    template<auto member>
    constexpr explicit Argument(Constexpr<member>, StringView argument_name = {}, StringView description = {},
                                bool required = false, Optional<ValueType> value_type = {})
        : m_parse(concrete_parse<member>)
        , m_get_values(detail::concrete_get_values<meta::MemberPointerValue<decltype(member)>>)
        , m_default_value(detail::concrete_default_value<member>)
        , m_argument_name(argument_name)
        , m_description(description)
        , m_value_type(value_type.value_or(default_value_type<meta::MemberPointerValue<decltype(member)>>))
        , m_required(required)
        , m_variadic(concrete_variadic<member>()) {
        static_assert(concepts::MemberObjectPointer<decltype(member)>,
                      "Argument member must be a pointer to a member object.");
    }

    constexpr auto parse(void* output, Span<TransparentStringView> input) const {
        DI_ASSERT(m_parse);
        return m_parse(output, input);
    }
    constexpr auto argument_name() const { return m_argument_name; }
    constexpr auto description() const { return m_description; }
    constexpr auto required() const { return m_required; }
    constexpr auto variadic() const { return m_variadic; }

    constexpr auto required_argument_count() const -> usize { return required() ? 1 : 0; }

    constexpr auto display_name() const {
        auto result = di::String {};
        if (!required()) {
            result.push_back(U'[');
        }
        result += m_argument_name;
        if (variadic()) {
            result.push_back(U'.');
            result.push_back(U'.');
            result.push_back(U'.');
        }
        if (!required()) {
            result.push_back(U']');
        }
        return result;
    }

    constexpr auto default_value() const -> String { return m_default_value(); }
    constexpr auto value_type() const -> ValueType { return m_value_type; }
    constexpr auto values() const -> Vector<Tuple<String, StringView>> { return m_get_values(); }

    constexpr auto zsh_completion_spec() const -> String {
        auto var = variadic() ? "*"_sv : ""_sv;
        auto optional = required_argument_count() > 0 ? ""_sv : ":"_sv;
        auto values = this->values();
        return format("{}:{} -- {}:{}"_sv, var, optional, zsh::escape_arg_description(description()),
                      zsh::value_completions(m_value_type, values.span()));
    }

private:
    Parse m_parse { nullptr };
    GetValues m_get_values { nullptr };
    DefaultValue m_default_value { nullptr };
    StringView m_argument_name;
    StringView m_description;
    ValueType m_value_type { ValueType::Unknown };
    bool m_required { false };
    bool m_variadic { false };
};
}
