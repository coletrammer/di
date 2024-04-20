#pragma once

namespace di::types {
using f32 = float;
using f64 = double;
}

namespace di {
using types::f32;
using types::f64;
}

#if !defined(DI_NO_GLOBALS) && !defined(DI_NO_GLOBAL_TYPES)
using di::f32;
using di::f64;
#endif
