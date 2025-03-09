#pragma once

#include "di/assert/assert_bool.h"
#include "di/meta/callable.h"
#include "di/sync/dumb_spinlock.h"
#include "di/sync/unique_lock.h"

namespace di::sync {
class DumbConditionVariable {
public:
    DumbConditionVariable() = default;

    DumbConditionVariable(DumbConditionVariable const&) = delete;
    auto operator=(DumbConditionVariable const&) -> DumbConditionVariable& = delete;

    void notify_one() {}
    void notify_all() {}

    void wait(UniqueLock<DumbSpinlock>& lock) {
        DI_ASSERT(lock.owns_lock());
        lock.unlock();
        cpu_relax();
        lock.lock();
    }

    template<di::concepts::CallableTo<bool> Pred>
    void wait(UniqueLock<DumbSpinlock>& lock, Pred predicate) {
        while (!predicate()) {
            wait(lock);
        }
    }
};
}

namespace di {
using sync::DumbConditionVariable;
}
