// SPDX-FileCopyrightText: 2026 citron Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

// Optional Tracy instrumentation. When CITRON_ENABLE_TRACY is off (the default),
// every macro below expands to nothing — no Tracy headers, no client code, no
// runtime overhead.

#if defined(CITRON_ENABLE_TRACY) && CITRON_ENABLE_TRACY

 #    ifndef TRACY_ENABLE
 #        define TRACY_ENABLE
 #    endif
 #    ifndef TRACY_FIBERS
 #        define TRACY_FIBERS
 #    endif
 #    ifndef TRACY_CALLSTACK
 #        define TRACY_CALLSTACK 15
 #    endif

 #    include <tracy/Tracy.hpp>

 #    define CITRON_PROFILE_SCOPE(name) ZoneScopedN(name)
 #    define CITRON_PROFILE_FRAME_MARK() FrameMark
 #    define CITRON_PROFILE_FRAME_MARK_N(name) FrameMarkNamed(name)

 #    if defined(CITRON_ENABLE_TRACY_MEMORY) && CITRON_ENABLE_TRACY_MEMORY
 #        define CITRON_PROFILE_MEM_SCOPE(name) CITRON_PROFILE_SCOPE(name)
 #    else
 #        define CITRON_PROFILE_MEM_SCOPE(name)
 #    endif

 #else

 #    define CITRON_PROFILE_SCOPE(name)
 #    define CITRON_PROFILE_FRAME_MARK()
 #    define CITRON_PROFILE_FRAME_MARK_N(name)
 #    define CITRON_PROFILE_MEM_SCOPE(name)

 #endif
