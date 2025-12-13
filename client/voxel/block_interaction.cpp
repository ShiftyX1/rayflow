#include "block_interaction.hpp"
#include "block_registry.hpp"
#include <cmath>

namespace voxel {

static void face_to_offset(int face, int& ox, int& oy, int& oz) {
    ox = 0;
    oy = 0;
    oz = 0;
    switch (face) {
        case 0: ox = 1; break;  // +X
        case 1: ox = -1; break; // -X
        case 2: oy = 1; break;  // +Y
        case 3: oy = -1; break; // -Y
        case 4: oz = 1; break;  // +Z
        case 5: oz = -1; break; // -Z
        default: break;
    }
}

std::optional<BlockInteraction::BreakRequest> BlockInteraction::consume_break_request() {
    auto out = outgoing_break_;
    outgoing_break_.reset();
    return out;
}

std::optional<BlockInteraction::PlaceRequest> BlockInteraction::consume_place_request() {
    auto out = outgoing_place_;
    outgoing_place_.reset();
    return out;
}

void BlockInteraction::on_action_rejected() {
    pending_break_.reset();
    pending_place_.reset();
    break_progress_ = 0.0f;
    was_breaking_ = false;
    was_placing_ = false;
}

void BlockInteraction::update(World& world, const Vector3& camera_pos, const Vector3& camera_dir,
                               const ecs::ToolHolder& tool, bool is_breaking, bool is_placing, float delta_time) {
    // Perform raycast
    target_ = raycast(world, camera_pos, camera_dir, MAX_REACH_DISTANCE);

    // Clear pending requests if the world already reflects the server result.
    if (pending_break_.has_value()) {
        const auto b = world.get_block(pending_break_->x, pending_break_->y, pending_break_->z);
        if (b == static_cast<Block>(BlockType::Air)) {
            pending_break_.reset();
        }
    }
    if (pending_place_.has_value()) {
        const auto b = world.get_block(pending_place_->x, pending_place_->y, pending_place_->z);
        if (b != static_cast<Block>(BlockType::Air)) {
            pending_place_.reset();
        }
    }
    
    if (!target_.hit) {
        break_progress_ = 0.0f;
        was_breaking_ = false;
        was_placing_ = false;
        return;
    }
    
    if (is_breaking) {
        if (pending_break_.has_value() &&
            pending_break_->x == target_.block_x &&
            pending_break_->y == target_.block_y &&
            pending_break_->z == target_.block_z) {
            was_breaking_ = true;
        } else {
        if (!was_breaking_) {
            break_progress_ = 0.0f;
        }
        
        float break_time = calculate_break_time(target_.block_type, tool);
        if (break_time > 0.0f) {
            break_progress_ += delta_time / break_time;
            
            if (break_progress_ >= 1.0f) {
                // Request break on the server (client must not mutate world).
                BreakRequest req;
                req.x = target_.block_x;
                req.y = target_.block_y;
                req.z = target_.block_z;
                outgoing_break_ = req;
                pending_break_ = req;
                break_progress_ = 0.0f;
            }
        }
        
        was_breaking_ = true;
        }
    } else {
        break_progress_ = 0.0f;
        was_breaking_ = false;
    }

    // Place is edge-triggered to avoid spamming while holding RMB.
    if (is_placing && !was_placing_) {
        int ox = 0, oy = 0, oz = 0;
        face_to_offset(target_.face, ox, oy, oz);
        const int px = target_.block_x + ox;
        const int py = target_.block_y + oy;
        const int pz = target_.block_z + oz;

        if (!pending_place_.has_value() || pending_place_->x != px || pending_place_->y != py || pending_place_->z != pz) {
            const auto existing = world.get_block(px, py, pz);
            if (existing == static_cast<Block>(BlockType::Air)) {
                PlaceRequest req;
                req.x = px;
                req.y = py;
                req.z = pz;
                req.block_type = BlockType::Dirt;
                outgoing_place_ = req;
                pending_place_ = req;
            }
        }
    }
    was_placing_ = is_placing;
}

BlockRaycastResult BlockInteraction::raycast(const World& world, const Vector3& origin,
                                              const Vector3& direction, float max_distance) const {
    BlockRaycastResult result;
    
    // Normalize direction
    float len = std::sqrt(direction.x * direction.x + direction.y * direction.y + direction.z * direction.z);
    if (len < 0.0001f) return result;
    
    Vector3 dir = {direction.x / len, direction.y / len, direction.z / len};
    
    // DDA algorithm for voxel traversal
    int x = static_cast<int>(std::floor(origin.x));
    int y = static_cast<int>(std::floor(origin.y));
    int z = static_cast<int>(std::floor(origin.z));
    
    int step_x = (dir.x >= 0) ? 1 : -1;
    int step_y = (dir.y >= 0) ? 1 : -1;
    int step_z = (dir.z >= 0) ? 1 : -1;
    
    float t_delta_x = (dir.x != 0) ? std::abs(1.0f / dir.x) : 1e30f;
    float t_delta_y = (dir.y != 0) ? std::abs(1.0f / dir.y) : 1e30f;
    float t_delta_z = (dir.z != 0) ? std::abs(1.0f / dir.z) : 1e30f;
    
    float t_max_x = (dir.x != 0) ? ((step_x > 0 ? (x + 1 - origin.x) : (origin.x - x)) * t_delta_x) : 1e30f;
    float t_max_y = (dir.y != 0) ? ((step_y > 0 ? (y + 1 - origin.y) : (origin.y - y)) * t_delta_y) : 1e30f;
    float t_max_z = (dir.z != 0) ? ((step_z > 0 ? (z + 1 - origin.z) : (origin.z - z)) * t_delta_z) : 1e30f;
    
    float distance = 0.0f;
    int face = 0;
    
    while (distance < max_distance) {
        Block block = world.get_block(x, y, z);
        
        if (block != static_cast<Block>(BlockType::Air)) {
            result.hit = true;
            result.block_x = x;
            result.block_y = y;
            result.block_z = z;
            result.face = face;
            result.distance = distance;
            result.block_type = static_cast<BlockType>(block);
            return result;
        }
        
        // Step to next voxel
        if (t_max_x < t_max_y && t_max_x < t_max_z) {
            distance = t_max_x;
            t_max_x += t_delta_x;
            x += step_x;
            face = (step_x > 0) ? 1 : 0;  // -X or +X face
        } else if (t_max_y < t_max_z) {
            distance = t_max_y;
            t_max_y += t_delta_y;
            y += step_y;
            face = (step_y > 0) ? 3 : 2;  // -Y or +Y face
        } else {
            distance = t_max_z;
            t_max_z += t_delta_z;
            z += step_z;
            face = (step_z > 0) ? 5 : 4;  // -Z or +Z face
        }
    }
    
    return result;
}

float BlockInteraction::calculate_break_time(BlockType block_type, const ecs::ToolHolder& tool) const {
    const auto& info = BlockRegistry::instance().get_block_info(block_type);
    
    if (info.hardness < 0) {
        return -1.0f;  // Unbreakable (bedrock)
    }
    
    float base_time = info.hardness;
    float mining_speed = tool.get_mining_speed();
    
    // Check if tool meets required level
    if (tool.get_harvest_level() < info.required_tool_level) {
        mining_speed = 1.0f;  // Use base speed if wrong tool
    }
    
    return base_time / mining_speed;
}

void BlockInteraction::render_highlight(const Camera3D& camera) const {
    if (!target_.hit) return;
    
    Vector3 pos = {
        static_cast<float>(target_.block_x) + 0.5f,
        static_cast<float>(target_.block_y) + 0.5f,
        static_cast<float>(target_.block_z) + 0.5f
    };
    
    DrawCubeWires(pos, 1.02f, 1.02f, 1.02f, BLACK);
}

void BlockInteraction::render_break_overlay(const Camera3D& camera) const {
    if (!target_.hit || break_progress_ <= 0.0f) return;
    
    Vector3 pos = {
        static_cast<float>(target_.block_x) + 0.5f,
        static_cast<float>(target_.block_y) + 0.5f,
        static_cast<float>(target_.block_z) + 0.5f
    };
    
    // Draw breaking progress overlay
    unsigned char alpha = static_cast<unsigned char>(break_progress_ * 200);
    Color overlay = {0, 0, 0, alpha};
    
    DrawCube(pos, 1.01f, 1.01f, 1.01f, overlay);
}

void BlockInteraction::render_crosshair(int screen_width, int screen_height) {
    int center_x = screen_width / 2;
    int center_y = screen_height / 2;
    int size = 10;
    int thickness = 2;
    
    DrawRectangle(center_x - size, center_y - thickness/2, size * 2, thickness, WHITE);
    DrawRectangle(center_x - thickness/2, center_y - size, thickness, size * 2, WHITE);
    
    DrawRectangleLines(center_x - size - 1, center_y - thickness/2 - 1, 
                       size * 2 + 2, thickness + 2, BLACK);
    DrawRectangleLines(center_x - thickness/2 - 1, center_y - size - 1, 
                       thickness + 2, size * 2 + 2, BLACK);
}

} // namespace voxel
