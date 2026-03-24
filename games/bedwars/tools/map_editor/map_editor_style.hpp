#ifndef MAP_EDITOR_STYLE_HPP
#define MAP_EDITOR_STYLE_HPP

#include "raylib.h"

namespace editor_ui {

constexpr Color kBgDark         = {18, 18, 24, 255};
constexpr Color kBgPanel        = {28, 28, 38, 255};
constexpr Color kBgPanelLight   = {38, 38, 52, 255};
constexpr Color kBgHover        = {48, 48, 65, 255};
constexpr Color kBgPressed      = {22, 22, 32, 255};

constexpr Color kAccentPrimary  = {86, 140, 245, 255};
constexpr Color kAccentHover    = {106, 160, 255, 255};
constexpr Color kAccentPressed  = {66, 120, 220, 255};
constexpr Color kAccentMuted    = {60, 80, 140, 255};

constexpr Color kTextPrimary    = {235, 235, 245, 255};
constexpr Color kTextSecondary  = {160, 165, 180, 255};
constexpr Color kTextMuted      = {100, 105, 120, 255};
constexpr Color kTextOnAccent   = {255, 255, 255, 255};

constexpr Color kSuccess        = {80, 200, 120, 255};
constexpr Color kWarning        = {240, 180, 60, 255};
constexpr Color kError          = {240, 80, 80, 255};

constexpr Color kBorderNormal   = {55, 58, 75, 255};
constexpr Color kBorderFocused  = {86, 140, 245, 255};
constexpr Color kSeparator      = {45, 48, 60, 255};


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


struct EditorFonts {
    Font regular;
    Font semiBold;
    Font bold;
    bool loaded;
};

void InitEditorFonts();

void ShutdownEditorFonts();

const EditorFonts& GetFonts();

void ApplyEditorStyle();

void ResetToDefaultStyle();

void DrawStyledPanel(Rectangle bounds, const char* title = nullptr);

void DrawSectionHeader(Rectangle bounds, const char* text, int iconId = -1);

void DrawSeparator(float x, float y, float width);

void DrawModalOverlay(int screenWidth, int screenHeight);

void DrawModalWindow(Rectangle bounds, const char* title);

void DrawStyledLabel(Rectangle bounds, const char* text, bool secondary = false);

void DrawTooltipBox(Rectangle controlBounds, const char* text);

bool StyledButton(Rectangle bounds, const char* text, int iconId = -1, bool primary = true);

bool StyledTextBox(Rectangle bounds, const char* label, char* text, int textSize, bool* editMode);

bool StyledValueBox(Rectangle bounds, const char* label, int* value, int minValue, int maxValue, bool* editMode);

bool StyledSlider(Rectangle bounds, const char* label, float* value, float minValue, float maxValue, const char* format = "%.2f");

bool StyledCheckBox(Rectangle bounds, const char* label, bool* checked);

bool StyledDropdownBox(Rectangle bounds, const char* label, const char* items, int* active, bool* editMode);

bool StyledListView(Rectangle bounds, const char* items, int* scrollIndex, int* active);

struct VerticalLayout {
    float x;
    float y;
    float width;
    float currentY;
    float gap;

    VerticalLayout(float x, float y, float width, float gap = kControlGap)
        : x(x), y(y), width(width), currentY(y), gap(gap) {}

    Rectangle NextRow(float height);

    void AddSpace(float space);

    float GetY() const { return currentY; }
};

void DrawBlockPreview(Rectangle bounds, int blockType);

} // namespace editor_ui

#endif // MAP_EDITOR_STYLE_HPP
