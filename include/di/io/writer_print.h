#pragma once

#include "di/format/prelude.h"
#include "di/io/writer_format_context.h"

namespace di::io {
namespace detail {
    template<concepts::Encoding Enc>
    struct WriterPrintFunction {
        template<Impl<Writer> Writer, typename... Args>
        constexpr void operator()(Writer& writer, fmt::FormatStringImpl<Enc, Args...> format_string,
                                  Args&&... args) const {
            auto context = WriterFormatContext<Writer, Enc>(writer, format_string.encoding());
            (void) fmt::vformat_encoded_context<Enc>(
                format_string, fmt::make_format_args<WriterFormatContext<Writer, Enc>>(args...), context);
        }
    };
}

template<concepts::Encoding Enc>
constexpr inline auto writer_print = detail::WriterPrintFunction<Enc> {};
}

namespace di {
using io::writer_print;
}
