#pragma once

#include "di/execution/algorithm/then.h"
#include "di/execution/concepts/sender.h"
#include "di/execution/interface/run.h"
#include "di/execution/meta/connect_result.h"
#include "di/execution/query/is_always_lockstep_sequence.h"
#include "di/execution/receiver/receiver_adaptor.h"
#include "di/execution/receiver/set_stopped.h"
#include "di/execution/sequence/sequence_sender.h"
#include "di/function/container/function.h"
#include "di/function/tag_invoke.h"
#include "di/util/addressof.h"
#include "di/util/declval.h"
#include "di/util/defer_construct.h"
#include "di/util/immovable.h"

namespace di::execution {
namespace resource_helper_ns {
    template<typename Op>
    struct Recevier1T {
        struct Type {
            using is_receiver = void;

            Function<void()> complete;

            friend void tag_invoke(Tag<set_value>, Type&& self) { self.complete(); }
            friend void tag_invoke(Tag<set_stopped>, Type&& self) { self.complete(); }
        };
    };

    template<typename Op>
    using Receiver1 = meta::Type<Recevier1T<Op>>;

    template<typename Rec>
    struct Recevier2T {
        struct Type : ReceiverAdaptor<Type> {
        private:
            using Base = ReceiverAdaptor<Type>;
            friend Base;

        public:
            explicit Type(Rec* receiver) : m_receiver(receiver) {}

            auto base() const& -> Rec const& { return *m_receiver; }
            auto base() && -> Rec&& { return di::move(*m_receiver); }

        private:
            Rec* m_receiver;
        };
    };

    template<typename Rec>
    using Receiver2 = meta::Type<Recevier2T<Rec>>;

    template<typename Object, typename T, typename Open, typename Close, typename OutRec>
    struct OperationStateT {
        struct Type : Immovable {
            static auto next_sender(Open open, Optional<T>& token_out) {
                return then(di::move(open), [&token_out](T token) -> T {
                    token_out = token;
                    return token;
                });
            }

            using Rec1 = Receiver1<Type>;
            using Rec2 = Receiver2<OutRec>;
            using NextSender =
                di::meta::NextSenderOf<OutRec, decltype(next_sender(di::declval<Open>(), di::declval<Optional<T>&>()))>;
            using Op1 = meta::ConnectResult<NextSender, Rec1>;
            using Op2 = meta::ConnectResult<Close, Rec2>;

            Object* object { nullptr };
            [[no_unique_address]] OutRec out_r;
            DI_IMMOVABLE_NO_UNIQUE_ADDRESS di::Variant<di::Void, Op1, Op2> op {};
            Optional<T> token;

            explicit Type(Object* object, OutRec out_r) : object(object), out_r(di::move(out_r)) {}

            void finish_phase1() {
                if (!token.has_value()) {
                    set_stopped(di::move(out_r));
                    return;
                }
                auto& op = this->op.template emplace<2>(di::DeferConstruct([&] {
                    return connect(token.value().close(), Rec2(di::addressof(out_r)));
                }));
                start(op);
            }

            friend void tag_invoke(di::Tag<di::execution::start>, Type& self) {
                auto& op = self.op.template emplace<1>(di::DeferConstruct([&] {
                    return connect(set_next(self.out_r, next_sender(self.object->open(), self.token)), Rec1([&self] {
                                       self.finish_phase1();
                                   }));
                }));
                start(op);
            }
        };
    };

    template<typename Object, typename T, typename Open, typename Close, typename OutRec>
    using OperationState = meta::Type<OperationStateT<Object, T, Open, Close, OutRec>>;

    template<typename Object, typename T, typename Open, typename Close>
    struct RunSenderT {
        class Type {
        public:
            using is_sender = di::SequenceTag;

            Object* object { nullptr };

            template<concepts::RemoveCVRefSameAs<Type> Self, typename E>
            requires(concepts::SenderIn<Open, E>)
            friend auto tag_invoke(Tag<get_completion_signatures>, Self&&, E&&)
                -> meta::CompletionSignaturesOf<Open, E> {
                return {};
            }

            template<concepts::RemoveCVRefSameAs<Type> Self, concepts::Receiver Rec>
            requires(
                concepts::SubscriberOf<Rec, meta::CompletionSignaturesOf<meta::Like<Self, Type>, meta::EnvOf<Rec>>>)
            friend auto tag_invoke(Tag<subscribe>, Self&& self, Rec out_r) {
                return OperationState<Object, T, Open, Close, Rec>(self.object, di::move(out_r));
            }

            constexpr friend auto tag_invoke(di::Tag<di::execution::get_env>, Type const& self) {
                return di::execution::make_env(self.object->get_env(), with(is_always_lockstep_sequence, true),
                                               with(get_sequence_cardinality, di::c_<1ZU>));
            }
        };
    };

    template<typename Object, typename T, typename Open, typename Close>
    using RunSender = meta::Type<RunSenderT<Object, T, Open, Close>>;

    template<typename Object, typename T, typename Open, typename Close>
    struct HelperT {
        class Type : Immovable {
            constexpr friend auto tag_invoke(Tag<run>, Object& self) {
                return RunSender<Object, T, Open, Close>(di::addressof(self));
            }
        };
    };
}

template<typename Object, typename T, concepts::Sender Open, concepts::Sender Close>
using ResourceHelper = meta::Type<resource_helper_ns::HelperT<Object, T, Open, Close>>;
}
