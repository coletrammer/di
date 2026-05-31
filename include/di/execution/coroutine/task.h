#pragma once

#include "di/execution/algorithm/affine_on.h"
#include "di/execution/algorithm/just.h"
#include "di/execution/any/any_scheduler.h"
#include "di/execution/concepts/forwarding_query.h"
#include "di/execution/concepts/receiver_of.h"
#include "di/execution/concepts/scheduler.h"
#include "di/execution/context/inline_scheduler.h"
#include "di/execution/coroutine/as_awaitable.h"
#include "di/execution/interface/connect.h"
#include "di/execution/interface/schedule.h"
#include "di/execution/meta/env_of.h"
#include "di/execution/meta/stop_token_of.h"
#include "di/execution/query/get_scheduler.h"
#include "di/execution/receiver/set_error.h"
#include "di/execution/receiver/set_stopped.h"
#include "di/execution/receiver/set_value.h"
#include "di/execution/types/completion_signuatures.h"
#include "di/function/container/function.h"
#include "di/function/tag_invoke.h"
#include "di/meta/core.h"
#include "di/meta/operations.h"
#include "di/platform/prelude.h"
#include "di/sync/concepts/unstoppable_token.h"
#include "di/sync/stop_token/prelude.h"
#include "di/util/addressof.h"
#include "di/util/coroutine.h"
#include "di/util/declval.h"
#include "di/util/exchange.h"
#include "di/util/immovable.h"
#include "di/vocab/error/error.h"
#include "di/vocab/error/result.h"

namespace di::execution {
namespace task_ns {
    struct AllocFailed {};
    struct DefaultEnviornment {};

    template<concepts::Scheduler Sched>
    struct ChangeCoroutineScheduler {
        meta::RemoveCVRef<Sched> scheduler;
    };

    template<concepts::Scheduler Sched>
    ChangeCoroutineScheduler(Sched) -> ChangeCoroutineScheduler<Sched>;

    template<typename Env>
    struct SchedulerT : meta::TypeConstant<AnyScheduler<>> {};

    template<typename Env>
    requires(requires { typename Env::Scheduler; })
    struct SchedulerT<Env> : meta::TypeConstant<typename Env::Scheduler> {};

    template<typename Env>
    using Scheduler = meta::Type<SchedulerT<Env>>;

    template<typename Env>
    struct StopSourceT : meta::TypeConstant<InPlaceStopSource> {};

    template<typename Env>
    requires(requires { typename Env::StopSource; })
    struct StopSourceT<Env> : meta::TypeConstant<typename Env::StopSource> {};

    template<typename Env>
    using StopSource = meta::Type<StopSourceT<Env>>;

    template<typename T>
    struct ValueCompletionT : meta::TypeConstant<SetValue(T)> {};

    template<>
    struct ValueCompletionT<void> : meta::TypeConstant<SetValue()> {};

    template<typename Env>
    using ValueCompletion = meta::Type<ValueCompletionT<Env>>;

    template<typename T = void, typename Environment = DefaultEnviornment>
    class Task {
        struct Promise;

    public:
        using StopSource = task_ns::StopSource<Environment>;
        using StopToken = decltype(di::declval<StopSource>().get_stop_token());
        using Scheduler = task_ns::Scheduler<Environment>;
        using ValueCompletion = task_ns::ValueCompletion<T>;
        using CompletionSignatures = di::CompletionSignatures<ValueCompletion, SetError(di::Error), SetStopped()>;
        using Handle = CoroutineHandle<Promise>;

    private:
        struct StateBaseT {
            struct Type : Immovable {
                template<typename E>
                static auto make_base_env(E const& receiver_env) -> Environment {
                    if constexpr (concepts::ConstructibleFrom<Environment, E const&>) {
                        return Environment(receiver_env);
                    } else {
                        return Environment();
                    }
                }

                template<typename E>
                static auto make_scheduler(E const& receiver_env) {
                    if constexpr (requires { Scheduler(get_scheduler(receiver_env)); }) {
                        return Scheduler(get_scheduler(receiver_env));
                    } else if constexpr (concepts::DefaultConstructible<Scheduler>) {
                        return Scheduler();
                    } else {
                        static_assert(false, "di::Task requires a scheduler in the receiver environment");
                    }
                }

