// Copyright (c) 2026 UniVex Studios. All Rights Reserved.

#include "uve/gameplay/pawn_uve.h"

#include <limits>

#include <gtest/gtest.h>

#include "uve/entity/entity_manager_uve.h"
#include "uve/events/event_system_uve.h"
#include "uve/memory/memory_manager_uve.h"

namespace UVE::Scene::Tests {
namespace {

class PawnUVETest : public ::testing::Test {
protected:
    Memory::MemoryManagerUVE memoryManager;
    Events::EventSystemUVE eventSystem;
    EntityManagerUVE entityManager{memoryManager.GetDefaultAllocatorUVE(), eventSystem};

    EntityUVE MakeControllerUVE(const ControllerKindUVE kind = ControllerKindUVE::Player) {
        const EntityUVE entity = entityManager.CreateEntityUVE();
        ControllerComponentUVE controller;
        controller.kind = kind;
        entityManager.AddComponentUVE<ControllerComponentUVE>(entity, controller);
        return entity;
    }

    EntityUVE MakePawnUVE() {
        const EntityUVE entity = entityManager.CreateEntityUVE();
        entityManager.AddComponentUVE<PawnComponentUVE>(entity, PawnComponentUVE{});
        return entity;
    }

    Gameplay::GameplayInputUVE SnapshotUVE() {
        Gameplay::GameplayInputUVE snapshot;
        snapshot.move = {1.0F, 2.0F, 3.0F};
        snapshot.rise = 4.0F;
        snapshot.jumpPressed = true;
        snapshot.lookPointer = {5.0F, 6.0F};
        snapshot.lookStick = {7.0F, 8.0F};
        snapshot.interactPressed = true;
        return snapshot;
    }
};

TEST_F(PawnUVETest, Possess_LinksBothSidesAndStealsExclusively) {
    const EntityUVE firstController = MakeControllerUVE();
    const EntityUVE secondController = MakeControllerUVE(ControllerKindUVE::AI);
    const EntityUVE firstPawn = MakePawnUVE();
    const EntityUVE secondPawn = MakePawnUVE();

    ASSERT_TRUE(PossessControllerUVE(entityManager, firstController, firstPawn));
    EXPECT_EQ(FindPossessedPawnUVE(entityManager, firstController), firstPawn);
    EXPECT_EQ(FindPawnControllerUVE(entityManager, firstPawn), firstController);
    RouteGameplayInputUVE(entityManager, SnapshotUVE());
    ASSERT_EQ(entityManager.GetComponentUVE<PawnComponentUVE>(firstPawn).input, SnapshotUVE());

    // Stealing from the controller side unlinks the previous pawn and purges its input.
    ASSERT_TRUE(PossessControllerUVE(entityManager, firstController, secondPawn));
    EXPECT_EQ(FindPossessedPawnUVE(entityManager, firstController), secondPawn);
    EXPECT_EQ(FindPawnControllerUVE(entityManager, firstPawn), kInvalidEntityUVE);
    EXPECT_EQ(entityManager.GetComponentUVE<PawnComponentUVE>(firstPawn).input,
              Gameplay::GameplayInputUVE{});

    // Stealing from the pawn side unlinks the previous controller.
    ASSERT_TRUE(PossessControllerUVE(entityManager, secondController, secondPawn));
    EXPECT_EQ(FindPossessedPawnUVE(entityManager, firstController), kInvalidEntityUVE);
    EXPECT_EQ(FindPawnControllerUVE(entityManager, secondPawn), secondController);

    // Re-possessing a mutual pair is a no-op success.
    EXPECT_TRUE(PossessControllerUVE(entityManager, secondController, secondPawn));
    EXPECT_EQ(FindPawnControllerUVE(entityManager, secondPawn), secondController);

    // A self-driven entity (both components, possessing itself) is allowed.
    const EntityUVE self = entityManager.CreateEntityUVE();
    entityManager.AddComponentUVE<ControllerComponentUVE>(self, ControllerComponentUVE{});
    entityManager.AddComponentUVE<PawnComponentUVE>(self, PawnComponentUVE{});
    EXPECT_TRUE(PossessControllerUVE(entityManager, self, self));
    EXPECT_EQ(FindPossessedPawnUVE(entityManager, self), self);
}

TEST_F(PawnUVETest, Possess_FailsClosedWithoutBothComponents) {
    const EntityUVE controller = MakeControllerUVE();
    const EntityUVE pawn = MakePawnUVE();
    const EntityUVE bare = entityManager.CreateEntityUVE();

    EXPECT_FALSE(PossessControllerUVE(entityManager, controller, bare));
    EXPECT_FALSE(PossessControllerUVE(entityManager, bare, pawn));
    EXPECT_FALSE(PossessControllerUVE(entityManager, bare, bare));
    EXPECT_EQ(entityManager.GetComponentUVE<ControllerComponentUVE>(controller).pawn,
              kInvalidEntityUVE);
    EXPECT_EQ(entityManager.GetComponentUVE<PawnComponentUVE>(pawn).controller, kInvalidEntityUVE);
}

TEST_F(PawnUVETest, Unpossess_ClearsBothSidesAndPurgesInput) {
    const EntityUVE controller = MakeControllerUVE();
    const EntityUVE pawn = MakePawnUVE();
    ASSERT_TRUE(PossessControllerUVE(entityManager, controller, pawn));
    RouteGameplayInputUVE(entityManager, SnapshotUVE());

    EXPECT_TRUE(UnpossessControllerUVE(entityManager, controller));
    EXPECT_EQ(entityManager.GetComponentUVE<ControllerComponentUVE>(controller).pawn,
              kInvalidEntityUVE);
    EXPECT_EQ(entityManager.GetComponentUVE<PawnComponentUVE>(pawn).controller, kInvalidEntityUVE);
    EXPECT_EQ(entityManager.GetComponentUVE<PawnComponentUVE>(pawn).input,
              Gameplay::GameplayInputUVE{});

    EXPECT_FALSE(UnpossessControllerUVE(entityManager, controller));
    EXPECT_FALSE(UnpossessControllerUVE(entityManager, entityManager.CreateEntityUVE()));
}

TEST_F(PawnUVETest, Route_DeliversToPlayerPawnsOnlyAndRepairsDanglingLinks) {
    const EntityUVE playerController = MakeControllerUVE(ControllerKindUVE::Player);
    const EntityUVE playerPawn = MakePawnUVE();
    const EntityUVE aiController = MakeControllerUVE(ControllerKindUVE::AI);
    const EntityUVE aiPawn = MakePawnUVE();
    ASSERT_TRUE(PossessControllerUVE(entityManager, playerController, playerPawn));
    ASSERT_TRUE(PossessControllerUVE(entityManager, aiController, aiPawn));

    RouteGameplayInputUVE(entityManager, SnapshotUVE());
    EXPECT_EQ(entityManager.GetComponentUVE<PawnComponentUVE>(playerPawn).input, SnapshotUVE());
    EXPECT_EQ(entityManager.GetComponentUVE<PawnComponentUVE>(aiPawn).input,
              Gameplay::GameplayInputUVE{});

    // A destroyed controller is repaired on the next route: link cleared, input purged.
    entityManager.DestroyEntityUVE(playerController);
    RouteGameplayInputUVE(entityManager, SnapshotUVE());
    EXPECT_EQ(entityManager.GetComponentUVE<PawnComponentUVE>(playerPawn).controller,
              kInvalidEntityUVE);
    EXPECT_EQ(entityManager.GetComponentUVE<PawnComponentUVE>(playerPawn).input,
              Gameplay::GameplayInputUVE{});

    // A removed pawn component is repaired from the controller side just the same.
    entityManager.RemoveComponentUVE<PawnComponentUVE>(aiPawn);
    RouteGameplayInputUVE(entityManager, SnapshotUVE());
    EXPECT_EQ(entityManager.GetComponentUVE<ControllerComponentUVE>(aiController).pawn,
              kInvalidEntityUVE);

    // A non-finite snapshot still repairs but never writes.
    const EntityUVE freshController = MakeControllerUVE();
    const EntityUVE freshPawn = MakePawnUVE();
    ASSERT_TRUE(PossessControllerUVE(entityManager, freshController, freshPawn));
    Gameplay::GameplayInputUVE broken = SnapshotUVE();
    broken.move.x = std::numeric_limits<float>::quiet_NaN();
    RouteGameplayInputUVE(entityManager, broken);
    EXPECT_EQ(entityManager.GetComponentUVE<PawnComponentUVE>(freshPawn).input,
              Gameplay::GameplayInputUVE{});
    EXPECT_EQ(FindPossessedPawnUVE(entityManager, freshController), freshPawn);
}

TEST_F(PawnUVETest, Finders_ReturnInvalidForAnythingButAMutualLink) {
    const EntityUVE controller = MakeControllerUVE();
    const EntityUVE pawn = MakePawnUVE();
    EXPECT_EQ(FindPossessedPawnUVE(entityManager, controller), kInvalidEntityUVE);
    EXPECT_EQ(FindPawnControllerUVE(entityManager, pawn), kInvalidEntityUVE);
    EXPECT_EQ(FindPossessedPawnUVE(entityManager, entityManager.CreateEntityUVE()),
              kInvalidEntityUVE);

    ASSERT_TRUE(PossessControllerUVE(entityManager, controller, pawn));
    EXPECT_EQ(FindPossessedPawnUVE(entityManager, controller), pawn);
    EXPECT_EQ(FindPawnControllerUVE(entityManager, pawn), controller);

    // A hand-broken one-sided link reads as no link at all.
    entityManager.GetComponentUVE<ControllerComponentUVE>(controller).pawn = MakePawnUVE();
    EXPECT_EQ(FindPossessedPawnUVE(entityManager, controller), kInvalidEntityUVE);
    EXPECT_EQ(FindPawnControllerUVE(entityManager, pawn), kInvalidEntityUVE);
}

TEST_F(PawnUVETest, Validity_RejectsBadKindsAndNonFiniteInput) {
    EXPECT_TRUE(IsControllerComponentValidUVE(ControllerComponentUVE{}));
    EXPECT_TRUE(IsPawnComponentValidUVE(PawnComponentUVE{}));

    ControllerComponentUVE badKind;
    badKind.kind = static_cast<ControllerKindUVE>(7U);
    EXPECT_FALSE(IsControllerComponentValidUVE(badKind));

    PawnComponentUVE bad;
    bad.input.move.x = std::numeric_limits<float>::quiet_NaN();
    EXPECT_FALSE(IsPawnComponentValidUVE(bad));
    bad.input = {};
    bad.input.move.y = std::numeric_limits<float>::infinity();
    EXPECT_FALSE(IsPawnComponentValidUVE(bad));
    bad.input = {};
    bad.input.rise = std::numeric_limits<float>::quiet_NaN();
    EXPECT_FALSE(IsPawnComponentValidUVE(bad));
    bad.input = {};
    bad.input.lookPointer.x = std::numeric_limits<float>::infinity();
    EXPECT_FALSE(IsPawnComponentValidUVE(bad));
    bad.input = {};
    bad.input.lookStick.y = std::numeric_limits<float>::quiet_NaN();
    EXPECT_FALSE(IsPawnComponentValidUVE(bad));
}

} // namespace
} // namespace UVE::Scene::Tests
