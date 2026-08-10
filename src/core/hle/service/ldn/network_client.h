// SPDX-FileCopyrightText: Copyright 2026 citron Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <functional>
#include <span>

#include "core/hle/result.h"
#include "core/hle/service/ldn/ldn_types.h"

namespace Service::LDN {

class INetworkClient {
public:
    using NetworkEventFunc = std::function<void()>;

    virtual ~INetworkClient() = default;

    virtual State GetState() const = 0;

    virtual Result GetNetworkInfo(NetworkInfo& out_network) const = 0;
    virtual Result GetNetworkInfo(NetworkInfo& out_network,
                                  std::span<NodeLatestUpdate> out_updates) = 0;

    virtual DisconnectReason GetDisconnectReason() const = 0;
    virtual Result Scan(std::span<NetworkInfo> out_networks, s16& out_count,
                        const ScanFilter& filter) = 0;
    virtual Result SetAdvertiseData(std::span<const u8> data) = 0;

    virtual Result OpenAccessPoint() = 0;
    virtual Result CloseAccessPoint() = 0;

    virtual Result OpenStation() = 0;
    virtual Result CloseStation() = 0;

    virtual Result CreateNetwork(const SecurityConfig& security_config,
                                const UserConfig& user_config,
                                const NetworkConfig& network_config) = 0;
    virtual Result DestroyNetwork() = 0;

    virtual Result Connect(const NetworkInfo& network_info, const UserConfig& user_config,
                          u16 local_communication_version) = 0;
    virtual Result Disconnect() = 0;

    virtual Result Initialize(NetworkEventFunc lan_event = {}, bool listening = true) = 0;
    virtual Result Finalize() = 0;
};

} // namespace Service::LDN