                template<typename E>
                static auto make_stop_token(E const& receiver_env) {
                    using S = meta::StopTokenOf<E>;
                    if constexpr (concepts::UnstoppableToken<S>) {
                        return StopToken();
                    } else {
                        return get_stop_token(receiver_env);
                    }
                }

                template<typename E>
                explicit Type(E const& receiver_env, Function<void()> complete)
                    : base_env(make_base_env(receiver_env))
                    , scheduler(make_scheduler(receiver_env))
                    , stop_token(make_stop_token(receiver_env))
                    , complete(di::move(complete)) {}

                Environment base_env;
                Scheduler scheduler;
                StopToken stop_token;
                Result<T> result { Unexpected(BasicError::NotEnoughMemory) };
                Function<void()> complete;
            };
        };

        using StateBase = meta::Type<StateBaseT>;

        struct Env {
            StateBase const* state { nullptr };

            friend auto tag_invoke(Tag<get_stop_token>, Env const& self) -> StopToken { return self.state->stop_token; }
            friend auto tag_invoke(Tag<get_scheduler>, Env const& self) -> Scheduler { return self.state->scheduler; }

            template<concepts::ForwardingQuery Q>
            requires(!concepts::OneOf<Q, Tag<get_stop_token>, Tag<get_scheduler>> &&
                     requires(Q tag, Environment const& e) { tag(e); })
            friend auto tag_invoke(Q tag, Env const& self) {
                tag(self.state->base_env);
            }
        };

        template<typename State>
        struct InitialReceiver {
            using is_receiver = void;

            State* state { nullptr };

            friend void tag_invoke(Tag<set_value>, InitialReceiver&& self) { self.state->scheduled(); }
            friend void tag_invoke(Tag<set_error>, InitialReceiver&& self, di::Error error) {
                self.state->result = Unexpected(di::move(error));
                self.state->complete();
            }
            friend void tag_invoke(Tag<set_stopped>, InitialReceiver&& self) {
                self.state->result = Unexpected(BasicError::OperationCanceled);
                self.state->complete();
            }
            friend auto tag_invoke(Tag<get_env>, InitialReceiver const& self) -> Env { return Env(self.state); }
        };

        struct PromiseVoid {
            StateBase* state { nullptr };

            void return_void() { state->result = {}; }
        };

        struct PromiseValue {
            StateBase* state { nullptr };

            template<concepts::ConvertibleTo<T> U>
            void return_value(U&& value) {
                state->result.emplace(di::forward<U>(value));
            }
        };

        using PromiseBase = meta::Conditional<concepts::LanguageVoid<T>, PromiseVoid, PromiseValue>;

        struct Promise : PromiseBase {
            struct FinalAwaiter {
                auto await_ready() noexcept -> bool { return false; }
                void await_suspend(Handle coroutine) noexcept {
                    Promise& current = coroutine.promise();
                    current.state->complete();
                }
                void await_resume() noexcept {}
            };

            template<typename U>
            struct ResultAwaiter {
                di::Result<U> maybe_value;

                auto await_ready() noexcept -> bool { return maybe_value.has_value(); }
                void await_suspend(Handle coroutine) noexcept {
                    Promise& current = coroutine.promise();
                    current.state->result = Unexpected(di::move(maybe_value).error());
                    current.state->complete();
                }
                auto await_resume() noexcept -> U { return di::move(maybe_value).value(); }
            };

            Promise() = default;

            auto operator new(usize size) noexcept -> void* { return ::operator new(size, std::nothrow); }
            void operator delete(void* ptr, usize size) noexcept { ::operator delete(ptr, size); }

            auto get_return_object() noexcept -> Task { return Task { CoroutineHandle<Promise>::from_promise(*this) }; }
            static auto get_return_object_on_allocation_failure() noexcept -> Task { return Task { AllocFailed {} }; }

            auto initial_suspend() noexcept -> SuspendAlways { return {}; }
            auto final_suspend() noexcept -> FinalAwaiter { return {}; }

            template<typename E>
            requires(concepts::ConstructibleFrom<Result<>, Unexpected<E>>)
            auto yield_value(Unexpected<E> error) {
                this->state->result = di::move(error);
                return FinalAwaiter {};
            }
            template<typename U>
            auto yield_value(Result<U> maybe_value) {
                return ResultAwaiter<U> { di::move(maybe_value) };
            }

