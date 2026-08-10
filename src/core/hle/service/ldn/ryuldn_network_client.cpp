// SPDX-FileCopyrightText: Copyright 2026 citron Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/hle/service/ldn/ryuldn_network_client.h"

#include "core/hle/service/ldn/ldn_results.h"

namespace Service::LDN {

RyuLdnNetworkClient::RyuLdnNetworkClient(Network::RyuLdn::RyuLdnClient& client_)
    : client{client_} {}
RyuLdnNetworkClient::~RyuLdnNetworkClient() = default;

State RyuLdnNetworkClient::GetState() const {
    std::scoped_lock lock{state_mutex};
    return state;
}

Result RyuLdnNetworkClient::GetNetworkInfo(NetworkInfo& out_network) const {
    std::scoped_lock lock{state_mutex};
    if (state != State::AccessPointCreated && state != State::StationConnected) {
        return ResultBadState;
    }
    const auto info = client.GetCurrentNetworkInfo();
    if (!info) {
        return ResultBadState;
    }
    out_network = *info;
    return ResultSuccess;
}

Result RyuLdnNetworkClient::GetNetworkInfo(NetworkInfo& out_network,
                                           std::span<NodeLatestUpdate> out_updates) {
    for (auto& update : out_updates) {
        update.state_change = NodeStateChange::None;
    }
    return GetNetworkInfo(out_network);
}

DisconnectReason RyuLdnNetworkClient::GetDisconnectReason() const {
    return DisconnectReason::None;
}

Result RyuLdnNetworkClient::Scan(std::span<NetworkInfo> out_networks, s16& out_count,
                                 const ScanFilter& filter) {
    out_count = 0;
    for (const auto& info : client.Scan(filter)) {
        if (static_cast<std::size_t>(out_count) >= out_networks.size()) {
            break;
        }
        out_networks[static_cast<std::size_t>(out_count)] = info;
        ++out_count;
    }
    return ResultSuccess;
}

Result RyuLdnNetworkClient::SetAdvertiseData(std::span<const u8> data) {
    client.SetAdvertiseData(data);
    return ResultSuccess;
}

Result RyuLdnNetworkClient::OpenAccessPoint() {
    std::scoped_lock lock{state_mutex};
    if (state == State::None) {
        return ResultBadState;
    }
    state = State::AccessPointOpened;
    return ResultSuccess;
}

Result RyuLdnNetworkClient::CloseAccessPoint() {
    std::scoped_lock lock{state_mutex};
    if (state == State::None) {
        return ResultBadState;
    }
    if (state == State::AccessPointCreated) {
        client.LeaveNetwork();
    }
    state = State::Initialized;
    return ResultSuccess;
}

Result RyuLdnNetworkClient::OpenStation() {
    std::scoped_lock lock{state_mutex};
    if (state == State::None) {
        return ResultBadState;
    }
    state = State::StationOpened;
    return ResultSuccess;
}

Result RyuLdnNetworkClient::CloseStation() {
    std::scoped_lock lock{state_mutex};
    if (state == State::None) {
        return ResultBadState;
    }
    if (state == State::StationConnected) {
        client.LeaveNetwork();
    }
    state = State::Initialized;
    return ResultSuccess;
}

Result RyuLdnNetworkClient::CreateNetwork(const SecurityConfig& security_config,
                                          const UserConfig& user_config,
                                          const NetworkConfig& network_config) {
    std::scoped_lock lock{state_mutex};
    if (state != State::AccessPointOpened) {
        return ResultBadState;
    }
    if (!client.CreateNetwork(security_config, user_config, network_config)) {
        return ResultAccessPointConnectionFailed;
    }
    state = State::AccessPointCreated;
    return ResultSuccess;
}

Result RyuLdnNetworkClient::DestroyNetwork() {
    std::scoped_lock lock{state_mutex};
    client.LeaveNetwork();
    state = State::AccessPointOpened;
    return ResultSuccess;
}

Result RyuLdnNetworkClient::Connect(const NetworkInfo& network_info, const UserConfig& user_config,
                                    u16 local_communication_version) {
    std::scoped_lock lock{state_mutex};
    if (state != State::StationOpened) {
        return ResultBadState;
    }
    if (!client.ConnectToNetwork(network_info, user_config, local_communication_version)) {
        return ResultConnectionFailed;
    }
    state = State::StationConnected;
    return ResultSuccess;
}

Result RyuLdnNetworkClient::Disconnect() {
    std::scoped_lock lock{state_mutex};
    client.LeaveNetwork();
    state = State::StationOpened;
    return ResultSuccess;
}

Result RyuLdnNetworkClient::Initialize(NetworkEventFunc lan_event_, bool) {
    lan_event = std::move(lan_event_);
    if (!client.Connect()) {
        return ResultConnectionFailed;
    }
    std::scoped_lock lock{state_mutex};
    state = State::Initialized;
    return ResultSuccess;
}

Result RyuLdnNetworkClient::Finalize() {
    client.Disconnect();
    std::scoped_lock lock{state_mutex};
    state = State::None;
    return ResultSuccess;
}

} // namespace Service::LDN
