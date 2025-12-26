# GUI approach for Rayflow

All UI below must live in `ui/` and be driven by game state. UI must never own gameplay truth; it only renders, collects input, and sends requests to the game layer.

> **For detailed AI agent instructions, see `ui-framework` rule**

## Quick Reference: What Requires Recompile?

| Change Type | Recompile? | Files to Modify |
|-------------|------------|-----------------|
| Layout (add/move nodes) | No | `client/static/ui/*.xml` |
| Styles (colors, sizes) | No | `client/static/ui/*.css` |
| Textures | No | `client/static/textures/ui/` |
| New node type | **Yes** | `ui/runtime/xmlui/ui_document.cpp` |
| New CSS property | **Yes** | `ui/runtime/xmlui/css_lite.cpp` |
| New action handler | **Yes** | `ui/runtime/ui_manager.cpp` |
| New game screen | **Yes** | Multiple files (see guide) |
| New view model data | **Yes** | `ui/runtime/ui_view_model.hpp` |

## Phases
- Phase 1 (Debug UI): use raygui for developer-only panels.
- Phase 2 (Player UI): build a custom retained-mode UI (XML + styles) for the actual player-facing interface.

## Core rules
- Separation:
  - debug_ui/ (raygui) is isolated from game_ui/ (custom).
  - Gameplay systems never depend on raygui types or calls.
- State ownership:
  - UI reads from a read-only view-model/state snapshot.
  - UI writes only via explicit commands/messages (e.g., UICommand::StartGame, UICommand::SetVolume).
- Input capture:
  - UI must expose wants_mouse / wants_keyboard (or a single captured_input flag).
  - If UI captures input, gameplay input must not react that frame.
- Determinism:
  - UI must not directly mutate simulation state mid-tick; queue commands and apply them at safe points.

## Debug UI (raygui)
- Purpose: quick iteration, debug toggles, profiling overlays, cheats (dev only).
- Requirements:
  - Toggleable at runtime (F1/F2) and removable via build flag (#ifdef DEBUG_UI).
  - Must not ship in release builds unless explicitly enabled.
  - May directly read engine internals for diagnostics, but must not become required for gameplay flow.

### Debug UI modes (required)
Debug output must support **two mutually exclusive modes**:

1) **Interactive debug UI**
- Shows windows/panels with controls (sliders, checkboxes, etc.).
- **Captures mouse/keyboard** while open (gameplay input must not react).
- Intended for developers to tweak settings.

2) **Overlay-only debug HUD**
- Shows informational panels/text over the game view.
- **Does not capture input** (mouse-look and gameplay must keep working).
- No windows and no settings; purely read-only diagnostics.
- Typical panels:
  - FPS/frametime, camera, entity inspector, spawn tools, lighting, navmesh, net stats.

## Player UI (custom XML UI)
- UI definition
  - Layout lives in `client/static/ui/*.xml` and is copied to `build/ui/*.xml` on build.
  - Styles live in `client/static/ui/*.css` and are copied to `build/ui/*.css` on build.
  - Optional scripts later (assets/ui/*.lua) but not required for MVP.
- Retained tree
  - UI is a persistent tree of nodes (Panel, Text, Button, Image, Row, Column, Spacer…).
  - Nodes have: id, class, layout params, computed rect, and interaction state (hover/pressed/focus).
- Layout scope
  - MVP layout supports only:
  - Row / Column stacking
  - padding/margin/gap
  - fixed sizes + "grow"
  - anchors (top/center/bottom, left/center/right)
  - Avoid full HTML/CSS flexbox/grid complexity.

### Current implementation (Dec 2025)

#### Runtime
- Implemented minimal XML + CSS-lite runtime in `ui/runtime/xmlui/`.
- XML parsing: `tinyxml2`.
- Style selectors supported: type (`HealthBar`), id (`#hp`), class (`.hud-hp`).
- CSS-lite properties supported (MVP):
  - `width`, `height` (integers; used for scaling UI sprites)
  - `gap` (integer)
  - `margin`, `padding` (integers; currently `margin` is used for anchored placement)
  - `anchor` values: top-left, top, top-right, left, center, right, bottom-left, bottom, bottom-right

#### Current player HUD specification
- Entry files:
  - `ui/hud.xml`
  - `ui/hud.css`
- Nodes:
  - `HealthBar` (only player-facing node implemented so far)
    - Attributes: `full`, `half`, `empty` (texture paths)
    - Renders a heart row (2 HP per heart) using `UIViewModel::player.health/max_health`
    - Supports scaling via CSS `width/height`
- Assets:
  - `textures/ui/health_bar/full.png`
  - `textures/ui/health_bar/half.png`
  - `textures/ui/health_bar/empty.png`
- Rendering
  - Use raylib draw calls only (no embedded browser/CEF).
  - Text rendering must be cached where possible (glyph/layout caching) to avoid per-frame heavy work.
- Events
  - UI emits high-level events/commands:
  - onClick, onChange, onNavigate, onSubmit
  - Event handlers map to UICommands handled by the game layer.
- Navigation
  - Must support mouse + keyboard/gamepad focus navigation.
  - Focus order is explicit or derived from tree order; never random.
- Asset pipeline & iteration
  - Prefer hot iteration, but not mandatory hot reload.
  - Minimum: "safe reload" for UI assets:
  - Rebuild UI tree from XML/CSS when re-entering menu or on a developer key (F5), allowed to reset UI state.
  - Keep UI assets versioned and validate on load (fail gracefully with error overlay).
- Safety constraints
  - UI must not:
  - allocate unbounded memory per frame
  - block the frame on file IO (load on transitions)
  - crash the game if an asset is missing (show fallback + log)
- UI must be resolution/DPI aware:
  - design coordinate space + scaling policy (e.g., reference resolution 1920×1080 + scale).

## Directory structure
- ui/debug/ — raygui panels and debug overlays
- ui/runtime/ — custom UI core (node tree, style, layout, events)
- ui/screens/ — screen controllers (MainMenuUI, PauseUI, HUDUI, InventoryUI)
- client/static/ui/ — XML layouts + CSS-lite styles (source, copied to build/ui/)

## Acceptance criteria
- Debug UI can be toggled and removed from release.
- Player UI can build a main menu from XML+styles, with clickable buttons and keyboard/gamepad navigation.
- All gameplay state changes triggered by UI go through commands and are validated by the game layer, not by UI.

