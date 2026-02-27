#pragma once

#include "di/any/concepts/impl.h"
#include "di/cli/argument.h"
#include "di/cli/bash.h"
#include "di/cli/error.h"
#include "di/cli/option.h"
#include "di/cli/subcommand.h"
#include "di/cli/zsh.h"
#include "di/container/algorithm/any_of.h"
#include "di/container/algorithm/prelude.h"
#include "di/container/algorithm/rotate.h"
#include "di/container/algorithm/sum.h"
#include "di/container/string/string_view.h"
#include "di/container/string/utf8_encoding.h"
#include "di/container/vector/static_vector.h"
#include "di/container/view/concat.h"
#include "di/container/view/filter.h"
#include "di/container/view/join_with.h"
#include "di/container/view/repeat.h"
#include "di/container/view/transform.h"
#include "di/format/style.h"
#include "di/function/dereference.h"
#include "di/function/monad/monad_try.h"
#include "di/function/not_fn.h"
#include "di/function/prelude.h"
#include "di/io/interface/writer.h"
#include "di/io/string_writer.h"
#include "di/io/writer_print.h"
#include "di/io/writer_println.h"
#include "di/math/numeric_limits.h"
#include "di/meta/algorithm.h"
#include "di/meta/constexpr.h"
#include "di/meta/language.h"
#include "di/meta/operations.h"
#include "di/reflect/prelude.h"
#include "di/util/construct.h"
#include "di/vocab/optional/lift_bool.h"

namespace di::cli {
enum class Shell {
    Bash,
    Zsh,
};

constexpr static auto tag_invoke(Tag<reflect>, InPlaceType<Shell>) {
    using enum Shell;
    return di::make_enumerators<"Shell">(di::enumerator<"bash", Bash, "bash shell">,
                                         di::enumerator<"zsh", Zsh, "zsh shell">);
}

namespace detail {
    template<concepts::Object Base>
    class Parser {
    private:
        constexpr static auto max_options = 100ZU;
        constexpr static auto max_arguments = 100ZU;
        constexpr static auto max_subcommands = 100ZU;

    public:
        constexpr explicit Parser(TransparentStringView app_name, StringView description)
            : m_app_name(app_name), m_description(description) {}

        template<auto member>
        requires(concepts::MemberObjectPointer<decltype(member)> &&
                 concepts::DerivedFrom<Base, meta::MemberPointerClass<decltype(member)>>)
        constexpr auto option(Optional<char> short_name, Optional<TransparentStringView> long_name,
                              StringView description, bool required = false, Optional<StringView> value_name = {},
                              Optional<ValueType> value_type = {}, bool is_help = false) && {
            auto new_option =
                Option { c_<member>, short_name, long_name, description, required, value_name, value_type, is_help };
            DI_ASSERT(m_options.push_back(new_option));
            return di::move(*this);
        }

        constexpr auto help(Optional<char> short_name = {}, Optional<TransparentStringView> long_name = "help"_tsv,
                            StringView description = "Print help message"_sv) {
            static_assert(
                requires { c_<&Base::help>; },
                "A help message requires the argument type to have a boolean help member.");
            static_assert(SameAs<meta::MemberPointerValue<decltype(&Base::help)>, bool>,
                          "A help message requires the argument type to have a boolean help member.");
            return di::move(*this).template option<&Base::help>(short_name, long_name, description, false, {}, {},
                                                                true);
        }

        template<auto member>
        requires(concepts::MemberObjectPointer<decltype(member)> &&
                 concepts::DerivedFrom<Base, meta::MemberPointerClass<decltype(member)>>)
        constexpr auto argument(StringView name, StringView description, bool required = false,
                                Optional<ValueType> value_type = {}) && {
            DI_ASSERT(!has_subcommands());
            auto new_argument = Argument { c_<member>, name, description, required, value_type };
            DI_ASSERT(m_arguments.push_back(new_argument));
            return di::move(*this);
        }

