# C3 — Kits, animation, ball, pitch

Real players on a real pitch: per-team kit colours over the original 101-frame bank, an
animation table + frame stepper driving `Entity.image_index`, the four-frame SWOS ball
with its own shadow sprite, the shirt-number marker that replaces C1a's white halo, and
the original stadium art in place of the reference tree's resampled tiles.

Depends on: A4, C1, B4, B9   Blocks: C4 (card presentation reuses the number sprite)   Wave: 4

**Status.** Landed, with two parts of the frame taxonomy explicitly unfinished (§7).

---

## 0. One-paragraph version

Player art stays **indexed** all the way to the GPU, so a kit is a **palette**, not a composite: at kickoff we build one 256-entry RGBA LUT per (team kit × face) — at most
eight for a match — and `DrawIndexedSprite` keys its texture cache on the palette
pointer, so tinting costs one texture set per kit and nothing per frame. Which of the
three shirt **geometries** a team uses is picked by `KitSpec.shirt_type` from three
banks imported out of `team1/2/3.dat`. Every tick the core steps a small
**frame-list bytecode** (show / delay / loop / hold) selected by `[player_state][keeper]
[direction]` and writes `Entity.image_index`; the renderer just blits that index,
Y-sorted, lifted by `z`. The ball becomes its **four real 4×4 rotation frames plus the
separate shadow sprite**, and the controlled player gets his **shirt number floating
above his head** (sprites 1188–1203) where the C1a ring used to be. Underneath all of
it the pitch becomes the original's own stadium — 863 chunky 16×16 tiles out of
`pitch1.blk` addressed by `pitch1.dat`'s 42×55 offset matrix — replacing the reference
tree's plurality-vote resample.

---

## 1. What the original install actually contains

Measured on `D:\SWOS 2020\swos` with the A4 band assembly (all 1334 headers
self-identify), not assumed. These findings drive the whole design and two of them
correct documents already in the tree.

| Sprites | Size | What it is |
|---|---|---|
| 1179–1182 | 4×4, centre (1,3) | **The ball** — four rotation frames, indices 1/2/3 |
| 1183 | 4×4 | **Ball shadow** — solid index 8 |
| 1184–1187 | 16×12 | Corner flag, four waving frames (own shadow pixels) |
| 1188–1196 | 3×5 | **Shirt numbers 1–9**, index 2 |
| 1197–1203 | 5×5 | **Shirt numbers 10–16** |
| 1204 | 5×5 | Diamond marker |
| 1273–1283 | 11–13×15 | Referee |
| 1285–1289 | 16×68, 35×27 … | Goal frame / nets |
| 341–643 | 9–13×15 | Team file in slot A, three 101-frame blocks |
| 947–1004 | — | Goalkeepers (58 imported of the 116-slot band) |

**Finding 1 — the three blocks inside a team file are byte-identical.**
`slotA_blk0 == slotA_blk1 == slotA_blk2` for all 101 frames, likewise for slot B. They
are the **three faces**, and a face is a palette difference (skin 4/5/6, hair 9/12/13),
so in index space the copies carry no information. We import **101 frames per geometry,
not 303**, and faces cost nothing.

**Finding 2 — the geometry is the team *file*, and A4 §6.5 is resolved.**
`team1/2/3.dat` differ in all 101 frames. Reading the shirt indices off the front
standing frame settles which is which:

| File | Torso rows | Sleeve columns | Geometry |
|---|---|---|---|
| `team1.dat` | `S s S s S` alternating | 10 | **Vertical stripes** (and plain, when stripes == shirt) |
| `team2.dat` | uniform per row, alternating by row | 10 | **Horizontal stripes** |
| `team3.dat` | uniform per row | **11** | **Coloured sleeves** |

That matches `docs/SWOS/sprites.txt` (files are geometries) and refutes
`convertGameSprites.py`'s block naming, exactly as A4 §6.5 suspected. Our importer today
loads only `team1.dat` + `team2.dat` and writes six packs, three of which are duplicates
of the other three; it never opens `team3.dat`, so **coloured sleeves are currently
unreachable**.

