// SPDX-FileCopyrightText: Copyright 2026 citron Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <mutex>

#include "core/hle/service/ldn/network_client.h"
#include "core/internal_network/ryuldn/ryuldn_client.h"

namespace Service::LDN {

class RyuLdnNetworkClient final : public INetworkClient {
public:
    explicit RyuLdnNetworkClient(Network::RyuLdn::RyuLdnClient& client_);
    ~RyuLdnNetworkClient() override;

    State GetState() const override;

    Result GetNetworkInfo(NetworkInfo& out_network) const override;
    Result GetNetworkInfo(NetworkInfo& out_network,
                          std::span<NodeLatestUpdate> out_updates) override;

    DisconnectReason GetDisconnectReason() const override;
    Result Scan(std::span<NetworkInfo> out_networks, s16& out_count,
               const ScanFilter& filter) override;
    Result SetAdvertiseData(std::span<const u8> data) override;

    Result OpenAccessPoint() override;
    Result CloseAccessPoint() override;

    Result OpenStation() override;
    Result CloseStation() override;

    Result CreateNetwork(const SecurityConfig& security_config, const UserConfig& user_config,
                         const NetworkConfig& network_config) override;
    Result DestroyNetwork() override;

    Result Connect(const NetworkInfo& network_info, const UserConfig& user_config,
                  u16 local_communication_version) override;
    Result Disconnect() override;

    Result Initialize(NetworkEventFunc lan_event = {}, bool listening = true) override;
    Result Finalize() override;

private:
    Network::RyuLdn::RyuLdnClient& client;
    NetworkEventFunc lan_event;
    mutable std::mutex state_mutex;
    State state{State::None};
};

} // namespace Service::LDN
