```instructions
# Configuration (rayflow.conf) — strict spec

## Purpose
Define a small, stable configuration surface for the engine/game.

- File: `rayflow.conf` (repo root)
- Format: INI-like
- Scope: **client-owned config** for input, logging, diagnostics, and render-only toggles.

This config must never change authoritative gameplay outcomes.

## Format
- Sections are declared as `[section_name]`.
- Entries are `key = value`.
- Whitespace around keys/values is ignored.
- Comments start with `#` or `;` (from the comment character to end of line).
- Unknown sections/keys are ignored (forward-compatible).

## Loading & precedence
- Defaults are set in code (see `core::Config::Config()` in `client/core/config.cpp`).
- If `rayflow.conf` exists, values are merged over defaults.
- If `rayflow.conf` is missing, the game runs with defaults.
- No runtime hot-reload (unless explicitly added later).

## Value types
### Boolean
Accepted values (case-insensitive):
- true: `1`, `true`, `yes`, `on`
- false: `0`, `false`, `no`, `off`

### Integers
- Base-10 integers.

### Floats
- Decimal floats (e.g. `4.0`).

### Strings
- Strings may be unquoted or quoted with `'` or `"`.

## Sections & keys

### [controls]
Keyboard/mouse bindings.

Keys:
- `forward`, `backward`, `left`, `right`
- `jump`, `sprint`, `fly_down`
- `toggle_creative`, `exit`
- `primary_mouse`, `secondary_mouse`
- `tool_1` .. `tool_5`

Notes:
- Keys accept readable names like `W`, `SPACE`, `ESCAPE`, `LEFT_CONTROL`, etc.
- Mouse accepts `MOUSE_LEFT`, `MOUSE_RIGHT`, `MOUSE_MIDDLE`.

### [logging]
Client logging to stderr and optional file.

Keys:
- `enabled` (bool)
- `level` (string or int): `all|trace|debug|info|warning|error|fatal|none`
- `file` (string, optional)
- `collision_debug` (bool, rate-limited)

### [debug]
Debug toggles that should not affect gameplay.

Keys:
- `collision` (bool) — alias for `logging.collision_debug`

### [sv_logging]
Logging filters for the embedded authoritative server (still configured via client config).

Keys:
- `enabled` (bool)
- tag filters (bool): `init`, `rx`, `tx`, `move`, `coll`

Rules:
- Must be rate-limited on the server side.
- Must not allocate per-tick.

### [profiling]
Client-only performance logging.

Keys:
- `enabled` (bool)
- `log_every_event` (bool)
- `log_interval_ms` (int)
- feature toggles (bool): `light_volume`, `chunk_mesh`, `upload_mesh`
- thresholds (float, ms): `warn_light_volume_ms`, `warn_chunk_mesh_ms`, `warn_upload_mesh_ms`

### [render]
Client rendering toggles. These are **visual only**.

Keys:
- `voxel_smooth_lighting` (bool)

Semantics:
- When `true`:
  - Voxel chunk meshes use smooth per-corner sky/block light sampling.
- When `false`:
  - Voxel chunk meshes keep AO, but use flat per-face sky/block light (no per-corner smoothing).

Use-cases:
- Diagnose diagonal light leaks at non-perfect corners.
- Compare performance/visual quality.

## Hard rules (must not break)
- Config must not introduce client authority over gameplay or simulation.
- Client config must not require `server/` or `shared/` to include raylib.
- Unknown keys must be ignored (compatibility).

## Example
```
[render]
voxel_smooth_lighting = true

[profiling]
enabled = false
warn_chunk_mesh_ms = 6.0
```

```