**Finding 3 — the whole pitch is in the install, and it is not where A4 looked.**
`pitchN.dat` (9240 = 42 × 55 × 4) is the **matrix**: one u32 per cell, and every value is
a **byte offset** into a sibling **`pitchN.blk`** — the file A4 never opened. `pitch1.blk`
is 220928 bytes = **863 tiles × 256 bytes**, and 256 bytes for a 16×16 tile means the
pitch is **chunky 8-bit**, not the sprites' 4-bit planar: rendering it through `pal.256`
uses 102 palette entries and produces the original stadium — grass weave, markings,
goals, dugouts, advertising hoardings and crowd. Six pitches ship (`pitch1..6`).

Two consequences, both measured:

- **A4 §6.1's blocker is gone.** The reference tree's 175-colour plurality-vote resample
  can be dropped entirely; these are the original's pixels.
- **A4 §6.2's answer changes.** The 55 rows are not "pad + 53 content + pad" — row 0 and
  row 54 carry stands, and drawing the matrix rows 1:1 as world rows puts the engine's
  own landmarks exactly on the painted markings (centre spot `(336,449)` on the painted
  spot, halfway line on the halfway line, touchlines on `x = 81 / 590`, goal lines on
  `y = 129 / 769`). So `PitchMatrixRow()`'s `world_row + 1` shift — correct for the
  reference tree's matrix — must be **dropped** for the original's, and the drawable
  world is 42 × 55 × 16 = 672 × **880**, with the extra height being the stands the
  camera may show.

---

## 2. Scope

### In

- `assetc`: geometry banks from `team1/2/3.dat`, ball + shadow, shirt numbers, keeper
  bank check; kit-colour ordinal table into `palette.atl` aux
- Kit palette build at kickoff from `KitSpec` (+ face, + keeper kit)
- Animation tables + frame stepper in `at_core`, writing `Entity.image_index`
- Ball: four rotation frames, separate shadow sprite, z lift
- Controlled-player shirt number; removal of the C1a ring
- C1a debug chrome moved behind one toggle instead of always on
- **Pitch from the original** — `pitchN.blk` + `pitchN.dat`, 8-bit tiles, row mapping
  corrected, all six pitches importable

### Out

| Excluded | Owner |
|---|---|
| Camera modes, lead-ahead | C2 |
| Scoreboard, card presentation, replays | C4 (reuses the number sprite) |
| Corner flags, referee sprite, goal nets | C4 |
| Which pitch a fixture picks (seasonal roll, weather) | later; `MatchSurface` already exists |
| Animated hoardings / crowd | C4 |
| Bench / subs art | C6 |
| Substitutions changing a face mid-match | later |

---

## 3. Design

### 3.1 Asset side — what `assetc` writes

Replace the six `slot{A,B}_blk{0,1,2}` packs with three geometry packs plus the small
banks the match needs:

```
kit_vstripe.atp   101 frames   team1.dat  341..441      ShirtType 0 (plain) and 2
kit_hstripe.atp   101 frames   team2.dat  341..441      ShirtType 3
kit_sleeves.atp   101 frames   team3.dat  341..441      ShirtType 1
keepers.atp        58 frames   goal1.dat  947..1004     unchanged
ball.atp            5 frames   bench.dat  1179..1183    4 rotation + 1 shadow
numbers.atp        16 frames   bench.dat  1188..1203    shirt numbers 1..16
palette.atl                    aux: 16 layer bytes + 10 kit-colour ordinals
```

The band table in `swos_sprites.cpp` keeps its slot-A/slot-B shape (it describes
`sprite.dat`, which really is two slots); the *import* loop reads the player bank three
times, once per team file, and takes only the first 101 frames of each. Bank names stop
being about slots and start being about geometry, which Finding 2 now licenses.

`palette.atl`'s aux grows from 16 to 26 bytes: the existing kit-layer routing table plus
the ten kit-colour ordinals `1,2,3,6,10,11,12,13,14,15` (RENDERING.md §5). A4 §2.4
already promised the ordinals live there; only the layer table was written.

`tools/gen_placeholder.py` and `assets/placeholder/` must grow the same bank names, and
`test_placeholder_parity` keeps them honest. Manifest fingerprint changes, so a stale
`assets/generated/` is rejected rather than half-loaded — which is the point of A4 §2.4
item 3 and will bite anyone who does not re-run `assetc`.

