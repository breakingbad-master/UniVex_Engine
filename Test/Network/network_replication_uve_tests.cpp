// Copyright (c) 2026 UniVex Studios. All Rights Reserved.

#include "uve/network/network_replication_uve.h"

#include <cstdint>
#include <map>
#include <memory>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

namespace UVE::Network::Tests {
namespace {

struct ReplicationHarnessUVE final {
    LoopbackTransportPairUVE link;
    NetworkReplicatorUVE a;
    NetworkReplicatorUVE b;
    std::map<NetworkEntityIdUVE, Math::Vector3UVE> positionsA;
    std::map<NetworkEntityIdUVE, Math::Vector3UVE> positionsB;

    void UpdateBothUVE(const std::uint64_t nowMs) {
        a.UpdateUVE(nowMs);
        b.UpdateUVE(nowMs);
    }

    [[nodiscard]] bool EstablishUVE(const std::uint64_t nowMs) {
        if (!a.SessionUVE().ConnectUVE(nowMs)) {
            return false;
        }
        UpdateBothUVE(nowMs);
        UpdateBothUVE(nowMs);
        return a.SessionUVE().StateUVE() == SessionStateUVE::Established &&
               b.SessionUVE().StateUVE() == SessionStateUVE::Established;
    }

    void RegisterMirroredUVE(const NetworkEntityIdUVE netId) {
        static_cast<void>(a.RegisterEntityUVE(
            netId, [this, netId]() { return positionsA[netId]; },
            [this, netId](const Math::Vector3UVE& position) { positionsA[netId] = position; }));
        static_cast<void>(b.RegisterEntityUVE(
            netId, [this, netId]() { return positionsB[netId]; },
            [this, netId](const Math::Vector3UVE& position) { positionsB[netId] = position; }));
    }
};

[[nodiscard]] ReplicationHarnessUVE MakeReplicatorsUVE(
    const std::uint64_t sendIntervalMs = kDefaultReplicationIntervalMsUVE) {
    LoopbackTransportPairUVE pair = CreateLoopbackTransportPairUVE(256U);
    NetworkSessionUVE sessionA(SessionRoleUVE::Client,
                               std::make_unique<LoopbackEndpointUVE>(pair.a));
    NetworkSessionUVE sessionB(SessionRoleUVE::Server,
                               std::make_unique<LoopbackEndpointUVE>(pair.b));
    NetworkReplicatorUVE a(std::move(sessionA), sendIntervalMs);
    NetworkReplicatorUVE b(std::move(sessionB), sendIntervalMs);
    return ReplicationHarnessUVE{
        std::move(pair), std::move(a), std::move(b), {}, {}};
}

TEST(NetworkReplicationUVETest, RegisterRules_DuplicatesAndEmptyCallbacksRejected) {
    ReplicationHarnessUVE harness = MakeReplicatorsUVE();
    const PositionSamplerUVE sampler = []() { return Math::Vector3UVE{}; };
    const PositionApplierUVE applier = [](const Math::Vector3UVE&) {};

    EXPECT_TRUE(harness.a.RegisterEntityUVE(7U, sampler, applier));
    EXPECT_FALSE(harness.a.RegisterEntityUVE(7U, sampler, applier));
    EXPECT_FALSE(harness.a.RegisterEntityUVE(8U, PositionSamplerUVE{}, applier));
    EXPECT_FALSE(harness.a.RegisterEntityUVE(8U, sampler, PositionApplierUVE{}));
    EXPECT_FALSE(harness.a.UnregisterEntityUVE(9U));
    EXPECT_TRUE(harness.a.UnregisterEntityUVE(7U));
    EXPECT_TRUE(harness.a.RegisterEntityUVE(7U, sampler, applier));
}

TEST(NetworkReplicationUVETest, Snapshot_DivergentValuesConvergeOnFirstSender) {
    // Authority-free by design: whoever's snapshot lands first wins, and the loser adopts it
    // before its own send goes out. The server establishes (and sends) first, so divergent
    // values deterministically converge on the server's: B-to-A delivery, proven by A holding a
    // value it never sampled. (A-to-B is proven directionally by Rate_SendsOnlyWhenDue.)
    ReplicationHarnessUVE harness = MakeReplicatorsUVE();
    harness.RegisterMirroredUVE(7U);
    harness.positionsA[7U] = Math::Vector3UVE{1.0F, 2.0F, 3.0F};
    harness.positionsB[7U] = Math::Vector3UVE{9.0F, 9.0F, 9.0F};

    ASSERT_TRUE(harness.EstablishUVE(1000U));
    EXPECT_EQ(harness.positionsA[7U], (Math::Vector3UVE{9.0F, 9.0F, 9.0F}));
    EXPECT_EQ(harness.positionsB[7U], (Math::Vector3UVE{9.0F, 9.0F, 9.0F}));
}

TEST(NetworkReplicationUVETest, Rate_SendsOnlyWhenDue) {
    ReplicationHarnessUVE harness = MakeReplicatorsUVE();
    harness.RegisterMirroredUVE(7U);
    ASSERT_TRUE(harness.EstablishUVE(1000U));
    ASSERT_EQ(harness.a.StatsUVE().snapshotsSent, 1U);

    harness.positionsA[7U] = Math::Vector3UVE{5.0F, 5.0F, 5.0F};
    harness.UpdateBothUVE(1050U);
    harness.UpdateBothUVE(1099U);
    EXPECT_EQ(harness.a.StatsUVE().snapshotsSent, 1U);
    EXPECT_EQ(harness.positionsB[7U], (Math::Vector3UVE{0.0F, 0.0F, 0.0F}));

    harness.UpdateBothUVE(1100U);
    EXPECT_EQ(harness.a.StatsUVE().snapshotsSent, 2U);
    EXPECT_EQ(harness.positionsB[7U], (Math::Vector3UVE{5.0F, 5.0F, 5.0F}));
}

TEST(NetworkReplicationUVETest, IntervalZero_SendsEveryUpdate) {
    ReplicationHarnessUVE harness = MakeReplicatorsUVE(0U);
    harness.RegisterMirroredUVE(7U);
    ASSERT_TRUE(harness.EstablishUVE(1000U));
    const std::uint64_t sentAfterHandshake = harness.a.StatsUVE().snapshotsSent;

    harness.UpdateBothUVE(1001U);
    EXPECT_EQ(harness.a.StatsUVE().snapshotsSent, sentAfterHandshake + 1U);
    harness.UpdateBothUVE(1002U);
    EXPECT_EQ(harness.a.StatsUVE().snapshotsSent, sentAfterHandshake + 2U);
}

TEST(NetworkReplicationUVETest, UnknownNetIds_IgnoredAndCounted) {
    ReplicationHarnessUVE harness = MakeReplicatorsUVE();
    const PositionSamplerUVE sampler = []() { return Math::Vector3UVE{1.0F, 2.0F, 3.0F}; };
    const PositionApplierUVE applier = [](const Math::Vector3UVE&) {};
    ASSERT_TRUE(harness.a.RegisterEntityUVE(7U, sampler, applier));

    ASSERT_TRUE(harness.EstablishUVE(1000U));
    EXPECT_EQ(harness.b.StatsUVE().snapshotsReceived, 1U);
    EXPECT_EQ(harness.b.StatsUVE().entriesApplied, 0U);
    EXPECT_EQ(harness.b.StatsUVE().entriesIgnoredUnknown, 1U);
}

TEST(NetworkReplicationUVETest, MalformedSnapshot_IgnoredWhole) {
    ReplicationHarnessUVE harness = MakeReplicatorsUVE();
    harness.RegisterMirroredUVE(7U);
    harness.positionsB[7U] = Math::Vector3UVE{9.0F, 9.0F, 9.0F};
    ASSERT_TRUE(harness.EstablishUVE(1000U));

    const std::uint8_t dataTag = static_cast<std::uint8_t>(SessionMessageTagUVE::Data);
    // Declares two entries, carries one.
    ASSERT_TRUE(harness.link.a.SendUVE(std::vector<std::uint8_t>{
        dataTag, 0x02U, 0x00U, 0x07U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
        0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U}));
    // Declares one entry, carries none.
    ASSERT_TRUE(
        harness.link.a.SendUVE(std::vector<std::uint8_t>{dataTag, 0x01U, 0x00U}));
    // Trailing garbage after a well-formed entry.
    ASSERT_TRUE(harness.link.a.SendUVE(std::vector<std::uint8_t>{
        dataTag, 0x01U, 0x00U, 0x07U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
        0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0xFFU}));

    harness.b.UpdateUVE(1100U);
    EXPECT_EQ(harness.b.StatsUVE().messagesIgnoredMalformed, 3U);
    EXPECT_EQ(harness.b.StatsUVE().snapshotsReceived, 1U);
    EXPECT_EQ(harness.positionsB[7U], (Math::Vector3UVE{9.0F, 9.0F, 9.0F}));
}

TEST(NetworkReplicationUVETest, Snapshot_TruncatedToFitLowestIdsWin) {
    ReplicationHarnessUVE harness = MakeReplicatorsUVE();
    for (NetworkEntityIdUVE id = 0U; id < 80U; ++id) {
        const PositionSamplerUVE sampler = [id]() {
            return Math::Vector3UVE{static_cast<float>(id), 0.0F, 0.0F};
        };
        const PositionApplierUVE applier = [](const Math::Vector3UVE&) {};
        ASSERT_TRUE(harness.a.RegisterEntityUVE(id, sampler, applier));
    }
    for (NetworkEntityIdUVE id = 74U; id < 80U; ++id) {
        harness.positionsB[id] = Math::Vector3UVE{-1.0F, -1.0F, -1.0F};
        static_cast<void>(harness.b.RegisterEntityUVE(
            id, [id]() { return Math::Vector3UVE{static_cast<float>(id), 0.0F, 0.0F}; },
            [&harness, id](const Math::Vector3UVE& position) { harness.positionsB[id] = position; }));
    }

    ASSERT_TRUE(harness.EstablishUVE(1000U));
    EXPECT_EQ(harness.a.StatsUVE().entriesTruncated, 6U);
    EXPECT_EQ(harness.b.StatsUVE().entriesIgnoredUnknown, 74U);
    for (NetworkEntityIdUVE id = 74U; id < 80U; ++id) {
        EXPECT_EQ(harness.positionsB[id], (Math::Vector3UVE{-1.0F, -1.0F, -1.0F}));
    }
}

TEST(NetworkReplicationUVETest, SendOnlyWhenEstablished) {
    ReplicationHarnessUVE harness = MakeReplicatorsUVE();
    harness.RegisterMirroredUVE(7U);

    harness.UpdateBothUVE(1000U);
    harness.UpdateBothUVE(2000U);
    EXPECT_EQ(harness.a.StatsUVE().snapshotsSent, 0U);
    EXPECT_EQ(harness.b.StatsUVE().snapshotsSent, 0U);

    ASSERT_TRUE(harness.a.SessionUVE().ConnectUVE(3000U));
    harness.a.UpdateUVE(3100U);
    harness.a.UpdateUVE(3200U);
    EXPECT_EQ(harness.a.SessionUVE().StateUVE(), SessionStateUVE::Connecting);
    EXPECT_EQ(harness.a.StatsUVE().snapshotsSent, 0U);
}

TEST(NetworkReplicationUVETest, EmptyRegistration_SendsHeartbeatSnapshots) {
    ReplicationHarnessUVE harness = MakeReplicatorsUVE();
    ASSERT_TRUE(harness.EstablishUVE(1000U));

    EXPECT_EQ(harness.a.StatsUVE().snapshotsSent, 1U);
    EXPECT_EQ(harness.b.StatsUVE().snapshotsReceived, 1U);
    EXPECT_EQ(harness.b.StatsUVE().entriesApplied, 0U);
}

TEST(NetworkReplicationUVETest, Stats_CountersExactAfterAMixedExchange) {
    ReplicationHarnessUVE harness = MakeReplicatorsUVE();
    const PositionApplierUVE ignore = [](const Math::Vector3UVE&) {};
    ASSERT_TRUE(harness.a.RegisterEntityUVE(
        1U, []() { return Math::Vector3UVE{1.0F, 0.0F, 0.0F}; }, ignore));
    ASSERT_TRUE(harness.a.RegisterEntityUVE(
        2U, []() { return Math::Vector3UVE{2.0F, 0.0F, 0.0F}; }, ignore));
    ASSERT_TRUE(harness.b.RegisterEntityUVE(
        2U, []() { return Math::Vector3UVE{20.0F, 0.0F, 0.0F}; }, ignore));
    ASSERT_TRUE(harness.b.RegisterEntityUVE(
        3U, []() { return Math::Vector3UVE{30.0F, 0.0F, 0.0F}; }, ignore));

    ASSERT_TRUE(harness.EstablishUVE(1000U));
    EXPECT_EQ(harness.a.StatsUVE(), (ReplicationStatsUVE{1U, 1U, 1U, 1U, 0U, 0U}));
    EXPECT_EQ(harness.b.StatsUVE(), (ReplicationStatsUVE{1U, 1U, 1U, 1U, 0U, 0U}));
}

} // namespace
} // namespace UVE::Network::Tests
