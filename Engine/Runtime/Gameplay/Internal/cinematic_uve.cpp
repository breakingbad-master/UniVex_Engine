// Copyright (c) 2026 UniVex Studios. All Rights Reserved.

#include "uve/gameplay/cinematic_uve.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace UVE::Scene {
namespace {

[[nodiscard]] bool IsValidIdUVE(const std::string_view id) noexcept {
    return !id.empty() && id.size() <= kMaximumCinematicEventIdBytesUVE;
}

[[nodiscard]] bool IsValidKeyTimeUVE(const double timeSeconds, const double durationSeconds) noexcept {
    return std::isfinite(timeSeconds) && timeSeconds >= 0.0 && timeSeconds <= durationSeconds;
}

template <typename KeyUVE>
[[nodiscard]] bool IsTimeOrderedUVE(const std::vector<KeyUVE>& keys, const double durationSeconds) noexcept {
    double previous = 0.0;
    bool first = true;
    for (const KeyUVE& key : keys) {
        if (!IsValidKeyTimeUVE(key.timeSeconds, durationSeconds)) {
            return false;
        }
        if (!first && key.timeSeconds < previous) {
            return false;
        }
        previous = key.timeSeconds;
        first = false;
    }
    return true;
}

[[nodiscard]] EntityUVE CameraCutAtUVE(const std::vector<CinematicCameraCutUVE>& cuts,
                                       const double timeSeconds) noexcept {
    EntityUVE camera = kInvalidEntityUVE;
    for (const CinematicCameraCutUVE& cut : cuts) {
        if (cut.timeSeconds > timeSeconds) {
            break;
        }
        camera = cut.camera;
    }
    return camera;
}

void CollectForwardUVE(const std::vector<CinematicEventKeyUVE>& events, const double fromExclusive,
                       const double toInclusive, std::vector<std::string>& out) {
    for (const CinematicEventKeyUVE& key : events) {
        if (key.timeSeconds > fromExclusive && key.timeSeconds <= toInclusive) {
            out.push_back(key.eventId);
        }
    }
}

} // namespace

bool IsCinematicComponentValidUVE(const CinematicComponentUVE& value) noexcept {
    if (!std::isfinite(value.durationSeconds) || value.durationSeconds <= 0.0 ||
        !std::isfinite(value.speed) || !std::isfinite(value.currentTimeSeconds) ||
        value.currentTimeSeconds < 0.0 || value.currentTimeSeconds > value.durationSeconds ||
        value.events.size() > kMaximumCinematicEventsUVE || value.cuts.size() > kMaximumCinematicCutsUVE ||
        value.cameraKeys.size() > kMaximumCinematicCameraKeysUVE) {
        return false;
    }
    for (const CinematicEventKeyUVE& key : value.events) {
        if (!IsValidIdUVE(key.eventId)) {
            return false;
        }
    }
    for (const CinematicCameraCutUVE& cut : value.cuts) {
        if (cut.camera == kInvalidEntityUVE) {
            return false;
        }
    }
    for (const CinematicCameraKeyUVE& key : value.cameraKeys) {
        if (!std::isfinite(key.position.x) || !std::isfinite(key.position.y) ||
            !std::isfinite(key.position.z) || !std::isfinite(key.rotation.x) ||
            !std::isfinite(key.rotation.y) || !std::isfinite(key.rotation.z) ||
            !std::isfinite(key.rotation.w) ||
            std::abs(Math::LengthSquaredUVE(key.rotation) - 1.0F) > 1e-4F) {
            return false;
        }
    }
    return IsTimeOrderedUVE(value.events, value.durationSeconds) &&
           IsTimeOrderedUVE(value.cuts, value.durationSeconds) &&
           IsTimeOrderedUVE(value.cameraKeys, value.durationSeconds);
}

bool AddCinematicEventUVE(CinematicComponentUVE& cinematic, const double timeSeconds, std::string eventId) {
    if (!IsValidIdUVE(eventId) || !IsValidKeyTimeUVE(timeSeconds, cinematic.durationSeconds) ||
        cinematic.events.size() >= kMaximumCinematicEventsUVE) {
        return false;
    }
    CinematicEventKeyUVE key;
    key.timeSeconds = timeSeconds;
    key.eventId = std::move(eventId);
    const auto slot =
        std::upper_bound(cinematic.events.begin(), cinematic.events.end(), timeSeconds,
                         [](const double time, const CinematicEventKeyUVE& existing) { return time < existing.timeSeconds; });
    cinematic.events.insert(slot, std::move(key));
    return true;
}

