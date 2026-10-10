// Copyright (c) 2026 UniVex Studios. All Rights Reserved.

#include "uve/scene/scene_serializer_uve.h"

#include <algorithm>
#include <optional>
#include <vector>

#include "uve/entity/entity_manager_uve.h"
#include "uve/events/event_system_uve.h"
#include "uve/gameplay/cinematic_uve.h"
#include "uve/gameplay/gameplay_attributes_uve.h"
#include "uve/gameplay/gameplay_tags_uve.h"
#include "uve/gameplay/status_effects_uve.h"
#include "uve/memory/memory_manager_uve.h"

#include <gtest/gtest.h>

namespace UVE::Scene::Tests {
namespace {

class GameplaySerializationUVETest : public ::testing::Test {
protected:
    Memory::MemoryManagerUVE memoryManager;
    Events::EventSystemUVE eventSystem;
    EntityManagerUVE entityManager{memoryManager.GetDefaultAllocatorUVE(), eventSystem};
    SceneSerializerUVE serializer;
};

} // namespace

TEST_F(GameplaySerializationUVETest, Attributes_RoundTripThroughCaptureRestore) {
    const EntityUVE source = entityManager.CreateEntityUVE();
    GameplayAttributesComponentUVE attributes;
    ASSERT_TRUE(AddGameplayAttributeUVE(attributes, "stamina", 100.0F, 70.0F, 2.0F));
    ASSERT_TRUE(AddGameplayAttributeUVE(attributes, "mana", 50.0F, 50.0F));
    entityManager.AddComponentUVE<GameplayAttributesComponentUVE>(source, attributes);

    const std::optional<SceneSnapshotUVE> snapshot =
        serializer.CaptureUVE(entityManager, {source}, SceneAssetTypeUVE::Scene);
    ASSERT_TRUE(snapshot.has_value());
    const std::vector<EntityUVE> restored = serializer.RestoreUVE(entityManager, *snapshot);
    ASSERT_EQ(restored.size(), 1U);
    const GameplayAttributesComponentUVE& revived =
        entityManager.GetComponentUVE<GameplayAttributesComponentUVE>(restored.front());
    EXPECT_EQ(revived, attributes);
}

TEST_F(GameplaySerializationUVETest, Tags_RoundTripThroughCaptureRestore) {
    const EntityUVE source = entityManager.CreateEntityUVE();
    GameplayTagComponentUVE tags;
    ASSERT_TRUE(AddGameplayTagUVE(tags, "undead"));
    ASSERT_TRUE(AddGameplayTagUVE(tags, "boss"));
    entityManager.AddComponentUVE<GameplayTagComponentUVE>(source, tags);

    const std::optional<SceneSnapshotUVE> snapshot =
        serializer.CaptureUVE(entityManager, {source}, SceneAssetTypeUVE::Scene);
    ASSERT_TRUE(snapshot.has_value());
    const std::vector<EntityUVE> restored = serializer.RestoreUVE(entityManager, *snapshot);
    ASSERT_EQ(restored.size(), 1U);
    const GameplayTagComponentUVE& revived =
        entityManager.GetComponentUVE<GameplayTagComponentUVE>(restored.front());
    EXPECT_EQ(revived, tags);
}

TEST_F(GameplaySerializationUVETest, StatusEffects_RoundTripThroughCaptureRestore) {
    const EntityUVE source = entityManager.CreateEntityUVE();
    StatusEffectsComponentUVE effects;
    ASSERT_TRUE(ApplyStatusEffectUVE(effects, "poison", "stamina", 10.0F, 3.0F));
    ASSERT_TRUE(ApplyStatusEffectUVE(effects, "regen-aura", "mana", -5.0F, 9.5F));
    entityManager.AddComponentUVE<StatusEffectsComponentUVE>(source, effects);

    const std::optional<SceneSnapshotUVE> snapshot =
        serializer.CaptureUVE(entityManager, {source}, SceneAssetTypeUVE::Scene);
    ASSERT_TRUE(snapshot.has_value());
    const std::vector<EntityUVE> restored = serializer.RestoreUVE(entityManager, *snapshot);
    ASSERT_EQ(restored.size(), 1U);
    const StatusEffectsComponentUVE& revived =
        entityManager.GetComponentUVE<StatusEffectsComponentUVE>(restored.front());
    EXPECT_EQ(revived, effects);
}


TEST_F(GameplaySerializationUVETest, Cinematic_RoundTripThroughCaptureRestore) {
    const EntityUVE source = entityManager.CreateEntityUVE();
    const EntityUVE camA = entityManager.CreateEntityUVE();
    const EntityUVE camB = entityManager.CreateEntityUVE();
    CinematicComponentUVE cinematic;
    cinematic.durationSeconds = 10.0;
    cinematic.autoplay = true;
    cinematic.speed = 2.0F;
    cinematic.loopMode = CinematicLoopModeUVE::Loop;
    ASSERT_TRUE(AddCinematicEventUVE(cinematic, 0.0, "open"));
    ASSERT_TRUE(AddCinematicEventUVE(cinematic, 5.0, "mid"));
    ASSERT_TRUE(AddCinematicCutUVE(cinematic, 0.0, camA));
    ASSERT_TRUE(AddCinematicCutUVE(cinematic, 5.0, camB));
    ASSERT_TRUE(AddCinematicCameraKeyUVE(cinematic, 0.0, Math::Vector3UVE{1.0F, 2.0F, 3.0F},
                                         Math::QuaternionUVE{0.0F, 0.70710678F, 0.0F, 0.70710678F}));
    ASSERT_TRUE(AddCinematicCameraKeyUVE(cinematic, 10.0, Math::Vector3UVE{4.0F, 5.0F, 6.0F},
                                         Math::QuaternionUVE{}));
    entityManager.AddComponentUVE<CinematicComponentUVE>(source, cinematic);

    const std::optional<SceneSnapshotUVE> snapshot =
        serializer.CaptureUVE(entityManager, {source, camA, camB}, SceneAssetTypeUVE::Scene);
    ASSERT_TRUE(snapshot.has_value());
    const std::vector<EntityUVE> restored = serializer.RestoreUVE(entityManager, *snapshot);
    ASSERT_EQ(restored.size(), 3U);
    std::optional<EntityUVE> revivedEntity;
    for (const EntityUVE entity : restored) {
        if (entityManager.HasComponentUVE<CinematicComponentUVE>(entity)) {
            revivedEntity = entity;
        }
    }
    ASSERT_TRUE(revivedEntity.has_value());
    const CinematicComponentUVE& revived =
        entityManager.GetComponentUVE<CinematicComponentUVE>(*revivedEntity);
    EXPECT_DOUBLE_EQ(revived.durationSeconds, 10.0);
    EXPECT_TRUE(revived.autoplay);
    EXPECT_FLOAT_EQ(revived.speed, 2.0F);
    EXPECT_EQ(revived.loopMode, CinematicLoopModeUVE::Loop);
    EXPECT_EQ(revived.events, cinematic.events);
    EXPECT_EQ(revived.cameraKeys, cinematic.cameraKeys);
    ASSERT_EQ(revived.cuts.size(), 2U);
    EXPECT_DOUBLE_EQ(revived.cuts[0].timeSeconds, 0.0);
    EXPECT_DOUBLE_EQ(revived.cuts[1].timeSeconds, 5.0);
    EXPECT_NE(revived.cuts[0].camera, kInvalidEntityUVE);
    EXPECT_NE(revived.cuts[1].camera, kInvalidEntityUVE);
    EXPECT_NE(revived.cuts[0].camera, revived.cuts[1].camera);
    EXPECT_NE(std::find(restored.begin(), restored.end(), revived.cuts[0].camera), restored.end());
    EXPECT_NE(std::find(restored.begin(), restored.end(), revived.cuts[1].camera), restored.end());
    EXPECT_FALSE(revived.isPlaying);
    EXPECT_FALSE(revived.finished);
    EXPECT_DOUBLE_EQ(revived.currentTimeSeconds, 0.0);
}

TEST_F(GameplaySerializationUVETest, Cinematic_DropsCutsOutsideTheSavedSet) {
    const EntityUVE source = entityManager.CreateEntityUVE();
    const EntityUVE outsider = entityManager.CreateEntityUVE();
    CinematicComponentUVE cinematic;
    cinematic.durationSeconds = 10.0;
    ASSERT_TRUE(AddCinematicEventUVE(cinematic, 1.0, "kept"));
    ASSERT_TRUE(AddCinematicCutUVE(cinematic, 1.0, outsider));
    entityManager.AddComponentUVE<CinematicComponentUVE>(source, cinematic);

    const std::optional<SceneSnapshotUVE> snapshot =
        serializer.CaptureUVE(entityManager, {source}, SceneAssetTypeUVE::Scene);
    ASSERT_TRUE(snapshot.has_value());
    const std::vector<EntityUVE> restored = serializer.RestoreUVE(entityManager, *snapshot);
    ASSERT_EQ(restored.size(), 1U);
    const CinematicComponentUVE& revived =
        entityManager.GetComponentUVE<CinematicComponentUVE>(restored.front());
    EXPECT_EQ(revived.events, cinematic.events);
    EXPECT_TRUE(revived.cuts.empty());
    EXPECT_TRUE(IsCinematicComponentValidUVE(revived));
}
} // namespace UVE::Scene::Tests
