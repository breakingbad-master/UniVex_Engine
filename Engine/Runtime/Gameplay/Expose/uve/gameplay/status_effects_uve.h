// Copyright (c) 2026 UniVex Studios. All Rights Reserved.

#pragma once

#include "uve/gameplay/gameplay_attributes_uve.h"

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace UVE::Scene {

// ---- Status effects ----------------------------------------------------------------
//
// Temporary forces on attribute pools: poison draining stamina, a regen aura refilling mana.
// Each effect names its target pool and a signed magnitude per second (positive damages,
// negative heals); reapplying an effect refreshes it - effects never stack, by design, so two
// poisons are one poison with a fresh clock.
//
// The tick ages every effect and applies the attribute-targeted ones. Effects aimed at the
// reserved "health" id are computed but NOT applied here: this file never sees a HealthComponent,
// so the engine sync applies those to health (damage only) and queues the health events, the same
// bridge the strike pipeline already uses. Attribute regen drifts silently in the same tick -
// passive drift fires no Damaged events - except that a pool emptied by decay still reports
// Depleted, since "stamina hit zero" is worth reacting to whatever the cause.

inline constexpr std::size_t kMaximumStatusEffectsUVE = 8U;
inline constexpr std::size_t kMaximumStatusEffectIdBytesUVE = 48U;

struct StatusEffectUVE final {
    std::string effectId;
    std::string attributeId;
    float magnitudePerSecond = 0.0F;
    float remainingSeconds = 0.0F;

    [[nodiscard]] bool operator==(const StatusEffectUVE&) const = default;
};

struct StatusEffectsComponentUVE final {
    std::vector<StatusEffectUVE> effects;

    [[nodiscard]] bool operator==(const StatusEffectsComponentUVE&) const = default;
};

/// Bounded count, usable ids, a named target pool, finite magnitude and non-negative time left.
[[nodiscard]] bool IsStatusEffectsComponentValidUVE(const StatusEffectsComponentUVE& value) noexcept;

/// Adds `effectId`, or refreshes it when already present (target, magnitude and clock are all
/// overwritten). False for bad ids, non-finite magnitude, non-positive duration, or a full set.
[[nodiscard]] bool ApplyStatusEffectUVE(StatusEffectsComponentUVE& effects, std::string effectId,
                                        std::string attributeId, float magnitudePerSecond,
                                        float durationSeconds);

/// Removes `effectId`. False when it was never there.
[[nodiscard]] bool RemoveStatusEffectUVE(StatusEffectsComponentUVE& effects, std::string_view effectId);

struct GameplayAttributeTickResultUVE final {
    /// What caused this entry: an effect id, or empty for passive regen drift.
    std::string effectId;
    std::string attributeId;
    /// Signed drift: positive damaged the pool, negative healed it.
    float amount = 0.0F;
    /// Pool remainder after the drift; zero when the pool is unknown (health, missing).
    float remaining = 0.0F;
    /// False for health-targeted effects (the sync applies those) and missing pools.
    bool applied = false;
    bool expired = false;
    bool depleted = false;

    [[nodiscard]] bool operator==(const GameplayAttributeTickResultUVE&) const = default;
};

/// Advances `status` (may be null: pools without effects still regen) and `attributes` by
/// `deltaSeconds`: effects apply first, then regen. Expired effects apply their final tick and are
/// dropped. Returns one entry per effect plus one per drifted pool, for event queueing. A
/// non-positive or non-finite step returns nothing and touches nothing.
[[nodiscard]] std::vector<GameplayAttributeTickResultUVE> TickGameplayAttributesUVE(
    GameplayAttributesComponentUVE& attributes, StatusEffectsComponentUVE* status, float deltaSeconds);

} // namespace UVE::Scene
