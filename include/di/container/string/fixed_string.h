#pragma once

#include "di/types/size_t.h"
#include "di/types/strong_ordering.h"

namespace di::container {
namespace detail {
    struct FixedStringConcat;
}

template<types::size_t count>
class FixedString {
public:
    char m_data[count + 1];

    constexpr FixedString(char const (&data)[count + 1]) {
        for (types::size_t i = 0; i < count; i++) {
            m_data[i] = data[i];
        }
        m_data[count] = '\0';
    }

    constexpr auto data() const -> char const* { return m_data; }
    constexpr static auto size() { return count; }

    constexpr auto begin() const -> char const* { return m_data; }
    constexpr auto end() const -> char const* { return m_data + count; }

    template<types::size_t other_size>
    requires(count != other_size)
    constexpr friend auto operator==(FixedString const&, FixedString<other_size> const&) -> bool {
        return false;
    }

    auto operator==(FixedString const&) const -> bool = default;
    auto operator<=>(FixedString const&) const = default;

    friend struct detail::FixedStringConcat;
};

template<types::size_t size>
FixedString(char const (&)[size]) -> FixedString<size - 1>;

namespace detail {
    struct FixedStringConcat {
    private:
        template<size_t s0, size_t s1>
        constexpr static auto h(FixedString<s0> a, FixedString<s1> b) -> FixedString<s0 + s1> {
            auto result = FixedString<s0 + s1> { {} };
            for (size_t i = 0; i < s0; i++) {
                result.m_data[i] = a.m_data[i];
            }
            for (size_t i = 0; i < s1; i++) {
                result.m_data[s0 + i] = b.m_data[i];
            }
            return result;
        }

    public:
        template<size_t sz0, size_t... sz>
        constexpr static auto operator()(FixedString<sz0> s0, FixedString<sz>... strs)
            -> FixedString<sz0 + (sz + ... + 0zu)> {
            return h(s0, FixedStringConcat::operator()(strs...));
        }

        constexpr static auto operator()() -> FixedString<0> { return { {} }; }
    };
}

constexpr inline auto fixed_string_concat = detail::FixedStringConcat {};
}

namespace di {
using container::FixedString;
}
