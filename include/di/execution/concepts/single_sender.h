#pragma once

#include "di/execution/concepts/sender_in.h"
#include "di/execution/meta/single_sender_value_type.h"
#include "di/execution/types/empty_env.h"

namespace di::concepts {
template<typename Send, typename Env = types::EmptyEnv>
concept SingleSender = SenderIn<Send, Env> && requires { typename meta::SingleSenderValueType<Send, Env>; };
}
