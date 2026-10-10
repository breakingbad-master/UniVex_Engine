// Copyright (c) 2026 UniVex Studios. All Rights Reserved.

#include "uve/scene/scene_serializer_uve.h"

#include <optional>
#include <vector>

#include "uve/entity/entity_manager_uve.h"
#include "uve/events/event_system_uve.h"
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

} // namespace UVE::Scene::Tests
