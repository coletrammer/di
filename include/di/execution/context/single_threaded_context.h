#pragma once

#include "di/execution/context/run_loop.h"
#include "di/platform/prelude.h"
#include "di/platform/thread.h"
#include "di/util/swap.h"
#include "di/vocab/error/result.h"
#include "di/vocab/pointer/box.h"

namespace di::execution {
class SingleThreadedContext {
public:
    static auto create() -> Result<SingleThreadedContext> {
        auto loop = make_box<RunLoop<>>();
        auto thread = Thread::create([&loop = *loop] {
            loop.run();
        });
        if (!thread) {
            return di::Unexpected(di::move(thread).error());
        }
        return Result<SingleThreadedContext>(in_place, di::move(thread).value(), di::move(loop));
    }

    explicit SingleThreadedContext(Thread thread, Box<RunLoop<>> loop)
        : m_loop(di::move(loop)), m_thread(di::move(thread)) {}

    SingleThreadedContext(SingleThreadedContext&&) = default;

    ~SingleThreadedContext() {
        if (m_loop) {
            m_loop->finish();
        }
    }

    auto get_scheduler() { return m_loop->get_scheduler(); }
    auto get_thread_id() const -> ThreadId { return m_thread.get_id(); }

private:
    Box<RunLoop<>> m_loop;
    Thread m_thread;
};
}

namespace di {
using execution::SingleThreadedContext;
}