### 3.2 Kit palettes — a kit is a LUT, not a composite

The reference composites six tinted layers into a texture set per face per team
(RENDERING.md §4) because SDL colour-mod needs separate surfaces. We never left the
index domain (A4 §2.3), so the same result is a 256-entry array:

```
KitPalette Build(const GamePalette& game, const KitSpec& kit, uint8_t face)
  base = game.rgba                       // 256 entries, unchanged
  base[10] = kKitColour[kit.shirt]       // shirt
  base[11] = kKitColour[kit.stripes]     // stripes / sleeves
  base[14] = kKitColour[kit.shorts]
  base[15] = kKitColour[kit.socks]
  base[4,5,6]  = kSkinRamp[face]         // three shades
  base[9,12,13]= kHairRamp[face]
```

`kKitColour[10]` is the game palette read through the kit-colour ordinal table from
`palette.atl`. `kSkinRamp` / `kHairRamp` are three-shade ramps per face; face 0 keeps
the imported palette's own values so the default look is bit-identical to the original,
and faces 1–2 are derived ramps (ginger, dark) in the same shading ratio.

A `KitBank` owned by the app builds these at kickoff: 2 teams × 3 faces + 2 keeper kits
= **at most 8 palettes**, rebuilt only when `TeamSheet` changes. `DrawIndexedSprite`
already keys its texture cache on `(pixel ptr, palette ptr)`, so distinct palettes get
distinct textures with no change to that function — provided the palette buffers are
stable for the process lifetime, which `KitBank` guarantees.

Shirt type → bank:

| `KitSpec.shirt_type` | Bank | Note |
|---|---|---|
| 0 Plain | `kit_vstripe` | stripes colour forced to the shirt colour |
| 2 VerticalStripes | `kit_vstripe` | |
| 3 HorizontalStripes | `kit_hstripe` | |
| 1 ColouredSleeves | `kit_sleeves` | |

Colour-clash handling (both sides landing on the same shirt colour → use `secondary`) is
a one-line policy at kickoff and belongs here rather than in C4.

### 3.3 Animation — the stepper lives in core

`Entity` already carries `frame_offset`, `frame_index`, `frame_delay`,
`cycle_frames_timer`, `frame_switch_counter`, `image_index`, `direction` (B1), and
`movement.hpp` already writes `frame_delay` from speed. Nothing steps them. C3 fills
that in, in `at_core`, because `frame_switch_counter` gates gameplay (header contact
timing — PLAYER_SPRITES.md §12) and because anything that gates gameplay must be inside
the hashed simulation.

**`core/animation.hpp`** — pure, table-driven:

- A *frame list* is `std::span<const int16_t>` interpreted per PLAYER_SPRITES.md §7:
  `≥0` show · `-1…-99` set delay · `-101` hold · `-999` loop to start · `-100`/`≤-102`
  relative jump.
- An *animation table* is `[is_keeper][direction 0..7]` → frame list. The reference's
  extra `[team]` dimension does not survive: it existed only because team 1 and team 2
  used different absolute sprite ranges, and our banks are per-geometry and zero-based.
- `PlayerState` (already in `match_state.hpp`) selects the table:
  standing, running, tackling, tackled, static/jump header attempt+hit, throw-in,
  celebrating, injured, booked.
- `StepPlayerAnimation(Entity&)` runs the interpreter; `SelectAnimationTable(Entity&)`
  applies the idle fallback (`kNormal` + stationary → standing).

Called from `MatchEngine::Step` after `MovePlayers`, for all 22 players plus the ball.
`image_index = frame_list[frame_index]` — **no face offset**, because faces are palette
here (Finding 1). `frame_offset` becomes unused for players; leave the field (layout is
load-bearing for `HashState`) and document it as reserved.

**The frame tables are the real work.** We do not have the reference's tables, so the
101 frames must be classified by looking at them. Work item 4 adds `assetc --sheet
<bank>` to dump a labelled contact sheet PNG; the taxonomy is then written once as a
data table in `core/animation_tables.hpp` and never guessed again. First read of the
sheet suggests roughly: 0–43 walk cycles across the eight octants, 44–53 tackle/fall,
54–63 lying/tackled, 64–90 header and throw poses, 91–100 celebration/booked/injured —
to be pinned precisely, not shipped as a guess.

