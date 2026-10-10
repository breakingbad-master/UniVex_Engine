// Copyright (c) 2026 UniVex Studios. All Rights Reserved.

#pragma once

namespace UVE::Scene {
class IEntityManagerUVE;
}

namespace UVE::UI {

/// Positions every stack container's direct hierarchy children inside the container rect: padding
/// insets all four sides, spacing separates consecutive children along the direction axis, and
/// the cross axis follows the alignment. Children run in siblingOrder (the entity handle breaks
/// ties); containers run shallowest-first so a nested container is positioned by its parent
/// before it positions its own children. Only positions are written - child sizes and the
/// container's own rect are preserved - and an invalid container is skipped entirely.
///
/// A nonzero `wrapAfter` turns the stack into a grid: at most that many items per line along the
/// direction axis, uniform cells sized to the largest participating child, spacing on both axes,
/// and `alignment` resolving within each cell. The grid always starts at the inner edge, and a
/// short line is still spaced as a grid, never packed as a stack.
///
/// Buttons and images lay out at their rect size, nested containers at their own rect size, and
/// text at {0, fontSize} because the font atlas exposes no text measurement yet. A child
/// carrying several positioned components advances the stack by one extent - button first, image
/// second, container third, text last - but every positioned component it has moves to the laid-out
/// position together, so a container with its own background quad stays in one piece. Children
/// without any positioned component are ignored, not spaced. Depth walks reuse the canvas ancestry cap, so a hierarchy cycle degrades
/// to an arbitrary-but-stable container order instead of hanging the frame.
void LayoutUIContainersUVE(Scene::IEntityManagerUVE& entityManager);

} // namespace UVE::UI
