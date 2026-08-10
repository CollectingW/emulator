// SPDX-FileCopyrightText: Copyright 2026 citron Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <span>
#include <thread>
#include <tuple>
#include <vector>

#include "common/common_types.h"
#include "core/internal_network/ryuldn/ryuldn_protocol.h"
#include "network/room_member.h"

namespace Network {
class Socket;
}

namespace Network::RyuLdn {

class RyuLdnClient {
public:
    RyuLdnClient();
    ~RyuLdnClient();

    RyuLdnClient(const RyuLdnClient&) = delete;
    RyuLdnClient& operator=(const RyuLdnClient&) = delete;

    bool Connect();
    void Disconnect();
    bool IsConnected() const;

    std::vector<NetworkInfo> Scan(const ScanFilter& filter);
    bool CreateNetwork(const SecurityConfig& security_config, const UserConfig& user_config,
                       const NetworkConfig& network_config);
    bool ConnectToNetwork(const NetworkInfo& network_info, const UserConfig& user_config,
                         u16 local_communication_version);
    void LeaveNetwork();
    void SetAdvertiseData(std::span<const u8> data);
    std::optional<NetworkInfo> GetCurrentNetworkInfo() const;

    void SendProxyPacket(const Network::ProxyPacket& packet);
    std::optional<Network::IPv4Address> GetProxyIp() const;

    using PingCallback = std::function<void(const PingMessage&)>;
    using NetworkErrorCallback = std::function<void(NetworkError)>;
    using DisconnectedCallback = std::function<void()>;
    using ProxyPacketCallback = std::function<void(const Network::ProxyPacket&)>;

    void SetOnPing(PingCallback callback);
    void SetOnNetworkError(NetworkErrorCallback callback);
    void SetOnDisconnected(DisconnectedCallback callback);
    void SetOnProxyPacketReceived(ProxyPacketCallback callback);

private:
    void ReceiveLoop();
    void HandlePacket(const LdnHeader& header, std::span<const u8> data);
    void SendRaw(std::span<const u8> bytes);
    void Send(PacketId type);
    template <typename T>
    void Send(PacketId type, const T& packet);
    void Send(PacketId type, std::span<const u8> data);
    void CloseSocketOnce();

    std::unique_ptr<Network::Socket> socket;
    std::thread receive_thread;
    std::atomic<bool> connected{false};
    std::mutex send_mutex;
    std::mutex socket_mutex;

    std::array<u8, 16> assigned_id{};
    std::array<u8, 6> assigned_mac{};

    mutable std::mutex state_mutex;
    std::vector<NetworkInfo> scan_results;
    std::optional<NetworkInfo> current_network_info;
    bool network_joined{false};
    std::optional<Network::IPv4Address> proxy_ip;

    std::mutex proxy_flows_mutex;
    std::set<std::tuple<u32, u16, u32, u16, s32>> known_proxy_flows;

    PingCallback on_ping;
    NetworkErrorCallback on_network_error;
    DisconnectedCallback on_disconnected;
    ProxyPacketCallback on_proxy_packet_received;
};

} // namespace Network::RyuLdn
