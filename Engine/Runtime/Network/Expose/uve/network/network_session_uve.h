// Copyright (c) 2026 UniVex Studios. All Rights Reserved.
#pragma once
//
// Scope: one client-server session over any transport endpoint (the loopback today, sockets
// later - the session holds the ITransportEndpointUVE seam, so nothing here changes when the
// wire does). Handshake, disconnect, and timeout; exactly one peer per session, and no
// reconnection, keepalive, or multi-peer rooms yet (each is a named follow-up, not a missing
// piece of this one). Time is an explicit monotonic millisecond clock passed in: production
// feeds its own clock, tests drive a fake one, and the module stays dependency-free.
//
#include <cstddef>
#include <cstdint>
#include <deque>
#include <memory>
#include <optional>
#include <vector>

#include "uve/network/network_transport_uve.h"

namespace UVE::Network {

/// Wire protocol version the Hello carries. A server answers a foreign version with Bye rather
/// than ignoring it, so the mismatch surfaces as a remote disconnect instead of a bare timeout.
inline constexpr std::uint16_t kSessionProtocolVersionUVE = 1U;
/// Largest SendData payload: one tag byte plus payload must fit one datagram.
inline constexpr std::size_t kSessionMaximumPayloadBytesUVE = kTransportMaximumDatagramBytesUVE - 1U;
/// Idle silence before a connecting or established session gives up. Zero disables timeouts.
inline constexpr std::uint64_t kDefaultSessionTimeoutMsUVE = 5000U;

/// First byte of every session datagram. Anything else on the wire is ignored, as are empty
/// datagrams and well-tagged datagrams that arrive in the wrong state.
enum class SessionMessageTagUVE : std::uint8_t { Hello = 0x01, Welcome = 0x02, Bye = 0x03, Data = 0x04 };

enum class SessionRoleUVE : std::uint8_t { Client, Server };
enum class SessionStateUVE : std::uint8_t { Disconnected, Connecting, Established };
enum class SessionCloseReasonUVE : std::uint8_t { None, LocalDisconnect, RemoteDisconnect, Timeout };

/// One peer's side of a connection. The pump model is explicit: UpdateUVE drains the endpoint
/// and runs the timeout check, TryReceiveDataUVE only sees what a previous Update queued, and
/// nothing here threads or ticks itself. Move-only (it owns its endpoint); single-threaded like
/// the endpoints it drives.
class NetworkSessionUVE final {
public:
    NetworkSessionUVE(SessionRoleUVE role, std::unique_ptr<ITransportEndpointUVE> endpoint,
                      std::uint64_t timeoutMs = kDefaultSessionTimeoutMsUVE) noexcept;

    /// Client only, from Disconnected: emits Hello and enters Connecting. Returns false (leaving
    /// everything untouched) for a server or an already-live session; returns the Hello's fate
    /// otherwise - a dropped first Hello still transitions, and the timeout is the backstop.
    [[nodiscard]] bool ConnectUVE(std::uint64_t nowMs);
    /// From Connecting/Established: best-effort Bye, queued data cleared, Disconnected with
    /// LocalDisconnect. Returns whether a Bye was emitted (false when already Disconnected, or
    /// when the queue was full - in which case the peer times out instead).
    [[nodiscard]] bool DisconnectUVE();
    /// Drains every pending datagram through the state machine, then times out a silent
    /// Connecting/Established session. Only recognized datagrams refresh the activity clock;
    /// garbage and wrong-state datagrams neither queue nor keep alive. `nowMs` must be monotonic
    /// per session - a backward jump merely delays the timeout, never triggers it early.
    void UpdateUVE(std::uint64_t nowMs);
    /// Queues one tagged payload for the peer. Established only, bounded by
    /// `kSessionMaximumPayloadBytesUVE`; a full transport queue reports false.
    [[nodiscard]] bool SendDataUVE(const std::vector<std::uint8_t>& payload);
    /// Pops the oldest payload a previous Update queued, or empty. Disconnects clear the queue.
    [[nodiscard]] std::optional<std::vector<std::uint8_t>> TryReceiveDataUVE();

    [[nodiscard]] SessionStateUVE StateUVE() const noexcept { return m_state; }
    [[nodiscard]] SessionCloseReasonUVE CloseReasonUVE() const noexcept { return m_closeReason; }

private:
    void HandleDatagramUVE(const std::vector<std::uint8_t>& datagram, std::uint64_t nowMs);
    void TransitionToDisconnectedUVE(SessionCloseReasonUVE reason) noexcept;

    SessionRoleUVE m_role = SessionRoleUVE::Client;
    std::unique_ptr<ITransportEndpointUVE> m_endpoint;
    std::uint64_t m_timeoutMs = kDefaultSessionTimeoutMsUVE;
    SessionStateUVE m_state = SessionStateUVE::Disconnected;
    SessionCloseReasonUVE m_closeReason = SessionCloseReasonUVE::None;
    std::uint64_t m_lastActivityMs = 0U;
    std::deque<std::vector<std::uint8_t>> m_inboundData;
};

} // namespace UVE::Network
