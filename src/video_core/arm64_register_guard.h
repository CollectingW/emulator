// SPDX-FileCopyrightText: Copyright 2026 citron Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <atomic>
#include <cstddef>

#include "common/common_types.h"

#if defined(ANDROID) && defined(ARCHITECTURE_arm64) && defined(__clang__)
#define CITRON_ARM64_REGISTER_GUARD_SUPPORTED 1
#else
#define CITRON_ARM64_REGISTER_GUARD_SUPPORTED 0
#endif

namespace VideoCore {

template <size_t MaskBits, typename Tag>
[[nodiscard]] bool IsFirstArm64RegisterCorruption(u32 mask) noexcept {
    static std::array<std::atomic_bool, 1ULL << MaskBits> reported_masks{};
    return mask < reported_masks.size() &&
           !reported_masks[mask].exchange(true, std::memory_order_relaxed);
}

} // namespace VideoCore
