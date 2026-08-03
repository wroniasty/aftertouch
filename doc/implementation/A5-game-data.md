# A5 — Game data

Team, player, tactics and career-seed readers for **our** on-disk schema; the
fictional default league a fresh clone can play. Excludes art packs (A4), SQLite
career persistence (D2), the rich career player model (E1), and SWOS file formats
— original team data may be imported later behind the same schema, never shipped.

Depends on: A4   Blocks: B9, B11, D2, E1   Wave: 3

---

## 0. One-paragraph version

[../DATA.md](../DATA.md) is reference-only: we need what a football game stores, not
SWOS's bytes. A5 defines a little-endian `ATGD` container (same discipline as A4's
`ATAP`: pure span encode/decode, offsets not pointers, fingerprinted, versioned),
in-memory records whose attributes are **0–7**, tactics as the 10×35 zonal grid
shape stolen from [../DATA.md](../DATA.md) §4, and a career snapshot that **references
team ids** rather than embedding mutated copies ([../DATA.md](../DATA.md) §8). A
committed-in-code fictional league of eight teams round-trips through the format and
projects two sides into `MatchState` as the engine's 0–7 view — the wall E1 will
later extend.

> **Range corrected to 0–7** by [B13](B13-amiga-oracle.md) / R2. `AdjustPlayerSkills`
> masks the packed longword with `$07777777` — three bits per nibble — so the high bit
> is discarded on load and a stored 8 reads as 0. The 0–15 reading, and the conclusion
> that four attribute-indexed tables were undersized, are both withdrawn:
> eight-entry tables are correctly sized. See [../AMIGA_CHANGES.md](../AMIGA_CHANGES.md) §2.1.

---

## 1. Scope

**In:**

- `src/data/`: types, `ATGD` encode/decode/validate, `ApplyKickoff`, fictional league.
- Engine-visible projection fields on `MatchState` (attrs, shirt, position, kits, names).
- Career snapshot schema that references teams by id (seed / interchange), not SQLite.
- Tests that pin round-trip, validation, attribute bounds, and kickoff projection.

**Out:**

| Excluded | Owner |
|---|---|
| Sprite / pitch / palette packs | A4 |
| SQLite save games and migrations | D2 |
| Age, ceilings, form, contracts, continuous abilities | E1 |
| Reading SWOS `team.*` / `.tac` as a runtime path | optional importer later |
| Off-ball AI that *consumes* tactics | B9 |
| Full `TeamGame` / arena layout | B1 |

---

## 2. Design

### 2.1 Why our own format

[../DATA.md](../DATA.md) is explicit: not an implementation basis. Shipping SWOS's
684-byte records would put a derivative of a copyrighted binary in the repo and glue
us to packing bugs we do not want. The useful findings are the **attribute range**, the
**tactics grid shape**, and **roles separate from players**.

### 2.2 Records

- **Attributes:** seven values, each 0–7: passing, shooting, heading, tackling,
  ball control, speed, finishing. Stored as whole bytes on disk with a validator that
  rejects >7 — packing is optional and not worth the opacity.
- **Position:** 0 GK, then RB, LB, D, RW, LW, M, A (same ordinals as the reference notes).
- **Face:** 0–2. **Shirt type:** 0–3. **Kit colours:** 0–9 (palette ordinals live in
  `palette.atl` from A4).
- **Tactic:** name, `out_of_play` index, `cells[10][35]` — when the ball is in quadrant
  *n*, role *r* aims at the packed `(x,y)` cell on the 16×15 grid. Keeper excluded.
  Authored as if the upper goal is ours; B9 mirrors for the second side.
- **Team:** stable `id`, name, coach, tactic id, tier, two kits, sixteen players
  (eleven + five).
- **Career snapshot:** season year, game type, balances, ordered team ids. No embedded
  team blobs — D2 / E1 own mutation.

### 2.3 Container (`ATGD`)

One file can hold a league (tactics + teams) or a career snapshot. Header is 48
bytes: magic, version, kind, section offsets/sizes, reserved, fingerprint.
Little-endian explicit writes. `Validate` rejects truncated or out-of-bounds
sections the same way A4 does.

### 2.4 Projection into `MatchState`

`ApplyKickoff(league, home_id, away_id, state)` copies each side's first eleven into
slots 0–10 and 11–21, fills parallel attribute / shirt / position arrays, team names
and kits, and resets phase to kickoff. It does **not** place players on the pitch —
that is B4. Career fields never enter `MatchState`.

### 2.5 Fictional default

Eight teams, three tactics (4-4-2, 5-3-2, 3-5-2), invented names. Built by
`MakeFictionalLeague()` so a clean clone needs no generator and no original install.
Names avoid "SWOS", "Sensible", and real club marks ([../PLAN.md](../PLAN.md) header).

---

## 3. Interfaces

| Path | Role |
|---|---|
| `src/data/include/data/game_data.hpp` | Types, encode/decode, validate, `ApplyKickoff` |
| `src/data/include/data/fictional.hpp` | `MakeFictionalLeague()` |
| `src/core/include/core/match_state.hpp` | Projected squad fields only |

Wall: `src/data/` does no SDL/ImGui and performs no file I/O in the library — the
caller supplies bytes. Nothing under `src/data/` is included from match tick code
except the projection result already sitting in `MatchState`.

---

## 4. Work items

1. **Types + `ATGD` format** — header, tactic/team/player/career layout, validate. →
   `test_game_data.cpp` (round-trip, reject malformed).
2. **`MatchState` projection fields** — attrs and sheet metadata. → compile + A3 still
   green (per-tick record unchanged).
3. **`ApplyKickoff`** — two teams → 22 slots. → acceptance test.
4. **`MakeFictionalLeague`** — eight teams, three tactics, attrs in 0–7. → round-trip
   the whole league through `ATGD`.
5. **Career snapshot** encode/decode with team-id references only.

---

## 5. Tests and acceptance

| Test | Pins |
|---|---|
| `test_game_data.cpp` — format | League and career round-trip; wrong magic/version/truncated rejected; attr >15 rejected. |
| `test_game_data.cpp` — kickoff | Fictional league → `ApplyKickoff` fills 22 players, names, kits; attrs all ≤15. |
| `test_game_data.cpp` — fiction | Eight teams, sixteen players each, three tactics; encode→decode equals. |

**Done when:** a league of fictional teams loads into `MatchState` and round-trips
through our format — the criterion in [PLAN.md](PLAN.md).

---

## 6. Open questions

- **SWOS team importer** as a second `assetc`-style tool: deferred until someone wants
  community season updates; schema is ready to receive the projection.
- **Whether shipped original teams use attrs above 7** ([../DATA.md](../DATA.md) §7):
  measurement, not format — belongs in [../LEGACY.md](../LEGACY.md) §15 when measured.
- **Default tactic grids:** hand-authored placeholders until B9 tunes them against feel;
  shape is fixed, cell values are starting guesses.
