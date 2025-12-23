# UI Framework Instructions for AI Agents

This document provides comprehensive instructions for AI agents working with the Rayflow UI framework. The goal is to enable UI changes with **minimal or zero C++ recompilation**.

## Architecture Overview

```
┌─────────────────────────────────────────────────────────────────┐
│                        Game Layer (C++)                         │
│  ┌─────────────┐    ┌──────────────┐    ┌───────────────────┐  │
│  │ Game State  │───▶│ UIViewModel  │───▶│   UIManager       │  │
│  │ (authority) │    │ (read-only)  │    │ (orchestrator)    │  │
│  └─────────────┘    └──────────────┘    └─────────┬─────────┘  │
│                                                    │            │
│  ┌─────────────┐    ┌──────────────┐              │            │
│  │ UICommand   │◀───│ User Input   │◀─────────────┘            │
│  │ (actions)   │    │ (clicks/keys)│                           │
│  └─────────────┘    └──────────────┘                           │
└─────────────────────────────────────────────────────────────────┘
                              ▲
                              │ loads at runtime
┌─────────────────────────────┴───────────────────────────────────┐
│                     UI Assets (no recompile)                    │
│  ┌─────────────┐    ┌──────────────┐    ┌───────────────────┐  │
│  │  *.xml      │    │   *.css      │    │  textures/ui/*    │  │
│  │  (layout)   │    │  (styles)    │    │  (images)         │  │
│  └─────────────┘    └──────────────┘    └───────────────────┘  │
│                                                                 │
│  ┌─────────────────────────────────────────────────────────┐   │
│  │                    *.lua (FUTURE)                        │   │
│  │              (dynamic behavior/animations)               │   │
│  └─────────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────┘
```

## File Locations

| Purpose | Source Location | Build Location | Recompile Required |
|---------|-----------------|----------------|-------------------|
| XML layouts | `client/static/ui/*.xml` | `build/ui/*.xml` | No (copy only) |
| CSS styles | `client/static/ui/*.css` | `build/ui/*.css` | No (copy only) |
| UI textures | `client/static/textures/ui/` | `build/textures/ui/` | No (copy only) |
| UI C++ runtime | `ui/runtime/` | compiled into binary | Yes |
| Screen logic | `client/core/game.cpp` | compiled into binary | Yes |

## What Can Be Changed Without Recompiling

### ✅ NO RECOMPILE NEEDED

1. **Layout changes** - Modify XML files:
   - Add/remove/reorder nodes
   - Change node hierarchy
   - Modify text content
   - Change `id`, `class`, `action` attributes
   - Add new screens (if UIManager already loads them)

2. **Style changes** - Modify CSS files:
   - Colors (background, text, border)
   - Sizes (width, height, font-size)
   - Spacing (padding, margin, gap)
   - Positioning (anchor)
   - Visual effects (border-radius, border-width)

3. **Asset changes**:
   - Replace textures with same filename
   - Modify texture sizes (CSS handles scaling)

### ⚠️ RECOMPILE REQUIRED

1. **New node types** - Adding new XML elements (e.g., `<Slider>`, `<ProgressBar>`)
2. **New CSS properties** - Adding properties not in the parser
3. **New actions** - Adding new `action=""` values that need C++ handling
4. **New screens** - Adding entirely new game screens (needs UIManager + Game changes)
5. **New data bindings** - Exposing new game state to UIViewModel

---

## XML Specification

### Document Structure

```xml
<UI>
  <!-- Top-level nodes are positioned using anchor -->
  <Panel id="unique-id" class="style-class">
    <!-- Nested nodes use parent's layout direction -->
    <Text>Content</Text>
    <Button action="action_name">Label</Button>
  </Panel>
</UI>
```

### Supported Node Types

