#pragma once

#include "di/bit/endian/endian.h"
#include "di/container/allocator/allocate_many.h"
#include "di/container/allocator/allocator.h"
#include "di/container/allocator/deallocate_many.h"
#include "di/container/vector/mutable_vector_interface.h"
#include "di/meta/core.h"
#include "di/meta/language.h"
#include "di/platform/prelude.h"
#include "di/types/integers.h"
#include "di/util/exchange.h"
#include "di/util/is_constant_evaluated.h"

namespace di::container::string {
template<concepts::Integral CodeUnit, concepts::Allocator Alloc = DefaultAllocator>
class HybridStorage : public MutableVectorInterface<HybridStorage<CodeUnit, Alloc>, CodeUnit> {
public:
    using Value = CodeUnit;
    using ConstValue = CodeUnit const;
    using Allocator = Alloc;

    constexpr static auto inline_capacity = sizeof(usize) + sizeof(usize) + sizeof(CodeUnit*) - sizeof(CodeUnit);

    constexpr HybridStorage() = default;
    constexpr HybridStorage(HybridStorage const&) = delete;
    constexpr HybridStorage(HybridStorage&& other) : m_allocator(util::move(other.m_allocator)) {
        if (other.is_small()) {
            m_string.small = di::exchange(other.m_string.small, {});
        } else {
            m_string.large = di::exchange(other.m_string.large, {});
        }
    }

    constexpr ~HybridStorage() { deallocate(); }

    constexpr auto operator=(HybridStorage const&) -> HybridStorage& = delete;
    constexpr auto operator=(HybridStorage&& other) -> HybridStorage& {
        if (this != &other) {
            deallocate();
            if (other.is_small()) {
                m_string.small = other.m_string.small;
            } else {
                m_string.large = di::exchange(other.m_string.large, {});
            }
        }
        return *this;
    }

    constexpr auto span() -> Span<Value> { return { data(), size() }; }
    constexpr auto span() const -> Span<ConstValue> { return { data(), size() }; }

    constexpr auto capacity() const -> usize {
        if (is_small()) {
            return inline_capacity;
        }
        return m_string.large.capacity & ~1;
    }
    constexpr auto max_size() const -> usize { return static_cast<usize>(-1); }

    constexpr auto reserve_from_nothing(usize n) -> meta::AllocatorResult<Alloc> {
        DI_ASSERT(capacity() == 0 || is_small());

        if (!di::is_constant_evaluated() && n <= inline_capacity) {
            if constexpr (concepts::SameAs<void, meta::AllocatorResult<Alloc>>) {
                return;
            } else {
                return {};
            }
        }
        if (n & 1) {
            n++;
        }
        return as_fallible(di::allocate_many<CodeUnit>(m_allocator, n)) % [&](AllocationResult<CodeUnit> result) {
            auto [data, new_capacity] = result;
            DI_ASSERT(new_capacity % 2 == 0);
            m_string.large.capacity = new_capacity | 1;
            m_string.large.data = data;
        } | try_infallible;
    }
    constexpr void assume_size(usize size) {
        if (is_small()) {
            DI_ASSERT(size <= inline_capacity);
            m_string.small.size_and_flag = (size << 1);
        } else {
            m_string.large.size = size;
        }
    }

    constexpr auto grow_capacity(usize min_capacity) const -> usize {
        if (min_capacity <= inline_capacity) {
            return inline_capacity;
        }

        constexpr auto smallest_allowed_capacity = 32zu;

        // The capacity must be even as the least significant bit is used as the flag to
        // use the inline storage (this is endian specific).
        if (min_capacity & 1) {
            min_capacity++;
        }
        if (capacity() >= min_capacity) {
            return min_capacity;
        }
        return di::max({ min_capacity, 2 * capacity(), smallest_allowed_capacity });
    }

    constexpr auto allocator() -> Alloc& { return m_allocator; }
    constexpr auto allocator() const -> Alloc const& { return m_allocator; }

    constexpr auto is_small() const -> bool {
        if consteval {
            return false;
        }
        return !(m_string.small.size_and_flag & 1);
    }

private:
    constexpr auto size() const -> usize {
        if (is_small()) {
            return m_string.small.size_and_flag >> 1;
        }
        return m_string.large.size;
    }

    constexpr auto data() -> CodeUnit* { return const_cast<CodeUnit*>(const_cast<HybridStorage const&>(*this).data()); }
    constexpr auto data() const -> CodeUnit const* {
        if (is_small()) {
            return m_string.small.data;
        }
        return m_string.large.data;
    }

    constexpr void deallocate() {
        if (is_small()) {
            return;
        }
        if consteval {
            if (this->capacity() == 0) {
                return;
            }
        }
        di::deallocate_many<CodeUnit>(m_allocator, m_string.large.data, m_string.large.capacity & ~1);
    }

    union {
        struct {
            usize capacity { 0 };
            usize size { 0 };
            CodeUnit* data { nullptr };
        } large {};
        struct {
            u8 size_and_flag { 0 };
            CodeUnit data[sizeof(usize) + sizeof(usize) + sizeof(CodeUnit*) - sizeof(CodeUnit)];
        } small;
    } m_string;
    [[no_unique_address]] Alloc m_allocator;
};

static_assert(Endian::Native == Endian::Little, "The SSO string storage requires little endian currently");
}
