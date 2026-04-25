#pragma once

#include "di/container/string/string_hybrid_storage.h"
#include "di/meta/language.h"

namespace di::container::string {
template<concepts::Encoding Enc, concepts::detail::MutableVector Vec = HybridStorage<meta::EncodingCodeUnit<Enc>>>
requires(concepts::SameAs<meta::detail::VectorValue<Vec>, meta::EncodingCodeUnit<Enc>>)
class StringImpl;
}
