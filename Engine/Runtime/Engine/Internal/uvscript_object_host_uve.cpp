// Copyright (c) 2026 UniVex Studios. All Rights Reserved.

#include "uve/core/uvscript_object_host_uve.h"

#include <cmath>
#include <string>

#include "uve/audio/i_audio_source_system_uve.h"
#include "uve/audio/i_audio_system_uve.h"
#include "uve/component/audio_source_component_uve.h"
#include "uve/component/camera_component_uve.h"
#include "uve/component/character_controller_component_uve.h"
#include "uve/component/light_component_uve.h"
#include "uve/component/name_component_uve.h"
#include "uve/component/rigid_3d_component_uve.h"
#include "uve/component/transform_component_uve.h"
#include "uve/component/visibility_component_uve.h"
#include "uve/component/world_transform_component_uve.h"
#include "uve/entity/i_entity_manager_uve.h"
#include "uve/input/i_input_system_uve.h"
#include "uve/logging/logging_macros_uve.h"
#include "uve/objects/3d/rigid_3d_uve.h"
#include "uve/uvscript/uvscript_instance_uve.h"

namespace UVE::Core {
namespace {

using UVScript::HostFunctionUVE;
using UVScript::HostPropertyUVE;
using UVScript::ObjectRefUVE;
using UVScript::TypeUVE;
using UVScript::ValueUVE;
using UVScript::Vec3ValueUVE;

/// A script's handle to another object packs its entity, plus one: 0 stays reserved for "no
/// object" (entity slot 0, generation 0 is a real entity and must keep its own handle).
[[nodiscard]] constexpr std::uint64_t PackEntityUVE(const Scene::EntityUVE entity) noexcept {
    return ((static_cast<std::uint64_t>(entity.generation) << 32U) | entity.index) + 1U;
}

[[nodiscard]] constexpr Scene::EntityUVE UnpackEntityUVE(const ObjectRefUVE ref) noexcept {
    const std::uint64_t packed = ref.id - 1U;
    return Scene::EntityUVE{static_cast<std::uint32_t>(packed),
                            static_cast<std::uint32_t>(packed >> 32U)};
}

void SetVisibleUVE(Scene::IEntityManagerUVE& entities, const Scene::EntityUVE entity, const bool shown) {
    if (entities.HasComponentUVE<Scene::VisibilityComponentUVE>(entity)) {
        entities.GetComponentUVE<Scene::VisibilityComponentUVE>(entity).visible = shown;
    } else if (!shown) {
        // Showing adds nothing: an object without the component is already visible.
        Scene::VisibilityComponentUVE visibility;
        visibility.visible = false;
        entities.AddComponentUVE<Scene::VisibilityComponentUVE>(entity, visibility);
    }
}

[[nodiscard]] Vec3ValueUVE ToScriptUVE(const Math::Vector3UVE& v) noexcept {
    return {static_cast<double>(v.x), static_cast<double>(v.y), static_cast<double>(v.z)};
}

[[nodiscard]] Math::Vector3UVE ToEngineUVE(const ValueUVE& value) {
    const Vec3ValueUVE& v = std::get<Vec3ValueUVE>(value);
    return {static_cast<float>(v.x), static_cast<float>(v.y), static_cast<float>(v.z)};
}

} // namespace

UVScriptObjectHostUVE::UVScriptObjectHostUVE(Scene::IEntityManagerUVE& entityManager, const Input::IInputSystemUVE* const input,
                                         const Scene::EntityUVE entity,
                                         Audio::IAudioSourceSystemUVE* const audioSources,
                                         Audio::IAudioSystemUVE* const audio,
                                         InstanceResolverUVE resolver) noexcept
    : m_entityManager(entityManager), m_input(input), m_entity(entity), m_audioSources(audioSources),
      m_audio(audio), m_resolver(std::move(resolver)) {}

std::optional<HostPropertyUVE> UVScriptObjectHostUVE::DescribePropertyUVE(const std::string_view name) const {
    if (name == "name") {
        return HostPropertyUVE{TypeUVE::StrUVE(), false};
    }
    if ((name == "position" || name == "scale") && m_entityManager.HasComponentUVE<Scene::TransformComponentUVE>(m_entity)) {
        return HostPropertyUVE{TypeUVE::Vec3UVE(), true};
    }
    if (name == "velocity" &&
        (m_entityManager.HasComponentUVE<Scene::CharacterControllerComponentUVE>(m_entity) ||
         m_entityManager.HasComponentUVE<Scene::Rigid3DComponentUVE>(m_entity))) {
        return HostPropertyUVE{TypeUVE::Vec3UVE(), true};
    }
    if ((name == "volume" || name == "pitch") &&
        m_entityManager.HasComponentUVE<Scene::AudioSourceComponentUVE>(m_entity)) {
        return HostPropertyUVE{TypeUVE::FloatUVE(), true};
    }
    // Ungated: every object is inherently visible, so a script reads true and a write adds the
    // component. The scene graph owns visibleInHierarchy; scripts only ever touch the switch.
    if (name == "visible") {
        return HostPropertyUVE{TypeUVE::BoolUVE(), true};
    }
    if (name == "intensity" && m_entityManager.HasComponentUVE<Scene::LightComponentUVE>(m_entity)) {
        return HostPropertyUVE{TypeUVE::FloatUVE(), true};
    }
    if (name == "fov" && m_entityManager.HasComponentUVE<Scene::CameraComponentUVE>(m_entity)) {
        return HostPropertyUVE{TypeUVE::FloatUVE(), true};
    }
    if (m_entityManager.HasComponentUVE<Scene::CharacterControllerComponentUVE>(m_entity)) {
        // "grounded", matching the other properties here: name, position, scale, velocity - a plain
        // word for the thing, no "is_" prefix and no snake_case. "is_on_floor" is the name this had
        // before, and a script written against it still reads, because a script out in the world is
        // not something a rename in here is allowed to break. Writing always uses the current name.
        if (name == "grounded" || name == "is_on_floor") {
            return HostPropertyUVE{TypeUVE::BoolUVE(), false};
        }
    }
    return std::nullopt;
}

std::optional<HostFunctionUVE> UVScriptObjectHostUVE::DescribeFunctionUVE(const std::string_view name) const {
    if (m_entityManager.HasComponentUVE<Scene::Rigid3DComponentUVE>(m_entity) &&
        (name == "physics.apply_force" || name == "physics.apply_impulse" || name == "physics.apply_torque")) {
        return HostFunctionUVE{{TypeUVE::Vec3UVE()}, TypeUVE::BoolUVE()};
    }
    if (m_entityManager.HasComponentUVE<Scene::AudioSourceComponentUVE>(m_entity) &&
        (name == "audio.play" || name == "audio.stop" || name == "audio.is_playing")) {
        return HostFunctionUVE{{}, TypeUVE::BoolUVE()};
    }
    if (name == "input.pressed" || name == "input.held" || name == "input.released") {
        return HostFunctionUVE{{TypeUVE::StrUVE()}, TypeUVE::BoolUVE()};
    }
    // Ungated: any script may look another object up. The kind is only known when the script runs,
    // so every lookup answers the common root; a missing name answers no object (id 0).
    if (name == "node") {
        return HostFunctionUVE{{TypeUVE::StrUVE()}, TypeUVE::ObjectUVE("Object3D")};
    }
    if (name == "input.axis") {
        return HostFunctionUVE{{TypeUVE::StrUVE(), TypeUVE::StrUVE()}, TypeUVE::FloatUVE()};
    }
    return std::nullopt;
}

std::optional<std::vector<TypeUVE>> UVScriptObjectHostUVE::DescribeEventUVE(const std::string_view event) const {
    if (event == "ready") {
        return std::vector<TypeUVE>{};
    }
    if (event == "tick") {
        return std::vector<TypeUVE>{TypeUVE::FloatUVE()};
    }
    // An AnimationSequencer's clip passed one of its events: sent to the player's own script and to
    // the script of the object it animates (a character's script hears its footsteps).
    if (event == "animation_event") {
        return std::vector<TypeUVE>{TypeUVE::StrUVE()};
    }
    // Contact edges: both parties' scripts hear, each with the other object's name ("" when the
    // other has no Name component). Scripts predate neither party - a contact already active when
    // a script starts surfaces no enter, though its exit still fires.
    if (event == "collision_enter" || event == "collision_exit" || event == "overlap_enter" ||
        event == "overlap_exit") {
        return std::vector<TypeUVE>{TypeUVE::StrUVE()};
    }
    return std::nullopt;
}

ValueUVE UVScriptObjectHostUVE::GetPropertyUVE(const std::string_view name) {
    if (name == "name") {
        return m_entityManager.HasComponentUVE<Scene::NameComponentUVE>(m_entity)
                   ? ValueUVE{m_entityManager.GetComponentUVE<Scene::NameComponentUVE>(m_entity).name}
                   : ValueUVE{std::string{}};
    }
    if (name == "position" || name == "scale") {
        const Scene::TransformComponentUVE& transform = m_entityManager.GetComponentUVE<Scene::TransformComponentUVE>(m_entity);
        return ToScriptUVE(name == "position" ? transform.localPosition : transform.localScale);
    }
    if (name == "velocity") {
        // Both bodies on one entity is nonsense, but the character wins rather than crashing.
        if (m_entityManager.HasComponentUVE<Scene::CharacterControllerComponentUVE>(m_entity)) {
            return ToScriptUVE(
                m_entityManager.GetComponentUVE<Scene::CharacterControllerComponentUVE>(m_entity).velocity);
        }
        return ToScriptUVE(m_entityManager.GetComponentUVE<Scene::Rigid3DComponentUVE>(m_entity).velocity);
    }
    if (name == "volume" || name == "pitch") {
        const Scene::AudioSourceComponentUVE& source =
            m_entityManager.GetComponentUVE<Scene::AudioSourceComponentUVE>(m_entity);
        return static_cast<double>(name == "volume" ? source.volume : source.pitch);
    }
    if (name == "visible") {
        return !m_entityManager.HasComponentUVE<Scene::VisibilityComponentUVE>(m_entity) ||
               m_entityManager.GetComponentUVE<Scene::VisibilityComponentUVE>(m_entity).visible;
    }
    if (name == "intensity") {
        return static_cast<double>(
            m_entityManager.GetComponentUVE<Scene::LightComponentUVE>(m_entity).intensity);
    }
    if (name == "fov") {
        return static_cast<double>(
            m_entityManager.GetComponentUVE<Scene::CameraComponentUVE>(m_entity).fieldOfViewDegrees);
    }
    return m_entityManager.GetComponentUVE<Scene::CharacterControllerComponentUVE>(m_entity).grounded;
}

void UVScriptObjectHostUVE::SetPropertyUVE(const std::string_view name, const ValueUVE& value) {
    if (name == "position" || name == "scale") {
        Scene::TransformComponentUVE& transform = m_entityManager.GetComponentUVE<Scene::TransformComponentUVE>(m_entity);
        (name == "position" ? transform.localPosition : transform.localScale) = ToEngineUVE(value);
        // Marked stale so the scene graph recomputes the world transform. Not through
        // SetLocalTransformUVE: that ignores an object that has no world transform yet.
        if (m_entityManager.HasComponentUVE<Scene::WorldTransformComponentUVE>(m_entity)) {
            m_entityManager.GetComponentUVE<Scene::WorldTransformComponentUVE>(m_entity).dirty = true;
        }
    } else if (name == "velocity") {
        if (m_entityManager.HasComponentUVE<Scene::CharacterControllerComponentUVE>(m_entity)) {
            m_entityManager.GetComponentUVE<Scene::CharacterControllerComponentUVE>(m_entity).velocity =
                ToEngineUVE(value);
        } else {
            m_entityManager.GetComponentUVE<Scene::Rigid3DComponentUVE>(m_entity).velocity = ToEngineUVE(value);
        }
    } else if (name == "volume" || name == "pitch") {
        const float updated = static_cast<float>(std::get<double>(value));
        Scene::AudioSourceComponentUVE& source =
            m_entityManager.GetComponentUVE<Scene::AudioSourceComponentUVE>(m_entity);
        // Unlike velocity's blind store, an invalid write is ignored: a NaN volume would make
        // AudioSourceSystemUVE skip the source on every Sync, so the component stays valid.
        const bool accepted = name == "volume" ? (std::isfinite(updated) && updated >= 0.0F)
                                               : (std::isfinite(updated) && updated > 0.0F);
        if (accepted) {
            (name == "volume" ? source.volume : source.pitch) = updated;
        }
    } else if (name == "visible") {
        SetVisibleUVE(m_entityManager, m_entity, std::get<bool>(value));
    } else if (name == "intensity") {
        const float updated = static_cast<float>(std::get<double>(value));
        if (std::isfinite(updated) && updated >= 0.0F) {
            m_entityManager.GetComponentUVE<Scene::LightComponentUVE>(m_entity).intensity = updated;
        }
    } else if (name == "fov") {
        const float updated = static_cast<float>(std::get<double>(value));
        if (std::isfinite(updated) && updated >= Scene::kMinimumCameraFieldOfViewDegreesUVE &&
            updated < 180.0F) {
            m_entityManager.GetComponentUVE<Scene::CameraComponentUVE>(m_entity).fieldOfViewDegrees =
                updated;
        }
    }
}

ValueUVE UVScriptObjectHostUVE::CallFunctionUVE(const std::string_view name, const std::span<const ValueUVE> args) {
    // Physics needs no input system, so it routes before the null-input early-out below.
    if (name == "physics.apply_force") {
        return Scene::Rigid3DUVE::ApplyForceUVE(m_entityManager, m_entity, ToEngineUVE(args[0]));
    }
    if (name == "physics.apply_impulse") {
        return Scene::Rigid3DUVE::ApplyImpulseUVE(m_entityManager, m_entity, ToEngineUVE(args[0]));
    }
    if (name == "physics.apply_torque") {
        return Scene::Rigid3DUVE::ApplyTorqueUVE(m_entityManager, m_entity, ToEngineUVE(args[0]));
    }
    // Audio needs no input system, so it routes before the null-input early-out below.
    if (name == "audio.play" || name == "audio.stop" || name == "audio.is_playing") {
        if (m_audioSources == nullptr || m_audio == nullptr) {
            return ValueUVE{false};
        }
        if (name == "audio.play") {
            return m_audioSources->PlayEntityUVE(m_entity, m_entityManager, *m_audio);
        }
        if (name == "audio.stop") {
            return m_audioSources->StopEntityUVE(m_entity, *m_audio);
        }
        return m_audioSources->IsEntityPlayingUVE(m_entity, *m_audio);
    }
    // A lookup needs no input system, so it routes before the null-input early-out below.
    if (name == "node") {
        const std::string& wanted = std::get<std::string>(args[0]);
        Scene::EntityUVE found = Scene::kInvalidEntityUVE;
        m_entityManager.ForEachUVE<Scene::NameComponentUVE>(
            [&](const Scene::EntityUVE entity, const Scene::NameComponentUVE& named) {
                if (found == Scene::kInvalidEntityUVE && named.name == wanted) {
                    found = entity;
                }
            });
        if (found == Scene::kInvalidEntityUVE) {
            return ValueUVE{ObjectRefUVE{}};
        }
        return ValueUVE{ObjectRefUVE{PackEntityUVE(found)}};
    }
    if (m_input == nullptr) {
        return name == "input.axis" ? ValueUVE{0.0} : ValueUVE{false};
    }
    if (name == "input.axis") {
        const bool negative = m_input->IsActionHeldUVE(std::get<std::string>(args[0]));
        const bool positive = m_input->IsActionHeldUVE(std::get<std::string>(args[1]));
        return (positive ? 1.0 : 0.0) - (negative ? 1.0 : 0.0);
    }
    const std::string& action = std::get<std::string>(args[0]);
    if (name == "input.pressed") {
        return m_input->IsActionTriggeredUVE(action);
    }
    if (name == "input.released") {
        return m_input->IsActionReleasedUVE(action);
    }
    return m_input->IsActionHeldUVE(action);
}

void UVScriptObjectHostUVE::CallMethodUVE(const ObjectRefUVE target, const std::string_view method,
                                       const std::span<const ValueUVE> args) {
    // A stale handle (its entity long destroyed) lands on another occupant or nothing at all; the
    // generation check inside IsAliveUVE tells the two apart, and both fail closed as nothing.
    if (target.id == 0U) {
        return;
    }
    const Scene::EntityUVE entity = UnpackEntityUVE(target);
    if (!m_entityManager.IsAliveUVE(entity)) {
        return;
    }
    // The target's own script first: a `fn hide()` there overrides the built-in below.
    if (m_resolver) {
        if (UVScript::ScriptInstanceUVE* const instance = m_resolver(entity);
            instance != nullptr && instance->HasFunctionUVE(method, args.size())) {
            static_cast<void>(instance->CallUVE(method, args));
            return;
        }
    }
    if (method == "hide" || method == "show") {
        SetVisibleUVE(m_entityManager, entity, method == "show");
    }
}

void UVScriptObjectHostUVE::PrintUVE(const std::string_view text) {
    const std::string object = m_entityManager.HasComponentUVE<Scene::NameComponentUVE>(m_entity)
                                 ? m_entityManager.GetComponentUVE<Scene::NameComponentUVE>(m_entity).name
                                 : std::string{"object"};
    UVE_INFO("[{}] {}", object, text);
}

} // namespace UVE::Core
