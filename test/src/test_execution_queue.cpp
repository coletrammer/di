#include "di/execution/algorithm/ensure_started.h"
#include "di/execution/algorithm/sync_wait.h"
#include "di/execution/util/queue.h"
#include "di/test/prelude.h"

namespace execution_queue {
static void basic() {
    auto queue = di::execution::Queue<i32> {};

    queue.push(4);

    ASSERT_EQ(di::execution::sync_wait(queue.pop()), 4);
}

static void wait() {
    auto queue = di::execution::Queue<i32> {};

    auto send = di::execution::ensure_started(queue.pop());
    queue.push(4);
    ASSERT_EQ(di::execution::sync_wait(di::move(send)), 4);
}

TEST(execution_queue, basic)
TEST(execution_queue, wait)
}
