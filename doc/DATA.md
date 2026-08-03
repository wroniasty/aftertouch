# DATA.md

The on-disk formats: teams, players, tactics, career saves, replays and highlights.
What the original stored, how it was packed, and the one packing decision that
turns out to explain a gameplay bug. Traced through the reference DOS port in
[../reference/swos-port/](../reference/swos-port/) and the porters' own
reverse-engineering notes in `docs/SWOS/`.

> **Reference only — not an implementation basis.** aftertouch builds its own engine
> and will not read SWOS's file formats. This document exists for three reasons:
> knowing what data a game of this kind actually needs; being able to *import*
> original team data later if we choose to; and recognising where a storage decision
> explains an otherwise baffling gameplay behaviour. §3 is the case where it does,
> and it is the one section here that changes what we build.
>
> **Provenance.** Unusually good. `docs/SWOS/teams.txt`, `tactics.txt` and
> `career.txt` are the porters' detailed field-by-field notes on the real files,
> cross-checked here against the C++ structs in
> [swos.h](../reference/swos-port/src/swos/swos.h). Where the notes hedge
> (`???`, "I'm guessing"), so does this document.

---

## 0. One-paragraph version

A team file (`team.NNN` in `\data`) is a big-endian team count followed by fixed
**684-byte team records**: a 76-byte header — name, tactics index, league, two kit
colour sets, coach — and **sixteen 38-byte player records**. Player attributes are
**packed two to a byte as 4-bit nibbles**, but the engine masks each nibble to three
bits on load, so the **used range is 0–7** — the fact that matters (§3). Tactics are separate **370-byte files**: a
name plus ten players × 35 bytes, each byte a packed `(x, y)` grid quadrant saying
"when the ball is *here*, stand *there*" — the entire off-ball AI as 350 bytes of
data. Career saves are the same 684-byte team records in bulk followed by a large,
mostly unmapped block of finances and competition state. Replays and highlights
share one format (`HIL2`), replays simply being a highlight that covers the whole
match.

---

## 1. Team files

`team.NNN` in `\data`. Maximum 64,000 bytes — the size of SWOS's fixed load buffer;
**anything larger terminates the game immediately**. Teams are sorted
alphabetically within a file.

```
word (big endian)   number of teams
team record × N     684 bytes each
```

`team.080`–`team.085` hold national teams by continent: 80 Europe, 81 Africa,
82 South America, 83 North America, 84 Asia, 85 Australia and Oceania.

**Team header — 76 bytes** (`TeamFileHeader` in
[swos.h](../reference/swos-port/src/swos/swos.h)):

| Off | Size | Field | Notes |
|---|---|---|---|
| 0 | 1 | country number | The `NNN` of the filename; indexes a translation table |
| 1 | 1 | team ordinal | Position within the file |
| 2 | 2 | global team number | Translation table result + ordinal; **unique across all of SWOS** |
| 4 | 1 | team status | 0 unselected, 1 computer, 2 player-coach, 3 coach. Irrelevant in the file |
| 5 | 17 | team name | Null-terminated |
| 23 | 1 | — | *"AND this with 0xFE"* |
| 24 | 1 | tactics | Index, §2 |
| 25 | 1 | league | 0 premier, 1 first, 2 second, 3 third, 4 non-league |
| 26 | 5 | primary kit | shirt type, stripes, basic, shorts, socks |
| 31 | 5 | secondary kit | same |
| 36 | 23 | coach name | |
| 59 | 1 | some flag | |
| 60 | 11 | player numbers | |

Shirt type: 0 plain, 1 coloured sleeves, 2 vertical stripes, 3 horizontal stripes —
the four patterns [PLAYER_SPRITES.md](PLAYER_SPRITES.md) §3 composites at runtime.

**The global team number is worth noting**: a stable unique ID derived from file
number and ordinal, not stored redundantly. Career saves and competition data
reference teams by it.

---

## 2. Player records — 38 bytes

`PlayerFile`. Sixteen per team: eleven starters plus five substitutes.

