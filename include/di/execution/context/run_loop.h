#pragma once

#include "di/container/intrusive/prelude.h"
#include "di/container/queue/prelude.h"
#include "di/execution/concepts/receiver.h"
#include "di/execution/concepts/receiver_of.h"
#include "di/execution/interface/connect.h"
#include "di/execution/interface/get_env.h"
#include "di/execution/interface/start.h"
#include "di/execution/query/get_completion_scheduler.h"
#include "di/function/tag_invoke.h"
#include "di/platform/prelude.h"
#include "di/sync/dumb_spinlock.h"
#include "di/sync/synchronized.h"
#include "di/sync/unique_lock.h"
#include "di/util/immovable.h"

namespace di::execution {
template<concepts::Lock Lock = DefaultLock>
class RunLoop {
private:
    struct OperationStateBase : IntrusiveForwardListNode<> {
    public:
        OperationStateBase(RunLoop* parent_) : parent(parent_) {}

        virtual void execute() = 0;

        RunLoop* parent { nullptr };
    };

    template<typename Receiver>
    struct OperationStateT {
        struct Type final : OperationStateBase {
        public:
            Type(RunLoop* parent, Receiver&& receiver) : OperationStateBase(parent), m_receiver(util::move(receiver)) {}

            void execute() override {
                if (get_stop_token(m_receiver).stop_requested()) {
                    set_stopped(util::move(m_receiver));
                } else {
                    set_value(util::move(m_receiver));
                }
            }

        private:
            void do_start() { this->parent->push_back(this); }

            friend void tag_invoke(types::Tag<start>, Type& self) { self.do_start(); }

            [[no_unique_address]] Receiver m_receiver;
        };
    };

    template<typename Receiver>
    using OperationState = meta::Type<OperationStateT<Receiver>>;

    struct Scheduler {
    private:
        struct Sender {
            using is_sender = void;

            using CompletionSignatures = types::CompletionSignatures<SetValue(), SetStopped()>;

            RunLoop* parent;

        private:
            template<concepts::ReceiverOf<CompletionSignatures> Receiver>
            friend auto tag_invoke(types::Tag<connect>, Sender self, Receiver receiver) {
                return OperationState<Receiver> { self.parent, util::move(receiver) };
            }

            struct Env {
                RunLoop* parent;

                template<typename CPO>
                constexpr friend auto tag_invoke(GetCompletionScheduler<CPO>, Env const& self) {
                    return Scheduler { self.parent };
                }
            };

            constexpr friend auto tag_invoke(types::Tag<get_env>, Sender const& self) { return Env { self.parent }; }
        };

    public:
        RunLoop* parent { nullptr };

    private:
        friend auto tag_invoke(types::Tag<schedule>, Scheduler const& self) { return Sender { self.parent }; }

        constexpr friend auto operator==(Scheduler const&, Scheduler const&) -> bool = default;
    };

    struct State {
        Queue<OperationStateBase, IntrusiveForwardList<OperationStateBase>> queue;
        bool stopped { false };
    };

public:
    RunLoop() = default;
    RunLoop(RunLoop&&) = delete;

    auto get_scheduler() -> Scheduler { return Scheduler { this }; }

    void run() {
        while (auto* operation = pop_front()) {
            operation->execute();
        }
    }

    void finish() {
        auto _ = UniqueLock(m_lock);
        m_state.stopped = true;
        m_cv.notify_one();
    }

private:
    auto pop_front() -> OperationStateBase* {
        auto guard = UniqueLock(m_lock);
        for (;;) {
            // NOTE: even if a stop is requested, we must continue first empty the queue
            //       before returning stopping execution. Otherwise, the receiver contract
            //       will be violated (operation state will be destroyed without completion
            //       ever occuring).
            if (!m_state.queue.empty()) {
                return util::addressof(*m_state.queue.pop());
            }
            if (m_state.stopped) {
                return nullptr;
            }
            m_cv.wait(guard);
        }
    }

    void push_back(OperationStateBase* operation) {
        auto _ = UniqueLock(m_lock);
        m_state.queue.push(*operation);
        m_cv.notify_one();
    }

    State m_state;
    DefaultLock m_lock;
    DefaultConditionVariable m_cv;
};
}

namespace di {
using execution::RunLoop;
}
