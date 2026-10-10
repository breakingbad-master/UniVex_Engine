// Copyright (c) 2026 UniVex Studios. All Rights Reserved.

#include "uve/ui/ui_runtime_uve.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

#include "uve/component/ui_button_component_uve.h"
#include "uve/component/ui_image_component_uve.h"
#include "uve/component/ui_progress_bar_component_uve.h"
#include "uve/component/ui_slider_component_uve.h"
#include "uve/component/ui_text_component_uve.h"
#include "uve/input/mouse_button_uve.h"
#include "uve/ui/canvas_ancestry_uve.h"
#include "uve/ui/ui_anchors_uve.h"
#include "uve/ui/ui_layout_uve.h"
#include "uve/ui/ui_tween_uve.h"

namespace UVE::UI {

namespace {

struct RankedWidgetUVE final {
    std::int32_t sortOrder = 0;
    bool hasCanvas = false;
    std::uint32_t canvasIndex = 0;
    std::uint32_t canvasGeneration = 0;
    std::uint8_t layer = 0;
    std::uint32_t entityIndex = 0;
    std::uint32_t entityGeneration = 0;
    std::vector<UIQuadUVE> quads;
};

void RankWidgetUVE(const Scene::EntityUVE entity, const CanvasAncestryUVE& ancestry, const std::uint8_t layer,
                   std::vector<UIQuadUVE> quads, std::vector<RankedWidgetUVE>& ranked) {
    RankedWidgetUVE rankedWidget;
    rankedWidget.sortOrder = ancestry.sortOrder;
    rankedWidget.hasCanvas = ancestry.hasCanvas;
    rankedWidget.canvasIndex = ancestry.canvas.index;
    rankedWidget.canvasGeneration = ancestry.canvas.generation;
    rankedWidget.layer = layer;
    rankedWidget.entityIndex = entity.index;
    rankedWidget.entityGeneration = entity.generation;
    rankedWidget.quads = std::move(quads);
    ranked.push_back(std::move(rankedWidget));
}

} // namespace

void UIRuntimeUVE::SetCoordinateTransformUVE(const UICoordinateTransformUVE& transform) noexcept {
    if (!std::isfinite(transform.scaleX) || !std::isfinite(transform.scaleY) || transform.scaleX <= 0.0F ||
        transform.scaleY <= 0.0F || !std::isfinite(transform.offsetX) || !std::isfinite(transform.offsetY) ||
        !std::isfinite(transform.inputScaleX) || !std::isfinite(transform.inputScaleY) ||
        transform.inputScaleX <= 0.0F || transform.inputScaleY <= 0.0F ||
        !std::isfinite(transform.inputOffsetX) || !std::isfinite(transform.inputOffsetY)) {
        m_coordinateTransform = UICoordinateTransformUVE{};
        return;
    }
    m_coordinateTransform = transform;
}

void UIRuntimeUVE::SetViewportSizeUVE(const Math::Vector2UVE& viewportSize) noexcept {
    if (!std::isfinite(viewportSize.x) || !std::isfinite(viewportSize.y) || viewportSize.x < 0.0F ||
        viewportSize.y < 0.0F) {
        return;
    }
    m_viewportSize = viewportSize;
}

void UIRuntimeUVE::SetDeltaTimeUVE(const float deltaTime) noexcept {
    if (!std::isfinite(deltaTime) || deltaTime < 0.0F) {
        return;
    }
    m_deltaTime = deltaTime;
}

void UIRuntimeUVE::TickUVE(Scene::IEntityManagerUVE& entityManager, const Input::IInputSystemUVE& inputSystem,
                           const UITextLocalizationUVE& localization) {
    // Anchors resolve first (roots against the viewport, children against fresh parent rects),
    // then containers auto-size to content and position their children, then active tweens
    // override the resting arrangement: hit-testing and every emitted quad agree on where a
    // widget is within the same tick.
    ResolveUIAnchorsUVE(entityManager, m_viewportSize);
    LayoutUIContainersUVE(entityManager, m_fontAtlas);
    TickUITweensUVE(entityManager, m_deltaTime);
    m_drawBatch.quads.clear();
    std::vector<RankedWidgetUVE> ranked;

    entityManager.ForEachUVE<Scene::UIImageComponentUVE>(
        [&entityManager, &ranked](const Scene::EntityUVE entity, const Scene::UIImageComponentUVE& image) {
            if (!ShouldDrawUiWidgetUVE(entityManager, entity)) {
                return;
            }
            UIQuadUVE quad{};
            quad.rect = image.rect;
            quad.color = image.tintColor;
            quad.alpha = TweenedAlphaUVE(entityManager, entity, image.alpha);
            quad.kind = image.textureAssetGuid.value == 0U ? UIDrawItemKindUVE::SolidColor : UIDrawItemKindUVE::Image;
            quad.imageAssetGuid = image.textureAssetGuid;
            RankWidgetUVE(entity, ResolveCanvasAncestryUVE(entityManager, entity), 0, {quad}, ranked);
        });

    entityManager.ForEachUVE<Scene::UIProgressBarComponentUVE>(
        [&entityManager, &ranked](const Scene::EntityUVE entity, const Scene::UIProgressBarComponentUVE& bar) {
            if (!ShouldDrawUiWidgetUVE(entityManager, entity) || !IsUIProgressBarComponentValidUVE(bar)) {
                return;
            }
            const float fraction =
                bar.maxValue > bar.minValue
                    ? std::clamp((bar.value - bar.minValue) / (bar.maxValue - bar.minValue), 0.0F, 1.0F)
                    : (bar.value >= bar.maxValue ? 1.0F : 0.0F);
            UIQuadUVE background{};
            background.rect = bar.rect;
            background.color = bar.backgroundColor;
            background.alpha = TweenedAlphaUVE(entityManager, entity, 1.0F);
            background.kind = UIDrawItemKindUVE::SolidColor;
            UIQuadUVE fill{};
            fill.rect = Math::RectUVE{bar.rect.position,
                                      Math::Vector2UVE{bar.rect.size.x * fraction, bar.rect.size.y}};
            fill.color = bar.fillColor;
            fill.alpha = TweenedAlphaUVE(entityManager, entity, 1.0F);
            fill.kind = UIDrawItemKindUVE::SolidColor;
            std::vector<UIQuadUVE> barQuads;
            barQuads.push_back(background);
            barQuads.push_back(fill);
            RankWidgetUVE(entity, ResolveCanvasAncestryUVE(entityManager, entity), 0, std::move(barQuads), ranked);
        });

    const Math::Vector2UVE rawMousePosition = inputSystem.GetMousePositionUVE();
    const Math::Vector2UVE mousePosition{
        (rawMousePosition.x * m_coordinateTransform.inputScaleX - m_coordinateTransform.inputOffsetX -
         m_coordinateTransform.offsetX) /
            m_coordinateTransform.scaleX,
        (rawMousePosition.y * m_coordinateTransform.inputScaleY - m_coordinateTransform.inputOffsetY -
         m_coordinateTransform.offsetY) /
            m_coordinateTransform.scaleY};
    const bool mouseDown = inputSystem.IsMouseButtonDownUVE(Input::MouseButtonUVE::Left);
    const bool mousePressedThisFrame = inputSystem.WasMouseButtonPressedThisFrameUVE(Input::MouseButtonUVE::Left);
    entityManager.ForEachUVE<Scene::UIButtonComponentUVE>(
        [&entityManager, &ranked, &mousePosition, mouseDown, mousePressedThisFrame](
            const Scene::EntityUVE entity, Scene::UIButtonComponentUVE& button) {
            if (!ShouldDrawUiWidgetUVE(entityManager, entity)) {
                button.isHovered = false;
                button.wasClickedThisFrame = false;
                return;
            }
            button.isHovered = Math::ContainsUVE(button.rect, mousePosition);
            button.wasClickedThisFrame = button.isHovered && mousePressedThisFrame;

            UIQuadUVE quad{};
            quad.rect = button.rect;
            quad.color = button.isHovered ? (mouseDown ? button.pressedColor : button.hoverColor) : button.normalColor;
            quad.alpha = TweenedAlphaUVE(entityManager, entity, 1.0F);
            quad.kind = UIDrawItemKindUVE::SolidColor;
            RankWidgetUVE(entity, ResolveCanvasAncestryUVE(entityManager, entity), 1, {quad}, ranked);
        });

    entityManager.ForEachUVE<Scene::UISliderComponentUVE>(
        [&entityManager, &ranked, &mousePosition, mouseDown, mousePressedThisFrame](
            const Scene::EntityUVE entity, Scene::UISliderComponentUVE& slider) {
            if (!ShouldDrawUiWidgetUVE(entityManager, entity) || !IsUISliderComponentValidUVE(slider)) {
                slider.isHovered = false;
                slider.isDragging = false;
                slider.wasChangedThisFrame = false;
                return;
            }
            slider.isHovered = Math::ContainsUVE(slider.rect, mousePosition);
            slider.wasChangedThisFrame = false;
            if (slider.isDragging && !mouseDown) {
                slider.isDragging = false;
            }
            if (mousePressedThisFrame && slider.isHovered) {
                slider.isDragging = true;
            }
            // The thumb never leaves the track: the pointer maps to the thumb center across the
            // travel, clamps past the ends, and snaps to `step` when one is set.
            const float travel = std::max(0.0F, slider.rect.size.x - slider.thumbWidth);
            if (slider.isDragging && travel > 0.0F) {
                const float pointerFraction =
                    (mousePosition.x - slider.rect.position.x - 0.5F * slider.thumbWidth) / travel;
                float dragged = slider.minValue + std::clamp(pointerFraction, 0.0F, 1.0F) *
                                                     (slider.maxValue - slider.minValue);
                if (slider.step > 0.0F) {
                    dragged =
                        slider.minValue + std::round((dragged - slider.minValue) / slider.step) * slider.step;
                    dragged = std::clamp(dragged, slider.minValue, slider.maxValue);
                }
                if (dragged != slider.value) {
                    slider.value = dragged;
                    slider.wasChangedThisFrame = true;
                }
            }
            const float drawnFraction =
                slider.maxValue > slider.minValue
                    ? std::clamp((slider.value - slider.minValue) / (slider.maxValue - slider.minValue), 0.0F,
                                 1.0F)
                    : (slider.value >= slider.maxValue ? 1.0F : 0.0F);
            const float thumbX = slider.rect.position.x + drawnFraction * travel;
            UIQuadUVE track{};
            track.rect = slider.rect;
            track.color = slider.trackColor;
            track.alpha = TweenedAlphaUVE(entityManager, entity, 1.0F);
            track.kind = UIDrawItemKindUVE::SolidColor;
            UIQuadUVE fill{};
            fill.rect = Math::RectUVE{slider.rect.position,
                                      Math::Vector2UVE{thumbX + 0.5F * slider.thumbWidth - slider.rect.position.x,
                                                       slider.rect.size.y}};
            fill.color = slider.fillColor;
            fill.alpha = TweenedAlphaUVE(entityManager, entity, 1.0F);
            fill.kind = UIDrawItemKindUVE::SolidColor;
            UIQuadUVE thumb{};
            thumb.rect = Math::RectUVE{Math::Vector2UVE{thumbX, slider.rect.position.y},
                                       Math::Vector2UVE{slider.thumbWidth, slider.rect.size.y}};
            thumb.color = slider.thumbColor;
            thumb.alpha = TweenedAlphaUVE(entityManager, entity, 1.0F);
            thumb.kind = UIDrawItemKindUVE::SolidColor;
            std::vector<UIQuadUVE> sliderQuads;
            sliderQuads.push_back(track);
            sliderQuads.push_back(fill);
            sliderQuads.push_back(thumb);
            RankWidgetUVE(entity, ResolveCanvasAncestryUVE(entityManager, entity), 1, std::move(sliderQuads), ranked);
        });

    std::vector<UIGlyphQuadUVE> glyphQuads;
    std::string translated;
    entityManager.ForEachUVE<Scene::UITextComponentUVE>(
        [this, &entityManager, &ranked, &glyphQuads, &localization, &translated](const Scene::EntityUVE entity,
                                                                                const Scene::UITextComponentUVE& text) {
            if (!ShouldDrawUiWidgetUVE(entityManager, entity)) {
                return;
            }
            glyphQuads.clear();
            const std::string* shown = &text.text;
            if (localization.service != nullptr &&
                (!localization.isAutoTranslated || localization.isAutoTranslated(entity))) {
                translated = localization.service->TranslateUVE(text.text);
                shown = &translated;
            }
            float cursorX = text.positionPixels.x;
            float cursorY = text.positionPixels.y + text.fontSize;
            m_fontAtlas.AppendTextQuadsUVE(*shown, cursorX, cursorY, text.fontSize, glyphQuads);
            std::vector<UIQuadUVE> quads;
            quads.reserve(glyphQuads.size());
            for (const UIGlyphQuadUVE& glyphQuad : glyphQuads) {
                UIQuadUVE quad{};
                quad.rect = Math::RectUVE{Math::Vector2UVE{glyphQuad.x0, glyphQuad.y0},
                                           Math::Vector2UVE{glyphQuad.x1 - glyphQuad.x0, glyphQuad.y1 - glyphQuad.y0}};
                quad.u0 = glyphQuad.u0;
                quad.v0 = glyphQuad.v0;
                quad.u1 = glyphQuad.u1;
                quad.v1 = glyphQuad.v1;
                quad.color = text.color;
                quad.alpha = TweenedAlphaUVE(entityManager, entity, text.alpha);
                quad.kind = UIDrawItemKindUVE::Glyph;
                quads.push_back(quad);
            }
            RankWidgetUVE(entity, ResolveCanvasAncestryUVE(entityManager, entity), 2, std::move(quads), ranked);
        });

    std::sort(ranked.begin(), ranked.end(), [](const RankedWidgetUVE& lhs, const RankedWidgetUVE& rhs) {
        if (lhs.sortOrder != rhs.sortOrder) {
            return lhs.sortOrder < rhs.sortOrder;
        }
        if (lhs.hasCanvas != rhs.hasCanvas) {
            return !lhs.hasCanvas && rhs.hasCanvas;
        }
        if (lhs.canvasIndex != rhs.canvasIndex) {
            return lhs.canvasIndex < rhs.canvasIndex;
        }
        if (lhs.canvasGeneration != rhs.canvasGeneration) {
            return lhs.canvasGeneration < rhs.canvasGeneration;
        }
        if (lhs.layer != rhs.layer) {
            return lhs.layer < rhs.layer;
        }
        if (lhs.entityIndex != rhs.entityIndex) {
            return lhs.entityIndex < rhs.entityIndex;
        }
        return lhs.entityGeneration < rhs.entityGeneration;
    });
    for (const RankedWidgetUVE& widget : ranked) {
        m_drawBatch.quads.insert(m_drawBatch.quads.end(), widget.quads.begin(), widget.quads.end());
    }
    for (UIQuadUVE& quad : m_drawBatch.quads) {
        quad.rect = Math::TransformUVE(quad.rect,
                                       Math::Vector2UVE{m_coordinateTransform.scaleX, m_coordinateTransform.scaleY},
                                       Math::Vector2UVE{m_coordinateTransform.offsetX, m_coordinateTransform.offsetY});
    }
}

} // namespace UVE::UI
