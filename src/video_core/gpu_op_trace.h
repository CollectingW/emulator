// SPDX-FileCopyrightText: Copyright 2026 Citron Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <atomic>
#include <cstddef>

#include "common/common_types.h"

namespace VideoCore {

// Ring buffer of recent GPU ops, indexed by a counter usable as a VK_NV_device_diagnostic_checkpoints
// marker so ReportLoss() can resolve what the GPU actually reached, not just what was submitted.
enum class GpuOpKind : u32 {
    None,
    Draw,
    DrawIndexed,
    DrawIndirect,
    DrawTexture,
    Clear,
    DispatchCompute,
    DispatchComputeIndirect,
};

struct GpuOpRecord {
    GpuOpKind kind = GpuOpKind::None;
    u32 arg0 = 0;
    u32 arg1 = 0;
    u32 arg2 = 0;
    u32 render_width = 0;
    u32 render_height = 0;
    std::array<u64, 6> pipeline_stage_hashes{}; // per-stage shader hash; zeroed for non-graphics ops
};

inline constexpr size_t GPU_OP_RING_SIZE = 4096;

struct GpuOpTraceState {
    std::atomic<u64> counter{0};
    std::array<GpuOpRecord, GPU_OP_RING_SIZE> ring{}; // single-writer, best-effort reads on report
};

inline GpuOpTraceState g_gpu_op_trace;

inline u64 RecordGpuOp(GpuOpKind kind, u32 arg0 = 0, u32 arg1 = 0, u32 arg2 = 0) {
    const u64 index = g_gpu_op_trace.counter.fetch_add(1, std::memory_order_relaxed);
    auto& entry = g_gpu_op_trace.ring[index % GPU_OP_RING_SIZE];
    entry = GpuOpRecord{.kind = kind, .arg0 = arg0, .arg1 = arg1, .arg2 = arg2};
    return index;
}

inline u64 RecordGpuOpGraphics(GpuOpKind kind, u32 arg0, u32 arg1,
                               const std::array<u64, 6>& stage_hashes) {
    const u64 index = RecordGpuOp(kind, arg0, arg1, 0);
    g_gpu_op_trace.ring[index % GPU_OP_RING_SIZE].pipeline_stage_hashes = stage_hashes;
    return index;
}

inline void SetGpuOpRenderArea(u64 index, u32 width, u32 height) {
    auto& entry = g_gpu_op_trace.ring[index % GPU_OP_RING_SIZE];
    entry.render_width = width;
    entry.render_height = height;
}

inline GpuOpRecord LookupGpuOp(u64 index) {
    return g_gpu_op_trace.ring[index % GPU_OP_RING_SIZE];
}

inline const char* GpuOpKindName(GpuOpKind kind) {
    switch (kind) {
    case GpuOpKind::Draw:
        return "Draw";
    case GpuOpKind::DrawIndexed:
        return "DrawIndexed";
    case GpuOpKind::DrawIndirect:
        return "DrawIndirect";
    case GpuOpKind::DrawTexture:
        return "DrawTexture";
    case GpuOpKind::Clear:
        return "Clear";
    case GpuOpKind::DispatchCompute:
        return "DispatchCompute";
    case GpuOpKind::DispatchComputeIndirect:
        return "DispatchComputeIndirect";
    default:
        return "None";
    }
}

} // namespace VideoCore
