/**
 * @file test_messages.cpp
 * @brief Unit tests for protocol messages.
 * 
 * Tests message construction, variant handling, and enum stability.
 */

#include <catch2/catch_test_macros.hpp>

#include "protocol/messages.hpp"

using namespace shared::proto;

// =============================================================================
// Protocol version tests
// =============================================================================

TEST_CASE("Protocol version is valid", "[protocol][version]") {
    REQUIRE(kProtocolVersion >= 1);
}

// =============================================================================
// RejectReason enum stability tests
// =============================================================================

TEST_CASE("RejectReason enum values are stable", "[protocol][enum]") {
    // These values must never change (binary protocol compatibility)
    REQUIRE(static_cast<int>(RejectReason::Unknown) == 0);
    REQUIRE(static_cast<int>(RejectReason::Invalid) == 1);
    REQUIRE(static_cast<int>(RejectReason::NotAllowed) == 2);
    REQUIRE(static_cast<int>(RejectReason::NotEnoughResources) == 3);
    REQUIRE(static_cast<int>(RejectReason::OutOfRange) == 4);
    REQUIRE(static_cast<int>(RejectReason::ProtectedBlock) == 5);
}

// =============================================================================
// Message default construction tests
// =============================================================================

TEST_CASE("ClientHello default construction", "[protocol][messages]") {
    ClientHello msg;
    REQUIRE(msg.version == kProtocolVersion);
    REQUIRE(msg.clientName.empty());
}

TEST_CASE("ServerHello default construction", "[protocol][messages]") {
    ServerHello msg;
    REQUIRE(msg.acceptedVersion == kProtocolVersion);
    REQUIRE(msg.tickRate == 30);
    REQUIRE(msg.worldSeed == 0);
    REQUIRE_FALSE(msg.hasMapTemplate);
    REQUIRE(msg.mapId.empty());
    REQUIRE(msg.mapVersion == 0);
}

TEST_CASE("JoinMatch default construction", "[protocol][messages]") {
    JoinMatch msg;
    // JoinMatch has no fields currently
    (void)msg;
}

TEST_CASE("JoinAck default construction", "[protocol][messages]") {
    JoinAck msg;
    REQUIRE(msg.playerId == 0);
}

TEST_CASE("InputFrame default construction", "[protocol][messages]") {
    InputFrame msg;
    REQUIRE(msg.seq == 0);
    REQUIRE(msg.moveX == 0.0f);
    REQUIRE(msg.moveY == 0.0f);
    REQUIRE(msg.yaw == 0.0f);
    REQUIRE(msg.pitch == 0.0f);
    REQUIRE_FALSE(msg.jump);
    REQUIRE_FALSE(msg.sprint);
    REQUIRE_FALSE(msg.camUp);
    REQUIRE_FALSE(msg.camDown);
}

TEST_CASE("TryPlaceBlock default construction", "[protocol][messages]") {
    TryPlaceBlock msg;
    REQUIRE(msg.seq == 0);
    REQUIRE(msg.x == 0);
    REQUIRE(msg.y == 0);
    REQUIRE(msg.z == 0);
    REQUIRE(msg.blockType == shared::voxel::BlockType::Air);
}

TEST_CASE("TryBreakBlock default construction", "[protocol][messages]") {
    TryBreakBlock msg;
    REQUIRE(msg.seq == 0);
    REQUIRE(msg.x == 0);
    REQUIRE(msg.y == 0);
    REQUIRE(msg.z == 0);
}

TEST_CASE("TrySetBlock default construction", "[protocol][messages]") {
    TrySetBlock msg;
    REQUIRE(msg.seq == 0);
    REQUIRE(msg.x == 0);
    REQUIRE(msg.y == 0);
    REQUIRE(msg.z == 0);
    REQUIRE(msg.blockType == shared::voxel::BlockType::Air);
}

TEST_CASE("StateSnapshot default construction", "[protocol][messages]") {
    StateSnapshot msg;
    REQUIRE(msg.serverTick == 0);
    REQUIRE(msg.playerId == 0);
    REQUIRE(msg.px == 0.0f);
    REQUIRE(msg.py == 0.0f);
    REQUIRE(msg.pz == 0.0f);
    REQUIRE(msg.vx == 0.0f);
    REQUIRE(msg.vy == 0.0f);
    REQUIRE(msg.vz == 0.0f);
}

TEST_CASE("BlockPlaced default construction", "[protocol][messages]") {
    BlockPlaced msg;
    REQUIRE(msg.x == 0);
    REQUIRE(msg.y == 0);
    REQUIRE(msg.z == 0);
    REQUIRE(msg.blockType == shared::voxel::BlockType::Air);
}

TEST_CASE("BlockBroken default construction", "[protocol][messages]") {
    BlockBroken msg;
    REQUIRE(msg.x == 0);
    REQUIRE(msg.y == 0);
    REQUIRE(msg.z == 0);
}

TEST_CASE("ActionRejected default construction", "[protocol][messages]") {
    ActionRejected msg;
    REQUIRE(msg.seq == 0);
    REQUIRE(msg.reason == RejectReason::Unknown);
}

