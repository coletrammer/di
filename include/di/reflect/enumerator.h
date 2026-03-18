#pragma once

#include "di/container/string/fixed_string.h"
#include "di/meta/core.h"
#include "di/meta/language.h"
#include "di/vocab/tuple/tuple.h"

namespace di::reflection {
template<container::FixedString enumerator_name, auto enumerator_value, container::FixedString enumerator_description>
requires(concepts::Enum<decltype(enumerator_value)>)
struct Enumerator {
    constexpr static auto name = enumerator_name;
    constexpr static auto value = enumerator_value;
    constexpr static auto description = enumerator_description;

    using Type = decltype(enumerator_value);

    constexpr static auto get() -> Type { return value; }

    constexpr static auto is_fields() -> bool { return false; }
    constexpr static auto is_field() -> bool { return false; }
    constexpr static auto is_enumerator() -> bool { return true; }
    constexpr static auto is_enumerators() -> bool { return false; }
    constexpr static auto is_atom() -> bool { return false; }
    constexpr static auto is_integer() -> bool { return false; }
    constexpr static auto is_bool() -> bool { return false; }
    constexpr static auto is_string() -> bool { return false; }
    constexpr static auto is_list() -> bool { return false; }
    constexpr static auto is_tuple() -> bool { return false; }
    constexpr static auto is_map() -> bool { return false; }
    constexpr static auto is_variant() -> bool { return false; }
    constexpr static auto is_box() -> bool { return false; }
    constexpr static auto is_custom_atom() -> bool { return false; }

    auto operator==(Enumerator const&) const -> bool = default;
    auto operator<=>(Enumerator const&) const = default;
};

template<container::FixedString enumerator_name, auto enumerator_value, container::FixedString description = "">
requires(concepts::Enum<decltype(enumerator_value)>)
constexpr auto enumerator = Enumerator<enumerator_name, enumerator_value, description> {};
}

namespace di::concepts {
template<typename T>
concept Enumerator = requires {
    { T::is_enumerator() } -> concepts::SameAs<bool>;
} && T::is_enumerator();
}

namespace di::reflection {
template<concepts::Constexpr EnumName, concepts::Constexpr Description, concepts::Enumerator... Es>
struct Enumerators : vocab::Tuple<Es...> {
    constexpr static auto name = EnumName::value;
    constexpr static auto description = Description::value;

    constexpr static auto is_fields() -> bool { return false; }
    constexpr static auto is_field() -> bool { return false; }
    constexpr static auto is_enumerator() -> bool { return false; }
    constexpr static auto is_enumerators() -> bool { return true; }
    constexpr static auto is_atom() -> bool { return false; }
    constexpr static auto is_integer() -> bool { return false; }
    constexpr static auto is_bool() -> bool { return false; }
    constexpr static auto is_string() -> bool { return false; }
    constexpr static auto is_list() -> bool { return false; }
    constexpr static auto is_tuple() -> bool { return false; }
    constexpr static auto is_map() -> bool { return false; }
    constexpr static auto is_variant() -> bool { return false; }
    constexpr static auto is_box() -> bool { return false; }
    constexpr static auto is_custom_atom() -> bool { return false; }
};

namespace detail {
    template<container::FixedString enum_name, container::FixedString description>
    struct MakeEnumeratorsFunction {
        template<concepts::Enumerator... Es>
        constexpr auto operator()(Es...) const {
            return Enumerators<meta::Constexpr<enum_name>, meta::Constexpr<description>, Es...> {};
        }
    };
}

template<container::FixedString enum_name, container::FixedString description = "">
constexpr inline auto make_enumerators = detail::MakeEnumeratorsFunction<enum_name, description> {};
}

namespace di {
using reflection::Enumerator;
using reflection::enumerator;
using reflection::Enumerators;
using reflection::make_enumerators;
}