**Cost, stated up front:** stepping these fields changes `HashState`, so every pinned
scenario re-pins — `test_determinism`, `test_move_players`, `test_dribble_turn`,
`test_curled_shot`, `test_contest_sequence`, `test_restart_cycle`, `test_ai_b9`,
`tests/golden/kickoff.attr`, and the corpus chains. That is expected and is why the
stepper goes in as one commit with its re-pin, not spread across several.

*Alternative considered and rejected:* animate in the renderer only, leaving `HashState`
untouched. Cheaper today, but it puts `frame_switch_counter` outside the simulation,
which breaks header-contact timing later and contradicts PLAYER_SPRITES.md §12. The
re-pin is accepted as the price.

### 3.4 The ball

`IAssetSource` gains `Ball(int frame)` and `BallShadow()`; `ImportedAssets` serves the
five entries of `ball.atp`, and the placeholder synthesises four discs plus a shadow so
a clean clone still runs.

The ball is an `Entity` with the same frame fields, so its rotation steps in core too —
one frame list, direction-independent, delay derived from `ball.speed` so a fast ball
spins fast. Draw order per item: **shadow at the ground point, ball at `ground.y − z`**,
both anchor-centred; the C1a "soft shadow" `FillCircle` goes away with the real sprite.
Ball Y-sort keeps its current position in the list.

### 3.5 The shirt number, and what else C1a leaves behind

The white `StrokeRing` is replaced by the original's own device: the controlled player's
**shirt number sprite drawn above his head**, `z`-lifted (REFEREE.md §4 uses
`kPlayerNumberOffset = 20` for the booked number; the controlled-player marker is the
same mechanism, `UpdateControlledPlayerNumbers` — SIMULATION.md §2). Number `n` maps to
`numbers.atp` entry `n-1`; `SquadPlayer.shirt_number` already carries it, reachable from
the entity's `player_ordinal` through the side's sheet.

Drawn only for the human side(s) — a CPU team has no controlled player worth marking
(`TeamControl.player_number == 0`).

Full C1a removal list:

| Artifact | `match_renderer.cpp` | Disposition |
|---|---|---|
| White ring on the controlled player | `StrokeRing` at ~L307 | **Replaced** by the number sprite |
| Landmark overlay (boxes, centre circle, goal mouths) | `DrawLandmarks` | Keep, but only on the no-packs fallback path (already conditional) |
| Coloured player dots | fallback block at ~L315 | Keep as the no-packs fallback only |
| Ball soft-shadow dot | `FillCircle` at ~L265 | **Replaced** by the shadow sprite |
| Debug HUD: state / control / kick telemetry / key help | `DrawMatchHud` | Move behind an `F1` toggle, default off, **not deleted** — B6a's timing work reads it |

### 3.6 The pitch

`assetc` grows a second pitch path beside the reference-tree one, selected by
`--swos-dir`:

```
for n in 1..6:
    tiles  = pitch<n>.blk           863 x (16x16 chunky 8-bit)   -> ATAP kPitch entries
    matrix = pitch<n>.dat           42 x 55 u32 byte offsets     -> aux, as tile INDICES
    palette= pal.256                256 RGBA                     -> pitch<n>.atl
```

Byte offsets are divided by 256 at import so the pack's aux stays what `PitchTiles`
already expects — u16 indices — and nothing downstream learns about the original's
addressing. Deduplication is already done for us: 863 distinct tiles across 2310 cells.

The runtime changes are small and mostly deletions:

| Today | Change |
|---|---|
| `PitchMatrixRow()` shifts `world_row + 1` when `grid_h == 55` | Drop the shift for original-sourced packs; the identity mapping is the measured one (Finding 3) |
| `kPitchWorldRows = 53`, world 672×848 | 55 rows, 672×880 — the two extra rows are stands, drawn but never walked on |
| Pitch palette from `pitch1.atl` (16-ish colours, resampled) | Same seam, now 256 entries from `pal.256` |
| `DrawLandmarks` overlay | Fallback-only (the tiles carry the markings, and now carry them correctly) |

