// Copyright (c) 2026 UniVex Studios. All Rights Reserved.
#pragma once
//
// Scope: the replication core - fixed-rate position snapshots over an established session.
// Each side registers the entities it replicates by stable net id with a sampler (read the
// local position) and an applier (write a received one); the replicator frames snapshots as
// little-endian session payloads and applies what arrives. Callbacks are the layering: Network
// never touches the scene graph or the entity manager, so production wires samplers to world
// reads and appliers to SceneGraphUVE writes while tests use plain locals. Symmetric and
// authority-free (both sides send and receive); authority, relevancy, delta compression, and
// non-position properties are named follow-ups, not missing pieces of this one.
//
#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <utility>

#include "uve/math/vector3_uve.h"
#include "uve/network/network_session_uve.h"

namespace UVE::Network {

/// Stable identity shared by both peers; the local-entity mapping lives outside Network.
using NetworkEntityIdUVE = std::uint32_t;
/// Reads the current local position of a registered entity. Called on the send tick.
using PositionSamplerUVE = std::function<Math::Vector3UVE()>;
/// Writes a received position for a registered entity. Called while pumping, in net-id order.
using PositionApplierUVE = std::function<void(const Math::Vector3UVE&)>;

/// Default snapshot cadence: 10 Hz, the genre-standard floor for position replication.
inline constexpr std::uint64_t kDefaultReplicationIntervalMsUVE = 100U;
/// Entries per snapshot: a u16 count plus 16-byte entries must fit one session payload (74).
inline constexpr std::size_t kReplicationMaximumEntriesPerSnapshotUVE =
    (kSessionMaximumPayloadBytesUVE - sizeof(std::uint16_t)) /
    (sizeof(NetworkEntityIdUVE) + 3U * sizeof(float));

struct ReplicationStatsUVE final {
    std::uint64_t snapshotsSent = 0U;
    std::uint64_t snapshotsReceived = 0U;
    std::uint64_t entriesApplied = 0U;
    std::uint64_t entriesIgnoredUnknown = 0U;
    std::uint64_t entriesTruncated = 0U;
    std::uint64_t messagesIgnoredMalformed = 0U;

    [[nodiscard]] bool operator==(const ReplicationStatsUVE&) const noexcept = default;
};

/// Fixed-rate snapshot exchange over one session. Owns the session's pump (Update drives the
/// session first, so handshakes and timeouts flow through the same call), sends while
/// established, and applies whatever well-formed snapshots arrive. Move-only (it owns its
/// session); single-threaded like everything below it.
class NetworkReplicatorUVE final {
public:
    explicit NetworkReplicatorUVE(NetworkSessionUVE session,
                                  std::uint64_t sendIntervalMs = kDefaultReplicationIntervalMsUVE) noexcept;

    /// Registers `netId` for both directions. Returns false (registering nothing) on a duplicate
    /// id or an empty sampler/applier - unregister first to rebind.
    [[nodiscard]] bool RegisterEntityUVE(NetworkEntityIdUVE netId, PositionSamplerUVE sampler,
                                         PositionApplierUVE applier);
    /// Drops `netId`'s registration. Returns false when it was never registered.
    [[nodiscard]] bool UnregisterEntityUVE(NetworkEntityIdUVE netId) noexcept;
    /// Pumps the session, applies every queued snapshot, then sends one when established and due.
    /// A zero interval sends on every update; a full transport queue skips the send and retries
    /// next update without advancing the cadence. Empty registrations still send (an empty
    /// snapshot doubles as a keepalive: inbound traffic refreshes the peer's timeout clock).
    void UpdateUVE(std::uint64_t nowMs);

    /// The owned session: handshake, disconnect, and state live here, not duplicated.
    [[nodiscard]] NetworkSessionUVE& SessionUVE() noexcept { return m_session; }
    [[nodiscard]] const NetworkSessionUVE& SessionUVE() const noexcept { return m_session; }
    [[nodiscard]] ReplicationStatsUVE StatsUVE() const noexcept { return m_stats; }

private:
    void HandleSnapshotUVE(const std::vector<std::uint8_t>& payload);

    NetworkSessionUVE m_session;
    std::uint64_t m_sendIntervalMs = kDefaultReplicationIntervalMsUVE;
    std::uint64_t m_lastSendMs = 0U;
    // Ordered by net id, so an over-full registration truncates deterministically: the lowest
    // ids always win, and the losers count into entriesTruncated every send.
    std::map<NetworkEntityIdUVE, std::pair<PositionSamplerUVE, PositionApplierUVE>> m_entries;
    ReplicationStatsUVE m_stats;
};

} // namespace UVE::Network
