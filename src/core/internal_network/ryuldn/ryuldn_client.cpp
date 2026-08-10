// SPDX-FileCopyrightText: Copyright 2026 citron Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/internal_network/ryuldn/ryuldn_client.h"

#include <chrono>
#include <cstring>

#include "common/logging.h"
#include "common/settings.h"
#include "core/internal_network/network.h"
#include "core/internal_network/sockets.h"

namespace Network::RyuLdn {

namespace {
constexpr s32 ConnectTimeoutMs = 4000;
constexpr u16 DefaultPort = 30456;

Network::IPv4Address WireIpToAddress(u32 wire_ip) {
    return {static_cast<u8>(wire_ip >> 24), static_cast<u8>(wire_ip >> 16),
           static_cast<u8>(wire_ip >> 8), static_cast<u8>(wire_ip)};
}

std::pair<std::string, u16> SplitHostPort(const std::string& server) {
    const auto colon = server.rfind(':');
    if (colon == std::string::npos) {
        return {server, DefaultPort};
    }
    const std::string host = server.substr(0, colon);
    u16 port = DefaultPort;
    try {
        port = static_cast<u16>(std::stoul(server.substr(colon + 1)));
    } catch (const std::exception&) {
        LOG_WARNING(Network, "RyuLDN: couldn't parse port in '{}', using default {}", server,
                   DefaultPort);
    }
    return {host, port};
}
} // namespace

RyuLdnClient::RyuLdnClient() = default;

RyuLdnClient::~RyuLdnClient() {
    Disconnect();
}

bool RyuLdnClient::Connect() {
    if (connected) {
        return true;
    }

    const auto [host, port] = SplitHostPort(Settings::values.ryuldn_server.GetValue());
    const auto resolved = GetAddressInfo(host, std::to_string(port));
    if (!resolved || resolved->empty()) {
        LOG_ERROR(Network, "RyuLDN: couldn't resolve '{}'", host);
        return false;
    }

    socket = std::make_unique<Network::Socket>();
    if (socket->Initialize(Domain::INET, Type::STREAM, Protocol::TCP) != Errno::SUCCESS) {
        LOG_ERROR(Network, "RyuLDN: socket() failed");
        socket.reset();
        return false;
    }

    socket->SetNonBlock(true);
    const SockAddrIn addr = resolved->front().addr;
    const Errno connect_err = socket->Connect(addr);
    if (connect_err != Errno::SUCCESS && connect_err != Errno::INPROGRESS &&
        connect_err != Errno::AGAIN) {
        LOG_ERROR(Network, "RyuLDN: connect() to {}:{} failed", host, port);
        socket.reset();
        return false;
    }

    std::vector<PollFD> poll_fds{PollFD{socket.get(), PollEvents::Out, PollEvents{}}};
    const auto [poll_count, poll_err] = Poll(poll_fds, ConnectTimeoutMs);
    if (poll_err != Errno::SUCCESS || poll_count == 0) {
        LOG_ERROR(Network, "RyuLDN: connect to {}:{} timed out", host, port);
        socket.reset();
        return false;
    }
    const auto [pending_err, getsockopt_err] = socket->GetPendingError();
    if (getsockopt_err != Errno::SUCCESS || pending_err != Errno::SUCCESS) {
        LOG_ERROR(Network, "RyuLDN: connect to {}:{} failed", host, port);
        socket.reset();
        return false;
    }
    socket->SetNonBlock(false);

    connected = true;

    Send(PacketId::Initialize, InitializeMessage{});
    Send(PacketId::Passphrase, PassphraseMessage{});

    receive_thread = std::thread(&RyuLdnClient::ReceiveLoop, this);

    LOG_INFO(Network, "RyuLDN: connected to {}:{}", host, port);
    return true;
}

void RyuLdnClient::Disconnect() {
    connected = false;
    CloseSocketOnce();
    if (receive_thread.joinable()) {
        receive_thread.join();
    }
    socket.reset();
}

bool RyuLdnClient::IsConnected() const {
    return connected;
}

void RyuLdnClient::CloseSocketOnce() {
    std::scoped_lock lock{socket_mutex};
    if (socket && socket->IsOpened()) {
        socket->Close();
    }
}

void RyuLdnClient::SetOnPing(PingCallback callback) {
    on_ping = std::move(callback);
}

void RyuLdnClient::SetOnNetworkError(NetworkErrorCallback callback) {
    on_network_error = std::move(callback);
}

void RyuLdnClient::SetOnDisconnected(DisconnectedCallback callback) {
    on_disconnected = std::move(callback);
}

void RyuLdnClient::SendRaw(std::span<const u8> bytes) {
    std::scoped_lock lock{send_mutex};
    std::size_t sent = 0;
    while (sent < bytes.size()) {
        const auto [n, err] = socket->Send(bytes.subspan(sent), 0);
        if (err != Errno::SUCCESS || n <= 0) {
            LOG_WARNING(Network, "RyuLDN: send failed, dropping connection");
            socket->Close();
            return;
        }
        sent += static_cast<std::size_t>(n);
    }
}

void RyuLdnClient::Send(PacketId type) {
    LdnHeader header{.magic = Magic, .type = static_cast<u8>(type),
                     .version = CurrentProtocolVersion, .data_size = 0};
    SendRaw(std::span{reinterpret_cast<const u8*>(&header), sizeof(header)});
}

template <typename T>
void RyuLdnClient::Send(PacketId type, const T& packet) {
    static_assert(std::is_trivially_copyable_v<T>);
    LdnHeader header{.magic = Magic, .type = static_cast<u8>(type),
                     .version = CurrentProtocolVersion, .data_size = sizeof(T)};
    std::vector<u8> buffer(sizeof(LdnHeader) + sizeof(T));
    std::memcpy(buffer.data(), &header, sizeof(LdnHeader));
    std::memcpy(buffer.data() + sizeof(LdnHeader), &packet, sizeof(T));
    SendRaw(buffer);
}

void RyuLdnClient::Send(PacketId type, std::span<const u8> data) {
    LdnHeader header{.magic = Magic, .type = static_cast<u8>(type),
                     .version = CurrentProtocolVersion,
                     .data_size = static_cast<s32>(data.size())};
    std::vector<u8> buffer(sizeof(LdnHeader) + data.size());
    std::memcpy(buffer.data(), &header, sizeof(LdnHeader));
    std::memcpy(buffer.data() + sizeof(LdnHeader), data.data(), data.size());
    SendRaw(buffer);
}

void RyuLdnClient::ReceiveLoop() {
    const auto recv_exact = [this](std::span<u8> out) {
        std::size_t received = 0;
        while (received < out.size()) {
            const auto [n, err] = socket->Recv(0, out.subspan(received));
            if (err != Errno::SUCCESS || n <= 0) {
                return false;
            }
            received += static_cast<std::size_t>(n);
        }
        return true;
    };

    while (connected) {
        LdnHeader header{};
        if (!recv_exact(std::span{reinterpret_cast<u8*>(&header), sizeof(header)})) {
            break;
        }
        if (header.magic != Magic) {
            LOG_ERROR(Network, "RyuLDN: bad magic in received packet, dropping connection");
            break;
        }
        if (header.version != CurrentProtocolVersion) {
            LOG_ERROR(Network, "RyuLDN: protocol version mismatch (got {}, expected {})",
                     header.version, CurrentProtocolVersion);
            break;
        }
        if (header.data_size < 0 ||
            static_cast<std::size_t>(header.data_size) + sizeof(LdnHeader) >= MaxPacketSize) {
            LOG_ERROR(Network, "RyuLDN: oversized packet ({} bytes), dropping connection",
                     header.data_size);
            break;
        }

        std::vector<u8> data(static_cast<std::size_t>(header.data_size));
        if (!data.empty() && !recv_exact(data)) {
            break;
        }

        HandlePacket(header, data);
    }

    const bool was_connected = connected.exchange(false);
    CloseSocketOnce();
    if (was_connected && on_disconnected) {
        on_disconnected();
    }
}

void RyuLdnClient::HandlePacket(const LdnHeader& header, std::span<const u8> data) {
    switch (static_cast<PacketId>(header.type)) {
    case PacketId::Initialize:
        if (data.size() >= sizeof(InitializeMessage)) {
            InitializeMessage msg;
            std::memcpy(&msg, data.data(), sizeof(msg));
            assigned_id = msg.id;
            assigned_mac = msg.mac_address;
        }
        break;
    case PacketId::Ping:
        if (data.size() >= sizeof(PingMessage)) {
            PingMessage msg;
            std::memcpy(&msg, data.data(), sizeof(msg));
            if (msg.requester == 0) {
                Send(PacketId::Ping, msg);
            }
            if (on_ping) {
                on_ping(msg);
            }
        }
        break;
    case PacketId::NetworkError:
        if (data.size() >= sizeof(NetworkErrorMessage)) {
            NetworkErrorMessage msg;
            std::memcpy(&msg, data.data(), sizeof(msg));
            LOG_WARNING(Network, "RyuLDN: server reported NetworkError {}",
                       static_cast<s32>(msg.error));
            if (on_network_error) {
                on_network_error(msg.error);
            }
        }
        break;
    case PacketId::ScanReply:
        if (data.size() >= sizeof(NetworkInfo)) {
            NetworkInfo info;
            std::memcpy(&info, data.data(), sizeof(info));
            std::scoped_lock lock{state_mutex};
            scan_results.push_back(info);
        }
        break;
    case PacketId::ScanReplyEnd:
        break;
    case PacketId::Connected:
        if (data.size() >= sizeof(NetworkInfo)) {
            NetworkInfo info;
            std::memcpy(&info, data.data(), sizeof(info));
            std::scoped_lock lock{state_mutex};
            current_network_info = info;
            network_joined = true;
        }
        break;
    case PacketId::SyncNetwork:
        if (data.size() >= sizeof(NetworkInfo)) {
            NetworkInfo info;
            std::memcpy(&info, data.data(), sizeof(info));
            std::scoped_lock lock{state_mutex};
            if (network_joined) {
                current_network_info = info;
            }
        }
        break;
    case PacketId::Disconnect:
        {
            std::scoped_lock lock{state_mutex};
            network_joined = false;
            current_network_info.reset();
            proxy_ip.reset();
        }
        {
            std::scoped_lock lock{proxy_flows_mutex};
            known_proxy_flows.clear();
        }
        break;
    case PacketId::ProxyConfig:
        if (data.size() >= sizeof(ProxyConfig)) {
            ProxyConfig msg;
            std::memcpy(&msg, data.data(), sizeof(msg));
            std::scoped_lock lock{state_mutex};
            proxy_ip = WireIpToAddress(msg.proxy_ip);
        }
        break;
    case PacketId::ProxyConnect:
        if (data.size() >= sizeof(ProxyConnectRequest) && on_proxy_packet_received) {
            ProxyConnectRequest msg;
            std::memcpy(&msg, data.data(), sizeof(msg));
            Network::ProxyPacket packet;
            packet.local_endpoint = {.family = Domain::INET,
                                     .ip = WireIpToAddress(msg.info.source_ip),
                                     .portno = msg.info.source_port};
            packet.remote_endpoint = {.family = Domain::INET,
                                      .ip = WireIpToAddress(msg.info.dest_ip),
                                      .portno = msg.info.dest_port};
            packet.protocol = FromWireProtocol(msg.info.protocol);
            packet.broadcast = false;
            on_proxy_packet_received(packet);
        }
        break;
    case PacketId::ProxyData:
        if (data.size() >= sizeof(ProxyDataHeader) && on_proxy_packet_received) {
            ProxyDataHeader msg;
            std::memcpy(&msg, data.data(), sizeof(msg));
            const std::size_t payload_offset = sizeof(ProxyDataHeader);
            const std::size_t payload_size =
                std::min(static_cast<std::size_t>(msg.data_length), data.size() - payload_offset);
            Network::ProxyPacket packet;
            packet.local_endpoint = {.family = Domain::INET,
                                     .ip = WireIpToAddress(msg.info.source_ip),
                                     .portno = msg.info.source_port};
            packet.remote_endpoint = {.family = Domain::INET,
                                      .ip = WireIpToAddress(msg.info.dest_ip),
                                      .portno = msg.info.dest_port};
            packet.protocol = FromWireProtocol(msg.info.protocol);
            packet.broadcast = false;
            packet.data.assign(data.begin() + payload_offset,
                               data.begin() + payload_offset + payload_size);
            on_proxy_packet_received(packet);
        }
        break;
    case PacketId::ProxyDisconnect:
        if (data.size() >= sizeof(ProxyDisconnectMessage)) {
            ProxyDisconnectMessage msg;
            std::memcpy(&msg, data.data(), sizeof(msg));
            std::scoped_lock lock{proxy_flows_mutex};
            known_proxy_flows.erase({msg.info.source_ip, msg.info.source_port, msg.info.dest_ip,
                                    msg.info.dest_port, msg.info.protocol});
        }
        break;
    default:
        LOG_DEBUG(Network, "RyuLDN: unhandled packet type {}", header.type);
        break;
    }
}

std::vector<NetworkInfo> RyuLdnClient::Scan(const ScanFilter& filter) {
    {
        std::scoped_lock lock{state_mutex};
        scan_results.clear();
    }
    Send(PacketId::Scan, filter);
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    std::scoped_lock lock{state_mutex};
    return scan_results;
}

bool RyuLdnClient::CreateNetwork(const SecurityConfig& security_config,
                                 const UserConfig& user_config,
                                 const NetworkConfig& network_config) {
    {
        std::scoped_lock lock{state_mutex};
        network_joined = false;
        current_network_info.reset();
    }
    CreateAccessPointRequest request{};
    request.security_config = security_config;
    request.user_config = user_config;
    request.network_config = network_config;
    Send(PacketId::CreateAccessPoint, request);
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    std::scoped_lock lock{state_mutex};
    return network_joined;
}

bool RyuLdnClient::ConnectToNetwork(const NetworkInfo& network_info, const UserConfig& user_config,
                                   u16 local_communication_version) {
    {
        std::scoped_lock lock{state_mutex};
        network_joined = false;
        current_network_info.reset();
    }
    ConnectRequest request{};
    request.user_config = user_config;
    request.local_communication_version = local_communication_version;
    request.option_unknown = 0;
    request.network_info = network_info;
    Send(PacketId::Connect, request);
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    std::scoped_lock lock{state_mutex};
    return network_joined;
}

void RyuLdnClient::LeaveNetwork() {
    Send(PacketId::Disconnect, DisconnectMessage{});
    std::scoped_lock lock{state_mutex};
    network_joined = false;
    current_network_info.reset();
}

void RyuLdnClient::SetAdvertiseData(std::span<const u8> data) {
    Send(PacketId::SetAdvertiseData, data);
}

std::optional<NetworkInfo> RyuLdnClient::GetCurrentNetworkInfo() const {
    std::scoped_lock lock{state_mutex};
    return current_network_info;
}

void RyuLdnClient::SetOnProxyPacketReceived(ProxyPacketCallback callback) {
    on_proxy_packet_received = std::move(callback);
}

std::optional<Network::IPv4Address> RyuLdnClient::GetProxyIp() const {
    std::scoped_lock lock{state_mutex};
    return proxy_ip;
}

void RyuLdnClient::SendProxyPacket(const Network::ProxyPacket& packet) {
    ProxyInfo info{.source_ip = IPv4AddressToInteger(packet.local_endpoint.ip),
                  .source_port = packet.local_endpoint.portno,
                  .dest_ip = IPv4AddressToInteger(packet.remote_endpoint.ip),
                  .dest_port = packet.remote_endpoint.portno,
                  .protocol = ToWireProtocol(packet.protocol)};

    const auto flow_key = std::make_tuple(info.source_ip, info.source_port, info.dest_ip,
                                          info.dest_port, info.protocol);
    {
        std::scoped_lock lock{proxy_flows_mutex};
        if (known_proxy_flows.insert(flow_key).second) {
            Send(PacketId::ProxyConnect, ProxyConnectRequest{.info = info});
        }
    }

    ProxyDataHeader header{.info = info, .data_length = static_cast<u32>(packet.data.size())};
    std::vector<u8> buffer(sizeof(ProxyDataHeader) + packet.data.size());
    std::memcpy(buffer.data(), &header, sizeof(header));
    if (!packet.data.empty()) {
        std::memcpy(buffer.data() + sizeof(header), packet.data.data(), packet.data.size());
    }
    Send(PacketId::ProxyData, std::span<const u8>{buffer});
}

} // namespace Network::RyuLdn
