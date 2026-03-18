#pragma once

#include "di/container/string/string.h"
#include "di/function/between_inclusive.h"

namespace di::container::string::utf8 {
class Utf8StreamDecoder {
public:
    constexpr static auto replacement_character = U'\uFFFD';

    // Decode the incoming byte stream as UTF-8. This may buffer code
    // units if the input doesn't end on a code point boundary.
    //
    // Invalid UTF-8 sequences are replaced with replacement characters.
    constexpr auto decode(Span<byte const> input) -> String {
        auto result = ""_s;
        for (auto byte : input) {
            decode_byte(result, byte);
        }
        return result;
    }

    // Flush any pending data. If there is any pending data, a single
    // replacement character will be output.
    constexpr auto flush() -> String {
        auto result = ""_s;
        if (m_pending_code_units > 0) {
            output_code_point(result, replacement_character);
        }
        return result;
    }

private:
    constexpr static auto default_lower_bound = u8(0x80);
    constexpr static auto default_upper_bound = u8(0xBF);

    constexpr void decode_byte(String& output, byte input) {
        if (m_pending_code_units == 0) {
            decode_first_byte(output, input);
            return;
        }

        auto input_u8 = di::to_integer<u8>(input);
        if (!di::between_inclusive(input_u8, m_lower_bound, m_upper_bound)) {
            output_code_point(output, replacement_character);
            decode_first_byte(output, input);
            return;
        }

        m_lower_bound = default_lower_bound;
        m_upper_bound = default_upper_bound;
        m_pending_code_point <<= 6;
        m_pending_code_point |= di::to_integer<u32>(input & byte(0b0011'1111));
        if (--m_pending_code_units == 0) {
            output_code_point(output, m_pending_code_point);
        }
    }

    constexpr void decode_first_byte(String& output, byte input) {
        // Valid ranges come from the Unicode core specification, table 3-7.
        //   https://www.unicode.org/versions/Unicode16.0.0/core-spec/chapter-3/#G27506
        auto input_u8 = di::to_integer<u8>(input);
        if (di::between_inclusive(input_u8, 0x00, 0x7F)) {
            output_code_point(output, di::to_integer<u32>(input));
        } else if (di::between_inclusive(input_u8, 0xC2, 0xDF)) {
            m_pending_code_units = 1;
            m_pending_code_point = di::to_integer<u32>(input & byte(0x1F));
        } else if (di::between_inclusive(input_u8, 0xE0, 0xEF)) {
            m_pending_code_units = 2;
            if (input == byte(0xE0)) {
                m_lower_bound = 0xA0;
            } else if (input == byte(0xED)) {
                m_upper_bound = 0x9F;
            }
            m_pending_code_point = di::to_integer<u32>(input & byte(0x0F));
        } else if (di::between_inclusive(input_u8, 0xF0, 0xF4)) {
            m_pending_code_units = 3;
            if (input == byte(0xF0)) {
                m_lower_bound = 0x90;
            } else if (input == byte(0xF4)) {
                m_upper_bound = 0x8F;
            }
            m_pending_code_point = di::to_integer<u32>(input & byte(0x07));
        } else {
            output_code_point(output, replacement_character);
        }
    }

    constexpr void output_code_point(String& output, c32 code_point) {
        output.push_back(code_point);

        // Reset state.
        *this = {};
    }

    u8 m_pending_code_units { 0 };
    u32 m_pending_code_point { 0 };
    u8 m_lower_bound { default_lower_bound };
    u8 m_upper_bound { default_upper_bound };
};
}

namespace di {
using container::string::utf8::Utf8StreamDecoder;
}
