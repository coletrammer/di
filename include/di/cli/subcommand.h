#pragma once

#include "di/any/container/any.h"
#include "di/cli/get_cli_parser.h"
#include "di/container/prelude.h"
#include "di/container/string/prelude.h"
#include "di/container/string/string_view.h"
#include "di/function/prelude.h"
#include "di/io/interface/writer.h"
#include "di/meta/language.h"
#include "di/parser/prelude.h"
#include "di/vocab/optional/optional_forward_declaration.h"
#include "di/vocab/prelude.h"

namespace di::cli::detail {
class Subcommand {
private:
    using Parse = Result<> (*)(void*, Span<TransparentStringView>, AnyRef<Writer>, Span<TransparentStringView>);
    using ZshCompletionsInner = void (*)(AnyRef<Writer>, u32, Span<TransparentStringView>);

    template<auto member, typename T>
    constexpr static auto concrete_parse(void* output_untyped, Span<TransparentStringView> input, AnyRef<Writer> writer,
                                         Span<TransparentStringView> base_commands = {}) -> Result<> {
        using Base = meta::MemberPointerClass<decltype(member)>;

        auto output = static_cast<Base*>(output_untyped);
        auto parser = get_cli_parser<T>();
        (*output).*member = DI_TRY(parser.parse(input, writer, base_commands));
        return {};
    }

    template<typename T>
    constexpr static void concrete_write_zsh_completions_inner(AnyRef<Writer> writer, u32 indentation,
                                                               Span<TransparentStringView> base_commands) {
        auto parser = get_cli_parser<T>();
        parser.write_zsh_completions_inner(writer, indentation, base_commands);
    }

public:
    Subcommand() = default;

    template<auto member, typename T>
    constexpr explicit Subcommand(Constexpr<member>, InPlaceType<T>)
        : m_parse(concrete_parse<member, T>)
        , m_write_zsh_completionions_inner(concrete_write_zsh_completions_inner<T>)
        , m_name(get_cli_parser<T>().app_name())
        , m_description(get_cli_parser<T>().app_description()) {}

    constexpr auto parse(void* base, Span<TransparentStringView> input, AnyRef<Writer> writer,
                         Span<TransparentStringView> base_commands = {}) const {
        DI_ASSERT(m_parse);
        return m_parse(base, input, writer, base_commands);
    }

    constexpr void write_zsh_completions_inner(AnyRef<Writer> writer, u32 indentation,
                                               Span<TransparentStringView> base_commands) const {
        m_write_zsh_completionions_inner(writer, indentation, base_commands);
    }

    constexpr auto name() const -> TransparentStringView { return m_name; }
    constexpr auto description() const -> StringView { return m_description; }

private:
    Parse m_parse { nullptr };
    ZshCompletionsInner m_write_zsh_completionions_inner { nullptr };
    TransparentStringView m_name;
    StringView m_description;
};
}
