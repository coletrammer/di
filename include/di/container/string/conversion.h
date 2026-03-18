#pragma once

#include "di/container/string/string.h"
#include "di/container/string/string_view.h"
#include "di/container/string/utf8_stream_decoder.h"
#include "di/container/string/utf8_strict_stream_decoder.h"
#include "di/function/pipeable.h"
#include "di/vocab/span/as_bytes.h"

namespace di::container::string {
struct ToUtf8StringLossy : function::pipeline::EnablePipeline {
    static auto operator()(TransparentStringView view) -> String {
        auto utf8_decoder = utf8::Utf8StreamDecoder {};
        auto result = utf8_decoder.decode(as_bytes(view.span()));
        result.append(utf8_decoder.flush());
        return result;
    }
};

struct ToUtf8String : function::pipeline::EnablePipeline {
    static auto operator()(TransparentStringView view) -> Optional<String> {
        auto utf8_decoder = utf8::Utf8StrictStreamDecoder {};
        auto result = String {};
        for (auto c : view) {
            auto r = utf8_decoder.decode(byte(c));
            if (!r) {
                return {};
            }
            for (auto code_point : r.value()) {
                result.push_back(code_point);
            }
        }
        if (!utf8_decoder.flush()) {
            return {};
        }
        return result;
    }
};

struct ToTransparentString : function::pipeline::EnablePipeline {
    constexpr static auto operator()(StringView view) -> TransparentString {
        auto result = TransparentString {};
        for (auto byte : view.span()) {
            result.push_back(char(byte));
        }
        return result;
    }
};

constexpr inline auto to_utf8_string_lossy = ToUtf8StringLossy {};
constexpr inline auto to_utf8_string = ToUtf8String {};
constexpr inline auto to_transparent_string = ToTransparentString {};
}

namespace di {
using container::string::to_transparent_string;
using container::string::to_utf8_string;
using container::string::to_utf8_string_lossy;
}