        template<auto member>
        requires(concepts::MemberObjectPointer<decltype(member)> &&
                 concepts::DerivedFrom<Base, meta::MemberPointerClass<decltype(member)>> &&
                 concepts::InstanceOf<meta::MemberPointerValue<decltype(member)>, Variant>)
        constexpr auto subcommands() && {
            DI_ASSERT(!has_arguments());

            auto do_subcommand = [&]<typename Type>(InPlaceType<Type>) {
                if constexpr (!SameAs<Type, Void>) {
                    DI_ASSERT(m_subcommands.push_back(Subcommand { c_<member>, in_place_type<Type> }));
                } else {
                    m_subcommand_is_required = true;
                }
            };

            auto do_subcommands = [&]<typename... Types>(InPlaceType<Variant<Types...>>) {
                (do_subcommand(in_place_type<Types>), ...);
            };

            do_subcommands(in_place_type<meta::MemberPointerValue<decltype(member)>>);
            return di::move(*this);
        }

        template<Impl<io::Writer> Writer>
        // NOLINTNEXTLINE(readability-function-cognitive-complexity)
        constexpr auto parse(Span<TransparentStringView> args, Writer& writer,
                             Span<TransparentStringView> base_commands = {}) -> Result<Base> {
            using namespace di::string_literals;

            // The first argument is the name of the command, so its ignored for parsing.
            auto const use_colors = io::interactive_device(writer);
            if (!args.empty()) {
                args = *args.subspan(1);
            }

            auto seen_options = Array<bool, max_options> {};
            seen_options.fill(false);

            auto count_option_processed = usize { 0 };
            auto subcommand_seen = false;

            auto send_arg_to_back = [&](usize i) {
                container::rotate(*args.subspan(i), args.begin() + i + 1);
                count_option_processed++;
            };

            auto result = Base {};
            for (usize i = 0; i < args.size(); i++) {
                auto arg_index = i - count_option_processed;
                auto arg = args[arg_index];

                // Handle positional argument.
                if (!arg.starts_with('-')) {
                    if (!has_subcommands()) {
                        continue;
                    }

                    // Now we need to match the subcommand to the argument.
                    auto subcommand_index = lookup_subcommand(arg);
                    if (!subcommand_index) {
                        return Unexpected(Error(UnknownSubcommand(arg, closest_subcommand_match(arg)), use_colors));
                    }

                    DI_TRY(subcommand_parse(
                        subcommand_index.value(), &result,
                        args.subspan(arg_index, args.size() - arg_index - count_option_processed).value(), writer,
                        base_commands));
                    count_option_processed = args.size();
                    subcommand_seen = true;
                    break;
                }

                // Handle exactly "--".
                if (arg == "--"_tsv) {
                    send_arg_to_back(arg_index);
                    break;
                }

                // Handle short argument.
                if (!arg.starts_with("--"_tsv)) {
                    for (usize char_index = 1; char_index < arg.size(); char_index++) {
                        auto index = lookup_short_name(arg[char_index]);
                        if (!index) {
                            return Unexpected(Error(UnknownShortOption(arg[char_index]), use_colors));
                        }

                        if (option_is_help(*index)) {
                            write_help(writer, base_commands);
                            return Unexpected(BasicError::Success);
                        }

                        // Parse boolean flag.
                        if (option_boolean(*index)) {
                            DI_TRY(option_parse(*index, seen_options, &result, {}, false, use_colors));
                            continue;
                        }

                        // Parse short option with directly specified value: '-iinput.txt'
                        auto value_view = arg.substr(arg.begin() + isize(char_index + 1));
                        if (!value_view.empty()) {
                            DI_TRY(option_parse(*index, seen_options, &result, value_view, false, use_colors));
                            break;
                        }

                        // Fail if the is no subsequent arguments left.
                        if (i + 1 >= args.size()) {
                            return Unexpected(Error(ShortOptionMissingRequiredValue(arg[char_index]), use_colors));
                        }

                        // Use the next argument as the value.
                        DI_TRY(option_parse(*index, seen_options, &result, args[arg_index + 1], false, use_colors));
                        send_arg_to_back(arg_index);
                        i++;
                    }
                    send_arg_to_back(arg_index);
                    continue;
                }

                // Handle long arguments.
                auto equal = arg.find('=');
                auto name = ""_tsv;
                if (!equal) {
                    name = arg.substr(arg.begin() + 2);
                } else {
                    name = arg.substr(arg.begin() + 2, equal.begin());
                }

                auto index = lookup_long_name(name);
                if (!index) {
                    return Unexpected(Error(UnknownLongOption(name, closest_option_match(name)), use_colors));
                }

                if (option_is_help(*index)) {
                    write_help(writer, base_commands);
                    return Unexpected(BasicError::Success);
                }

                if (option_boolean(*index)) {
                    auto value = Optional<TransparentStringView> {};
                    if (equal) {
                        value = arg.substr(equal.end());
                    }

                    DI_TRY(option_parse(*index, seen_options, &result, value, true, use_colors));
                    send_arg_to_back(arg_index);
                    continue;
                }

                auto value = ""_tsv;
                if (!equal && i + 1 >= args.size()) {
                    return Unexpected(Error(LongOptionMissingRequiredValue(name), use_colors));
                }
                if (!equal) {
                    value = args[arg_index + 1];
                    send_arg_to_back(arg_index);
                    send_arg_to_back(arg_index);
                    i++;
                } else {
                    value = arg.substr(equal.end());
                    send_arg_to_back(arg_index);
                }

                DI_TRY(option_parse(*index, seen_options, &result, value, true, use_colors));
            }

            // Validate all required options were processed.
            for (usize i = 0; i < m_options.size(); i++) {
                if (!seen_options[i] && option_required(i)) {
                    return Unexpected(
                        Error(MissingRequiredOption(m_options[i].short_name(), m_options[i].long_name()), use_colors));
                }
            }

            // If we have subcommands, we should've already matched them.
            if (has_subcommands() && m_subcommand_is_required && !subcommand_seen) {
                return Unexpected(Error(MissingSubcommand(), use_colors));
            }

            // All the positional arguments are now at the front of the array.
            auto positional_arguments = *args.subspan(0, args.size() - count_option_processed);
            if (positional_arguments.size() < minimum_required_argument_count()) {
                auto available_arguments = positional_arguments.size();
                for (auto& argument : m_arguments) {
                    if (argument.required_argument_count() > available_arguments) {
                        return Unexpected(Error(MissingRequiredArgument(argument.argument_name(), available_arguments,
                                                                        argument.required_argument_count()),
                                                use_colors));
                    }
                    available_arguments -= argument.required_argument_count();
                }
            }

            auto argument_index = usize(0);
            auto i = usize(0);
            for (; i < positional_arguments.size(); argument_index++) {
                auto count_to_consume = !argument_variadic(i) ? 1 : positional_arguments.size() - argument_count() + 1;
                auto input = *positional_arguments.subspan(i, count_to_consume);
                DI_TRY(argument_parse(argument_index, &result, input, use_colors));
                i += count_to_consume;
            }
            if (i < positional_arguments.size()) {
                return Unexpected(
                    Error(ExtraArguments(positional_arguments.subspan(i).value() | di::to<Vector>()), use_colors));
            }
            return result;
        }

