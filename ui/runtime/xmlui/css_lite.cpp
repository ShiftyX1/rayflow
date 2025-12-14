#include "css_lite.hpp"

#include <cctype>
#include <optional>
#include <utility>

namespace ui::xmlui {

static void skip_ws(std::string_view s, size_t& i) {
    while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i]))) {
        i++;
    }
}

static bool consume(std::string_view s, size_t& i, char c) {
    skip_ws(s, i);
    if (i < s.size() && s[i] == c) {
        i++;
        return true;
    }
    return false;
}

static std::string parse_ident(std::string_view s, size_t& i) {
    skip_ws(s, i);
    size_t start = i;
    while (i < s.size()) {
        char ch = s[i];
        if (std::isalnum(static_cast<unsigned char>(ch)) || ch == '-' || ch == '_') {
            i++;
            continue;
        }
        break;
    }
    return std::string{s.substr(start, i - start)};
}

static std::string parse_until(std::string_view s, size_t& i, char end) {
    skip_ws(s, i);
    size_t start = i;
    while (i < s.size() && s[i] != end) {
        i++;
    }
    return std::string{s.substr(start, i - start)};
}

static std::optional<int> parse_int(std::string_view s) {
    size_t i = 0;
    while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i]))) i++;
    bool neg = false;
    if (i < s.size() && s[i] == '-') {
        neg = true;
        i++;
    }
    int v = 0;
    bool any = false;
    while (i < s.size() && std::isdigit(static_cast<unsigned char>(s[i]))) {
        any = true;
        v = v * 10 + (s[i] - '0');
        i++;
    }
    if (!any) return std::nullopt;
    if (neg) v = -v;
    return v;
}

static void set_box_all(UIBox& b, int v) {
    b.left = v;
    b.top = v;
    b.right = v;
    b.bottom = v;
}

static std::optional<UIAnchor> parse_anchor(std::string_view v) {
    auto norm = [](std::string_view x) {
        std::string out;
        out.reserve(x.size());
        for (char c : x) {
            if (!std::isspace(static_cast<unsigned char>(c))) {
                out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
            }
        }
        return out;
    };

    const std::string s = norm(v);
    if (s == "top-left" || s == "topleft") return UIAnchor::TopLeft;
    if (s == "top") return UIAnchor::Top;
    if (s == "top-right" || s == "topright") return UIAnchor::TopRight;
    if (s == "left") return UIAnchor::Left;
    if (s == "center") return UIAnchor::Center;
    if (s == "right") return UIAnchor::Right;
    if (s == "bottom-left" || s == "bottomleft") return UIAnchor::BottomLeft;
    if (s == "bottom") return UIAnchor::Bottom;
    if (s == "bottom-right" || s == "bottomright") return UIAnchor::BottomRight;
    return std::nullopt;
}

static bool selector_matches(const CSSSelector& sel, std::string_view type, std::string_view id, std::string_view class_name) {
    switch (sel.kind) {
        case CSSSelector::Kind::Type: return sel.value == type;
        case CSSSelector::Kind::Id: return sel.value == id;
        case CSSSelector::Kind::Class: return sel.value == class_name;
    }
    return false;
}

CSSParseResult parse_css_lite(std::string_view css) {
    CSSParseResult out;

    size_t i = 0;
    while (true) {
        skip_ws(css, i);
        if (i >= css.size()) break;

        // selector
        CSSRule rule;
        if (css[i] == '#') {
            i++;
            rule.selector.kind = CSSSelector::Kind::Id;
            rule.selector.value = parse_ident(css, i);
        } else if (css[i] == '.') {
            i++;
            rule.selector.kind = CSSSelector::Kind::Class;
            rule.selector.value = parse_ident(css, i);
        } else {
            rule.selector.kind = CSSSelector::Kind::Type;
            rule.selector.value = parse_ident(css, i);
        }

        if (rule.selector.value.empty()) {
            out.error = "CSS parse error: expected selector";
            return out;
        }

        if (!consume(css, i, '{')) {
            out.error = "CSS parse error: expected '{'";
            return out;
        }

        while (true) {
            skip_ws(css, i);
            if (i >= css.size()) {
                out.error = "CSS parse error: unexpected end of file";
                return out;
            }
            if (css[i] == '}') {
                i++;
                break;
            }

            const std::string key = parse_ident(css, i);
            if (key.empty()) {
                out.error = "CSS parse error: expected property name";
                return out;
            }
            if (!consume(css, i, ':')) {
                out.error = "CSS parse error: expected ':'";
                return out;
            }

            const std::string value = parse_until(css, i, ';');
            if (!consume(css, i, ';')) {
                out.error = "CSS parse error: expected ';'";
                return out;
            }

            if (key == "width") {
                if (auto v = parse_int(value); v.has_value()) rule.style.width = *v;
            } else if (key == "height") {
                if (auto v = parse_int(value); v.has_value()) rule.style.height = *v;
            } else if (key == "gap") {
                if (auto v = parse_int(value); v.has_value()) rule.style.gap = *v;
            } else if (key == "margin") {
                if (auto v = parse_int(value); v.has_value()) set_box_all(rule.style.margin, *v);
            } else if (key == "padding") {
                if (auto v = parse_int(value); v.has_value()) set_box_all(rule.style.padding, *v);
            } else if (key == "anchor") {
                if (auto a = parse_anchor(value); a.has_value()) rule.style.anchor = *a;
            }
        }

        out.rules.push_back(std::move(rule));
    }

    return out;
}

UIStyle compute_style(
    const std::vector<CSSRule>& rules,
    std::string_view type,
    std::string_view id,
    std::string_view class_name) {
    UIStyle out;

    // Apply in order; later rules override earlier.
    for (const auto& r : rules) {
        if (!selector_matches(r.selector, type, id, class_name)) continue;

        if (r.style.width.has_value()) out.width = r.style.width;
        if (r.style.height.has_value()) out.height = r.style.height;

        out.margin = r.style.margin;
        out.padding = r.style.padding;
        out.gap = r.style.gap;
        out.anchor = r.style.anchor;
    }

    return out;
}

} // namespace ui::xmlui
