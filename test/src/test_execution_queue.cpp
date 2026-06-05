#include "di/execution/algorithm/ensure_started.h"
#include "di/execution/algorithm/starts_on.h"
#include "di/execution/algorithm/sync_wait.h"
#include "di/execution/algorithm/then.h"
#include "di/execution/algorithm/use_resources.h"
#include "di/execution/context/single_threaded_context.h"
#include "di/execution/scope/counting_scope.h"
#include "di/execution/scope/scope.h"
#include "di/execution/util/queue.h"
#include "di/function/make_deferred.h"
#include "di/test/prelude.h"

namespace execution_queue {
static void basic() {
    auto queue = di::execution::Queue<i32>::create();

    queue.push(4);

    ASSERT_EQ(di::execution::sync_wait(queue.pop()), 4);
}

static void wait() {
    auto queue = di::execution::Queue<i32>::create();

    auto send = di::execution::ensure_started(queue.pop());
    queue.push(4);
    ASSERT_EQ(di::execution::sync_wait(di::move(send)), 4);
}

static void contention() {
    auto queue = di::execution::Queue<i64>::create();
    auto r1 = *di::SingleThreadedContext::create();
    auto r2 = *di::SingleThreadedContext::create();
    auto r3 = *di::SingleThreadedContext::create();
    auto r4 = *di::SingleThreadedContext::create();

    constexpr auto N = 1'000'000_i64;
    auto sum = di::Atomic<i64> {};
    auto spawn_future_sender = di::execution::use_resources(
        [&](auto scope) {
            for (int i = 0; i < N; i++) {
                auto scheduler = [&] {
                    switch (i % 4) {
                        case 0:
                            return r1.get_scheduler();
                        case 1:
                            return r2.get_scheduler();
                        case 2:
                            return r3.get_scheduler();
                        default:
                            return r4.get_scheduler();
                    }
                }();

                di::execution::spawn(scope, di::execution::starts_on(scheduler, queue.pop(scheduler)) |
                                                di::execution::then([&](i64 value) {
                                                    sum.fetch_add(value);
                                                }));
            }
            return di::execution::just();
        },
        di::make_deferred<di::CountingScope<>>());
    auto push_sender = di::execution::just() | di::execution::then([&] {
                           for (auto i : di::range(N)) {
                               queue.push(i);
                           }
                       });
    ASSERT(di::execution::sync_wait(di::execution::when_all(di::move(spawn_future_sender), di::move(push_sender))));
    ASSERT_EQ(sum.load(), (N * (N - 1)) / 2);
}

TEST(execution_queue, basic)
TEST(execution_queue, wait)
TEST(execution_queue, contention)
}
