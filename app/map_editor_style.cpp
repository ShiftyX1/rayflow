/**
 * @file map_editor_style.cpp
 * @brief Implementation of custom raygui styling for the Map Editor.
 */

#include "map_editor_style.hpp"
#include "../ui/raygui.h"

#include <cstdio>
#include <cstring>
#include <filesystem>

namespace editor_ui {

// ============================================================================
// Static state
// ============================================================================

static EditorFonts s_fonts = {};

// ============================================================================
// Font management
// ============================================================================

static std::filesystem::path find_fonts_dir() {
    namespace fs = std::filesystem;
    const fs::path candidates[] = {
        fs::path{"fonts"},
        fs::path{"../fonts"},
        fs::path{"../../fonts"},
        fs::path{"../client/static/fonts"},
        fs::path{"../../client/static/fonts"},
    };
    for (const auto& p : candidates) {
        if (fs::exists(p) && fs::is_directory(p)) return p;
    }
    return fs::path{"fonts"};
}

void InitEditorFonts() {
    if (s_fonts.loaded) return;

    auto fontsDir = find_fonts_dir();
    
    // Pre-construct paths as std::string to keep memory valid
    std::string regularStr = (fontsDir / "Inter_18pt-Regular.ttf").string();
    std::string semiBoldStr = (fontsDir / "Inter_18pt-SemiBold.ttf").string();
    std::string boldStr = (fontsDir / "Inter_18pt-Bold.ttf").string();

    // Default codepoints (ASCII + extended Latin)
    s_fonts.regular = LoadFontEx(regularStr.c_str(), 18, nullptr, 0);
    s_fonts.semiBold = LoadFontEx(semiBoldStr.c_str(), 18, nullptr, 0);
    s_fonts.bold = LoadFontEx(boldStr.c_str(), 22, nullptr, 0);

    // Check if fonts loaded successfully
    if (s_fonts.regular.texture.id == 0) {
        TraceLog(LOG_WARNING, "[EditorUI] Failed to load regular font, using default");
        s_fonts.regular = GetFontDefault();
    }
    if (s_fonts.semiBold.texture.id == 0) {
        s_fonts.semiBold = s_fonts.regular;
    }
    if (s_fonts.bold.texture.id == 0) {
        s_fonts.bold = s_fonts.regular;
    }

    // Set texture filter for better rendering
    if (s_fonts.regular.texture.id != GetFontDefault().texture.id) {
        SetTextureFilter(s_fonts.regular.texture, TEXTURE_FILTER_BILINEAR);
    }
    if (s_fonts.semiBold.texture.id != GetFontDefault().texture.id) {
        SetTextureFilter(s_fonts.semiBold.texture, TEXTURE_FILTER_BILINEAR);
    }
    if (s_fonts.bold.texture.id != GetFontDefault().texture.id) {
        SetTextureFilter(s_fonts.bold.texture, TEXTURE_FILTER_BILINEAR);
    }

    s_fonts.loaded = true;
}

void ShutdownEditorFonts() {
    if (!s_fonts.loaded) return;

    // Only unload if not the default font
    Font defaultFont = GetFontDefault();
    if (s_fonts.regular.texture.id != defaultFont.texture.id) {
        UnloadFont(s_fonts.regular);
    }
    if (s_fonts.semiBold.texture.id != defaultFont.texture.id && 
        s_fonts.semiBold.texture.id != s_fonts.regular.texture.id) {
        UnloadFont(s_fonts.semiBold);
    }
    if (s_fonts.bold.texture.id != defaultFont.texture.id && 
        s_fonts.bold.texture.id != s_fonts.regular.texture.id) {
        UnloadFont(s_fonts.bold);
    }

    s_fonts = {};
}

const EditorFonts& GetFonts() {
    return s_fonts;
}

// ============================================================================
// Style setup
// ============================================================================

static int ColorToStyleInt(Color c) {
    return ((int)c.r << 24) | ((int)c.g << 16) | ((int)c.b << 8) | (int)c.a;
}

void ApplyEditorStyle() {
    // Set custom font for raygui
    if (s_fonts.loaded && s_fonts.regular.texture.id != 0) {
        GuiSetFont(s_fonts.regular);
    }

    // Global defaults
    GuiSetStyle(DEFAULT, TEXT_SIZE, 16);
    GuiSetStyle(DEFAULT, TEXT_SPACING, 1);
    GuiSetStyle(DEFAULT, TEXT_LINE_SPACING, 20);
    GuiSetStyle(DEFAULT, TEXT_ALIGNMENT_VERTICAL, TEXT_ALIGN_MIDDLE);
    GuiSetStyle(DEFAULT, BACKGROUND_COLOR, ColorToStyleInt(kBgDark));  // Used by Panel, ListView background
    GuiSetStyle(DEFAULT, LINE_COLOR, ColorToStyleInt(kSeparator));     // Used for panel borders

    // DEFAULT control (base for all)
    GuiSetStyle(DEFAULT, BORDER_COLOR_NORMAL, ColorToStyleInt(kBorderNormal));
    GuiSetStyle(DEFAULT, BASE_COLOR_NORMAL, ColorToStyleInt(kBgPanelLight));
    GuiSetStyle(DEFAULT, TEXT_COLOR_NORMAL, ColorToStyleInt(kTextPrimary));
    
    GuiSetStyle(DEFAULT, BORDER_COLOR_FOCUSED, ColorToStyleInt(kBorderFocused));
    GuiSetStyle(DEFAULT, BASE_COLOR_FOCUSED, ColorToStyleInt(kBgHover));
    GuiSetStyle(DEFAULT, TEXT_COLOR_FOCUSED, ColorToStyleInt(kTextPrimary));
    
    GuiSetStyle(DEFAULT, BORDER_COLOR_PRESSED, ColorToStyleInt(kAccentPressed));
    GuiSetStyle(DEFAULT, BASE_COLOR_PRESSED, ColorToStyleInt(kAccentPrimary));
    GuiSetStyle(DEFAULT, TEXT_COLOR_PRESSED, ColorToStyleInt(kTextOnAccent));
    
    GuiSetStyle(DEFAULT, BORDER_COLOR_DISABLED, ColorToStyleInt(kBgPanelLight));
    GuiSetStyle(DEFAULT, BASE_COLOR_DISABLED, ColorToStyleInt(kBgPanel));
    GuiSetStyle(DEFAULT, TEXT_COLOR_DISABLED, ColorToStyleInt(kTextMuted));

    GuiSetStyle(DEFAULT, BORDER_WIDTH, kBorderWidth);
    GuiSetStyle(DEFAULT, TEXT_PADDING, 8);

    // BUTTON
    GuiSetStyle(BUTTON, BORDER_COLOR_NORMAL, ColorToStyleInt(kAccentMuted));
    GuiSetStyle(BUTTON, BASE_COLOR_NORMAL, ColorToStyleInt(kBgPanelLight));
    GuiSetStyle(BUTTON, TEXT_COLOR_NORMAL, ColorToStyleInt(kTextPrimary));
    
    GuiSetStyle(BUTTON, BORDER_COLOR_FOCUSED, ColorToStyleInt(kAccentPrimary));
    GuiSetStyle(BUTTON, BASE_COLOR_FOCUSED, ColorToStyleInt(kBgHover));
    GuiSetStyle(BUTTON, TEXT_COLOR_FOCUSED, ColorToStyleInt(kTextPrimary));
    
    GuiSetStyle(BUTTON, BORDER_COLOR_PRESSED, ColorToStyleInt(kAccentPressed));
    GuiSetStyle(BUTTON, BASE_COLOR_PRESSED, ColorToStyleInt(kAccentPrimary));
    GuiSetStyle(BUTTON, TEXT_COLOR_PRESSED, ColorToStyleInt(kTextOnAccent));

    // TEXTBOX
    GuiSetStyle(TEXTBOX, BORDER_COLOR_NORMAL, ColorToStyleInt(kBorderNormal));
    GuiSetStyle(TEXTBOX, BASE_COLOR_NORMAL, ColorToStyleInt(kBgDark));
    GuiSetStyle(TEXTBOX, TEXT_COLOR_NORMAL, ColorToStyleInt(kTextPrimary));
    
    GuiSetStyle(TEXTBOX, BORDER_COLOR_FOCUSED, ColorToStyleInt(kAccentPrimary));
    GuiSetStyle(TEXTBOX, BASE_COLOR_FOCUSED, ColorToStyleInt(kBgDark));
    GuiSetStyle(TEXTBOX, TEXT_COLOR_FOCUSED, ColorToStyleInt(kTextPrimary));

    GuiSetStyle(TEXTBOX, BORDER_COLOR_PRESSED, ColorToStyleInt(kAccentPrimary));
    GuiSetStyle(TEXTBOX, BASE_COLOR_PRESSED, ColorToStyleInt(kBgDark));
    GuiSetStyle(TEXTBOX, TEXT_COLOR_PRESSED, ColorToStyleInt(kTextPrimary));

    // VALUEBOX
    GuiSetStyle(VALUEBOX, BORDER_COLOR_NORMAL, ColorToStyleInt(kBorderNormal));
    GuiSetStyle(VALUEBOX, BASE_COLOR_NORMAL, ColorToStyleInt(kBgDark));
    GuiSetStyle(VALUEBOX, TEXT_COLOR_NORMAL, ColorToStyleInt(kTextPrimary));

    // SLIDER
    GuiSetStyle(SLIDER, BORDER_COLOR_NORMAL, ColorToStyleInt(kBorderNormal));
    GuiSetStyle(SLIDER, BASE_COLOR_NORMAL, ColorToStyleInt(kBgDark));
    GuiSetStyle(SLIDER, TEXT_COLOR_NORMAL, ColorToStyleInt(kTextSecondary));
    
    GuiSetStyle(SLIDER, BORDER_COLOR_PRESSED, ColorToStyleInt(kAccentPrimary));
    GuiSetStyle(SLIDER, BASE_COLOR_PRESSED, ColorToStyleInt(kAccentPrimary));
    
    GuiSetStyle(SLIDER, SLIDER_WIDTH, 12);
    GuiSetStyle(SLIDER, SLIDER_PADDING, 2);

    // CHECKBOX
    GuiSetStyle(CHECKBOX, BORDER_COLOR_NORMAL, ColorToStyleInt(kBorderNormal));
    GuiSetStyle(CHECKBOX, BASE_COLOR_NORMAL, ColorToStyleInt(kBgDark));
    GuiSetStyle(CHECKBOX, TEXT_COLOR_NORMAL, ColorToStyleInt(kTextPrimary));
    
    GuiSetStyle(CHECKBOX, BORDER_COLOR_PRESSED, ColorToStyleInt(kAccentPrimary));
    GuiSetStyle(CHECKBOX, BASE_COLOR_PRESSED, ColorToStyleInt(kAccentPrimary));

    GuiSetStyle(CHECKBOX, CHECK_PADDING, 4);

    // DROPDOWNBOX
    GuiSetStyle(DROPDOWNBOX, BORDER_COLOR_NORMAL, ColorToStyleInt(kBorderNormal));
    GuiSetStyle(DROPDOWNBOX, BASE_COLOR_NORMAL, ColorToStyleInt(kBgDark));
    GuiSetStyle(DROPDOWNBOX, TEXT_COLOR_NORMAL, ColorToStyleInt(kTextPrimary));
    
    GuiSetStyle(DROPDOWNBOX, BORDER_COLOR_FOCUSED, ColorToStyleInt(kAccentPrimary));
    GuiSetStyle(DROPDOWNBOX, BASE_COLOR_FOCUSED, ColorToStyleInt(kBgPanelLight));
    GuiSetStyle(DROPDOWNBOX, TEXT_COLOR_FOCUSED, ColorToStyleInt(kTextPrimary));

    GuiSetStyle(DROPDOWNBOX, BORDER_COLOR_PRESSED, ColorToStyleInt(kAccentPrimary));
    GuiSetStyle(DROPDOWNBOX, BASE_COLOR_PRESSED, ColorToStyleInt(kBgHover));
    GuiSetStyle(DROPDOWNBOX, TEXT_COLOR_PRESSED, ColorToStyleInt(kTextPrimary));

    // LISTVIEW
    GuiSetStyle(LISTVIEW, BORDER_COLOR_NORMAL, ColorToStyleInt(kBorderNormal));
    GuiSetStyle(LISTVIEW, BASE_COLOR_NORMAL, ColorToStyleInt(kBgDark));
    GuiSetStyle(LISTVIEW, TEXT_COLOR_NORMAL, ColorToStyleInt(kTextPrimary));

    GuiSetStyle(LISTVIEW, BORDER_COLOR_FOCUSED, ColorToStyleInt(kAccentPrimary));
    GuiSetStyle(LISTVIEW, BASE_COLOR_FOCUSED, ColorToStyleInt(kBgPanelLight));
    GuiSetStyle(LISTVIEW, TEXT_COLOR_FOCUSED, ColorToStyleInt(kTextPrimary));

    GuiSetStyle(LISTVIEW, BORDER_COLOR_PRESSED, ColorToStyleInt(kAccentPrimary));
    GuiSetStyle(LISTVIEW, BASE_COLOR_PRESSED, ColorToStyleInt(kBgHover));
    GuiSetStyle(LISTVIEW, TEXT_COLOR_PRESSED, ColorToStyleInt(kTextPrimary));
    
    GuiSetStyle(LISTVIEW, LIST_ITEMS_HEIGHT, 28);
    GuiSetStyle(LISTVIEW, LIST_ITEMS_SPACING, 2);
    GuiSetStyle(LISTVIEW, SCROLLBAR_WIDTH, 10);

    // SCROLLBAR
    GuiSetStyle(SCROLLBAR, BORDER_COLOR_NORMAL, ColorToStyleInt(kBgPanel));
    GuiSetStyle(SCROLLBAR, BASE_COLOR_NORMAL, ColorToStyleInt(kBgPanelLight));
    GuiSetStyle(SCROLLBAR, BORDER_COLOR_FOCUSED, ColorToStyleInt(kAccentMuted));
    GuiSetStyle(SCROLLBAR, BASE_COLOR_FOCUSED, ColorToStyleInt(kBgHover));
    GuiSetStyle(SCROLLBAR, ARROWS_VISIBLE, 0);

    // LABEL
    GuiSetStyle(LABEL, TEXT_COLOR_NORMAL, ColorToStyleInt(kTextSecondary));

    // PROGRESSBAR
    GuiSetStyle(PROGRESSBAR, BORDER_COLOR_NORMAL, ColorToStyleInt(kBorderNormal));
    GuiSetStyle(PROGRESSBAR, BASE_COLOR_NORMAL, ColorToStyleInt(kBgDark));
    GuiSetStyle(PROGRESSBAR, BASE_COLOR_PRESSED, ColorToStyleInt(kAccentPrimary));
}

void ResetToDefaultStyle() {
    GuiLoadStyleDefault();
}

// ============================================================================
// Drawing helpers
// ============================================================================

void DrawStyledPanel(Rectangle bounds, const char* title) {
    // Panel background with slight rounded appearance
    DrawRectangleRec(bounds, kBgPanel);
    DrawRectangleLinesEx(bounds, 1.0f, kBorderNormal);

    // Title bar if title provided
    if (title != nullptr && title[0] != '\0') {
        Rectangle titleBar = {bounds.x, bounds.y, bounds.width, 32.0f};
        DrawRectangleRec(titleBar, kBgPanelLight);
        DrawLineEx(
            {bounds.x, bounds.y + 32.0f},
            {bounds.x + bounds.width, bounds.y + 32.0f},
            1.0f, kSeparator
        );

        // Draw title text
        Vector2 textPos = {
            bounds.x + kPanelPadding,
            bounds.y + (32.0f - 16.0f) / 2.0f
        };
        if (s_fonts.loaded) {
            DrawTextEx(s_fonts.semiBold, title, textPos, 16, 1, kTextPrimary);
        } else {
            DrawText(title, (int)textPos.x, (int)textPos.y, 16, kTextPrimary);
        }
    }
}

void DrawSectionHeader(Rectangle bounds, const char* text, int iconId) {
    float textX = bounds.x;

    // Draw icon if provided
    if (iconId >= 0) {
        GuiDrawIcon(iconId, (int)bounds.x, (int)(bounds.y + (bounds.height - 16) / 2), 1, kAccentPrimary);
        textX += 20.0f;
    }

    // Draw text
    Vector2 textPos = {textX, bounds.y + (bounds.height - 14.0f) / 2.0f};
    if (s_fonts.loaded) {
        DrawTextEx(s_fonts.semiBold, text, textPos, 14, 1, kTextSecondary);
    } else {
        DrawText(text, (int)textPos.x, (int)textPos.y, 14, kTextSecondary);
    }

    // Draw underline
    DrawLineEx(
        {bounds.x, bounds.y + bounds.height - 1},
        {bounds.x + bounds.width, bounds.y + bounds.height - 1},
        1.0f, kSeparator
    );
}

void DrawSeparator(float x, float y, float width) {
    DrawLineEx({x, y}, {x + width, y}, 1.0f, kSeparator);
}

void DrawModalOverlay(int screenWidth, int screenHeight) {
    DrawRectangle(0, 0, screenWidth, screenHeight, Fade(BLACK, 0.7f));
}

void DrawModalWindow(Rectangle bounds, const char* title) {
    // Shadow effect (offset rectangle)
    DrawRectangle(
        (int)(bounds.x + 4),
        (int)(bounds.y + 4),
        (int)bounds.width,
        (int)bounds.height,
        Fade(BLACK, 0.3f)
    );

    // Main window background
    DrawRectangleRec(bounds, kBgPanel);
    DrawRectangleLinesEx(bounds, 1.0f, kBorderNormal);

    // Title bar
    if (title != nullptr && title[0] != '\0') {
        Rectangle titleBar = {bounds.x, bounds.y, bounds.width, 40.0f};
        DrawRectangleRec(titleBar, kBgPanelLight);
        DrawLineEx(
            {bounds.x, bounds.y + 40.0f},
            {bounds.x + bounds.width, bounds.y + 40.0f},
            1.0f, kSeparator
        );

        // Title text centered
        Vector2 textSize = {0, 0};
        if (s_fonts.loaded) {
            textSize = MeasureTextEx(s_fonts.bold, title, 18, 1);
        } else {
            textSize.x = (float)MeasureText(title, 18);
            textSize.y = 18.0f;
        }

        Vector2 textPos = {
            bounds.x + (bounds.width - textSize.x) / 2.0f,
            bounds.y + (40.0f - textSize.y) / 2.0f
        };

        if (s_fonts.loaded) {
            DrawTextEx(s_fonts.bold, title, textPos, 18, 1, kTextPrimary);
        } else {
            DrawText(title, (int)textPos.x, (int)textPos.y, 18, kTextPrimary);
        }
    }
}

void DrawStyledLabel(Rectangle bounds, const char* text, bool secondary) {
    Color color = secondary ? kTextMuted : kTextSecondary;
    Vector2 textPos = {bounds.x, bounds.y + (bounds.height - 14.0f) / 2.0f};
    
    if (s_fonts.loaded) {
        DrawTextEx(s_fonts.regular, text, textPos, 14, 1, color);
    } else {
        DrawText(text, (int)textPos.x, (int)textPos.y, 14, color);
    }
}

void DrawTooltipBox(Rectangle controlBounds, const char* text) {
    if (text == nullptr || text[0] == '\0') return;

    Vector2 textSize = MeasureTextEx(GetFontDefault(), text, 14, 1);
    float padding = 8.0f;

    Rectangle tipBounds = {
        controlBounds.x,
        controlBounds.y - textSize.y - padding * 2 - 4,
        textSize.x + padding * 2,
        textSize.y + padding * 2
    };

    // Ensure tooltip is visible on screen
    if (tipBounds.y < 0) {
        tipBounds.y = controlBounds.y + controlBounds.height + 4;
    }

    DrawRectangleRec(tipBounds, kBgPanelLight);
    DrawRectangleLinesEx(tipBounds, 1.0f, kBorderNormal);
    DrawText(text, (int)(tipBounds.x + padding), (int)(tipBounds.y + padding), 14, kTextPrimary);
}

// ============================================================================
// Custom controls
// ============================================================================

bool StyledButton(Rectangle bounds, const char* text, int iconId, bool primary) {
    // For primary buttons, temporarily override colors
    if (primary) {
        GuiSetStyle(BUTTON, BASE_COLOR_NORMAL, ColorToStyleInt(kAccentPrimary));
        GuiSetStyle(BUTTON, TEXT_COLOR_NORMAL, ColorToStyleInt(kTextOnAccent));
        GuiSetStyle(BUTTON, BORDER_COLOR_NORMAL, ColorToStyleInt(kAccentPressed));
        
        GuiSetStyle(BUTTON, BASE_COLOR_FOCUSED, ColorToStyleInt(kAccentHover));
        GuiSetStyle(BUTTON, TEXT_COLOR_FOCUSED, ColorToStyleInt(kTextOnAccent));
        GuiSetStyle(BUTTON, BORDER_COLOR_FOCUSED, ColorToStyleInt(kAccentPrimary));
    }

    // Add icon to text if provided
    const char* displayText = text;
    if (iconId >= 0) {
        displayText = GuiIconText(iconId, text);
    }

    bool result = GuiButton(bounds, displayText);

    // Reset to normal button style
    if (primary) {
        GuiSetStyle(BUTTON, BORDER_COLOR_NORMAL, ColorToStyleInt(kAccentMuted));
        GuiSetStyle(BUTTON, BASE_COLOR_NORMAL, ColorToStyleInt(kBgPanelLight));
        GuiSetStyle(BUTTON, TEXT_COLOR_NORMAL, ColorToStyleInt(kTextPrimary));
        
        GuiSetStyle(BUTTON, BORDER_COLOR_FOCUSED, ColorToStyleInt(kAccentPrimary));
        GuiSetStyle(BUTTON, BASE_COLOR_FOCUSED, ColorToStyleInt(kBgHover));
        GuiSetStyle(BUTTON, TEXT_COLOR_FOCUSED, ColorToStyleInt(kTextPrimary));
    }

    return result;
}

bool StyledTextBox(Rectangle bounds, const char* label, char* text, int textSize, bool* editMode) {
    float labelWidth = 80.0f;
    
    // Draw label
    if (label != nullptr && label[0] != '\0') {
        DrawStyledLabel({bounds.x, bounds.y, labelWidth, bounds.height}, label);
    }

    // Adjust text box bounds
    Rectangle textBounds = {
        bounds.x + labelWidth,
        bounds.y,
        bounds.width - labelWidth,
        bounds.height
    };

    bool wasEditing = *editMode;
    if (GuiTextBox(textBounds, text, textSize, *editMode)) {
        *editMode = !(*editMode);
    }
    return wasEditing != *editMode;
}

bool StyledValueBox(Rectangle bounds, const char* label, int* value, int minValue, int maxValue, bool* editMode) {
    float labelWidth = 80.0f;

    // Draw label
    if (label != nullptr && label[0] != '\0') {
        DrawStyledLabel({bounds.x, bounds.y, labelWidth, bounds.height}, label);
    }

    // Adjust value box bounds
    Rectangle valueBounds = {
        bounds.x + labelWidth,
        bounds.y,
        bounds.width - labelWidth,
        bounds.height
    };

    bool wasEditing = *editMode;
    if (GuiValueBox(valueBounds, nullptr, value, minValue, maxValue, *editMode)) {
        *editMode = !(*editMode);
    }
    return wasEditing != *editMode;
}

bool StyledSlider(Rectangle bounds, const char* label, float* value, float minValue, float maxValue, const char* format) {
    float labelWidth = 60.0f;
    float valueWidth = 50.0f;

    // Draw label on left
    if (label != nullptr && label[0] != '\0') {
        DrawStyledLabel({bounds.x, bounds.y, labelWidth, bounds.height}, label);
    }

    // Draw value on right
    char valueStr[32];
    snprintf(valueStr, sizeof(valueStr), format, *value);
    
    Vector2 valueSize = MeasureTextEx(GetFontDefault(), valueStr, 14, 1);
    Vector2 valuePos = {
        bounds.x + bounds.width - valueSize.x,
        bounds.y + (bounds.height - 14.0f) / 2.0f
    };
    DrawText(valueStr, (int)valuePos.x, (int)valuePos.y, 14, kTextSecondary);

    // Slider bounds
    Rectangle sliderBounds = {
        bounds.x + labelWidth,
        bounds.y,
        bounds.width - labelWidth - valueWidth - 8,
        bounds.height
    };

    float oldValue = *value;
    GuiSliderBar(sliderBounds, nullptr, nullptr, value, minValue, maxValue);
    return *value != oldValue;
}

bool StyledCheckBox(Rectangle bounds, const char* label, bool* checked) {
    bool oldValue = *checked;
    GuiCheckBox(bounds, label, checked);
    return *checked != oldValue;
}

bool StyledDropdownBox(Rectangle bounds, const char* label, const char* items, int* active, bool* editMode) {
    float labelWidth = 80.0f;

    // Draw label
    if (label != nullptr && label[0] != '\0') {
        DrawStyledLabel({bounds.x, bounds.y, labelWidth, bounds.height}, label);
    }

    // Dropdown bounds
    Rectangle dropBounds = {
        bounds.x + labelWidth,
        bounds.y,
        bounds.width - labelWidth,
        bounds.height
    };

    int oldActive = *active;
    if (GuiDropdownBox(dropBounds, items, active, *editMode)) {
        *editMode = !(*editMode);
    }
    return *active != oldActive;
}

bool StyledListView(Rectangle bounds, const char* items, int* scrollIndex, int* active) {
    int oldActive = *active;
    GuiListView(bounds, items, scrollIndex, active);
    return *active != oldActive;
}

// ============================================================================
// Layout helpers
// ============================================================================

Rectangle VerticalLayout::NextRow(float height) {
    Rectangle r = {x, currentY, width, height};
    currentY += height + gap;
    return r;
}

void VerticalLayout::AddSpace(float space) {
    currentY += space;
}

// ============================================================================
// Block palette (placeholder)
// ============================================================================

void DrawBlockPreview(Rectangle bounds, int blockType) {
    // Simple colored rectangle as block preview (can be extended with actual textures)
    Color colors[] = {
        {128, 128, 128, 255},  // Air
        {100, 200, 100, 255},  // Grass
        {139, 90, 43, 255},    // Dirt
        {128, 128, 128, 255},  // Stone
        {50, 50, 50, 255},     // Bedrock
        {200, 180, 150, 255},  // Sand
        // ... more colors for other block types
    };

    int colorIndex = blockType % (sizeof(colors) / sizeof(colors[0]));
    DrawRectangleRec(bounds, colors[colorIndex]);
    DrawRectangleLinesEx(bounds, 1.0f, kBorderNormal);
}

} // namespace editor_ui
