// Copyright (c) 2026 UniVex Studios. All Rights Reserved.

#include "uve/ui/ui_layout_uve.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "uve/component/entity_uve.h"
#include "uve/component/hierarchy_component_uve.h"
#include "uve/component/ui_button_component_uve.h"
#include "uve/component/ui_image_component_uve.h"
#include "uve/component/ui_layout_container_component_uve.h"
#include "uve/component/ui_text_component_uve.h"
#include "uve/entity/i_entity_manager_uve.h"
#include "uve/math/vector2_uve.h"
#include "uve/ui/canvas_ancestry_uve.h"

namespace UVE::UI {

namespace {

struct OrderedContainerUVE final {
    Scene::EntityUVE entity = Scene::kInvalidEntityUVE;
    std::size_t depth = 0U;
};

struct OrderedChildUVE final {
    Scene::EntityUVE entity = Scene::kInvalidEntityUVE;
    std::int64_t siblingOrder = 0;
};

[[nodiscard]] bool IsEarlierChildUVE(const OrderedChildUVE& lhs, const OrderedChildUVE& rhs) {
    if (lhs.siblingOrder != rhs.siblingOrder) {
        return lhs.siblingOrder < rhs.siblingOrder;
    }
    if (lhs.entity.index != rhs.entity.index) {
        return lhs.entity.index < rhs.entity.index;
    }
    return lhs.entity.generation < rhs.entity.generation;
}

struct PositionableChildUVE final {
    Scene::EntityUVE entity = Scene::kInvalidEntityUVE;
    Math::Vector2UVE extent{};
    bool isButton = false;
    bool isImage = false;
    bool isContainer = false;
    bool isText = false;
};

void MoveChildUVE(Scene::IEntityManagerUVE& entityManager, const PositionableChildUVE& child,
                  const Math::Vector2UVE position) {
    if (child.isButton) {
        entityManager.GetComponentUVE<Scene::UIButtonComponentUVE>(child.entity).rect.position = position;
    }
    if (child.isImage) {
        entityManager.GetComponentUVE<Scene::UIImageComponentUVE>(child.entity).rect.position = position;
    }
    if (child.isContainer) {
        entityManager.GetComponentUVE<Scene::UILayoutContainerComponentUVE>(child.entity).rect.position = position;
    }
    if (child.isText) {
        entityManager.GetComponentUVE<Scene::UITextComponentUVE>(child.entity).positionPixels = position;
    }
}

void LayoutContainerUVE(Scene::IEntityManagerUVE& entityManager, const Scene::EntityUVE container,
                        const std::vector<OrderedChildUVE>& children) {
    const Scene::UILayoutContainerComponentUVE layout =
        entityManager.GetComponentUVE<Scene::UILayoutContainerComponentUVE>(container);
    if (!IsUILayoutContainerComponentValidUVE(layout)) {
        return;
    }
    const bool vertical = layout.direction == Scene::UILayoutDirectionUVE::Vertical;
    const float innerMinX = layout.rect.position.x + layout.padding;
    const float innerMinY = layout.rect.position.y + layout.padding;
    const float innerWidth = std::max(0.0F, layout.rect.size.x - 2.0F * layout.padding);
    const float innerHeight = std::max(0.0F, layout.rect.size.y - 2.0F * layout.padding);
    const float alignFactor = layout.alignment == Scene::UILayoutAlignmentUVE::Start
                                  ? 0.0F
                                  : (layout.alignment == Scene::UILayoutAlignmentUVE::Center ? 0.5F : 1.0F);

    std::vector<PositionableChildUVE> items;
    for (const OrderedChildUVE& child : children) {
        PositionableChildUVE item;
        item.entity = child.entity;
        item.isButton = entityManager.HasComponentUVE<Scene::UIButtonComponentUVE>(child.entity);
        item.isImage = entityManager.HasComponentUVE<Scene::UIImageComponentUVE>(child.entity);
        item.isContainer = entityManager.HasComponentUVE<Scene::UILayoutContainerComponentUVE>(child.entity);
        item.isText = entityManager.HasComponentUVE<Scene::UITextComponentUVE>(child.entity);
        if (!item.isButton && !item.isImage && !item.isContainer && !item.isText) {
            continue; // nothing positionable: ignored, not even spaced
        }
        if (item.isButton) {
            item.extent = entityManager.GetComponentUVE<Scene::UIButtonComponentUVE>(child.entity).rect.size;
        } else if (item.isImage) {
            item.extent = entityManager.GetComponentUVE<Scene::UIImageComponentUVE>(child.entity).rect.size;
        } else if (item.isContainer) {
            item.extent =
                entityManager.GetComponentUVE<Scene::UILayoutContainerComponentUVE>(child.entity).rect.size;
        } else {
            item.extent = Math::Vector2UVE{0.0F,
                                           entityManager.GetComponentUVE<Scene::UITextComponentUVE>(child.entity)
                                               .fontSize};
        }
        items.push_back(item);
    }

    if (layout.wrapAfter != 0U) {
        Math::Vector2UVE cell{};
        for (const PositionableChildUVE& item : items) {
            cell.x = std::max(cell.x, item.extent.x);
            cell.y = std::max(cell.y, item.extent.y);
        }
        const float cellMain = vertical ? cell.y : cell.x;
        const float cellCross = vertical ? cell.x : cell.y;
        for (std::size_t index = 0U; index < items.size(); ++index) {
            const std::size_t line = index / layout.wrapAfter;
            const std::size_t slot = index % layout.wrapAfter;
            const float mainPos = (vertical ? innerMinY : innerMinX) +
                                  static_cast<float>(slot) * (cellMain + layout.spacing);
            const float crossBase = (vertical ? innerMinX : innerMinY) +
                                    static_cast<float>(line) * (cellCross + layout.spacing);
            const float childCross = vertical ? items[index].extent.x : items[index].extent.y;
            // Within the cell now, not the inner rect: a child larger than its cell clamps to the
            // cell's edge instead of spilling into the next one.
            const float crossOffset = std::max(0.0F, alignFactor * (cellCross - childCross));
            const Math::Vector2UVE position =
                vertical ? Math::Vector2UVE{crossBase + crossOffset, mainPos}
                         : Math::Vector2UVE{mainPos, crossBase + crossOffset};
            MoveChildUVE(entityManager, items[index], position);
        }
        return;
    }

    float cursor = 0.0F;
    bool first = true;
    for (const PositionableChildUVE& item : items) {
        if (!first) {
            cursor += layout.spacing;
        }
        first = false;
        const float mainExtent = vertical ? item.extent.y : item.extent.x;
        const float crossInner = vertical ? innerWidth : innerHeight;
        const float crossExtent = vertical ? item.extent.x : item.extent.y;
        // A child larger than the inner rect clamps to the inner edge instead of spilling past it.
        const float crossOffset = std::max(0.0F, alignFactor * (crossInner - crossExtent));
        const Math::Vector2UVE position = vertical ? Math::Vector2UVE{innerMinX + crossOffset, innerMinY + cursor}
                                                   : Math::Vector2UVE{innerMinX + cursor, innerMinY + crossOffset};
        MoveChildUVE(entityManager, item, position);
        cursor += mainExtent;
    }
}

} // namespace

void LayoutUIContainersUVE(Scene::IEntityManagerUVE& entityManager) {
    std::vector<OrderedContainerUVE> containers;
    entityManager.ForEachUVE<Scene::UILayoutContainerComponentUVE>(
        [&entityManager, &containers](const Scene::EntityUVE entity, const Scene::UILayoutContainerComponentUVE&) {
            containers.push_back(OrderedContainerUVE{entity, UIHierarchyDepthUVE(entityManager, entity)});
        });
    std::sort(containers.begin(), containers.end(), [](const OrderedContainerUVE& lhs, const OrderedContainerUVE& rhs) {
        if (lhs.depth != rhs.depth) {
            return lhs.depth < rhs.depth;
        }
        if (lhs.entity.index != rhs.entity.index) {
            return lhs.entity.index < rhs.entity.index;
        }
        return lhs.entity.generation < rhs.entity.generation;
    });

    std::vector<OrderedChildUVE> candidates;
    entityManager.ForEachUVE<Scene::HierarchyComponentUVE>(
        [&candidates](const Scene::EntityUVE entity, const Scene::HierarchyComponentUVE& hierarchy) {
            candidates.push_back(OrderedChildUVE{entity, hierarchy.siblingOrder});
        });
    // One hierarchy scan shared by every container; each container filters its direct children.
    for (const OrderedContainerUVE& container : containers) {
        if (!entityManager.IsAliveUVE(container.entity) ||
            !entityManager.HasComponentUVE<Scene::UILayoutContainerComponentUVE>(container.entity)) {
            continue;
        }
        std::vector<OrderedChildUVE> children;
        for (const OrderedChildUVE& candidate : candidates) {
            if (entityManager.GetComponentUVE<Scene::HierarchyComponentUVE>(candidate.entity).parent ==
                container.entity) {
                children.push_back(candidate);
            }
        }
        std::sort(children.begin(), children.end(), IsEarlierChildUVE);
        LayoutContainerUVE(entityManager, container.entity, children);
    }
}

} // namespace UVE::UI
