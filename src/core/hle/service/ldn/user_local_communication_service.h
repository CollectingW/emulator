// SPDX-FileCopyrightText: Copyright 2024 yuzu Emulator Project
// SPDX-FileCopyrightText: Copyright 2025 citron Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <memory>

#include "core/hle/service/cmif_types.h"
#include "core/hle/service/kernel_helpers.h"
#include "core/hle/service/ldn/ldn_types.h"
#include "core/hle/service/ldn/network_client.h"
#include "core/hle/service/service.h"

namespace Core {
class System;
}

namespace Network {
class RoomNetwork;
}

namespace Service::LDN {

class IUserLocalCommunicationService final
    : public ServiceFramework<IUserLocalCommunicationService> {
public:
    explicit IUserLocalCommunicationService(Core::System& system_);
    ~IUserLocalCommunicationService() override;

private:
    Result GetState(Out<State> out_state);

    Result GetNetworkInfo(OutLargeData<NetworkInfo, BufferAttr_HipcPointer> out_network_info);

    Result GetIpv4Address(Out<Ipv4Address> out_current_address, Out<Ipv4Address> out_subnet_mask);

    Result GetDisconnectReason(Out<DisconnectReason> out_disconnect_reason);

    Result GetSecurityParameter(Out<SecurityParameter> out_security_parameter);

    Result GetNetworkConfig(Out<NetworkConfig> out_network_config);

    Result AttachStateChangeEvent(OutCopyHandle<Kernel::KReadableEvent> out_event);

    Result GetNetworkInfoLatestUpdate(
        OutLargeData<NetworkInfo, BufferAttr_HipcPointer> out_network_info,
        OutArray<NodeLatestUpdate, BufferAttr_HipcPointer> out_node_latest_update);

    Result Scan(Out<s16> network_count, WifiChannel channel, const ScanFilter& scan_filter,
                OutArray<NetworkInfo, BufferAttr_HipcAutoSelect> out_network_info);

    Result ScanPrivate(Out<s16> network_count, WifiChannel channel, const ScanFilter& scan_filter,
                       OutArray<NetworkInfo, BufferAttr_HipcAutoSelect> out_network_info);

    Result SetWirelessControllerRestriction(WirelessControllerRestriction wireless_restriction);

    Result SetWirelessAudioPolicy(WirelessAudioRestriction wireless_audio_restriction);

    Result SetProtocol(Protocol protocol);

    Result OpenAccessPoint();

    Result CloseAccessPoint();

    Result CreateNetwork(const CreateNetworkConfig& create_network_Config);

    Result CreateNetworkPrivate(const CreateNetworkConfigPrivate& create_network_Config,
                                InArray<AddressEntry, BufferAttr_HipcPointer> address_list);

    Result DestroyNetwork();

    Result Reject(Ipv4Address ip_address, u16 port);

    Result SetAdvertiseData(InBuffer<BufferAttr_HipcAutoSelect> buffer_data);

    Result SetStationAcceptPolicy(AcceptPolicy accept_policy);

    Result AddAcceptFilterEntry(MacAddress mac_address);

    Result ClearAcceptFilter();

    Result OpenStation();

    Result CloseStation();

    Result Connect(const ConnectNetworkData& connect_data,
                   InLargeData<NetworkInfo, BufferAttr_HipcPointer> network_info);

    Result ConnectPrivate(const ConnectNetworkData& connect_data,
                          InLargeData<NetworkInfo, BufferAttr_HipcPointer> network_info);

    Result Disconnect();

    Result Initialize(ClientProcessId aruid);

    Result Finalize();

    Result Initialize2(u32 version, ClientProcessId aruid);

private:
    void OnEventFired();

    KernelHelpers::ServiceContext service_context;
    Kernel::KEvent* state_change_event;
    Network::RoomNetwork& room_network;
    std::unique_ptr<INetworkClient> network_client;

    bool is_initialized{};
    Protocol current_protocol{Protocol::NX};
};

} // namespace Service::LDN
