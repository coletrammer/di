#pragma once

#include "di/function/pipeable.h"
#include "di/meta/language.h"
#include "di/meta/operations.h"
#include "di/meta/util.h"
#include "di/meta/vocab.h"
#include "di/vocab/expected/expected_void_error.h"
#include "di/vocab/expected/expected_void_void.h"

namespace di::vocab {
namespace detail {
    struct TryInfallibleFunction : function::pipeline::EnablePipeline {
        template<concepts::Expected T>
        requires(concepts::LanguageVoid<meta::ExpectedError<T>> &&
                 concepts::ConstructibleFrom<meta::ExpectedValue<T>, meta::Like<T, meta::ExpectedValue<T>>>)
        constexpr auto operator()(T&& value) const -> meta::ExpectedValue<T> {
            return util::forward<T>(value).value();
        }

        constexpr void operator()(Expected<void, void>) const {}

        template<concepts::Expected T>
        requires(!concepts::LanguageVoid<meta::ExpectedError<T>> && concepts::ConstructibleFrom<meta::Decay<T>, T>)
        constexpr auto operator()(T&& value) const -> meta::Decay<T> {
            return util::forward<T>(value);
        }
    };
}

constexpr inline auto try_infallible = detail::TryInfallibleFunction {};
}

namespace di {
using vocab::try_infallible;
}
