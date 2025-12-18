#pragma once

#include <string>
#include <unordered_map>

namespace core {

struct ControlsConfig {
    // Movement
    int move_forward{0};
    int move_backward{0};
    int move_left{0};
    int move_right{0};

    // Actions
    int jump{0};
    int sprint{0};
    int fly_down{0};

    int toggle_creative{0};
    int exit{0};

    // Mouse
    int primary_mouse{0};
    int secondary_mouse{0};

    // Tool hotbar
    int tool_1{0};
    int tool_2{0};
    int tool_3{0};
    int tool_4{0};
    int tool_5{0};
};

struct LoggingConfig {
    bool enabled{true};
    int level{0};
    std::string file{};

    bool collision_debug{false};
};

struct ClientConfig {
    ControlsConfig controls{};
    LoggingConfig logging{};

    struct RenderConfig {
        // Voxel mesh vertex lighting:
        // - per-corner AO and smooth light sampling when true
        // - flat lighting per face when false (debug/diagnostic)
        bool voxel_smooth_lighting{true};

        // Render-only brightness curve for Minecraft-style combined light.
        // Prevents fully black pixels in deep shade while keeping discrete 0..15 light values intact.
        //
        // brightness = ambient_min + (1 - ambient_min) * pow(combined01, gamma)
        float voxel_light_ambient_min{0.08f};
        float voxel_light_gamma{1.0f};

        // Shadow expressiveness (render-only): how strongly AO darkens corners.
        // 0 = disable AO darkening, 1 = current AO behavior.
        float voxel_ao_strength{1.0f};
    } render{};

    // Server-side (sv) log filtering for the embedded authoritative server.
    // Note: still configured via the client config file because the app embeds both.
    struct ServerLoggingConfig {
        bool enabled{true};

        // Tag filters (match server::core::logf tag values)
        bool init{true};
        bool rx{true};
        bool tx{true};
        bool move{true};
        bool coll{true};
    } sv_logging{};

    struct ProfilingConfig {
        // Master switch
        bool enabled{false};

        // Logging behavior
        bool log_every_event{false};
        int log_interval_ms{250};

        // Per-feature toggles
        bool light_volume{true};
        bool chunk_mesh{true};
        bool upload_mesh{true};

        // Warn thresholds (ms)
        float warn_light_volume_ms{4.0f};
        float warn_chunk_mesh_ms{6.0f};
        float warn_upload_mesh_ms{2.0f};
    } profiling{};
};

class Config {
public:
    static Config& instance();

    // Loads and merges values from file. If the file doesn't exist, keeps defaults.
    bool load_from_file(const std::string& path);

    // Empty when no config file was found/read.
    const std::string& loaded_from_path() const { return loaded_from_path_; }

    const ClientConfig& get() const { return config_; }

    // Convenience for common lookups.
    const ControlsConfig& controls() const { return config_.controls; }
    const LoggingConfig& logging() const { return config_.logging; }
    const ClientConfig::ProfilingConfig& profiling() const { return config_.profiling; }
    const ClientConfig::ServerLoggingConfig& sv_logging() const { return config_.sv_logging; }

private:
    Config();

    ClientConfig config_{};

    std::string loaded_from_path_{};

    static std::string trim(std::string s);
    static std::string to_lower(std::string s);

    static bool parse_bool(const std::string& v, bool default_value);
    static int parse_int(const std::string& v, int default_value);

    static int key_from_string(const std::string& v, int default_value);
    static int mouse_from_string(const std::string& v, int default_value);
    static int log_level_from_string(const std::string& v, int default_value);

    void apply_kv(const std::string& section, const std::string& key, const std::string& value);
};

// Human-readable names for current bindings (useful for UI/help output).
std::string key_name(int key);
std::string mouse_button_name(int button);

} // namespace core
