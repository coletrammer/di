#include "di/any/storage/hybrid_storage.h"
#include "di/any/storage/prelude.h"
#include "di/any/storage/unique_storage.h"
#include "di/any/vtable/maybe_inline_vtable.h"
#include "di/container/algorithm/prelude.h"
#include "di/container/allocator/allocation_result.h"
#include "di/container/allocator/allocator.h"
#include "di/container/allocator/fail_allocator.h"
#include "di/container/view/prelude.h"
#include "di/execution/algorithm/bulk.h"
#include "di/execution/algorithm/ensure_started.h"
#include "di/execution/algorithm/execute.h"
#include "di/execution/algorithm/into_result.h"
#include "di/execution/algorithm/into_variant.h"
#include "di/execution/algorithm/just.h"
#include "di/execution/algorithm/just_from.h"
#include "di/execution/algorithm/just_or_error.h"
#include "di/execution/algorithm/just_void_or_stopped.h"
#include "di/execution/algorithm/let_value_with.h"
#include "di/execution/algorithm/on.h"
#include "di/execution/algorithm/prelude.h"
#include "di/execution/algorithm/split.h"
#include "di/execution/algorithm/start_detached.h"
#include "di/execution/algorithm/sync_wait.h"
#include "di/execution/algorithm/then.h"
#include "di/execution/algorithm/transfer_just.h"
#include "di/execution/algorithm/use_resources.h"
#include "di/execution/algorithm/when_all.h"
#include "di/execution/algorithm/with_env.h"
#include "di/execution/any/any_operation_state.h"
#include "di/execution/any/any_sender.h"
#include "di/execution/concepts/prelude.h"
#include "di/execution/concepts/receiver.h"
#include "di/execution/concepts/receiver_of.h"
#include "di/execution/context/inline_scheduler.h"
#include "di/execution/context/run_loop.h"
#include "di/execution/interface/run.h"
#include "di/execution/meta/completion_signatures_of.h"
#include "di/execution/meta/sends_stopped.h"
#include "di/execution/prelude.h"
#include "di/execution/query/get_allocator.h"
#include "di/execution/query/get_scheduler.h"
#include "di/execution/query/get_stop_token.h"
#include "di/execution/query/make_env.h"
#include "di/execution/receiver/prelude.h"
#include "di/execution/receiver/set_value.h"
#include "di/execution/scope/counting_scope.h"
#include "di/execution/scope/scope.h"
#include "di/execution/types/empty_env.h"
#include "di/execution/types/prelude.h"
#include "di/function/make_deferred.h"
#include "di/platform/prelude.h"
#include "di/sync/prelude.h"
#include "di/test/prelude.h"
#include "di/types/integers.h"
#include "di/util/prelude.h"
#include "di/vocab/error/prelude.h"
#include "di/vocab/expected/prelude.h"

