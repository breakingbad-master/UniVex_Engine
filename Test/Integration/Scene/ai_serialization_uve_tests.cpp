// Copyright (c) 2026 UniVex Studios. All Rights Reserved.

#include "uve/scene/scene_serializer_uve.h"

#include <optional>
#include <vector>

#include "uve/ai/utility_ai_uve.h"
#include "uve/entity/entity_manager_uve.h"
#include "uve/events/event_system_uve.h"
#include "uve/memory/memory_manager_uve.h"

#include <gtest/gtest.h>

namespace UVE::Scene::Tests {
namespace {

class AiSerializationUVETest : public ::testing::Test {
protected:
    Memory::MemoryManagerUVE memoryManager;
    Events::EventSystemUVE eventSystem;
    EntityManagerUVE entityManager{memoryManager.GetDefaultAllocatorUVE(), eventSystem};
    SceneSerializerUVE serializer;
};

} // namespace

TEST_F(AiSerializationUVETest, Brain_RoundTripResetsSelectionState) {
    const EntityUVE source = entityManager.CreateEntityUVE();
    AiBrainComponentUVE brain;
    brain.hysteresis = 0.2F;
    AiActionUVE flee;
    flee.actionId = "flee";
    flee.baseScore = 0.9F;
    flee.considerations.push_back(
        AiConsiderationUVE{"health", AiResponseCurveUVE::InverseQuadratic, 2.0F});
    brain.actions.push_back(std::move(flee));
    brain.actions.push_back(AiActionUVE{"patrol", 0.4F, {}});
    brain.currentAction = "flee";
    brain.currentScore = 0.8F;
    entityManager.AddComponentUVE<AiBrainComponentUVE>(source, brain);

    const std::optional<SceneSnapshotUVE> snapshot =
        serializer.CaptureUVE(entityManager, {source}, SceneAssetTypeUVE::Scene);
    ASSERT_TRUE(snapshot.has_value());
    const std::vector<EntityUVE> restored = serializer.RestoreUVE(entityManager, *snapshot);
    ASSERT_EQ(restored.size(), 1U);
    const AiBrainComponentUVE& revived =
        entityManager.GetComponentUVE<AiBrainComponentUVE>(restored.front());
    EXPECT_FLOAT_EQ(revived.hysteresis, 0.2F);
    ASSERT_EQ(revived.actions.size(), 2U);
    EXPECT_EQ(revived.actions, brain.actions);
    EXPECT_TRUE(revived.currentAction.empty());
    EXPECT_FLOAT_EQ(revived.currentScore, 0.0F);
}

TEST_F(AiSerializationUVETest, Blackboard_RoundTripThroughCaptureRestore) {
    const EntityUVE source = entityManager.CreateEntityUVE();
    BlackboardComponentUVE board;
    ASSERT_TRUE(SetBlackboardValueUVE(board, "health", 0.25F));
    ASSERT_TRUE(SetBlackboardValueUVE(board, "enemyNear", 1.0F));
    entityManager.AddComponentUVE<BlackboardComponentUVE>(source, board);

    const std::optional<SceneSnapshotUVE> snapshot =
        serializer.CaptureUVE(entityManager, {source}, SceneAssetTypeUVE::Scene);
    ASSERT_TRUE(snapshot.has_value());
    const std::vector<EntityUVE> restored = serializer.RestoreUVE(entityManager, *snapshot);
    ASSERT_EQ(restored.size(), 1U);
    const BlackboardComponentUVE& revived =
        entityManager.GetComponentUVE<BlackboardComponentUVE>(restored.front());
    EXPECT_EQ(revived, board);
}

} // namespace UVE::Scene::Tests