`SourceKind` in the pack header already distinguishes `kRefTree` from `kOriginal`, so the
row-shift rule keys off data rather than a build flag and a placeholder clone is
unaffected. Physics is untouched: the barrier box and every pitch constant in
`match_state.hpp` are unchanged — Finding 3 says they were already right.

### 3.7 Draw order (final)

1. Pitch tiles (C1)
2. Y-sorted pass: for each entity — player sprite (`image_index`, kit palette, `z` lift)
   or ball (shadow at ground, ball at `y − z`)
3. Shirt number above the controlled player
4. HUD / debug overlay (toggled)

---

## 4. Interfaces

| Path | Change |
|---|---|
| `src/tools/assetc/swos_import.cpp` | Geometry banks ×3, ball, numbers; aux gains kit ordinals |
| `src/tools/assetc/pitch.cpp` | **`.blk` + `.dat` path** beside the ref-tree importer; offsets → indices |
| `src/tools/assetc/main.cpp` | `--sheet <bank>` contact-sheet dump; `--pitch all` under `--swos-dir` |
| `src/app/render/asset_source.hpp` | `kPitchWorldRows` 53 → 55; `PitchMatrixRow` keyed on source kind |
| `src/app/render/asset_source.hpp` | `Player(ShirtGeometry, frame)`, `Ball(frame)`, `BallShadow()`, `Number(n)`, kit-ordinal accessor |
| `src/app/render/imported_assets.cpp` | Load the new bank set |
| `src/app/render/placeholder_assets.cpp` + `tools/gen_placeholder.py` | Parity for the new banks |
| `src/app/render/kit_palette.{hpp,cpp}` | **new** — `KitBank`, `BuildKitPalette` (pure; no SDL) |
| `src/core/include/core/animation.hpp` | **new** — stepper + opcode interpreter |
| `src/core/include/core/animation_tables.hpp` | **new** — the frame lists |
| `src/core/src/match_engine.cpp` | Step animation after `MovePlayers` |
| `src/app/render/match_renderer.cpp` | Draw by `image_index`; number sprite; artifact removal |
| `assets/generated/` | Re-import required (fingerprint changes) |

Walls unchanged: no SDL in `src/core/` or `at_asset_source`; `kit_palette` is pure data
so it can be unit-tested without a window.

---

## 5. Work items

Ordered so the screen improves early and the hash re-pin lands once.

1. **The pitch** — `.blk`/`.dat` import path, offsets → indices, row shift dropped,
   `kPitchWorldRows` 55, `pitch1.atl` from `pal.256`, `test_pitch_tiles` extended.
   *Largest visible change in the plan and independent of everything else — first.*
2. **`assetc` bank rework** — three geometries, ball, numbers, kit ordinals in aux;
   re-import; update `ImportedAssets`, placeholder, `asset_tests`, parity test.
3. **The ball** — `Ball(frame)` / `BallShadow()`, shadow + lift in the renderer, static
   frame 0 first. *Visible win, no core change.*
4. **Kit palettes** — `KitBank`, geometry selection from `shirt_type`, faces, keeper
   kit, clash policy. *Both teams stop wearing the imported default.*
5. **Frame taxonomy** — `--sheet` dump, classify the 101 frames, write
   `animation_tables.hpp`. *Paper work item; nothing renders differently.*
6. **The stepper** — `core/animation.hpp`, wire into `Step`, renderer draws
   `image_index`, ball rotation. **One commit with the full re-pin.**
7. **Shirt number + artifact removal** — number sprite, ring and dot removal, HUD behind
   a toggle (default off; `F1`).
8. **Polish** — deterministic Y-sort tie-break, anchor audit against the original's
   centre points, keeper bank frame-count check.

---

## 6. Tests and acceptance

**Automated**

- `asset_tests`: the three geometry banks exist, are 101 frames, and differ from each
  other; `ball.atp` has 5 entries at 4×4; `numbers.atp` has 16; `palette.atl` aux is 26
  bytes with the documented ordinals.
- `test_pitch_tiles`: an original-sourced pack maps matrix row `r` to world row `r`
  (no shift) while a ref-tree pack keeps the old behaviour; 863 tiles / 2310 cells for
  pitch 1; every matrix value divides by 256 at import.
