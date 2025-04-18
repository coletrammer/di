#pragma once

#include "di/container/allocator/allocator.h"
#include "di/container/allocator/fallible_allocator.h"
#include "di/container/allocator/infallible_allocator.h"
#include "di/container/associative/map_interface.h"
#include "di/container/concepts/prelude.h"
#include "di/container/tree/rb_tree.h"
#include "di/container/tree/tree_map.h"
#include "di/container/view/transform.h"
#include "di/function/compare.h"
#include "di/platform/prelude.h"
#include "di/util/deduce_create.h"
#include "di/vocab/optional/prelude.h"

namespace di::container {
template<typename Key, typename Value, concepts::StrictWeakOrder<Key> Comp = function::Compare,
         concepts::Allocator Alloc = DefaultAllocator>
class TreeMultiMap
    : public OwningRBTree<
          Tuple<Key, Value>, detail::TreeMapCompAdapter<Comp, Key>, detail::TreeMapTag<Key, Value>, Alloc,
          MapInterface<
              TreeMultiMap<Key, Value, Comp, Alloc>, Tuple<Key, Value>, Key, Value,
              RBTreeIterator<Tuple<Key, Value>, detail::TreeMapTag<Key, Value>>,
              container::ConstIteratorImpl<RBTreeIterator<Tuple<Key, Value>, detail::TreeMapTag<Key, Value>>>,
              detail::RBTreeValidForLookup<Tuple<Key, Value>, detail::TreeMapCompAdapter<Comp, Key>>::template Type,
              true>,
          true> {
private:
    using Base = OwningRBTree<
        Tuple<Key, Value>, detail::TreeMapCompAdapter<Comp, Key>, detail::TreeMapTag<Key, Value>, Alloc,
        MapInterface<
            TreeMultiMap<Key, Value, Comp, Alloc>, Tuple<Key, Value>, Key, Value,
            RBTreeIterator<Tuple<Key, Value>, detail::TreeMapTag<Key, Value>>,
            container::ConstIteratorImpl<RBTreeIterator<Tuple<Key, Value>, detail::TreeMapTag<Key, Value>>>,
            detail::RBTreeValidForLookup<Tuple<Key, Value>, detail::TreeMapCompAdapter<Comp, Key>>::template Type,
            true>,
        true>;

public:
    TreeMultiMap() = default;

    TreeMultiMap(Comp const& comparator) : Base(detail::TreeMapCompAdapter<Comp, Key> { comparator }) {}
};

template<concepts::InputContainer Con, concepts::TupleLike T = meta::ContainerValue<Con>>
requires(meta::TupleSize<T> == 2)
auto tag_invoke(types::Tag<util::deduce_create>, InPlaceTemplate<TreeMultiMap>, Con&&)
    -> TreeMultiMap<meta::TupleElement<T, 0>, meta::TupleElement<T, 1>>;

template<concepts::InputContainer Con, concepts::TupleLike T = meta::ContainerValue<Con>,
         concepts::StrictWeakOrder<T> Comp>
requires(meta::TupleSize<T> == 2)
auto tag_invoke(types::Tag<util::deduce_create>, InPlaceTemplate<TreeMultiMap>, Con&&, Comp)
    -> TreeMultiMap<meta::TupleElement<T, 0>, meta::TupleElement<T, 1>, Comp>;
}

namespace di {
using container::TreeMultiMap;
}