| Off | Size | Field |
|---|---|---|
| 0 | 1 | nationality |
| 1 | 1 | ? |
| 2 | 1 | shirt number |
| 3 | 23 | player name, null-terminated |
| 26 | 1 | **position and face, bit-packed** |
| 27 | 1 | **cards and injuries, bit-packed** |
| 28 | 1 | ? (high nibble) / **passing** (low) |
| 29 | 1 | **shooting** (high) / **heading** (low) |
| 30 | 1 | **tackling** (high) / **ball control** (low) |
| 31 | 1 | **speed** (high) / **finishing** (low) |
| 32 | 1 | price index, §5 |
| 33–37 | 5 | ? (init to zero) |

**Position and face** share byte 26: bits 5–7 are position (0 = goalkeeper, then
RB, LB, D, RW, LW, M, A), bits 3–4 are face (00 white, 01 ginger, 10 black). So a
black attacker is `0xF0`, a white goalkeeper `0x00`.

**Cards and injuries** share byte 27, and the porters' note records something odd:
*"value of this field is not read from file, before the game bits 5, 6, 7 are set
to random values"*. **Injuries are rolled fresh at load time, not persisted.** Bit
5 marks an injury bad enough that the player misses the next game.

---

## 3. Attributes are nibbles — **storage is 4 bits, the used range is 0–7**

`docs/SWOS/teams.txt` states it directly: *"(l.o. nibble) passing (values 0..15)"*.
Seven attributes packed two to a byte across bytes 28–31. **The storage width is 4
bits and that part is correct.**

> ⚠️ **Corrected.** This section previously concluded that because the nibble holds
> 0–15, the *attribute range* is 0–15, and that four attribute-indexed tables in the
> engine are therefore undersized. **That conclusion is wrong**, and it propagated
> into [HEADING.md](HEADING.md) §6, [STATE.md](STATE.md) §5, [TACKLING.md](TACKLING.md)
> §10 and [README.md](README.md)'s conventions list.
>
> The Amiga original's `AdjustPlayerSkills` (asm:102092) loads the packed longword
> and masks it with **`$07777777`** — three bits per nibble, not four — then unpacks
> seven bytes and clamps each to 7. **The high bit of every nibble is discarded on
> load.** A stored 8 arrives as 0 and a stored 15 as 7, which is exactly the
> long-standing community observation recorded in [LEGACY.md](LEGACY.md) §9. Every
> attribute-indexed table in the engine has exactly eight entries because eight is
> the right number. See [amiga/PLAYERS.md](amiga/PLAYERS.md) §1.
>
> The second caveat below was the strongest counter-argument, and it too was
> mistaken: `kPlayerHeaderSpeedIncrease` has **eight** entries, not thirteen, and the
> five apparently-positive values above index 7 belong to the next data item in the
> segment ([HEADING.md](HEADING.md) §10).

So the corrected picture:

| Table | Entries | Index | Max index | Overrun? |
|---|---|---|---|---|
| `kPlayerHeaderSpeedIncrease` | **8** | `heading` | 7 | no |
| `kPlAvgTacklingBallControlDiffChance` | 8 | \|difference of averages\| | 7 | no |
| `kPlayerTacklingDownTime` | 8 | `tackling` | 7 | no |
| `kComputerTacklingDownTime` | 8 | `tackling` | 7 | no |

Two things survive from the original reading and are worth keeping:

- **The high bit is real storage and may mean something.** It is masked off before
  any skill use, so it cannot be skill magnitude — but preserving it in an importer
  rather than normalising it away costs nothing and might turn out to matter.
- **Whether shipped team data ever sets it** is still directly measurable from the
  `team.*` files, and is now a *curiosity* about the data rather than a question
  about engine correctness.

One thing this section did not know at all: **the unpacked nibbles are not the final
ratings.** A factor derived from the player's transfer value is applied to every
skill during unpack ([LEGACY.md](LEGACY.md) §9). Any tuning fitted against numbers
read straight out of a team file will be systematically off unless that transform is
modelled.

**Implementation consequence, restated.** Model attributes as **0–7** internally and
offset only at the presentation boundary. No bounds policy is needed for these four
tables — the indices cannot exceed 7. The overrun class of bug documented in
[REFEREE.md](REFEREE.md) §4 is real but does not apply here.

