#pragma once

#include "di/any/container/any.h"
#include "di/any/meta/merge_interfaces.h"
#include "di/any/storage/hybrid_storage.h"
#include "di/any/storage/storage_category.h"
#include "di/any/types/equal.h"
#include "di/any/types/method.h"
#include "di/any/vtable/maybe_inline_vtable.h"
#include "di/execution/algorithm/with_sender_env.h"
#include "di/execution/any/any_sender.h"
#include "di/execution/concepts/scheduler.h"
#include "di/execution/query/get_completion_scheduler.h"
#include "di/execution/query/make_env.h"
#include "di/execution/receiver/set_value.h"
#include "di/execution/types/completion_signuatures.h"
#include "di/execution/types/empty_env.h"
#include "di/meta/util.h"

namespace di::execution {
namespace any_scheduler_ns {
    using AnyScheduleSender = AnySender<CompletionSignatures<SetValue(), SetStopped()>>;

    using Interface = meta::List<Method<Tag<schedule>, AnyScheduleSender(di::This&)>, any::Equal>;

    template<typename Extra, typename Storage, typename VTablePolicy>
    struct AnySchedulerT {
        struct Type : Any<meta::MergeInterfaces<Interface, Extra>, Storage, VTablePolicy> {
            using Base = Any<meta::MergeInterfaces<Interface, Extra>, Storage, VTablePolicy>;

            friend auto tag_invoke(Tag<schedule>, Type& self) {
                return with_sender_env(make_env(empty_env, with(get_completion_scheduler<SetValue>, self)),
                                       schedule(static_cast<Base&>(self)));
            }
            friend auto tag_invoke(Tag<schedule>, Type&& self) { return tag_invoke(schedule, self); }
        };
    };

}

template<typename Extra = meta::List<>,
         typename Storage = any::InlineStorage<any::StorageCategory::Copyable, 2 * sizeof(void*), alignof(void*)>,
         typename VTablePolicy = any::MaybeInlineVTable<3>>
using AnyScheduler = meta::Type<any_scheduler_ns::AnySchedulerT<Extra, Storage, VTablePolicy>>;
}

namespace di {
using execution::AnyScheduler;
}
