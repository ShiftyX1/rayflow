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
};

struct CSSSelector {
    enum class Kind { Type, Class, Id };
    Kind kind{Kind::Type};
    std::string value{};
};

struct CSSRule {
    CSSSelector selector{};
    UIStyle style{};
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