---

## 4. Tactics — 370 bytes

Separate files, one per tactic. From `docs/SWOS/tactics.txt`:

```
 0     9   name, 8 chars + null
 9    35   player 1 positions
44    35   player 2 positions
...
324   35   player 10 positions
359   10   unknown table, "doesn't seem to be used"
369    1   index of the tactic applied when the ball is out of play
```

**The 35 bytes per player are the whole off-ball AI.** The pitch is divided into
**35 ball quadrants (5 rows × 7 columns)**, and each byte says: *when the ball is in
quadrant N, head for this position*. The byte packs the destination as **high
nibble = x quadrant, low nibble = y quadrant**, in a finer 240-cell grid
(16 rows × 15 columns).

Details that matter for reading [AI.md](AI.md) §3:

- **Quadrant 0 is the lower-right; quadrant 34 is the top-left.** X increases
  right-to-left, y bottom-to-top.
- **The goalkeeper is not in the table.** Ten players, not eleven — the keeper has
  its own logic ([AI.md](AI.md) §4).
- **Tactics are always authored as if the upper goal is ours.** For the second team
  the indices are reversed at lookup. One authored tactic serves both directions.
- The out-of-play tactic at byte 369 is also what the menus use to draw the lineup.

**Assignment of players to tactic slots is computed, not stored.** SWOS sorts the
eleven players by "defensiveness" — for each position, the maximum of distance from
the centre in x and distance from own goal in y, summed across positions — and
fills the tactic's slots in that order. So a tactic describes *roles*, and the
squad is fitted to them by a derived ordering. `docs/SWOS/tactics.txt` lists the
resulting default orderings per formation.

**Eighteen tactics**, matching `kNumFormationEntries = 18`
([BENCH.md](BENCH.md) §6):

```
0 4-4-2   1 5-4-1   2 4-5-1   3 5-3-2   4 3-5-2   5 4-3-3
6 4-2-4   7 4-3-3   8 SWEEP   9 5-2-3  10 ATACK  11 DEFEND
12-17 USER A-F
```

Note indices 5 and 7 are both labelled `4-3-3` in the notes, and index 7 is listed
as `3-4-3` in the orderings table — a discrepancy in the source notes, not resolved
here.

**350 bytes of data drive the entire off-ball movement of a football team.** That
is the most striking thing in this document, and it is why [AI.md](AI.md) §3 is
titled "a zonal grid, and nothing else".

---

## 5. Career saves

From `docs/SWOS/career.txt`. Structurally simple, semantically mostly unmapped:

```
0            2        number of teams k (usually 80)
2            k*684    team records, same 684-byte format as §1
0x0d5c0      28       ???
0x0d5dc      4        new balance
0x0d5e0      4        old balance
0x0d5e4      4        ???
0x0d5e8      4        income from player sales
0x0d5ec      4        general expenditures
0x0d5f0      4        player wages
0x0d5f4      4        ticket income
   ...                (large unmapped region)
0x16abd      1        season year, minus 2000 (or 2100 if > 100)
0x16ac2      2        game type: 1 DIY, 2 preset, 3 season
```

The career file **embeds full team records** rather than referencing a master
database — so a career carries its own mutated copy of every team, which is how
transfers and player development persist. It also means a career save is ~55 KB of
team data before anything else.

The gap between `0x0d5f8` and `0x16abc` is roughly **36 KB of unmapped state**:
competition tables, fixtures, player contracts, job offers. `AddJobOffer` appears
in the disassembly cross-references, so that machinery exists.

Player **price** (byte 32 of a player record) is an index into a 50-entry value
table, not a number: 0 = "25K−", 25 = 800K, 49 = "15M+". Non-linear, hand-authored,
and the endpoints are open ranges.

---

## 6. Replays and highlights

One format, `HIL2`, documented in `docs/highlights.txt`. Replays are *"internally
implemented as one big highlight"*.

