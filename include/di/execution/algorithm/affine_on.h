#pragma once

#include "di/execution/algorithm/continues_on.h"
#include "di/execution/concepts/prelude.h"
#include "di/execution/interface/connect.h"
#include "di/execution/interface/get_env.h"
#include "di/execution/meta/connect_result.h"
#include "di/execution/meta/env_of.h"
#include "di/execution/meta/prelude.h"
#include "di/execution/query/get_completion_scheduler.h"
#include "di/execution/receiver/prelude.h"
#include "di/execution/types/prelude.h"
#include "di/function/curry_back.h"
#include "di/function/tag_invoke.h"
#include "di/util/declval.h"
#include "di/util/defer_construct.h"

namespace di::execution {
namespace affine_on_ns {
    template<typename Send, typename Rec, typename Sched>
    struct OperationStateT {
        struct Type : di::Immovable {
        private:
            using Completions = meta::CompletionSignaturesOf<Send, meta::EnvOf<Rec>>;

            using AdaptedSender = decltype(continues_on(di::declval<Send>(), di::declval<Sched>()));
            using Op1 = meta::ConnectResult<Send, Rec>;
            using Op2 = meta::ConnectResult<AdaptedSender, Rec>;

        public:
            template<typename S>
            explicit Type(Sched scheduler, Rec receiver, S&& sender) {
                if constexpr (requires { scheduler == get_completion_scheduler<SetValue>(get_env(sender)); }) {
                    if (scheduler == get_completion_scheduler<SetValue>(get_env(sender))) {
                        m_op.template emplace<Op1>(DeferConstruct([&] {
                            return connect(di::forward<S>(sender), di::move(receiver));
                        }));
                        return;
                    }
                }
                m_op.template emplace<Op2>(DeferConstruct([&] {
                    return connect(continues_on(di::forward<S>(sender), scheduler), di::move(receiver));
                }));
            }

        private:
            friend void tag_invoke(types::Tag<execution::start>, Type& self) {
                di::visit(
                    [&](auto& op) {
                        if constexpr (!SameAs<decltype(op), Void&>) {
                            start(op);
                        }
                    },
                    self.m_op);
            }

            Variant<Void, Op1, Op2> m_op;
        };
    };

    template<concepts::Sender Send, concepts::Receiver Rec, concepts::Scheduler Sched>
    using OperationState = meta::Type<OperationStateT<Send, Rec, Sched>>;

    template<typename Send, typename Sched>
    struct SenderT {
        struct Type {
        public:
            using is_sender = void;

            [[no_unique_address]] Send sender;
            [[no_unique_address]] Sched scheduler;

        private:
            template<concepts::DecaysTo<Type> Self, typename Env>
            friend auto tag_invoke(types::Tag<get_completion_signatures>, Self&&, Env&&)
                -> meta::MakeCompletionSignatures<
                    meta::Like<Self, Send>, MakeEnv<Env>,
                    meta::MakeCompletionSignatures<meta::ScheduleResult<Sched>, MakeEnv<Env>, CompletionSignatures<>,
                                                   meta::TypeConstant<CompletionSignatures<>>::template Invoke>> {
                return {};
            }

            template<concepts::DecaysTo<Type> Self, typename Rec>
            requires(concepts::DecayConstructible<meta::Like<Self, Send>> &&
                     concepts::SenderTo<meta::Like<Self, Send>, Rec>)
            friend auto tag_invoke(types::Tag<connect>, Self&& self, Rec receiver) {
                return OperationState<Send, Rec, Sched> { di::forward_like<Self>(self.scheduler), di::move(receiver),
                                                          di::forward_like<Self>(self.sender) };
            }

            using SenderEnv = meta::EnvOf<Send>;

            struct Env {
                Sched scheduler;
                SenderEnv sender_env;

                friend auto tag_invoke(GetCompletionScheduler<SetValue>, Env const& self) { return self.scheduler; }

                template<concepts::ForwardingQuery Tag, typename... Args>
                requires(!concepts::OneOf<Tag, GetCompletionScheduler<SetValue>, GetCompletionScheduler<SetError>,
                                          GetCompletionScheduler<SetStopped>>)
                constexpr friend auto tag_invoke(Tag tag, Env const& self, Args&&... args)
                    -> meta::InvokeResult<Tag, SenderEnv const&, Args...> {
                    return tag(self.sender_env, di::forward<Args>(args)...);
                }
            };

            friend auto tag_invoke(types::Tag<get_env>, Type const& self) {
                return Env { self.scheduler, get_env(self.sender) };
            }
        };
    };

    template<concepts::Sender Send, concepts::Scheduler Sched>
    using Sender = meta::Type<SenderT<Send, Sched>>;

    struct Function : CurryBack<Function> {
        template<concepts::Sender Send, concepts::Scheduler Sched>
        static auto operator()(Send&& sender, Sched&& scheduler) -> concepts::Sender auto {
            if constexpr (concepts::TagInvocable<Function, Send, Sched>) {
                return function::tag_invoke(Function {}, di::forward<Send>(sender), di::forward<Sched>(scheduler));
            } else {
                return Sender<meta::Decay<Send>, meta::Decay<Sched>> { di::forward<Send>(sender),
                                                                       di::forward<Sched>(scheduler) };
            }
        }

        using CurryBack<Function>::operator();
        constexpr static auto max_arity = 2ZU;
    };
}

constexpr inline auto affine_on = affine_on_ns::Function {};
}
