// Copyright (c) 2026 UniVex Studios. All Rights Reserved.

#include "uve/gameplay/status_effects_uve.h"

#include <cmath>
#include <utility>
#include <vector>

namespace UVE::Scene {
namespace {

[[nodiscard]] bool IsValidIdUVE(const std::string_view id) noexcept {
    return !id.empty() && id.size() <= kMaximumStatusEffectIdBytesUVE;
}

[[nodiscard]] bool IsValidTargetUVE(const std::string_view attributeId) noexcept {
    return !attributeId.empty() && attributeId.size() <= kMaximumGameplayAttributeIdBytesUVE;
}

[[nodiscard]] bool IsUsableUVE(const StatusEffectUVE& effect) noexcept {
    return IsValidIdUVE(effect.effectId) && IsValidTargetUVE(effect.attributeId) &&
           std::isfinite(effect.magnitudePerSecond) && std::isfinite(effect.remainingSeconds) &&
           effect.remainingSeconds >= 0.0F;
}

} // namespace

bool IsStatusEffectsComponentValidUVE(const StatusEffectsComponentUVE& value) noexcept {
    if (value.effects.size() > kMaximumStatusEffectsUVE) {
        return false;
    }
    for (const StatusEffectUVE& effect : value.effects) {
        if (!IsUsableUVE(effect)) {
            return false;
        }
    }
    for (std::size_t first = 0U; first < value.effects.size(); ++first) {
        for (std::size_t second = first + 1U; second < value.effects.size(); ++second) {
            if (value.effects[first].effectId == value.effects[second].effectId) {
                return false;
            }
        }
    }
    return true;
}

bool ApplyStatusEffectUVE(StatusEffectsComponentUVE& effects, std::string effectId, std::string attributeId,
                           const float magnitudePerSecond, const float durationSeconds) {
    if (!IsValidIdUVE(effectId) || !IsValidTargetUVE(attributeId) || !std::isfinite(magnitudePerSecond) ||
        !std::isfinite(durationSeconds) || durationSeconds <= 0.0F) {
        return false;
    }
    for (StatusEffectUVE& held : effects.effects) {
        if (held.effectId == effectId) {
            held.attributeId = std::move(attributeId);
            held.magnitudePerSecond = magnitudePerSecond;
            held.remainingSeconds = durationSeconds;
            return true;
        }
    }
    if (effects.effects.size() >= kMaximumStatusEffectsUVE) {
        return false;
    }
    StatusEffectUVE effect;
    effect.effectId = std::move(effectId);
    effect.attributeId = std::move(attributeId);
    effect.magnitudePerSecond = magnitudePerSecond;
    effect.remainingSeconds = durationSeconds;
    effects.effects.push_back(std::move(effect));
    return true;
}

bool RemoveStatusEffectUVE(StatusEffectsComponentUVE& effects, const std::string_view effectId) {
    const auto removed = std::erase_if(effects.effects,
                                       [effectId](const StatusEffectUVE& held) { return held.effectId == effectId; });
    return removed > 0;
}

std::vector<GameplayAttributeTickResultUVE> TickGameplayAttributesUVE(GameplayAttributesComponentUVE& attributes,
                                                                      StatusEffectsComponentUVE* status,
                                                                      const float deltaSeconds) {
    std::vector<GameplayAttributeTickResultUVE> results;
    if (!std::isfinite(deltaSeconds) || deltaSeconds <= 0.0F) {
        return results;
    }
    if (status != nullptr) {
        for (StatusEffectUVE& effect : status->effects) {
            GameplayAttributeTickResultUVE result;
            result.effectId = effect.effectId;
            result.attributeId = effect.attributeId;
            if (effect.attributeId == kReservedHealthAttributeIdUVE) {
                result.amount = effect.magnitudePerSecond * deltaSeconds;
            } else if (GameplayAttributeUVE* pool = FindGameplayAttributeUVE(attributes, effect.attributeId);
                       pool != nullptr) {
                const float drift = effect.magnitudePerSecond * deltaSeconds;
                if (drift >= 0.0F) {
                    const GameplayAttributeDamageResultUVE damage = DamageGameplayAttributeUVE(*pool, drift);
                    result.amount = damage.amount;
                    result.remaining = damage.remaining;
                    result.applied = damage.applied;
                    result.depleted = damage.depleted;
                } else {
                    const GameplayAttributeDamageResultUVE heal = HealGameplayAttributeUVE(*pool, -drift);
                    result.amount = -heal.amount;
                    result.remaining = heal.remaining;
                    result.applied = heal.applied;
                }
            }
            effect.remainingSeconds -= deltaSeconds;
            result.expired = effect.remainingSeconds <= 0.0F;
            results.push_back(std::move(result));
        }
        std::erase_if(status->effects,
                      [](const StatusEffectUVE& effect) { return effect.remainingSeconds <= 0.0F; });
    }
    for (GameplayAttributeUVE& pool : attributes.attributes) {
        if (pool.regenPerSecond == 0.0F) {
            continue;
        }
        const float drift = pool.regenPerSecond * deltaSeconds;
        GameplayAttributeTickResultUVE result;
        result.attributeId = pool.id;
        if (drift >= 0.0F) {
            const GameplayAttributeDamageResultUVE heal = HealGameplayAttributeUVE(pool, drift);
            result.amount = -heal.amount;
            result.remaining = heal.remaining;
            result.applied = heal.applied;
        } else {
            const GameplayAttributeDamageResultUVE damage = DamageGameplayAttributeUVE(pool, -drift);
            result.amount = damage.amount;
            result.remaining = damage.remaining;
            result.applied = damage.applied;
            result.depleted = damage.depleted;
        }
        results.push_back(std::move(result));
    }
    return results;
}

} // namespace UVE::Scene
