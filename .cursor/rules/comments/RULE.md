# Commenting & Documentation Specification

## Goals
- Keep comments **rare, precise, and durable**.
- Prefer clear code and good names over commentary.
- Document **intent, constraints, invariants, and non-obvious tradeoffs**.

## Language
- Use **English** for code comments and docstrings.

## Comment Styles

### File Header (engine public API)
For key engine headers that define public interfaces:
```cpp
// =============================================================================
// ComponentName - Brief one-line description
// Second line if needed for context.
// =============================================================================
```

### Section Dividers (within files)
For grouping related code within a file:
```cpp
// --- Section Name ---
```

### Doxygen (public API methods)
```cpp
/// Brief description of the method.
/// @param name Description (only when non-obvious).
/// @return Description (only when non-obvious).
virtual void send(std::span<const std::uint8_t> data) = 0;
```

### Inline Comments
```cpp
// NOTE: Explanation of non-obvious behavior
// TODO(area): Tracked follow-up task
// HOTPATH: No allocations allowed here
```

## Where comments are required

### Required
- Public API interfaces in `engine/` — file header + Doxygen on methods
- Game protocol types in `games/*/shared/` — brief Doxygen
- Server validators with non-obvious checks — short intent comment
- Non-trivial algorithms — summary + key invariants

### Allowed
- `// NOTE:` for subtle behavior or caveats
- `// TODO(area):` for tracked follow-ups (with owner/area)
- `// FIXME:` for known bugs with clear symptom
- `// HOTPATH:` for tick-critical code

### Avoid
- Comments that restate the code ("increment i", "loop over blocks")
- Large prose blocks inside `.cpp`
- Commented-out code
- `// DEPRECATED:` comments — use `[[deprecated("reason")]]` attribute instead

## Style Rules
- Prefer `//` for single-line, `///` for Doxygen
- Keep line length ~100 chars
- Keep comments close to the thing they describe
- Remove comments when code becomes self-explanatory

## Determinism Notes
- If code touches determinism (RNG, time, ordering), add `// NOTE:` explaining invariant
- Never use `std::rand`/`std::srand` in simulation

