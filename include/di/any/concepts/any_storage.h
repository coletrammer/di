#pragma once

#include "di/any/storage/storage_category.h"
#include "di/meta/constexpr.h"
#include "di/meta/core.h"
#include "di/meta/operations.h"
#include "di/types/prelude.h"

namespace di::concepts {
template<typename T>
concept AnyStorage = DefaultConstructible<T> && requires {
    typename T::Interface;

    // Must be a constant expression.
    { T::storage_category() } -> SameAs<any::StorageCategory>;
    typename meta::Constexpr<T::storage_category()>;

    // This must be evaluatable for all types, not just Void.
    { T::creation_is_fallible(in_place_type<Void>) } -> SameAs<bool>;

    // Must be a constant expression.
    typename meta::Constexpr<T::creation_is_fallible(in_place_type<Void>)>;

    // This must be evaluatable for all types, not just Void. The return type is the result of creating the storage for
    // a given type.
    typename T::template CreationResult<Void>;
};
}
