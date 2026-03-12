#pragma once

// =============================================================================
// key_codes.hpp — Input constants (GLFW-compatible values)
//
// Phase 0 migration: replaces raylib KEY_*, MOUSE_*, CAMERA_* constants.
// Values match GLFW key codes (which also matched raylib since raylib used GLFW).
// Phase 1 will introduce a proper InputFacade wrapping GLFW callbacks.
// =============================================================================

namespace rf {
namespace input {

// --- Keyboard keys (GLFW values) ---
constexpr int KEY_SPACE         = 32;
constexpr int KEY_APOSTROPHE    = 39;
constexpr int KEY_COMMA         = 44;
constexpr int KEY_MINUS         = 45;
constexpr int KEY_PERIOD        = 46;
constexpr int KEY_SLASH         = 47;

constexpr int KEY_ZERO  = 48;
constexpr int KEY_ONE   = 49;
constexpr int KEY_TWO   = 50;
constexpr int KEY_THREE = 51;
constexpr int KEY_FOUR  = 52;
constexpr int KEY_FIVE  = 53;
constexpr int KEY_SIX   = 54;
constexpr int KEY_SEVEN = 55;
constexpr int KEY_EIGHT = 56;
constexpr int KEY_NINE  = 57;

constexpr int KEY_A = 65;
constexpr int KEY_B = 66;
constexpr int KEY_C = 67;
constexpr int KEY_D = 68;
constexpr int KEY_E = 69;
constexpr int KEY_F = 70;
constexpr int KEY_G = 71;
constexpr int KEY_H = 72;
constexpr int KEY_I = 73;
constexpr int KEY_J = 74;
constexpr int KEY_K = 75;
constexpr int KEY_L = 76;
constexpr int KEY_M = 77;
constexpr int KEY_N = 78;
constexpr int KEY_O = 79;
constexpr int KEY_P = 80;
constexpr int KEY_Q = 81;
constexpr int KEY_R = 82;
constexpr int KEY_S = 83;
constexpr int KEY_T = 84;
constexpr int KEY_U = 85;
constexpr int KEY_V = 86;
constexpr int KEY_W = 87;
constexpr int KEY_X = 88;
constexpr int KEY_Y = 89;
constexpr int KEY_Z = 90;

constexpr int KEY_ESCAPE        = 256;
constexpr int KEY_ENTER         = 257;
constexpr int KEY_TAB           = 258;
constexpr int KEY_BACKSPACE     = 259;
constexpr int KEY_INSERT        = 260;
constexpr int KEY_DELETE        = 261;
constexpr int KEY_RIGHT         = 262;
constexpr int KEY_LEFT          = 263;
constexpr int KEY_DOWN          = 264;
constexpr int KEY_UP            = 265;

constexpr int KEY_F1  = 290;
constexpr int KEY_F2  = 291;
constexpr int KEY_F3  = 292;
constexpr int KEY_F4  = 293;
constexpr int KEY_F5  = 294;
constexpr int KEY_F6  = 295;
constexpr int KEY_F7  = 296;
constexpr int KEY_F8  = 297;
constexpr int KEY_F9  = 298;
constexpr int KEY_F10 = 299;
constexpr int KEY_F11 = 300;
constexpr int KEY_F12 = 301;

constexpr int KEY_LEFT_SHIFT    = 340;
constexpr int KEY_LEFT_CONTROL  = 341;
constexpr int KEY_LEFT_ALT      = 342;
constexpr int KEY_LEFT_SUPER    = 343;
constexpr int KEY_RIGHT_SHIFT   = 344;
constexpr int KEY_RIGHT_CONTROL = 345;
constexpr int KEY_RIGHT_ALT     = 346;
constexpr int KEY_RIGHT_SUPER   = 347;

// --- Mouse buttons ---
constexpr int MOUSE_LEFT_BUTTON   = 0;
constexpr int MOUSE_RIGHT_BUTTON  = 1;
constexpr int MOUSE_MIDDLE_BUTTON = 2;

} // namespace input
} // namespace rf

// ---------------------------------------------------------------------------
// Backward-compatible global aliases (used across the codebase).
// These will be removed once Phase 1 input facade is in place.
// ---------------------------------------------------------------------------
using namespace rf::input;
