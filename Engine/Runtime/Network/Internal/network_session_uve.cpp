// Copyright (c) 2026 UniVex Studios. All Rights Reserved.

#include "uve/network/network_session_uve.h"

namespace UVE::Network {
namespace {

[[nodiscard]] std::vector<std::uint8_t> TaggedUVE(const SessionMessageTagUVE tag) {
    return std::vector<std::uint8_t>{static_cast<std::uint8_t>(tag)};
}

} // namespace

NetworkSessionUVE::NetworkSessionUVE(const SessionRoleUVE role,
                                     std::unique_ptr<ITransportEndpointUVE> endpoint,
                                     const std::uint64_t timeoutMs) noexcept
    : m_role(role), m_endpoint(std::move(endpoint)), m_timeoutMs(timeoutMs) {}

bool NetworkSessionUVE::ConnectUVE(const std::uint64_t nowMs) {
    if (m_role != SessionRoleUVE::Client || m_state != SessionStateUVE::Disconnected ||
        m_endpoint == nullptr) {
        return false;
    }
    // Version travels little-endian in bytes 1-2; the server ignores a Hello too short to hold it.
    const std::vector<std::uint8_t> hello{
        static_cast<std::uint8_t>(SessionMessageTagUVE::Hello),
        static_cast<std::uint8_t>(kSessionProtocolVersionUVE & 0xFFU),
        static_cast<std::uint8_t>((kSessionProtocolVersionUVE >> 8) & 0xFFU)};
    const bool helloSent = m_endpoint->SendUVE(hello);
    m_state = SessionStateUVE::Connecting;
    m_closeReason = SessionCloseReasonUVE::None;
    m_lastActivityMs = nowMs;
    return helloSent;
}

bool NetworkSessionUVE::DisconnectUVE() {
    if (m_state == SessionStateUVE::Disconnected || m_endpoint == nullptr) {
        return false;
    }
    const bool byeSent = m_endpoint->SendUVE(TaggedUVE(SessionMessageTagUVE::Bye));
    TransitionToDisconnectedUVE(SessionCloseReasonUVE::LocalDisconnect);
    return byeSent;
}

void NetworkSessionUVE::UpdateUVE(const std::uint64_t nowMs) {
    if (m_endpoint != nullptr) {
        for (;;) {
            const std::optional<std::vector<std::uint8_t>> datagram = m_endpoint->TryReceiveUVE();
            if (!datagram.has_value()) {
                break;
            }
            HandleDatagramUVE(*datagram, nowMs);
        }
    }
    if (m_timeoutMs != 0U &&
        (m_state == SessionStateUVE::Connecting || m_state == SessionStateUVE::Established) &&
        nowMs >= m_lastActivityMs && nowMs - m_lastActivityMs >= m_timeoutMs) {
        TransitionToDisconnectedUVE(SessionCloseReasonUVE::Timeout);
    }
}

bool NetworkSessionUVE::SendDataUVE(const std::vector<std::uint8_t>& payload) {
    if (m_state != SessionStateUVE::Established || m_endpoint == nullptr ||
        payload.size() > kSessionMaximumPayloadBytesUVE) {
        return false;
    }
    std::vector<std::uint8_t> datagram;
    datagram.reserve(payload.size() + 1U);
    datagram.push_back(static_cast<std::uint8_t>(SessionMessageTagUVE::Data));
    datagram.insert(datagram.end(), payload.begin(), payload.end());
    return m_endpoint->SendUVE(datagram);
}

std::optional<std::vector<std::uint8_t>> NetworkSessionUVE::TryReceiveDataUVE() {
    if (m_inboundData.empty()) {
        return std::nullopt;
    }
    std::vector<std::uint8_t> payload = std::move(m_inboundData.front());
    m_inboundData.pop_front();
    return payload;
}

void NetworkSessionUVE::HandleDatagramUVE(const std::vector<std::uint8_t>& datagram,
                                          const std::uint64_t nowMs) {
    if (datagram.empty()) {
        return;
    }
    switch (static_cast<SessionMessageTagUVE>(datagram[0])) {
        case SessionMessageTagUVE::Hello: {
            // One peer per session: a Hello only opens a listening (Disconnected) server.
            if (m_role != SessionRoleUVE::Server || m_state != SessionStateUVE::Disconnected) {
                return;
            }
            if (datagram.size() < 3U) {
                return;
            }
            const std::uint16_t version = static_cast<std::uint16_t>(
                static_cast<std::uint16_t>(datagram[1]) |
                (static_cast<std::uint16_t>(datagram[2]) << 8));
            if (version != kSessionProtocolVersionUVE) {
                m_endpoint->SendUVE(TaggedUVE(SessionMessageTagUVE::Bye));
                return;
            }
            m_endpoint->SendUVE(TaggedUVE(SessionMessageTagUVE::Welcome));
            m_state = SessionStateUVE::Established;
            m_closeReason = SessionCloseReasonUVE::None;
            m_lastActivityMs = nowMs;
            return;
        }
        case SessionMessageTagUVE::Welcome: {
            if (m_role != SessionRoleUVE::Client || m_state != SessionStateUVE::Connecting) {
                return;
            }
            m_state = SessionStateUVE::Established;
            m_closeReason = SessionCloseReasonUVE::None;
            m_lastActivityMs = nowMs;
            return;
        }
        case SessionMessageTagUVE::Bye: {
            if (m_state == SessionStateUVE::Disconnected) {
                return;
            }
            TransitionToDisconnectedUVE(SessionCloseReasonUVE::RemoteDisconnect);
            return;
        }
        case SessionMessageTagUVE::Data: {
            if (m_state != SessionStateUVE::Established) {
                return;
            }
            m_inboundData.emplace_back(datagram.begin() + 1, datagram.end());
            m_lastActivityMs = nowMs;
            return;
        }
    }
}

void NetworkSessionUVE::TransitionToDisconnectedUVE(const SessionCloseReasonUVE reason) noexcept {
    m_state = SessionStateUVE::Disconnected;
    m_closeReason = reason;
    m_inboundData.clear();
}

} // namespace UVE::Network
