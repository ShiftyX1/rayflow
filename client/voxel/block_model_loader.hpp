#pragma once

#include "voxel/block_shape.hpp"
#include "block.hpp"
#include <string>
#include <unordered_map>
#include <memory>
#include <optional>

namespace voxel {


class BlockModelLoader {
public:
    static BlockModelLoader& instance();
    
    bool init(const std::string& models_path = "models/block");
    
    void destroy();
    
    const shared::voxel::BlockModel* get_model(BlockType type) const;
    
    const shared::voxel::BlockModel* get_model_by_id(const std::string& id) const;
    
    std::optional<shared::voxel::BlockModel> load_model_file(const std::string& path);
    
    void register_model(BlockType type, shared::voxel::BlockModel model);
    
    bool is_initialized() const { return initialized_; }
    
private:
    BlockModelLoader() = default;
    ~BlockModelLoader() = default;
    
    BlockModelLoader(const BlockModelLoader&) = delete;
    BlockModelLoader& operator=(const BlockModelLoader&) = delete;
    
    void register_builtin_models();
    
    void resolve_parent(shared::voxel::BlockModel& model);
    
    std::optional<shared::voxel::BlockModel> parse_model_json(const std::string& json, const std::string& id);
    
    std::unordered_map<BlockType, shared::voxel::BlockModel> type_models_;
    
    std::unordered_map<std::string, shared::voxel::BlockModel> id_models_;
    
    std::string models_path_;
    bool initialized_{false};
};

} // namespace voxel