namespace execution {
static void meta() {
    auto sender = di::execution::just(5);
    auto sender2 = di::execution::just(5, 10);
    auto sender3 = di::execution::just_error(5);
    auto sender4 = di::execution::just_stopped();

    static_assert(di::SameAs<di::meta::ValueTypesOf<decltype(sender)>, di::Variant<di::Tuple<int>>>);
    static_assert(di::concepts::SenderOf<decltype(sender), di::SetValue(int)>);
    static_assert(di::SameAs<di::meta::ValueTypesOf<decltype(sender2)>, di::Variant<di::Tuple<int, int>>>);
    static_assert(di::SameAs<di::meta::ValueTypesOf<decltype(sender3)>, di::meta::detail::EmptyVariant>);
    static_assert(di::SameAs<di::meta::ErrorTypesOf<decltype(sender3)>, di::Variant<int>>);
    static_assert(!di::meta::sends_stopped<decltype(sender3)>);
    static_assert(di::meta::sends_stopped<decltype(sender4)>);

    static_assert(di::SameAs<di::meta::Unique<di::meta::List<int, short, int, int>>, di::meta::List<int, short>>);

    using A = di::meta::MakeCompletionSignatures<
        decltype(sender), di::types::EmptyEnv,
        di::CompletionSignatures<di::SetValue(i64), di::SetStopped(), di::SetValue(i64)>>;
    static_assert(
        di::SameAs<A, di::types::CompletionSignatures<di::SetValue(i64), di::SetStopped(), di::SetValue(int)>>);

    static_assert(di::SameAs<di::meta::AsList<di::Tuple<>>, di::meta::List<>>);

    using S = decltype(di::declval<di::RunLoop<>>().get_scheduler());
    using SS = decltype(di::execution::schedule(di::declval<S const&>()));
    using SE = decltype(di::execution::get_env(di::declval<SS const&>()));
    static_assert(!di::SameAs<SE, di::EmptyEnv>);
    static_assert(di::Sender<SS>);
    static_assert(di::TagInvocable<di::Tag<di::execution::get_completion_scheduler<di::SetValue>>, SE const&>);
    static_assert(
        di::SameAs<S, decltype(di::execution::get_completion_scheduler<di::SetValue>(di::declval<SE const&>()))>);
    static_assert(di::Scheduler<S>);

    static_assert(di::concepts::IsAwaitable<di::Lazy<i32>>);
    static_assert(
        di::SameAs<di::CompletionSignatures<di::SetValue(i32), di::SetError(di::Error), di::SetStopped()>,
                   decltype(di::execution::get_completion_signatures(di::declval<di::Lazy<i32>>(), di::EmptyEnv {}))>);

    using R = di::execution::sync_wait_ns::Receiver<
        di::execution::sync_wait_ns::ResultType<di::execution::RunLoop<>, di::Lazy<i32>>, di::execution::RunLoop<>>;

    static_assert(
        di::SameAs<di::CompletionSignatures<di::SetValue(i32), di::SetError(di::Error), di::SetStopped()>,
                   di::meta::Type<di::execution::connect_awaitable_ns::CompletionSignatures<di::Lazy<i32>, R>>>);

    static_assert(di::SameAs<di::CompletionSignatures<di::SetValue(), di::SetError(di::Error), di::SetStopped()>,
                             di::meta::Type<di::execution::connect_awaitable_ns::CompletionSignatures<di::Lazy<>, R>>>);

    static_assert(di::concepts::Receiver<R>);
    static_assert(di::concepts::ReceiverOf<
                  R, di::CompletionSignatures<di::SetValue(i32), di::SetError(di::Error), di::SetStopped()>>);
    static_assert(
        di::concepts::ReceiverOf<R,
                                 di::CompletionSignatures<di::SetValue(), di::SetError(di::Error), di::SetStopped()>>);

    static_assert(
        di::SameAs<
            void, di::meta::ValueTypesOf<di::Lazy<>, di::types::EmptyEnv, di::meta::detail::SingleSenderValueTypeHelper,
                                         di::meta::detail::SingleSenderValueTypeHelper>>);
    static_assert(di::concepts::SingleSender<di::Lazy<i32>, di::EmptyEnv>);
    static_assert(di::concepts::SingleSender<di::Lazy<>, di::EmptyEnv>);

    namespace ex = di::execution;

    constexpr auto env = ex::make_env(di::empty_env, ex::with(ex::get_allocator, di::FallibleAllocator {}));
    static_assert(di::SameAs<di::FallibleAllocator, di::meta::AllocatorOf<decltype(env)>>);
}

static void sync_wait() {
    namespace ex = di::execution;

    ASSERT_EQ(ex::sync_wait(ex::just(42)), 42);

    ASSERT(ex::sync_wait(ex::get_scheduler()));
    ASSERT(ex::sync_wait(ex::get_stop_token()));

    ASSERT_EQ(ex::sync_wait(ex::just_error(di::BasicError::InvalidArgument)),
              di::Unexpected(di::BasicError::InvalidArgument));

    ASSERT_EQ(ex::sync_wait(ex::just_stopped()), di::Unexpected(di::BasicError::OperationCanceled));
}

static void lazy() {
    constexpr static auto t2 = [] -> di::Lazy<> {
        co_return {};
    };

    constexpr static auto task = [] -> di::Lazy<i32> {
        co_await t2();
        co_return 42;
    };

    ASSERT(di::sync_wait(t2()));
    ASSERT_EQ(di::sync_wait(task()), 42);
}

static void just() {
    ASSERT_EQ(di::sync_wait(di::execution::just(42)), 42);
    ASSERT_EQ(di::sync_wait(di::execution::just_error(di::BasicError::InvalidArgument)),
              di::Unexpected(di::BasicError::InvalidArgument));
    ASSERT_EQ(di::sync_wait(di::execution::just_stopped()), di::Unexpected(di::BasicError::OperationCanceled));

    ASSERT_EQ(di::sync_wait(di::execution::just_void_or_stopped(true)),
              di::Unexpected(di::BasicError::OperationCanceled));
    ASSERT(di::sync_wait(di::execution::just_void_or_stopped(false)));

    ASSERT_EQ(di::sync_wait(di::execution::just_or_error(di::Result<int>(42))), 42);
    ASSERT_EQ(
        di::sync_wait(di::execution::just_or_error(di::Result<int>(di::unexpect, di::BasicError::InvalidArgument))),
        di::Unexpected(di::BasicError::InvalidArgument));
}

static void coroutine() {
    namespace ex = di::execution;

    constexpr static auto task = [] -> di::Lazy<i32> {
        auto x = co_await ex::just(42);
        co_return x;
    };
    ASSERT_EQ(di::sync_wait(task()), 42);

    constexpr static auto error = [] -> di::Lazy<i32> {
        co_await ex::just_error(di::BasicError::InvalidArgument);
        co_return 56;
    };
    ASSERT_EQ(di::sync_wait(error()), di::Unexpected(di::BasicError::InvalidArgument));

    constexpr static auto error_direct = [] -> di::Lazy<i32> {
        co_return di::Unexpected(di::BasicError::InvalidArgument);
    };
    ASSERT_EQ(di::sync_wait(error_direct()), di::Unexpected(di::BasicError::InvalidArgument));

    constexpr static auto stopped = [] -> di::Lazy<i32> {
        co_await ex::just_stopped();
        co_return 56;
    };
    ASSERT_EQ(di::sync_wait(stopped()), di::Unexpected(di::BasicError::OperationCanceled));

    constexpr static auto stopped_direct = [] -> di::Lazy<i32> {
        co_return di::stopped;
    };
    ASSERT_EQ(di::sync_wait(stopped_direct()), di::Unexpected(di::BasicError::OperationCanceled));
}

static void then() {
    //! [then]
    namespace execution = di::execution;

    di::Sender auto work = execution::just(42) | execution::then([](int x) {
                               return x * 2;
                           });
    ASSERT_EQ(execution::sync_wait(work), 84);

    auto failure = execution::just() | execution::then([] {
                       return di::Result<int>(di::Unexpected(di::BasicError::InvalidArgument));
                   });
    ASSERT_EQ(execution::sync_wait(failure), di::Unexpected(di::BasicError::InvalidArgument));
    //! [then]

    di::Sender auto w2 = execution::just(42) | execution::then(di::into_void);
    ASSERT(execution::sync_wait(w2));

    auto map_error =
        execution::just_error(di::Error(di::BasicError::InvalidArgument)) | execution::upon_error([](auto) {
            return 42;
        });
    ASSERT_EQ(execution::sync_wait(di::move(map_error)), 42);

    auto map_stopped = execution::just_stopped() | execution::upon_stopped([]() {
                           return 42;
                       });
    ASSERT_EQ(execution::sync_wait(map_stopped), 42);
}

static void inline_scheduler() {
    namespace ex = di::execution;

    auto scheduler = di::InlineScheduler {};

    auto work = ex::schedule(scheduler) | ex::then([] {
                    return 42;
                });

    auto w2 = ex::on(scheduler, ex::just(42));

    ASSERT_EQ(ex::sync_wait(work), 42);
    ASSERT_EQ(ex::sync_wait(di::move(w2)), 42);

    auto v = ex::on(scheduler, ex::get_scheduler());
    ASSERT_EQ(*ex::sync_wait(v), scheduler);
}

static void let() {
    namespace ex = di::execution;

    auto w = ex::just(42) | ex::let_value(ex::just);
    ASSERT_EQ(ex::sync_wait(di::move(w)), 42);

    auto scheduler = di::InlineScheduler {};

    auto v =
        ex::schedule(scheduler) | ex::let_value(ex::get_scheduler) | ex::let_value([](di::Scheduler auto& scheduler) {
            return ex::schedule(scheduler) | ex::then([] {
                       return 43;
                   });
        });
    ASSERT_EQ(ex::sync_wait(v), 43);

    auto z = ex::just_from([] {
                 return di::Result<long>(44);
             }) |
             ex::let_value([](long) {
                 return ex::just(44);
             });

    ASSERT_EQ(ex::sync_wait(di::move(z)), 44);

    auto y = ex::schedule(scheduler) | ex::let_value([] {
                 return ex::just_error(di::BasicError::InvalidArgument);
             }) |
             ex::let_error([](auto) {
                 return ex::just(42);
             }) |
             ex::let_value([](auto) {
                 return ex::just_error(di::BasicError::InvalidArgument);
             }) |
             ex::let_error([](auto) {
                 return ex::just_stopped();
             }) |
             ex::let_stopped([] {
                 return ex::just(42);
             });
    ASSERT_EQ(ex::sync_wait(y), 42);

    //! [let_value_with]
    namespace execution = di::execution;

    struct Y : di::Immovable {
        explicit Y(int value) : y(value) {}

        int y;
    };

    auto a = execution::let_value_with(
        [](int& x, Y& y) {
            return execution::just(x + y.y);
        },
        [] {
            return 10;
        },
        di::make_deferred<Y>(32));

    ASSERT_EQ(execution::sync_wait(di::move(a)), 42);
    //! [let_value_with]
}

static void transfer() {
    namespace ex = di::execution;

    auto scheduler = di::InlineScheduler {};

    auto w = ex::transfer_just(scheduler, 42);

    ASSERT_EQ(ex::sync_wait(di::move(w)), 42);
}

static void as() {
    namespace ex = di::execution;

    auto w = ex::just_stopped() | ex::stopped_as_optional;
    ASSERT_EQ(ex::sync_wait(di::move(w)), di::nullopt);

    auto v = ex::just_stopped() | ex::stopped_as_error(42) | ex::let_error([](int x) {
                 ASSERT_EQ(x, 42);
                 return ex::just(x);
             });
    ASSERT_EQ(ex::sync_wait(di::move(v)), 42);
}

struct AsyncI32 {
    i32 value;

private:
    friend auto tag_invoke(di::Tag<di::execution::run>, AsyncI32& value) { return di::execution::just(di::ref(value)); }
};

static void use_resources() {
    //! [use_resources]
    namespace execution = di::execution;

    auto send = execution::use_resources(
        [](auto token) {
            return execution::just(token.get().value);
        },
        di::make_deferred<AsyncI32>(42));

    ASSERT_EQ(execution::sync_wait(di::move(send)), 42);
    //! [use_resources]
}

static void any_sender() {
    namespace ex = di::execution;

    using Sigs = di::CompletionSignatures<di::SetValue(int)>;
    using Sender = di::AnySender<Sigs>;
    using Receiver = Sender::Receiver;
    using OperationState = Sender::OperationState;

    static_assert(di::concepts::Receiver<Receiver>);
    static_assert(di::concepts::OperationState<OperationState>);
    static_assert(di::concepts::MoveConstructible<OperationState>);

    static_assert(di::concepts::ReceiverOf<Receiver, Sigs>);

    auto x = Sender(ex::just(42));

    ASSERT_EQ(ex::sync_wait(di::move(x)), 42);

    auto y = Sender(ex::just(42)) | ex::let_value([](int x) {
                 return ex::just(x);
             });

    ASSERT_EQ(ex::sync_wait(di::move(y)), 42);

    using Sender2 = di::AnySender<
        Sigs, void, di::any::HybridStorage<>, di::any::MaybeInlineVTable<3>,
        di::AnyOperationState<void, di::any::HybridStorage<di::StorageCategory::MoveOnly, 8 * sizeof(void*),
                                                           alignof(void*), di::FailAllocator>>>;

    auto z = ex::just(42) | ex::let_value([](int x) {
                 return ex::just(x);
             });
    auto yy = Sender2(di::move(z));

    ASSERT_EQ(ex::sync_wait(di::move(yy)), di::Unexpected(di::BasicError::NotEnoughMemory));

    using Sender3 = di::AnySender<
        Sigs, void,
        di::any::HybridStorage<di::StorageCategory::MoveOnly, 2 * sizeof(void*), alignof(void*), di::FailAllocator>>;

    auto dummy = 0;
    auto zz = ex::just(42) | ex::let_value([=](int x) {
                  return ex::just(x + dummy);
              }) |
              ex::let_value([=](int x) {
                  return ex::just(x + dummy);
              }) |
              ex::let_value([=](int x) {
                  return ex::just(x + dummy);
              }) |
              ex::let_value([=](int x) {
                  return ex::just(x + dummy);
              });
    auto yyy = Sender3(di::move(zz));

    ASSERT_EQ(ex::sync_wait(di::move(yyy)), di::Unexpected(di::BasicError::NotEnoughMemory));

    using Sender4 = di::AnySender<di::CompletionSignatures<di::SetValue(), di::SetValue(int), di::SetStopped()>>;

    ASSERT_EQ(ex::sync_wait_with_variant(Sender4(ex::just(52))), di::make_tuple(52));
    ASSERT_EQ(ex::sync_wait_with_variant(Sender4(ex::just())), di::make_tuple());
    ASSERT_EQ(ex::sync_wait_with_variant(Sender4(ex::just_stopped())),
              di::Unexpected(di::BasicError::OperationCanceled));
    ASSERT_EQ(ex::sync_wait_with_variant(Sender4(ex::just_error(di::BasicError::InvalidArgument))),
              di::Unexpected(di::BasicError::InvalidArgument));

    using Sender5 = di::AnySenderOf<int>;

    auto task = [] -> Sender5 {
        auto x = co_await ex::just(42);
        co_return x;
    };
    ASSERT_EQ(di::sync_wait(Sender5(task())), 42);

    auto task2 = [] -> Sender5 {
        auto x = co_await di::Result<int>(42);
        co_return x;
    };
    ASSERT_EQ(di::sync_wait(Sender5(task2())), 42);

    auto task3 = [] -> Sender5 {
        auto x = co_await di::Result<int>(di::Unexpected(di::BasicError::InvalidArgument));
        co_return x;
    };
    ASSERT_EQ(di::sync_wait(Sender5(task3())), di::Unexpected(di::BasicError::InvalidArgument));

    ASSERT_EQ(di::sync_wait(Sender5(di::Unexpected(di::BasicError::InvalidArgument))),
              di::Unexpected(di::BasicError::InvalidArgument));
    ASSERT_EQ(di::sync_wait(Sender5(di::stopped)), di::Unexpected(di::BasicError::OperationCanceled));

    ASSERT_EQ(di::sync_wait(di::Result<int>(42)), 42);
    ASSERT_EQ(di::sync_wait(di::Unexpected(di::BasicError::InvalidArgument)),
              di::Unexpected(di::BasicError::InvalidArgument));
}

static void into_result() {
    namespace ex = di::execution;

    using Sender = di::AnySenderOf<int>;

    enum class Result { Ok, Err, Stopped };

    auto process = [](Sender sender) {
        return di::move(sender) | ex::into_result | ex::then([](di::Result<int> result) {
                   if (result) {
                       return Result::Ok;
                   }
                   if (result.error() == di::BasicError::OperationCanceled) {
                       return Result::Stopped;
                   }
                   return Result::Err;
               });
    };

    ASSERT_EQ(ex::sync_wait(process(ex::just(42))), Result::Ok);
    ASSERT_EQ(ex::sync_wait(process(ex::just_error(di::BasicError::InvalidArgument))), Result::Err);
    ASSERT_EQ(ex::sync_wait(process(ex::just_stopped())), Result::Stopped);
}

static void when_all() {
    namespace ex = di::execution;

    using S1 = decltype(ex::when_all(ex::just(42), ex::just(43, 44L), ex::just()));
    static_assert(di::SameAs<di::CompletionSignatures<di::SetValue(int&&, int&&, long&&), di::SetStopped()>,
                             di::meta::CompletionSignaturesOf<S1>>);

    using S2 = decltype(ex::when_all(ex::just(42), ex::just(43, 44L), ex::just(),
                                     ex::just_error(di::Error(di::BasicError::InvalidArgument))));
    static_assert(di::SameAs<di::CompletionSignatures<di::SetError(di::Error&&), di::SetStopped()>,
                             di::meta::CompletionSignaturesOf<S2>>);

    using S3 =
        decltype(ex::when_all(ex::just(42), ex::just(43, 44L), ex::just(), ex::just_or_error(di::Result<int>(45))));
    static_assert(di::SameAs<di::CompletionSignatures<di::SetValue(int&&, int&&, long&&, int&&),
                                                      di::SetError(di::Error&&), di::SetStopped()>,
                             di::meta::CompletionSignaturesOf<S3>>);

    auto s1 = ex::when_all(ex::just(42), ex::just(43, 44L), ex::just());
    ASSERT_EQ(ex::sync_wait(s1), di::make_tuple(42, 43, 44L));

    auto s2 = ex::when_all(ex::just(42), ex::just(43, 44L), ex::just(),
                           ex::just_error(di::Error(di::BasicError::InvalidArgument)));
    ASSERT_EQ(ex::sync_wait(di::move(s2)), di::Unexpected(di::BasicError::InvalidArgument));

    auto s3 = ex::when_all(ex::just(42), ex::just(43, 44L), ex::just(), ex::just_or_error(di::Result<int>(45)));
    ASSERT_EQ(ex::sync_wait(di::move(s3)), di::make_tuple(42, 43, 44L, 45));

    auto executed = false;
    auto read_stop_token = ex::get_stop_token() | ex::then([&](di::concepts::StoppableToken auto stop_token) {
                               DI_ASSERT(stop_token.stop_requested());
                               executed = true;
                           });
    auto s4 = ex::when_all(ex::just_stopped(), read_stop_token);
    ASSERT_EQ(ex::sync_wait(di::move(s4)), di::Unexpected(di::BasicError::OperationCanceled));
    ASSERT(executed);

    executed = false;
    auto s5 = ex::when_all(ex::just_or_error(di::Result<int>(di::Unexpected(di::BasicError::InvalidArgument))),
                           read_stop_token);
    ASSERT_EQ(ex::sync_wait(di::move(s5)), di::Unexpected(di::BasicError::InvalidArgument));
    ASSERT(executed);
}

static void with_env() {
    //! [with_env]
    namespace execution = di::execution;

    auto stop_source = di::InPlaceStopSource {};
    auto env =
        execution::make_env(di::empty_env, execution::with(execution::get_stop_token, stop_source.get_stop_token()));
    auto send = execution::read(execution::get_stop_token) | execution::let_value([](auto stop_token) {
                    return execution::just_void_or_stopped(stop_token.stop_requested());
                });

    // The sender will run to completion if stop is not .
    ASSERT(execution::sync_wait(execution::with_env(env, send)));

    // After requesting stop, the sender will return cancelled.
    stop_source.request_stop();
    ASSERT(!execution::sync_wait(execution::with_env(env, send)));
    //! [with_env]
}

static void counting_scope() {
    namespace execution = di::execution;

    auto noop_sender = execution::use_resources(
        [](auto) {
            return execution::just();
        },
        di::make_deferred<di::CountingScope<>>());
    ASSERT(execution::sync_wait(noop_sender));

    //! [nest]
    auto nest_sender = execution::use_resources(
        [](auto scope) {
            return execution::when_all(execution::nest(scope, execution::just(11)),
                                       execution::nest(scope, execution::just(22)),
                                       execution::nest(scope, execution::just(33)));
        },
        di::make_deferred<di::CountingScope<>>());
    ASSERT_EQ(execution::sync_wait(nest_sender), di::make_tuple(11, 22, 33));
    //! [nest]

    //! [spawn]
    auto count = 0;
    auto spawn_sender = execution::use_resources(
        [&](auto scope) {
            di::for_each(di::range(10), [&](auto) {
                execution::spawn(scope, execution::just_from([&count] {
                                     ++count;
                                 }));
            });
            return execution::just();
        },
        di::make_deferred<di::CountingScope<>>());
    ASSERT(execution::sync_wait(spawn_sender));
    ASSERT_EQ(count, 10);
    //! [spawn]

    //! [spawn_future]
    auto spawn_future_sender = execution::use_resources(
        [&](auto scope) {
            return execution::get_scheduler() | execution::let_value([scope](auto scheduler) {
                       return execution::when_all(
                                  execution::spawn_future(scope, execution::on(scheduler, execution::just(11))),
                                  execution::spawn_future(scope, execution::on(scheduler, execution::just(22))),
                                  execution::spawn_future(scope, execution::on(scheduler, execution::just(33)))) |
                              execution::then([](auto... values) {
                                  return (values + ...);
                              });
                   });
        },
        di::make_deferred<di::CountingScope<>>());
    ASSERT_EQ(execution::sync_wait(spawn_future_sender), 66);
    //! [spawn_future]
}

static void start_detached() {
    namespace execution = di::execution;

    auto ran = false;
    execution::start_detached(execution::just_from([&] {
        ran = true;
    }));

    // NOTE: this is only valid since we know that the passed sender will complete inline.
    ASSERT(ran);

    ASSERT_EQ(execution::start_detached(execution::just(), di::fail_allocator),
              di::Unexpected(di::BasicError::NotEnoughMemory));

    ran = false;
    auto scheduler = execution::InlineScheduler {};
    execution::execute(scheduler, [&] {
        ran = true;
    });

    // NOTE: this is only valid since we know that the scheduler will complete inline.
    ASSERT(ran);
}

static void ensure_started() {
    namespace execution = di::execution;

    auto sender = execution::just(42);
    auto started = execution::ensure_started(sender);
    ASSERT_EQ(execution::sync_wait(di::move(started)), 42);

    ASSERT_EQ(execution::ensure_started(execution::just(), di::fail_allocator),
              di::Unexpected(di::BasicError::NotEnoughMemory));

    auto spawn_work = execution::get_scheduler() | execution::let_value([](auto scheduler) {
                          return execution::ensure_started(execution::on(scheduler, execution::just(42)));
                      });
    ASSERT_EQ(execution::sync_wait(spawn_work), 42);
}

static void bulk() {
    //! [bulk]
    namespace execution = di::execution;

    // NOTE: this could be a thread-pool scheduler instead.
    auto scheduler = execution::InlineScheduler {};

    constexpr usize count = 1000;
    constexpr usize tile_count = 10;

    struct CachelinePadded {
        alignas(128) usize value;
    };

    auto numbers = di::range(count) | di::to<di::Vector>();
    auto partials = di::repeat(CachelinePadded(0)) | di::take(tile_count) | di::to<di::Vector>();
    auto work = execution::transfer_just(scheduler, di::move(numbers), di::move(partials)) |
                execution::bulk(tile_count,
                                [](usize i, di::Vector<usize>& numbers, di::Vector<CachelinePadded>& partials) {
                                    auto start = i * (numbers.size() / tile_count);
                                    auto end = di::min((i + 1) * (numbers.size() / tile_count), numbers.size());
                                    partials[i].value = di::sum(numbers | di::drop(start) | di::take(end - start));
                                }) |
                execution::then([](di::Vector<usize>, di::Vector<CachelinePadded> partials) {
                    return di::sum(partials | di::transform(&CachelinePadded::value));
                });
    ASSERT_EQ(execution::sync_wait(di::move(work)), count * (count - 1) / 2);
    //! [bulk]

    auto error_sender = execution::just() | execution::bulk(10, [](auto) -> di::Result<void> {
                            return di::Unexpected(di::BasicError::InvalidArgument);
                        });
    ASSERT_EQ(execution::sync_wait(error_sender), di::Unexpected(di::BasicError::InvalidArgument));
}

static void split() {
    namespace execution = di::execution;

    struct NoncopyableI32 {
        explicit NoncopyableI32(i32 x_) : x(x_) {}

        NoncopyableI32(NoncopyableI32 const&) = delete;
        NoncopyableI32(NoncopyableI32&&) = default;

        auto operator=(NoncopyableI32 const&) -> NoncopyableI32& = delete;
        auto operator=(NoncopyableI32&&) -> NoncopyableI32& = default;

        i32 x;
    };

    auto multi_sender = execution::just(NoncopyableI32(42)) | execution::split;

    auto do_sender = [&] {
        return multi_sender | execution::then([](NoncopyableI32 const& x) {
                   return x.x;
               });
    };

    ASSERT_EQ(execution::sync_wait(do_sender()), 42);
    ASSERT_EQ(execution::sync_wait(do_sender()), 42);
    ASSERT_EQ(execution::sync_wait(do_sender()), 42);

    auto delayed_sender = execution::get_scheduler() | execution::let_value([](auto scheduler) {
                              auto work =
                                  execution::on(scheduler, execution::just(NoncopyableI32(42))) | execution::split;
                              return execution::when_all(work | execution::then([](NoncopyableI32 const& x) {
                                                             return x.x;
                                                         }),
                                                         work | execution::then([](NoncopyableI32 const& x) {
                                                             return x.x;
                                                         }),
                                                         work | execution::then([](NoncopyableI32 const& x) {
                                                             return x.x;
                                                         }));
                          });

    ASSERT_EQ(execution::sync_wait(delayed_sender), di::make_tuple(42, 42, 42));

    ASSERT_EQ(execution::just() | execution::split(di::fail_allocator),
              di::Unexpected(di::BasicError::NotEnoughMemory));
}

TEST(execution, meta)
TEST(execution, sync_wait)
TEST(execution, just)
TEST(execution, lazy)
TEST(execution, coroutine)
TEST(execution, then)
TEST(execution, inline_scheduler)
TEST(execution, let)
TEST(execution, transfer)
TEST(execution, as)
TEST(execution, use_resources)
TEST(execution, any_sender)
TEST(execution, into_result)
TEST(execution, when_all)
TEST(execution, with_env)
TEST(execution, counting_scope)
TEST(execution, start_detached)
TEST(execution, ensure_started)
TEST(execution, bulk)
TEST(execution, split)
}