        template<Impl<io::Writer> Writer>
        // NOLINTNEXTLINE(readability-function-cognitive-complexity)
        constexpr void write_help(Writer& writer, Span<TransparentStringView> base_commands = {}) const {
            using Enc = container::string::Utf8Encoding;

            constexpr auto header_effect = di::FormatEffect::Bold | di::FormatColor::Yellow;
            constexpr auto program_effect = di::FormatEffect::Bold;
            constexpr auto option_effect = di::FormatColor::Cyan;
            constexpr auto option_value_effect = di::FormatColor::Green;
            constexpr auto argument_effect = di::FormatColor::Green;
            constexpr auto subcommand_effect = di::FormatEffect::Bold;

            auto write_values = [&](Vector<Tuple<String, StringView>> values) {
                if (values.empty() || values.size() > 10) {
                    return;
                }
                for (auto const& [value, description] : values) {
                    io::writer_println<Enc>(writer, "      {}{}{}"_sv, di::Styled(value, option_value_effect),
                                            description.empty() ? ""_sv : ": "_sv, description);
                }
            };

            auto program_name =
                concat(base_commands, single(m_app_name)) | join_with(' ') | di::to<TransparentString>();
            io::writer_println<Enc>(writer, "{}"_sv, di::Styled("NAME:"_sv, header_effect));
            io::writer_println<Enc>(writer, "  {}: {}"_sv, di::Styled(program_name, program_effect), m_description);

            io::writer_println<Enc>(writer, "\n{}"_sv, di::Styled("USAGE:"_sv, header_effect));
            io::writer_print<Enc>(writer, "  {}"_sv, di::Styled(program_name, program_effect));
            if (di::any_of(m_options, di::not_fn(&Option::required))) {
                io::writer_print<Enc>(writer, " [OPTIONS]"_sv);
            }
            for (auto const& option : m_options) {
                if (option.required()) {
                    io::writer_print<Enc>(writer, " {}"_sv, di::Styled(option.display_name(), option_effect));

                    if (!option.boolean()) {
                        io::writer_print<Enc>(writer, " {}"_sv,
                                              di::Styled(format("<{}>"_sv, option.value_name()), option_value_effect));
                    }
                }
            }
            for (auto const& argument : m_arguments) {
                io::writer_print<Enc>(writer, " {}"_sv, di::Styled(argument.display_name(), argument_effect));
            }
            if (has_subcommands()) {
                auto display_name = m_subcommand_is_required ? "COMMAND"_sv : "[COMMAND]"_sv;
                io::writer_print<Enc>(writer, " {}"_sv, display_name);
            }
            io::writer_println<Enc>(writer, ""_sv);

            if (has_subcommands()) {
                io::writer_println<Enc>(writer, "\n{}"_sv, di::Styled("COMMANDS:"_sv, header_effect));
                auto first = true;
                for (auto const& subcommand : m_subcommands) {
                    io::writer_println<Enc>(writer, "  {}: {}{}"_sv, di::Styled(subcommand.name(), subcommand_effect),
                                            subcommand.description(),
                                            first && !m_subcommand_is_required ? " (default)"_sv : ""_sv);
                    first = false;
                }
            }

            if (!m_arguments.empty()) {
                io::writer_println<Enc>(writer, "\n{}"_sv, di::Styled("ARGUMENTS:"_sv, header_effect));
                for (auto const& argument : m_arguments) {
                    auto default_value = argument.default_value();
                    if (!default_value.empty() && argument.required_argument_count() == 0) {
                        default_value = format(" (default: {})"_sv, default_value);
                    } else {
                        default_value = {};
                    }
                    io::writer_println<Enc>(writer, "  {}: {}{}"_sv,
                                            di::Styled(argument.display_name(), argument_effect),
                                            argument.description(), default_value);
                    write_values(argument.values());
                }
            }

            if (!m_options.empty()) {
                io::writer_println<Enc>(writer, "\n{}"_sv, di::Styled("OPTIONS:"_sv, header_effect));
                for (auto const& option : m_options) {
                    io::writer_print<Enc>(writer, "  "_sv);
                    if (option.short_name()) {
                        io::writer_print<Enc>(writer, "{}"_sv, di::Styled(option.short_display_name(), option_effect));
                        if (!option.boolean() && !option.long_name()) {
                            io::writer_print<Enc>(
                                writer, " {}"_sv,
                                di::Styled(format("<{}>"_sv, option.value_name()), option_value_effect));
                        }
                    }
                    if (option.short_name() && option.long_name()) {
                        io::writer_print<Enc>(writer, ", "_sv);
                    }
                    if (option.long_name()) {
                        io::writer_print<Enc>(writer, "{}"_sv, di::Styled(option.long_display_name(), option_effect));
                        if (!option.boolean()) {
                            io::writer_print<Enc>(
                                writer, " {}"_sv,
                                di::Styled(format("<{}>"_sv, option.value_name()), option_value_effect));
                        }
                    }
                    auto default_value = option.default_value();
                    if (!default_value.empty() && !option.required()) {
                        default_value = format(" (default: {})"_sv, default_value);
                    } else {
                        default_value = {};
                    }
                    io::writer_println<Enc>(writer, ": {}{}"_sv, option.description(), default_value);
                    write_values(option.values());
                }
            }
        }

