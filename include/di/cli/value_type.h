#pragma once

#include "di/container/path/prelude.h"
#include "di/format/to_string.h"
#include "di/meta/core.h"
#include "di/meta/language.h"
#include "di/meta/vocab.h"
#include "di/reflect/reflect.h"

namespace di::cli {
enum class ValueType {
    Unknown,
    Enum,
    Path,
    Directory,
    Executable,
    CommandName,
    CommandWithArgs,
};

namespace detail {
    template<typename Value>
    constexpr static auto concrete_get_values() -> Vector<Tuple<String, StringView>> {
        if constexpr (!concepts::Enum<Value> || !concepts::ReflectableToEnumerators<Value>) {
            return {};
        } else {
            auto result = Vector<Tuple<String, StringView>> {};
            tuple_for_each(
                [&](auto enumerator) {
                    result.emplace_back(container::fixed_string_to_utf8_string_view<enumerator.name>().to_owned(),
                                        container::fixed_string_to_utf8_string_view<enumerator.description>());
                },
                reflect(Value()));
            return result;
        }
    }

    template<auto member>
    constexpr static auto concrete_default_value() -> String {
        using Base = meta::MemberPointerClass<decltype(member)>;
        using Value = meta::MemberPointerValue<decltype(member)>;

        if constexpr (concepts::Optional<Value> || concepts::InstanceOf<Value, Vector> ||
                      concepts::SameAs<bool, Value>) {
            return {};
        } else {
            auto v = Base();
            return fmt::to_string(v.*member);
        }
    }
}

namespace detail {
    template<typename T>
    struct DefaultValueType {
        constexpr static auto value = ValueType::Unknown;
    };

    template<>
    struct DefaultValueType<PathView> {
        constexpr static auto value = ValueType::Path;
    };

    template<>
    struct DefaultValueType<Path> {
        constexpr static auto value = ValueType::Path;
    };

    template<typename T>
    struct DefaultValueType<Optional<T>> : DefaultValueType<T> {};

    template<typename T>
    struct DefaultValueType<Vector<T>> : DefaultValueType<T> {};

    template<concepts::Enum T>
    struct DefaultValueType<T> {
        constexpr static auto value = ValueType::Enum;
    };
}

template<typename T>
constexpr inline auto default_value_type = detail::DefaultValueType<meta::RemoveCVRef<T>>::value;
}
