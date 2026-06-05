#pragma once

#include "di/execution/concepts/receiver_of.h"
#include "di/execution/concepts/sender.h"
#include "di/execution/interface/connect.h"
#include "di/execution/interface/get_env.h"
#include "di/execution/interface/start.h"
#include "di/execution/meta/completion_signatures_of.h"
#include "di/execution/meta/connect_result.h"
#include "di/execution/meta/decayed_tuple.h"
#include "di/execution/meta/env_of.h"
#include "di/execution/query/get_completion_signatures.h"
#include "di/execution/query/make_env.h"
#include "di/execution/receiver/set_error.h"
#include "di/execution/receiver/set_stopped.h"
#include "di/execution/receiver/set_value.h"
#include "di/execution/types/completion_signuatures.h"
#include "di/function/curry_back.h"
#include "di/function/invoke.h"
#include "di/function/tag_invoke.h"
#include "di/meta/algorithm.h"
#include "di/meta/core.h"
#include "di/meta/language.h"
#include "di/meta/util.h"
#include "di/meta/vocab.h"
#include "di/platform/compiler.h"
#include "di/util/addressof.h"

namespace di::execution {
namespace transform_error_ns {
    template<typename Completions>
    using PassthroughSignatures = meta::Filter<meta::AsList<Completions>, meta::Not<meta::IsFunctionTo<SetError>>>;

    template<typename Completions>
    using ArgTypes = meta::Transform<meta::Filter<meta::AsList<Completions>, meta::IsFunctionTo<SetError>>,
                                     meta::Quote<meta::AsList>>;

    template<typename Fun, typename Completions>
    using ResultTypes =
        meta::Transform<ArgTypes<Completions>, meta::Uncurry<meta::BindFront<meta::Quote<meta::InvokeResult>, Fun>>>;

    template<typename T>
    struct ComplSigT : meta::TypeConstant<SetError(T)> {};

    template<>
    struct ComplSigT<void> : meta::TypeConstant<SetError()> {};

    template<typename T>
    using ComplSig = meta::Type<ComplSigT<T>>;

    template<typename T>
    struct InvokeSigsT : meta::TypeConstant<meta::List<ComplSig<T>>> {};

    template<typename T>
    using InvokeSigs = meta::Type<InvokeSigsT<T>>;

    template<typename Send, typename Env, typename Fun>
    using Sigs = meta::AsTemplate<
        CompletionSignatures,
        meta::Concat<PassthroughSignatures<meta::CompletionSignaturesOf<Send, MakeEnv<Env>>>,
                     meta::Join<meta::Transform<ResultTypes<Fun, meta::CompletionSignaturesOf<Send, MakeEnv<Env>>>,
                                                meta::Quote<InvokeSigs>>>>>;

    template<typename Fun, typename Rec>
    struct DataT {
        struct Type {
            DI_IMMOVABLE_NO_UNIQUE_ADDRESS Fun function;
            DI_IMMOVABLE_NO_UNIQUE_ADDRESS Rec receiver;
        };
    };

    template<typename Fun, typename Rec>
    using Data = meta::Type<DataT<meta::Decay<Fun>, Rec>>;

    template<typename Fun, typename Rec>
    struct ReceiverT {
        struct Type {
            using is_receiver = void;

            Data<Fun, Rec>* data;

            template<typename... Args>
            friend void tag_invoke(SetError, Type&& self, Args&&... args)
            requires(concepts::Invocable<Fun, Args...> && !concepts::Expected<meta::InvokeResult<Fun, Args...>> &&
                     concepts::ReceiverOf<Rec, CompletionSignatures<ComplSig<meta::InvokeResult<Fun, Args...>>>>)
            {
                using R = meta::InvokeResult<Fun, Args...>;

                if constexpr (concepts::LanguageVoid<R>) {
                    function::invoke(util::move(self.data->function), util::forward<Args>(args)...);
                    execution::set_error(util::move(self.data->receiver));
                } else {
                    execution::set_error(
                        util::move(self.data->receiver),
                        function::invoke(util::move(self.data->function), util::forward<Args>(args)...));
                }
            }