        constexpr void bash_completions_inner(TreeMap<Vector<TransparentString>, String>& states,
                                              Span<TransparentStringView> base_commands = {}) {
            auto content = ""_s;
            auto possible_values = Vector<String> {};
            for (auto const& option : m_options) {
                if (option.short_name()) {
                    possible_values.push_back(to_string(option.short_display_name()));
                }
                if (option.long_name()) {
                    possible_values.push_back(to_string(option.long_display_name()));
                }
            }
            for (auto const& subcommand : m_subcommands) {
                auto new_base_commands = base_commands | di::to<Vector>();
                new_base_commands.push_back(m_app_name);
                subcommand.inner_bash_completions(states, new_base_commands.span());

                possible_values.push_back(to_string(subcommand.name()));
            }
            for (auto const& argument : m_arguments) {
                for (auto& [value, _] : argument.values()) {
                    possible_values.push_back(di::move(value));
                }
            }

            content += format("            opts=\"{}\"\n"_sv,
                              possible_values | transform(bash::escape_value) | join_with(U' ') | di::to<String>());
            content += format("            if [[ ${{cur}} == -* || ${{COMP_WORD}} -eq {} ]]; then\n"_sv,
                              base_commands.size() + 1);
            content += format("                COMPREPLY=( $(compgen -W \"${{opts}}\" -- \"${{cur}}\") )\n"_sv);
            content += format("                return 0\n"_sv);
            content += format("            fi\n"_sv);
            content += format("            case \"${{prev}}\" in\n"_sv);
            for (auto const& option : m_options) {
                if (option.boolean()) {
                    continue;
                }
                auto pattern = ""_s;
                if (option.short_name()) {
                    pattern += to_string(option.short_display_name());
                }
                if (option.long_name()) {
                    if (!pattern.empty()) {
                        pattern.push_back(U'|');
                    }
                    pattern += to_string(option.long_display_name());
                }
                content += format("                {})\n"_sv, pattern);
                auto values = option.values();
                content += format("{}\n"_sv, bash::value_completions(option.value_type(), values.span()));
                content += format("                    return 0\n"_sv);
                content += format("                    ;;\n"_sv);
            }
            content += format("                *)\n"_sv);
            content += format("                    COMPREPLY=( $(compgen -W \"${{opts}}\" -- \"${{cur}}\") )\n"_sv);
            content += format("                    return 0\n"_sv);
            content += format("                    ;;\n"_sv);
            content += format("            esac\n"_sv);

            auto state = Vector<TransparentString> {};
            for (auto part : base_commands) {
                state.push_back(part.to_owned());
            }
            state.push_back(m_app_name.to_owned());
            states.try_emplace(di::move(state), content);
        }

