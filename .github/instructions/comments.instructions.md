# Commenting & Documentation Specification

## Goals
- Keep comments **rare, precise, and durable**.
- Prefer clear code and good names over commentary.
- Document **intent, constraints, invariants, and non-obvious tradeoffs**.

## Language
- Use **English** for code comments and docstrings (matches existing codebase + protocol/docs).

## Where comments are allowed/required
### Required
- Public APIs in `shared/` (protocol types, serialization, IDs/enums): brief Doxygen-style comments.
- Server validators and rules where a check is non-obvious (e.g., anti-cheat constraints, bounds, template protection): short intent comments.
- Any non-trivial algorithm (noise/shuffle, hashing, collision resolution): one short summary comment + key invariants.

### Allowed
- `// NOTE:` for subtle behavior or caveats.
- `// TODO(owner|area): ...` for tracked follow-ups.
- `// FIXME:` for known bugs with a clear symptom.
- `// HOTPATH:` to mark tick-critical code where allocations/logging are prohibited.

### Avoid
- Comments that restate the code ("increment i", "loop over blocks").
- Large blocks of prose inside `.cpp`.
- Commented-out code.

## Style
- Prefer `//` for single-line comments.
- Use Doxygen blocks only for public-facing declarations:
  - `/** Brief summary. */`
  - `@param`, `@return` only when it adds value.
- Keep line length reasonable (~100).
- Keep comments **close to the thing they describe**.

## Determinism & Simulation rules (comment policy)
- If code touches determinism-sensitive behavior (RNG, time, ordering), add a short `// NOTE:` explaining the invariant.
- Never introduce `std::rand`/`std::srand` in simulation; if changing RNG, comment that it must remain deterministic and locally seeded.
