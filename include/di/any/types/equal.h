#pragma once

#include "di/any/types/method.h"
#include "di/any/types/this.h"

namespace di::any {
struct EqualTag {};

using Equal = Method<EqualTag, bool(This const&, void*)>;
}
