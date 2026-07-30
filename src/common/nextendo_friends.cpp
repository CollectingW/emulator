// SPDX-FileCopyrightText: Copyright 2026 citron Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <mutex>

#include "common/nextendo_friends.h"

namespace Common::NextendoFriends {

namespace {
std::mutex g_mutex;
std::vector<Entry> g_entries;
s32 g_local_status = 0;
std::string g_local_app_field;
bool g_local_dirty = false;
} // Anonymous namespace

void Set(std::vector<Entry> entries) {
    std::lock_guard lock{g_mutex};
    g_entries = std::move(entries);
}

std::vector<Entry> Get() {
    std::lock_guard lock{g_mutex};
    return g_entries;
}

void SetLocalPresence(s32 status, std::string app_field) {
    std::lock_guard lock{g_mutex};
    if (g_local_status != status || g_local_app_field != app_field) {
        g_local_dirty = true;
    }
    g_local_status = status;
    g_local_app_field = std::move(app_field);
}

void SetLocalStatus(s32 status) {
    std::lock_guard lock{g_mutex};
    if (g_local_status != status) {
        g_local_status = status;
        g_local_dirty = true;
    }
}

s32 GetLocalStatus() {
    std::lock_guard lock{g_mutex};
    return g_local_status;
}

std::string GetLocalAppField() {
    std::lock_guard lock{g_mutex};
    return g_local_app_field;
}

bool TakeLocalPresenceIfChanged(s32& status, std::string& app_field) {
    std::lock_guard lock{g_mutex};
    if (!g_local_dirty) {
        return false;
    }
    g_local_dirty = false;
    status = g_local_status;
    app_field = g_local_app_field;
    return true;
}

} // namespace Common::NextendoFriends
