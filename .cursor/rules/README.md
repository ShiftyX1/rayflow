# Cursor Rules for Rayflow

This directory contains project-specific rules for Cursor AI assistant in the modern `.cursor/rules/` format.

## Structure

Each rule is in its own directory with a `RULE.md` file:

```
.cursor/rules/
├── main/               # Main project rules (summary of all)
├── architecture/       # Authoritative server + thin client
├── protocol-transport/ # Transport and protocol specs
├── server-simulation/  # Server tick loop, validation, determinism
├── dedicated-server/   # RFDS dedicated server implementation
├── scripting/          # Lua scripting system (two-tier: user + engine)
├── comments/           # Commenting conventions
├── bedwars-gameplay/   # BedWars gameplay systems
├── migration-plan/     # Migration stages A-D
├── lighting-minecraft/ # Minecraft-style 0..15 lighting
├── lighting-baked/     # Baked lighting (legacy spec)
├── map-editor/         # Map templates and editor tool
├── testing/            # Testing guidelines
├── gui/                # GUI approach overview
├── ui-framework/       # UI framework details for AI agents
├── resources/          # VFS and asset packaging (RFPK)
├── build-system/       # CMake, dependencies, CI/CD
└── config/             # Configuration file format
```

## Usage

Cursor automatically loads these rules when working in the project. You can reference specific rules by name in chat or let Cursor find relevant rules based on context.

## Legacy

The original instruction files in `.github/instructions/` are kept for reference but are superseded by these rules. The content has been migrated without changes to the actual specifications.

## Updating Rules

To update a rule:
1. Edit the `RULE.md` file in the corresponding directory
2. Keep content concise and focused
3. Use Markdown formatting
4. If adding a new rule, create a new directory with `RULE.md`

## Rule Categories

| Category | Rules |
|----------|-------|
| **Architecture** | main, architecture, migration-plan |
| **Server** | server-simulation, protocol-transport, scripting, dedicated-server |
| **Client** | gui, ui-framework, resources |
| **Gameplay** | bedwars-gameplay, lighting-minecraft, lighting-baked, map-editor |
| **Development** | testing, build-system, config, comments |

