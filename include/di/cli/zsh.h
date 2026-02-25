#pragma once

#include "di/cli/value_type.h"
#include "di/container/action/to.h"
#include "di/container/algorithm/all_of.h"
#include "di/container/interface/empty.h"
#include "di/container/string/prelude.h"
#include "di/container/vector/prelude.h"
#include "di/container/view/join_with.h"
#include "di/container/view/transform.h"
#include "di/format/prelude.h"
#include "di/vocab/tuple/prelude.h"

namespace di::cli::zsh {
// Escape a description used in spec like:
//   --flag[description]
// OR
//   ::command:((name\:"description"))
constexpr auto escape_description(StringView input) -> String {
    auto result = String {};
    for (auto c : input) {
        switch (c) {
            case U'\\':
                result += "\\\\"_sv;
                break;
            case U'\'':
                result += "'\\''"_sv;
                break;
            case U'[':
                result += "\\["_sv;
                break;
            case U']':
                result += "\\]"_sv;
                break;
            case U':':
                result += "\\:"_sv;
                break;
            case U'$':
                result += "\\$"_sv;
                break;
            case U'`':
                result += "\\`"_sv;
                break;
            case U'\n':
                result += " "_sv;
                break;
            default:
                result.push_back(c);
                break;
        }
    }
    return result;
}

// Escape a description used in spec like:
//   :: -- description:_default
constexpr auto escape_arg_description(StringView input) -> String {
    auto result = String {};
    for (auto c : input) {
        switch (c) {
            case U'\'':
                result += "'\\''"_sv;
                break;
            case U'[':
                result += "\\["_sv;
                break;
            case U']':
                result += "\\]"_sv;
                break;
            case U':':
                result += "\\:"_sv;
                break;
            default:
                result.push_back(c);
                break;
        }
    }
    return result;
}

// Escape a key used in an spec like:
//   ::command:((key\:"value"))
constexpr auto escape_key(StringView input) -> String {
    auto result = String {};
    for (auto c : input) {
        switch (c) {
            case U'\\':
                result += "\\\\"_sv;
                break;
            case U'\'':
                result += "'\\''"_sv;
                break;
            case U'[':
                result += "\\["_sv;
                break;
            case U']':
                result += "\\]"_sv;
                break;
            case U':':
                result += "\\:"_sv;
                break;
            case U'$':
                result += "\\$"_sv;
                break;
            case U'`':
                result += "\\`"_sv;
                break;
            case U'(':
                result += "\\("_sv;
                break;
            case U')':
                result += "\\)"_sv;
                break;
            case U' ':
                result += "\\ "_sv;
                break;
            default:
                result.push_back(c);
                break;
        }
    }
    return result;
}

constexpr auto completion_value_map(Span<Tuple<String, StringView>> input) -> String {
    if (all_of(input, empty, [](auto const& item) {
            return di::get<1>(item);
        })) {
        auto inner = keys(input) | join_with(U' ') | di::to<String>();
        return format("({})"_sv, inner);
    }
    auto inner = input | transform([](auto const& item) {
                     auto const& [key, description] = item;
                     return format("{}\\:\"{}\""_sv, escape_key(key), escape_description(description));
                 }) |
                 join_with(U' ') | di::to<String>();
    return format("(({}))"_sv, inner);
}

constexpr auto value_completions(ValueType value_type, Span<Tuple<String, StringView>> values) -> String {
    using enum ValueType;
    switch (value_type) {
        case Unknown:
            return {};
        case Path:
            return "_files"_s;
        case Directory:
            return "_files -/"_s;
        case Executable:
            return "_absolute_command_paths"_s;
        case CommandName:
            return "_command_names -e"_s;
        case CommandWithArgs:
            return "_cmdambivalent"_s;
        case Enum: {
            if (!values.empty()) {
                return zsh::completion_value_map(values.span());
            }
            return {};
        }
    }
    return {};
}
}
