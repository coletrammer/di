#pragma once

#include "di/container/algorithm/max.h"
#include "di/container/string/string_view.h"
#include "di/format/style.h"
#include "di/function/overload.h"
#include "di/io/string_writer.h"
#include "di/io/writer_print.h"
#include "di/types/prelude.h"
#include "di/vocab/error/error.h"
#include "di/vocab/error/into_status_code.h"
#include "di/vocab/error/prelude.h"
#include "di/vocab/error/status_code.h"
#include "di/vocab/error/status_code_domain.h"
#include "di/vocab/pointer/box.h"
#include "di/vocab/variant/variant.h"
#include "di/vocab/variant/visit.h"

namespace di::serialization::json_deserializer {
struct ReadError {
    di::Error error;

    auto operator==(ReadError const&) const -> bool = default;
};

struct ParseBoolError {
    auto operator==(ParseBoolError const&) const -> bool = default;
};

struct ParseEnumError {
    di::String value;
    di::Vector<String> possible;

    auto operator==(ParseEnumError const&) const -> bool = default;
};

struct ParseValueError {
    di::String value;
    di::Error error;

    auto operator==(ParseValueError const&) const -> bool = default;
};

struct ParseNumberError {
    di::String input;

    auto operator==(ParseNumberError const&) const -> bool = default;
};

struct UnexpectedKeyError {
    di::String key;

    auto operator==(UnexpectedKeyError const&) const -> bool = default;
};

struct UnexpectedCharacterError {
    c32 code_point {};
    di::Optional<c32> expected {};

    auto operator==(UnexpectedCharacterError const&) const -> bool = default;
};

struct MissingArrayElementsError {
    usize actual { 0 };
    usize expected { 0 };

    auto operator==(MissingArrayElementsError const&) const -> bool = default;
};

struct UnexpectedEndOfInputError {
    auto operator==(UnexpectedEndOfInputError const&) const -> bool = default;
};

struct InvalidUtf8Error {
    auto operator==(InvalidUtf8Error const&) const -> bool = default;
};

using ErrorVariant =
    Variant<ReadError, ParseBoolError, ParseEnumError, ParseValueError, ParseNumberError, UnexpectedKeyError,
            UnexpectedCharacterError, MissingArrayElementsError, UnexpectedEndOfInputError, InvalidUtf8Error>;

struct ConcreteError {
    ErrorVariant error;
    di::String key;

    auto operator==(ConcreteError const&) const -> bool = default;
};

class ErrorDomain;
class Error;
using ErrorCode = vocab::StatusCode<ErrorDomain>;

class Error {
public:
    Error() = default;

    constexpr explicit Error(ErrorVariant error, di::String key)
        : m_value(di::make_box<ConcreteError>(di::move(error), di::move(key))) {}

    constexpr auto empty() const { return !m_value; }

    constexpr auto inner() -> ConcreteError& { return *m_value; }
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

    constexpr auto into_status_code() && -> ErrorCode;

private:
    constexpr friend auto tag_invoke(Tag<concepts::trivially_relocatable>, InPlaceType<Error>) -> bool { return true; }

    di::Box<ConcreteError> m_value;
};

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

    constexpr auto name() const -> container::ErasedString override {
        return container::ErasedString(u8"JSON Deserializer Error Domain");
    }

    constexpr auto payload_info() const -> PayloadInfo override {
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

        auto const& value = down_cast(code).value().inner();
        auto writer = StringWriter<>();
        di::visit(
            overload(
                [&](ReadError const& error) {
                    writer_print<Enc>(writer, "Failing reading input stream while parsing '{}': {}"_sv, value.key,
                                      error.error);
                },
                [&](ParseBoolError) {
                    writer_print<Enc>(writer, "Cannot parse '{}' as boolean"_sv, value.key);
                },
                [&](ParseEnumError const& error) {
                    writer_print<Enc>(writer, "Cannot parse '{}': '{}' as enum. Expected one of [ {} ]"_sv, value.key,
                                      error.value, error.possible | di::join_with(", "_sv) | di::to<di::String>());
                },
                [&](ParseValueError const& error) {
                    writer_print<Enc>(writer, "Cannot parse '{}' as value for key '{}': {}"_sv, error.value, value.key,
                                      error.error);
                },
                [&](ParseNumberError const& error) {
                    writer_print<Enc>(writer, "Cannot parse '{}': '{}' as number"_sv, value.key, error.input);
                },
                [&](UnexpectedKeyError const& error) {
                    writer_print<Enc>(writer, "Unexpected key '{}' while parsing '{}'"_sv, error.key, value.key);
                },
                [&](UnexpectedEndOfInputError) {
                    writer_print<Enc>(writer, "Unexpected end of input while parsing '{}'"_sv, value.key);
                },
                [&](MissingArrayElementsError const& error) {
                    writer_print<Enc>(writer, "Expected {} elements but got {} while parsing '{}'"_sv, error.expected,
                                      error.actual, value.key);
                },
                [&](UnexpectedCharacterError const& error) {
                    if (error.expected) {
                        writer_print<Enc>(writer, "Unexpected character while parsing '{}' (expected {}): {:?}"_sv,
                                          value.key, error.expected.value(), error.code_point);
                    } else {
                        writer_print<Enc>(writer, "Unexpected character while parsing '{}': {:#4x}"_sv, value.key,
                                          error.code_point);
                    }
                },
                [&](InvalidUtf8Error) {
                    writer_print<Enc>(writer, "Invalid UTF-8 byte detected while parsing '{}'"_sv, value.key);
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

constexpr auto Error::into_status_code() && -> ErrorCode {
    return ErrorCode(di::in_place, di::move(*this));
}

constexpr inline auto error_domain = ErrorDomain {};

constexpr auto ErrorDomain::get() -> ErrorDomain const& {
    return error_domain;
}
}
