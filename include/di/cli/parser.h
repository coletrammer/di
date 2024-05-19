#pragma once

#include <di/cli/argument.h>
#include <di/cli/option.h>
#include <di/container/algorithm/prelude.h>
#include <di/container/algorithm/rotate.h>
#include <di/container/algorithm/sum.h>
#include <di/container/string/string_view.h>
#include <di/container/vector/static_vector.h>
#include <di/function/monad/monad_try.h>
#include <di/function/prelude.h>
#include <di/meta/constexpr.h>
#include <di/vocab/optional/lift_bool.h>

namespace di::cli {
namespace detail {
    template<concepts::Object Base>
    class Parser {
    private:
        constexpr static auto max_options = 100zu;
        constexpr static auto max_arguments = 100zu;

    public:
        constexpr explicit Parser(Optional<StringView> app_name, Optional<StringView> description)
            : m_app_name(app_name), m_description(description) {}

        template<auto member>
        requires(concepts::MemberObjectPointer<decltype(member)> &&
                 concepts::SameAs<Base, meta::MemberPointerClass<decltype(member)>>)
        constexpr auto flag(Optional<char> short_name, Optional<TransparentStringView> long_name = {},
                            Optional<StringView> description = {}, bool required = false) && {
            auto new_option = Option { c_<member>, short_name, long_name, description, required };
            DI_ASSERT(m_options.push_back(new_option));
            return di::move(*this);
        }

        template<auto member>
        requires(concepts::MemberObjectPointer<decltype(member)> &&
                 concepts::SameAs<Base, meta::MemberPointerClass<decltype(member)>>)
        constexpr auto argument(Optional<StringView> name = {}, Optional<StringView> description = {},
                                bool required = false) && {
            auto new_argument = Argument { c_<member>, name, description, required };
            DI_ASSERT(m_arguments.push_back(new_argument));
            return di::move(*this);
        }

        constexpr Result<Base> parse(Span<TransparentStringView> args) {
            using namespace di::string_literals;

            if (args.empty()) {
                return Unexpected(BasicError::InvalidArgument);
            }
            args = *args.subspan(1);

            auto seen_arguments = Array<bool, max_options> {};
            seen_arguments.fill(false);

            auto count_option_processed = usize { 0 };

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
                    continue;
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
                            return Unexpected(BasicError::InvalidArgument);
                        }

                        // Parse boolean flag.
                        if (option_boolean(*index)) {
                            DI_TRY(option_parse(*index, seen_arguments, &result, {}));
                            continue;
                        }

                        // Parse short option with directly specified value: '-iinput.txt'
                        auto value_view = arg.substr(arg.begin() + char_index + 1);
                        if (!value_view.empty()) {
                            DI_TRY(option_parse(*index, seen_arguments, &result, value_view));
                            break;
                        }

                        // Fail if the is no subsequent arguments left.
                        if (i + 1 >= args.size()) {
                            return Unexpected(BasicError::InvalidArgument);
                        }

                        // Use the next argument as the value.
                        DI_TRY(option_parse(*index, seen_arguments, &result, args[arg_index + 1]));
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
                    return Unexpected(BasicError::InvalidArgument);
                }

                if (option_boolean(*index)) {
                    if (equal) {
                        return Unexpected(BasicError::InvalidArgument);
                    }
                    DI_TRY(option_parse(*index, seen_arguments, &result, {}));
                    send_arg_to_back(arg_index);
                    continue;
                }

                auto value = ""_tsv;
                if (!equal && i + 1 >= args.size()) {
                    return Unexpected(BasicError::InvalidArgument);
                } else if (!equal) {
                    value = args[arg_index + 1];
                    send_arg_to_back(arg_index);
                    send_arg_to_back(arg_index);
                    i++;
                } else {
                    value = arg.substr(equal.end());
                    send_arg_to_back(arg_index);
                }

                DI_TRY(option_parse(*index, seen_arguments, &result, value));
            }

            // Validate all required arguments were processed.
            for (usize i = 0; i < m_options.size(); i++) {
                if (!seen_arguments[i] && option_required(i)) {
                    return Unexpected(BasicError::InvalidArgument);
                }
            }

            // All the positional arguments are now at the front of the array.
            auto positional_arguments = *args.subspan(0, args.size() - count_option_processed);
            if (positional_arguments.size() < minimum_required_argument_count()) {
                return Unexpected(BasicError::InvalidArgument);
            }

            auto argument_index = usize(0);
            for (auto i = usize(0); i < positional_arguments.size(); argument_index++) {
                auto count_to_consume = !argument_variadic(i) ? 1 : positional_arguments.size() - argument_count() + 1;
                auto input = *positional_arguments.subspan(i, count_to_consume);
                DI_TRY(argument_parse(argument_index, &result, input));
                i += count_to_consume;
            }
            return result;
        }

    private:
        constexpr bool option_required(usize index) const { return m_options[index].required(); }

        constexpr bool option_boolean(usize index) const { return m_options[index].boolean(); }

        constexpr Result<void> option_parse(usize index, Span<bool> seen_arguments, Base* output,
                                            Optional<TransparentStringView> input) const {
            DI_TRY(m_options[index].parse(output, input));
            seen_arguments[index] = true;
            return {};
        }

        constexpr bool argument_variadic(usize index) const { return m_arguments[index].variadic(); }

        constexpr Result<void> argument_parse(usize index, Base* output, Span<TransparentStringView> input) const {
            return m_arguments[index].parse(output, input);
        }

        constexpr usize minimum_required_argument_count() const {
            return di::sum(m_arguments | di::transform(&Argument::required_argument_count));
        }

        constexpr usize argument_count() const { return m_arguments.size(); }

        constexpr Optional<usize> lookup_short_name(char short_name) const {
            auto it = di::find(m_options, short_name, &Option::short_name);
            return lift_bool(it != m_options.end()) % [&] {
                return usize(it - m_options.begin());
            };
        }

        constexpr Optional<usize> lookup_long_name(TransparentStringView long_name) const {
            auto it = di::find(m_options, long_name, &Option::long_name);
            return lift_bool(it != m_options.end()) % [&] {
                return usize(it - m_options.begin());
            };
        }

        Optional<StringView> m_app_name;
        Optional<StringView> m_description;
        StaticVector<Option, Constexpr<max_options>> m_options;
        StaticVector<Argument, Constexpr<max_arguments>> m_arguments;
    };
}

template<concepts::Object T>
constexpr auto cli_parser(Optional<StringView> app_name = {}, Optional<StringView> description = {}) {
    return detail::Parser<T> { app_name, description };
}
}

namespace di {
using cli::cli_parser;
}
