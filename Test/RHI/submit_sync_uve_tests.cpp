// Copyright (c) 2026 UniVex Studios. All Rights Reserved.
//
// Submission-level (binary) semaphore sync contract tests, all against the Null device — the
// backend that models the frame boundary without a GPU. What makes these cases genuine rather
// than decorative is that Null keeps real signal/wait state: a recorded signal becomes
// consumable exactly at PresentUVE(), a wait spends that signal, and a wait nothing has
// signalled is refused instead of "succeeding" against nothing. The capability honesty checks
// live here too, so a backend cannot claim submit sync implicitly.
//
// The equivalent native assertions (a real VkSemaphore pair driving two frame submissions on
// lavapipe) live in Test/RHI/Vulkan/vulkan_render_device_uve_tests.cpp; these cases pin the
// shared rules every submitting backend must agree on.

#include "uve/rhi_null/null_render_device_uve.h"

#include <cstdint>
#include <memory>
#include <variant>

#include <gtest/gtest.h>

namespace UVE::Render::Tests {
namespace {

using ResultUVE = RenderOperationResultUVE;

SubmitSyncUVE MakeSyncUVE(const std::initializer_list<BinarySemaphoreHandleUVE> waits,
                         const std::initializer_list<BinarySemaphoreHandleUVE> signals,
                         const SubmitWaitStageUVE stage = SubmitWaitStageUVE::Transfer) {
    SubmitSyncUVE sync;
    for (const BinarySemaphoreHandleUVE wait : waits) {
        sync.waitStages[sync.waitCount] = stage;
        sync.waits[sync.waitCount++] = wait;
    }
    for (const BinarySemaphoreHandleUVE signal : signals)
        sync.signals[sync.signalCount++] = signal;
    return sync;
}

TEST(SubmitSyncUVETest, UnsupportedByDefaultAndShapeRulesAreBackendIndependent) {
    EXPECT_EQ(RenderDeviceCapabilitiesUVE{}.submitSync, RenderFeatureSupportUVE::Unsupported);
    EXPECT_EQ(kInvalidBinarySemaphoreHandleUVE.value, 0U);

    EXPECT_TRUE(IsSubmitSyncShapeValidUVE(SubmitSyncUVE{}));
    EXPECT_TRUE(IsSubmitSyncEmptyUVE(SubmitSyncUVE{}));
    EXPECT_TRUE(IsSubmitWaitStageValidUVE(SubmitWaitStageUVE::Transfer));
    EXPECT_TRUE(IsSubmitWaitStageValidUVE(SubmitWaitStageUVE::ColorAttachment));
    EXPECT_TRUE(IsSubmitWaitStageValidUVE(SubmitWaitStageUVE::AllCommands));
    // The stage vocabulary is closed: bit combinations and out-of-range values are refused
    // rather than silently mapped onto one backend's stage model.
    EXPECT_FALSE(IsSubmitWaitStageValidUVE(static_cast<SubmitWaitStageUVE>(0U)));
    EXPECT_FALSE(IsSubmitWaitStageValidUVE(static_cast<SubmitWaitStageUVE>(3U)));
    EXPECT_FALSE(IsSubmitWaitStageValidUVE(static_cast<SubmitWaitStageUVE>(8U)));
    EXPECT_FALSE(IsSubmitWaitStageValidUVE(static_cast<SubmitWaitStageUVE>(0xFFU)));

    SubmitSyncUVE overfilled;
    overfilled.waitCount = SubmitSyncUVE::kMaxWaitsUVE + 1U;
    EXPECT_FALSE(IsSubmitSyncShapeValidUVE(overfilled));
    SubmitSyncUVE missingStage;
    missingStage.waitCount = 1U; // stage left at the zero value
    EXPECT_FALSE(IsSubmitSyncShapeValidUVE(missingStage));
    EXPECT_FALSE(IsSubmitSyncEmptyUVE(missingStage));
    SubmitSyncUVE tooManySignals;
    tooManySignals.signalCount = SubmitSyncUVE::kMaxSignalsUVE + 1U;
    EXPECT_FALSE(IsSubmitSyncShapeValidUVE(tooManySignals));

    // Repeated handles inside ONE call are structural, so they are refused before any backend
    // looks at state: two waits would spend two credits for one signal, and a native submit may
    // not name one semaphore twice. This is the in-call twin of the per-frame role rule.
    const BinarySemaphoreHandleUVE handle{7U};
    EXPECT_FALSE(HasSubmitSyncRepeatedHandleUVE(SubmitSyncUVE{}));
    EXPECT_TRUE(HasSubmitSyncRepeatedHandleUVE(MakeSyncUVE({handle}, {handle})));
    EXPECT_TRUE(HasSubmitSyncRepeatedHandleUVE(MakeSyncUVE({handle, handle}, {})));
    EXPECT_TRUE(HasSubmitSyncRepeatedHandleUVE(MakeSyncUVE({}, {handle, handle})));
    EXPECT_TRUE(HasSubmitSyncRepeatedHandleUVE(MakeSyncUVE({handle}, {handle})));
    EXPECT_FALSE(IsSubmitSyncShapeValidUVE(MakeSyncUVE({handle, handle}, {})));
    EXPECT_FALSE(IsSubmitSyncShapeValidUVE(MakeSyncUVE({handle}, {handle})));
}

TEST(SubmitSyncUVETest, NullRefusesEveryShapeAndHandleProblemWithoutTouchingTheFrame) {
    NullRenderDeviceUVE device;
    EXPECT_EQ(device.GetCapabilitiesUVE().submitSync, RenderFeatureSupportUVE::Model);
    ASSERT_NE(device.CreateCommandBufferUVE(), nullptr); // the backend really hands out buffers

    // Shape problems first: capacity and an undefined wait stage.
    SubmitSyncUVE overfilled;
    overfilled.signalCount = SubmitSyncUVE::kMaxSignalsUVE + 1U;
    EXPECT_EQ(device.SubmitWithSyncUVE(device.CreateCommandBufferUVE(), overfilled),
              ResultUVE::InvalidArgument);
    SubmitSyncUVE missingStage;
    missingStage.waitCount = 1U;
    missingStage.waits[0] = BinarySemaphoreHandleUVE{1U};
    EXPECT_EQ(device.SubmitWithSyncUVE(device.CreateCommandBufferUVE(), missingStage),
              ResultUVE::InvalidArgument);
    // A null or foreign command buffer is reported instead of silently ignored: submit is what
    // makes a caller's ordering real, so dropping it must be visible.
    EXPECT_EQ(device.SubmitWithSyncUVE(nullptr, SubmitSyncUVE{}), ResultUVE::InvalidArgument);
    EXPECT_EQ(device.SubmitWithSyncUVE(device.CreateCommandBufferUVE(),
                                      MakeSyncUVE({}, {BinarySemaphoreHandleUVE{777U}})),
              ResultUVE::InvalidHandle);
    EXPECT_EQ(device.SubmitWithSyncUVE(device.CreateCommandBufferUVE(),
                                      MakeSyncUVE({BinarySemaphoreHandleUVE{777U}}, {})),
              ResultUVE::InvalidHandle);
    // Empty sync is exactly SubmitUVE and is accepted with no sync bookkeeping attached.
    EXPECT_EQ(device.SubmitWithSyncUVE(device.CreateCommandBufferUVE(), SubmitSyncUVE{}),
              ResultUVE::Recorded);
    EXPECT_EQ(device.GetLiveResourceCountUVE(), 0U) << "an empty sync creates no resources";
}

TEST(SubmitSyncUVETest, ASignalBecomesConsumableOnlyWhenItsFrameIsPresented) {
    NullRenderDeviceUVE device;
    BinarySemaphoreHandleUVE semaphore;
    ASSERT_EQ(device.CreateBinarySemaphoreUVE(semaphore), ResultUVE::Recorded);
    EXPECT_NE(semaphore, kInvalidBinarySemaphoreHandleUVE);

    // Nothing has been submitted yet: waiting now would wait forever, so it is refused.
    EXPECT_EQ(device.SubmitWithSyncUVE(device.CreateCommandBufferUVE(), MakeSyncUVE({semaphore}, {})),
              ResultUVE::InvalidState);

    ASSERT_EQ(device.SubmitWithSyncUVE(device.CreateCommandBufferUVE(), MakeSyncUVE({}, {semaphore})),
              ResultUVE::Recorded);
    // Recorded but not yet submitted: the frame that carries the signal has not presented, so a
    // wait in that same frame is the self-wait the one-role rule refuses.
    EXPECT_EQ(device.SubmitWithSyncUVE(device.CreateCommandBufferUVE(), MakeSyncUVE({semaphore}, {})),
              ResultUVE::InvalidArgument)
        << "a signal recorded in the same frame must not satisfy a wait in that frame";

    device.PresentUVE();
    // Credited by the submit: the next frame may wait on it, and only once.
    ASSERT_EQ(device.SubmitWithSyncUVE(device.CreateCommandBufferUVE(), MakeSyncUVE({semaphore}, {})),
              ResultUVE::Recorded);
    device.PresentUVE();
    EXPECT_EQ(device.SubmitWithSyncUVE(device.CreateCommandBufferUVE(), MakeSyncUVE({semaphore}, {})),
              ResultUVE::InvalidState)
        << "one signal is consumed by exactly one wait";
    EXPECT_EQ(device.WaitIdleUVE(), ResultUVE::Recorded);
    EXPECT_EQ(device.DestroyBinarySemaphoreUVE(semaphore), ResultUVE::Success);
    EXPECT_EQ(device.DestroyBinarySemaphoreUVE(semaphore), ResultUVE::InvalidHandle);
}

TEST(SubmitSyncUVETest, OneRolePerSemaphorePerFrameAndUnconsumedSignalsBlockResignals) {
    NullRenderDeviceUVE device;
    BinarySemaphoreHandleUVE semaphore;
    ASSERT_EQ(device.CreateBinarySemaphoreUVE(semaphore), ResultUVE::Recorded);

    ASSERT_EQ(device.SubmitWithSyncUVE(device.CreateCommandBufferUVE(), MakeSyncUVE({}, {semaphore})),
              ResultUVE::Recorded);
    // Same frame: a second signal, a wait on it, or a duplicate wait all break the one-role rule.
    EXPECT_EQ(device.SubmitWithSyncUVE(device.CreateCommandBufferUVE(), MakeSyncUVE({}, {semaphore})),
              ResultUVE::InvalidArgument);
    EXPECT_EQ(device.SubmitWithSyncUVE(device.CreateCommandBufferUVE(), MakeSyncUVE({semaphore}, {})),
              ResultUVE::InvalidArgument)
        << "waiting on the semaphore this frame signals is a self-wait";
    EXPECT_EQ(device.SubmitWithSyncUVE(device.CreateCommandBufferUVE(),
                                      MakeSyncUVE({semaphore}, {}, SubmitWaitStageUVE::ColorAttachment)),
              ResultUVE::InvalidArgument)
        << "the first wait already claimed this semaphore for the frame";
    EXPECT_EQ(device.DestroyBinarySemaphoreUVE(semaphore), ResultUVE::InvalidState)
        << "a frame that still names the semaphore must keep it alive";

    device.PresentUVE();
    // The signal is submitted and still unconsumed, so signalling again is invalid usage —
    // the honest refusal, since a real driver rejects it too.
    EXPECT_EQ(device.SubmitWithSyncUVE(device.CreateCommandBufferUVE(), MakeSyncUVE({}, {semaphore})),
              ResultUVE::InvalidState);
    // Waiting consumes it; only then may it be signalled again.
    ASSERT_EQ(device.SubmitWithSyncUVE(device.CreateCommandBufferUVE(), MakeSyncUVE({semaphore}, {})),
              ResultUVE::Recorded);
    device.PresentUVE();
    // A submitted frame still names it, so it is not destroyable until that frame completes.
    EXPECT_EQ(device.DestroyBinarySemaphoreUVE(semaphore), ResultUVE::InvalidState);
    EXPECT_EQ(device.WaitIdleUVE(), ResultUVE::Recorded);
    ASSERT_EQ(device.SubmitWithSyncUVE(device.CreateCommandBufferUVE(), MakeSyncUVE({}, {semaphore})),
              ResultUVE::Recorded);
    device.PresentUVE();
    EXPECT_EQ(device.WaitIdleUVE(), ResultUVE::Recorded);
    EXPECT_EQ(device.DestroyBinarySemaphoreUVE(semaphore), ResultUVE::Success);
}

TEST(SubmitSyncUVETest, RefusedSubmissionsLeaveTheFrameAndItsCreditsExactlyAsTheyWere) {
    NullRenderDeviceUVE device;
    BinarySemaphoreHandleUVE first, second;
    ASSERT_EQ(device.CreateBinarySemaphoreUVE(first), ResultUVE::Recorded);
    ASSERT_EQ(device.CreateBinarySemaphoreUVE(second), ResultUVE::Recorded);

    auto commands = device.CreateCommandBufferUVE();
    ASSERT_EQ(device.SubmitWithSyncUVE(std::move(commands), MakeSyncUVE({}, {first})),
              ResultUVE::Recorded);
    device.PresentUVE();

    // A call that refuses because of its SECOND handle must not have claimed its first: the
    // whole sync is validated before anything is spent. Otherwise `first` would be left waiting
    // for a frame it never joined, and its credit would leak.
    commands = device.CreateCommandBufferUVE();
    EXPECT_EQ(device.SubmitWithSyncUVE(std::move(commands), MakeSyncUVE({first}, {first})),
              ResultUVE::InvalidArgument);
    commands = device.CreateCommandBufferUVE();
    ASSERT_EQ(device.SubmitWithSyncUVE(std::move(commands), MakeSyncUVE({first}, {second})),
              ResultUVE::Recorded)
        << "the refusal above must not have consumed first's signal";
    device.PresentUVE();

    // And the refused wait never became a frame: first's signal was really consumed by the
    // accepted call, so no credit is left for a third wait.
    commands = device.CreateCommandBufferUVE();
    EXPECT_EQ(device.SubmitWithSyncUVE(std::move(commands), MakeSyncUVE({first}, {})),
              ResultUVE::InvalidState);
    // That frame carried no sync at all (its only wait was refused), so presenting it releases
    // the previous frame's uses: both semaphores are destroyable, and neither kept a credit the
    // refused call should not have left behind.
    device.PresentUVE();
    EXPECT_EQ(device.DestroyBinarySemaphoreUVE(first), ResultUVE::Success);
    EXPECT_EQ(device.DestroyBinarySemaphoreUVE(second), ResultUVE::Success);
}

TEST(SubmitSyncUVETest, AcceptedSyncIsRecordedWithItsExactArgumentsAndCountsAsALiveResource) {
    NullRenderDeviceUVE device;
    BinarySemaphoreHandleUVE wait, signal;
    ASSERT_EQ(device.CreateBinarySemaphoreUVE(wait), ResultUVE::Recorded);
    ASSERT_EQ(device.CreateBinarySemaphoreUVE(signal), ResultUVE::Recorded);
    EXPECT_EQ(device.GetLiveResourceCountUVE(), 2U) << "binary semaphores are live resources";

    auto commands = device.CreateCommandBufferUVE();
    ASSERT_EQ(device.SubmitWithSyncUVE(std::move(commands),
                                      MakeSyncUVE({}, {wait}, SubmitWaitStageUVE::ColorAttachment)),
              ResultUVE::Recorded);
    device.PresentUVE();
    commands = device.CreateCommandBufferUVE();
    ASSERT_EQ(device.SubmitWithSyncUVE(std::move(commands),
                                      MakeSyncUVE({wait}, {signal}, SubmitWaitStageUVE::AllCommands)),
              ResultUVE::Recorded);

    bool found = false;
    for (const RecordedDeviceOperationUVE &operation : device.GetRecordedDeviceOperationsUVE()) {
        const auto *recorded = std::get_if<SubmitSyncOperationUVE>(&operation);
        if (recorded == nullptr || recorded->sync.waitCount != 1U || recorded->sync.signalCount != 1U)
            continue;
        ASSERT_EQ(recorded->sync.waitCount, 1U);
        EXPECT_EQ(recorded->sync.waits[0], wait);
        EXPECT_EQ(recorded->sync.waitStages[0], SubmitWaitStageUVE::AllCommands);
        EXPECT_EQ(recorded->sync.signals[0], signal);
        found = true;
    }
    EXPECT_TRUE(found) << "the accepted sync must be visible in the device operation history";
    device.PresentUVE();
    EXPECT_EQ(device.WaitIdleUVE(), ResultUVE::Recorded);
    EXPECT_EQ(device.DestroyBinarySemaphoreUVE(wait), ResultUVE::Success);
    EXPECT_EQ(device.DestroyBinarySemaphoreUVE(signal), ResultUVE::Success);
    EXPECT_EQ(device.GetLiveResourceCountUVE(), 0U);
}

TEST(SubmitSyncUVETest, ConcurrentSubmissionsDoNotCorruptTheFrame) {
    // SubmitUVE's threading contract applies here too: sync is recorded under the same
    // submission lock, so worker threads submitting their own command buffers cannot lose or
    // duplicate a frame's sync bookkeeping.
    NullRenderDeviceUVE device;
    BinarySemaphoreHandleUVE signal;
    ASSERT_EQ(device.CreateBinarySemaphoreUVE(signal), ResultUVE::Recorded);

    std::atomic<int> accepted{0};
    std::vector<std::thread> workers;
    for (int worker = 0; worker < 4; ++worker) {
        workers.emplace_back([&device, &accepted]() {
            auto commands = device.CreateCommandBufferUVE();
            if (device.SubmitWithSyncUVE(std::move(commands), SubmitSyncUVE{}) == ResultUVE::Recorded)
                accepted.fetch_add(1, std::memory_order_relaxed);
        });
    }
    for (std::thread &worker : workers)
        worker.join();
    EXPECT_EQ(accepted.load(std::memory_order_relaxed), 4);
    device.PresentUVE();

    // The single-role rule is still enforced after the concurrent traffic above.
    auto commands = device.CreateCommandBufferUVE();
    ASSERT_EQ(device.SubmitWithSyncUVE(std::move(commands), MakeSyncUVE({}, {signal})),
              ResultUVE::Recorded);
    device.PresentUVE();
    EXPECT_EQ(device.WaitIdleUVE(), ResultUVE::Recorded);
    EXPECT_EQ(device.DestroyBinarySemaphoreUVE(signal), ResultUVE::Success);
}

} // namespace
} // namespace UVE::Render::Tests
