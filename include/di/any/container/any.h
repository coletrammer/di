#pragma once

#include <di/any/concepts/prelude.h>
#include <di/any/meta/prelude.h>
#include <di/any/storage/prelude.h>
#include <di/any/types/prelude.h>
#include <di/any/vtable/prelude.h>
#include <di/concepts/derived_from.h>
#include <di/function/monad/monad_try.h>
#include <di/meta/like_expected.h>
#include <di/meta/remove_cvref.h>
#include <di/util/addressof.h>
#include <di/util/forward.h>
#include <di/util/initializer_list.h>
#include <di/util/swap.h>
#include <di/util/voidify.h>

namespace di::any {
namespace detail {
    template<typename E, typename S, typename Interface>
    struct MethodImpl {};

    template<typename E, typename S, typename Tag, typename R, concepts::RemoveCVRefSameAs<This> Self,
             typename... BArgs, typename... Rest>
    struct MethodImpl<E, S, meta::List<Method<Tag, R(Self, BArgs...)>, Rest...>>
        : MethodImpl<E, S, meta::List<Rest...>> {
    private:
        constexpr static auto vtable(auto&& self) -> decltype(auto) { return (self.m_vtable); }
        constexpr static auto storage(E& self) -> S& { return self; }
        constexpr static auto storage(E const& self) -> S const& { return self; }

        template<typename X>
        requires(concepts::DerivedFrom<meta::RemoveCVRef<X>, E>)
        constexpr friend R tag_invoke(Tag, X&& self_in, BArgs... bargs) {
            auto&& self = static_cast<meta::Like<Self, E>>(self_in);
            auto const& vtable = MethodImpl::vtable(self);
            auto* storage = util::voidify(util::addressof(MethodImpl::storage(self)));

            auto erased_fp = vtable[Method<Tag, R(Self, BArgs...)> {}];
            return erased_fp(storage, util::forward<BArgs>(bargs)...);
        }
    };
}

template<concepts::Interface UserInterface, concepts::AnyStorage Storage = HybridStorage<>,
         typename VTablePolicy = MaybeInlineVTable<3>>
class Any
    : private detail::MethodImpl<Any<UserInterface, Storage, VTablePolicy>, Storage,
                                 meta::MergeInterfaces<UserInterface, typename Storage::Interface>>
    , public Storage {
    template<typename, typename, typename>
    friend struct detail::MethodImpl;

public:
    using AnyStorage = Storage;
    using Interface = meta::MergeInterfaces<UserInterface, typename Storage::Interface>;
    using VTable = meta::Invoke<VTablePolicy, Interface>;

private:
    static_assert(concepts::VTableFor<VTable, Interface>,
                  "VTablePolicy policy returned an invalid VTable for the provided interface.");

    constexpr static auto storage_category = Storage::storage_category();

    constexpr static bool is_reference = storage_category == StorageCategory::Reference;
    constexpr static bool is_trivially_destructible = is_reference || storage_category == StorageCategory::Trivial;
    constexpr static bool is_trivially_copyable = is_reference || storage_category == StorageCategory::Trivial;
    constexpr static bool is_trivially_moveable = is_trivially_copyable;

    constexpr static bool is_moveable = !is_trivially_moveable && storage_category != StorageCategory::Immovable;
    constexpr static bool is_copyable = storage_category == StorageCategory::Copyable;

    template<typename T>
    using RemoveConstructQualifiers = meta::Conditional<is_reference, T, meta::RemoveCVRef<T>>;

public:
    template<typename U, concepts::Impl<Interface> VU = RemoveConstructQualifiers<U>>
    requires(!concepts::RemoveCVRefSameAs<U, Any> && !concepts::InstanceOf<meta::RemoveCVRef<U>, InPlaceType> &&
             concepts::AnyStorable<VU, Storage> && concepts::ConstructibleFrom<VU, U>)
    constexpr static auto create(U&& value) {
        if constexpr (!concepts::AnyStorableInfallibly<VU, Storage>) {
            using R = meta::LikeExpected<typename Storage::template CreationResult<VU>, Any>;

            auto result = R {};
            Storage::create(in_place_type<Any>, result, in_place_type<VU>, util::forward<U>(value));
            if (result) {
                result->m_vtable = VTable::template create_for<Storage, VU>();
            }
            return R(util::move(result));
        } else {
            return Any(in_place_type<VU>, util::forward<U>(value));
        }
    }

    template<typename T, typename... Args, concepts::Impl<Interface> VT = RemoveConstructQualifiers<T>>
    requires(concepts::AnyStorable<VT, Storage> && concepts::ConstructibleFrom<VT, Args...>)
    constexpr static auto create(InPlaceType<T>, Args&&... args) {
        if constexpr (!concepts::AnyStorableInfallibly<VT, Storage>) {
            using R = meta::LikeExpected<typename Storage::template CreationResult<VT>, Any>;

            auto result = R {};
            Storage::create(in_place_type<Any>, result, in_place_type<VT>, util::forward<Args>(args)...);
            if (result) {
                result->m_vtable = VTable::template create_for<Storage, VT>();
            }
            return result;
        } else {
            return Any(in_place_type<VT>, util::forward<Args>(args)...);
        }
    }

    template<typename T, typename U, typename... Args, concepts::Impl<Interface> VT = RemoveConstructQualifiers<T>>
    requires(concepts::AnyStorable<VT, Storage> && concepts::ConstructibleFrom<VT, std::initializer_list<U>&, Args...>)
    constexpr static auto create(InPlaceType<T>, std::initializer_list<U> list, Args&&... args) {
        if constexpr (!concepts::AnyStorableInfallibly<VT, Storage>) {
            using R = meta::LikeExpected<typename Storage::template CreationResult<VT>, Any>;

            auto result = R {};
            Storage::create(in_place_type<Any>, result, in_place_type<VT>, list, util::forward<Args>(args)...);
            if (result) {
                result->m_vtable = VTable::template create_for<Storage, VT>();
            }
            return result;
        } else {
            return Any(in_place_type<VT>, list, util::forward<Args>(args)...);
        }
    }

    Any()
    requires(!is_reference)
    = default;

    Any(Any const&)
    requires(is_trivially_copyable)
    = default;

    Any(Any const& other)
    requires(is_copyable)
        : Storage(), m_vtable(other.m_vtable) {
        Storage::copy_construct(other.m_vtable, this, util::addressof(other));
    }

    Any(Any&&)
    requires(is_trivially_moveable)
    = default;

    Any(Any&& other)
    requires(is_moveable)
        : m_vtable(other.m_vtable) {
        Storage::move_construct(other.m_vtable, this, util::addressof(other));
    }

    template<typename U, concepts::Impl<Interface> VU = RemoveConstructQualifiers<U>>
    requires(!concepts::DerivedFrom<meta::RemoveCVRef<U>, Any> &&
             !concepts::InstanceOf<meta::RemoveCVRef<U>, InPlaceType> && concepts::AnyStorableInfallibly<VU, Storage> &&
             concepts::ConstructibleFrom<VU, U>)
    constexpr Any(U&& value) {
        this->emplace(in_place_type<VU>, util::forward<U>(value));
    }

    template<typename T, typename... Args, concepts::Impl<Interface> VT = RemoveConstructQualifiers<T>>
    requires(concepts::AnyStorableInfallibly<VT, Storage> && concepts::ConstructibleFrom<VT, Args...>)
    constexpr Any(InPlaceType<T>, Args&&... args) {
        this->emplace(in_place_type<VT>, util::forward<Args>(args)...);
    }

    template<typename T, typename U, typename... Args, concepts::Impl<Interface> VT = RemoveConstructQualifiers<T>>
    requires(concepts::AnyStorableInfallibly<VT, Storage> &&
             concepts::ConstructibleFrom<VT, std::initializer_list<U>&, Args...>)
    constexpr Any(InPlaceType<T>, std::initializer_list<U> list, Args&&... args) {
        this->emplace(in_place_type<VT>, list, util::forward<Args>(args)...);
    }

    ~Any()
    requires(is_trivially_destructible)
    = default;

    ~Any()
    requires(!is_trivially_destructible)
    {
        Storage::destroy(m_vtable, this);
    }

    Any& operator=(Any const&)
    requires(is_trivially_copyable)
    = default;

    Any& operator=(Any&&)
    requires(is_trivially_moveable)
    = default;

    Any& operator=(Any const& other)
    requires(is_copyable)
    {
        Storage::copy_assign(m_vtable, this, other.m_vtable, util::addressof(other));
        return *this;
    }

    Any& operator=(Any&& other)
    requires(is_moveable)
    {
        Storage::move_assign(m_vtable, this, other.m_vtable, util::addressof(other));
        return *this;
    }

    template<typename U, concepts::Impl<Interface> VU = RemoveConstructQualifiers<U>>
    requires(!concepts::DerivedFrom<meta::RemoveCVRef<U>, Any> &&
             !concepts::InstanceOf<meta::RemoveCVRef<U>, InPlaceType> && concepts::AnyStorableInfallibly<VU, Storage> &&
             concepts::ConstructibleFrom<VU, U>)
    Any& operator=(U&& value) {
        if constexpr (!is_trivially_destructible) {
            Storage::destroy(m_vtable, this);
        }
        m_vtable = VTable::template create_for<Storage, VU>();
        Storage::init(this, in_place_type<VU>, util::forward<U>(value));
        return *this;
    }

    constexpr bool has_value() const
    requires(!is_reference)
    {
        return !m_vtable.empty();
    }

    template<typename U, concepts::Impl<Interface> VU = RemoveConstructQualifiers<U>>
    requires(!concepts::RemoveCVRefSameAs<U, Any> && !concepts::InstanceOf<meta::RemoveCVRef<U>, InPlaceType> &&
             concepts::AnyStorable<VU, Storage> && concepts::ConstructibleFrom<VU, U>)
    constexpr auto emplace(U&& value) {
        if constexpr (!is_reference) {
            reset();
        }
        if constexpr (!concepts::AnyStorableInfallibly<VU, Storage>) {
            return Storage::init(this, in_place_type<VU>, util::forward<U>(value)) % [&] {
                m_vtable = VTable::template create_for<Storage, VU>();
            };
        } else {
            Storage::init(this, in_place_type<VU>, util::forward<U>(value));
            m_vtable = VTable::template create_for<Storage, VU>();
        }
    }

    template<typename T, typename... Args, concepts::Impl<Interface> VT = RemoveConstructQualifiers<T>>
    requires(concepts::AnyStorable<VT, Storage> && concepts::ConstructibleFrom<VT, Args...>)
    constexpr auto emplace(InPlaceType<T>, Args&&... args) {
        if constexpr (!is_reference) {
            reset();
        }
        if constexpr (!concepts::AnyStorableInfallibly<VT, Storage>) {
            return Storage::init(this, in_place_type<VT>, util::forward<Args>(args)...) % [&] {
                m_vtable = VTable::template create_for<Storage, VT>();
            };
        } else {
            Storage::init(this, in_place_type<VT>, util::forward<Args>(args)...);
            m_vtable = VTable::template create_for<Storage, VT>();
        }
    }

    template<typename T, typename U, typename... Args, concepts::Impl<Interface> VT = RemoveConstructQualifiers<T>>
    requires(concepts::AnyStorable<VT, Storage> && concepts::ConstructibleFrom<VT, std::initializer_list<U>&, Args...>)
    constexpr auto emplace(InPlaceType<T>, std::initializer_list<U> list, Args&&... args) {
        if constexpr (!is_reference) {
            reset();
        }
        if constexpr (!concepts::AnyStorableInfallibly<VT, Storage>) {
            return Storage::init(this, in_place_type<VT>, list, util::forward<Args>(args)...) % [&] {
                m_vtable = VTable::template create_for<Storage, VT>();
            };
        } else {
            Storage::init(this, in_place_type<VT>, list, util::forward<Args>(args)...);
            m_vtable = VTable::template create_for<Storage, VT>();
        }
    }

    void reset()
    requires(!is_reference)
    {
        if constexpr (!is_trivially_destructible) {
            if (has_value()) {
                Storage::destroy(m_vtable, this);
            }
        }
        m_vtable.reset();
    }

private:
    VTable m_vtable {};
};
}
