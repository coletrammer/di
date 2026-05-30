#pragma once

#include "di/execution/concepts/receiver.h"
#include "di/execution/concepts/sender.h"
#include "di/execution/concepts/sender_to.h"
#include "di/execution/interface/connect.h"
#include "di/execution/interface/get_env.h"
#include "di/execution/meta/completion_signatures_of.h"
#include "di/execution/query/get_completion_signatures.h"
#include "di/function/curry.h"
#include "di/function/pipeable.h"
#include "di/function/tag_invoke.h"
#include "di/meta/constexpr.h"
#include "di/meta/core.h"
#include "di/meta/operations.h"
#include "di/meta/util.h"

namespace di::execution {
namespace with_sender_env_ns {
    template<typename Send, typename Env>
    struct SenderT {
        struct Type {
            using is_sender = void;

            [[no_unique_address]] Send m_sender;
            [[no_unique_address]] Env m_env;

        private:
            template<concepts::RemoveCVRefSameAs<Type> Self, typename En>
            friend auto tag_invoke(types::Tag<get_completion_signatures>, Self&&, En&&)
                -> meta::CompletionSignaturesOf<meta::Like<Self, Send>, En> {
                return {};
            }

            template<concepts::RemoveCVRefSameAs<Type> Self, concepts::Receiver Rec>
            requires(concepts::SenderTo<meta::Like<Self, Send>, Rec>)
            friend auto tag_invoke(types::Tag<connect>, Self&& self, Rec receiver) {
                return connect(util::forward_like<Self>(self.m_sender), util::move(receiver));
            }

            friend auto tag_invoke(types::Tag<get_env>, Type const& self) -> Env { return self.m_env; }
        };
    };

    template<concepts::Sender Send, typename Env>
    using Sender = meta::Type<SenderT<meta::RemoveCVRef<Send>, meta::Decay<Env>>>;

    struct Function {
        template<concepts::CopyConstructible Env, concepts::Sender Send>
        auto operator()(Env&& env, Send&& sender) const {
            return Sender<Send, Env> { util::forward<Send>(sender), util::forward<Env>(env) };
        }
    };
}

/// @brief Adapts a sender to have a certain environment
///
/// @param env The environment to assign to the sender
/// @param sender The sender to adapt.
///
/// @returns A sender whose get_env() query returns the provided env
///
/// @see with
/// @see make_env
constexpr inline auto with_sender_env = function::curry(with_sender_env_ns::Function {}, c_<2ZU>);
}
