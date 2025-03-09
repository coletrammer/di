#pragma once

#include "di/platform/architecture.h"
#include "di/sync/atomic.h"
#include "di/sync/concepts/lock.h"

namespace di::sync {
inline void cpu_relax() {
#ifdef DI_X86_64
    asm volatile("pause" ::: "memory");
#elifdef DI_ARM64
    asm volatile("isb" ::: "memory");
#endif
}

class DumbSpinlock {
public:
    DumbSpinlock() = default;

    DumbSpinlock(DumbSpinlock const&) = delete;
    auto operator=(DumbSpinlock const&) -> DumbSpinlock& = delete;

    void lock() {
        for (;;) {
            if (try_lock()) {
                return;
            }
            while (m_state.load(MemoryOrder::Relaxed)) {
                cpu_relax();
            }
        }
    }
    auto try_lock() -> bool { return !m_state.exchange(true, MemoryOrder::Acquire); }
    void unlock() { m_state.store(false, MemoryOrder::Release); }

private:
    Atomic<bool> m_state { false };
};
}

namespace di {
using sync::cpu_relax;
using sync::DumbSpinlock;
}
