// SPDX-FileCopyrightText: Copyright 2026 citron Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <functional>
#include <map>
#include <string>
#include <vector>
#include "common/common_types.h"

namespace WebService::NextendoApi {

struct LoginResult {
    bool ok = false;
    std::string error;       // Set when ok is false.
    u64 pid = 0;
    std::string username;
    std::string friend_code;
    std::string token;
};

// Reason a NEX login was refused, as evaluated by the account server's gates.
struct OnlineStatus {
    bool queried = false;    // False when the request itself failed (offline, no token).
    bool allow = false;
    std::string reason;      // unknown | disabled | unverified | discord_unlinked | elsewhere
    std::string message;     // Human-readable text for the reason.
};

// The API base url. NEXTENDO_API overrides it; only https or loopback is accepted, because this
// request carries the account token.
std::string BaseUrl();

// Signs in through the user's browser (OAuth loopback + PKCE), so the emulator never sees the
// e-mail or password: password login on /api/login is website-only, behind a captcha. `open_url` is
// handed the authorize URL to open. Blocks until the browser reaches the loopback callback.
LoginResult SignInWithBrowser(const std::function<void(const std::string&)>& open_url);

// Uses the stored account token. Answers only about the caller's own account.
OnlineStatus GetOnlineStatus();

struct HistoryEntry {
    std::string title_id; // 16 uppercase hex digits
    std::string name;     // may be empty; the server keeps the name it already has
    u64 seconds = 0;
    std::string last_played;  // RFC3339 UTC
    std::string icon_base64;  // may be empty
};

// Pushes play time for the account's history. The server merges, so sending only what changed is
// enough. Best-effort: a failure is logged and dropped.
void SyncHistory(const std::vector<HistoryEntry>& entries);

struct Friend {
    u64 pid = 0;
    std::string name;        // console nickname if set, else account username
    std::string friend_code;
    s32 presence_status = 0; // 0 offline, 1 online, 2 in a game
    std::string app_field;   // opaque per-title presence blob
    std::string app_id;      // running title id, 16 hex digits; empty when nothing is running
    std::string image_base64; // profile picture, base64 JPEG; empty if none set
};

struct FriendList {
    bool ok = false;
    std::string error;
    std::vector<Friend> friends;
    std::vector<Friend> requests; // incoming, awaiting accept/decline
};

FriendList GetFriends();

// All return an empty string on success, else a message fit to show the user.
std::string AddFriendByCode(const std::string& friend_code);
std::string AcceptFriend(u64 pid);
std::string DeclineFriend(u64 pid);
std::string RemoveFriend(u64 pid);

// Pushes the console nickname so friends and games see the account's name, not "Player".
void PushProfileName(const std::string& name);

// Publishes this player's presence so friends see them online and, for titles that use it,
// joinable. app_id is the running title id; empty when nothing is running.
void PushPresence(s32 status, const std::string& app_field, const std::string& app_id);

// Downloads the BCAT schedule seed (a zip of vsdata/coopdata/fesdata) for titles that need one
// locally to go online (Splatoon 2). title_id_hex is 16 uppercase hex digits. Empty on failure.
std::vector<u8> DownloadBcatSeed(const std::string& title_id_hex);

// Public, unauthenticated. Maps lowercase-hex title id -> currently connected player count.
std::map<std::string, int> GetOnlineCounts();

struct Profile {
    bool ok = false;
    std::string error;
    std::string name;         // acct.Username, the authoritative account nickname
    std::string image_base64; // own profile picture, base64 JPEG; empty if none set
};

// Uses the stored account token. Own profile only (name + picture).
Profile GetProfile();

struct HistoryItem {
    std::string title_id;     // 16 hex digits
    std::string name;
    std::string icon_base64;  // may be empty
    u64 seconds = 0;
    std::string last_played;  // RFC3339 UTC, may be empty if never recorded
};

struct HistoryList {
    bool ok = false;
    std::string error;
    std::vector<HistoryItem> entries;
};

HistoryList GetHistory();

} // namespace WebService::NextendoApi
