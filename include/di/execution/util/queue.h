#pragma once

#include "di/container/queue/queue.h"
#include "di/execution/concepts/prelude.h"
#include "di/execution/concepts/receiver_of.h"
#include "di/execution/meta/prelude.h"
#include "di/execution/receiver/prelude.h"
#include "di/execution/receiver/set_stopped.h"
#include "di/execution/types/prelude.h"
#include "di/function/container/function.h"
#include "di/function/invoke.h"
#include "di/meta/core.h"
#include "di/meta/operations.h"
#include "di/sync/synchronized.h"
#include "di/util/immovable.h"

namespace di::execution::queue_ns {
template<typename Queue, typename Rec, typename... Values>
struct OperationStateT {
    struct Type : util::Immovable {
        explicit Type(Queue* queue, Rec receiver) : m_queue(queue), m_receiver(di::move(receiver)) {}

    private:
        using CallbackArg = di::Variant<SetStopped, Values...>;

        friend void tag_invoke(types::Tag<execution::start>, Type& self) {
            self.m_queue->register_callback(di::make_function<void(CallbackArg)>([&self](CallbackArg arg) {
                di::visit(
                    [&]<typename T>(T& value) {
                        if constexpr (concepts::SameAs<T, SetStopped>) {
                            set_stopped(di::move(self.m_receiver));
                        } else {
                            set_value(di::move(self.m_receiver), di::move(value));
                        }
                    },
                    arg);
            }));
        }

        Queue* m_queue { nullptr };
        DI_IMMOVABLE_NO_UNIQUE_ADDRESS Rec m_receiver;
    };
};

template<typename Queue, concepts::Receiver Rec, concepts::Movable... Values>
using OperationState = meta::Type<OperationStateT<Queue, Rec, Values...>>;

template<typename Queue, typename... Values>
struct SenderT {
    struct Type {
        using is_sender = void;

        Queue* queue { nullptr };

        explicit Type(Queue* queue) : queue(queue) {}

    private:
        template<concepts::DecaysTo<Type> Self,
                 concepts::ReceiverOf<CompletionSignatures<SetValue(Values)..., SetStopped()>> Rec>
        friend auto tag_invoke(types::Tag<connect>, Self&& self, Rec receiver) {
            return OperationState<Queue, Rec, Values...> { self.queue, util::move(receiver) };
        }

        template<concepts::DecaysTo<Type> Self, typename Env>
        friend auto tag_invoke(types::Tag<get_completion_signatures>, Self&&, Env&&)
            -> CompletionSignatures<SetValue(Values)..., SetStopped()> {
            return {};
        }
    };
};

template<typename Queue, concepts::Movable... Values>
using Sender = meta::Type<SenderT<Queue, Values...>>;

template<typename... Values>
struct QueueT {
    class Type : util::Immovable {
        using CallbackArg = di::Variant<SetStopped, Values...>;
        using Callback = di::Function<void(CallbackArg)>;

        struct State {
            di::Queue<CallbackArg> m_pending_values;
            di::Queue<Callback> m_pending_receivers;
        };

    public:
        Type() = default;

        ~Type() {
            for (auto& callback : m_state.get_assuming_no_concurrent_accesses().m_pending_receivers) {
                invoke(di::move(callback), CallbackArg(in_place_type<SetStopped>));
            }
        }

        template<concepts::OneOf<Values...> Value>
        void push(Value value) {
            auto maybe_callback = m_state.with_lock([&](State& state) -> Optional<Callback> {
                if (state.m_pending_receivers.empty()) {
                    state.m_pending_values.emplace(in_place_type<Value>, di::move(value));
                    return {};
                }
                return state.m_pending_receivers.pop();
            });
            if (maybe_callback) {
                invoke(di::move(maybe_callback).value(), CallbackArg(in_place_type<Value>, di::move(value)));
            }
        }

        auto pop() { return Sender<Type, Values...>(this); }

        void register_callback(Callback callback) {
            auto maybe_value = m_state.with_lock([&](State& state) -> Optional<CallbackArg> {
                if (auto value = state.m_pending_values.pop()) {
                    return di::move(value);
                }
                state.m_pending_receivers.push(di::move(callback));
                return {};
            });
            if (maybe_value) {
                invoke(di::move(callback), di::move(maybe_value).value());
            }
        }

    private:
        Synchronized<State> m_state;
    };
};
}

namespace di::execution {
template<concepts::Movable... Values>
requires(sizeof...(Values) > 0)
using Queue = meta::Type<queue_ns::QueueT<Values...>>;
}