| Node | Purpose | Attributes | Can Have Children |
|------|---------|------------|-------------------|
| `Panel` | Container with background | id, class | Yes |
| `Row` | Horizontal layout container | id, class | Yes |
| `Column` | Vertical layout container | id, class | Yes |
| `Text` | Static text display | id, class | No (text content) |
| `Button` | Clickable button | id, class, action | No (text content) |
| `Spacer` | Empty space for layout | id, class | No |
| `HealthBar` | Heart-based health display | id, class, full, half, empty | No |

### Node Attributes

| Attribute | Required | Description |
|-----------|----------|-------------|
| `id` | No | Unique identifier for CSS targeting (`#id`) |
| `class` | No | Style class name (`.class`) |
| `action` | No (Button only) | Action string sent to game on click |
| `full` | HealthBar only | Path to full heart texture |
| `half` | HealthBar only | Path to half heart texture |
| `empty` | HealthBar only | Path to empty heart texture |

### Registered Actions

These actions are handled by the game layer:

| Action | Effect | Screen |
|--------|--------|--------|
| `start_game` | Starts gameplay, hides menu | MainMenu |
| `quit_game` | Exits application | MainMenu |
| `open_settings` | Opens settings screen | MainMenu |
| `close_settings` | Closes settings screen | Settings |
| `resume_game` | Returns to gameplay | Pause |

To add a new action, modify `UIManager::handle_ui_click()` in `ui/runtime/ui_manager.cpp`.

---

## CSS-Lite Specification

### Selector Types

```css
/* Type selector - matches all nodes of type */
Panel { }
Button { }
Text { }

/* ID selector - matches single node by id */
#main-menu { }
#btn-play { }

/* Class selector - matches nodes with class */
.menu-button { }
.fullscreen-menu { }
```

**Selector priority** (highest to lowest):
1. ID selector (`#id`)
2. Class selector (`.class`)
3. Type selector (`NodeType`)

### Supported Properties

#### Layout Properties

| Property | Values | Default | Description |
|----------|--------|---------|-------------|
| `width` | integer | auto | Fixed width in pixels |
| `height` | integer | auto | Fixed height in pixels |
| `padding` | integer | 0 | Inner spacing (all sides) |
| `margin` | integer | 0 | Outer spacing (all sides) |
| `gap` | integer | 0 | Space between children |
| `direction` | `row`, `column` | `column` | Child layout direction |
| `grow` | `true`, `false` | `false` | Expand to fill parent |
| `anchor` | see below | `top-left` | Screen positioning |

#### Anchor Values

```
top-left      top       top-right
    ┌──────────┬──────────┐
    │          │          │
left├──────────┼──────────┤right
    │        center       │
    │          │          │
    └──────────┴──────────┘
bottom-left  bottom  bottom-right
```

#### Visual Properties

| Property | Values | Default | Description |
|----------|--------|---------|-------------|
| `background-color` | color | transparent | Background fill |
| `color` | color | white | Text/foreground color |
| `font-size` | integer | 20 | Text size in pixels |
| `border-width` | integer | 0 | Border thickness |
| `border-color` | color | gray | Border color |
| `border-radius` | integer | 0 | Corner rounding |
| `visible` | `true`, `false` | `true` | Show/hide node |

#### Color Formats

```css
/* Named colors */
color: white;
color: black;
color: red;
color: green;
color: blue;
color: yellow;
color: cyan;
color: magenta;
color: gray;
color: transparent;

/* Hex colors */
color: #fff;           /* RGB shorthand */
color: #ffffff;        /* RGB full */
color: #ffffffcc;      /* RGBA with alpha */

/* Future: rgb()/rgba() functions */
```

### CSS Comments

```css
/* Single-line comment */

/*
 * Multi-line
 * comment
 */

.selector {
  property: value; /* Inline comment */
}
```

---

## Creating a New Screen

### Step 1: Create XML Layout

Create `client/static/ui/my_screen.xml`:

```xml
<UI>
  <Panel id="my-screen" class="fullscreen-panel">
    <Text class="title">My Screen</Text>
    
    <Panel class="content">
      <!-- Screen content here -->
    </Panel>
    
    <Button action="close_my_screen" class="back-button">Back</Button>
  </Panel>
</UI>
```