        template<Impl<io::Writer> Writer>
        constexpr void write_bash_completions(Writer& writer) {
            using Enc = container::string::Utf8Encoding;

            // The bash completion header primarily just registers our completion function. Unlike
            // zsh, we handle subcommands by determining which subcommand by scanning all the arguments.
            // Then completion logic is handled by compgen.
            //
            // The logic for completions is based off the rust create
            // [clap_complete](https://docs.rs/crate/clap_complete/latest/source/)
            io::writer_println<Enc>(writer, R"~(_{}() {{
    local i cur prev opts cmd
    COMPREPLY=()
    if [[ "${{BASH_VERSINFO[0]}}" -ge 4 ]]; then
        cur="$2"
    else
        cur="${{COMP_WORDS[COMP_CWORD]}}"
    fi
    prev="$3"
    cmd=""
    opts=""
)~"_sv,
                                    m_app_name);

            auto states = TreeMap<Vector<TransparentString>, String> {};
            bash_completions_inner(states);

            auto state_name = [&](Span<TransparentString const> state) -> String {
                return state | join_with('_') | transform(construct<c32>) | di::to<String>();
            };

            io::writer_println<Enc>(writer, "    for i in \"${{COMP_WORDS[@]:0:COMP_CWORD}}\"; do"_sv);
            io::writer_println<Enc>(writer, "        case \"${{cmd}},${{i}}\" in"_sv);
            for (auto const& [state, _] : states) {
                auto condition = "\",$1\""_s;
                if (state.size() > 1) {
                    condition = format("{},{}"_sv, state_name(state.subspan(0, state.size() - 1).value()),
                                       state.back().value());
                }
                io::writer_println<Enc>(writer, "            {})"_sv, condition);
                io::writer_println<Enc>(writer, "                cmd=\"{}\""_sv, state_name(state.span()));
                io::writer_println<Enc>(writer, "                ;;"_sv);
            }
            io::writer_println<Enc>(writer, "            *)  ;;"_sv);
            io::writer_println<Enc>(writer, "        esac"_sv);
            io::writer_println<Enc>(writer, "    done\n"_sv);

