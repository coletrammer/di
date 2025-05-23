#include "di/test/prelude.h"
#include "di/vocab/md/prelude.h"

namespace vocab_md {
constexpr static void extents() {
    static_assert(
        di::SameAs<di::Dextents<size_t, 5>, di::Extents<size_t, di::dynamic_extent, di::dynamic_extent,
                                                        di::dynamic_extent, di::dynamic_extent, di::dynamic_extent>>);

    auto a = di::Extents { 1, 2, 3, 4, 5 };
    ASSERT_EQ(a.extent(0), 1U);
    ASSERT_EQ(a.extent(1), 2U);
    ASSERT_EQ(a.extent(2), 3U);
    ASSERT_EQ(a.extent(3), 4U);
    ASSERT_EQ(a.extent(4), 5U);

    auto b = di::Extents<size_t, di::dynamic_extent, 3, di::dynamic_extent, 5, di::dynamic_extent> { 2, 4, 6 };
    ASSERT_EQ(b.extent(0), 2U);
    ASSERT_EQ(b.extent(1), 3U);
    ASSERT_EQ(b.extent(2), 4U);
    ASSERT_EQ(b.extent(3), 5U);
    ASSERT_EQ(b.extent(4), 6U);

    auto c = di::Extents<size_t, di::dynamic_extent, 3, di::dynamic_extent, 5, di::dynamic_extent> {
        di::to_array<size_t>({ 2, 3, 4, 5, 6 })
    };
    ASSERT_EQ(c.extent(0), 2U);
    ASSERT_EQ(c.extent(1), 3U);
    ASSERT_EQ(c.extent(2), 4U);
    ASSERT_EQ(c.extent(3), 5U);
    ASSERT_EQ(c.extent(4), 6U);

    ASSERT_EQ(b, c);
    ASSERT_NOT_EQ(a, c);

    ASSERT_EQ(c.fwd_prod_of_extents(0), 1U);
    ASSERT_EQ(c.fwd_prod_of_extents(2), 6U);
    ASSERT_EQ(c.rev_prod_of_extents(2), 30U);
    ASSERT_EQ(c.rev_prod_of_extents(4), 1U);
}

constexpr static void layout_left() {
    di::concepts::MDLayoutMapping auto mapping = di::LayoutLeft::Mapping<di::Extents<size_t, 4, 2, 3>> {};

    ASSERT_EQ(mapping.required_span_size(), 4U * 2U * 3U);

    ASSERT_EQ(mapping.stride(0), 1U);
    ASSERT_EQ(mapping.stride(1), 4U);
    ASSERT_EQ(mapping.stride(2), 8U);
    ASSERT_EQ(mapping(1, 1, 1), 1U + 4U + 8U);
}

constexpr static void layout_right() {
    di::concepts::MDLayoutMapping auto mapping = di::LayoutRight::Mapping<di::Extents<size_t, 4, 2, 3>> {};

    ASSERT_EQ(mapping.required_span_size(), 4U * 2U * 3U);

    ASSERT_EQ(mapping.stride(0), 6U);
    ASSERT_EQ(mapping.stride(1), 3U);
    ASSERT_EQ(mapping.stride(2), 1U);
    ASSERT_EQ(mapping(1, 1, 1), 6U + 3U + 1U);
}

constexpr static void default_accessor() {
    di::concepts::MDAccessor auto accessor = di::DefaultAccessor<int> {};

    int p[] = { 1, 2, 3, 4, 5 };
    ASSERT_EQ(accessor.access(p, 2), 3);
    ASSERT_EQ(accessor.offset(p, 2), p + 2);
}

constexpr static void mdspan() {
    auto storage = di::Array { 1, 2, 3, 4, 5, 6, 7, 8 };
    auto span = di::MDSpan { storage.data(), di::Extents<size_t, 2, 2, 2> {} };

    ASSERT_EQ((span(0, 0, 0)), 1);
    ASSERT_EQ((span(0, 0, 1)), 2);
    ASSERT_EQ((span(0, 1, 0)), 3);
    ASSERT_EQ((span(0, 1, 1)), 4);
    ASSERT_EQ((span(1, 0, 0)), 5);
    ASSERT_EQ((span(1, 0, 1)), 6);
    ASSERT_EQ((span(1, 1, 0)), 7);
    ASSERT_EQ((span(1, 1, 1)), 8);
    ASSERT_EQ(span.size(), 8U);
    ASSERT(!span.empty());

    auto lspan = di::MDSpan { storage.data(), di::LayoutLeft::Mapping(di::Extents<size_t, 2, 2, 2> {}) };

    ASSERT_EQ((lspan(0, 0, 0)), 1);
    ASSERT_EQ((lspan(0, 0, 1)), 5);
    ASSERT_EQ((lspan(0, 1, 0)), 3);
    ASSERT_EQ((lspan(0, 1, 1)), 7);
    ASSERT_EQ((lspan(1, 0, 0)), 2);
    ASSERT_EQ((lspan(1, 0, 1)), 6);
    ASSERT_EQ((lspan(1, 1, 0)), 4);
    ASSERT_EQ((lspan(1, 1, 1)), 8);
    ASSERT_EQ(lspan.size(), 8U);
    ASSERT(!lspan.empty());

    auto r = lspan.each() | di::to<di::Vector>();
    auto ex = di::Array { 1, 5, 3, 7, 2, 6, 4, 8 } | di::to<di::Vector>();
    ASSERT_EQ(r, ex);

    auto sspan = di::MDSpan { storage.data(), di::LayoutStride::Mapping(di::Extents<size_t, 2> {}, di::Array { 4 }) };
    auto r2 = sspan.each() | di::to<di::Vector>();
    auto ex2 = di::Array { 1, 5 } | di::to<di::Vector>();
    ASSERT_EQ(r2, ex2);
}

TESTC(vocab_md, extents)
TESTC(vocab_md, layout_left)
TESTC(vocab_md, layout_right)
TESTC(vocab_md, default_accessor)
TESTC(vocab_md, mdspan)
}
