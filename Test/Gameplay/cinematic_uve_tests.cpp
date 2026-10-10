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


TEST(CinematicUVETest, Sample_ClampsToTheFirstAndLastKeys) {
    CinematicComponentUVE cinematic = MakeCinematicUVE();
    ASSERT_TRUE(AddCinematicCameraKeyUVE(cinematic, 2.0, Math::Vector3UVE{2.0F, 0.0F, 0.0F},
                                         Math::QuaternionUVE{}));
    ASSERT_TRUE(AddCinematicCameraKeyUVE(cinematic, 8.0, Math::Vector3UVE{8.0F, 0.0F, 0.0F},
                                         Math::QuaternionUVE{}));
    const auto before = SampleCinematicCameraUVE(cinematic, 0.0);
    ASSERT_TRUE(before.has_value());
    EXPECT_FLOAT_EQ(before->position.x, 2.0F);
    const auto after = SampleCinematicCameraUVE(cinematic, 10.0);
    ASSERT_TRUE(after.has_value());
    EXPECT_FLOAT_EQ(after->position.x, 8.0F);

    CinematicComponentUVE single = MakeCinematicUVE();
    ASSERT_TRUE(AddCinematicCameraKeyUVE(single, 5.0, Math::Vector3UVE{5.0F, 1.0F, 0.0F},
                                         Math::QuaternionUVE{}));
    const auto held = SampleCinematicCameraUVE(single, 9.0);
    ASSERT_TRUE(held.has_value());
    EXPECT_EQ(held->position, Math::Vector3UVE(5.0F, 1.0F, 0.0F));
}

TEST(CinematicUVETest, Sample_InterpolatesPositionLinearly) {
    CinematicComponentUVE cinematic = MakeCinematicUVE();
    ASSERT_TRUE(AddCinematicCameraKeyUVE(cinematic, 0.0, Math::Vector3UVE{0.0F, 0.0F, 0.0F},
                                         Math::QuaternionUVE{}));
    ASSERT_TRUE(AddCinematicCameraKeyUVE(cinematic, 10.0, Math::Vector3UVE{10.0F, 0.0F, 0.0F},
                                         Math::QuaternionUVE{}));
    const auto quarter = SampleCinematicCameraUVE(cinematic, 2.5);
    ASSERT_TRUE(quarter.has_value());
    EXPECT_FLOAT_EQ(quarter->position.x, 2.5F);
    EXPECT_FLOAT_EQ(quarter->position.y, 0.0F);
    EXPECT_EQ(quarter->rotation, Math::QuaternionUVE{});
}

TEST(CinematicUVETest, Sample_SlerpsRotationBetweenKeys) {
    CinematicComponentUVE cinematic = MakeCinematicUVE();
    ASSERT_TRUE(AddCinematicCameraKeyUVE(cinematic, 0.0, Math::Vector3UVE{}, Math::QuaternionUVE{}));
    // 90 degrees about Y.
    ASSERT_TRUE(AddCinematicCameraKeyUVE(cinematic, 10.0, Math::Vector3UVE{},
                                         Math::QuaternionUVE{0.0F, 0.70710678F, 0.0F, 0.70710678F}));
    const auto half = SampleCinematicCameraUVE(cinematic, 5.0);
    ASSERT_TRUE(half.has_value());
    // 45 degrees about Y: (0, sin 22.5, 0, cos 22.5).
    EXPECT_NEAR(half->rotation.x, 0.0, 1e-5);
    EXPECT_NEAR(half->rotation.y, 0.38268343, 1e-5);
    EXPECT_NEAR(half->rotation.z, 0.0, 1e-5);
    EXPECT_NEAR(half->rotation.w, 0.92387953, 1e-5);
}

TEST(CinematicUVETest, Sample_EmptyTrackAndBadTimeReturnNullopt) {
    const CinematicComponentUVE bare = MakeCinematicUVE();
    EXPECT_FALSE(SampleCinematicCameraUVE(bare, 1.0).has_value());
    CinematicComponentUVE cinematic = MakeCinematicUVE();
    ASSERT_TRUE(AddCinematicCameraKeyUVE(cinematic, 1.0, Math::Vector3UVE{}, Math::QuaternionUVE{}));
    EXPECT_FALSE(
        SampleCinematicCameraUVE(cinematic, std::numeric_limits<double>::quiet_NaN()).has_value());
}

TEST(CinematicUVETest, Add_NormalizesRotationAndRejectsBadKeys) {
    CinematicComponentUVE cinematic = MakeCinematicUVE();
    ASSERT_TRUE(AddCinematicCameraKeyUVE(cinematic, 1.0, Math::Vector3UVE{},
                                         Math::QuaternionUVE{0.0F, 0.0F, 0.0F, 2.0F}));
    EXPECT_EQ(cinematic.cameraKeys.front().rotation, Math::QuaternionUVE{});
    EXPECT_FALSE(AddCinematicCameraKeyUVE(cinematic, 1.0, Math::Vector3UVE{},
                                          Math::QuaternionUVE{0.0F, 0.0F, 0.0F, 0.0F}));
    EXPECT_FALSE(AddCinematicCameraKeyUVE(
        cinematic, 1.0, Math::Vector3UVE{std::numeric_limits<float>::quiet_NaN(), 0.0F, 0.0F},
        Math::QuaternionUVE{}));
    EXPECT_FALSE(AddCinematicCameraKeyUVE(cinematic, -1.0, Math::Vector3UVE{}, Math::QuaternionUVE{}));
    EXPECT_FALSE(AddCinematicCameraKeyUVE(cinematic, 11.0, Math::Vector3UVE{}, Math::QuaternionUVE{}));

    CinematicComponentUVE full = MakeCinematicUVE();
    for (std::size_t index = 0U; index < kMaximumCinematicCameraKeysUVE; ++index) {
        ASSERT_TRUE(AddCinematicCameraKeyUVE(full, static_cast<double>(index) * 0.1,
                                             Math::Vector3UVE{}, Math::QuaternionUVE{}));
    }
    EXPECT_FALSE(
        AddCinematicCameraKeyUVE(full, 9.9, Math::Vector3UVE{}, Math::QuaternionUVE{}));
}

