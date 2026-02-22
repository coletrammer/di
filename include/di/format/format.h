#pragma once

#include "di/container/string/utf8_encoding.h"
#include "di/format/format_encoded.h"

namespace di::fmt {
constexpr inline auto format = format_encoded<container::string::Utf8Encoding>;
}

namespace di {
using fmt::format;
}