### Step 2: Create CSS Styles

Create `client/static/ui/my_screen.css`:

```css
.fullscreen-panel {
  anchor: center;
  width: 600;
  height: 400;
  padding: 30;
  gap: 20;
  direction: column;
  background-color: #1a1a2ecc;
  border-radius: 12;
}

.title {
  font-size: 32;
  color: #ffffff;
}

.content {
  grow: true;
  gap: 10;
  direction: column;
}

.back-button {
  width: 150;
  height: 40;
  font-size: 18;
  background-color: #3a3a5a;
  border-radius: 6;
}
```

### Step 3: Register in UIManager (requires recompile)

In `ui/runtime/ui_manager.hpp`, add:

```cpp
UIDocument my_screen_;
bool my_screen_loaded_ = false;
```

In `ui/runtime/ui_manager.cpp`, in `init()`:

```cpp
my_screen_.set_on_click([this](const std::string& action) {
    handle_ui_click(action);
});
my_screen_loaded_ = my_screen_.load_from_files("ui/my_screen.xml", "ui/my_screen.css");
```

### Step 4: Add GameScreen enum (requires recompile)

In `ui/runtime/ui_view_model.hpp`:

```cpp
enum class GameScreen {
    MainMenu,
    Playing,
    Paused,
    Settings,
    MyScreen  // Add new screen
};
```

### Step 5: Handle navigation (requires recompile)

In `UIManager::handle_ui_click()`:

```cpp
} else if (action == "open_my_screen") {
    // Game will handle screen transition
    pending_commands_.emplace_back(OpenMyScreen{});
} else if (action == "close_my_screen") {
    pending_commands_.emplace_back(CloseMyScreen{});
}
```

---

## UIViewModel Data Bindings

The UIViewModel provides read-only game state to UI. Current bindings:

```cpp
struct UIViewModel {
    // Screen info
    int screen_width;
    int screen_height;
    float dt;
    int fps;
    GameScreen game_screen;
    
    // Player data (for HUD)
    struct {
        int health;
        int max_health;
        Vector3 position;
        Vector3 velocity;
        float yaw, pitch;
        float camera_sensitivity;
        bool on_ground;
        bool sprinting;
        bool creative;
    } player;
    
    // Network info
    struct {
        bool has_server_hello;
        bool has_join_ack;
        bool has_snapshot;
        int tick_rate;
        int world_seed;
        uint32_t player_id;
        uint32_t server_tick;
    } net;
};
```

To expose new data, modify `UIViewModel` struct and populate it in `Game::refresh_ui_view_model()`.

---

## Best Practices

### Layout Guidelines

1. **Use semantic hierarchy**:
   ```xml
   <!-- Good -->
   <Panel class="settings-group">
     <Text class="group-title">Audio</Text>
     <Panel class="group-content">...</Panel>
   </Panel>
   
   <!-- Bad: flat structure -->
   <Text>Audio</Text>
   <Button>Volume Up</Button>
   <Button>Volume Down</Button>
   ```

2. **Prefer classes over IDs** for reusable styles:
   ```css
   /* Good - reusable */
   .menu-button { width: 200; height: 50; }
   
   /* Use ID only for unique overrides */
   #btn-play { background-color: #2a5a3a; }
   ```

3. **Use consistent spacing**:
   ```css
   /* Define spacing scale */
   .gap-sm { gap: 8; }
   .gap-md { gap: 16; }
   .gap-lg { gap: 24; }
   ```

### Performance Guidelines

1. **Minimize node count** - Each node has layout/render cost
2. **Avoid deep nesting** - Flat hierarchies are faster
3. **Use visibility** - Hide unused nodes instead of removing them
4. **Cache textures** - Textures are cached automatically; reuse paths

### Debugging Tips

1. **Check logs** for load errors:
   ```
   [ui] CSS parse error in ui/my_screen.css: unexpected token at line 15
   [ui] Failed to load XML file: ui/my_screen.xml
   ```

