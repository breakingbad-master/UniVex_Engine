// Copyright (c) 2026 UniVex Studios. All Rights Reserved.

#include "uve/ui/ui_tween_uve.h"

#include <algorithm>
#include <cmath>

#include "uve/component/ui_button_component_uve.h"
#include "uve/component/ui_image_component_uve.h"
#include "uve/component/ui_layout_container_component_uve.h"
#include "uve/component/ui_progress_bar_component_uve.h"
#include "uve/component/ui_slider_component_uve.h"
#include "uve/component/ui_text_component_uve.h"
#include "uve/entity/i_entity_manager_uve.h"
#include "uve/math/rect_uve.h"
#include "uve/math/scalar_uve.h"

namespace UVE::UI {

float EaseUITweenUVE(const Scene::UITweenEaseUVE ease, const float t) noexcept {
    const float clamped = std::clamp(t, 0.0F, 1.0F);
    switch (ease) {
        case Scene::UITweenEaseUVE::Linear:
            return clamped;
        case Scene::UITweenEaseUVE::SineInOut:
            return 0.5F * (1.0F - std::cos(Math::kPiUVE * clamped));
        case Scene::UITweenEaseUVE::QuadIn:
            return clamped * clamped;
        case Scene::UITweenEaseUVE::QuadOut:
            return 1.0F - (1.0F - clamped) * (1.0F - clamped);
        case Scene::UITweenEaseUVE::QuadInOut:
            return clamped < 0.5F ? 2.0F * clamped * clamped
                                  : 1.0F - (-2.0F * clamped + 2.0F) * (-2.0F * clamped + 2.0F) * 0.5F;
        case Scene::UITweenEaseUVE::CubicIn:
            return clamped * clamped * clamped;
        case Scene::UITweenEaseUVE::CubicOut: {
            const float remaining = 1.0F - clamped;
            return 1.0F - remaining * remaining * remaining;
        }
        case Scene::UITweenEaseUVE::CubicInOut: {
            const float doubled = -2.0F * clamped + 2.0F;
            return clamped < 0.5F ? 4.0F * clamped * clamped * clamped
                                  : 1.0F - doubled * doubled * doubled * 0.5F;
        }
        case Scene::UITweenEaseUVE::OutBack: {
            // The standard 1.70158 overshoot constant: settles at 1 having swung past it.
            constexpr float kBackOvershootUVE = 1.70158F;
            const float past = clamped - 1.0F;
            return 1.0F + (kBackOvershootUVE + 1.0F) * past * past * past + kBackOvershootUVE * past * past;
        }
    }
    return clamped;
}

namespace {

void ApplyTweenedRectUVE(Scene::IEntityManagerUVE& entityManager, const Scene::EntityUVE entity,
                         const Math::RectUVE& rect) {
    // Every positioned component moves together, mirroring the layout pass's own rule: a tweened
    // entity stays in one piece.
    if (entityManager.HasComponentUVE<Scene::UIButtonComponentUVE>(entity)) {
        entityManager.GetComponentUVE<Scene::UIButtonComponentUVE>(entity).rect = rect;
    }
    if (entityManager.HasComponentUVE<Scene::UIImageComponentUVE>(entity)) {
        entityManager.GetComponentUVE<Scene::UIImageComponentUVE>(entity).rect = rect;
    }
    if (entityManager.HasComponentUVE<Scene::UISliderComponentUVE>(entity)) {
        entityManager.GetComponentUVE<Scene::UISliderComponentUVE>(entity).rect = rect;
    }
    if (entityManager.HasComponentUVE<Scene::UIProgressBarComponentUVE>(entity)) {
        entityManager.GetComponentUVE<Scene::UIProgressBarComponentUVE>(entity).rect = rect;
    }
    if (entityManager.HasComponentUVE<Scene::UILayoutContainerComponentUVE>(entity)) {
        entityManager.GetComponentUVE<Scene::UILayoutContainerComponentUVE>(entity).rect = rect;
    }
    if (entityManager.HasComponentUVE<Scene::UITextComponentUVE>(entity)) {
        entityManager.GetComponentUVE<Scene::UITextComponentUVE>(entity).positionPixels = rect.position;
    }
}

} // namespace

void TickUITweensUVE(Scene::IEntityManagerUVE& entityManager, const float dt) {
    const bool advance = dt > 0.0F && std::isfinite(dt);
    entityManager.ForEachUVE<Scene::UITweenComponentUVE>(
        [&entityManager, dt, advance](const Scene::EntityUVE entity, Scene::UITweenComponentUVE& tween) {
            if (!IsUITweenComponentValidUVE(tween)) {
                tween.elapsed = 0.0F;
                tween.playing = false;
                tween.completedThisFrame = false;
                tween.currentAlpha = 1.0F;
                return;
            }
            tween.completedThisFrame = false;
            if (!tween.playing || !advance) {
                return;
            }
            const float previousT = (tween.elapsed - tween.delay) / tween.duration;
            tween.elapsed += dt;
            const float t = (tween.elapsed - tween.delay) / tween.duration;
            float fraction = 0.0F;
            if (tween.loop == Scene::UITweenLoopUVE::Once) {
                if (t >= 1.0F) {
                    fraction = 1.0F;
                    tween.playing = false;
                    tween.completedThisFrame = previousT < 1.0F;
                } else {
                    fraction = std::max(0.0F, t);
                }
            } else {
                if (t >= 0.0F) {
                    const float cycle = std::floor(t);
                    const float cycleFraction = t - cycle;
                    fraction = tween.loop == Scene::UITweenLoopUVE::Loop
                                   ? cycleFraction
                                   : (static_cast<int>(cycle) % 2 == 0 ? cycleFraction : 1.0F - cycleFraction);
                    // A wrap is a completion: cycle 1+ reached past where the previous tick was.
                    // Delay phases (negative previousT) never count - ending a delay starts the
                    // tween, it doesn't complete anything.
                    tween.completedThisFrame = cycle >= 1.0F && cycle > std::floor(previousT);
                }
            }
            const float eased = EaseUITweenUVE(tween.ease, fraction);
            if (tween.target == Scene::UITweenTargetUVE::Rect) {
                const Math::RectUVE easedRect{
                    Math::Vector2UVE{Math::LerpUVE(tween.fromRect.position.x, tween.toRect.position.x, eased),
                                     Math::LerpUVE(tween.fromRect.position.y, tween.toRect.position.y, eased)},
                    Math::Vector2UVE{Math::LerpUVE(tween.fromRect.size.x, tween.toRect.size.x, eased),
                                     Math::LerpUVE(tween.fromRect.size.y, tween.toRect.size.y, eased)}};
                ApplyTweenedRectUVE(entityManager, entity, easedRect);
            } else {
                tween.currentAlpha =
                    std::clamp(Math::LerpUVE(tween.fromAlpha, tween.toAlpha, eased), 0.0F, 1.0F);
            }
        });
}

float TweenedAlphaUVE(Scene::IEntityManagerUVE& entityManager, const Scene::EntityUVE entity,
                      const float baseAlpha) {
    if (!entityManager.IsAliveUVE(entity) ||
        !entityManager.HasComponentUVE<Scene::UITweenComponentUVE>(entity)) {
        return baseAlpha;
    }
    const Scene::UITweenComponentUVE& tween =
        entityManager.GetComponentUVE<Scene::UITweenComponentUVE>(entity);
    if (tween.target != Scene::UITweenTargetUVE::Alpha || !IsUITweenComponentValidUVE(tween)) {
        return baseAlpha;
    }
    return baseAlpha * tween.currentAlpha;
}

} // namespace UVE::UI
