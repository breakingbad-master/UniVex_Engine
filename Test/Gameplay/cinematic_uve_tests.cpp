// Copyright (c) 2026 UniVex Studios. All Rights Reserved.

#include "uve/gameplay/cinematic_uve.h"

#include <limits>

#include <gtest/gtest.h>

namespace UVE::Scene::Tests {

namespace {

CinematicComponentUVE MakeCinematicUVE() {
    CinematicComponentUVE cinematic;
    cinematic.durationSeconds = 10.0;
    return cinematic;
}

EntityUVE MakeCameraUVE(std::uint32_t id) {
    EntityUVE camera;
    camera.index = id;
    camera.generation = 1U;
    return camera;
}

} // namespace

TEST(CinematicUVETest, Play_FiresTimeZeroKeysAndRestarts) {
    CinematicComponentUVE cinematic = MakeCinematicUVE();
    ASSERT_TRUE(AddCinematicEventUVE(cinematic, 0.0, "open"));
    ASSERT_TRUE(AddCinematicEventUVE(cinematic, 5.0, "later"));
    const std::vector<std::string> fired = PlayCinematicUVE(cinematic);
    EXPECT_EQ(fired, (std::vector<std::string>{"open"}));
    EXPECT_TRUE(cinematic.isPlaying);
    EXPECT_FALSE(cinematic.finished);
    EXPECT_DOUBLE_EQ(cinematic.currentTimeSeconds, 0.0);

    CinematicComponentUVE broken;
    EXPECT_TRUE(PlayCinematicUVE(broken).empty());
}

TEST(CinematicUVETest, Step_FiresKeysInPassingOrder) {
    CinematicComponentUVE cinematic = MakeCinematicUVE();
    ASSERT_TRUE(AddCinematicEventUVE(cinematic, 3.0, "third"));
    ASSERT_TRUE(AddCinematicEventUVE(cinematic, 1.0, "first"));
    ASSERT_TRUE(AddCinematicEventUVE(cinematic, 2.0, "second"));
    static_cast<void>(PlayCinematicUVE(cinematic));
    const CinematicStepResultUVE result = StepCinematicUVE(cinematic, 2.5F);
    EXPECT_EQ(result.firedEventIds, (std::vector<std::string>{"first", "second"}));
    EXPECT_DOUBLE_EQ(cinematic.currentTimeSeconds, 2.5);
    EXPECT_TRUE(cinematic.isPlaying);
}

TEST(CinematicUVETest, Step_OnceParksAtTheEnd) {
    CinematicComponentUVE cinematic = MakeCinematicUVE();
    ASSERT_TRUE(AddCinematicEventUVE(cinematic, 9.0, "near-end"));
    static_cast<void>(PlayCinematicUVE(cinematic));
    const CinematicStepResultUVE last = StepCinematicUVE(cinematic, 50.0F);
    EXPECT_EQ(last.firedEventIds, (std::vector<std::string>{"near-end"}));
    EXPECT_TRUE(last.justFinished);
    EXPECT_FALSE(cinematic.isPlaying);
    EXPECT_TRUE(cinematic.finished);
    EXPECT_DOUBLE_EQ(cinematic.currentTimeSeconds, 10.0);
    EXPECT_TRUE(StepCinematicUVE(cinematic, 1.0F).firedEventIds.empty());
}

TEST(CinematicUVETest, Step_LoopWrapsAndRefiresInOrder) {
    CinematicComponentUVE cinematic = MakeCinematicUVE();
    cinematic.durationSeconds = 2.0;
    cinematic.loopMode = CinematicLoopModeUVE::Loop;
    ASSERT_TRUE(AddCinematicEventUVE(cinematic, 0.5, "early"));
    ASSERT_TRUE(AddCinematicEventUVE(cinematic, 1.5, "late"));
    static_cast<void>(PlayCinematicUVE(cinematic));
    const CinematicStepResultUVE first = StepCinematicUVE(cinematic, 1.2F);
    EXPECT_EQ(first.firedEventIds, (std::vector<std::string>{"early"}));
    const CinematicStepResultUVE wrapped = StepCinematicUVE(cinematic, 1.6F);
    EXPECT_EQ(wrapped.firedEventIds, (std::vector<std::string>{"late", "early"}));
    EXPECT_NEAR(cinematic.currentTimeSeconds, 0.8, 1e-6);
    EXPECT_TRUE(cinematic.isPlaying);
    EXPECT_FALSE(wrapped.justFinished);
}

TEST(CinematicUVETest, Step_ResolvesCameraCuts) {
    CinematicComponentUVE cinematic = MakeCinematicUVE();
    const EntityUVE camA = MakeCameraUVE(11U);
    const EntityUVE camB = MakeCameraUVE(22U);
    ASSERT_TRUE(AddCinematicCutUVE(cinematic, 0.0, camA));
    ASSERT_TRUE(AddCinematicCutUVE(cinematic, 5.0, camB));
    static_cast<void>(PlayCinematicUVE(cinematic));
    EXPECT_EQ(StepCinematicUVE(cinematic, 3.0F).activeCamera, camA);
    EXPECT_EQ(StepCinematicUVE(cinematic, 4.0F).activeCamera, camB);

    CinematicComponentUVE noCuts = MakeCinematicUVE();
    static_cast<void>(PlayCinematicUVE(noCuts));
    EXPECT_EQ(StepCinematicUVE(noCuts, 1.0F).activeCamera, kInvalidEntityUVE);
}

TEST(CinematicUVETest, Add_InsertsSortedAndRejectsBadKeys) {
    CinematicComponentUVE cinematic = MakeCinematicUVE();
    EXPECT_FALSE(AddCinematicEventUVE(cinematic, -1.0, "x"));
    EXPECT_FALSE(AddCinematicEventUVE(cinematic, 11.0, "x"));
    EXPECT_FALSE(AddCinematicEventUVE(cinematic, 1.0, ""));
    EXPECT_FALSE(AddCinematicEventUVE(cinematic, 1.0, std::string(65U, 'x')));
    EXPECT_FALSE(AddCinematicCutUVE(cinematic, 1.0, kInvalidEntityUVE));
    EXPECT_FALSE(AddCinematicCutUVE(cinematic, 11.0, MakeCameraUVE(1U)));
    ASSERT_TRUE(AddCinematicEventUVE(cinematic, 5.0, "b"));
    ASSERT_TRUE(AddCinematicEventUVE(cinematic, 5.0, "c"));
    ASSERT_TRUE(AddCinematicEventUVE(cinematic, 1.0, "a"));
    ASSERT_EQ(cinematic.events.size(), 3U);
    EXPECT_EQ(cinematic.events[0].eventId, "a");
    EXPECT_EQ(cinematic.events[1].eventId, "b");
    EXPECT_EQ(cinematic.events[2].eventId, "c");
}

TEST(CinematicUVETest, Remove_RespectsBounds) {
    CinematicComponentUVE cinematic = MakeCinematicUVE();
    ASSERT_TRUE(AddCinematicEventUVE(cinematic, 1.0, "a"));
    EXPECT_FALSE(RemoveCinematicEventUVE(cinematic, 1U));
    EXPECT_TRUE(RemoveCinematicEventUVE(cinematic, 0U));
    EXPECT_FALSE(RemoveCinematicCutUVE(cinematic, 0U));
}

TEST(CinematicUVETest, Step_NegativeSpeedRewindsWithoutFiring) {
    CinematicComponentUVE cinematic = MakeCinematicUVE();
    cinematic.speed = -1.0F;
    ASSERT_TRUE(AddCinematicEventUVE(cinematic, 4.0, "mid"));
    ASSERT_TRUE(AddCinematicCutUVE(cinematic, 0.0, MakeCameraUVE(7U)));
    static_cast<void>(PlayCinematicUVE(cinematic));
    cinematic.currentTimeSeconds = 5.0;
    const CinematicStepResultUVE result = StepCinematicUVE(cinematic, 2.0F);
    EXPECT_TRUE(result.firedEventIds.empty());
    EXPECT_DOUBLE_EQ(cinematic.currentTimeSeconds, 3.0);
    EXPECT_EQ(result.activeCamera, MakeCameraUVE(7U));
}

TEST(CinematicUVETest, Scrub_ResolvesCameraWithoutMoving) {
    CinematicComponentUVE cinematic = MakeCinematicUVE();
    ASSERT_TRUE(AddCinematicCutUVE(cinematic, 2.0, MakeCameraUVE(3U)));
    EXPECT_EQ(ActiveCinematicCameraUVE(cinematic, 1.0), kInvalidEntityUVE);
    EXPECT_EQ(ActiveCinematicCameraUVE(cinematic, 2.0), MakeCameraUVE(3U));
    EXPECT_EQ(ActiveCinematicCameraUVE(cinematic, 9.0), MakeCameraUVE(3U));
    EXPECT_EQ(ActiveCinematicCameraUVE(cinematic, std::numeric_limits<double>::quiet_NaN()),
              kInvalidEntityUVE);
    EXPECT_DOUBLE_EQ(cinematic.currentTimeSeconds, 0.0);
}

TEST(CinematicUVETest, Validity_RejectsBrokenTimelines) {
    CinematicComponentUVE good = MakeCinematicUVE();
    ASSERT_TRUE(AddCinematicEventUVE(good, 1.0, "a"));
    EXPECT_TRUE(IsCinematicComponentValidUVE(good));

    CinematicComponentUVE noDuration = good;
    noDuration.durationSeconds = 0.0;
    EXPECT_FALSE(IsCinematicComponentValidUVE(noDuration));

    CinematicComponentUVE badSpeed = good;
    badSpeed.speed = std::numeric_limits<float>::quiet_NaN();
    EXPECT_FALSE(IsCinematicComponentValidUVE(badSpeed));

    CinematicComponentUVE unsorted = good;
    unsorted.events.push_back(CinematicEventKeyUVE{0.5, "early-late"});
    EXPECT_FALSE(IsCinematicComponentValidUVE(unsorted));

    CinematicComponentUVE overfull = MakeCinematicUVE();
    for (std::size_t index = 0U; index <= kMaximumCinematicEventsUVE; ++index) {
        overfull.events.push_back(CinematicEventKeyUVE{1.0, "e" + std::to_string(index)});
    }
    EXPECT_FALSE(IsCinematicComponentValidUVE(overfull));
}

} // namespace UVE::Scene::Tests