bool AddCinematicCutUVE(CinematicComponentUVE& cinematic, const double timeSeconds, const EntityUVE camera) {
    if (camera == kInvalidEntityUVE || !IsValidKeyTimeUVE(timeSeconds, cinematic.durationSeconds) ||
        cinematic.cuts.size() >= kMaximumCinematicCutsUVE) {
        return false;
    }
    CinematicCameraCutUVE cut;
    cut.timeSeconds = timeSeconds;
    cut.camera = camera;
    const auto slot =
        std::upper_bound(cinematic.cuts.begin(), cinematic.cuts.end(), timeSeconds,
                         [](const double time, const CinematicCameraCutUVE& existing) { return time < existing.timeSeconds; });
    cinematic.cuts.insert(slot, std::move(cut));
    return true;
}

bool RemoveCinematicEventUVE(CinematicComponentUVE& cinematic, const std::size_t index) {
    if (index >= cinematic.events.size()) {
        return false;
    }
    cinematic.events.erase(cinematic.events.begin() + static_cast<std::ptrdiff_t>(index));
    return true;
}

bool RemoveCinematicCutUVE(CinematicComponentUVE& cinematic, const std::size_t index) {
    if (index >= cinematic.cuts.size()) {
        return false;
    }
    cinematic.cuts.erase(cinematic.cuts.begin() + static_cast<std::ptrdiff_t>(index));
    return true;
}

bool AddCinematicCameraKeyUVE(CinematicComponentUVE& cinematic, const double timeSeconds,
                              const Math::Vector3UVE position, const Math::QuaternionUVE rotation) {
    Math::QuaternionUVE normalized{};
    if (!std::isfinite(position.x) || !std::isfinite(position.y) || !std::isfinite(position.z) ||
        !Math::TryNormalizeUVE(rotation, normalized) ||
        !IsValidKeyTimeUVE(timeSeconds, cinematic.durationSeconds) ||
        cinematic.cameraKeys.size() >= kMaximumCinematicCameraKeysUVE) {
        return false;
    }
    CinematicCameraKeyUVE key;
    key.timeSeconds = timeSeconds;
    key.position = position;
    key.rotation = normalized;
    const auto slot = std::upper_bound(
        cinematic.cameraKeys.begin(), cinematic.cameraKeys.end(), timeSeconds,
        [](const double time, const CinematicCameraKeyUVE& existing) { return time < existing.timeSeconds; });
    cinematic.cameraKeys.insert(slot, std::move(key));
    return true;
}

bool RemoveCinematicCameraKeyUVE(CinematicComponentUVE& cinematic, const std::size_t index) {
    if (index >= cinematic.cameraKeys.size()) {
        return false;
    }
    cinematic.cameraKeys.erase(cinematic.cameraKeys.begin() + static_cast<std::ptrdiff_t>(index));
    return true;
}

std::optional<CinematicCameraPoseUVE> SampleCinematicCameraUVE(const CinematicComponentUVE& cinematic,
                                                               const double timeSeconds) noexcept {
    if (cinematic.cameraKeys.empty() || !std::isfinite(timeSeconds)) {
        return std::nullopt;
    }
    const auto upper = std::upper_bound(
        cinematic.cameraKeys.begin(), cinematic.cameraKeys.end(), timeSeconds,
        [](const double time, const CinematicCameraKeyUVE& existing) { return time < existing.timeSeconds; });
    if (upper == cinematic.cameraKeys.begin()) {
        const CinematicCameraKeyUVE& first = cinematic.cameraKeys.front();
        return CinematicCameraPoseUVE{first.position, first.rotation};
    }
    if (upper == cinematic.cameraKeys.end()) {
        const CinematicCameraKeyUVE& last = cinematic.cameraKeys.back();
        return CinematicCameraPoseUVE{last.position, last.rotation};
    }
    const CinematicCameraKeyUVE& right = *upper;
    const CinematicCameraKeyUVE& left = *(upper - 1);
    const double span = right.timeSeconds - left.timeSeconds;
    const float alpha =
        span <= 0.0 ? 0.0F : static_cast<float>((timeSeconds - left.timeSeconds) / span);
    CinematicCameraPoseUVE pose;
    pose.position = left.position + (right.position - left.position) * alpha;
    if (!Math::TrySlerpUVE(left.rotation, right.rotation, alpha, pose.rotation)) {
        pose.rotation = left.rotation;
    }
    return pose;
}

