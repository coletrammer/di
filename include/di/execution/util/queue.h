#pragma once

#include "di/container/interface/erase.h"
#include "di/container/intrusive/forward_list.h"
#include "di/container/intrusive/forward_list_node.h"
#include "di/container/queue/queue.h"
#include "di/execution/algorithm/continues_on.h"
#include "di/execution/concepts/receiver_of.h"
#include "di/execution/concepts/scheduler.h"
#include "di/execution/meta/prelude.h"
#include "di/execution/receiver/prelude.h"
#include "di/execution/receiver/set_stopped.h"
#include "di/execution/types/completion_signuatures.h"
#include "di/execution/types/prelude.h"
#include "di/function/container/function.h"
#include "di/function/overload.h"
#include "di/meta/core.h"
#include "di/meta/operations.h"
#include "di/sync/atomic.h"
#include "di/sync/memory_order.h"
#include "di/sync/stop_token/prelude.h"
#include "di/sync/synchronized.h"
#include "di/util/addressof.h"
#include "di/util/exchange.h"
#include "di/util/guarded_reference.h"
#include "di/vocab/pointer/arc.h"

namespace di::execution::queue_ns {
template<typename... Values>
struct OperationStateBaseT {
    struct Type : IntrusiveForwardListNode<> {
        Function<void(Optional<Variant<Values...>>)> did_complete;
        bool cancelled { false };
    };
};

template<typename... Values>
using OperationStateBase = meta::Type<OperationStateBaseT<Values...>>;

template<typename... Values>
struct StateT {
    struct Type : IntrusiveRefCount<Type> {
        using Op = OperationStateBase<Values...>;
        using Val = Variant<Values...>;

        struct InnerState {
            IntrusiveForwardList<Op> waiters;
            di::Queue<Val> values;
            bool cancelled { false };
        };

        struct Cancelled {};
        struct Waiting {};

        template<typename V>
        void push(V&& v) {
            auto waiter = inner.with_lock([&](InnerState& state) -> di::Optional<Op&> {
                if (state.cancelled) {
                    return {};
                }
                if (state.waiters.empty()) {
                    state.values.emplace(di::forward<V>(v));
                    return {};
                }
                return state.waiters.pop_front();
            });
            if (waiter) {
                waiter.value().did_complete(Val(di::forward<V>(v)));
            }
        }

        auto register_op(Op& op) -> bool {
            auto result = inner.with_lock([&](InnerState& state) -> Variant<Cancelled, Waiting, Val> {
                if (state.cancelled || op.cancelled) {
                    return Cancelled {};
                }
                if (state.values.empty()) {
                    state.waiters.push_back(op);
                    return Waiting {};
                }
                return state.values.pop().value();
            });
            return di::visit(overload(
                                 [&](Cancelled) {
                                     op.did_complete(nullopt);
                                     return true;
                                 },
                                 [&](Waiting) {
                                     return false;
                                 },
                                 [&](Val& value) {
                                     op.did_complete(di::move(value));
                                     return true;
                                 }),
                             result);
        }

        void unregister(Op& op) {
            // When unregistering we could race with a call to push(). So we only complete
            // the operation if we were still in the list.
            auto result = inner.with_lock([&](InnerState& state) -> bool {
                op.cancelled = true;
                if (state.cancelled) {
                    return false;
                }
                auto count = erase_if(state.waiters, [&](Op const& value) -> bool {
                    return util::addressof(value) == util::addressof(op);
                });
                return count > 0;
            });
            if (result) {
                op.did_complete(nullopt);
            }
        }

        void cancel() {
            auto items = inner.with_lock([&](InnerState& state) -> IntrusiveForwardList<Op> {
                if (state.cancelled) {
                    return {};
                }
                state.cancelled = true;
                state.values = {};
                return di::exchange(state.waiters, IntrusiveForwardList<Op> {});
            });

            // We can't do normal iteration because completing the operation state may destroy it
            // immediately.
            for (auto it = items.begin(); it != items.end();) {
                auto& item = *it++;
                item.did_complete(nullopt);
            }
        }

