# B1 — State & layout

Entity structs, the fixed arena, the citation bridge from [../STATE.md](../STATE.md),
and the accessor discipline every later B part writes against. Excludes tick order
and the match state machine (B2), physics (B3), movement (B4), and every behaviour
that mutates these fields.

Depends on: A2   Blocks: B2–B10   Wave: 2

---

## 0. One-paragraph version

[../STATE.md](../STATE.md) is a DOS memory map; we keep its design facts and discard
its offsets. B1 replaces the A2/A5 `MatchState` stub with a pointer-free arena —
ball, 22 players, referee, booked-number indicator — plus per-side `TeamControl`
(intent/input), 16-slot squads, a tactics snapshot copied at kickoff, and the named
match globals B2 will drive. Named STATE.md fields only; unnamed bytes and
`unknownTail` are omitted until a behaviour doc needs them. ATTR bumps to v2 so the
full state round-trips field-by-field. No gameplay lands here.

---

## 1. Scope

**In:**

- `Entity` (Sprite semantics), `TeamControl` (TGI), `SquadPlayer` (PlayerInfo),
  `TacticsSnapshot`, `MatchGlobals`, `MatchSide`, accessors on `MatchState`.
- Citation bridge in this subfile (`Sprite.speed (+44)` → `Entity::speed`).
- `ApplyKickoff` filling squads[16], tactics cells, and pitch identity fields.
- ATTR format version 2: lossless serialise of the full `MatchState` (+ input).
- `HashState` over the new layout (`presentation_rng` still excluded).
- Tests that pin unique-rep, accessors, kickoff projection, and ATTR round-trip.

**Out:**

| Excluded | Owner |
|---|---|
| `game_state` transitions, clock, period end, statistics behaviour | B2 |
| Ball friction / bounce / predictors as behaviour | B3 |
| Starting positions and movement pipeline | B4 |
| Possession, kicks, contests, set pieces, AI | B5–B9 |
| Device → seven input fields | B10 |
| Unnamed `field_*`, `TeamGame::unknownTail`, `shotChanceTable` | omit until needed |
| Physics constants (`kGravityConstant`, pitch factors) | A2 profile / B3 |
| First-launch asset prompt | A4 residual / D1 |
| Stub-oracle → real SWOS ATTR | A3 follow-up |

---

## 2. Design

### 2.1 Responsibility split

| Struct | Owns |
|---|---|
| `Entity` | Physics, animation cursor, per-entity flags (cards, tackle, heading contact) |
| `TeamControl` | Intent, seven-field input surface, proximity bands, aftertouch timers, AI scratch |
| `SquadPlayer` | Identity, names, 0–15 attrs, shirt/position/face |
| `TacticsSnapshot` | `out_of_play` + `cells[10][35]` — copy at kickoff so the tick never includes `src/data/` |
| `MatchGlobals` | Restart/ref/bench/predictor *storage* (values only; machines are later parts) |

### 2.2 Arena slots

Fixed indices, never pointers:

| Index / member | Role |
|---|---|
| `ball` | The ball |
| `players[0..10]` | Home first eleven |
| `players[11..21]` | Away first eleven |
| `referee` | Referee sprite (`team_number == 3`) |
| `booked_indicator` | Booked-number indicator |

`TeamControl::controlled_slot` / `pass_to_slot` / contest slots are `int8_t` into
`players[]`, or `-1` for none.

### 2.3 Unknown-field policy

Carry every STATE.md field that has a **real name** and a documented reader or
writer. Omit unnamed `field_*`, the 686-byte `unknownTail`, dead globals
(`injuriesForever`), and unread pointer tables. Do not invent names. If a later
part discovers a write site for an omitted byte, add it then with the assembly name.

### 2.4 Attributes

Range **0–15** (A5 / [../DATA.md](../DATA.md) §3). B1 only stores bytes. Table
overrun policy for attribute-indexed tables remains for B7 / LEGACY.

### 2.5 ATTR v2

Per-tick record is field-by-field little-endian of the **full** serialisable
`MatchState` plus `MatchInput`, then FNV-1a over the payload. `presentation_rng`
is on the wire for replay fidelity; `HashState` still omits it (A2 §2.6). Format
version and stride are checked on load — v1 files are rejected.

