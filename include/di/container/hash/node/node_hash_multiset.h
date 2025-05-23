#pragma once

#include "di/container/allocator/allocator.h"
#include "di/container/allocator/fallible_allocator.h"
#include "di/container/allocator/infallible_allocator.h"
#include "di/container/associative/set_interface.h"
#include "di/container/concepts/prelude.h"
#include "di/container/hash/default_hasher.h"
#include "di/container/hash/node/hash_node.h"
#include "di/container/hash/node/node_hash_set.h"
#include "di/container/hash/node/owning_node_hash_table.h"
#include "di/container/intrusive/forward_list_forward_declaration.h"
#include "di/container/vector/mutable_vector.h"
#include "di/container/vector/vector.h"
#include "di/container/view/transform.h"
#include "di/function/compare.h"
#include "di/function/equal.h"
#include "di/platform/prelude.h"
#include "di/util/deduce_create.h"
#include "di/vocab/optional/prelude.h"

namespace di::container {
template<typename Value, typename Eq = function::Equal, concepts::Hasher Hasher = DefaultHasher,
         typename Buckets = container::Vector<
             IntrusiveForwardList<HashNode<detail::NodeHashSetTag<Value>>, detail::NodeHashSetTag<Value>>>,
         concepts::Allocator Alloc = platform::DefaultAllocator>
class NodeHashMultiSet
    : public OwningNodeHashTable<
          Value, Eq, Hasher, Buckets, detail::NodeHashSetTag<Value>, Alloc,
          SetInterface<NodeHashMultiSet<Value, Eq, Hasher, Buckets, Alloc>, Value,
                       HashNodeIterator<Value, detail::NodeHashSetTag<Value>>,
                       container::ConstIteratorImpl<HashNodeIterator<Value, detail::NodeHashSetTag<Value>>>,
                       detail::NodeHashTableValidForLookup<Value, Eq>::template Type, true>,
          true, false> {
private:
    using Base = OwningNodeHashTable<
        Value, Eq, Hasher, Buckets, detail::NodeHashSetTag<Value>, Alloc,
        SetInterface<NodeHashMultiSet<Value, Eq, Hasher, Buckets, Alloc>, Value,
                     HashNodeIterator<Value, detail::NodeHashSetTag<Value>>,
                     container::ConstIteratorImpl<HashNodeIterator<Value, detail::NodeHashSetTag<Value>>>,
                     detail::NodeHashTableValidForLookup<Value, Eq>::template Type, true>,
        true, false>;

public:
    using Base::Base;
};

template<concepts::InputContainer Con, typename T = meta::ContainerValue<Con>>
auto tag_invoke(types::Tag<util::deduce_create>, InPlaceTemplate<NodeHashMultiSet>, Con&&) -> NodeHashMultiSet<T>;

template<concepts::InputContainer Con, typename T = meta::ContainerValue<Con>, typename Eq>
auto tag_invoke(types::Tag<util::deduce_create>, InPlaceTemplate<NodeHashMultiSet>, Con&&, Eq)
    -> NodeHashMultiSet<T, Eq>;

template<concepts::InputContainer Con, typename T = meta::ContainerValue<Con>, typename Eq, typename Hasher>
auto tag_invoke(types::Tag<util::deduce_create>, InPlaceTemplate<NodeHashMultiSet>, Con&&, Eq, Hasher)
    -> NodeHashMultiSet<T, Eq, Hasher>;
}

namespace di {
using container::NodeHashMultiSet;
}
