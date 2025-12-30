#include "block_model_loader.hpp"
#include "../core/resources.hpp"
#include "vfs/vfs.hpp"
#include <cstdio>
#include <raylib.h>

namespace voxel {

using namespace shared::voxel;

BlockModelLoader& BlockModelLoader::instance() {
    static BlockModelLoader inst;
    return inst;
}

bool BlockModelLoader::init(const std::string& models_path) {
    if (initialized_) return true;
    
    models_path_ = models_path;
    
    register_builtin_models();
    
    // Use VFS to list and load JSON model files
    if (shared::vfs::exists(models_path)) {
        auto files = shared::vfs::list_dir(models_path);
        for (const auto& file : files) {
            // Skip directories (they end with '/')
            if (!file.empty() && file.back() == '/') continue;
            
            // Check for .json extension
            if (file.size() > 5 && file.substr(file.size() - 5) == ".json") {
                std::string path = models_path + "/" + file;
                auto model = load_model_file(path);
                if (model) {
                    id_models_[model->id] = std::move(*model);
                    TraceLog(LOG_DEBUG, "[BlockModelLoader] Loaded model: %s", model->id.c_str());
                }
            }
        }
    }
    
    initialized_ = true;
    TraceLog(LOG_INFO, "[BlockModelLoader] Initialized with %zu type models, %zu id models", 
             type_models_.size(), id_models_.size());
    
    return true;
}

void BlockModelLoader::destroy() {
    if (initialized_) {
        type_models_.clear();
        id_models_.clear();
        initialized_ = false;
    }
}

const BlockModel* BlockModelLoader::get_model(BlockType type) const {
    auto it = type_models_.find(type);
    return (it != type_models_.end()) ? &it->second : nullptr;
}

const BlockModel* BlockModelLoader::get_model_by_id(const std::string& id) const {
    auto it = id_models_.find(id);
    return (it != id_models_.end()) ? &it->second : nullptr;
}

void BlockModelLoader::register_model(BlockType type, BlockModel model) {
    if (!model.id.empty()) {
        id_models_[model.id] = model;
    }
    type_models_[type] = std::move(model);
}

void BlockModelLoader::register_builtin_models() {
    {
        auto model = models::make_full_block();
        model.id = "full_block";
        register_model(BlockType::Stone, model);
        register_model(BlockType::Dirt, model);
        register_model(BlockType::Grass, model);
        register_model(BlockType::Sand, model);
        register_model(BlockType::Wood, model);
        register_model(BlockType::Leaves, model);
        register_model(BlockType::Bedrock, model);
        register_model(BlockType::Gravel, model);
        register_model(BlockType::Coal, model);
        register_model(BlockType::Iron, model);
        register_model(BlockType::Gold, model);
        register_model(BlockType::Diamond, model);
        id_models_["full_block"] = model;
    }
    
    {
        BlockModel model;
        model.id = "air";
        model.shape = BlockShape::Empty;
        register_model(BlockType::Air, model);
    }
    
    {
        auto model = models::make_full_block();
        model.id = "water";
        model.shape = BlockShape::Empty;
        register_model(BlockType::Water, model);
    }
    
    {
        BlockModel model;
        model.id = "light";
        model.shape = BlockShape::Empty;
        register_model(BlockType::Light, model);
    }
    
    {
        auto model = models::make_bottom_slab();
        model.id = "stone_slab";
        model.textures["all"] = "blocks/stone";
        register_model(BlockType::StoneSlab, model);
    }
    
    {
        auto model = models::make_top_slab();
        model.id = "stone_slab_top";
        model.textures["all"] = "blocks/stone";
        register_model(BlockType::StoneSlabTop, model);
    }
    
    {
        auto model = models::make_bottom_slab();
        model.id = "wood_slab";
        model.textures["all"] = "blocks/planks_oak";
        register_model(BlockType::WoodSlab, model);
    }
    
    {
        auto model = models::make_top_slab();
        model.id = "wood_slab_top";
        model.textures["all"] = "blocks/planks_oak";
        register_model(BlockType::WoodSlabTop, model);
    }
    
    {
        auto model = models::make_fence_post();
        model.id = "oak_fence";
        model.textures["post"] = "blocks/planks_oak";
        register_model(BlockType::OakFence, model);
    }
    
    // Vegetation (cross-shaped models)
    {
        auto model = models::make_cross();
        model.id = "tall_grass";
        model.textures["cross"] = "blocks/tallgrass";
        register_model(BlockType::TallGrass, model);
    }
    
    {
        auto model = models::make_cross();
        model.id = "poppy";
        model.textures["cross"] = "blocks/flower_rose";
        register_model(BlockType::Poppy, model);
    }
    
    {
        auto model = models::make_cross();
        model.id = "dandelion";
        model.textures["cross"] = "blocks/flower_dandelion";
        register_model(BlockType::Dandelion, model);
    }
    
    {
        auto model = models::make_cross();
        model.id = "blue_orchid";
        model.textures["cross"] = "blocks/flower_blue_orchid";
        register_model(BlockType::DeadBush, model);
    }
}

namespace json_parser {

enum class TokenType {
    OpenBrace,
    CloseBrace,
    OpenBracket,
    CloseBracket,
    Colon,
    Comma,
    String,
    Number,
    True,
    False,
    Null,
    End,
    Error
};

struct Token {
    TokenType type{TokenType::Error};
    std::string value;
    double number{0.0};
};

class Lexer {
public:
    explicit Lexer(const std::string& input) : input_(input), pos_(0) {}
    
