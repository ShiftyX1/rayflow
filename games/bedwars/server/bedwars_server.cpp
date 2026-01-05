#include "bedwars_server.hpp"
#include "physics_utils.hpp"
#include "../shared/protocol/serialization.hpp"

// Terrain (now in bedwars)
#include "voxel/terrain.hpp"

// BedWars constants
#include "../shared/constants.hpp"
#include <engine/modules/voxel/shared/block_state.hpp>

// Map loading
#include <engine/maps/rfmap_io.hpp>
#include <engine/maps/runtime_paths.hpp>

#include <algorithm>
#include <cmath>
#include <filesystem>

namespace bedwars::server {

namespace {
// Fast floor helper
inline int fast_floor(float v) {
    int i = static_cast<int>(v);
    return (v < i) ? i - 1 : i;
}

// Load a specific .rfmap by name from maps directory
static bool load_rfmap_by_name(const std::string& name, shared::maps::MapTemplate* outMap, std::filesystem::path* outPath) {
    if (!outMap || name.empty()) return false;
    
    auto mapsDir = shared::maps::runtime_maps_dir();
    std::error_code ec;
    if (!std::filesystem::exists(mapsDir, ec)) return false;
    
    // Try exact name first
    auto path = mapsDir / name;
    if (!std::filesystem::exists(path, ec)) {
        // Try with .rfmap extension
        path = mapsDir / (name + ".rfmap");
        if (!std::filesystem::exists(path, ec)) return false;
    }
    
    std::string err;
    if (!shared::maps::read_rfmap(path, outMap, &err)) return false;
    if (outMap->mapId.empty() || outMap->version == 0) return false;
    
    if (outPath) *outPath = path;
    return true;
}

// Load the most recently modified .rfmap from maps directory
static bool load_latest_rfmap(shared::maps::MapTemplate* outMap, std::filesystem::path* outPath) {
    if (!outMap) return false;
    
    auto mapsDir = shared::maps::runtime_maps_dir();
    std::error_code ec;
    if (!std::filesystem::exists(mapsDir, ec)) return false;
    
    std::filesystem::path bestPath;
    std::filesystem::file_time_type bestTime{};
    bool haveBest = false;
    
    for (const auto& entry : std::filesystem::directory_iterator(mapsDir, ec)) {
        if (ec) break;
        if (!entry.is_regular_file(ec)) continue;
        if (entry.path().extension() != ".rfmap") continue;
        
        auto t = entry.last_write_time(ec);
        if (ec) continue;
        if (!haveBest || t > bestTime) {
            bestTime = t;
            bestPath = entry.path();
            haveBest = true;
        }
    }
    
    if (!haveBest) return false;
    
    std::string err;
    if (!shared::maps::read_rfmap(bestPath, outMap, &err)) return false;
    if (outMap->mapId.empty() || outMap->version == 0) return false;
    
    if (outPath) *outPath = bestPath;
    return true;
}

// Check if placing a block at (bx, by, bz) would intersect player AABB
// Player hitbox: centered at (px, pz), bottom at py, width kPlayerWidth, height kPlayerHeight
inline bool would_intersect_player(float px, float py, float pz, int bx, int by, int bz) {
    const float halfW = bedwars::kPlayerWidth * 0.5f;
    
    // Player AABB
    const float pMinX = px - halfW;
    const float pMaxX = px + halfW;
    const float pMinY = py;
    const float pMaxY = py + bedwars::kPlayerHeight;
    const float pMinZ = pz - halfW;
    const float pMaxZ = pz + halfW;
    
    // Block AABB (full block = 1x1x1)
    const float bMinX = static_cast<float>(bx);
    const float bMaxX = static_cast<float>(bx + 1);
    const float bMinY = static_cast<float>(by);
    const float bMaxY = static_cast<float>(by + 1);
    const float bMinZ = static_cast<float>(bz);
    const float bMaxZ = static_cast<float>(bz + 1);
    
    // AABB intersection test
    return (pMinX < bMaxX && pMaxX > bMinX &&
            pMinY < bMaxY && pMaxY > bMinY &&
            pMinZ < bMaxZ && pMaxZ > bMinZ);
}
}

// ============================================================================
// Construction
// ============================================================================

BedWarsServer::BedWarsServer(std::uint32_t seed)
    : BedWarsServer(seed, Options{}) {
}

BedWarsServer::BedWarsServer(std::uint32_t seed, const Options& opts)
    : opts_(opts), worldSeed_(seed) {
}

BedWarsServer::~BedWarsServer() = default;

// ============================================================================
// Lifecycle
// ============================================================================

void BedWarsServer::on_init(engine::IEngineServices& engine) {
    engine_ = &engine;
    terrain_ = std::make_unique<::bedwars::voxel::Terrain>(worldSeed_);
    
    // Editor mode: empty terrain (no procedural generation)
    if (opts_.editorCameraMode) {
        terrain_->set_void_base(true);
    }
    
    // Load map template if enabled
    if (opts_.loadMapTemplate) {
        shared::maps::MapTemplate map;
        std::filesystem::path path;
        bool loaded = false;
        
        if (!opts_.mapName.empty()) {
            // Load specific map by name
            loaded = load_rfmap_by_name(opts_.mapName, &map, &path);
            if (!loaded) {
                engine_->log_warning("Failed to load map '" + opts_.mapName + "', trying most recent");
            }
        }
        
        if (!loaded) {
            // Fall back to most recent map
            loaded = load_latest_rfmap(&map, &path);
        }
        
        if (loaded) {
            hasMapTemplate_ = true;
            mapId_ = map.mapId;
            mapVersion_ = map.version;
            
            // Calculate spawn position from map bounds (center of the map, find ground level)
            const auto& bounds = map.bounds;
            int centerBlockX = ((bounds.chunkMinX + bounds.chunkMaxX) / 2) * 16 + 8;
            int centerBlockZ = ((bounds.chunkMinZ + bounds.chunkMaxZ) / 2) * 16 + 8;
            
            // Store for use in team spawn calculation
            mapCenterX_ = static_cast<float>(centerBlockX);
            mapCenterZ_ = static_cast<float>(centerBlockZ);
            
            terrain_->set_map_template(std::move(map));
            engine_->log_info("Loaded map: " + path.filename().string() + " (id=" + mapId_ + " v" + std::to_string(mapVersion_) + ")");
            engine_->log_info("Map center: " + std::to_string(centerBlockX) + ", " + std::to_string(centerBlockZ));
        } else {
            engine_->log_warning("No .rfmap files found, using procedural terrain");
        }
    }
    
    // Calculate spawn height (find ground level at map center)
    float spawnY = 80.0f;  // Default spawn height
    if (hasMapTemplate_) {
        // Find the highest solid block at map center
        for (int y = 255; y >= 0; --y) {
            auto block = terrain_->get_block(static_cast<int>(mapCenterX_), y, static_cast<int>(mapCenterZ_));
            if (block != shared::voxel::BlockType::Air) {
                spawnY = static_cast<float>(y + 2);  // Spawn 2 blocks above ground
                break;
            }
        }
        engine_->log_info("Spawn Y calculated: " + std::to_string(spawnY));
    }
    
    // Initialize teams - if map loaded, spawn all teams at map center (temporary until proper spawn points)
    if (hasMapTemplate_) {
        // Single spawn point at map center for all teams (for testing)
        teams_[0] = TeamState{proto::Teams::Red, "Red", mapCenterX_, spawnY, mapCenterZ_, 
                              static_cast<int>(mapCenterX_), static_cast<int>(spawnY - 2), static_cast<int>(mapCenterZ_), true, {}};
        teams_[1] = TeamState{proto::Teams::Blue, "Blue", mapCenterX_ + 5.0f, spawnY, mapCenterZ_, 
                              static_cast<int>(mapCenterX_) + 5, static_cast<int>(spawnY - 2), static_cast<int>(mapCenterZ_), true, {}};
        teams_[2] = TeamState{proto::Teams::Green, "Green", mapCenterX_, spawnY, mapCenterZ_ + 5.0f, 
                              static_cast<int>(mapCenterX_), static_cast<int>(spawnY - 2), static_cast<int>(mapCenterZ_) + 5, true, {}};
        teams_[3] = TeamState{proto::Teams::Yellow, "Yellow", mapCenterX_ - 5.0f, spawnY, mapCenterZ_, 
                              static_cast<int>(mapCenterX_) - 5, static_cast<int>(spawnY - 2), static_cast<int>(mapCenterZ_), true, {}};
    } else {
        // Default BedWars layout for procedural terrain
        teams_[0] = TeamState{proto::Teams::Red, "Red", 0.0f, 80.0f, 50.0f, 0, 78, 50, true, {}};
        teams_[1] = TeamState{proto::Teams::Blue, "Blue", 0.0f, 80.0f, -50.0f, 0, 78, -50, true, {}};
        teams_[2] = TeamState{proto::Teams::Green, "Green", 50.0f, 80.0f, 0.0f, 50, 78, 0, true, {}};
        teams_[3] = TeamState{proto::Teams::Yellow, "Yellow", -50.0f, 80.0f, 0.0f, -50, 78, 0, true, {}};
    }
    
    // Initialize generators only for procedural terrain (skip for custom maps until spawn points are defined)
    if (!hasMapTemplate_) {
        // Center diamond generator
        float genY = 65.0f;
        generators_.push_back(GeneratorState{next_entity_id(), mapCenterX_, genY, mapCenterZ_, 2, 30.0f, 30.0f, proto::Teams::None, true});
        
        // Team iron/gold generators
        for (std::size_t i = 0; i < matchConfig_.teamCount && i < teams_.size(); ++i) {
            const auto& team = teams_[i];
            // Iron generator near spawn
            generators_.push_back(GeneratorState{next_entity_id(), team.spawnX + 3.0f, team.spawnY, team.spawnZ, 0, 1.0f, 1.0f, team.id, false});
            // Gold generator
            generators_.push_back(GeneratorState{next_entity_id(), team.spawnX - 3.0f, team.spawnY, team.spawnZ, 1, 8.0f, 8.0f, team.id, false});
        }
    }
    
    std::string mode = opts_.editorCameraMode ? " (editor mode)" : "";
    engine_->log_info("BedWars server initialized with seed " + std::to_string(worldSeed_) + mode);
    engine_->log_info("Teams: " + std::to_string(matchConfig_.teamCount) + ", Generators: " + std::to_string(generators_.size()));
}

void BedWarsServer::on_shutdown() {
    engine_->log_info("BedWars server shutting down");
    players_.clear();
    terrain_.reset();
    engine_ = nullptr;
}

// ============================================================================
// Tick
// ============================================================================

void BedWarsServer::on_tick(float dt) {
    // Update match phase
    update_match_phase(dt);
    
    // Update generators and items
    update_generators(dt);
    update_items(dt);
    
    // Update combat systems
    if (matchPhase_ == MatchPhase::InProgress) {
        update_regeneration(dt);
        update_respawns(dt);
    }
    
    for (auto& [id, player] : players_) {
        if (!player.joined) continue;
        
        // Use editor camera mode or normal physics
        if (opts_.editorCameraMode) {
            simulate_editor_camera(player, dt);
        } else if (player.alive) {
            simulate_player(player, dt);
            
            // Check for item pickup
            process_item_pickup(id);
        }
        
        // Send state snapshot
        proto::StateSnapshot snapshot;
        snapshot.serverTick = engine_->current_tick();
        snapshot.playerId = id;
        snapshot.px = player.px;
        snapshot.py = player.py;
        snapshot.pz = player.pz;
        snapshot.vx = player.vx;
        snapshot.vy = player.vy;
        snapshot.vz = player.vz;
        
        send_message(id, snapshot);
    }
}

// ============================================================================
// Physics simulation (simplified from legacy server)
// ============================================================================

void BedWarsServer::simulate_editor_camera(PlayerState& player, float dt) {
    using namespace physics;
    
    const auto& input = player.lastInput;
    const float speed = input.sprint ? kEditorFlySpeed * 2.0f : kEditorFlySpeed;
    
    // Calculate movement direction from yaw
    const float yawRad = input.yaw * kDegToRad;
    const float forwardX = std::sin(yawRad);
    const float forwardZ = std::cos(yawRad);
    const float rightX = std::cos(yawRad);
    const float rightZ = -std::sin(yawRad);
    
    // Horizontal movement
    const float moveX = input.moveX * speed;
    const float moveZ = input.moveY * speed;
    player.vx = rightX * moveX + forwardX * moveZ;
    player.vz = rightZ * moveX + forwardZ * moveZ;
    
    // Vertical movement (camUp/camDown)
    player.vy = 0.0f;
    if (input.camUp) player.vy = speed;
    if (input.camDown) player.vy = -speed;
    
    // Apply movement directly (no collision in editor mode)
    player.px += player.vx * dt;
    player.py += player.vy * dt;
    player.pz += player.vz * dt;
}

void BedWarsServer::simulate_player(PlayerState& player, float dt) {
    if (!terrain_) return;
    
    const auto& input = player.lastInput;
    
    // Use full physics simulation with step-up and proper collision
    physics::simulate_physics_step(
        *terrain_,
        player.px, player.py, player.pz,
        player.vx, player.vy, player.vz,
        player.onGround, player.lastJumpHeld,
        input.moveX, input.moveY, input.yaw,
        input.jump, input.sprint,
        dt
    );
}

bool BedWarsServer::check_collision_at(float px, float py, float pz) const {
    if (!terrain_) return false;
    
    const float half_w = physics::kPlayerWidth * 0.5f;
    const float half_d = physics::kPlayerWidth * 0.5f;
    
    // Check all blocks player AABB overlaps
    int minBx = physics::fast_floor(px - half_w + physics::kEps);
    int maxBx = physics::fast_floor(px + half_w - physics::kEps);
    int minBy = physics::fast_floor(py + physics::kEps);
    int maxBy = physics::fast_floor(py + physics::kPlayerHeight - physics::kEps);
    int minBz = physics::fast_floor(pz - half_d + physics::kEps);
    int maxBz = physics::fast_floor(pz + half_d - physics::kEps);
    
    for (int bx = minBx; bx <= maxBx; ++bx) {
        for (int by = minBy; by <= maxBy; ++by) {
            for (int bz = minBz; bz <= maxBz; ++bz) {
                auto block = terrain_->get_block(bx, by, bz);
                if (physics::check_block_collision_3d(block, bx, by, bz,
                        px, py, pz, half_w, physics::kPlayerHeight, half_d)) {
                    return true;
                }
            }
        }
    }
    
    return false;
}

// ============================================================================
// Players
// ============================================================================

void BedWarsServer::on_player_connect(engine::PlayerId id) {
    engine_->log_info("Player " + std::to_string(id) + " connecting...");
    players_[id] = PlayerState{};
}

void BedWarsServer::on_player_disconnect(engine::PlayerId id) {
    engine_->log_info("Player " + std::to_string(id) + " disconnected");
    players_.erase(id);
}

void BedWarsServer::on_player_message(engine::PlayerId id, std::span<const std::uint8_t> data) {
    auto msg = proto::deserialize(data);
    if (!msg) {
        engine_->log_warning("Failed to deserialize message from player " + std::to_string(id));
        return;
    }
    
    std::visit([this, id](const auto& m) {
        using T = std::decay_t<decltype(m)>;
        
        if constexpr (std::is_same_v<T, proto::ClientHello>) {
            handle_client_hello(id, m);
        }
        else if constexpr (std::is_same_v<T, proto::JoinMatch>) {
            handle_join_match(id, m);
        }
        else if constexpr (std::is_same_v<T, proto::InputFrame>) {
            handle_input_frame(id, m);
        }
        else if constexpr (std::is_same_v<T, proto::TryPlaceBlock>) {
            handle_try_place_block(id, m);
        }
        else if constexpr (std::is_same_v<T, proto::TryBreakBlock>) {
            handle_try_break_block(id, m);
        }
        else if constexpr (std::is_same_v<T, proto::TrySetBlock>) {
            handle_try_set_block(id, m);
        }
        else if constexpr (std::is_same_v<T, proto::TryExportMap>) {
            handle_try_export_map(id, m);
        }
    }, *msg);
}

// ============================================================================
// Message Handlers
// ============================================================================

void BedWarsServer::handle_client_hello(engine::PlayerId id, const proto::ClientHello& msg) {
    engine_->log_info("ClientHello from " + std::to_string(id) + ": " + msg.clientName);
    
    auto it = players_.find(id);
    if (it == players_.end()) return;
    
    it->second.name = msg.clientName;
    
    // Send ServerHello with map template info
    proto::ServerHello hello;
    hello.acceptedVersion = proto::kProtocolVersion;
    hello.tickRate = static_cast<std::uint32_t>(engine_->tick_rate());
    hello.worldSeed = worldSeed_;
    hello.hasMapTemplate = hasMapTemplate_;
    hello.mapId = mapId_;
    hello.mapVersion = mapVersion_;
    
    engine_->log_info("Sending ServerHello: hasMapTemplate=" + std::to_string(hasMapTemplate_) + 
                      " mapId=" + mapId_ + " mapVersion=" + std::to_string(mapVersion_));
    
    send_message(id, hello);
}

void BedWarsServer::handle_join_match(engine::PlayerId id, const proto::JoinMatch& /*msg*/) {
    engine_->log_info("Player " + std::to_string(id) + " joining match");
    
    auto it = players_.find(id);
    if (it == players_.end()) return;
    
    it->second.joined = true;
    it->second.alive = true;
    
    // Assign team
    proto::TeamId assignedTeam = assign_team(id);
    it->second.team = assignedTeam;
    
    // Set spawn position based on team
    std::size_t teamIdx = static_cast<std::size_t>(assignedTeam);
    if (teamIdx > 0 && teamIdx <= teams_.size()) {
        const auto& team = teams_[teamIdx - 1];
        it->second.px = team.spawnX;
        it->second.py = team.spawnY;
        it->second.pz = team.spawnZ;
        it->second.hasBed = team.bedAlive;
    } else {
        // Default spawn for no team
        it->second.px = 0.0f;
        it->second.py = 80.0f;
        it->second.pz = 0.0f;
    }
    
    // Send JoinAck
    proto::JoinAck ack;
    ack.playerId = id;
    send_message(id, ack);
    
    // Send all modified blocks to the new player (delta from map template)
    if (!modifiedBlocks_.empty()) {
        engine_->log_info("Sending " + std::to_string(modifiedBlocks_.size()) + " modified blocks to player " + std::to_string(id));
        for (const auto& mb : modifiedBlocks_) {
            if (mb.type == shared::voxel::BlockType::Air) {
                // Send as BlockBroken
                proto::BlockBroken broken;
                broken.x = mb.x;
                broken.y = mb.y;
                broken.z = mb.z;
                send_message(id, broken);
            } else {
                // Send as BlockPlaced
                proto::BlockPlaced placed;
                placed.x = mb.x;
                placed.y = mb.y;
                placed.z = mb.z;
                placed.blockType = mb.type;
                placed.stateByte = mb.state;
                send_message(id, placed);
            }
        }
    }
    
    // Broadcast team assignment
    proto::TeamAssigned teamMsg;
    teamMsg.playerId = id;
    teamMsg.teamId = assignedTeam;
    broadcast_message(teamMsg);
    
    // Send health
    proto::HealthUpdate health;
    health.playerId = id;
    health.hp = it->second.hp;
    health.maxHp = it->second.maxHp;
    send_message(id, health);
    
    engine_->log_info("Player " + std::to_string(id) + " assigned to team " + std::to_string(static_cast<int>(assignedTeam)));
}

void BedWarsServer::handle_input_frame(engine::PlayerId id, const proto::InputFrame& msg) {
    auto it = players_.find(id);
    if (it == players_.end() || !it->second.joined) return;
    
    it->second.lastInput = msg;
}

void BedWarsServer::handle_try_place_block(engine::PlayerId id, const proto::TryPlaceBlock& msg) {
    using namespace shared::voxel;
    
    engine_->log_debug("Player " + std::to_string(id) + " TryPlaceBlock at " +
                       std::to_string(msg.x) + "," + std::to_string(msg.y) + "," + std::to_string(msg.z) +
                       " type=" + std::to_string(static_cast<int>(msg.blockType)));

    auto it = players_.find(id);
    if (it == players_.end() || !it->second.joined) {
        proto::ActionRejected reject;
        reject.seq = msg.seq;
        reject.reason = proto::RejectReason::NotAllowed;
        send_message(id, reject);
        return;
    }

    // Validate Y bounds
    if (msg.y < 0 || msg.y >= static_cast<int>(CHUNK_HEIGHT)) {
        proto::ActionRejected reject;
        reject.seq = msg.seq;
        reject.reason = proto::RejectReason::Invalid;
        send_message(id, reject);
        return;
    }

    // Check reach distance (using eye position)
    proto::RejectReason reason;
    if (!validate_place_block(id, msg, reason)) {
        proto::ActionRejected reject;
        reject.seq = msg.seq;
        reject.reason = reason;
        send_message(id, reject);
        return;
    }

    // Can't place air or bedrock
    if (msg.blockType == BlockType::Air || msg.blockType == BlockType::Bedrock) {
        proto::ActionRejected reject;
        reject.seq = msg.seq;
        reject.reason = proto::RejectReason::Invalid;
        send_message(id, reject);
        return;
    }

    if (!terrain_) return;

    const auto cur = terrain_->get_block(msg.x, msg.y, msg.z);
    const auto curState = terrain_->get_block_state(msg.x, msg.y, msg.z);
    const BlockType reqType = msg.blockType;

    // Handle slab merging: placing slab on same-category slab
    if (is_slab(reqType) && is_slab(cur)) {
        SlabCategory placingCat = get_slab_category(get_base_slab_type(reqType));
        SlabCategory existingCat = get_slab_category(get_base_slab_type(cur));

        if (placingCat == existingCat && curState.slabType != SlabType::Double) {
            SlabType newSlabType = determine_slab_type_from_hit(msg.hitY, msg.face);

            if ((curState.slabType == SlabType::Bottom && newSlabType == SlabType::Top) ||
                (curState.slabType == SlabType::Top && newSlabType == SlabType::Bottom)) {
                // Merge into double slab
                BlockType fullBlock = get_double_slab_type(placingCat);
                terrain_->set_block(msg.x, msg.y, msg.z, fullBlock);
                terrain_->set_block_state(msg.x, msg.y, msg.z, BlockRuntimeState::defaults());

                proto::BlockPlaced placed;
                placed.x = msg.x;
                placed.y = msg.y;
                placed.z = msg.z;
                placed.blockType = fullBlock;
                placed.stateByte = 0;
                broadcast_message(placed);
                return;
            }
        }
    }

    // Normal placement: position must be air
    if (cur != BlockType::Air) {
        proto::ActionRejected reject;
        reject.seq = msg.seq;
        reject.reason = proto::RejectReason::Collision;
        send_message(id, reject);
        return;
    }

    // Normalize slab types and place
    BlockType finalBlockType = get_base_slab_type(reqType);
    terrain_->place_player_block(msg.x, msg.y, msg.z, finalBlockType);

    // Compute block state (connections for fences, slab type)
    auto state = terrain_->compute_block_state(msg.x, msg.y, msg.z, finalBlockType);
    if (is_slab(finalBlockType)) {
        state.slabType = determine_slab_type_from_hit(msg.hitY, msg.face);
    }
    terrain_->set_block_state(msg.x, msg.y, msg.z, state);

    // Track modified block for late joiners
    modifiedBlocks_.push_back({msg.x, msg.y, msg.z, finalBlockType, state.to_byte()});

    // Broadcast placement
    proto::BlockPlaced placed;
    placed.x = msg.x;
    placed.y = msg.y;
    placed.z = msg.z;
    placed.blockType = finalBlockType;
    placed.stateByte = state.to_byte();
    broadcast_message(placed);

    // Update neighbor connections (fences connecting to this block)
    broadcast_neighbor_updates(msg.x, msg.y, msg.z);
}

void BedWarsServer::handle_try_break_block(engine::PlayerId id, const proto::TryBreakBlock& msg) {
    using namespace shared::voxel;
    
    engine_->log_debug("Player " + std::to_string(id) + " TryBreakBlock at " +
                       std::to_string(msg.x) + "," + std::to_string(msg.y) + "," + std::to_string(msg.z));

    auto it = players_.find(id);
    if (it == players_.end() || !it->second.joined) {
        proto::ActionRejected reject;
        reject.seq = msg.seq;
        reject.reason = proto::RejectReason::NotAllowed;
        send_message(id, reject);
        return;
    }

    // Validate Y bounds
    if (msg.y < 0 || msg.y >= static_cast<int>(CHUNK_HEIGHT)) {
        proto::ActionRejected reject;
        reject.seq = msg.seq;
        reject.reason = proto::RejectReason::Invalid;
        send_message(id, reject);
        return;
    }

    // Validate reach and existence
    proto::RejectReason reason;
    if (!validate_break_block(id, msg, reason)) {
        proto::ActionRejected reject;
        reject.seq = msg.seq;
        reject.reason = reason;
        send_message(id, reject);
        return;
    }

    if (!terrain_) return;

    const auto cur = terrain_->get_block(msg.x, msg.y, msg.z);
    
    // Check if this is a bed position
    TeamState* bedTeam = get_team_at_bed(msg.x, msg.y, msg.z);
    if (bedTeam && bedTeam->bedAlive) {
        auto playerIt = players_.find(id);
        if (playerIt != players_.end() && playerIt->second.team == bedTeam->id) {
            // Can't break own bed
            proto::ActionRejected reject;
            reject.seq = msg.seq;
            reject.reason = proto::RejectReason::ProtectedBlock;
            send_message(id, reject);
            return;
        }
        // Process bed break (enemy bed)
        process_bed_break(msg.x, msg.y, msg.z, id);
        // Don't return - still break the block normally after processing bed destruction
    }
    
    // Check protected blocks (beds, template blocks, etc.)
    if (!terrain_->can_player_break(msg.x, msg.y, msg.z, cur)) {
        proto::ActionRejected reject;
        reject.seq = msg.seq;
        reject.reason = proto::RejectReason::ProtectedBlock;
        send_message(id, reject);
        return;
    }

    // Break the block
    terrain_->break_player_block(msg.x, msg.y, msg.z);
    terrain_->set_block_state(msg.x, msg.y, msg.z, BlockRuntimeState::defaults());

    // Track modified block for late joiners (Air block)
    modifiedBlocks_.push_back({msg.x, msg.y, msg.z, BlockType::Air, 0});

    // Broadcast break event
    proto::BlockBroken broken;
    broken.x = msg.x;
    broken.y = msg.y;
    broken.z = msg.z;
    broadcast_message(broken);

    // Update neighbor connections (fences that were connected to this block)
    broadcast_neighbor_updates(msg.x, msg.y, msg.z);
}

void BedWarsServer::handle_try_set_block(engine::PlayerId id, const proto::TrySetBlock& msg) {
    using namespace shared::voxel;
    
    // Editor-only: directly set block without resource validation
    if (!opts_.editorCameraMode) {
        proto::ActionRejected reject;
        reject.seq = msg.seq;
        reject.reason = proto::RejectReason::NotAllowed;
        send_message(id, reject);
        return;
    }
    
    // Validate Y bounds
    if (msg.y < 0 || msg.y >= static_cast<int>(CHUNK_HEIGHT)) {
        proto::ActionRejected reject;
        reject.seq = msg.seq;
        reject.reason = proto::RejectReason::Invalid;
        send_message(id, reject);
        return;
    }
    
    engine_->log_debug("Player " + std::to_string(id) + " TrySetBlock at " +
                       std::to_string(msg.x) + "," + std::to_string(msg.y) + "," + std::to_string(msg.z) +
                       " type=" + std::to_string(static_cast<int>(msg.blockType)));
    
    if (!terrain_) return;

    const auto prev = terrain_->get_block(msg.x, msg.y, msg.z);
    const BlockType reqType = msg.blockType;
    
    terrain_->set_block(msg.x, msg.y, msg.z, reqType);
    const auto cur = terrain_->get_block(msg.x, msg.y, msg.z);

    // No-op check (placing same block that already exists)
    if (cur == prev && cur != BlockType::Air) {
        proto::ActionRejected reject;
        reject.seq = msg.seq;
        reject.reason = proto::RejectReason::Invalid;
        send_message(id, reject);
        return;
    }

    if (cur == BlockType::Air) {
        // Breaking via editor
        proto::BlockBroken broken;
        broken.x = msg.x;
        broken.y = msg.y;
        broken.z = msg.z;
        broadcast_message(broken);
    } else {
        // Compute and set block state
        auto state = terrain_->compute_block_state(msg.x, msg.y, msg.z, cur);
        if (is_slab(cur)) {
            state.slabType = determine_slab_type_from_hit(msg.hitY, msg.face);
        }
        terrain_->set_block_state(msg.x, msg.y, msg.z, state);

        proto::BlockPlaced placed;
        placed.x = msg.x;
        placed.y = msg.y;
        placed.z = msg.z;
        placed.blockType = cur;
        placed.stateByte = state.to_byte();
        broadcast_message(placed);
    }

    // Update neighbor connections
    broadcast_neighbor_updates(msg.x, msg.y, msg.z);
}

void BedWarsServer::handle_try_export_map(engine::PlayerId id, const proto::TryExportMap& msg) {
    engine_->log_info("Player " + std::to_string(id) + " requested map export: " + msg.mapId + 
                      " v" + std::to_string(msg.version) + 
                      " chunks=[(" + std::to_string(msg.chunkMinX) + "," + std::to_string(msg.chunkMinZ) + 
                      ")-(" + std::to_string(msg.chunkMaxX) + "," + std::to_string(msg.chunkMaxZ) + ")]");
    
    proto::ExportResult result;
    result.seq = msg.seq;
    
    // Editor mode required
    if (!opts_.editorCameraMode) {
        result.ok = false;
        result.reason = proto::RejectReason::NotAllowed;
        result.path = "";
        send_message(id, result);
        return;
    }
    
    // Validate map ID (alphanumeric, underscores, hyphens)
    auto is_valid_map_id = [](const std::string& s) {
        if (s.empty() || s.size() > 64) return false;
        for (char c : s) {
            if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_' && c != '-') return false;
        }
        return true;
    };
    
    if (!is_valid_map_id(msg.mapId) || msg.version == 0) {
        result.ok = false;
        result.reason = proto::RejectReason::Invalid;
        result.path = "";
        send_message(id, result);
        return;
    }
    
    // Validate bounds
    if (msg.chunkMinX > msg.chunkMaxX || msg.chunkMinZ > msg.chunkMaxZ) {
        result.ok = false;
        result.reason = proto::RejectReason::Invalid;
        result.path = "";
        send_message(id, result);
        return;
    }
    
    // Maps directory
    const std::filesystem::path mapsDir = shared::maps::runtime_maps_dir();
    std::error_code ec;
    std::filesystem::create_directories(mapsDir, ec);
    if (ec) {
        engine_->log_error("Failed to create maps directory: " + ec.message());
        result.ok = false;
        result.reason = proto::RejectReason::Unknown;
        result.path = "";
        send_message(id, result);
        return;
    }
    
    const auto fileName = msg.mapId + "_v" + std::to_string(msg.version) + ".rfmap";
    const auto outPath = mapsDir / fileName;
    
    // Prepare export request
    shared::maps::ExportRequest exportReq;
    exportReq.mapId = msg.mapId;
    exportReq.version = msg.version;
    exportReq.bounds.chunkMinX = msg.chunkMinX;
    exportReq.bounds.chunkMinZ = msg.chunkMinZ;
    exportReq.bounds.chunkMaxX = msg.chunkMaxX;
    exportReq.bounds.chunkMaxZ = msg.chunkMaxZ;
    
    // Copy breakable blocks from current template if present
    if (terrain_ && terrain_->has_map_template()) {
        if (const auto* tmpl = terrain_->map_template(); tmpl) {
            exportReq.breakableTemplateBlocks = tmpl->breakableTemplateBlocks;
        }
    }
    
    // Visual settings from request (clamp values)
    exportReq.visualSettings = shared::maps::default_visual_settings();
    {
        constexpr std::uint8_t kMaxSkyboxId = 25u;
        exportReq.visualSettings.skyboxKind = static_cast<shared::maps::MapTemplate::SkyboxKind>(
            std::min(msg.skyboxKind, kMaxSkyboxId));
        
        exportReq.visualSettings.timeOfDayHours = std::clamp(msg.timeOfDayHours, 0.0f, 24.0f);
        exportReq.visualSettings.useMoon = msg.useMoon;
        exportReq.visualSettings.sunIntensity = std::clamp(msg.sunIntensity, 0.0f, 10.0f);
        exportReq.visualSettings.ambientIntensity = std::clamp(msg.ambientIntensity, 0.0f, 5.0f);
        exportReq.visualSettings.temperature = std::clamp(msg.temperature, 0.0f, 1.0f);
        exportReq.visualSettings.humidity = std::clamp(msg.humidity, 0.0f, 1.0f);
    }
    
    // Write map file
    std::string err;
    
    // Debug: count blocks before export
    std::uint32_t totalBlocks = 0;
    for (int cz = msg.chunkMinZ; cz <= msg.chunkMaxZ; cz++) {
        for (int cx = msg.chunkMinX; cx <= msg.chunkMaxX; cx++) {
            for (int y = 0; y < static_cast<int>(shared::voxel::CHUNK_HEIGHT); y++) {
                for (int lz = 0; lz < static_cast<int>(shared::voxel::CHUNK_DEPTH); lz++) {
                    for (int lx = 0; lx < static_cast<int>(shared::voxel::CHUNK_WIDTH); lx++) {
                        int wx = cx * shared::voxel::CHUNK_WIDTH + lx;
                        int wz = cz * shared::voxel::CHUNK_DEPTH + lz;
                        auto bt = terrain_->get_block(wx, y, wz);
                        if (bt != shared::voxel::BlockType::Air) {
                            totalBlocks++;
                        }
                    }
                }
            }
        }
    }
    engine_->log_info("Export: found " + std::to_string(totalBlocks) + " non-air blocks in terrain");
    
    const bool ok = shared::maps::write_rfmap(
        outPath,
        exportReq,
        [this](int x, int y, int z) { return terrain_->get_block(x, y, z); },
        &err);
    
    if (!ok) {
        engine_->log_error("Failed to write map: " + err);
        result.ok = false;
        result.reason = proto::RejectReason::Unknown;
        result.path = "";
        send_message(id, result);
        return;
    }
    
    engine_->log_info("Map exported successfully: " + outPath.string());
    result.ok = true;
    result.reason = proto::RejectReason::Unknown;  // Not used on success
    result.path = outPath.string();
    send_message(id, result);
}

// ============================================================================
// Block Validation
// ============================================================================

bool BedWarsServer::validate_place_block(engine::PlayerId id, const proto::TryPlaceBlock& msg, 
                                          proto::RejectReason& reason) {
    auto it = players_.find(id);
    if (it == players_.end()) {
        reason = proto::RejectReason::Invalid;
        return false;
    }
    
    const auto& player = it->second;
    
    // Use eye position for reach calculation (like Minecraft)
    const float eyeY = player.py + bedwars::kPlayerEyeHeight;
    float dx = msg.x + 0.5f - player.px;
    float dy = msg.y + 0.5f - eyeY;
    float dz = msg.z + 0.5f - player.pz;
    float distSq = dx*dx + dy*dy + dz*dz;
    
    if (distSq > bedwars::kBlockReachDistance * bedwars::kBlockReachDistance) {
        reason = proto::RejectReason::OutOfRange;
        return false;
    }
    
    // Check if block would intersect player AABB
    if (would_intersect_player(player.px, player.py, player.pz, msg.x, msg.y, msg.z)) {
        reason = proto::RejectReason::Collision;
        return false;
    }
    
    return true;
}

bool BedWarsServer::validate_break_block(engine::PlayerId id, const proto::TryBreakBlock& msg,
                                          proto::RejectReason& reason) {
    auto it = players_.find(id);
    if (it == players_.end()) {
        reason = proto::RejectReason::Invalid;
        return false;
    }
    
    const auto& player = it->second;
    
    // Use eye position for reach calculation
    const float eyeY = player.py + bedwars::kPlayerEyeHeight;
    float dx = msg.x + 0.5f - player.px;
    float dy = msg.y + 0.5f - eyeY;
    float dz = msg.z + 0.5f - player.pz;
    float distSq = dx*dx + dy*dy + dz*dz;
    
    if (distSq > bedwars::kBlockReachDistance * bedwars::kBlockReachDistance) {
        reason = proto::RejectReason::OutOfRange;
        return false;
    }
    
    // Check if block exists
    if (terrain_) {
        auto existing = terrain_->get_block(msg.x, msg.y, msg.z);
        if (existing == shared::voxel::BlockType::Air) {
            reason = proto::RejectReason::Invalid;
            return false;
        }
    }
    
    return true;
}

void BedWarsServer::broadcast_neighbor_updates(int x, int y, int z) {
    if (!terrain_) return;
    
    auto updates = terrain_->update_neighbor_states(x, y, z);
    for (const auto& update : updates) {
        proto::BlockPlaced neighborPlaced;
        neighborPlaced.x = update.x;
        neighborPlaced.y = update.y;
        neighborPlaced.z = update.z;
        neighborPlaced.blockType = update.type;
        neighborPlaced.stateByte = update.state.to_byte();
        
        engine_->log_debug("Neighbor update at " + std::to_string(update.x) + "," + 
                          std::to_string(update.y) + "," + std::to_string(update.z) +
                          " type=" + std::to_string(static_cast<int>(update.type)) +
                          " state=" + std::to_string(neighborPlaced.stateByte));
        broadcast_message(neighborPlaced);
    }
}

void BedWarsServer::send_chunk_data(engine::PlayerId id, int chunkX, int chunkZ) {
    if (!terrain_) return;
    
    proto::ChunkData chunk;
    chunk.chunkX = chunkX;
    chunk.chunkZ = chunkZ;
    
    // Reserve space for 16x256x16 blocks
    constexpr int kChunkSize = 16;
    constexpr int kWorldHeight = 256;
    chunk.blocks.resize(kChunkSize * kWorldHeight * kChunkSize);
    
    int baseX = chunkX * kChunkSize;
    int baseZ = chunkZ * kChunkSize;
    
    for (int y = 0; y < kWorldHeight; ++y) {
        for (int z = 0; z < kChunkSize; ++z) {
            for (int x = 0; x < kChunkSize; ++x) {
                int idx = y * kChunkSize * kChunkSize + z * kChunkSize + x;
                auto block = terrain_->get_block(baseX + x, y, baseZ + z);
                chunk.blocks[idx] = static_cast<std::uint8_t>(block);
            }
        }
    }
    
    send_message(id, chunk);
}

// ============================================================================
// Helpers
// ============================================================================

void BedWarsServer::send_message(engine::PlayerId id, const proto::Message& msg) {
    auto data = proto::serialize(msg);
    engine_->send(id, data);
}

void BedWarsServer::broadcast_message(const proto::Message& msg) {
    auto data = proto::serialize(msg);
    engine_->broadcast(data);
}

// ============================================================================
// TeamState
// ============================================================================

bool TeamState::is_eliminated() const {
    if (bedAlive) return false;
    // Team is eliminated if bed is destroyed and no members are alive
    return members.empty();  // Simplified: actual check would verify player alive state
}

// ============================================================================
// Match Flow
// ============================================================================

void BedWarsServer::update_match_phase(float dt) {
    phaseTimer_ += dt;
    
    switch (matchPhase_) {
        case MatchPhase::Waiting: {
            // Count joined players
            std::size_t joinedCount = 0;
            for (const auto& [id, player] : players_) {
                if (player.joined) ++joinedCount;
            }
            
            if (joinedCount >= matchConfig_.minPlayersToStart && opts_.autoStartMatch) {
                matchPhase_ = MatchPhase::Starting;
                phaseTimer_ = 0.0f;
                engine_->log_info("Match starting countdown...");
            }
            break;
        }
        
        case MatchPhase::Starting: {
            if (phaseTimer_ >= matchConfig_.startCountdown) {
                start_match();
            }
            break;
        }
        
        case MatchPhase::InProgress: {
            check_win_condition();
            break;
        }
        
        case MatchPhase::Ending: {
            if (phaseTimer_ >= matchConfig_.endDelay) {
                matchPhase_ = MatchPhase::Finished;
                engine_->log_info("Match finished");
            }
            break;
        }
        
        case MatchPhase::Finished:
            // Do nothing, wait for server restart or new match
            break;
    }
}

void BedWarsServer::start_match() {
    matchPhase_ = MatchPhase::InProgress;
    phaseTimer_ = 0.0f;
    engine_->log_info("Match started!");
    
    // Teleport players to their team spawn points
    for (auto& [id, player] : players_) {
        if (!player.joined) continue;
        
        std::size_t teamIdx = static_cast<std::size_t>(player.team);
        if (teamIdx > 0 && teamIdx <= teams_.size()) {
            const auto& team = teams_[teamIdx - 1];
            player.px = team.spawnX;
            player.py = team.spawnY;
            player.pz = team.spawnZ;
            player.vx = player.vy = player.vz = 0.0f;
            player.alive = true;
            player.hp = player.maxHp;
        }
    }
    
    // Activate generators
    for (auto& gen : generators_) {
        gen.isActive = true;
        gen.timeUntilSpawn = gen.spawnInterval;
    }
}

void BedWarsServer::end_match(proto::TeamId winner) {
    matchPhase_ = MatchPhase::Ending;
    phaseTimer_ = 0.0f;
    
    engine_->log_info("Match ended! Winner: Team " + std::to_string(static_cast<int>(winner)));
    
    proto::MatchEnded msg;
    msg.winnerTeamId = winner;
    broadcast_message(msg);
}

void BedWarsServer::check_win_condition() {
    if (matchPhase_ != MatchPhase::InProgress) return;
    
    // Count teams that are not eliminated
    std::vector<proto::TeamId> aliveTeams;
    for (const auto& team : teams_) {
        if (team.id == proto::Teams::None) continue;
        
        // Check if team has any alive players or bed
        bool hasAlivePlayers = false;
        for (const auto& memberId : team.members) {
            auto it = players_.find(memberId);
            if (it != players_.end() && it->second.alive) {
                hasAlivePlayers = true;
                break;
            }
        }
        
        if (team.bedAlive || hasAlivePlayers) {
            aliveTeams.push_back(team.id);
        }
    }
    
    if (aliveTeams.size() == 1) {
        end_match(aliveTeams[0]);
    } else if (aliveTeams.empty()) {
        end_match(proto::Teams::None);  // Draw
    }
}

proto::TeamId BedWarsServer::assign_team(engine::PlayerId playerId) {
    // Round-robin team assignment
    std::size_t bestTeamIdx = 0;
    std::size_t minPlayers = SIZE_MAX;
    
    for (std::size_t i = 0; i < matchConfig_.teamCount && i < teams_.size(); ++i) {
        if (teams_[i].members.size() < minPlayers &&
            teams_[i].members.size() < matchConfig_.maxPlayersPerTeam) {
            minPlayers = teams_[i].members.size();
            bestTeamIdx = i;
        }
    }
    
    if (minPlayers < matchConfig_.maxPlayersPerTeam) {
        teams_[bestTeamIdx].members.push_back(playerId);
        return teams_[bestTeamIdx].id;
    }
    
    return proto::Teams::None;  // All teams full
}

// ============================================================================
// Combat
// ============================================================================

void BedWarsServer::process_damage(engine::PlayerId targetId, std::uint8_t damage, engine::PlayerId attackerId) {
    auto it = players_.find(targetId);
    if (it == players_.end() || !it->second.alive) return;
    
    auto& target = it->second;
    
    // Friendly fire check
    if (!matchConfig_.friendlyFire && attackerId != 0) {
        auto attackerIt = players_.find(attackerId);
        if (attackerIt != players_.end() && attackerIt->second.team == target.team) {
            return;  // No friendly fire
        }
    }
    
    // Apply damage
    if (damage >= target.hp) {
        target.hp = 0;
        process_death(targetId, attackerId);
    } else {
        target.hp -= damage;
        target.lastDamageTaken = 0.0f;  // Reset regen timer
        
        // Send health update
        proto::HealthUpdate health;
        health.playerId = targetId;
        health.hp = target.hp;
        health.maxHp = target.maxHp;
        broadcast_message(health);
    }
}

void BedWarsServer::process_death(engine::PlayerId playerId, engine::PlayerId killerId) {
    auto it = players_.find(playerId);
    if (it == players_.end()) return;
    
    auto& player = it->second;
    player.alive = false;
    player.hp = 0;
    
    // Check if this is a final kill (no bed)
    bool isFinalKill = !player.hasBed;
    
    // Update team bed status
    std::size_t teamIdx = static_cast<std::size_t>(player.team);
    if (teamIdx > 0 && teamIdx <= teams_.size()) {
        player.hasBed = teams_[teamIdx - 1].bedAlive;
        isFinalKill = !player.hasBed;
    }
    
    // Broadcast death
    proto::PlayerDied death;
    death.victimId = playerId;
    death.killerId = killerId;
    death.isFinalKill = isFinalKill;
    broadcast_message(death);
    
    engine_->log_info("Player " + std::to_string(playerId) + " died" +
                      (isFinalKill ? " (FINAL KILL)" : ""));
    
    // Start respawn timer if not final kill
    if (!isFinalKill) {
        player.respawnTimer = matchConfig_.respawnDelay;
    } else {
        // Remove from team
        if (teamIdx > 0 && teamIdx <= teams_.size()) {
            auto& members = teams_[teamIdx - 1].members;
            members.erase(std::remove(members.begin(), members.end(), playerId), members.end());
        }
        
        // Check if team is eliminated
        check_win_condition();
    }
}

void BedWarsServer::process_respawn(engine::PlayerId playerId) {
    auto it = players_.find(playerId);
    if (it == players_.end() || it->second.alive) return;
    
    auto& player = it->second;
    
    // Can't respawn without bed
    if (!player.hasBed) return;
    
    // Get team spawn point
    std::size_t teamIdx = static_cast<std::size_t>(player.team);
    if (teamIdx > 0 && teamIdx <= teams_.size()) {
        const auto& team = teams_[teamIdx - 1];
        player.px = team.spawnX;
        player.py = team.spawnY;
        player.pz = team.spawnZ;
    }
    
    player.vx = player.vy = player.vz = 0.0f;
    player.alive = true;
    player.hp = player.maxHp;
    player.respawnTimer = 0.0f;
    
    // Broadcast respawn
    proto::PlayerRespawned respawn;
    respawn.playerId = playerId;
    respawn.x = player.px;
    respawn.y = player.py;
    respawn.z = player.pz;
    broadcast_message(respawn);
    
    engine_->log_info("Player " + std::to_string(playerId) + " respawned");
}

void BedWarsServer::update_regeneration(float dt) {
    for (auto& [id, player] : players_) {
        if (!player.alive || player.hp >= player.maxHp) continue;
        
        player.lastDamageTaken += dt;
        
        if (player.lastDamageTaken >= matchConfig_.regenDelay) {
            // Regenerate 1 HP per second after delay
            player.hp = std::min<std::uint8_t>(player.hp + 1, player.maxHp);
            
            proto::HealthUpdate health;
            health.playerId = id;
            health.hp = player.hp;
            health.maxHp = player.maxHp;
            send_message(id, health);
        }
    }
}

void BedWarsServer::update_respawns(float dt) {
    for (auto& [id, player] : players_) {
        if (player.alive || player.respawnTimer <= 0.0f) continue;
        
        player.respawnTimer -= dt;
        if (player.respawnTimer <= 0.0f) {
            process_respawn(id);
        }
    }
}

// ============================================================================
// Generators & Items
// ============================================================================

void BedWarsServer::update_generators(float dt) {
    if (matchPhase_ != MatchPhase::InProgress) return;
    
    for (auto& gen : generators_) {
        if (!gen.isActive) continue;
        
        gen.timeUntilSpawn -= dt;
        if (gen.timeUntilSpawn <= 0.0f) {
            spawn_item(gen);
            gen.timeUntilSpawn = gen.spawnInterval;
        }
    }
}

void BedWarsServer::spawn_item(const GeneratorState& gen) {
    DroppedItemState item;
    item.entityId = next_entity_id();
    item.x = gen.x;
    item.y = gen.y;
    item.z = gen.z;
    item.count = 1;
    item.lifetime = 0.0f;
    item.pickupDelay = 0.0f;
    item.active = true;
    
    // Item type based on generator tier
    switch (gen.tier) {
        case 0: item.itemType = proto::ItemType::Iron; break;
        case 1: item.itemType = proto::ItemType::Gold; break;
        case 2: item.itemType = proto::ItemType::Diamond; break;
        case 3: item.itemType = proto::ItemType::Emerald; break;
        default: item.itemType = proto::ItemType::Iron; break;
    }
    
    droppedItems_.push_back(item);
    
    // Broadcast spawn
    proto::ItemSpawned spawned;
    spawned.entityId = item.entityId;
    spawned.itemType = item.itemType;
    spawned.x = item.x;
    spawned.y = item.y;
    spawned.z = item.z;
    spawned.count = item.count;
    broadcast_message(spawned);
}

void BedWarsServer::update_items(float dt) {
    for (auto& item : droppedItems_) {
        if (!item.active) continue;
        
        item.lifetime += dt;
        if (item.pickupDelay > 0.0f) {
            item.pickupDelay -= dt;
        }
        
        // Despawn after 5 minutes
        if (item.lifetime > 300.0f) {
            item.active = false;
        }
    }
    
    // Remove inactive items
    droppedItems_.erase(
        std::remove_if(droppedItems_.begin(), droppedItems_.end(),
                       [](const DroppedItemState& i) { return !i.active; }),
        droppedItems_.end()
    );
}

void BedWarsServer::process_item_pickup(engine::PlayerId playerId) {
    auto playerIt = players_.find(playerId);
    if (playerIt == players_.end() || !playerIt->second.alive) return;
    
    auto& player = playerIt->second;
    
    for (auto& item : droppedItems_) {
        if (!item.active || item.pickupDelay > 0.0f) continue;
        
        // Check distance
        float dx = item.x - player.px;
        float dy = item.y - player.py;
        float dz = item.z - player.pz;
        float distSq = dx*dx + dy*dy + dz*dz;
        
        if (distSq <= matchConfig_.itemPickupRadius * matchConfig_.itemPickupRadius) {
            // Add to inventory
            player.inventory[item.itemType] += item.count;
            item.active = false;
            
            // Broadcast pickup
            proto::ItemPickedUp pickup;
            pickup.entityId = item.entityId;
            pickup.playerId = playerId;
            broadcast_message(pickup);
            
            // Send inventory update
            proto::InventoryUpdate inv;
            inv.playerId = playerId;
            inv.itemType = item.itemType;
            inv.count = player.inventory[item.itemType];
            send_message(playerId, inv);
        }
    }
}

std::uint32_t BedWarsServer::next_entity_id() {
    return nextEntityId_++;
}

// ============================================================================
// Beds
// ============================================================================

void BedWarsServer::process_bed_break(int x, int y, int z, engine::PlayerId breakerId) {
    TeamState* team = get_team_at_bed(x, y, z);
    if (!team || !team->bedAlive) return;
    
    team->bedAlive = false;
    
    // Update all team members' hasBed flag
    for (auto memberId : team->members) {
        auto it = players_.find(memberId);
        if (it != players_.end()) {
            it->second.hasBed = false;
        }
    }
    
    // Broadcast bed destruction
    proto::BedDestroyed bedMsg;
    bedMsg.teamId = team->id;
    bedMsg.destroyerId = breakerId;
    broadcast_message(bedMsg);
    
    engine_->log_info("Bed destroyed! Team " + std::to_string(static_cast<int>(team->id)));
    
    // Check if team is eliminated (no alive players + no bed)
    bool hasAlivePlayers = false;
    for (auto memberId : team->members) {
        auto it = players_.find(memberId);
        if (it != players_.end() && it->second.alive) {
            hasAlivePlayers = true;
            break;
        }
    }
    
    if (!hasAlivePlayers) {
        proto::TeamEliminated elim;
        elim.teamId = team->id;
        broadcast_message(elim);
        engine_->log_info("Team " + std::to_string(static_cast<int>(team->id)) + " eliminated!");
    }
    
    check_win_condition();
}

TeamState* BedWarsServer::get_team_at_bed(int x, int y, int z) {
    for (auto& team : teams_) {
        if (team.id == proto::Teams::None) continue;
        
        // Simple check: is position within 1 block of bed position
        if (std::abs(x - team.bedX) <= 1 &&
            std::abs(y - team.bedY) <= 1 &&
            std::abs(z - team.bedZ) <= 1) {
            return &team;
        }
    }
    return nullptr;
}

} // namespace bedwars::server
