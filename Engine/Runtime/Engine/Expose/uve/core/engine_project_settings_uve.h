// Copyright (c) 2026 UniVex Studios. All Rights Reserved.

#pragma once

#include <cstddef>
#include <string>
#include <string_view>

#include "uve/config/settings_document_uve.h"
#include "uve/config/settings_stack_uve.h"
#include "uve/core/engine_config_uve.h"

namespace UVE::Core {

/// Ids of settings registered by the engine. Most are project settings overriding fields of
/// EngineConfigUVE; `headless` is a hidden, command-line-only startup option.
namespace EngineProjectSettingIdUVE {
inline constexpr std::string_view kPhysicsTicksPerSecondUVE = "physics.common.ticksPerSecond";
inline constexpr std::string_view kPhysicsMaxFrameTimeUVE = "physics.common.maxFrameTime";
inline constexpr std::string_view kPhysicsMaxStepsPerFrameUVE = "physics.common.maxStepsPerFrame";
inline constexpr std::string_view kPhysicsGravityUVE = "physics.3d.gravity";
inline constexpr std::string_view kShadowMapResolutionUVE = "rendering.shadows.mapResolution";
inline constexpr std::string_view kShadowFilterUVE = "rendering.shadows.filter";
inline constexpr std::string_view kAutoSaveIntervalUVE = "application.save.autoSaveInterval";
/// The `.uventity` (Content-relative path) a player is spawned from when Play starts. Empty: none.
inline constexpr std::string_view kDefaultPlayerEntityUVE = "game.player.defaultEntity";
inline constexpr std::string_view kHeadlessUVE = "headless";
} // namespace EngineProjectSettingIdUVE

/// The two sets of 32 layers a project names: physics layers (what a collider is on and looks
/// for) and render layers (what a camera, light or decal sees).
enum class LayerSetUVE {
    Physics,
    Render,
};
inline constexpr std::size_t kLayerCountUVE = 32U;

/// The project setting holding the name of layer `index` (0-based, bit `index`) of `set`, e.g.
/// "layers.physics.1" for the first physics layer.
[[nodiscard]] std::string GetLayerNameSettingIdUVE(LayerSetUVE set, std::size_t index);
/// The name `document` gives layer `index` of `set`; empty when the project has not named it or
/// `index` is out of range.
[[nodiscard]] std::string GetLayerNameUVE(const Config::SettingsDocumentUVE& document, LayerSetUVE set,
                                          std::size_t index);

/// Declares engine configuration settings and project settings in `registry`. Defaults match
/// EngineConfigUVE's defaults; startup settings are RestartRequired. Shadow resolution/filter are
/// PerPlatform, and the hidden `headless` option is NotPersisted and command-line-only. Layer names
/// are read wherever they are shown. False if any declaration is refused - a programming error a
/// test catches.
[[nodiscard]] bool RegisterEngineProjectSettingsUVE(Config::SettingsRegistryUVE& registry);
/// Whether `id` is one of the registered settings that directly overrides an EngineConfigUVE field.
[[nodiscard]] bool IsEngineConfigSettingIdUVE(std::string_view id);

/// Applies every non-default value resolved by `settings` to the matching field of `config`.
/// EngineConfigUVE remains the caller's base configuration when no project/user/platform/command-line
/// store supplies an effective value.
void ApplyEngineSettingsUVE(const Config::SettingsStackUVE& settings, EngineConfigUVE& config);
/// Compatibility convenience for a project-only stack: copies every setting `document` stores into
/// its matching field, leaving application-selected fields alone when the project does not set them.
void ApplyEngineProjectSettingsUVE(const Config::SettingsDocumentUVE& document, EngineConfigUVE& config);

} // namespace UVE::Core