TEST(CinematicUVETest, RemoveCameraKey_RespectsBounds) {
    CinematicComponentUVE cinematic = MakeCinematicUVE();
    ASSERT_TRUE(AddCinematicCameraKeyUVE(cinematic, 1.0, Math::Vector3UVE{}, Math::QuaternionUVE{}));
    EXPECT_FALSE(RemoveCinematicCameraKeyUVE(cinematic, 1U));
    EXPECT_TRUE(RemoveCinematicCameraKeyUVE(cinematic, 0U));
    EXPECT_TRUE(cinematic.cameraKeys.empty());
}

TEST(CinematicUVETest, Step_CarriesTheSampledCameraPose) {
    CinematicComponentUVE cinematic = MakeCinematicUVE();
    ASSERT_TRUE(AddCinematicCameraKeyUVE(cinematic, 0.0, Math::Vector3UVE{0.0F, 0.0F, 0.0F},
                                         Math::QuaternionUVE{}));
    ASSERT_TRUE(AddCinematicCameraKeyUVE(cinematic, 10.0, Math::Vector3UVE{10.0F, 0.0F, 0.0F},
                                         Math::QuaternionUVE{}));
    static_cast<void>(PlayCinematicUVE(cinematic));
    const CinematicStepResultUVE result = StepCinematicUVE(cinematic, 2.5F);
    ASSERT_TRUE(result.cameraPose.has_value());
    EXPECT_FLOAT_EQ(result.cameraPose->position.x, 2.5F);

    CinematicComponentUVE bare = MakeCinematicUVE();
    static_cast<void>(PlayCinematicUVE(bare));
    EXPECT_FALSE(StepCinematicUVE(bare, 1.0F).cameraPose.has_value());
}

TEST(CinematicUVETest, Write_PosesTransformAndMirrorsEuler) {
    TransformComponentUVE transform;
    CinematicCameraPoseUVE pose;
    pose.position = Math::Vector3UVE{1.0F, 2.0F, 3.0F};
    pose.rotation = Math::QuaternionUVE{0.0F, 0.70710678F, 0.0F, 0.70710678F};
    WriteCinematicCameraPoseUVE(pose, transform);
    EXPECT_EQ(transform.localPosition, pose.position);
    // The writer re-normalizes, so the stored rotation matches to float dust, not bitwise.
    EXPECT_NEAR(transform.localRotation.x, pose.rotation.x, 1e-6);
    EXPECT_NEAR(transform.localRotation.y, pose.rotation.y, 1e-6);
    EXPECT_NEAR(transform.localRotation.z, pose.rotation.z, 1e-6);
    EXPECT_NEAR(transform.localRotation.w, pose.rotation.w, 1e-6);
    // The mirrored Euler angles rebuild the same rotation: the Inspector shows what is on screen.
    // The quaternion-Euler-quaternion detour costs ~5e-4 here - the same price the animation
    // writer pays every frame, and a fraction of a pixel on screen.
    ASSERT_TRUE(TrySyncRotationFromEulerUVE(transform));
    EXPECT_NEAR(transform.localRotation.x, pose.rotation.x, 1e-3);
    EXPECT_NEAR(transform.localRotation.y, pose.rotation.y, 1e-3);
    EXPECT_NEAR(transform.localRotation.z, pose.rotation.z, 1e-3);
    EXPECT_NEAR(transform.localRotation.w, pose.rotation.w, 1e-3);

    const Math::QuaternionUVE rebuilt = transform.localRotation;
    CinematicCameraPoseUVE degenerate;
    degenerate.position = Math::Vector3UVE{9.0F, 9.0F, 9.0F};
    degenerate.rotation = Math::QuaternionUVE{0.0F, 0.0F, 0.0F, 0.0F};
    WriteCinematicCameraPoseUVE(degenerate, transform);
    EXPECT_EQ(transform.localPosition, degenerate.position);
    EXPECT_EQ(transform.localRotation, rebuilt);
}

TEST(CinematicUVETest, Validity_RejectsBrokenCameraTracks) {
    CinematicComponentUVE unsorted = MakeCinematicUVE();
    ASSERT_TRUE(AddCinematicCameraKeyUVE(unsorted, 5.0, Math::Vector3UVE{}, Math::QuaternionUVE{}));
    unsorted.cameraKeys.push_back(CinematicCameraKeyUVE{1.0, Math::Vector3UVE{}, Math::QuaternionUVE{}});
    EXPECT_FALSE(IsCinematicComponentValidUVE(unsorted));

    CinematicComponentUVE denormal = MakeCinematicUVE();
    denormal.cameraKeys.push_back(
        CinematicCameraKeyUVE{1.0, Math::Vector3UVE{}, Math::QuaternionUVE{0.0F, 0.0F, 0.0F, 2.0F}});
    EXPECT_FALSE(IsCinematicComponentValidUVE(denormal));

    CinematicComponentUVE badPosition = MakeCinematicUVE();
    badPosition.cameraKeys.push_back(CinematicCameraKeyUVE{
        1.0, Math::Vector3UVE{std::numeric_limits<float>::infinity(), 0.0F, 0.0F},
        Math::QuaternionUVE{}});
    EXPECT_FALSE(IsCinematicComponentValidUVE(badPosition));
}

} // namespace UVE::Scene::Tests