- **Landmark agreement** (pure, no SDL): for a known original pitch pack, the tile cells
  covering `kCentreSpotX/Y`, the halfway line, the touchlines and both goal lines are the
  cells that carry those markings — the numeric form of the overlay that measured
  Finding 3, so a future re-import cannot silently shift the pitch by a tile.
- `test_kit_palette` (pure): a known `KitSpec` + face produces the expected RGBA at
  indices 4/5/6, 9/10/11/12/13, 14, 15 and leaves every other index untouched.
- `test_animation` (pure): the opcode interpreter against a hand-written frame list —
  show, delay change, loopback, hold, loop; and a golden 60-tick frame sequence for a
  running player.
- Re-pinned `HashState` scenarios + golden + corpus chains (one commit).
- `wall_check` unchanged; `core_tests` still links no SDL.

**Manual**

1. MATCH: the original stadium — crowd, hoardings, dugouts, crisp markings — with the
   ball-follow camera at 1 pitch unit = 1 pixel and no tile seams.
2. MATCH: two teams in visibly different kits, both distinct from the keeper.
3. A player walking a circle cycles legs in all eight octants; standing still settles to
   a standing frame.
4. The ball spins while travelling and stops spinning when it stops; its shadow stays on
   the ground through a lofted pass.
5. The controlled player carries his shirt number; no white ring anywhere.
6. Placeholder-only clone still runs and still plays.

**Done when:** both teams' kits render from arbitrary `KitSpec` values over the original
frames on the original pitch, players animate from the deterministic stepper, and the
match view contains no C1a instrumentation with the debug overlay off — PLAN.md C3
done-when plus C1's outstanding fidelity item.

---

## 7. Open questions

- **The two extra pitch rows.** Drawing 55 rows gives a 672×880 world while physics stays
  in the 672×848 the constants assume. Nothing breaks — the camera is clamped to the
  dead-ball frame `y ∈ [100, 799]` and never reaches them — but C2 should decide
  deliberately whether the stands are reachable by a camera mode or are permanent
  overscan.
- **Which pitch to play on.** Six pitches import; the fixture-to-pitch rule (and its
  relationship to `MatchSurface`) is not C3's. Pitch 1 stays hardcoded here.
- **Face assignment.** `SquadPlayer.face` exists and A5 fills it; whether our fictional
  league distributes faces sensibly is untested.
- **Keeper bank shape.** 58 frames imported from a 116-slot band — establish whether
  keepers have 58 real frames or whether the band is two 58-frame copies, before
  authoring keeper animation tables.
- **Number sprite colour.** Index 2 (white) in the original art; whether the original
  tinted it per team is unverified. Start white.
- **`frame_offset`** becomes unused. Leave the field (layout is load-bearing) or reuse
  it for the geometry bank ordinal — decide during work item 5.

---

## 8. What this resolves elsewhere

- **A4 §6.5** — resolved by Finding 2; update that section and the bank-naming comment
  in `swos_import.cpp`, which currently says the question is C3's to answer.
- **A4 §6.1** — the "reference tree is a resampled bootstrap" blocker closes for the
  pitch as well as the sprites: `pitchN.blk` is the faithful source. The resample warning
  in `assetc` stays, but nothing we ship depends on it any more.
- **A4 §6.2** — its "row 0 and 54 are pad, all tile 0" answer holds only for the
  reference tree's matrix. Restate it: the original's 55 rows are all content, and world
  row = matrix row.
- **A4 §6.4** — storing 1× and scaling at draw is confirmed correct: the original pitch
  is 1 pitch unit per pixel and the follow camera already runs at scale 1.0.
- **C1-render-core.md** — its §2.1 "drawable grid 42×53" and §6 "faithful pitch import
  still outstanding" both change here.
- **PLAYER_SPRITES.md §3** — its block table (0 = ordinary, 101 = horizontal, 202 =
  sleeves) describes the reference port's extraction, not the original's team files.
  Add a note; the reference document stays as-is otherwise.
- **PLAN-CURRENTSTATE.md** — add the C3 row and move C1's "kit sheets / anim tables
  remain C3" line to point here.