    Token next() {
        skip_whitespace();
        
        if (pos_ >= input_.size()) {
            return {TokenType::End, "", 0.0};
        }
        
        char c = input_[pos_];
        
        if (c == '{') { pos_++; return {TokenType::OpenBrace, "{", 0.0}; }
        if (c == '}') { pos_++; return {TokenType::CloseBrace, "}", 0.0}; }
        if (c == '[') { pos_++; return {TokenType::OpenBracket, "[", 0.0}; }
        if (c == ']') { pos_++; return {TokenType::CloseBracket, "]", 0.0}; }
        if (c == ':') { pos_++; return {TokenType::Colon, ":", 0.0}; }
        if (c == ',') { pos_++; return {TokenType::Comma, ",", 0.0}; }
        
        if (c == '"') {
            return parse_string();
        }
        
        if (c == '-' || std::isdigit(c)) {
            return parse_number();
        }
        
        if (std::isalpha(c)) {
            return parse_keyword();
        }
        
        return {TokenType::Error, std::string(1, c), 0.0};
    }
    
private:
    void skip_whitespace() {
        while (pos_ < input_.size() && std::isspace(input_[pos_])) {
            pos_++;
        }
    }
    
    Token parse_string() {
        pos_++;
        std::string result;
        
        while (pos_ < input_.size() && input_[pos_] != '"') {
            if (input_[pos_] == '\\' && pos_ + 1 < input_.size()) {
                pos_++;
                switch (input_[pos_]) {
                    case 'n': result += '\n'; break;
                    case 't': result += '\t'; break;
                    case 'r': result += '\r'; break;
                    case '"': result += '"'; break;
                    case '\\': result += '\\'; break;
                    default: result += input_[pos_]; break;
                }
            } else {
                result += input_[pos_];
            }
            pos_++;
        }
        
        if (pos_ < input_.size()) pos_++;
        return {TokenType::String, result, 0.0};
    }
    
    Token parse_number() {
        size_t start = pos_;
        if (input_[pos_] == '-') pos_++;
        
        while (pos_ < input_.size() && (std::isdigit(input_[pos_]) || input_[pos_] == '.' || input_[pos_] == 'e' || input_[pos_] == 'E' || input_[pos_] == '+' || input_[pos_] == '-')) {
            pos_++;
        }
        
        std::string num_str = input_.substr(start, pos_ - start);
        double num = std::stod(num_str);
        return {TokenType::Number, num_str, num};
    }
    
    Token parse_keyword() {
        size_t start = pos_;
        while (pos_ < input_.size() && std::isalpha(input_[pos_])) {
            pos_++;
        }
        
        std::string kw = input_.substr(start, pos_ - start);
        if (kw == "true") return {TokenType::True, kw, 1.0};
        if (kw == "false") return {TokenType::False, kw, 0.0};
        if (kw == "null") return {TokenType::Null, kw, 0.0};
        return {TokenType::Error, kw, 0.0};
    }
    
