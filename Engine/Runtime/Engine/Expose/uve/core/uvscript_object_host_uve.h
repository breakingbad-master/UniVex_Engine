// Copyright (c) 2026 UniVex Studios. All Rights Reserved.

#pragma once

#include <functional>

#include "uve/component/entity_uve.h"
#include "uve/uvscript/uvscript_host_uve.h"

namespace UVE::Input {
class IInputSystemUVE;
} // namespace UVE::Input

namespace UVE::Scene {
class IEntityManagerUVE;
} // namespace UVE::Scene

namespace UVE::Audio {
class IAudioSourceSystemUVE;
class IAudioSystemUVE;
} // namespace Audio

namespace UVE::UVScript {
class ScriptInstanceUVE;
} // namespace UVE::UVScript

namespace UVE::Core {

/// What a `.uvs` script can reach on the object it is attached to. The object's components decide:
/// - every object: `name` (read-only);
/// - an object with a transform: `position`, `scale` (local, metres);
/// - a character body: `velocity`, `grounded` (read-only);
/// - a rigid body: `velocity` (linear, metres/second);
/// - a rigid body: `physics.apply_force/apply_impulse/apply_torque(vec3)` - a persistent force in
///   newtons until changed, an instant mass-scaled velocity kick, a persistent torque - each
///   answering whether it applied;
/// - an object with gameplay attributes: `attributes.has/get/max(id)` reads a pool (a missing id
///   reads false and 0.0), `attributes.damage/heal(id, amount)` moves it, answering whether it
///   applied - health itself stays in the strike pipeline, out of these pools;
/// - an object with gameplay tags: `tags.has/add/remove(tag)`, each answering whether it applied;
/// - always: `input.pressed/held/released(action)` and `input.axis(negative, positive)`, which
///   read the project's input map; events `ready` and `tick(dt)`.
/// - always: `node(name)` finds another object by name, and `ref.method(args)` calls one of its
///   script functions (or the built-in `hide()`/`show()`), fire-and-forget.
class UVScriptObjectHostUVE final : public UVScript::UVScriptHostUVE {
public:
    /// Finds the running script of an entity, for routing cross-node calls at it. Null when the
    /// entity runs no script. Empty on check-only hosts, whose calls then only reach built-ins.
    using InstanceResolverUVE = std::function<UVScript::ScriptInstanceUVE*(Scene::EntityUVE)>;

    /// `input` may be null (no input system): input calls then read as not pressed.
    /// `audioSources`/`audio` may be null (no audio): audio calls then fail closed as false. Both
    /// default to null so check-only hosts (editor diagnostics, tests that never touch audio) stay
    /// three-argument constructions.
    UVScriptObjectHostUVE(Scene::IEntityManagerUVE& entityManager, const Input::IInputSystemUVE* input,
                          Scene::EntityUVE entity, Audio::IAudioSourceSystemUVE* audioSources = nullptr,
                          Audio::IAudioSystemUVE* audio = nullptr,
                          InstanceResolverUVE resolver = InstanceResolverUVE{}) noexcept;

    [[nodiscard]] std::optional<UVScript::HostPropertyUVE> DescribePropertyUVE(std::string_view name) const override;
    [[nodiscard]] std::optional<UVScript::HostFunctionUVE> DescribeFunctionUVE(std::string_view name) const override;
    [[nodiscard]] std::optional<std::vector<UVScript::TypeUVE>> DescribeEventUVE(std::string_view event) const override;

    [[nodiscard]] UVScript::ValueUVE GetPropertyUVE(std::string_view name) override;
    void SetPropertyUVE(std::string_view name, const UVScript::ValueUVE& value) override;
    [[nodiscard]] UVScript::ValueUVE CallFunctionUVE(std::string_view name,
                                                      std::span<const UVScript::ValueUVE> args) override;
    void CallMethodUVE(UVScript::ObjectRefUVE target, std::string_view method,
                       std::span<const UVScript::ValueUVE> args) override;
    void PrintUVE(std::string_view text) override;

private:
    Scene::IEntityManagerUVE& m_entityManager;
    const Input::IInputSystemUVE* m_input = nullptr;
    Scene::EntityUVE m_entity = Scene::kInvalidEntityUVE;
    Audio::IAudioSourceSystemUVE* m_audioSources = nullptr;
    Audio::IAudioSystemUVE* m_audio = nullptr;
    InstanceResolverUVE m_resolver;
};

} // namespace UVE::Core
