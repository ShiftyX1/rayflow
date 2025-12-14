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

struct UIBox {
    int left{0};
    int top{0};
    int right{0};
    int bottom{0};
};

struct UIStyle {
    std::optional<int> width{};
    std::optional<int> height{};

    UIBox margin{};
    UIBox padding{};

    int gap{0};
    UIAnchor anchor{UIAnchor::TopLeft};
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