    const std::string& input_;
    size_t pos_;
};

struct JsonValue;
using JsonObject = std::unordered_map<std::string, JsonValue>;
using JsonArray = std::vector<JsonValue>;

struct JsonValue {
    enum class Type { Null, Bool, Number, String, Array, Object };
    Type type{Type::Null};
    bool bool_val{false};
    double number_val{0.0};
    std::string string_val;
    std::shared_ptr<JsonArray> array_val;
    std::shared_ptr<JsonObject> object_val;
    
    bool is_null() const { return type == Type::Null; }
    bool is_bool() const { return type == Type::Bool; }
    bool is_number() const { return type == Type::Number; }
    bool is_string() const { return type == Type::String; }
    bool is_array() const { return type == Type::Array; }
    bool is_object() const { return type == Type::Object; }
    
    bool as_bool(bool def = false) const { return is_bool() ? bool_val : def; }
    double as_number(double def = 0.0) const { return is_number() ? number_val : def; }
    float as_float(float def = 0.0f) const { return static_cast<float>(as_number(def)); }
    int as_int(int def = 0) const { return static_cast<int>(as_number(def)); }
    const std::string& as_string(const std::string& def = "") const { return is_string() ? string_val : def; }
    
    const JsonArray& as_array() const {
        static JsonArray empty;
        return is_array() && array_val ? *array_val : empty;
    }
    
    const JsonObject& as_object() const {
        static JsonObject empty;
        return is_object() && object_val ? *object_val : empty;
    }
    
    const JsonValue& operator[](const std::string& key) const {
        static JsonValue null_val;
        if (!is_object() || !object_val) return null_val;
        auto it = object_val->find(key);
        return it != object_val->end() ? it->second : null_val;
    }
    
    const JsonValue& operator[](size_t index) const {
        static JsonValue null_val;
        if (!is_array() || !array_val || index >= array_val->size()) return null_val;
        return (*array_val)[index];
    }
    
    size_t size() const {
        if (is_array() && array_val) return array_val->size();
        if (is_object() && object_val) return object_val->size();
        return 0;
    }
    
    bool has(const std::string& key) const {
        return is_object() && object_val && object_val->find(key) != object_val->end();
    }
};

class Parser {
public:
    explicit Parser(const std::string& input) : lexer_(input) {
        current_ = lexer_.next();
    }
    
    JsonValue parse() {
        return parse_value();
    }
    
private:
    Token current_;
    Lexer lexer_;
    
    void advance() {
        current_ = lexer_.next();
    }
    
    bool expect(TokenType type) {
        if (current_.type == type) {
            advance();
            return true;
        }
        return false;
    }
    
    JsonValue parse_value() {
        JsonValue val;
        
        switch (current_.type) {
            case TokenType::Null:
                val.type = JsonValue::Type::Null;
                advance();
                break;
            case TokenType::True:
                val.type = JsonValue::Type::Bool;
                val.bool_val = true;
                advance();
                break;
            case TokenType::False:
                val.type = JsonValue::Type::Bool;
                val.bool_val = false;
                advance();
                break;
            case TokenType::Number:
                val.type = JsonValue::Type::Number;
                val.number_val = current_.number;
                advance();
                break;
            case TokenType::String:
                val.type = JsonValue::Type::String;
                val.string_val = current_.value;
                advance();
                break;
            case TokenType::OpenBracket:
                return parse_array();
            case TokenType::OpenBrace:
                return parse_object();
            default:
                break;
        }
        
        return val;
    }
    
    JsonValue parse_array() {
        JsonValue val;
        val.type = JsonValue::Type::Array;
        val.array_val = std::make_shared<JsonArray>();
        
        advance(); // Skip '['
        
        if (current_.type == TokenType::CloseBracket) {
            advance();
            return val;
        }
        
        while (true) {
            val.array_val->push_back(parse_value());
            
            if (current_.type == TokenType::Comma) {
                advance();
            } else {
                break;
            }
        }
        
        expect(TokenType::CloseBracket);
        return val;
    }
    
