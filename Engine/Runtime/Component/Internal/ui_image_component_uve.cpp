// Copyright (c) 2026 UniVex Studios. All Rights Reserved.

#include "uve/component/ui_image_component_uve.h"

#include <cmath>

namespace UVE::Scene {

[[nodiscard]] bool IsUIImageComponentValidUVE(const UIImageComponentUVE& component) noexcept {
    return Math::IsFiniteUVE(component.rect) &&
           component.rect.size.x >= kMinimumUIImageSizePixelsUVE &&
           component.rect.size.x <= kMaximumUIImageSizePixelsUVE &&
           component.rect.size.y >= kMinimumUIImageSizePixelsUVE &&
           component.rect.size.y <= kMaximumUIImageSizePixelsUVE && std::isfinite(component.tintColor.x) &&
           std::isfinite(component.tintColor.y) && std::isfinite(component.tintColor.z) &&
           std::isfinite(component.alpha) && component.alpha >= 0.0F && component.alpha <= 1.0F;
}

} // namespace UVE::Scene
