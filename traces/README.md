# Live match traces

After a MATCH (ESC or full time), `aftertouch` writes:

- `match_<seed>.atin` — input log (seed + per-tick stick/fire)
- `match_<seed>.txt` — sparse agent-readable transcript (paste into chat).
  Each emitted tick includes `Hpos:` / `Apos:` for all 22 players
  (`slot@(x,y)`, `*` = controlled, `B` = has ball).

The `.txt` is captured from the live engine (ApplyKickoff bootstrap).  
Regenerating ATTR from the `.atin` alone uses a bare `Reset` path and will
not bit-match the live session — use the `.txt` for agent context.

Offline from corpus / ATTR:

```
tracegen --atin tests/corpus/kickoff/input.atin --transcript out.txt --atin-only
tracegen --attr tests/corpus/kickoff/engine.attr --transcript out.txt
```
