#pragma once

#include "di/cli/value_type.h"
#include "di/container/view/join_with.h"
#include "di/container/view/keys.h"
#include "di/container/view/transform.h"
#include "di/format/prelude.h"
#include "di/vocab/tuple/prelude.h"

namespace di::cli::bash {
// Escape a value used in a double-quoted string.
constexpr auto escape_value(StringView input) -> String {
    auto result = String {};
    for (auto c : input) {
        switch (c) {
            case U'\\':
                result += "\\\\"_sv;
                break;
            case U'"':
                result += "\"'\"'\""_sv;
                break;
            case U'$':
                result += "\\$"_sv;
                break;
            case U'`':
                result += "\\`"_sv;
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

constexpr auto value_completions(ValueType value_type, Span<Tuple<String, StringView>> values) -> String {
    using enum ValueType;
    switch (value_type) {
        case Unknown:
            break;
        case Enum:
            if (!values.empty()) {
                return format(R"~(                    COMPREPLY=($(compgen -W "{}" -- "${{cur}}")))~"_sv,
                              values | keys | transform(escape_value) | join_with(U' ') | di::to<String>());
            }
            [[fallthrough]];
        case Executable:
        case CommandName:
        case CommandWithArgs:
        case Path:
            return R"~(                    COMPREPLY=($(compgen -f "${cur}")))~"_s;
        case Directory:
            return R"~(                    COMPREPLY=()
                    if [[ "${BASH_VERSINFO[0]}" -ge 4 ]]; then
                        compopt -o plusdirs
                    fi)~"_s;
    }
    return R"~(                    COMPREPLY=("${cur}")
                    if [[ "${BASH_VERSINFO[0]}" -ge 4 ]]; then
                        compopt -o nospace
                    fi)~"_s;
}
}
