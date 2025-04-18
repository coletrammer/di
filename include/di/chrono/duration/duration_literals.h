#pragma once

#include "di/chrono/duration/duration.h"

namespace di::chrono {
using Picoseconds = Duration<i64, math::Pico>;
using Nanoseconds = Duration<i64, math::Nano>;
using Microseconds = Duration<i64, math::Micro>;
using Milliseconds = Duration<i64, math::Milli>;
using Seconds = Duration<i64>;
using Minutes = Duration<i64, math::Ratio<60>>;
using Hours = Duration<i64, math::Ratio<3600>>;
using Days = Duration<i32, math::Ratio<86400>>;
using Weeks = Duration<i32, math::Ratio<604800>>;
using Months = Duration<i32, math::Ratio<2629746>>;
using Years = Duration<i32, math::Ratio<31556952>>;
}

namespace di {
inline namespace literals {
    inline namespace chrono_duration_literals {
        constexpr auto operator""_h(unsigned long long value) {
            return chrono::Hours { value };
        }

        constexpr auto operator""_min(unsigned long long value) {
            return chrono::Minutes { value };
        }

        constexpr auto operator""_s(unsigned long long value) {
            return chrono::Seconds { value };
        }

        constexpr auto operator""_ms(unsigned long long value) {
            return chrono::Milliseconds { value };
        }

        constexpr auto operator""_us(unsigned long long value) {
            return chrono::Microseconds { value };
        }

        constexpr auto operator""_ns(unsigned long long value) {
            return chrono::Nanoseconds { value };
        }

        constexpr auto operator""_ps(unsigned long long value) {
            return chrono::Picoseconds { value };
        }
    }
}
}

namespace di {
using chrono::Days;
using chrono::Hours;
using chrono::Microseconds;
using chrono::Milliseconds;
using chrono::Minutes;
using chrono::Months;
using chrono::Nanoseconds;
using chrono::Picoseconds;
using chrono::Seconds;
using chrono::Years;
}

#if !defined(DI_NO_GLOBALS) && !defined(DI_NO_GLOBAL_CHRONO_LITERALS)
using namespace di::literals::chrono_duration_literals;
#endif
