#pragma once

namespace di::concepts {
namespace detail {
    template<typename T>
    constexpr inline bool const_helper = false;

    template<typename T>
    constexpr inline bool const_helper<T const> = true;
}

template<typename T>
concept Const = detail::const_helper<T>;
}