            io::writer_println<Enc>(writer, "    case \"${{cmd}}\" in"_sv);
            for (auto const& [state, logic] : states) {
                io::writer_println<Enc>(writer, "        {})"_sv, state_name(state.span()));
                io::writer_print<Enc>(writer, "{}"_sv, logic);
                io::writer_println<Enc>(writer, "            ;;"_sv);
            }
            io::writer_println<Enc>(writer, "    esac"_sv);

            io::writer_println<Enc>(writer, R"~(}}

if [[ "${{BASH_VERSINFO[0]}}" -eq 4 && "${{BASH_VERSINFO[1]}}" -ge 4 || "${{BASH_VERSINFO[0]}}" -gt 4 ]]; then
    complete -F _{} -o nosort -o bashdefault -o default {}
else
    complete -F _{} -o bashdefault -o default {}
fi)~"_sv,
                                    m_app_name, m_app_name, m_app_name, m_app_name);
        }

        template<Impl<io::Writer> Writer>
        void write_zsh_completions_inner(Writer& writer, u32 indentation, Span<TransparentStringView> base_commands) {
            auto indent = repeat(U' ', indentation) | di::to<String>();
            auto extra_indent = "    "_sv;

            using Enc = container::string::Utf8Encoding;
            io::writer_println<Enc>(writer, "{}{}"_sv, indent, R"~(_arguments "${_arguments_options[@]}" : \)~"_sv);
            for (auto const& option : m_options) {
                for (auto const& spec : option.zsh_completion_specs()) {
                    io::writer_println<Enc>(writer, "{}{}'{}' \\"_sv, indent, extra_indent, spec);
                }
            }
            for (auto const& argument : m_arguments) {
                auto spec = argument.zsh_completion_spec();
                io::writer_println<Enc>(writer, "{}{}'{}' \\"_sv, indent, extra_indent, spec);
            }

            auto const state_name = concat(base_commands, single(m_app_name)) | join_with('_') |
                                    transform(construct<c32>) | di::to<String>();
            if (has_subcommands()) {
                // Here we're just adding the optarg spec for the completions. Later we will add the subcommand logic.
                auto possibilities = Vector<Tuple<String, StringView>> {};
                for (auto const& subcommand : m_subcommands) {
                    possibilities.emplace_back(format("{}"_sv, subcommand.name()), subcommand.description());
                }
                io::writer_println<Enc>(writer, "{}{}':::{}' \\"_sv, indent, extra_indent,
                                        zsh::completion_value_map(possibilities.span()));
                io::writer_println<Enc>(writer, "{}{}'*::: :->{}' \\"_sv, indent, extra_indent, state_name);
            }
            io::writer_println<Enc>(writer, "{}{}&& ret=0"_sv, indent, extra_indent);
            if (has_subcommands()) {
                io::writer_println<Enc>(writer, "{}case $state in"_sv, indent);
                io::writer_println<Enc>(writer, "{}({})"_sv, indent, state_name);
                io::writer_println<Enc>(writer, "{}{}words=($line[1] \"${{words[@]}}\")"_sv, indent, extra_indent);
                io::writer_println<Enc>(writer, "{}{}(( CURRENT += 1 ))"_sv, indent, extra_indent);
                io::writer_println<Enc>(writer, "{}{}curcontext=\"${{curcontext%:*:*}}:{}-command-$line[1]:\""_sv,
                                        indent, extra_indent, state_name);
                io::writer_println<Enc>(writer, "{}{}case $line[1] in"_sv, indent, extra_indent);

                for (auto const& subcommand : m_subcommands) {
                    io::writer_println<Enc>(writer, "{}{}{}({})"_sv, indent, extra_indent, extra_indent,
                                            subcommand.name());
                    auto new_base_commands = base_commands | di::to<Vector>();
                    new_base_commands.push_back(m_app_name);
                    subcommand.write_zsh_completions_inner(writer, indentation + 12, new_base_commands.span());
                    io::writer_println<Enc>(writer, "{}{}{};;"_sv, indent, extra_indent, extra_indent);
                }

                io::writer_println<Enc>(writer, "{}{}esac"_sv, indent, extra_indent);
                io::writer_println<Enc>(writer, "{}{};;"_sv, indent, extra_indent);
                io::writer_println<Enc>(writer, "{}esac"_sv, indent);
            }
        }

