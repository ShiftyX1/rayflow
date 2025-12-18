/**
 * @file map_editor_style.hpp
 * @brief Custom raygui styling and UI helpers for the Map Editor.
 *
 * Provides a modern dark-theme UI with improved UX for the Rayflow Map Editor.
 * Uses raygui styling system with custom colors, fonts, and helper drawing functions.
 */

#ifndef MAP_EDITOR_STYLE_HPP
#define MAP_EDITOR_STYLE_HPP

#include "raylib.h"

namespace editor_ui {

// ============================================================================
// Color palette - Modern dark theme with blue accents
// ============================================================================

// Background colors
constexpr Color kBgDark         = {18, 18, 24, 255};       // Main background
constexpr Color kBgPanel        = {28, 28, 38, 255};       // Panel background
constexpr Color kBgPanelLight   = {38, 38, 52, 255};       // Lighter panel elements
constexpr Color kBgHover        = {48, 48, 65, 255};       // Hover state
constexpr Color kBgPressed      = {22, 22, 32, 255};       // Pressed state

// Accent colors - Blue theme
constexpr Color kAccentPrimary  = {86, 140, 245, 255};     // Primary accent (buttons, active)
constexpr Color kAccentHover    = {106, 160, 255, 255};    // Hover accent
constexpr Color kAccentPressed  = {66, 120, 220, 255};     // Pressed accent
constexpr Color kAccentMuted    = {60, 80, 140, 255};      // Muted accent for borders

// Text colors
constexpr Color kTextPrimary    = {235, 235, 245, 255};    // Primary text
constexpr Color kTextSecondary  = {160, 165, 180, 255};    // Secondary/label text
constexpr Color kTextMuted      = {100, 105, 120, 255};    // Muted/disabled text
constexpr Color kTextOnAccent   = {255, 255, 255, 255};    // Text on accent bg

// Semantic colors
constexpr Color kSuccess        = {80, 200, 120, 255};
constexpr Color kWarning        = {240, 180, 60, 255};
constexpr Color kError          = {240, 80, 80, 255};

// Border/separator colors
constexpr Color kBorderNormal   = {55, 58, 75, 255};
constexpr Color kBorderFocused  = {86, 140, 245, 255};
constexpr Color kSeparator      = {45, 48, 60, 255};

// ============================================================================
// Layout constants
// ============================================================================

constexpr int kPanelPadding     = 16;
constexpr int kSectionGap       = 12;
constexpr int kControlHeight    = 28;
constexpr int kControlGap       = 6;
constexpr int kLabelGap         = 4;
constexpr int kBorderRadius     = 6;
constexpr int kBorderWidth      = 1;

constexpr int kButtonHeight     = 36;
constexpr int kButtonHeightSmall = 28;
constexpr int kInputHeight      = 32;

constexpr int kModalWidth       = 480;
constexpr int kModalWidthLarge  = 560;

// ============================================================================
// Font management
// ============================================================================

/// Loaded custom fonts for the editor UI.
struct EditorFonts {
    Font regular;       // Regular weight (16pt default)
    Font semiBold;      // Semi-bold for headings
    Font bold;          // Bold for titles
    bool loaded;
};

/// Initialize custom fonts. Call once after InitWindow().
void InitEditorFonts();

/// Shutdown and unload fonts. Call before CloseWindow().
void ShutdownEditorFonts();

/// Get the loaded editor fonts.
const EditorFonts& GetFonts();

// ============================================================================
// Style setup
// ============================================================================

/// Apply the dark theme style to raygui. Call once after InitWindow().
void ApplyEditorStyle();

/// Reset raygui to default style.
void ResetToDefaultStyle();

// ============================================================================
// Drawing helpers
// ============================================================================

/// Draw a styled panel with rounded corners.
void DrawStyledPanel(Rectangle bounds, const char* title = nullptr);

/// Draw a section header with optional icon.
void DrawSectionHeader(Rectangle bounds, const char* text, int iconId = -1);

/// Draw a horizontal separator line.
void DrawSeparator(float x, float y, float width);

/// Draw a modal background overlay.
void DrawModalOverlay(int screenWidth, int screenHeight);

/// Draw a styled modal window.
void DrawModalWindow(Rectangle bounds, const char* title);

/// Draw stylized label (secondary color).
void DrawStyledLabel(Rectangle bounds, const char* text, bool secondary = false);

/// Draw a tooltip near the given rectangle.
void DrawTooltipBox(Rectangle controlBounds, const char* text);

// ============================================================================
// Custom controls
// ============================================================================

/// Styled button with icon support. Returns true when clicked.
bool StyledButton(Rectangle bounds, const char* text, int iconId = -1, bool primary = true);

/// Styled text input field with label. Returns true when editing.
bool StyledTextBox(Rectangle bounds, const char* label, char* text, int textSize, bool* editMode);

/// Styled integer value box with label. Returns true when editing.
bool StyledValueBox(Rectangle bounds, const char* label, int* value, int minValue, int maxValue, bool* editMode);

/// Styled slider with label. Returns true when value changed.
bool StyledSlider(Rectangle bounds, const char* label, float* value, float minValue, float maxValue, const char* format = "%.2f");

/// Styled checkbox with label. Returns true when toggled.
bool StyledCheckBox(Rectangle bounds, const char* label, bool* checked);

/// Styled dropdown box. Returns true when selection changed.
bool StyledDropdownBox(Rectangle bounds, const char* label, const char* items, int* active, bool* editMode);

/// Styled list view. Returns true when selection changed.
bool StyledListView(Rectangle bounds, const char* items, int* scrollIndex, int* active);

// ============================================================================
// Layout helpers
// ============================================================================

/// Layout helper for vertical stacking of controls.
struct VerticalLayout {
    float x;
    float y;
    float width;
    float currentY;
    float gap;

    VerticalLayout(float x, float y, float width, float gap = kControlGap)
        : x(x), y(y), width(width), currentY(y), gap(gap) {}

    /// Get next rectangle with given height and advance.
    Rectangle NextRow(float height);

    /// Advance by additional spacing.
    void AddSpace(float space);

    /// Get current Y position.
    float GetY() const { return currentY; }
};

// ============================================================================
// Block palette drawing
// ============================================================================

/// Draw a block preview image.
void DrawBlockPreview(Rectangle bounds, int blockType);

} // namespace editor_ui

#endif // MAP_EDITOR_STYLE_HPP
