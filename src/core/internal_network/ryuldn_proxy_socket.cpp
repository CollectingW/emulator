// SPDX-FileCopyrightText: Copyright 2026 citron Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <chrono>
#include <thread>

#include "common/assert.h"
#include "common/logging.h"
#include "core/internal_network/network.h"
#include "core/internal_network/network_interface.h"
#include "core/internal_network/ryuldn/ryuldn_client.h"
#include "core/internal_network/ryuldn_proxy_socket.h"

#if defined(__unix__) || defined(__APPLE__)
#include <sys/socket.h>
#endif

namespace Network {

RyuLdnProxySocket::RyuLdnProxySocket(RyuLdn::RyuLdnClient& client_) noexcept : client{client_} {}

RyuLdnProxySocket::~RyuLdnProxySocket() {
    if (fd == INVALID_SOCKET) {
        return;
    }
    fd = INVALID_SOCKET;
}

void RyuLdnProxySocket::HandleProxyPacket(const ProxyPacket& packet) {
    const auto my_ip = client.GetProxyIp();
    if (!my_ip) {
        return;
    }

    if (packet.local_endpoint.ip == *my_ip) {
        return;
    }

    if (!packet.broadcast && packet.remote_endpoint.ip != *my_ip) {
        return;
    }

    if (protocol != packet.protocol || local_endpoint.portno != packet.remote_endpoint.portno ||
        closed) {
        return;
    }

    if (!broadcast && packet.broadcast) {
        return;
    }

    std::lock_guard guard(packets_mutex);
    received_packets.push(packet);
}

template <typename T>
Errno RyuLdnProxySocket::SetSockOpt(SOCKET fd_, int option, T value) {
    LOG_DEBUG(Network, "(STUBBED) called");
    return Errno::SUCCESS;
}

Errno RyuLdnProxySocket::Initialize(Domain domain, Type type, Protocol socket_protocol) {
    protocol = socket_protocol;
    SetSockOpt(fd, SO_TYPE, type);

    is_bound = false;
    closed = false;
    broadcast = false;
    local_endpoint = {};

    std::lock_guard guard(packets_mutex);
    while (!received_packets.empty()) {
        received_packets.pop();
    }

    return Errno::SUCCESS;
}

std::pair<RyuLdnProxySocket::AcceptResult, Errno> RyuLdnProxySocket::Accept() {
    LOG_WARNING(Network, "(STUBBED) called");
    return {AcceptResult{}, Errno::SUCCESS};
}

Errno RyuLdnProxySocket::Connect(SockAddrIn addr_in) {
    LOG_WARNING(Network, "(STUBBED) called");
    return Errno::SUCCESS;
}

std::pair<SockAddrIn, Errno> RyuLdnProxySocket::GetPeerName() {
    LOG_WARNING(Network, "(STUBBED) called");
    return {SockAddrIn{}, Errno::SUCCESS};
}

std::pair<SockAddrIn, Errno> RyuLdnProxySocket::GetSockName() {
    LOG_WARNING(Network, "(STUBBED) called");
    return {SockAddrIn{}, Errno::SUCCESS};
}

Errno RyuLdnProxySocket::Bind(SockAddrIn addr) {
    if (is_bound) {
        LOG_WARNING(Network, "Rebinding Socket is unimplemented!");
        return Errno::SUCCESS;
    }
    local_endpoint = addr;
    is_bound = true;

    return Errno::SUCCESS;
}

Errno RyuLdnProxySocket::Listen(s32 backlog) {
    LOG_WARNING(Network, "(STUBBED) called");
    return Errno::SUCCESS;
}

Errno RyuLdnProxySocket::Shutdown(ShutdownHow how) {
    LOG_WARNING(Network, "(STUBBED) called");
    return Errno::SUCCESS;
}

std::pair<s32, Errno> RyuLdnProxySocket::Recv(int flags, std::span<u8> message) {
    LOG_WARNING(Network, "(STUBBED) called");
    ASSERT(flags == 0);
    ASSERT(message.size() < static_cast<size_t>(std::numeric_limits<int>::max()));

    return {static_cast<s32>(0), Errno::SUCCESS};
}

std::pair<s32, Errno> RyuLdnProxySocket::RecvFrom(int flags, std::span<u8> message,
                                                  SockAddrIn* addr) {
    ASSERT((static_cast<u32>(flags) & ~FLAG_MSG_PEEK) == 0);
    ASSERT(message.size() < static_cast<size_t>(std::numeric_limits<int>::max()));

    const auto timestamp = std::chrono::steady_clock::now();
    const auto timeout = receive_timeout == 0 ? 5000 : receive_timeout;
    while (true) {
        {
            std::lock_guard guard(packets_mutex);
            if (received_packets.size() > 0) {
                return ReceivePacket(flags, message, addr, message.size());
            }
        }

        if (!blocking) {
            return {-1, Errno::AGAIN};
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(1));

        const auto time_diff = std::chrono::steady_clock::now() - timestamp;
        const auto time_diff_ms =
            std::chrono::duration_cast<std::chrono::milliseconds>(time_diff).count();

        if (time_diff_ms > timeout) {
            return {-1, Errno::TIMEDOUT};
        }
    }
}

std::pair<s32, Errno> RyuLdnProxySocket::ReceivePacket(int flags, std::span<u8> message,
                                                       SockAddrIn* addr, std::size_t max_length) {
    ProxyPacket& packet = received_packets.front();
    if (addr) {
        addr->family = Domain::INET;
        addr->ip = packet.local_endpoint.ip;
        addr->portno = packet.local_endpoint.portno;
    }

    bool peek = (flags & FLAG_MSG_PEEK) != 0;
    std::size_t read_bytes;
    if (packet.data.size() > max_length) {
        read_bytes = max_length;
        memcpy(message.data(), packet.data.data(), max_length);

        if (protocol == Protocol::UDP) {
            if (!peek) {
                received_packets.pop();
            }
            return {-1, Errno::MSGSIZE};
        } else if (protocol == Protocol::TCP) {
            std::vector<u8> numArray(packet.data.size() - max_length);
            std::copy(packet.data.begin() + max_length, packet.data.end(),
                      std::back_inserter(numArray));
            packet.data = numArray;
        }
    } else {
        read_bytes = packet.data.size();
        memcpy(message.data(), packet.data.data(), read_bytes);
        if (!peek) {
            received_packets.pop();
        }
    }

    return {static_cast<u32>(read_bytes), Errno::SUCCESS};
}

std::pair<s32, Errno> RyuLdnProxySocket::Send(std::span<const u8> message, int flags) {
    LOG_WARNING(Network, "(STUBBED) called");
    ASSERT(message.size() < static_cast<size_t>(std::numeric_limits<int>::max()));
    ASSERT(flags == 0);

    return {static_cast<s32>(0), Errno::SUCCESS};
}

std::pair<s32, Errno> RyuLdnProxySocket::SendTo(u32 flags, std::span<const u8> message,
                                                const SockAddrIn* addr) {
    ASSERT(flags == 0);

    if (!is_bound) {
        LOG_ERROR(Network, "RyuLdnProxySocket is not bound!");
        return {static_cast<s32>(message.size()), Errno::SUCCESS};
    }

    if (!addr) {
        LOG_ERROR(Network, "SendTo called on RyuLdnProxySocket without destination address");
        return {-1, Errno::INVAL};
    }

    if (!client.IsConnected()) {
        return {static_cast<s32>(message.size()), Errno::SUCCESS};
    }

    ProxyPacket packet;
    packet.local_endpoint = local_endpoint;
    packet.remote_endpoint = *addr;
    packet.protocol = protocol;
    packet.broadcast = broadcast && packet.remote_endpoint.ip[3] == 255;

    auto& ip = local_endpoint.ip;
    const auto my_ip = client.GetProxyIp();
    if (std::all_of(ip.begin(), ip.end(), [](u8 i) { return i == 0; }) && my_ip) {
        packet.local_endpoint.ip = *my_ip;
    }

    packet.data.clear();
    std::copy(message.begin(), message.end(), std::back_inserter(packet.data));

    client.SendProxyPacket(packet);

    return {static_cast<s32>(message.size()), Errno::SUCCESS};
}

Errno RyuLdnProxySocket::Close() {
    std::lock_guard guard(packets_mutex);
    fd = INVALID_SOCKET;
    closed = true;

    while (!received_packets.empty()) {
        received_packets.pop();
    }

    return Errno::SUCCESS;
}

Errno RyuLdnProxySocket::SetLinger(bool enable, u32 linger) {
    struct Linger {
        u16 linger_enable;
        u16 linger_time;
    } values;
    values.linger_enable = enable ? 1 : 0;
    values.linger_time = static_cast<u16>(linger);

    return SetSockOpt(fd, SO_LINGER, values);
}

Errno RyuLdnProxySocket::SetReuseAddr(bool enable) {
    return SetSockOpt<u32>(fd, SO_REUSEADDR, enable ? 1 : 0);
}

Errno RyuLdnProxySocket::SetBroadcast(bool enable) {
    broadcast = enable;
    return SetSockOpt<u32>(fd, SO_BROADCAST, enable ? 1 : 0);
}

Errno RyuLdnProxySocket::SetSndBuf(u32 value) {
    return SetSockOpt(fd, SO_SNDBUF, value);
}

Errno RyuLdnProxySocket::SetKeepAlive(bool enable) {
    return Errno::SUCCESS;
}

Errno RyuLdnProxySocket::SetRcvBuf(u32 value) {
    return SetSockOpt(fd, SO_RCVBUF, value);
}

Errno RyuLdnProxySocket::SetSndTimeo(u32 value) {
    send_timeout = value;
    return SetSockOpt(fd, SO_SNDTIMEO, static_cast<int>(value));
}

Errno RyuLdnProxySocket::SetRcvTimeo(u32 value) {
    receive_timeout = value;
    return SetSockOpt(fd, SO_RCVTIMEO, static_cast<int>(value));
}

Errno RyuLdnProxySocket::SetNonBlock(bool enable) {
    blocking = !enable;
    return Errno::SUCCESS;
}

std::pair<Errno, Errno> RyuLdnProxySocket::GetPendingError() {
    LOG_DEBUG(Network, "(STUBBED) called");
    return {Errno::SUCCESS, Errno::SUCCESS};
}

bool RyuLdnProxySocket::IsOpened() const {
    return fd != INVALID_SOCKET;
}

} // namespace Network