        template<Impl<io::Writer> Writer>
        void write_zsh_completions(Writer& writer) {
            using Enc = container::string::Utf8Encoding;

            // The zsh completion header most declares the completion function and sets up the arguments.
            // We want -s as we supported stacke short options, and -S since we support `--` as a delimiter.
            // -C is needed for subcommand handling.
            //
            // The logic for completions is based off the rust create
            // [clap_complete](https://docs.rs/crate/clap_complete/latest/source/)
            io::writer_println<Enc>(writer, R"~(#compdef {}

autoload -U is-at-least

_{}() {{
    typeset -A opt_args
    typeset -a _arguments_options
    local ret=1

    if is-at-least 5.2; then
        _arguments_options=(-s -S -C)
    else
        _arguments_options=(-s -C)
    fi

    local context curcontext="$curcontext" state line)~"_sv,
                                    m_app_name, m_app_name);

            write_zsh_completions_inner(writer, 4, {});

            io::writer_println<Enc>(writer, R"~(}}

if [ "$funcstack[1]" = "_{}" ]; then
    _{} "$@"
else
    compdef _{} {}
fi)~"_sv,
                                    m_app_name, m_app_name, m_app_name, m_app_name);
        }

        template<Impl<io::Writer> Writer>
        void write_completions(Writer& writer, Shell shell) {
            using enum Shell;
            switch (shell) {
                case Bash:
                    return write_bash_completions(writer);
                case Zsh:
                    return write_zsh_completions(writer);
            }
        };

        constexpr auto help_string(Span<TransparentStringView> base_commands = {}) const {
            auto writer = di::StringWriter {};
            write_help(writer, base_commands);
            return di::move(writer).output();
        }

        constexpr auto app_name() const -> TransparentStringView { return m_app_name; }
        constexpr auto app_description() const -> StringView { return m_description; }
        constexpr auto has_subcommands() const -> bool { return !m_subcommands.empty(); }
        constexpr auto has_arguments() const -> bool { return !m_arguments.empty(); }

    private:
        constexpr static auto closest_match(TransparentStringView name, Span<TransparentStringView> possibilities)
            -> Optional<TransparentStringView> {
            if (name.empty()) {
                return {};
            }

            auto result = ""_tsv;
            auto score = NumericLimits<i32>::max;
            for (auto possibility : possibilities) {
                if (possibility.empty()) {
                    continue;
                }

                auto dp = di::Vector<di::Vector<i32>> {};
                for (auto _ : range(name.size() + 1)) {
                    dp.push_back(repeat(NumericLimits<i32>::max, possibility.size() + 1) | di::to<Vector>());
                }

                for (auto x : range(dp.size())) {
                    dp[x][0] = i32(x);
                }
                for (auto x : range(dp[0].size())) {
                    dp[0][x] = i32(x);
                }

                for (auto i : range(1zu, dp.size())) {
                    for (auto j : range(1zu, dp[i].size())) {
                        dp[i][j] = di::min({
                            1 + dp[i - 1][j],
                            1 + dp[i][j - 1],
                            i32(name[i] != possibility[j]) + dp[i - 1][j - 1],
                        });
                    }
                }

                auto new_score = dp.back().value().back().value();
                if (new_score < score) {
                    result = possibility;
                    score = new_score;
                }
            }
            if (score <= i32(name.size()) / 2 + 1) {
                return result;
            }
            return {};
        }

        constexpr auto closest_option_match(TransparentStringView name) const -> Optional<TransparentStringView> {
            auto possibilities = m_options | transform(&Option::long_name) |
                                 filter(&Optional<TransparentStringView>::has_value) | transform(dereference) |
                                 di::to<Vector>();
            return closest_match(name, possibilities.span());
        }

