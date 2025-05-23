#pragma once

#include "di/container/algorithm/all_of.h"
#include "di/container/algorithm/compare.h"
#include "di/container/algorithm/equal.h"
#include "di/container/path/constant_path_interface.h"
#include "di/container/path/path_iterator.h"
#include "di/container/string/string_view.h"
#include "di/util/to_owned.h"
#include "di/vocab/optional/prelude.h"
#include "di/vocab/tuple/prelude.h"

namespace di::container {
template<concepts::InstanceOf<string::StringImpl> Str>
class PathImpl;

template<concepts::Encoding Enc>
class PathViewImpl
    : public meta::EnableView<PathViewImpl<Enc>>
    , public meta::EnableBorrowedContainer<PathViewImpl<Enc>>
    , public ConstantPathInterface<PathViewImpl<Enc>, Enc>
    , public util::OwnedType<PathViewImpl<Enc>, PathImpl<string::StringImpl<Enc>>> {
private:
    using View = string::StringViewImpl<Enc>;
    using CodePoint = meta::EncodingCodePoint<Enc>;
    using ViewIter = meta::ContainerIterator<View>;
    using Iterator = PathIterator<Enc>;

public:
    using Encoding = Enc;

    PathViewImpl() = default;

    constexpr PathViewImpl(View view) : m_view(view) { this->compute_first_component_end(); }

    constexpr PathViewImpl(Iterator start, Iterator end)
        : PathViewImpl(View(encoding::assume_valid, start.current_data(), end.current_data())) {}

    constexpr auto data() const -> View { return m_view; }

private:
    View m_view;
};
}
