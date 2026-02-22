#pragma once

#include "di/container/algorithm/max.h"
#include "di/container/string/encoding.h"
#include "di/container/string/erased_string.h"
#include "di/container/string/string.h"
#include "di/container/string/string_impl.h"
#include "di/format/concepts/formattable.h"
#include "di/format/format_string_impl.h"
#include "di/format/make_format_args.h"
#include "di/format/prelude.h"
#include "di/format/present.h"
#include "di/format/vpresent_encoded.h"
#include "di/platform/prelude.h"
#include "di/types/prelude.h"
#include "di/vocab/error/error.h"
#include "di/vocab/error/status_code.h"
#include "di/vocab/error/status_code_domain.h"
#include "di/vocab/pointer/arc.h"
#include "di/vocab/pointer/box.h"

namespace di::vocab {
class StringError {
public:
    StringError() = default;

    constexpr explicit StringError(String value) : m_value(di::make_box<String>(di::move(value))) {}

    constexpr auto empty() const { return !m_value; }

    constexpr auto operator==(StringError const& other) const -> bool {
        if (this->empty() != other.empty()) {
            return false;
        }
        if (this->empty()) {
            return true;
        }
        return *this->m_value == *other.m_value;
    }

    auto erased() const -> ErasedString {
        if (!m_value) {
            return "Success"_sv;
        }
        // TODO: use a reference counted string instead to avoid cloning().
        return m_value->clone();
    }

private:
    constexpr friend auto tag_invoke(Tag<concepts::trivially_relocatable>, InPlaceType<StringError>) -> bool {
        return true;
    }

    Box<String> m_value;
};

class StringErrorDomain;

using StringErrorCode = vocab::StatusCode<StringErrorDomain>;

class StringErrorDomain final : public StatusCodeDomain {
private:
    using Base = StatusCodeDomain;

public:
    using Value = StringError;
    using UniqueId = Base::UniqueId;

    constexpr explicit StringErrorDomain(UniqueId id = 0x2b0b934342552b05) : Base(id) {}

    StringErrorDomain(StringErrorDomain const&) = default;
    StringErrorDomain(StringErrorDomain&&) = default;

    auto operator=(StringErrorDomain const&) -> StringErrorDomain& = default;
    auto operator=(StringErrorDomain&&) -> StringErrorDomain& = default;

    constexpr static auto get() -> StringErrorDomain const&;

    auto name() const -> container::ErasedString override { return container::ErasedString(u8"String Error Domain"); }

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
        return down_cast(code).value().erased();
    }

    constexpr void do_erased_destroy(StatusCode<void>& code, size_t) const override { down_cast(code).value() = {}; }

private:
    template<typename Domain>
    friend class di::vocab::StatusCode;

    constexpr auto down_cast(vocab::StatusCode<void> const& code) const -> StringErrorCode const& {
        DI_ASSERT(code.domain() == *this);
        return static_cast<StringErrorCode const&>(code);
    }

    constexpr auto down_cast(vocab::StatusCode<void>& code) const -> StringErrorCode& {
        DI_ASSERT(code.domain() == *this);
        return static_cast<StringErrorCode&>(code);
    }
};

constexpr inline auto error_domain = StringErrorDomain {};

constexpr auto StringErrorDomain::get() -> StringErrorDomain const& {
    return error_domain;
}

constexpr auto tag_invoke(di::types::Tag<di::vocab::into_status_code>, di::vocab::StringError error) {
    return di::vocab::StringErrorCode(di::in_place, di::move(error));
}

namespace detail {
    struct FormatError {
        using Enc = di::String::Encoding;

        template<concepts::Formattable... Args>
        constexpr static auto operator()(format::FormatStringImpl<Enc, Args...> format, Args&&... args)
            -> StringErrorCode {
            return StringError(
                *format::vpresent_encoded<Enc>(format, format::make_format_args<format::FormatContext<Enc>>(args...)));
        }
    };
}
}

namespace di {
using vocab::StringError;
using vocab::StringErrorCode;

constexpr inline auto format_error = vocab::detail::FormatError {};
}
