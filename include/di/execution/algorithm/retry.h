#pragma once

#include "di/execution/algorithm/repeat_effect_until.h"
#include "di/execution/concepts/single_sender.h"
#include "di/execution/meta/single_sender_value_type.h"
#include "di/execution/types/empty_env.h"

namespace di::execution {
namespace retry_ns {
    struct Function {
        template<concepts::SingleSender Send>
        constexpr static auto operator()(Send&& sender) {
            using V = meta::SingleSenderValueType<Send, EmptyEnv>;
            return let_value_with(
                [&](Result<V>& result, bool& done) {
                    return sender | upon_stopped([&] {
                               done = true;
                           }) |
                           then([&](V value) {
                               result = di::move(value);
                               done = true;
                           }) |
                           repeat_effect_until([&] {
                               return done;
                           }) |
                           then([&] -> Result<V> {
                               return di::move(result);
                           });
                },
                [] -> Result<V> {
                    return Unexpected(platform::BasicError::Cancelled);
                },
                [] {
                    return false;
                });
        }
    };
}
}