```
  0     4   "HIL2" magic
  4     4   version, word.word = major.minor (currently 2.0)
  8     4   offset to scene data buffer
 12  1704   first (top) team, in-game structure
1716 1704   second (bottom) team
3420    40   game name ("RUSSIAN PREMIER DIVISION")
3460    40   game round ("FIRST ROUND")
3500     2   number of scenes
3502     2   goals, team 1
3504     2   goals, team 2
3506     1   pitch type
3507     1   pitch file number
3508     2   maximum substitutes
3510     2   padding
```

The **1704-byte team blocks confirm `TeamGame`'s size** independently
([STATE.md](STATE.md) §6) — the highlight format simply memcpy's the live in-match
squad structure.

Note the format stores **both pitch type and pitch number**
([PITCH.md](PITCH.md) §1) — playback needs the surface as well as the artwork,
because the surface affects physics.

`HIL2` is the porters' extension, not the original: it widens coordinates from
whole to full 32-bit fixed point, adds game time and statistics per frame, and
removes the original's 10-scene-per-file limit.

The notes record a alignment defect worth a smile: *"length of hil file header is
3,626 bytes, which isn't divisible by 4, but dword replay data buffer immediately
follows (making it improperly aligned)"*.

---

## 7. What this tells us

**Confirmed:**

- Team file: big-endian count + 684-byte records (76-byte header + 16 × 38-byte
  players); 64,000-byte hard ceiling. ✓
- **Attributes are 4-bit nibbles**, packed two per byte — but the **used range is
  0–7**, because the high bit of each nibble is masked off on load (§3). ✓
- Position and face share one byte; cards and injuries share another, and
  **injuries are randomised at load, not persisted**. ✓
- Tactics: 370 bytes, 10 players × 35 ball-quadrant → destination bytes, nibble
  packed, keeper excluded, authored one-directional and mirrored at lookup. ✓
- Player-to-slot assignment is derived by sorting on "defensiveness", not stored. ✓
- Career saves embed full mutated team records. ✓
- Player price is an index into a 50-entry non-linear table. ✓
- `HIL2` confirms `TeamGame` = 1704 bytes. ✓

**Open:**

- **Do shipped teams ever set the masked-off high bit?** Directly measurable from
  the `team.*` files. No longer a correctness question (§3) — but if the bit is ever
  set, it is carrying *something*, and nobody knows what.
- **The value→skill transform** applied during unpack. It stands between a team
  file's numbers and the engine's, and it is currently a black box
  ([amiga/PLAYERS.md](amiga/PLAYERS.md) §3).
- The ~36 KB unmapped region of career saves.
- `TeamGame::unknownTail[686]` ([STATE.md](STATE.md) §8) — likely the in-match
  statistics, and the highlight format copies it wholesale.
- Byte 28's unused high nibble in the player record — one spare attribute slot.
- The `4-3-3` / `3-4-3` discrepancy at tactic index 7.
- The `unkTable[10]` at tactics offset 359 — the notes say it appears unused.
- Byte 23's *"AND this with 0xFE"* in the team header.
- The country-number → global-team-number translation table.

---

## 8. Guidance

We are not implementing these formats, so this is short.

- **Model attributes as 0–7** (§3). Read the nibbles as 4-bit storage, mask to 3
  bits on import exactly as the original does, and keep the discarded bit somewhere
  if it is ever set. Do not size any engine table for 0–15.
- **Steal the tactics format's shape, not its bytes.** "For each of 35 ball
  quadrants, where does each of 10 roles stand" is an excellent representation:
  tiny, authorable by hand, trivially moddable, and it produces convincing team
  shape with no per-player logic. [PLAN.md](PLAN.md) §9 Phase 1 lists tactics
  lookup as core, and this is the model to copy.
- **Keep roles separate from players**, with the assignment derived. It means a
  tactic is valid for any squad and substitutions need no special handling
  ([BENCH.md](BENCH.md) §5 re-runs `ApplyTeamTactics` for exactly this reason).
- **Do not embed full team records in save files.** SWOS did because it had no
  alternative; we have SQLite ([PLAN.md](PLAN.md) §9 Phase 2). Career state should
  reference a team database, not copy it.
- **If we ever import original data**, §1–§3 is sufficient to write the reader, and
  the nibble packing is the only non-obvious part.
