#include "di/execution/algorithm/continues_on.h"
#include "di/execution/algorithm/ensure_started.h"
#include "di/execution/algorithm/just.h"
#include "di/execution/algorithm/let.h"
#include "di/execution/algorithm/starts_on.h"
#include "di/execution/algorithm/sync_wait.h"
#include "di/execution/algorithm/then.h"
#include "di/execution/algorithm/use_resources.h"
#include "di/execution/algorithm/when_all.h"
#include "di/execution/context/run_loop.h"
#include "di/execution/context/single_threaded_context.h"
#include "di/execution/scope/counting_scope.h"
#include "di/execution/scope/scope.h"
#include "di/execution/util/mutex.h"
#include "di/function/make_deferred.h"
#include "di/sync/scoped_lock.h"
#include "di/sync/unique_lock.h"
#include "di/test/prelude.h"
#include "di/util/compiler_barrier.h"

namespace execution_mutex {
static void basic() {
    auto mutex = di::execution::Mutex<i32> { 4 };

    auto sender = mutex.lock() | di::execution::then([](di::execution::MutexRef<i32> value) -> i32 {
                      return value.get();
                  });

    ASSERT_EQ(di::execution::sync_wait(di::move(sender)), 4);
}

static void cancelled() {
    auto mutex = di::execution::Mutex<i32> { 4 };

    auto sender = mutex.lock() | di::execution::then([](di::execution::MutexRef<i32> value) -> i32 {
                      return value.get();
                  });

    mutex.cancel();
    ASSERT_EQ(di::execution::sync_wait(di::move(sender)), di::Unexpected(di::BasicError::OperationCanceled));
}

static void stop() {
    auto mutex = di::execution::Mutex<i32> { 4 };
    auto t0 = *di::SingleThreadedContext::create();
    auto t1 = *di::SingleThreadedContext::create();
    auto t2 = *di::SingleThreadedContext::create();
    auto ss = 0;
    auto l = di::DefaultLock();
    auto cv = di::DefaultConditionVariable();
    auto cv2 = di::DefaultConditionVariable();
    auto l2 = di::DefaultLock();
    auto b = di::Atomic<bool> { false };

    auto s0 = di::execution::starts_on(t0.get_scheduler(), di::execution::just() | di::execution::let_value([&] {
                                                               auto g2 = di::UniqueLock(l2);
                                                               cv2.wait(g2, [&] {
                                                                   return b.load();
                                                               });
                                                               return di::stopped;
                                                           }));

    auto s1 = di::execution::starts_on(t1.get_scheduler(), mutex.lock()) | di::execution::then([&](auto) {
                  {
                      auto _ = di::UniqueLock(l2);
                      b.store(true);
                      cv2.notify_one();
                  }
                  auto g = di::UniqueLock(l);
                  cv.wait(g);
              }) |
              di::execution::let_stopped([&] {
                  ss += 1;
                  auto g = di::UniqueLock(l);
                  cv.notify_one();
                  return di::stopped;
              });

    auto s2 = di::execution::starts_on(t2.get_scheduler(), mutex.lock()) | di::execution::then([&](auto) {
                  {
                      auto _ = di::UniqueLock(l2);
                      b.store(true);
                      cv2.notify_one();
                  }
                  auto g = di::UniqueLock(l);
                  cv.wait(g);
              }) |
              di::execution::let_stopped([&] {
                  ss += 1;
                  auto g = di::UniqueLock(l);
                  cv.notify_one();
                  return di::stopped;
              });

    auto s = di::execution::when_all(di::move(s0), di::move(s1), di::move(s2));
    ASSERT_EQ(di::execution::sync_wait(di::move(s)), di::Unexpected(di::BasicError::OperationCanceled));
    ASSERT_EQ(ss, 1);
}

static void contention() {
    auto mutex = di::execution::Mutex<i32> { 0 };
    auto r1 = *di::SingleThreadedContext::create();
    auto r2 = *di::SingleThreadedContext::create();
    auto r3 = *di::SingleThreadedContext::create();
    auto r4 = *di::SingleThreadedContext::create();

    constexpr auto N = 10'000;
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

                di::execution::spawn(scope, di::execution::starts_on(scheduler, mutex.lock(scheduler)) |
                                                di::execution::then([&](auto value) {
                                                    auto v = *static_cast<i32 volatile*>(&value.get());
                                                    di::compiler_barrier();
                                                    *static_cast<i32 volatile*>(&value.get()) = v + 1;
                                                }));
            }
            return di::execution::just();
        },
        di::make_deferred<di::CountingScope<>>());
    ASSERT(di::execution::sync_wait(spawn_future_sender));
    ASSERT_EQ(mutex.get_assuming_no_concurrent_accesses(), N);
}

TEST(execution_mutex, basic)
TEST(execution_mutex, cancelled)
TEST(execution_mutex, stop)
TEST(execution_mutex, contention)
}