        Synchronized<InnerState> inner;
    };
};

template<typename... Values>
using InnerState = meta::Type<StateT<Values...>>;

template<typename... Values>
struct StopCallbackFunctionT {
    struct Type {
        InnerState<Values...>* state { nullptr };
        OperationStateBase<Values...>* op { nullptr };

        void operator()() const noexcept { state->unregister(*op); }
    };
};

template<typename... Values>
using StopCallbackFunction = meta::Type<StopCallbackFunctionT<Values...>>;

template<typename Rec, typename... Values>
struct OperationStateT {
    struct Type : OperationStateBase<Values...> {
        using State = InnerState<Values...>;

        explicit Type(Arc<State> state, Rec receiver) : m_state(di::move(state)), m_receiver(di::move(receiver)) {
            this->did_complete = [this](Optional<Variant<Values...>> value) {
                m_stop_callback.reset();

                if (value) {
                    di::visit(
                        [&](auto& v) {
                            set_value(di::move(m_receiver), di::move(v));
                        },
                        value.value());
                } else {
                    set_stopped(di::move(m_receiver));
                }
            };
        }

    private:
        friend void tag_invoke(types::Tag<execution::start>, Type& self) {
            self.m_stop_callback.emplace(execution::get_stop_token(execution::get_env(self.m_receiver)),
                                         StopCallbackFunction<Values...> { self.m_state.get(), util::addressof(self) });

            if (self.m_state->register_op(self)) {
                return;
            }
        }

        Arc<State> m_state;
        DI_IMMOVABLE_NO_UNIQUE_ADDRESS Rec m_receiver;
        Optional<typename meta::StopTokenOf<meta::EnvOf<Rec>>::template CallbackType<StopCallbackFunction<Values...>>>
            m_stop_callback;
    };
};

template<concepts::Receiver Rec, typename... Values>
using OperationState = meta::Type<OperationStateT<Rec, Values...>>;

template<typename... Values>
struct SenderT {
    struct Type {
        using is_sender = void;

        using CompletionSignatures = di::CompletionSignatures<SetValue(Values)..., SetStopped()>;

        using State = InnerState<Values...>;

        Arc<State> state;

        explicit Type(Arc<State> state) : state(di::move(state)) {}

    private:
        template<concepts::DecaysTo<Type> Self, concepts::ReceiverOf<Type::CompletionSignatures> Rec>
        friend auto tag_invoke(types::Tag<connect>, Self&& self, Rec receiver) {
            return OperationState<Rec, Values...> { di::forward<Self>(self).state, util::move(receiver) };
        }
    };
};

template<typename... Values>
using Sender = meta::Type<SenderT<Values...>>;

template<typename... Values>
struct QueueT {
    class Type {
        using State = InnerState<Values...>;

        Type(Arc<State> state) : m_state(di::move(state)) {}

    public:
        static auto create() -> Type { return Type(di::make_arc<State>()); }

        ~Type() { m_state->cancel(); }

        /// @brief Cancel this queue. After this call push() can never be called again. Waiters will be stopped.
        void cancel() { m_state->cancel(); }

        /// @brief Push an item into the queue, possibly unblocking a waiter.
        template<concepts::OneOf<Values...> V>
        void push(V value) {
            m_state->push(di::move(value));
        }

        /// @brief Return a sender which completes with an item pushed into the queue.
        auto pop() { return Sender<Values...>(m_state); }

        /// @brief Return a sender which completes with an item pushed into the queue.
        ///
        /// The resulting sender will complete on the provided scheduler.
        auto pop(concepts::Scheduler auto scheduler) { return pop() | continues_on(scheduler); }

    private:
        Arc<State> m_state;
    };
};
}

namespace di::execution {
template<concepts::Movable... Values>
requires(sizeof...(Values) > 0)
using Queue = meta::Type<queue_ns::QueueT<Values...>>;
}
