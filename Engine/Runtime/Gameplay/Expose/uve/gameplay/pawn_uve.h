// Copyright (c) 2026 UniVex Studios. All Rights Reserved.

#pragma once

#include <cstdint>

#include "uve/component/entity_uve.h"
#include "uve/entity/i_entity_manager_uve.h"
#include "uve/gameplay/gameplay_input_uve.h"

namespace UVE::Scene {

// ---- Pawns and controllers -------------------------------------------------------------
//
// The possession kernel of the gameplay object model: a Controller drives at most one Pawn, a
// Pawn answers to at most one Controller, and the link is always mutual or absent - never
// one-sided. PossessControllerUVE() steals both sides cleanly (the previous pawn is unlinked
// with its input purged, the previous controller is unlinked), so re-possession at runtime is an
// ordinary call, not surgery.
//
// The polled keyboard/gamepad snapshot routes to the pawns of Player controllers only; AI
// controllers own their pawns' input themselves: brains select action ids, and mapping those
// to input is game code's, never the engine's. Routing also
// self-heals: a link whose other side lost its component is cleared, and an orphaned pawn's
// input is purged, so a destroyed controller can never leave a pawn running on stale input.
//
// Possession changes are caller-known imperative calls, so v1 queues no events. And the
// player-look/interact/character flow resolves its player through possession (a maintained
// self-possession fill keeps possessOnPlay bodies driven). Movement still reads the raw polled
// snapshot rather than pawn input - closing that loop is the next slice, not this one.

enum class ControllerKindUVE : std::uint8_t {
    /// Driven by the polled input snapshot through RouteGameplayInputUVE().
    Player = 0U,
    /// Driven by AI; routing repairs its links but never touches its pawn's input.
    AI = 1U,
};

struct ControllerComponentUVE final {
    EntityUVE pawn = kInvalidEntityUVE;
    ControllerKindUVE kind = ControllerKindUVE::Player;

    [[nodiscard]] bool operator==(const ControllerComponentUVE&) const = default;
};

/// A known controller kind. The pawn link is dynamic, not structural: any value validates.
[[nodiscard]] bool IsControllerComponentValidUVE(const ControllerComponentUVE& value) noexcept;

struct PawnComponentUVE final {
    EntityUVE controller = kInvalidEntityUVE;
    Gameplay::GameplayInputUVE input;

    [[nodiscard]] bool operator==(const PawnComponentUVE&) const = default;
};

/// Finite input numbers. The controller link is dynamic, not structural: any value validates.
[[nodiscard]] bool IsPawnComponentValidUVE(const PawnComponentUVE& value) noexcept;

/// Drives `pawnEntity` from `controllerEntity`, stealing both sides: the controller's previous
/// pawn (if any) is unlinked with its input purged, and the pawn's previous controller (if any)
/// is unlinked. Re-possessing an already-mutual pair is a no-op success. False unless both
/// entities carry their respective components.
[[nodiscard]] bool PossessControllerUVE(IEntityManagerUVE& entityManager, EntityUVE controllerEntity,
                                       EntityUVE pawnEntity);

/// Unlinks `controllerEntity` from its pawn (if any), purging the pawn's input. False when the
/// entity carries no controller or drives nothing.
[[nodiscard]] bool UnpossessControllerUVE(IEntityManagerUVE& entityManager, EntityUVE controllerEntity);

/// The pawn `controllerEntity` mutually drives, or invalid for a missing component, an empty
/// link, a pawn without its component, or a one-sided link. Pure: repairs are Route's job.
[[nodiscard]] EntityUVE FindPossessedPawnUVE(const IEntityManagerUVE& entityManager,
                                             EntityUVE controllerEntity);

/// The controller mutually driving `pawnEntity`, or invalid under the same fail-closed rules.
[[nodiscard]] EntityUVE FindPawnControllerUVE(const IEntityManagerUVE& entityManager, EntityUVE pawnEntity);

/// Delivers `snapshot` to every live pawn of a Player controller, in (index, generation) order,
/// then sweeps every pawn: links whose other side is gone (dead entity, lost component, or
/// never linked back) are
/// cleared, and orphaned pawns have their input purged. A non-finite snapshot still repairs but
/// never writes, so routing can never invalidate a valid pawn.
void RouteGameplayInputUVE(IEntityManagerUVE& entityManager,
                           const Gameplay::GameplayInputUVE& snapshot);

} // namespace UVE::Scene
