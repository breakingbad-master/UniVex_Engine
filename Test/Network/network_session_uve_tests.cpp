// Copyright (c) 2026 UniVex Studios. All Rights Reserved.

#include "uve/network/network_session_uve.h"

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

namespace UVE::Network::Tests {
namespace {

[[nodiscard]] std::vector<std::uint8_t> BytesUVE(const std::string& text) {
    return std::vector<std::uint8_t>(text.begin(), text.end());
}

[[nodiscard]] std::vector<std::uint8_t> TaggedUVE(const std::uint8_t tag, const std::string& text) {
    std::vector<std::uint8_t> datagram{tag};
    for (const std::uint8_t byte : BytesUVE(text)) {
        datagram.push_back(byte);
    }
    return datagram;
}

struct SessionHarnessUVE final {
    LoopbackTransportPairUVE link;
    NetworkSessionUVE client;
    NetworkSessionUVE server;

    void UpdateBothUVE(const std::uint64_t nowMs) {
        client.UpdateUVE(nowMs);
        server.UpdateUVE(nowMs);
    }

    // A handshake needs two pump rounds: the first carries Hello to the server and queues the
    // Welcome, the second delivers it to the client.
    [[nodiscard]] bool EstablishUVE(const std::uint64_t nowMs) {
        if (!client.ConnectUVE(nowMs)) {
            return false;
        }
        UpdateBothUVE(nowMs);
        UpdateBothUVE(nowMs);
        return client.StateUVE() == SessionStateUVE::Established &&
               server.StateUVE() == SessionStateUVE::Established;
    }
};

[[nodiscard]] SessionHarnessUVE MakeSessionsUVE(
    const std::uint64_t timeoutMs = kDefaultSessionTimeoutMsUVE) {
    LoopbackTransportPairUVE pair = CreateLoopbackTransportPairUVE(64U);
    NetworkSessionUVE client(SessionRoleUVE::Client,
                             std::make_unique<LoopbackEndpointUVE>(pair.a), timeoutMs);
    NetworkSessionUVE server(SessionRoleUVE::Server,
                             std::make_unique<LoopbackEndpointUVE>(pair.b), timeoutMs);
    return SessionHarnessUVE{std::move(pair), std::move(client), std::move(server)};
}

TEST(NetworkSessionUVETest, Handshake_ClientAndServerReachEstablished) {
    SessionHarnessUVE harness = MakeSessionsUVE();

    ASSERT_TRUE(harness.client.ConnectUVE(1000U));
    EXPECT_EQ(harness.client.StateUVE(), SessionStateUVE::Connecting);
    harness.UpdateBothUVE(1000U);
    harness.UpdateBothUVE(1000U);

    EXPECT_EQ(harness.client.StateUVE(), SessionStateUVE::Established);
    EXPECT_EQ(harness.server.StateUVE(), SessionStateUVE::Established);
    EXPECT_EQ(harness.client.CloseReasonUVE(), SessionCloseReasonUVE::None);
    EXPECT_EQ(harness.server.CloseReasonUVE(), SessionCloseReasonUVE::None);
}

TEST(NetworkSessionUVETest, RoleRules_RejectInvalidTransitions) {
    SessionHarnessUVE harness = MakeSessionsUVE();

    EXPECT_FALSE(harness.server.ConnectUVE(1000U));
    EXPECT_EQ(harness.server.StateUVE(), SessionStateUVE::Disconnected);
    EXPECT_FALSE(harness.client.DisconnectUVE());
    EXPECT_FALSE(harness.client.SendDataUVE(BytesUVE("early")));

    ASSERT_TRUE(harness.client.ConnectUVE(1000U));
    EXPECT_FALSE(harness.client.ConnectUVE(1000U));
    EXPECT_FALSE(harness.client.SendDataUVE(BytesUVE("connecting")));
}

TEST(NetworkSessionUVETest, DataFlowsBothWaysAfterHandshake) {
    SessionHarnessUVE harness = MakeSessionsUVE();
    ASSERT_TRUE(harness.EstablishUVE(1000U));
    ASSERT_EQ(harness.client.StateUVE(), SessionStateUVE::Established);

    ASSERT_TRUE(harness.client.SendDataUVE(BytesUVE("c-to-s")));
    ASSERT_TRUE(harness.server.SendDataUVE(BytesUVE("s-to-c")));
    harness.UpdateBothUVE(1000U);

    EXPECT_EQ(harness.server.TryReceiveDataUVE(),
              std::optional<std::vector<std::uint8_t>>(BytesUVE("c-to-s")));
    EXPECT_EQ(harness.client.TryReceiveDataUVE(),
              std::optional<std::vector<std::uint8_t>>(BytesUVE("s-to-c")));
    EXPECT_FALSE(harness.client.TryReceiveDataUVE().has_value());
}

TEST(NetworkSessionUVETest, Disconnect_ByeClosesBothSidesAndClearsQueuedData) {
    SessionHarnessUVE harness = MakeSessionsUVE();
    ASSERT_TRUE(harness.EstablishUVE(1000U));
    ASSERT_TRUE(harness.client.SendDataUVE(BytesUVE("unread")));

    EXPECT_TRUE(harness.client.DisconnectUVE());
    EXPECT_EQ(harness.client.StateUVE(), SessionStateUVE::Disconnected);
    EXPECT_EQ(harness.client.CloseReasonUVE(), SessionCloseReasonUVE::LocalDisconnect);

    harness.server.UpdateUVE(1000U);
    EXPECT_EQ(harness.server.StateUVE(), SessionStateUVE::Disconnected);
    EXPECT_EQ(harness.server.CloseReasonUVE(), SessionCloseReasonUVE::RemoteDisconnect);
    EXPECT_FALSE(harness.server.TryReceiveDataUVE().has_value());
}

TEST(NetworkSessionUVETest, Timeout_ConnectingWithoutWelcome) {
    SessionHarnessUVE harness = MakeSessionsUVE();
    ASSERT_TRUE(harness.client.ConnectUVE(1000U));

    harness.client.UpdateUVE(5999U);
    EXPECT_EQ(harness.client.StateUVE(), SessionStateUVE::Connecting);

    harness.client.UpdateUVE(6000U);
    EXPECT_EQ(harness.client.StateUVE(), SessionStateUVE::Disconnected);
    EXPECT_EQ(harness.client.CloseReasonUVE(), SessionCloseReasonUVE::Timeout);
}

TEST(NetworkSessionUVETest, Timeout_EstablishedGoesQuiet) {
    SessionHarnessUVE harness = MakeSessionsUVE();
    ASSERT_TRUE(harness.EstablishUVE(1000U));

    harness.UpdateBothUVE(5999U);
    EXPECT_EQ(harness.client.StateUVE(), SessionStateUVE::Established);
    EXPECT_EQ(harness.server.StateUVE(), SessionStateUVE::Established);

    harness.UpdateBothUVE(6000U);
    EXPECT_EQ(harness.client.StateUVE(), SessionStateUVE::Disconnected);
    EXPECT_EQ(harness.client.CloseReasonUVE(), SessionCloseReasonUVE::Timeout);
    EXPECT_EQ(harness.server.StateUVE(), SessionStateUVE::Disconnected);
    EXPECT_EQ(harness.server.CloseReasonUVE(), SessionCloseReasonUVE::Timeout);
}

TEST(NetworkSessionUVETest, TrafficResetsTheTimeoutPerDirection) {
    SessionHarnessUVE harness = MakeSessionsUVE();
    ASSERT_TRUE(harness.EstablishUVE(1000U));

    // Only inbound datagrams refresh a session's clock: each side must hear the other.
    ASSERT_TRUE(harness.client.SendDataUVE(BytesUVE("c1")));
    ASSERT_TRUE(harness.server.SendDataUVE(BytesUVE("s1")));
    harness.UpdateBothUVE(5000U);
    harness.UpdateBothUVE(9999U);
    EXPECT_EQ(harness.client.StateUVE(), SessionStateUVE::Established);
    EXPECT_EQ(harness.server.StateUVE(), SessionStateUVE::Established);

    harness.UpdateBothUVE(10000U);
    EXPECT_EQ(harness.client.StateUVE(), SessionStateUVE::Disconnected);
    EXPECT_EQ(harness.server.StateUVE(), SessionStateUVE::Disconnected);
}

TEST(NetworkSessionUVETest, VersionMismatch_ServerAnswersByeAndStaysDisconnected) {
    SessionHarnessUVE harness = MakeSessionsUVE();
    const std::vector<std::uint8_t> badHello{
        static_cast<std::uint8_t>(SessionMessageTagUVE::Hello), 0x09U, 0x00U};
    ASSERT_TRUE(harness.link.a.SendUVE(badHello));

    harness.server.UpdateUVE(1000U);
    EXPECT_EQ(harness.server.StateUVE(), SessionStateUVE::Disconnected);
    EXPECT_EQ(harness.server.CloseReasonUVE(), SessionCloseReasonUVE::None);

    const std::vector<std::uint8_t> expectedBye{
        static_cast<std::uint8_t>(SessionMessageTagUVE::Bye)};
    EXPECT_EQ(harness.link.a.TryReceiveUVE(),
              std::optional<std::vector<std::uint8_t>>(expectedBye));
}

TEST(NetworkSessionUVETest, GarbageAndEarlyData_AreIgnored) {
    SessionHarnessUVE harness = MakeSessionsUVE();
    ASSERT_TRUE(harness.link.a.SendUVE(BytesUVE("no-tag-here")));
    ASSERT_TRUE(harness.link.a.SendUVE(std::vector<std::uint8_t>{}));
    ASSERT_TRUE(harness.link.a.SendUVE(
        TaggedUVE(static_cast<std::uint8_t>(SessionMessageTagUVE::Data), "early-data")));

    harness.server.UpdateUVE(1000U);
    EXPECT_EQ(harness.server.StateUVE(), SessionStateUVE::Disconnected);
    EXPECT_FALSE(harness.server.TryReceiveDataUVE().has_value());

    ASSERT_TRUE(harness.EstablishUVE(2000U));
    ASSERT_EQ(harness.server.StateUVE(), SessionStateUVE::Established);
    ASSERT_TRUE(harness.link.a.SendUVE(TaggedUVE(0xFFU, "unknown-tag")));
    harness.server.UpdateUVE(2000U);
    EXPECT_EQ(harness.server.StateUVE(), SessionStateUVE::Established);
    EXPECT_FALSE(harness.server.TryReceiveDataUVE().has_value());
}

TEST(NetworkSessionUVETest, DataPayload_BoundedOneByteUnderTheDatagramCap) {
    SessionHarnessUVE harness = MakeSessionsUVE();
    ASSERT_TRUE(harness.EstablishUVE(1000U));

    const std::vector<std::uint8_t> maxed(kSessionMaximumPayloadBytesUVE, 0xCDU);
    const std::vector<std::uint8_t> tooBig(kSessionMaximumPayloadBytesUVE + 1U, 0xCDU);
    ASSERT_TRUE(harness.client.SendDataUVE(maxed));
    EXPECT_FALSE(harness.client.SendDataUVE(tooBig));

    harness.server.UpdateUVE(1000U);
    const std::optional<std::vector<std::uint8_t>> received = harness.server.TryReceiveDataUVE();
    ASSERT_TRUE(received.has_value());
    EXPECT_EQ(*received, maxed);
    EXPECT_FALSE(harness.server.TryReceiveDataUVE().has_value());
}

} // namespace
} // namespace UVE::Network::Tests
