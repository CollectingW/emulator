// SPDX-FileCopyrightText: Copyright 2026 citron Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <string>
#include <vector>

#include "common/common_types.h"

// Snapshot of the account's friends, refreshed by the frontend and read by the friend service.
// The service must never make a network call from an IPC handler, so it reads this instead.
namespace Common::NextendoFriends {

struct Entry {
    u64 pid = 0;
    std::string name;
    s32 status = 0;        // 0 offline, 1 online, 2 in a game
    std::string app_field; // opaque per-title presence blob, base64 as the server sends it
};

void Set(std::vector<Entry> entries);
std::vector<Entry> Get();

// This player's own presence, as last set by the running game. Pushed to the account server so
// friends see them online.
// nn::friends::PresenceStatus
enum : s32 {
    PresenceOffline = 0,
    PresenceOnline = 1,
    PresenceOnlinePlay = 2,
};

void SetLocalPresence(s32 status, std::string app_field);

// Sets only the status, keeping any app_field the running game published; a title's joinable-session
// blob must survive an emulator-driven status change.
void SetLocalStatus(s32 status);
s32 GetLocalStatus();
std::string GetLocalAppField();

// Hands out the local presence only when it changed since the last call, so the frontend can poll
// cheaply without republishing the same state.
bool TakeLocalPresenceIfChanged(s32& status, std::string& app_field);

} // namespace Common::NextendoFriends
