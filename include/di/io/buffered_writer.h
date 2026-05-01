#pragma once

#include "di/container/algorithm/min.h"
#include "di/container/vector/static_vector.h"
#include "di/io/interface/writer.h"
#include "di/io/write_exactly.h"
#include "di/vocab/error/result.h"

namespace di::io {
template<concepts::ConstexprOf<usize> SizeConstant, Impl<Writer> W>
class BufferedWriter {
private:
    constexpr static auto inline_capacity = usize(SizeConstant {});

public:
    constexpr explicit BufferedWriter(meta::Constexpr<inline_capacity>, W writer) : m_writer(di::move(writer)) {}

    constexpr auto write_some(Span<byte const> data) -> Result<usize> {
        auto const byte_count = data.size();
        if (data.size() >= inline_capacity) {
            DI_TRY(flush());
            DI_TRY(write_exactly(m_writer, data));
            return byte_count;
        }
        while (!data.empty()) {
            auto to_consume = di::min(data.size(), inline_capacity - m_buffer.size());
            m_buffer.append_container(*data.first(to_consume));
            data = *data.subspan(to_consume);

            if (m_buffer.size() == inline_capacity) {
                DI_TRY(flush());
            }
        }
        return byte_count;
    }

    constexpr auto flush() -> Result<> {
        if (!m_buffer.empty()) {
            DI_TRY(write_exactly(m_writer, m_buffer.span()));
            m_buffer.clear();
        }
        return {};
    }

    constexpr auto interactive_device() const -> bool { return io::interactive_device(m_writer); }

private:
    W m_writer;
    di::StaticVector<byte, Constexpr<inline_capacity>> m_buffer;
};

template<typename T, typename U>
BufferedWriter(T&&, U&&) -> BufferedWriter<meta::RemoveCVRef<T>, meta::RemoveCVRef<U>>;
}

namespace di {
using io::BufferedWriter;
}
