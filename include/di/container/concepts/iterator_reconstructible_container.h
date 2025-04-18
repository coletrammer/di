#pragma once

#include "di/container/interface/reconstruct.h"
#include "di/meta/language.h"
#include "di/util/forward.h"

namespace di::concepts {
template<typename It, typename Sent = It>
concept IteratorReconstructibleContainer = requires(It iterator, Sent sentinel) {
    container::reconstruct(util::forward<It>(iterator), util::forward<Sent>(sentinel));
};
}
