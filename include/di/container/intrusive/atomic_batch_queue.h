#pragma once

#include "di/container/intrusive/forward_list.h"
#include "di/container/intrusive/forward_list_node.h"
#include "di/sync/atomic.h"
#include "di/sync/atomic_ref.h"
#include "di/sync/memory_order.h"
#include "di/util/addressof.h"
#include "di/util/exchange.h"
#include "di/util/immovable.h"

namespace di::container {
template<typename T, typename Tag = DefaultIntrusiveForwardListTag, typename Self = Void>
class IntrusiveAtomicBatchQueue : util::Immovable {
private:
    using Node = IntrusiveForwardListNode<Tag>;
    using ConcreteNode = decltype(Tag::node_type(in_place_type<T>));
    using ConcreteSelf = meta::Conditional<SameAs<Void, Self>, IntrusiveAtomicBatchQueue, Self>;
    using ForwardList = IntrusiveForwardList<T, Tag>;

    constexpr auto down_cast_self() -> decltype(auto) {
        if constexpr (concepts::SameAs<Void, Self>) {
            return *this;
        } else {
            return static_cast<Self&>(*this);
        }
    }

public:
    constexpr auto empty() const -> bool { return AtomicRef(m_head.next).load(MemoryOrder::Relaxed) == nullptr; }

    constexpr void push(Node& node) {
        // NOTE: we push to the front of the queue as the makes the atomic operations trivial.
        auto* old_head = m_head.load(MemoryOrder::Relaxed);
        do {
            node.next = old_head;
        } while (!m_head.compare_exchange_weak(old_head, util::addressof(node), MemoryOrder::AcquireRelease));

        Tag::did_insert(down_cast_self(), static_cast<ConcreteNode&>(node));
    }

    constexpr auto batch_pop() -> ForwardList {
        // Clear the head pointer.
        auto* old_head = m_head.load(MemoryOrder::Relaxed);
        while (!m_head.compare_exchange_weak(old_head, nullptr, MemoryOrder::AcquireRelease)) {}

        // Now that we've fetched head atomic operations are no longer needed.
        // NOTE: since we push to the front of the linked list, to maintain LIFO semantics we need
        // to reverse the returned queue.
        auto result = ForwardList();
        for (auto* node = old_head; node;) {
            auto* next = node->next;
            result.push_front(*node);
            Tag::did_remove(down_cast_self(), static_cast<ConcreteNode&>(*node));
            node = next;
        }
        return result;
    }

private:
    di::Atomic<Node*> m_head { nullptr };
};
}

namespace di {
using container::IntrusiveAtomicBatchQueue;
}
