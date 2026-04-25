#pragma once

#include "di/container/interface/erase.h"
#include "di/container/intrusive/forward_list.h"
#include "di/container/intrusive/forward_list_node.h"
#include "di/execution/algorithm/continues_on.h"
#include "di/execution/concepts/receiver_of.h"
#include "di/execution/concepts/scheduler.h"
#include "di/execution/meta/prelude.h"
#include "di/execution/receiver/prelude.h"
#include "di/execution/receiver/set_stopped.h"
#include "di/execution/types/prelude.h"
#include "di/function/container/function.h"
#include "di/meta/core.h"
#include "di/meta/operations.h"
#include "di/sync/atomic.h"
#include "di/sync/memory_order.h"
#include "di/sync/synchronized.h"
#include "di/util/addressof.h"
#include "di/util/exchange.h"
#include "di/util/guarded_reference.h"
#include "di/vocab/pointer/arc.h"

namespace di::execution::mutex_ns {
template<typename T>
struct OperationStateBaseT {
    struct Type : IntrusiveForwardListNode<> {
        Function<void(Optional<T&>)> did_complete;
        bool cancelled { false };
    };
};

template<typename T>
using OperationStateBase = meta::Type<OperationStateBaseT<T>>;

template<typename T>
struct StateT {
    struct Type : IntrusiveRefCount<Type> {
        using Op = OperationStateBase<T>;

        struct InnerState {
            IntrusiveForwardList<Op> queue;
            bool cancelled { false };
            bool locked { false };
        };

        template<typename... Types>
        requires(concepts::ConstructibleFrom<T, Types...>)
        explicit Type(InPlace, Types&&... values) : value(di::forward<Types>(values)...) {}

        auto register_op(Op& op) -> bool {
            auto result = inner.with_lock([&](InnerState& state) -> Optional<bool> {
                if (state.cancelled || op.cancelled) {
                    return false;
                }
                if (state.locked) {
                    state.queue.push_back(op);
                    return {};
                }
                state.locked = true;
                return true;
            });
            if (result.has_value()) {
                if (result.value()) {
                    op.did_complete(value);
                } else {
                    op.did_complete(nullopt);
                }
                return true;
            }
            return false;
        }

        void unregister(Op& op) {
            // When unregistering we could race with a call to unlock(). So we only complete
            // the operation if we were still in the list.
            auto result = inner.with_lock([&](InnerState& state) -> bool {
                op.cancelled = true;
                if (state.cancelled) {
                    return false;
                }
                auto count = erase_if(state.queue, [&](Op const& value) -> bool {
                    return util::addressof(value) == util::addressof(op);
                });
                return count > 0;
            });
            if (result) {
                op.did_complete(nullopt);
            }
        }

        void unlock() {
            auto next = inner.with_lock([&](InnerState& state) -> Optional<Op&> {
                DI_ASSERT(state.locked);
                if (state.cancelled) {
                    return {};
                }
                if (state.queue.empty()) {
                    state.locked = false;
                    return {};
                }
                return state.queue.pop_front();
            });
            if (next) {
                next.value().did_complete(value);
            }
        }

        void cancel() {
            auto items = inner.with_lock([&](InnerState& state) -> IntrusiveForwardList<Op> {
                if (state.cancelled) {
                    return {};
                }
                state.cancelled = true;
                return di::exchange(state.queue, IntrusiveForwardList<Op> {});
            });

            // We can't do normal iteration because completing the operation state may destroy it
            // immediately.
            for (auto it = items.begin(); it != items.end();) {
                auto& item = *it++;
                item.did_complete(nullopt);
            }
        }

        T value;
        Synchronized<InnerState> inner;
    };
};

template<typename T>
using InnerState = meta::Type<StateT<T>>;

template<typename T>
struct GuardT {
    struct Type {
        Arc<InnerState<T>> state;

        Type() = default;
        Type(Arc<InnerState<T>> state) : state(di::move(state)) {}
        Type(Type&&) = default;
        auto operator=(Type&&) -> Type& = default;

        ~Type() {
            if (state) {
                state->unlock();
            }
        }
    };
};

template<typename T>
using Guard = meta::Type<GuardT<T>>;

template<typename T>
using Ref = util::GuardedReference<T, Guard<T>>;

template<typename T>
struct StopCallbackFunctionT {
    struct Type {
        InnerState<T>* state { nullptr };
        OperationStateBase<T>* op { nullptr };