        constexpr auto closest_subcommand_match(TransparentStringView name) const -> Optional<TransparentStringView> {
            auto possibilities = m_subcommands | transform(&Subcommand::name) | di::to<Vector>();
            return closest_match(name, possibilities.span());
        }

        constexpr auto option_required(usize index) const -> bool { return m_options[index].required(); }
        constexpr auto option_boolean(usize index) const -> bool { return m_options[index].boolean(); }
        constexpr auto option_is_help(usize index) const -> bool { return m_options[index].is_help(); }

        constexpr auto option_parse(usize index, Span<bool> seen_arguments, Base* output,
                                    Optional<TransparentStringView> input, bool is_long, bool use_colors) const
            -> Result<void> {
            DI_TRY(m_options[index].parse(output, input).transform_error([&](auto error) -> di::Error {
                return Error(
                    ParseOptionError {
                        .short_name = !is_long ? m_options[index].short_name().value_or('\0') : '\0',
                        .long_name = is_long ? m_options[index].long_name().value_or(""_tsv) : ""_tsv,
                        .bad_value = input.value_or(""_tsv),
                        .is_long = is_long,
                        .parse_error = di::move(error),
                    },
                    use_colors);
            }));
            seen_arguments[index] = true;
            return {};
        }

        constexpr auto argument_variadic(usize index) const -> bool { return m_arguments[index].variadic(); }

        constexpr auto argument_parse(usize index, Base* output, Span<TransparentStringView> input,
                                      bool use_colors) const -> Result<void> {
            return m_arguments[index].parse(output, input).transform_error([&](auto error) -> di::Error {
                return Error(
                    ParseArgumentError {
                        .argument_name = m_arguments[index].argument_name(),
                        .bad_values = input | di::to<Vector>(),
                        .parse_error = di::move(error),
                    },
                    use_colors);
            });
        }

        constexpr auto minimum_required_argument_count() const -> usize {
            return di::sum(m_arguments | di::transform(&Argument::required_argument_count));
        }

        constexpr auto argument_count() const -> usize { return m_arguments.size(); }

        constexpr auto subcommand_parse(usize index, Base* output, Span<TransparentStringView> input,
                                        AnyRef<Writer> writer, Span<TransparentStringView> base_commands) const
            -> Result<> {
            auto new_base_commands = base_commands | di::to<Vector>();
            new_base_commands.push_back(m_app_name);
            return m_subcommands[index]
                .parse(output, input, writer, new_base_commands.span())
                .transform_error([&](auto error) -> di::Error {
                    if (error.success()) {
                        return error;
                    }
                    return Error(
                        ParseSubcommandError {
                            .subcommand_name = m_subcommands[index].name(),
                            .parse_error = di::move(error),
                        },
                        io::interactive_device(writer));
                });
        }

        constexpr auto lookup_short_name(char short_name) const -> Optional<usize> {
            auto const* it = di::find(m_options, short_name, &Option::short_name);
            return lift_bool(it != m_options.end()) % [&] {
                return usize(it - m_options.begin());
            };
        }

        constexpr auto lookup_long_name(TransparentStringView long_name) const -> Optional<usize> {
            auto const* it = di::find(m_options, long_name, &Option::long_name);
            return lift_bool(it != m_options.end()) % [&] {
                return usize(it - m_options.begin());
            };
        }

        constexpr auto lookup_subcommand(TransparentStringView subcommand_name) const -> Optional<usize> {
            auto const* it = di::find(m_subcommands, subcommand_name, &Subcommand::name);
            return lift_bool(it != m_subcommands.end()) % [&] {
                return usize(it - m_subcommands.begin());
            };
        }

        TransparentStringView m_app_name;
        StringView m_description;
        StaticVector<Option, Constexpr<max_options>> m_options;
        StaticVector<Argument, Constexpr<max_arguments>> m_arguments;
        StaticVector<Subcommand, Constexpr<max_subcommands>> m_subcommands;
        bool m_subcommand_is_required { false };
    };
}

template<concepts::Object T>
constexpr auto cli_parser(TransparentStringView app_name, StringView description) {
    return detail::Parser<T> { app_name, description };
}
}

namespace di {
using cli::cli_parser;
}
