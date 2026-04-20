#pragma once

#ifdef DI_CUSTOM_PLATFORM
#include DI_CUSTOM_PLATFORM
#elifdef DI_NO_USE_STD
#include "di/container/allocator/forward_declaration.h"
#include "di/platform/default_generic_domain.h"
#include "di/sync/dumb_spinlock.h"

namespace di::platform {
using ThreadId = int;

inline auto get_current_thread_id() -> ThreadId {
    return 0;
}

using DefaultLock = di::sync::DumbSpinlock;
using DefaultAllocator = container::InfallibleAllocator;
using DefaultFallibleAllocator = container::FallibleAllocator;
}
#else
#include <condition_variable>
#include <mutex>
#include <thread>

#include "di/assert/assert_bool.h"
#include "di/container/allocator/forward_declaration.h"
#include "di/platform/default_generic_domain.h"
#include "di/sync/unique_lock.h"
#include "di/vocab/error/result.h"
#include "di/vocab/expected/expected_forward_declaration.h"

namespace di::platform {
using ThreadId = std::thread::id;

inline auto get_current_thread_id() -> ThreadId {
    return std::this_thread::get_id();
}

using DefaultLock = std::mutex;

class DefaultConditionVariable {
public:
    DefaultConditionVariable() = default;

    void notify_one() { m_cv.notify_one(); }

    void notify_all() { m_cv.notify_all(); }

    void wait(di::UniqueLock<DefaultLock>& lock) {
        DI_ASSERT(lock.owns_lock());
        auto l = std::unique_lock(*lock.mutex(), std::adopt_lock);
        m_cv.wait(l);
        l.release();
    }

    template<di::concepts::CallableTo<bool> Pred>
    void wait(di::UniqueLock<DefaultLock>& lock, Pred predicate) {
        while (!predicate()) {
            wait(lock);
        }
    }

private:
    std::condition_variable m_cv;
};

using DefaultAllocator = container::InfallibleAllocator;
using DefaultFallibleAllocator = container::FallibleAllocator;
}
#endif
