// Copyright (c) 2026 UniVex Studios. All Rights Reserved.

#include "uve/network/network_replication_uve.h"

#include <cstddef>
#include <optional>
#include <utility>
#include <vector>

#include "uve/utilities/binary_buffer_uve.h"

namespace UVE::Network {

NetworkReplicatorUVE::NetworkReplicatorUVE(NetworkSessionUVE session,
                                           const std::uint64_t sendIntervalMs) noexcept
    : m_session(std::move(session)), m_sendIntervalMs(sendIntervalMs) {}

bool NetworkReplicatorUVE::RegisterEntityUVE(const NetworkEntityIdUVE netId, PositionSamplerUVE sampler,
                                             PositionApplierUVE applier) {
    if (sampler == nullptr || applier == nullptr || m_entries.contains(netId)) {
        return false;
    }
    m_entries.emplace(netId, std::make_pair(std::move(sampler), std::move(applier)));
    return true;
}

bool NetworkReplicatorUVE::UnregisterEntityUVE(const NetworkEntityIdUVE netId) noexcept {
    return m_entries.erase(netId) > 0U;
}

void NetworkReplicatorUVE::UpdateUVE(const std::uint64_t nowMs) {
    m_session.UpdateUVE(nowMs);
    for (;;) {
        const std::optional<std::vector<std::uint8_t>> payload = m_session.TryReceiveDataUVE();
        if (!payload.has_value()) {
            break;
        }
        HandleSnapshotUVE(*payload);
    }
    if (m_session.StateUVE() != SessionStateUVE::Established ||
        nowMs - m_lastSendMs < m_sendIntervalMs) {
        return;
    }
    std::vector<std::byte> snapshot;
    std::size_t entryCount = 0U;
    for (const auto& [netId, callbacks] : m_entries) {
        if (entryCount >= kReplicationMaximumEntriesPerSnapshotUVE) {
            break;
        }
        const Math::Vector3UVE position = callbacks.first();
        Utilities::AppendUint32LeUVE(snapshot, netId);
        Utilities::AppendFloatLeUVE(snapshot, position.x);
        Utilities::AppendFloatLeUVE(snapshot, position.y);
        Utilities::AppendFloatLeUVE(snapshot, position.z);
        ++entryCount;
    }
    m_stats.entriesTruncated += m_entries.size() - entryCount;
    std::vector<std::byte> framed;
    Utilities::AppendUint16LeUVE(framed, static_cast<std::uint16_t>(entryCount));
    framed.insert(framed.end(), snapshot.begin(), snapshot.end());
    std::vector<std::uint8_t> datagram;
    datagram.reserve(framed.size());
    for (const std::byte byte : framed) {
        datagram.push_back(static_cast<std::uint8_t>(byte));
    }
    if (!m_session.SendDataUVE(datagram)) {
        return;
    }
    ++m_stats.snapshotsSent;
    m_lastSendMs = nowMs;
}

void NetworkReplicatorUVE::HandleSnapshotUVE(const std::vector<std::uint8_t>& payload) {
    // Strict framing: the byte count must match the declared entry count exactly, or the whole
    // message goes out unapplied - no partial snapshots, no trailing bytes, no exceptions.
    std::vector<std::byte> buffer;
    buffer.reserve(payload.size());
    for (const std::uint8_t byte : payload) {
        buffer.push_back(static_cast<std::byte>(byte));
    }
    std::size_t offset = 0U;
    std::uint16_t entryCount = 0U;
    if (!Utilities::ReadUint16LeFromBufferUVE(buffer, offset, entryCount) ||
        buffer.size() != sizeof(std::uint16_t) +
                                 static_cast<std::size_t>(entryCount) *
                                     (sizeof(NetworkEntityIdUVE) + 3U * sizeof(float))) {
        ++m_stats.messagesIgnoredMalformed;
        return;
    }
    std::vector<std::pair<NetworkEntityIdUVE, Math::Vector3UVE>> parsed;
    parsed.reserve(entryCount);
    for (std::uint16_t i = 0U; i < entryCount; ++i) {
        NetworkEntityIdUVE netId = 0U;
        Math::Vector3UVE position{};
        if (!Utilities::ReadUint32LeFromBufferUVE(buffer, offset, netId) ||
            !Utilities::ReadFloatLeFromBufferUVE(buffer, offset, position.x) ||
            !Utilities::ReadFloatLeFromBufferUVE(buffer, offset, position.y) ||
            !Utilities::ReadFloatLeFromBufferUVE(buffer, offset, position.z)) {
            ++m_stats.messagesIgnoredMalformed;
            return;
        }
        parsed.emplace_back(netId, position);
    }
    ++m_stats.snapshotsReceived;
    for (const auto& [netId, position] : parsed) {
        const auto entry = m_entries.find(netId);
        if (entry == m_entries.end()) {
            ++m_stats.entriesIgnoredUnknown;
            continue;
        }
        entry->second.second(position);
        ++m_stats.entriesApplied;
    }
}

} // namespace UVE::Network
