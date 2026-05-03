#pragma once

#include "di/execution/concepts/receiver.h"
#include "di/execution/concepts/receiver_of.h"
#include "di/execution/concepts/sender.h"
#include "di/execution/concepts/sender_in.h"
#include "di/execution/interface/connect.h"
#include "di/execution/meta/completion_signatures_of.h"
#include "di/execution/meta/connect_result.h"
#include "di/execution/meta/env_of.h"
#include "di/meta/algorithm.h"
#include "di/meta/core.h"
#include "di/vocab/variant/visit.h"

namespace di::execution {
namespace variant_sender_ns {
    template<typename E, typename... Senders>
    using Sigs =
        meta::AsTemplate<CompletionSignatures,
                         meta::Unique<meta::Concat<meta::AsList<meta::CompletionSignaturesOf<Senders, E>>...>>>;

    template<typename R, typename... Senders>
    struct OpT {
        struct Type {
            di::Variant<meta::ConnectResult<Senders, R>...> op;

            template<typename S>
            explicit Type(S&& sender, R receiver)
                : op(in_place_type<meta::ConnectResult<S, R>>, connect(di::forward<S>(sender), di::move(receiver))) {}

            friend void tag_invoke(Tag<start>, Type& self) { di::visit(start, self.op); }
        };
    };

    template<concepts::Receiver R, typename... Senders>
    using Op = meta::Type<OpT<R, Senders...>>;

    template<typename... Senders>
    struct VariantSenderT {
        struct Type {
            using is_sender = void;

            template<typename S>
            requires(concepts::OneOf<meta::RemoveCVRef<S>, Senders...>)
            Type(S&& sender) : sender(in_place_type<meta::RemoveCVRef<S>>, di::forward<S>(sender)) {}

            di::Variant<Senders...> sender;

            template<concepts::RemoveCVRefSameAs<Type> Self, typename E>
            requires(concepts::ConstructibleFrom<Type, Self> && (concepts::SenderIn<Senders, E> && ...))
            friend auto tag_invoke(Tag<get_completion_signatures>, Self&&, E&&) -> Sigs<E, Senders...> {
                return {};
            }

            template<concepts::RemoveCVRefSameAs<Type> Self, concepts::Receiver Rec>
            requires(concepts::ConstructibleFrom<Type, Self> &&
                     concepts::ReceiverOf<Rec, Sigs<meta::EnvOf<Rec>, Senders...>>)
            friend auto tag_invoke(Tag<connect>, Self&& self, Rec out_r) {
                return di::visit(
                    [&](auto& s) -> Op<Rec, Senders...> {
                        return Op<Rec, Senders...>(di::forward_like<Self>(s), di::move(out_r));
                    },
                    self.sender);
            }
        };
    };
}

template<concepts::Sender... Senders>
using VariantSender = meta::Type<variant_sender_ns::VariantSenderT<Senders...>>;
}

namespace di {
using execution::VariantSender;
}