            template<concepts::OneOf<SetValue, SetStopped> Tg, typename... Args>
            friend void tag_invoke(Tg const tag, Type&& self, Args&&... args)
            requires(sizeof...(Args) < 2 && concepts::Invocable<Tg, Rec, Args...>)
            {
                tag(util::move(self.data->receiver), util::forward<Args>(args)...);
            }

            friend auto tag_invoke(Tag<get_env>, Type const& self) { return make_env(get_env(self.data->receiver)); }
        };
    };

    template<typename Fun, typename Rec>
    using Receiver = meta::Type<ReceiverT<meta::Decay<Fun>, Rec>>;

    template<typename Send, typename Fun, typename Rec>
    struct OperationStateT {
        struct Type : util::Immovable {
        private:
            using Rc = Receiver<Fun, Rec>;
            using Op = meta::ConnectResult<Send, Rc>;

        public:
            explicit Type(Send&& sender, Fun&& function, Rec receiver)
                : m_data(util::forward<Fun>(function), util::move(receiver))
                , m_operation(connect(util::forward<Send>(sender), Rc(util::addressof(m_data)))) {}

        private:
            friend void tag_invoke(Tag<start>, Type& self) { start(self.m_operation); }

            Data<Fun, Rec> m_data;
            Op m_operation;
        };
    };

    template<typename Send, typename Fun, typename Rec>
    using OperationState = meta::Type<OperationStateT<Send, Fun, Rec>>;

    template<typename Send, typename Fun>
    struct SenderT {
        struct Type {
            using is_sender = void;

            [[no_unique_address]] Send sender;
            [[no_unique_address]] Fun function;

            template<concepts::RemoveCVRefSameAs<Type> Self, typename E>
            requires(concepts::DecayConvertible<meta::Like<Self, Fun>>)
            friend auto tag_invoke(Tag<get_completion_signatures>, Self&&, E&&)
                -> Sigs<meta::Like<Self, Send>, E, Fun> {
                return {};
            }

            template<concepts::RemoveCVRefSameAs<Type> Self, typename Rec>
            requires(concepts::DecayConvertible<meta::Like<Self, Fun>> &&
                     concepts::ReceiverOf<Rec, Sigs<meta::Like<Self, Send>, meta::EnvOf<Rec>, Fun>>)
            friend auto tag_invoke(Tag<connect>, Self&& self, Rec receiver) {
                return OperationState<meta::Like<Self, Send>, meta::Like<Self, Fun>, Rec>(
                    di::forward_like<Self>(self.sender), di::forward_like<Self>(self.function), util::move(receiver));
            }

            friend auto tag_invoke(Tag<get_env>, Type const& self) { return make_env(get_env(self.sender)); }
        };
    };

    template<typename Send, typename Fun>
    using Sender = meta::Type<SenderT<meta::RemoveCVRef<Send>, meta::Decay<Fun>>>;

    struct Function {
        template<concepts::Sender Send, concepts::MovableValue Fun>
        auto operator()(Send&& sender, Fun&& function) const -> concepts::Sender auto {
            if constexpr (requires {
                              function::tag_invoke(*this, get_completion_scheduler<SetError>(get_env(sender)),
                                                   util::forward<Send>(sender), util::forward<Fun>(function));
                          }) {
                return function::tag_invoke(*this, get_completion_scheduler<SetError>(get_env(sender)),
                                            util::forward<Send>(sender), util::forward<Fun>(function));
            } else if constexpr (requires {
                                     function::tag_invoke(*this, util::forward<Send>(sender),
                                                          util::forward<Fun>(function));
                                 }) {
                return function::tag_invoke(*this, util::forward<Send>(sender), util::forward<Fun>(function));
            } else {
                return Sender<Send, Fun> { util::forward<Send>(sender), util::forward<Fun>(function) };
            }
        }
    };
}

/// @brief A sender that maps errors into different errors.
///
/// @param sender The sender to map.
/// @param function The function to map the value with.
///
/// @returns A sender that maps errors into different errors.
///
/// This function synchronously maps an error into another error.
///
/// @see then
constexpr inline auto transform_error = function::curry_back(transform_error_ns::Function {}, c_<2ZU>);
}
