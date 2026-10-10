// Copyright (c) 2026 UniVex Studios. All Rights Reserved.

#pragma once

#include "uve/component/entity_uve.h"

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace UVE::Scene {

// ---- Cinematic timeline --------------------------------------------------------------
//
// A cutscene on a shared clock: event keys that fire gameplay events and camera cuts that switch
// the active camera, all evaluated from one currentTime. The component mixes authored data (keys,
// cuts, duration) with player state (isPlaying, finished, currentTime), the same shape as the
// AnimationSequencer player. Everything here is pure: stepping returns what fired and which camera
// is live, and the engine sync turns that into queued events plus SetActiveCameraUVE.
//
// Firing rules, stated once: keys fire going forward only, in passing order, exclusive of the
// time the playhead sits on (a key exactly under the playhead does not refire every frame).
// Keys at 0.0 fire on Play, not on the first Step. Loop wraps refire the wrapped span. Scrubbing
// and reverse motion never fire - they only move the clock and resolve the camera.

inline constexpr std::size_t kMaximumCinematicEventsUVE = 64U;
inline constexpr std::size_t kMaximumCinematicCutsUVE = 32U;
inline constexpr std::size_t kMaximumCinematicEventIdBytesUVE = 64U;

enum class CinematicLoopModeUVE : std::uint8_t {
    Once = 0,
    Loop,
};

struct CinematicEventKeyUVE final {
    double timeSeconds = 0.0;
    std::string eventId;

    [[nodiscard]] bool operator==(const CinematicEventKeyUVE&) const = default;
};

struct CinematicCameraCutUVE final {
    double timeSeconds = 0.0;
    EntityUVE camera = kInvalidEntityUVE;

    [[nodiscard]] bool operator==(const CinematicCameraCutUVE&) const = default;
};

struct CinematicComponentUVE final {
    double durationSeconds = 0.0;
    std::vector<CinematicEventKeyUVE> events;
    std::vector<CinematicCameraCutUVE> cuts;
    /// Cutscenes stay parked until something presses play; autoplay is opt-in per shot.
    bool autoplay = false;
    float speed = 1.0F;
    CinematicLoopModeUVE loopMode = CinematicLoopModeUVE::Once;
    bool isPlaying = false;
    bool finished = false;
    double currentTimeSeconds = 0.0;

    [[nodiscard]] bool operator==(const CinematicComponentUVE&) const = default;
};

/// Finite positive duration and speed, key times inside [0, duration], usable event ids, valid
/// cut cameras, both lists sorted by time and within their caps.
[[nodiscard]] bool IsCinematicComponentValidUVE(const CinematicComponentUVE& value) noexcept;

/// Inserts an event key keeping time order (equal times keep insertion order). False for a bad
/// id, a time outside [0, duration], or a full track. Times are judged against the duration the
/// component has now; changing the duration afterwards can strand keys past the end.
[[nodiscard]] bool AddCinematicEventUVE(CinematicComponentUVE& cinematic, double timeSeconds,
                                        std::string eventId);

/// Inserts a camera cut keeping time order. False for an invalid camera, a time outside
/// [0, duration], or a full track.
[[nodiscard]] bool AddCinematicCutUVE(CinematicComponentUVE& cinematic, double timeSeconds,
                                      EntityUVE camera);

/// Removes the key/cut at `index`. False when out of range.
[[nodiscard]] bool RemoveCinematicEventUVE(CinematicComponentUVE& cinematic, std::size_t index);
[[nodiscard]] bool RemoveCinematicCutUVE(CinematicComponentUVE& cinematic, std::size_t index);

/// Restarts the clock and fires the keys sitting exactly at 0.0, returning their ids. A
/// non-positive duration starts nothing and fires nothing.
[[nodiscard]] std::vector<std::string> PlayCinematicUVE(CinematicComponentUVE& cinematic) noexcept;

/// Parks the playhead where it is. Play restarts from zero.
void StopCinematicUVE(CinematicComponentUVE& cinematic) noexcept;

struct CinematicStepResultUVE final {
    std::vector<std::string> firedEventIds;
    EntityUVE activeCamera = kInvalidEntityUVE;
    bool justFinished = false;

    [[nodiscard]] bool operator==(const CinematicStepResultUVE&) const = default;
};

/// Advances a playing cinematic by `deltaSeconds` (scaled by speed): collects the fired keys,
/// resolves the live camera cut, and parks at the end for Once (looping wraps instead). Returns
/// nothing and moves nothing when parked, finished, or stepped by a non-positive/non-finite dt.
[[nodiscard]] CinematicStepResultUVE StepCinematicUVE(CinematicComponentUVE& cinematic, float deltaSeconds);

/// The cut live at `timeSeconds` without moving anything: what scrubbing previews.
[[nodiscard]] EntityUVE ActiveCinematicCameraUVE(const CinematicComponentUVE& cinematic,
                                                 double timeSeconds) noexcept;

} // namespace UVE::Scene