    JsonValue parse_object() {
        JsonValue val;
        val.type = JsonValue::Type::Object;
        val.object_val = std::make_shared<JsonObject>();
        
        advance(); // Skip '{'
        
        if (current_.type == TokenType::CloseBrace) {
            advance();
            return val;
        }
        
        while (true) {
            if (current_.type != TokenType::String) break;
            
            std::string key = current_.value;
            advance();
            
            if (!expect(TokenType::Colon)) break;
            
            (*val.object_val)[key] = parse_value();
            
            if (current_.type == TokenType::Comma) {
                advance();
            } else {
                break;
            }
        }
        
        expect(TokenType::CloseBrace);
        return val;
    }
};

JsonValue parse(const std::string& json) {
    Parser parser(json);
    return parser.parse();
}

} // namespace json_parser

std::optional<BlockModel> BlockModelLoader::load_model_file(const std::string& path) {
    auto json_opt = shared::vfs::read_text_file(path);
    if (!json_opt) {
        TraceLog(LOG_WARNING, "[BlockModelLoader] Failed to open: %s", path.c_str());
        return std::nullopt;
    }
    
    // Extract model ID from path (filename without .json extension)
    std::string id;
    auto slash_pos = path.rfind('/');
    std::string filename = (slash_pos != std::string::npos) ? path.substr(slash_pos + 1) : path;
    if (filename.size() > 5 && filename.substr(filename.size() - 5) == ".json") {
        id = filename.substr(0, filename.size() - 5);
    } else {
        id = filename;
    }
    
    return parse_model_json(*json_opt, id);
}

std::optional<BlockModel> BlockModelLoader::parse_model_json(const std::string& json, const std::string& id) {
    auto root = json_parser::parse(json);
    
    if (!root.is_object()) {
        TraceLog(LOG_WARNING, "[BlockModelLoader] Invalid JSON for model: %s", id.c_str());
        return std::nullopt;
    }
    
    BlockModel model;
    model.id = id;
    
    if (root.has("parent")) {
        model.parent = root["parent"].as_string();
    }
    
    if (root.has("textures") && root["textures"].is_object()) {
        for (const auto& [key, val] : root["textures"].as_object()) {
            if (val.is_string()) {
                model.textures[key] = val.as_string();
            }
        }
    }
    
    if (root.has("ambientocclusion")) {
        model.ambientOcclusion = root["ambientocclusion"].as_bool(true);
    }
    
    if (root.has("elements") && root["elements"].is_array()) {
        for (const auto& elem_val : root["elements"].as_array()) {
            if (!elem_val.is_object()) continue;
            
            ModelElement elem;
            
            if (elem_val.has("from") && elem_val["from"].is_array() && elem_val["from"].size() >= 3) {
                elem.from[0] = elem_val["from"][0].as_float();
                elem.from[1] = elem_val["from"][1].as_float();
                elem.from[2] = elem_val["from"][2].as_float();
            }
            
            if (elem_val.has("to") && elem_val["to"].is_array() && elem_val["to"].size() >= 3) {
                elem.to[0] = elem_val["to"][0].as_float();
                elem.to[1] = elem_val["to"][1].as_float();
                elem.to[2] = elem_val["to"][2].as_float();
            }
            
            if (elem_val.has("rotation") && elem_val["rotation"].is_object()) {
                const auto& rot = elem_val["rotation"];
                if (rot.has("origin") && rot["origin"].is_array() && rot["origin"].size() >= 3) {
                    elem.rotationOrigin[0] = rot["origin"][0].as_float();
                    elem.rotationOrigin[1] = rot["origin"][1].as_float();
                    elem.rotationOrigin[2] = rot["origin"][2].as_float();
                }
                if (rot.has("axis") && rot["axis"].is_string() && !rot["axis"].as_string().empty()) {
                    elem.rotationAxis = rot["axis"].as_string()[0];
                }
                elem.rotationAngle = rot["angle"].as_float();
                elem.rotationRescale = rot["rescale"].as_bool(false);
            }
            
            if (elem_val.has("faces") && elem_val["faces"].is_object()) {
                static const std::unordered_map<std::string, Face> face_map = {
                    {"east", Face::East}, {"west", Face::West},
                    {"up", Face::Up}, {"down", Face::Down},
                    {"south", Face::South}, {"north", Face::North}
                };
                
                for (const auto& [face_name, face_val] : elem_val["faces"].as_object()) {
                    auto it = face_map.find(face_name);
                    if (it == face_map.end() || !face_val.is_object()) continue;
                    
                    int face_idx = static_cast<int>(it->second);
                    elem.faceEnabled[face_idx] = true;
                    
                    ModelElement::FaceData& fd = elem.faces[face_idx];
                    
                    if (face_val.has("texture")) {
                        fd.texture = face_val["texture"].as_string();
                    }
                    
                    if (face_val.has("uv") && face_val["uv"].is_array() && face_val["uv"].size() >= 4) {
                        fd.uv[0] = face_val["uv"][0].as_float();
                        fd.uv[1] = face_val["uv"][1].as_float();
                        fd.uv[2] = face_val["uv"][2].as_float();
                        fd.uv[3] = face_val["uv"][3].as_float();
                    }
                    
                    fd.rotation = face_val["rotation"].as_int(0);
                    fd.tintIndex = face_val["tintindex"].as_int(-1);
                    
                    if (face_val.has("cullface")) {
                        fd.cullface = true;
                    }
                }
            }
            
            model.elements.push_back(elem);
        }
    }
    
    if (root.has("collision") && root["collision"].is_array()) {
        model.collisionBoxes.clear();
        for (const auto& box_val : root["collision"].as_array()) {
            if (box_val.is_array() && box_val.size() >= 6) {
                AABB box;
                box.minX = box_val[0].as_float() / 16.0f;
                box.minY = box_val[1].as_float() / 16.0f;
                box.minZ = box_val[2].as_float() / 16.0f;
                box.maxX = box_val[3].as_float() / 16.0f;
                box.maxY = box_val[4].as_float() / 16.0f;
                box.maxZ = box_val[5].as_float() / 16.0f;
                model.collisionBoxes.push_back(box);
            }
        }
    }
    
    if (model.collisionBoxes.empty() && !model.elements.empty()) {
        for (const auto& elem : model.elements) {
            model.collisionBoxes.push_back(elem.to_aabb());
        }
    }
    
    if (model.elements.empty()) {
        model.shape = BlockShape::Empty;
    } else if (model.elements.size() == 1) {
        const auto& elem = model.elements[0];
        float minY = elem.from[1] / 16.0f;
        float maxY = elem.to[1] / 16.0f;
        float width = (elem.to[0] - elem.from[0]) / 16.0f;
        float depth = (elem.to[2] - elem.from[2]) / 16.0f;
        
        if (width >= 0.99f && depth >= 0.99f) {
            if (maxY >= 0.99f && minY <= 0.01f) {
                model.shape = BlockShape::Full;
            } else if (minY <= 0.01f && maxY <= 0.51f) {
                model.shape = BlockShape::BottomSlab;
            } else if (minY >= 0.49f && maxY >= 0.99f) {
                model.shape = BlockShape::TopSlab;
            } else {
                model.shape = BlockShape::Custom;
            }
        } else if (width < 0.5f && depth < 0.5f) {
            model.shape = BlockShape::Fence;
        } else {
            model.shape = BlockShape::Custom;
        }
    } else {
        model.shape = BlockShape::Custom;
    }
    
    resolve_parent(model);
    
    return model;
}

void BlockModelLoader::resolve_parent(BlockModel& model) {
    if (model.parent.empty()) return;
    
    std::string parent_id = model.parent;
    if (parent_id.find("minecraft:") == 0) {
        parent_id = parent_id.substr(10);
    }
    if (parent_id.find("block/") == 0) {
        parent_id = parent_id.substr(6);
    }
    
    auto it = id_models_.find(parent_id);
    if (it == id_models_.end()) {
        std::string parent_path = models_path_ + "/" + parent_id + ".json";
        auto parent_model = load_model_file(parent_path);
        if (parent_model) {
            id_models_[parent_id] = std::move(*parent_model);
            it = id_models_.find(parent_id);
        }
    }
    
    if (it != id_models_.end()) {
        const BlockModel& parent = it->second;
        
        for (const auto& [key, value] : parent.textures) {
            if (model.textures.find(key) == model.textures.end()) {
                model.textures[key] = value;
            }
        }
        
        if (model.elements.empty()) {
            model.elements = parent.elements;
        }
        
        if (model.collisionBoxes.empty()) {
            model.collisionBoxes = parent.collisionBoxes;
        }
        
        if (model.shape == BlockShape::Full && parent.shape != BlockShape::Full) {
            model.shape = parent.shape;
        }
        
        model.ambientOcclusion = parent.ambientOcclusion;
    }
}

} // namespace voxel