### 2.6 Citation bridge (offset map)

Not a runtime table. When a behaviour doc cites a DOS offset, map it here:

| Reference | Our field |
|---|---|
| `Sprite.x/y/z (+30…)` | `Entity::pos` |
| `Sprite.deltaX/Y/Z (+46…)` | `Entity::delta` |
| `Sprite.destX/Y (+58)` | `Entity::dest_x/y` |
| `Sprite.direction (+42)`, `speed (+44)` | `Entity::direction`, `speed` |
| `Sprite.fullDirection (+82)` | `Entity::full_direction` |
| `Sprite.playerState (+12)` | `Entity::player_state` |
| `Sprite.heading (+98)` | `Entity::heading` |
| `Sprite.cards (+102)` | `Entity::cards` (`-1` = sent off) |
| `TeamGeneralInfo` input block (+42…+54) | `TeamControl` seven fields + `fire_counter` |
| `controlledPlDirection (+56)` | `TeamControl::controlled_pl_direction` (one field; also spin-table index) |
| Proximity bands (+61…+69) | `TeamControl::pl_*` / `ball_*` |
| `spinTimer` / `leftSpin` / `rightSpin` | `TeamControl::spin_*` |
| `PlayerInfo` attrs (+27…+33) | `SquadPlayer::attrs` |
| `gameState`, `foulX/Y`, `ballNextX/Y`, ref/bench globals | `MatchGlobals` |

---

## 3. Interfaces

| Path | Role |
|---|---|
| `src/core/include/core/match_state.hpp` | Types, arena, accessors |
| `src/core/include/core/hash.hpp` | `HashState` over new fields |
| `src/core/include/core/trace.hpp` | ATTR v2 serialize/deserialize |
| `src/data/.../game_data.hpp` | `ApplyKickoff` projection |

**Wall:** still no SDL/I/O/float/clock in `src/core/`. Accessors allocate nothing.
`MatchEngine::Step` remains the A2 stub (adapted to `Entity::delta` /
`full_direction` only).

Accessors: `Ball()`, `Player(i)`, `Referee()`, `BookedIndicator()`, `Side(s)`,
`Controlled(s)`, `Squad(s, i)`.

---

## 4. Work items

1. **Types + accessors + HashState** — compile; A2 stub Step still runs.
2. **`ApplyKickoff`** — 16×2 squad, tactics snapshot, pitch identity; season runner
   applies sheet into the engine after `Reset`.
3. **ATTR v2** — full-state record; tracediff field classes updated.
4. **Tests** — `test_match_state`, trace round-trip, re-pin determinism hash, refresh
   golden/corpus engine traces.
5. **PLAN-CURRENTSTATE** — Wave 2 / B1 status.

---

## 5. Tests and acceptance

| Test | Pins |
|---|---|
| `test_match_state.cpp` | unique-rep; accessors; default zeros; kickoff identity on pitch slots |
| `test_trace.cpp` | ATTR v2 lossless round-trip of a fully populated state |
| `test_hash.cpp` / `test_determinism.cpp` | Hash sensitivity; committed cross-platform hash re-pinned |
| `test_game_data.cpp` | ApplyKickoff fills squads + tactics; attrs ≤15 |
| A3/A6 consumers | Green against v2 stride |

**Technique (§7):** unit + golden/trace round-trip. Invariant added to the always-on
set: state serialises and round-trips losslessly (engine invariant in PLAN §7).

**Done when:** a busy post-kickoff `MatchState` serialises to an ATTR record and
deserialises bit-identically in `core_tests` on CI.

---

## 6. Open questions

- **Attribute-indexed table bounds** when values exceed 7 — B7 / LEGACY §15; not layout.
- **`TeamStatsData` layout** — B2/B12 when statistics behaviour lands.
- **Whether `saveSprite` is replay-related** — carry the field; purpose deferred.
- **Real SWOS reference ATTR** replacing stub-oracle corpus refs — A3 follow-up; not a
  B1 gate.
- **`PlayerPosition` enum beyond substituted** — A5 ordinals used; expand if needed.