2. **Verify files are copied** to build directory:
   ```bash
   ls -la build/ui/
   ```

3. **Test CSS in isolation** - Use simple selectors first

4. **Use visible backgrounds** to debug layout:
   ```css
   .debug { background-color: #ff000044; }
   ```

---

## Future: Lua Scripting (Planned)

### Design Goals

1. **Dynamic behavior** without recompiling:
   - Animations
   - Conditional visibility
   - Data formatting
   - Complex interactions

2. **Safe sandbox**:
   - No file system access
   - No network access
   - Limited memory/CPU budget
   - Timeout on long scripts

3. **Integration points**:
   ```lua
   -- ui/scripts/main_menu.lua
   
   function on_enter()
     -- Called when screen becomes active
     ui.animate("#title", "fade_in", 0.5)
   end
   
   function on_update(dt)
     -- Called every frame
     local health = vm.player.health
     ui.set_visible("#low-health-warning", health < 5)
   end
   
   function on_click(action)
     -- Called before C++ handler
     if action == "start_game" then
       ui.play_sound("menu_select")
     end
     return true -- continue to C++ handler
   end
   ```

### Proposed API

```lua
-- UI manipulation
ui.set_text(selector, text)
ui.set_visible(selector, bool)
ui.set_style(selector, property, value)
ui.animate(selector, animation, duration)

-- Read-only game state
vm.player.health
vm.player.position
vm.game_screen
vm.fps

-- Events
ui.play_sound(name)
ui.emit_command(command_name, params)

-- Utilities
ui.format_time(seconds)
ui.format_number(value)
```

### File Association

```xml
<!-- In XML, reference script -->
<UI script="scripts/main_menu.lua">
  ...
</UI>
```

---

## Appendix: Quick Reference

### Common Patterns

**Centered modal dialog:**
```xml
<Panel class="modal">
  <Text class="modal-title">Title</Text>
  <Panel class="modal-content">...</Panel>
  <Panel class="modal-buttons">
    <Button action="confirm">OK</Button>
    <Button action="cancel">Cancel</Button>
  </Panel>
</Panel>
```
```css
.modal {
  anchor: center;
  width: 400;
  height: 300;
  padding: 20;
  gap: 15;
  direction: column;
  background-color: #2a2a3acc;
  border-radius: 10;
  border-width: 2;
  border-color: #4a4a5a;
}
```

**Horizontal button row:**
```xml
<Row class="button-row">
  <Button action="prev">Previous</Button>
  <Spacer />
  <Button action="next">Next</Button>
</Row>
```
```css
.button-row {
  direction: row;
  gap: 10;
  padding: 10;
}
```

**HUD element (bottom-left):**
```xml
<Panel class="hud-panel">
  <HealthBar full="textures/ui/heart_full.png" ... />
</Panel>
```
```css
.hud-panel {
  anchor: bottom-left;
  margin: 20;
  padding: 10;
}
```

### Color Palette

```css
/* Recommended UI colors */

/* Backgrounds */
--bg-dark: #1a1a2e;
--bg-panel: #2a2a3a;
--bg-button: #3a3a5a;

/* Accents */
--accent-green: #2a5a3a;
--accent-red: #5a2a3a;
--accent-blue: #3a3a7a;

/* Text */
--text-primary: #ffffff;
--text-secondary: #888899;
--text-muted: #666677;

/* Borders */
--border-light: #5a5a7a;
--border-dark: #4a4a5a;
```

### Checklist for New UI

- [ ] XML file created in `client/static/ui/`
- [ ] CSS file created in `client/static/ui/`
- [ ] Files use correct syntax (check parser logs)
- [ ] Actions are registered in `handle_ui_click()`
- [ ] Screen enum added to `GameScreen` (if new screen)
- [ ] UIDocument loaded in `UIManager::init()`
- [ ] Screen rendered in `UIManager::render()`
- [ ] Navigation triggers added to game logic
- [ ] Tested at different resolutions
