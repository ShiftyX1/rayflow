#include "config.hpp"

#include <cctype>
#include <cstdio>
#include <fstream>
#include <sstream>

#include <raylib.h>

namespace core {

std::string key_name(int key) {
    // Letters
    if (key >= KEY_A && key <= KEY_Z) {
        char c = static_cast<char>('A' + (key - KEY_A));
        return std::string(1, c);
    }

    // Digits
    if (key >= KEY_ZERO && key <= KEY_NINE) {
        char c = static_cast<char>('0' + (key - KEY_ZERO));
        return std::string(1, c);
    }

    switch (key) {
        case KEY_SPACE: return "SPACE";
        case KEY_ESCAPE: return "ESC";
        case KEY_LEFT_SHIFT: return "L-SHIFT";
        case KEY_LEFT_CONTROL: return "L-CTRL";
        case KEY_LEFT_ALT: return "L-ALT";
        case KEY_RIGHT_SHIFT: return "R-SHIFT";
        case KEY_RIGHT_CONTROL: return "R-CTRL";
        case KEY_RIGHT_ALT: return "R-ALT";
        case KEY_ENTER: return "ENTER";
        case KEY_TAB: return "TAB";
        case KEY_BACKSPACE: return "BACKSPACE";
        case KEY_UP: return "UP";
        case KEY_DOWN: return "DOWN";
        case KEY_LEFT: return "LEFT";
        case KEY_RIGHT: return "RIGHT";
        default: break;
    }

    // Fallback to numeric.
    return std::to_string(key);
}

std::string mouse_button_name(int button) {
    switch (button) {
        case MOUSE_LEFT_BUTTON: return "MOUSE_LEFT";
        case MOUSE_RIGHT_BUTTON: return "MOUSE_RIGHT";
        case MOUSE_MIDDLE_BUTTON: return "MOUSE_MIDDLE";
        default: break;
    }

    return std::to_string(button);
}

Config& Config::instance() {
    static Config inst;
    return inst;
}

Config::Config() {
    // Defaults (match current hardcoded controls)
    config_.controls.move_forward = KEY_W;
    config_.controls.move_backward = KEY_S;
    config_.controls.move_left = KEY_A;
    config_.controls.move_right = KEY_D;

    config_.controls.jump = KEY_SPACE;
    config_.controls.sprint = KEY_LEFT_CONTROL;
    config_.controls.fly_down = KEY_LEFT_SHIFT;

    config_.controls.toggle_creative = KEY_C;
    config_.controls.exit = KEY_ESCAPE;

    config_.controls.primary_mouse = MOUSE_LEFT_BUTTON;
    config_.controls.secondary_mouse = MOUSE_RIGHT_BUTTON;

    config_.controls.tool_1 = KEY_ONE;
    config_.controls.tool_2 = KEY_TWO;
    config_.controls.tool_3 = KEY_THREE;
    config_.controls.tool_4 = KEY_FOUR;
    config_.controls.tool_5 = KEY_FIVE;

    // Logging defaults
    config_.logging.enabled = true;
    config_.logging.level = LOG_INFO;
    config_.logging.file = "";
    config_.logging.collision_debug = false;
}

std::string Config::trim(std::string s) {
    auto is_space = [](unsigned char c) { return std::isspace(c) != 0; };

    while (!s.empty() && is_space(static_cast<unsigned char>(s.front()))) s.erase(s.begin());
    while (!s.empty() && is_space(static_cast<unsigned char>(s.back()))) s.pop_back();

    return s;
}

std::string Config::to_lower(std::string s) {
    for (char& c : s) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return s;
}

bool Config::parse_bool(const std::string& v, bool default_value) {
    std::string s = to_lower(trim(v));
    if (s == "1" || s == "true" || s == "yes" || s == "on") return true;
    if (s == "0" || s == "false" || s == "no" || s == "off") return false;
    return default_value;
}

int Config::parse_int(const std::string& v, int default_value) {
    try {
        size_t idx = 0;
        int out = std::stoi(trim(v), &idx, 10);
        (void)idx;
        return out;
    } catch (...) {
        return default_value;
    }
}

static float parse_float_local(const std::string& v, float default_value) {
    try {
        auto is_space = [](unsigned char c) { return std::isspace(c) != 0; };
        std::string s = v;
        while (!s.empty() && is_space(static_cast<unsigned char>(s.front()))) s.erase(s.begin());
        while (!s.empty() && is_space(static_cast<unsigned char>(s.back()))) s.pop_back();

        size_t idx = 0;
        float out = std::stof(s, &idx);
        (void)idx;
        return out;
    } catch (...) {
        return default_value;
    }
}

static std::string strip_quotes(std::string s) {
    auto is_space = [](unsigned char c) { return std::isspace(c) != 0; };
    while (!s.empty() && is_space(static_cast<unsigned char>(s.front()))) s.erase(s.begin());
    while (!s.empty() && is_space(static_cast<unsigned char>(s.back()))) s.pop_back();

    if (s.size() >= 2) {
        const char a = s.front();
        const char b = s.back();
        if ((a == '"' && b == '"') || (a == '\'' && b == '\'')) {
            return s.substr(1, s.size() - 2);
        }
    }
    return s;
}

int Config::key_from_string(const std::string& v, int default_value) {
    std::string s = to_lower(strip_quotes(v));

    // Allow raw ASCII letters/digits like "w".
    if (s.size() == 1) {
        const char c = s[0];
        if (c >= 'a' && c <= 'z') {
            // raylib key codes for letters are KEY_A..KEY_Z.
            return KEY_A + (c - 'a');
        }
        if (c >= '0' && c <= '9') {
            // Prefer explicit names for digits; but support '1'..'9'.
            if (c == '0') return KEY_ZERO;
            return KEY_ONE + (c - '1');
        }
    }

    if (s.rfind("key_", 0) == 0) s = s.substr(4);

    static const std::unordered_map<std::string, int> map = {
        {"w", KEY_W}, {"a", KEY_A}, {"s", KEY_S}, {"d", KEY_D},
        {"space", KEY_SPACE},
        {"lctrl", KEY_LEFT_CONTROL}, {"left_control", KEY_LEFT_CONTROL}, {"leftctrl", KEY_LEFT_CONTROL},
        {"lshift", KEY_LEFT_SHIFT}, {"left_shift", KEY_LEFT_SHIFT}, {"leftshift", KEY_LEFT_SHIFT},
        {"escape", KEY_ESCAPE}, {"esc", KEY_ESCAPE},
        {"c", KEY_C},
        {"1", KEY_ONE}, {"one", KEY_ONE},
        {"2", KEY_TWO}, {"two", KEY_TWO},
        {"3", KEY_THREE}, {"three", KEY_THREE},
        {"4", KEY_FOUR}, {"four", KEY_FOUR},
        {"5", KEY_FIVE}, {"five", KEY_FIVE},
        {"0", KEY_ZERO}, {"zero", KEY_ZERO},
    };

    auto it = map.find(s);
    if (it != map.end()) return it->second;

    return default_value;
}

int Config::mouse_from_string(const std::string& v, int default_value) {
    std::string s = to_lower(strip_quotes(v));
    if (s.rfind("mouse_", 0) == 0) s = s.substr(6);

    static const std::unordered_map<std::string, int> map = {
        {"left", MOUSE_LEFT_BUTTON},
        {"right", MOUSE_RIGHT_BUTTON},
        {"middle", MOUSE_MIDDLE_BUTTON},
    };

    auto it = map.find(s);
    if (it != map.end()) return it->second;

    return default_value;
}

int Config::log_level_from_string(const std::string& v, int default_value) {
    std::string s = to_lower(strip_quotes(v));

    static const std::unordered_map<std::string, int> map = {
        {"all", LOG_ALL},
        {"trace", LOG_TRACE},
        {"debug", LOG_DEBUG},
        {"info", LOG_INFO},
        {"warning", LOG_WARNING}, {"warn", LOG_WARNING},
        {"error", LOG_ERROR},
        {"fatal", LOG_FATAL},
        {"none", LOG_NONE}, {"off", LOG_NONE},
    };

    auto it = map.find(s);
    if (it != map.end()) return it->second;

    // Allow numeric.
    return parse_int(s, default_value);
}

void Config::apply_kv(const std::string& section, const std::string& key, const std::string& value) {
    const std::string sec = to_lower(trim(section));
    const std::string k = to_lower(trim(key));
    const std::string v = strip_quotes(value);

    if (sec == "controls") {
        if (k == "forward") config_.controls.move_forward = key_from_string(v, config_.controls.move_forward);
        else if (k == "backward") config_.controls.move_backward = key_from_string(v, config_.controls.move_backward);
        else if (k == "left") config_.controls.move_left = key_from_string(v, config_.controls.move_left);
        else if (k == "right") config_.controls.move_right = key_from_string(v, config_.controls.move_right);
        else if (k == "jump") config_.controls.jump = key_from_string(v, config_.controls.jump);
        else if (k == "sprint") config_.controls.sprint = key_from_string(v, config_.controls.sprint);
        else if (k == "fly_down") config_.controls.fly_down = key_from_string(v, config_.controls.fly_down);
        else if (k == "toggle_creative") config_.controls.toggle_creative = key_from_string(v, config_.controls.toggle_creative);
        else if (k == "exit") config_.controls.exit = key_from_string(v, config_.controls.exit);
        else if (k == "primary_mouse") config_.controls.primary_mouse = mouse_from_string(v, config_.controls.primary_mouse);
        else if (k == "secondary_mouse") config_.controls.secondary_mouse = mouse_from_string(v, config_.controls.secondary_mouse);
        else if (k == "tool_1") config_.controls.tool_1 = key_from_string(v, config_.controls.tool_1);
        else if (k == "tool_2") config_.controls.tool_2 = key_from_string(v, config_.controls.tool_2);
        else if (k == "tool_3") config_.controls.tool_3 = key_from_string(v, config_.controls.tool_3);
        else if (k == "tool_4") config_.controls.tool_4 = key_from_string(v, config_.controls.tool_4);
        else if (k == "tool_5") config_.controls.tool_5 = key_from_string(v, config_.controls.tool_5);
        return;
    }

    if (sec == "logging") {
        if (k == "enabled") config_.logging.enabled = parse_bool(v, config_.logging.enabled);
        else if (k == "level") config_.logging.level = log_level_from_string(v, config_.logging.level);
        else if (k == "file") config_.logging.file = strip_quotes(v);
        else if (k == "collision_debug") config_.logging.collision_debug = parse_bool(v, config_.logging.collision_debug);
        return;
    }

    if (sec == "debug") {
        if (k == "collision") config_.logging.collision_debug = parse_bool(v, config_.logging.collision_debug);
        return;
    }

    if (sec == "profiling") {
        if (k == "enabled") config_.profiling.enabled = parse_bool(v, config_.profiling.enabled);
        else if (k == "log_every_event") config_.profiling.log_every_event = parse_bool(v, config_.profiling.log_every_event);
        else if (k == "log_interval_ms") config_.profiling.log_interval_ms = parse_int(v, config_.profiling.log_interval_ms);

        else if (k == "light_volume") config_.profiling.light_volume = parse_bool(v, config_.profiling.light_volume);
        else if (k == "chunk_mesh") config_.profiling.chunk_mesh = parse_bool(v, config_.profiling.chunk_mesh);
        else if (k == "upload_mesh") config_.profiling.upload_mesh = parse_bool(v, config_.profiling.upload_mesh);

        else if (k == "warn_light_volume_ms") config_.profiling.warn_light_volume_ms = parse_float_local(v, config_.profiling.warn_light_volume_ms);
        else if (k == "warn_chunk_mesh_ms") config_.profiling.warn_chunk_mesh_ms = parse_float_local(v, config_.profiling.warn_chunk_mesh_ms);
        else if (k == "warn_upload_mesh_ms") config_.profiling.warn_upload_mesh_ms = parse_float_local(v, config_.profiling.warn_upload_mesh_ms);

        return;
    }

    if (sec == "sv_logging") {
        if (k == "enabled") config_.sv_logging.enabled = parse_bool(v, config_.sv_logging.enabled);
        else if (k == "init") config_.sv_logging.init = parse_bool(v, config_.sv_logging.init);
        else if (k == "rx") config_.sv_logging.rx = parse_bool(v, config_.sv_logging.rx);
        else if (k == "tx") config_.sv_logging.tx = parse_bool(v, config_.sv_logging.tx);
        else if (k == "move") config_.sv_logging.move = parse_bool(v, config_.sv_logging.move);
        else if (k == "coll") config_.sv_logging.coll = parse_bool(v, config_.sv_logging.coll);
        return;
    }
}

bool Config::load_from_file(const std::string& path) {
    std::ifstream in(path);
    if (!in.is_open()) {
        return false;
    }

    std::string section;
    std::string line;

    while (std::getline(in, line)) {
        // Strip comments (# or ;) - simplest approach: cut at first occurrence.
        auto hash = line.find('#');
        auto semi = line.find(';');
        size_t cut = std::string::npos;
        if (hash != std::string::npos) cut = hash;
        if (semi != std::string::npos) cut = (cut == std::string::npos) ? semi : std::min(cut, semi);
        if (cut != std::string::npos) line = line.substr(0, cut);

        line = trim(line);
        if (line.empty()) continue;

        if (line.front() == '[' && line.back() == ']') {
            section = trim(line.substr(1, line.size() - 2));
            continue;
        }

        auto eq = line.find('=');
        if (eq == std::string::npos) continue;

        std::string key = trim(line.substr(0, eq));
        std::string value = trim(line.substr(eq + 1));
        if (key.empty()) continue;

        apply_kv(section, key, value);
    }

    return true;
}

} // namespace core
