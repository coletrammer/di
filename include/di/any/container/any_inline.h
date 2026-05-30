#pragma once

#include "di/any/container/any.h"

namespace di::any {
template<concepts::Interface Interface, StorageCategory storage_category = StorageCategory::MoveOnly,
         size_t inline_size = 2 * sizeof(void*), size_t inline_align = alignof(void*)>
using AnyInline = Any<Interface, InlineStorage<storage_category, inline_size, inline_align>>;
}

namespace di {
using any::AnyInline;
}
