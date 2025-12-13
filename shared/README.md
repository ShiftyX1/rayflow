# shared/

Shared, transport-agnostic definitions used by both client and server.

Planned contents:
- Protocol message types + versioning
- IDs (PlayerId/EntityId/MatchId)
- Enums/constants (blocks/items/teams/rejection reasons)
- Serialization helpers

Rule: `client/` and `server/` may depend on `shared/`, but `client/` must never depend on `server/`.
