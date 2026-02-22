#pragma once

#include "di/container/algorithm/max.h"
#include "di/container/string/string_view.h"
#include "di/format/style.h"
#include "di/function/overload.h"
#include "di/io/string_writer.h"
#include "di/io/writer_print.h"
#include "di/types/prelude.h"
#include "di/vocab/error/error.h"
#include "di/vocab/error/status_code.h"
#include "di/vocab/error/status_code_domain.h"
#include "di/vocab/pointer/box.h"
#include "di/vocab/variant/variant.h"
#include "di/vocab/variant/visit.h"

namespace di::cli {
struct ParseArgumentError {
    di::StringView argument_name;
    di::Vector<di::TransparentStringView> bad_values;
    di::Error parse_error;

    auto operator==(ParseArgumentError const&) const -> bool = default;
};

struct ParseOptionError {
    char short_name { 0 };
    di::TransparentStringView long_name;
    di::TransparentStringView bad_value;
    bool is_long { false };
    di::Error parse_error;

    auto operator==(ParseOptionError const&) const -> bool = default;
};

struct ShortOptionMissingRequiredValue {
    char short_name { 0 };

    auto operator==(ShortOptionMissingRequiredValue const&) const -> bool = default;
};

struct LongOptionMissingRequiredValue {
    di::TransparentStringView long_name;

    auto operator==(LongOptionMissingRequiredValue const&) const -> bool = default;
};

struct MissingRequiredOption {
    di::Optional<char> short_name;
    di::Optional<di::TransparentStringView> long_name;

    auto operator==(MissingRequiredOption const&) const -> bool = default;
};

struct MissingRequiredArgument {
    di::StringView argument_name;
    usize count_received { 0 };
    usize count_needed { 0 };

    auto operator==(MissingRequiredArgument const&) const -> bool = default;
};

struct UnknownShortOption {
    char bad_character { 0 };

    auto operator==(UnknownShortOption const&) const -> bool = default;
};

struct UnknownLongOption {
    di::TransparentStringView bad_option;
    di::Optional<di::TransparentStringView> closest_match;

    auto operator==(UnknownLongOption const&) const -> bool = default;
};

struct ExtraArguments {
    di::Vector<di::TransparentStringView> extra_arguments;

    auto operator==(ExtraArguments const&) const -> bool = default;
};

using ErrorVariant =
    Variant<ParseArgumentError, ParseOptionError, ShortOptionMissingRequiredValue, LongOptionMissingRequiredValue,
            MissingRequiredOption, MissingRequiredArgument, UnknownShortOption, UnknownLongOption, ExtraArguments>;

struct ConcreteError {
    ErrorVariant error;
    bool use_colors { false };

    auto operator==(ConcreteError const&) const -> bool = default;
};

class Error {
public:
    Error() = default;

    constexpr explicit Error(ErrorVariant error, bool use_colors)
        : m_value(di::make_box<ConcreteError>(di::move(error), use_colors)) {}

    constexpr auto empty() const { return !m_value; }

    constexpr auto inner() const -> ConcreteError const& { return *m_value; }

    constexpr auto operator==(Error const& other) const -> bool {
        if (this->empty() != other.empty()) {
            return false;
        }
        if (this->empty()) {
            return true;
        }
        return *this->m_value == *other.m_value;
    }

private:
    constexpr friend auto tag_invoke(Tag<concepts::trivially_relocatable>, InPlaceType<Error>) -> bool { return true; }

    di::Box<ConcreteError> m_value;
};

class ErrorDomain;

using ErrorCode = vocab::StatusCode<ErrorDomain>;

class ErrorDomain final : public StatusCodeDomain {
private:
    using Base = StatusCodeDomain;

public:
    using Value = Error;
    using UniqueId = Base::UniqueId;

    constexpr explicit ErrorDomain(UniqueId id = 0x3acfeb8908d1656b) : Base(id) {}

    ErrorDomain(ErrorDomain const&) = default;
    ErrorDomain(ErrorDomain&&) = default;

    auto operator=(ErrorDomain const&) -> ErrorDomain& = default;
    auto operator=(ErrorDomain&&) -> ErrorDomain& = default;

    constexpr static auto get() -> ErrorDomain const&;

    auto name() const -> container::ErasedString override { return container::ErasedString(u8"CLI Error Domain"); }

    auto payload_info() const -> PayloadInfo override {
        return { sizeof(Value), sizeof(Value) + sizeof(StatusCodeDomain const*),
                 container::max(alignof(Value), alignof(StatusCodeDomain const*)) };
    }

protected:
    constexpr auto do_failure(vocab::StatusCode<void> const& code) const -> bool override {
        auto const& value = down_cast(code);
        return !value.value().empty();
    }

