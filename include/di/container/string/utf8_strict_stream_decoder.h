#pragma once

#include "di/container/string/string.h"
#include "di/function/between_inclusive.h"

namespace di::container::string::utf8 {
class Utf8StrictStreamDecoder {
public:
    // Decode the incoming byte stream as UTF-8. This may buffer code
    // units if the input doesn't end on a code point boundary.
    constexpr auto decode(byte input) -> Result<Optional<c32>> { return decode_byte(input); }

    // Flush any pending data. If there is any pending data, the input is
    // invalid utf8.
    constexpr auto flush() -> Result<> {
        if (m_pending_code_units > 0) {
            return Unexpected(BasicError::InvalidArgument);
        }
        return {};
    }

private:
    constexpr static auto default_lower_bound = u8(0x80);
    constexpr static auto default_upper_bound = u8(0xBF);

    constexpr auto decode_byte(byte input) -> Result<Optional<c32>> {
        if (m_pending_code_units == 0) {
            return decode_first_byte(input);
        }

        auto input_u8 = di::to_integer<u8>(input);
        if (!di::between_inclusive(input_u8, m_lower_bound, m_upper_bound)) {
            return Unexpected(BasicError::InvalidArgument);
        }

        m_lower_bound = default_lower_bound;
        m_upper_bound = default_upper_bound;
        m_pending_code_point <<= 6;
        m_pending_code_point |= di::to_integer<u32>(input & byte(0b0011'1111));
        if (--m_pending_code_units == 0) {
            return output_code_point(m_pending_code_point);
        }
        return {};
    }

    constexpr auto decode_first_byte(byte input) -> Result<Optional<c32>> {
        // Valid ranges come from the Unicode core specification, table 3-7.
        //   https://www.unicode.org/versions/Unicode16.0.0/core-spec/chapter-3/#G27506
        auto input_u8 = di::to_integer<u8>(input);
        if (di::between_inclusive(input_u8, 0x00, 0x7F)) {
            return output_code_point(di::to_integer<u32>(input));
        }
        if (di::between_inclusive(input_u8, 0xC2, 0xDF)) {
            m_pending_code_units = 1;
            m_pending_code_point = di::to_integer<u32>(input & byte(0x1F));
            return {};
        }
        if (di::between_inclusive(input_u8, 0xE0, 0xEF)) {
            m_pending_code_units = 2;
            if (input == byte(0xE0)) {
                m_lower_bound = 0xA0;
            } else if (input == byte(0xED)) {
                m_upper_bound = 0x9F;
            }
            m_pending_code_point = di::to_integer<u32>(input & byte(0x0F));
            return {};
        }
        if (di::between_inclusive(input_u8, 0xF0, 0xF4)) {
            m_pending_code_units = 3;
            if (input == byte(0xF0)) {
                m_lower_bound = 0x90;
            } else if (input == byte(0xF4)) {
                m_upper_bound = 0x8F;
            }
            m_pending_code_point = di::to_integer<u32>(input & byte(0x07));
            return {};
        }
        return Unexpected(BasicError::InvalidArgument);
    }

    constexpr auto output_code_point(c32 code_point) -> c32 {
        // Reset state.
        *this = {};

        return code_point;
    }

    u8 m_pending_code_units { 0 };
    u32 m_pending_code_point { 0 };
    u8 m_lower_bound { default_lower_bound };
    u8 m_upper_bound { default_upper_bound };
};
}

namespace di {
using container::string::utf8::Utf8StrictStreamDecoder;
}