void WriteCinematicCameraPoseUVE(const CinematicCameraPoseUVE& pose, TransformComponentUVE& target) noexcept {
    target.localPosition = pose.position;
    Math::QuaternionUVE normalized{};
    if (Math::TryNormalizeUVE(pose.rotation, normalized)) {
        target.localRotation = normalized;
        Math::Vector3UVE euler{};
        if (Math::TryToEulerOrderedUVE(normalized, target.eulerOrder, euler)) {
            target.localEulerRadians = euler;
        }
    }
}

std::vector<std::string> PlayCinematicUVE(CinematicComponentUVE& cinematic) noexcept {
    cinematic.isPlaying = true;
    cinematic.finished = false;
    cinematic.currentTimeSeconds = 0.0;
    std::vector<std::string> fired;
    if (cinematic.durationSeconds <= 0.0) {
        return fired;
    }
    for (const CinematicEventKeyUVE& key : cinematic.events) {
        if (key.timeSeconds > 0.0) {
            break;
        }
        fired.push_back(key.eventId);
    }
    return fired;
}

void StopCinematicUVE(CinematicComponentUVE& cinematic) noexcept {
    cinematic.isPlaying = false;
}

CinematicStepResultUVE StepCinematicUVE(CinematicComponentUVE& cinematic, const float deltaSeconds) {
    CinematicStepResultUVE result;
    if (!cinematic.isPlaying || !std::isfinite(deltaSeconds) || deltaSeconds <= 0.0F ||
        !std::isfinite(cinematic.durationSeconds) || cinematic.durationSeconds <= 0.0) {
        return result;
    }
    const double from = cinematic.currentTimeSeconds;
    const double advanced = from + static_cast<double>(deltaSeconds) * static_cast<double>(cinematic.speed);
    if (cinematic.loopMode == CinematicLoopModeUVE::Loop && cinematic.durationSeconds > 0.0) {
        double wrapped = std::fmod(advanced, cinematic.durationSeconds);
        if (wrapped < 0.0) {
            wrapped += cinematic.durationSeconds;
        }
        if (advanced >= cinematic.durationSeconds) {
            CollectForwardUVE(cinematic.events, from, cinematic.durationSeconds, result.firedEventIds);
            CollectForwardUVE(cinematic.events, -1.0, wrapped, result.firedEventIds);
        } else if (advanced < 0.0) {
            // Reverse motion rewinds the clock and resolves the camera, but fires nothing.
        } else {
            CollectForwardUVE(cinematic.events, from, wrapped, result.firedEventIds);
        }
        cinematic.currentTimeSeconds = wrapped;
    } else {
        const double to = std::clamp(advanced, 0.0, cinematic.durationSeconds);
        if (to > from) {
            CollectForwardUVE(cinematic.events, from, to, result.firedEventIds);
        }
        cinematic.currentTimeSeconds = to;
        if (advanced >= cinematic.durationSeconds) {
            cinematic.isPlaying = false;
            cinematic.finished = true;
            result.justFinished = true;
        }
    }
    result.activeCamera = CameraCutAtUVE(cinematic.cuts, cinematic.currentTimeSeconds);
    if (!cinematic.cameraKeys.empty()) {
        result.cameraPose = SampleCinematicCameraUVE(cinematic, cinematic.currentTimeSeconds);
    }
    return result;
}

EntityUVE ActiveCinematicCameraUVE(const CinematicComponentUVE& cinematic, const double timeSeconds) noexcept {
    if (!std::isfinite(timeSeconds)) {
        return kInvalidEntityUVE;
    }
    return CameraCutAtUVE(cinematic.cuts, timeSeconds);
}

} // namespace UVE::Scene