        void operator()() const noexcept { state->unregister(*op); }
    };
};

template<typename T>
using StopCallbackFunction = meta::Type<StopCallbackFunctionT<T>>;

template<typename T, typename Rec>
struct OperationStateT {
    struct Type : OperationStateBase<T> {
        using State = InnerState<T>;

        explicit Type(Arc<State> state, Rec receiver) : m_state(di::move(state)), m_receiver(di::move(receiver)) {
            this->did_complete = [this](Optional<T&> value) {
                m_stop_callback.reset();

                if (value) {
                    set_value(di::move(m_receiver), Ref<T>(value.value(), Guard<T>(m_state)));
                } else {
                    set_stopped(di::move(m_receiver));
                }
            };
        }

    private:
        friend void tag_invoke(types::Tag<execution::start>, Type& self) {
            self.m_stop_callback.emplace(execution::get_stop_token(execution::get_env(self.m_receiver)),
                                         StopCallbackFunction<T> { self.m_state.get(), util::addressof(self) });

            if (self.m_state->register_op(self)) {
                return;
            }
        }

        Arc<State> m_state;
        DI_IMMOVABLE_NO_UNIQUE_ADDRESS Rec m_receiver;
        Optional<typename meta::StopTokenOf<meta::EnvOf<Rec>>::template CallbackType<StopCallbackFunction<T>>>
            m_stop_callback;
    };
};

template<typename T, concepts::Receiver Rec>
using OperationState = meta::Type<OperationStateT<T, Rec>>;

template<typename T>
struct SenderT {
    struct Type {
        using is_sender = void;

        using State = InnerState<T>;

        Arc<State> state;

        explicit Type(Arc<State> state) : state(di::move(state)) {}

    private:
        template<concepts::DecaysTo<Type> Self,
                 concepts::ReceiverOf<CompletionSignatures<SetValue(Ref<T>), SetStopped()>> Rec>
        friend auto tag_invoke(types::Tag<connect>, Self&& self, Rec receiver) {
            return OperationState<T, Rec> { di::forward<Self>(self).state, util::move(receiver) };
        }

        template<concepts::DecaysTo<Type> Self, typename Env>
        friend auto tag_invoke(types::Tag<get_completion_signatures>, Self&&, Env&&)
            -> CompletionSignatures<SetValue(Ref<T>), SetStopped()> {
            return {};
        }
    };
};

template<typename T>
using Sender = meta::Type<SenderT<T>>;

template<typename T>
struct MutexT {
    class Type {
        using State = InnerState<T>;

    public:
        Type()
        requires(concepts::DefaultConstructible<T>)
            : Type(in_place) {}

        template<typename U>
        requires(!concepts::SameAs<U, InPlace> && !concepts::RemoveCVRefSameAs<U, Type> &&
                 concepts::ConstructibleFrom<T, U>)
        constexpr explicit Type(U&& value) : Type(in_place, di::forward<U>(value)) {}

        template<typename... Args>
        requires(concepts::ConstructibleFrom<T, Args...>)
        constexpr explicit Type(InPlace, Args&&... args)
            : m_state(make_arc<State>(in_place, di::forward<Args>(args)...)) {}

        Type(Type const&) = default;
        Type(Type&&) = default;

        ~Type() { m_state->cancel(); }

        auto operator=(Type const&) -> Type& = default;
        auto operator=(Type&&) -> Type& = default;

        /// @brief Cancel this mutex. After this call lock() can never be called again.
        void cancel() { m_state->cancel(); }

        /// @brief Return a sender which completes with a locked reference to the underlying value.
        auto lock() -> Sender<T> { return Sender<T>(m_state); }

        /// @brief Return a sender which completes with a locked reference to the underlying value.
        ///
        /// The resulting sender will complete on the provided scheduler.
        auto lock(concepts::Scheduler auto scheduler) { return lock() | continues_on(scheduler); }

        auto get_assuming_no_concurrent_accesses() -> T& { return m_state->value; }
        auto get_const_assuming_no_concurrent_mutations() const -> T const& { return m_state->value; }

    private:
        Arc<State> m_state;
    };
};
}

namespace di::execution {
template<typename T>
using Mutex = meta::Type<mutex_ns::MutexT<T>>;

template<typename T>
using MutexRef = mutex_ns::Ref<T>;
}