            void unhandled_exception() { util::unreachable(); }
            auto unhandled_error(Error error) -> CoroutineHandle<> {
                this->state->result = Unexpected(di::move(error));
                this->state->complete();
                return noop_coroutine();
            }
            auto unhandled_stopped() -> CoroutineHandle<> {
                this->state->result = Unexpected(BasicError::OperationCanceled);
                this->state->complete();
                return noop_coroutine();
            }

            template<concepts::Sender Send>
            auto await_transform(Send&& sender) noexcept {
                if constexpr (concepts::SameAs<Scheduler, InlineScheduler>) {
                    return as_awaitable(di::forward<Send>(sender), *this);
                } else {
                    return as_awaitable(affine_on(di::forward<Send>(sender), this->state->scheduler), *this);
                }
            }

            template<typename Sched>
            auto await_transform(ChangeCoroutineScheduler<Sched> sched) noexcept {
                return this->await_transform(just(di::exchange(this->state->scheduler, Scheduler(sched.scheduler))));
            }

            friend auto tag_invoke(Tag<get_env>, Promise const& self) { return Env(self.state); }
        };

        template<typename E>
        struct WrapperT {
            struct Type {
                E own_env;
            };
        };

        template<typename E>
        using Wrapper = meta::Type<WrapperT<E>>;

        template<typename R>
        struct StateT {
            struct Type
                : Wrapper<meta::EnvOf<R>>
                , StateBase {
                auto receiver_env() const -> meta::EnvOf<R> const& { return this->own_env; }

                using Op = meta::ConnectResult<decltype(schedule(di::declval<Scheduler>())), InitialReceiver<Type>>;

                Type(Handle handle, R&& receiver)
                    : Wrapper<meta::EnvOf<R>>(get_env(receiver))
                    , StateBase(receiver_env(),
                                [this] {
                                    complete();
                                })
                    , handle(di::move(handle))
                    , receiver(di::move(receiver))
                    , initial_op(connect(schedule(this->scheduler), InitialReceiver<Type>(this))) {}

                ~Type() {
                    if (handle) {
                        handle.destroy();
                    }
                }

                friend void tag_invoke(Tag<start>, Type& self) {
                    // If we don't have a handle, presumably allocation failed.
                    if (!self.handle) {
                        self.complete();
                        return;
                    }

                    auto& promise = self.handle.promise();
                    promise.state = di::addressof(self);
                    start(self.initial_op);
                }

                void complete() {
                    if (this->result.has_value()) {
                        if constexpr (concepts::LanguageVoid<T>) {
                            set_value(di::move(receiver));
                        } else {
                            set_value(di::move(receiver), di::move(this->result.value()));
                        }
                    } else if (this->result == Unexpected(BasicError::OperationCanceled)) {
                        set_stopped(di::move(receiver));
                    } else {
                        set_error(di::move(receiver), di::move(this->result.error()));
                    }
                }

                void scheduled() { handle.resume(); }

                Handle handle;
                R receiver;
                DI_IMMOVABLE_NO_UNIQUE_ADDRESS Op initial_op;
            };
        };

        template<concepts::ReceiverOf<CompletionSignatures> R>
        using State = meta::Type<StateT<meta::RemoveCVRef<R>>>;

    public:
        using is_sender = void;
        using promise_type = Promise;

        Task(Task&& other) : m_handle(di::exchange(other.m_handle, {})) {}

        ~Task() {
            if (m_handle) {
                m_handle.destroy();
            }
        }

        template<concepts::ReceiverOf<CompletionSignatures> R>
        friend auto tag_invoke(Tag<connect>, Task&& self, R&& receiver) {
            return State<R>(di::exchange(self.m_handle, {}), di::forward<R>(receiver));
        }

    private:
        explicit Task(Handle handle) : m_handle(handle) {}
        explicit Task(AllocFailed) {}

        Handle m_handle;
    };
}

using task_ns::ChangeCoroutineScheduler;
using task_ns::Task;
}

namespace di {
using execution::ChangeCoroutineScheduler;
using execution::Task;
}
