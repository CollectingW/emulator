// SPDX-FileCopyrightText: Copyright 2026 citron Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>

#include "common/common_types.h"
#include "common/socket_types.h"
#include "core/hle/service/ldn/ldn_types.h"

namespace Network::RyuLdn {

using NetworkInfo = Service::LDN::NetworkInfo;
using ScanFilter = Service::LDN::ScanFilter;
using SecurityConfig = Service::LDN::SecurityConfig;
using SecurityParameter = Service::LDN::SecurityParameter;
using UserConfig = Service::LDN::UserConfig;
using NetworkConfig = Service::LDN::NetworkConfig;
using ConnectRequest = Service::LDN::ConnectRequest;

constexpr u8 CurrentProtocolVersion = 1;
constexpr u32 Magic = 0x4E444C52;
constexpr std::size_t MaxPacketSize = 131072;

enum class PacketId : u8 {
    Initialize = 0,
    Passphrase = 1,
    CreateAccessPoint = 2,
    CreateAccessPointPrivate = 3,
    ExternalProxy = 4,
    ExternalProxyToken = 5,
    ExternalProxyState = 6,
    SyncNetwork = 7,
    Reject = 8,
    RejectReply = 9,
    Scan = 10,
    ScanReply = 11,
    ScanReplyEnd = 12,
    Connect = 13,
    ConnectPrivate = 14,
    Connected = 15,
    Disconnect = 16,
    ProxyConfig = 17,
    ProxyConnect = 18,
    ProxyConnectReply = 19,
    ProxyData = 20,
    ProxyDisconnect = 21,
    SetAcceptPolicy = 22,
    SetAdvertiseData = 23,
    Ping = 254,
    NetworkError = 255,
};

enum class NetworkError : s32 {
    None = 0,
    PortUnreachable = 1,
    TooManyPlayers = 2,
    VersionTooLow = 3,
    VersionTooHigh = 4,
    ConnectFailure = 5,
    ConnectNotFound = 6,
    ConnectTimeout = 7,
    ConnectRejected = 8,
    RejectFailed = 9,
    Unknown = -1,
};

#pragma pack(push, 1)

struct LdnHeader {
    u32 magic;
    u8 type;
    u8 version;
    s32 data_size;
};
static_assert(sizeof(LdnHeader) == 0xA);

struct InitializeMessage {
    std::array<u8, 16> id{};
    std::array<u8, 6> mac_address{};
};
static_assert(sizeof(InitializeMessage) == 0x16);

struct PassphraseMessage {
    std::array<u8, 128> passphrase{};
};
static_assert(sizeof(PassphraseMessage) == 0x80);

struct PingMessage {
    u8 requester;
    u8 id;
};
static_assert(sizeof(PingMessage) == 0x2);

struct NetworkErrorMessage {
    NetworkError error;
};
static_assert(sizeof(NetworkErrorMessage) == 0x4);

struct RejectRequest {
    u32 node_id;
    s32 disconnect_reason;
};
static_assert(sizeof(RejectRequest) == 0x8);

struct DisconnectMessage {
    u32 disconnect_ip;
};
static_assert(sizeof(DisconnectMessage) == 0x4);

struct RyuNetworkConfig {
    std::array<u8, 16> game_version{};
    std::array<u8, 16> private_ip{};
    s32 address_family{};
    u16 external_proxy_port{};
    u16 internal_proxy_port{};
};
static_assert(sizeof(RyuNetworkConfig) == 0x28);

struct CreateAccessPointRequest {
    SecurityConfig security_config;
    UserConfig user_config;
    NetworkConfig network_config;
    RyuNetworkConfig ryu_network_config;
};
static_assert(sizeof(CreateAccessPointRequest) == 0xBC);

struct ProxyConfig {
    u32 proxy_ip;
    u32 proxy_subnet_mask;
};
static_assert(sizeof(ProxyConfig) == 0x8);

struct ProxyInfo {
    u32 source_ip;
    u16 source_port;
    u32 dest_ip;
    u16 dest_port;
    s32 protocol;
};
static_assert(sizeof(ProxyInfo) == 0x10);

struct ProxyConnectRequest {
    ProxyInfo info;
};
static_assert(sizeof(ProxyConnectRequest) == 0x10);

struct ProxyConnectResponse {
    ProxyInfo info;
};
static_assert(sizeof(ProxyConnectResponse) == 0x10);

struct ProxyDataHeader {
    ProxyInfo info;
    u32 data_length;
};
static_assert(sizeof(ProxyDataHeader) == 0x14);

struct ProxyDisconnectMessage {
    ProxyInfo info;
    s32 disconnect_reason;
};
static_assert(sizeof(ProxyDisconnectMessage) == 0x14);

struct SetAcceptPolicyRequest {
    Service::LDN::AcceptPolicy station_accept_policy;
};
static_assert(sizeof(SetAcceptPolicyRequest) == 0x1);

constexpr s32 WireProtocolTcp = 6;
constexpr s32 WireProtocolUdp = 17;

inline s32 ToWireProtocol(Network::Protocol protocol) {
    switch (protocol) {
    case Network::Protocol::TCP:
        return WireProtocolTcp;
    case Network::Protocol::UDP:
        return WireProtocolUdp;
    default:
        return 0;
    }
}

inline Network::Protocol FromWireProtocol(s32 protocol) {
    switch (protocol) {
    case WireProtocolTcp:
        return Network::Protocol::TCP;
    case WireProtocolUdp:
        return Network::Protocol::UDP;
    default:
        return Network::Protocol::Unspecified;
    }
}

#pragma pack(pop)

} // namespace Network::RyuLdn