    constexpr auto do_equivalent(vocab::StatusCode<void> const& a, vocab::StatusCode<void> const& b) const
        -> bool override {
        DI_ASSERT(a.domain() == *this);
        return b.domain() == *this && down_cast(a).value() == down_cast(b).value();
    }

    constexpr auto do_convert_to_generic(vocab::StatusCode<void> const& a) const -> vocab::GenericCode override {
        DI_ASSERT(a.domain() == *this);
        return a.failure() ? platform::BasicError::InvalidArgument : platform::BasicError::Success;
    }

    constexpr auto do_message(vocab::StatusCode<void> const& code) const -> container::ErasedString override {
        DI_ASSERT(code.domain() == *this);
        using Enc = di::String::Encoding;

        constexpr auto argument_effect = di::FormatColor::Green;
        constexpr auto option_effect = di::FormatColor::Cyan;
        constexpr auto value_effect = di::FormatEffect::Bold;

        auto const& value = down_cast(code).value().inner();
        auto writer = StringWriter<>(value.use_colors);
        di::visit(overload(
                      [&](ParseArgumentError const& error) {
                          writer_print<Enc>(writer, "Failed to parse '{}' as values for argument '{}': {}"_sv,
                                            di::Styled(error.bad_values, value_effect),
                                            di::Styled(error.argument_name, argument_effect), error.parse_error);
                      },
                      [&](ParseOptionError const& error) {
                          auto option_name = error.short_name != 0 ? di::format("-{}"_sv, error.short_name)
                                                                   : di::format("--{}"_sv, error.long_name);
                          writer_print<Enc>(writer, "Failed to parse '{}' as value for option '{}': {}"_sv,
                                            di::Styled(error.bad_value, value_effect),
                                            di::Styled(option_name, option_effect), error.parse_error);
                      },
                      [&](ShortOptionMissingRequiredValue const& error) {
                          auto option_name = di::format("-{}"_sv, error.short_name);
                          writer_print<Enc>(writer, "Missing required value for option '{}'"_sv,
                                            di::Styled(option_name, option_effect));
                      },
                      [&](LongOptionMissingRequiredValue const& error) {
                          auto option_name = di::format("--{}"_sv, error.long_name);
                          writer_print<Enc>(writer, "Missing required value for option '{}'"_sv,
                                            di::Styled(option_name, option_effect));
                      },
                      [&](MissingRequiredOption const& error) {
                          auto option_name = ""_s;
                          if (error.short_name) {
                              option_name.push_back('-');
                              option_name.push_back(error.short_name.value());
                          }
                          if (error.long_name) {
                              if (!option_name.empty()) {
                                  option_name.push_back('/');
                              }
                              option_name.push_back('-');
                              option_name.push_back('-');
                              option_name.append(di::to_string(error.long_name.value()));
                          }
                          writer_print<Enc>(writer, "Option '{}' must be specified"_sv,
                                            di::Styled(option_name, option_effect));
                      },
                      [&](MissingRequiredArgument const& error) {
                          writer_print<Enc>(writer, "Argument '{}' must be provided"_sv,
                                            di::Styled(error.argument_name, argument_effect));
                      },
                      [&](UnknownShortOption const& error) {
                          auto option_name = di::format("-{}"_sv, error.bad_character);
                          writer_print<Enc>(writer, "Option '{}' is not a valid option"_sv,
                                            di::Styled(option_name, option_effect));
                      },
                      [&](UnknownLongOption const& error) {
                          auto option_name = di::format("--{}"_sv, error.bad_option);
                          writer_print<Enc>(writer, "Option '{}' is not a valid option"_sv,
                                            di::Styled(option_name, option_effect));
                          if (error.closest_match) {
                              writer_print<Enc>(
                                  writer, ". Did you mean '{}'?"_sv,
                                  di::Styled(di::format("--{}"_sv, error.closest_match.value()), option_effect));
                          }
                      },
                      [&](ExtraArguments const& error) {
                          writer_print<Enc>(writer, "Extra arguments {} are not recognized"_sv,
                                            di::Styled(error.extra_arguments, value_effect));
                      }),
                  value.error);
        return di::move(writer).output();
    }

private:
    template<typename Domain>
    friend class di::vocab::StatusCode;

    constexpr auto down_cast(vocab::StatusCode<void> const& code) const -> ErrorCode const& {
        DI_ASSERT(code.domain() == *this);
        return static_cast<ErrorCode const&>(code);
    }
};

constexpr inline auto error_domain = ErrorDomain {};

constexpr auto ErrorDomain::get() -> ErrorDomain const& {
    return error_domain;
}

constexpr auto tag_invoke(Tag<vocab::into_status_code>, Error error) {
    return ErrorCode(di::in_place, di::move(error));
}
}