TEST_CASE("TryExportMap default construction", "[protocol][messages]") {
    TryExportMap msg;
    REQUIRE(msg.seq == 0);
    REQUIRE(msg.mapId.empty());
    REQUIRE(msg.version == 0);
    REQUIRE(msg.chunkMinX == 0);
    REQUIRE(msg.chunkMinZ == 0);
    REQUIRE(msg.chunkMaxX == 0);
    REQUIRE(msg.chunkMaxZ == 0);
    REQUIRE(msg.skyboxKind == 1);
    REQUIRE(msg.timeOfDayHours == 12.0f);
    REQUIRE_FALSE(msg.useMoon);
    REQUIRE(msg.sunIntensity == 1.0f);
    REQUIRE(msg.ambientIntensity == 0.25f);
    REQUIRE(msg.temperature == 0.5f);
    REQUIRE(msg.humidity == 1.0f);
}

TEST_CASE("ExportResult default construction", "[protocol][messages]") {
    ExportResult msg;
    REQUIRE(msg.seq == 0);
    REQUIRE_FALSE(msg.ok);
    REQUIRE(msg.reason == RejectReason::Unknown);
    REQUIRE(msg.path.empty());
}

// =============================================================================
// Message variant tests
// =============================================================================

TEST_CASE("Message variant can hold all message types", "[protocol][variant]") {
    Message msg;

    SECTION("ClientHello") {
        msg = ClientHello{.clientName = "Test"};
        REQUIRE(std::holds_alternative<ClientHello>(msg));
        REQUIRE(std::get<ClientHello>(msg).clientName == "Test");
    }

    SECTION("ServerHello") {
        msg = ServerHello{.tickRate = 60};
        REQUIRE(std::holds_alternative<ServerHello>(msg));
        REQUIRE(std::get<ServerHello>(msg).tickRate == 60);
    }

    SECTION("JoinMatch") {
        msg = JoinMatch{};
        REQUIRE(std::holds_alternative<JoinMatch>(msg));
    }

    SECTION("JoinAck") {
        msg = JoinAck{.playerId = 42};
        REQUIRE(std::holds_alternative<JoinAck>(msg));
        REQUIRE(std::get<JoinAck>(msg).playerId == 42);
    }

    SECTION("InputFrame") {
        msg = InputFrame{.seq = 100, .moveX = 1.0f, .jump = true};
        REQUIRE(std::holds_alternative<InputFrame>(msg));
        auto& input = std::get<InputFrame>(msg);
        REQUIRE(input.seq == 100);
        REQUIRE(input.moveX == 1.0f);
        REQUIRE(input.jump);
    }

    SECTION("TryPlaceBlock") {
        msg = TryPlaceBlock{.x = 10, .y = 64, .z = -5, .blockType = shared::voxel::BlockType::Stone};
        REQUIRE(std::holds_alternative<TryPlaceBlock>(msg));
        auto& place = std::get<TryPlaceBlock>(msg);
        REQUIRE(place.x == 10);
        REQUIRE(place.y == 64);
        REQUIRE(place.z == -5);
        REQUIRE(place.blockType == shared::voxel::BlockType::Stone);
    }

    SECTION("TryBreakBlock") {
        msg = TryBreakBlock{.x = 1, .y = 2, .z = 3};
        REQUIRE(std::holds_alternative<TryBreakBlock>(msg));
    }

    SECTION("TrySetBlock") {
        msg = TrySetBlock{.blockType = shared::voxel::BlockType::Grass};
        REQUIRE(std::holds_alternative<TrySetBlock>(msg));
    }

    SECTION("StateSnapshot") {
        msg = StateSnapshot{.serverTick = 999, .px = 50.0f, .py = 80.0f};
        REQUIRE(std::holds_alternative<StateSnapshot>(msg));
        auto& snap = std::get<StateSnapshot>(msg);
        REQUIRE(snap.serverTick == 999);
        REQUIRE(snap.px == 50.0f);
    }

    SECTION("BlockPlaced") {
        msg = BlockPlaced{.x = 5, .blockType = shared::voxel::BlockType::Wood};
        REQUIRE(std::holds_alternative<BlockPlaced>(msg));
    }

    SECTION("BlockBroken") {
        msg = BlockBroken{.x = 7, .y = 8, .z = 9};
        REQUIRE(std::holds_alternative<BlockBroken>(msg));
    }

    SECTION("ActionRejected") {
        msg = ActionRejected{.seq = 50, .reason = RejectReason::OutOfRange};
        REQUIRE(std::holds_alternative<ActionRejected>(msg));
        REQUIRE(std::get<ActionRejected>(msg).reason == RejectReason::OutOfRange);
    }

    SECTION("TryExportMap") {
        msg = TryExportMap{.mapId = "testmap"};
        REQUIRE(std::holds_alternative<TryExportMap>(msg));
    }

    SECTION("ExportResult") {
        msg = ExportResult{.ok = true, .path = "/maps/test.rfmap"};
        REQUIRE(std::holds_alternative<ExportResult>(msg));
        REQUIRE(std::get<ExportResult>(msg).ok);
    }
}
