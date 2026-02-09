#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace ui::xmlui {

enum class UIAnchor {
    TopLeft,
    Top,
    TopRight,
    Left,
    Center,
    Right,
    BottomLeft,
    Bottom,
    BottomRight,
};

enum class UIDirection {
    Row,
    Column,
};

enum class UIAlign {
    Start,
    Center,
    End,
};

enum class UITextAlign {
    Left,
    Center,
    Right,
};

enum class UIVerticalAlign {
    Top,
    Middle,
    Bottom,
};

struct UIBox {
    int left{0};
    int top{0};
    int right{0};
    int bottom{0};
};

struct UIColor {
    unsigned char r{255};
    unsigned char g{255};
    unsigned char b{255};
    unsigned char a{255};

    bool operator==(const UIColor& other) const {
        return r == other.r && g == other.g && b == other.b && a == other.a;
    }
};

struct UIStyle {
    std::optional<int> width{};
    std::optional<int> height{};

    UIBox margin{};
    UIBox padding{};

    int gap{0};
    UIAnchor anchor{UIAnchor::TopLeft};

    UIDirection direction{UIDirection::Column};
    UIAlign align_items{UIAlign::Start};
    UIAlign justify_content{UIAlign::Start};
    bool grow{false};

    int font_size{20};
    UIColor color{255, 255, 255, 255};
    UITextAlign text_align{UITextAlign::Left};
    UIVerticalAlign vertical_align{UIVerticalAlign::Top};

    std::optional<UIColor> background_color{};

    int border_width{0};
    UIColor border_color{100, 100, 100, 255};
    int border_radius{0};

    bool visible{true};
    float opacity{1.0f};
    
    bool has_shadow{false};
    int shadow_offset_x{0};
    int shadow_offset_y{4};
    int shadow_blur{8};
    UIColor shadow_color{0, 0, 0, 128};
};

struct CSSSelector {
    enum class Kind { Type, Class, Id };
    Kind kind{Kind::Type};
    std::string value{};
};

// Bitmask tracking which CSS properties were explicitly set in a rule.
// Used during cascade so that unspecified properties don't overwrite
// values from earlier matching rules with their defaults
namespace CSSProp {
    constexpr uint32_t Margin       = 1u << 0;
    constexpr uint32_t Padding      = 1u << 1;
    constexpr uint32_t Gap          = 1u << 2;
    constexpr uint32_t Anchor       = 1u << 3;
    constexpr uint32_t Direction    = 1u << 4;
    constexpr uint32_t AlignItems   = 1u << 5;
    constexpr uint32_t Justify      = 1u << 6;
    constexpr uint32_t Grow         = 1u << 7;
    constexpr uint32_t FontSize     = 1u << 8;
    constexpr uint32_t Color        = 1u << 9;
    constexpr uint32_t TextAlign    = 1u << 10;
    constexpr uint32_t VAlign       = 1u << 11;
    constexpr uint32_t BorderWidth  = 1u << 12;
    constexpr uint32_t BorderColor  = 1u << 13;
    constexpr uint32_t BorderRadius = 1u << 14;
    constexpr uint32_t Visible      = 1u << 15;
    constexpr uint32_t Opacity      = 1u << 16;
    constexpr uint32_t Shadow       = 1u << 17;
};

struct CSSRule {
    CSSSelector selector{};
    UIStyle style{};
    uint32_t set_props{0};  // Bitmask of explicitly specified properties
};

struct CSSParseResult {
    std::vector<CSSRule> rules{};
    std::string error{};

    bool ok() const { return error.empty(); }
};

CSSParseResult parse_css_lite(std::string_view css);

UIStyle compute_style(
    const std::vector<CSSRule>& rules,
    std::string_view type,
    std::string_view id,
    std::string_view class_name);

} // namespace ui::xmlui
