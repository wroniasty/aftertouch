# PITCH.md

The playing surface: the two independent things "pitch" means in SWOS, how each is
chosen, how the surface couples into ball physics, and how the tile grid is stored
and drawn. Traced through the reference DOS port in
[../reference/swos-port/](../reference/swos-port/).

The physics constants the pitch type selects are tabulated in
[BALL.md](BALL.md) §9 and not repeated here; this document is where they come from
and what else the pitch controls. Camera limits over the pitch are
[CAMERA.md](CAMERA.md) §7.

> **Provenance.** [pitch.cpp](../reference/swos-port/src/game/pitch/pitch.cpp) is
> modern C++, not decompilation, so the selection logic and probability tables are
> read directly and are reliable. **But this file mixes two eras**: the pitch type /
> number selection and the 16-pixel tile grid are the original's; the zoom system,
> the multi-resolution asset handling and the texture-batched renderer are the
> porters' additions and have no counterpart in the 1994 game. §5 and §6 are marked
> accordingly. Read to understand the design; write our own code.

---

## 0. One-paragraph version

"Pitch" is **two independent values**. `m_pitchType` (0–6: Frozen, Muddy, Wet,
Soft, Normal, Dry, Hard) is the *surface*, and it feeds three ball-physics
constants — rolling friction, horizontal bounce loss, vertical restitution. It is
rolled from a **12 × 7 seasonal probability table** so that Frozen appears in
January and never in July. `m_pitchNumber` (0–4) is the *graphics*, chosen from a
16-entry weighting table indexed either by a random draw (friendlies) or by a
**hash of the home team's name and kit colours** — so a given fixture always looks
the same in a career, without storing anything. The pitch itself is a **42 × 53
grid of 16-pixel patterns**, indexed indirectly through a per-pitch matrix into a
shared atlas, and drawn as a visible window of that grid with the tiles sorted by
texture to reduce state changes.

---

## 1. Type versus number

| | `m_pitchType` | `m_pitchNumber` |
|---|---|---|
| Range | 0–6 | 0–4 |
| Meaning | Surface condition | Which pitch artwork |
| Affects | Ball physics | Appearance only |
| Chosen by | Season / month, or flat table | Team hash, or random |
| Set by | [setPitchType()](../reference/swos-port/src/game/pitch/pitch.cpp#L222) | [setPitchNumber()](../reference/swos-port/src/game/pitch/pitch.cpp#L259) |

Both are set together by `setPitchTypeAndNumber()` at match start. They are
completely independent: a Frozen pitch can use any of the five artworks.

```
enum PitchTypes {
    kRandom = -1, kFrozen = 0, kMuddy = 1, kWet = 2,
    kSoft = 3, kNormal = 4, kDry = 5, kHard = 6, kMaxPitchType = 6,
};
```

Note `kNormal = 4` sits in the middle of the ordering, and the ordering runs
**wet-to-dry**: Frozen, Muddy, Wet, Soft, Normal, Dry, Hard. The physics tables in
[BALL.md](BALL.md) §9 are indexed by this enum, and reading them against this
ordering is what makes them legible — `ballSpeedBounceFactor` runs 24, 80, 80, 72,
64, 40, 32, peaking on the wet surfaces and lowest on Frozen and Hard.

> **Amiga cross-check.** The Amiga original carries all three tables with the same
> values in the same order ([BALL.md](BALL.md) §12), but the pitch-type index
> arrives from outside its match module, so the Amiga document can only infer the
> naming from the tables' shape — it reads index 0 as "a fast, true surface" and
> 1–2 as "heavy, slow, dead", which is exactly Frozen / Muddy / Wet. **This
> document supplies the names the Amiga binary cannot**, and the fact that a
> blind reading of the numbers reconstructs the same wet-to-dry ordering is decent
> corroboration for both. See [amiga/BALL.md](amiga/BALL.md) §5.

---

## 2. Choosing the surface

[setPitchType()](../reference/swos-port/src/game/pitch/pitch.cpp#L222):

```
if (gamePitchTypeOrSeason || gamePitchType == -1) {
    probabilities = gamePitchTypeOrSeason
        ? kPitchTypeSeasonalProbabilities[gameSeason]     // month-dependent
        : kPitchTypeProbabilities;                        // flat
    p = rand() * 100 / 256;                               // 0..99
    m_pitchType = 0;
    while (p >= *probabilities) { p -= *probabilities++; m_pitchType++; }
} else {
    m_pitchType = gamePitchType;                          // explicitly chosen
}
```

A standard roulette-wheel selection over percentages summing to 100.

**The seasonal table**, 12 months × 7 types
([:226-239](../reference/swos-port/src/game/pitch/pitch.cpp#L226-L239)):

| Month | Frozen | Muddy | Wet | Soft | Normal | Dry | Hard |
|---|---|---|---|---|---|---|---|
| 1 | 30 | 20 | 30 | 20 | 0 | 0 | 0 |
| 2 | 20 | 30 | 20 | 20 | 10 | 0 | 0 |
| 3 | 10 | 30 | 10 | 30 | 20 | 0 | 0 |
| 4 | 0 | 10 | 10 | 30 | 40 | 10 | 0 |
| 5 | 0 | 0 | 0 | 10 | 40 | 40 | 10 |
| 6 | 0 | 0 | 0 | 0 | 40 | 40 | 20 |
| 7 | 0 | 0 | 0 | 0 | 30 | 30 | 40 |
| 8 | 0 | 0 | 0 | 0 | 50 | 30 | 20 |
| 9 | 0 | 0 | 0 | 20 | 40 | 30 | 10 |
| 10 | 0 | 20 | 0 | 40 | 30 | 10 | 0 |
| 11 | 10 | 30 | 10 | 40 | 10 | 0 | 0 |
| 12 | 20 | 30 | 20 | 30 | 0 | 0 | 0 |

**This is a weather model expressed as one table**, and it is the reason a SWOS
season feels like a season: January is 80 % likely to be Frozen, Muddy or Wet and
cannot be Dry or Hard at all; July is 70 % Dry-or-Hard and cannot be anything wet.
The transitions are smooth — no month jumps more than 20 points in any column.

The **flat table**, used when the season is not driving selection:

```
kPitchTypeProbabilities = { 5, 5, 10, 20, 30, 20, 10 }
```

A bell curve centred on Normal. Extremes are rare — 5 % each for Frozen and Muddy.

---

## 3. Choosing the artwork

[setPitchNumber()](../reference/swos-port/src/game/pitch/pitch.cpp#L259):

```
kPitchNumberProbabilities[16] = { 0,0,0,0,0, 1,1,1,1,1, 4,4, 2,2, 3,3 }

if (g_trainingGame)      m_pitchNumber = kTrainingPitchIndex
else if (plg_D0_param)   index = rand() & 0xf                    // friendlies
else {                                                            // career
    index  = topTeamInGame.teamName[0] | (topTeamInGame.teamName[1] << 8)
    index ^= topTeamInGame.prShirtCol
    index ^= topTeamInGame.prShortsCol
    index &= 0xf
}
m_pitchNumber = kPitchNumberProbabilities[index]
```

Weighting: pitch 0 and pitch 1 get 5/16 each, pitches 2, 3 and 4 get 2/16 each.

**The career path is the interesting one**, and the source comments it:
*"pseudo-random, but always the same in regards to the teams that are playing; used
in career."* The index is a **hash of the home team's first two name characters
XORed with its shirt and shorts colours**. No storage, no seed, no per-fixture
state — every visit to a given ground produces the same pitch, forever, because it
is derived from data that never changes.

This is a genuinely elegant trick and worth copying wholesale: *derive cosmetic
per-entity variation from a hash of the entity's own immutable data rather than
storing it.* It also means two teams with similar names and identical kits share a
ground appearance, which nobody has ever noticed.

Note the friendly path consumes an RNG draw and the career path does not — relevant
to replay determinism ([AI.md](AI.md) §6).

---

## 4. The tile grid

```
kPitchWidth  = 672      kSwosPatternSize = 16
kPitchHeight = 848
```

with a `static_assert` binding them
([:16-17](../reference/swos-port/src/game/pitch/pitch.cpp#L16-L17)):
`kPitchWidth == kPitchPatternWidth * 16` and
`kPitchHeight == kPitchPatternHeight * 16`. So the grid is **42 × 53 patterns** of
16 × 16 pixels.

(`docs/rendering.txt` describes the matrix as "55x42 tiles", which does not match
the assert. Trust the assert; flag the doc.)

Lookup is **two levels of indirection**
([loadPitch()](../reference/swos-port/src/game/pitch/pitch.cpp#L74),
[drawPitch()](../reference/swos-port/src/game/pitch/pitch.cpp#L286)):

```
patternIndex = kPitchIndices[m_pitchNumber][row][col]     // per-pitch matrix
pattern      = kPatterns[m_res][kPitchPatternStartIndices[m_pitchNumber] + patternIndex]
texture      = m_pitchTextures[m_res][pattern.texture]
srcRect      = { pattern.x, pattern.y, patternSize, patternSize }
```

So each pitch owns a **matrix of indices into its own slice of a shared pattern
table**, and each pattern names an atlas texture plus a source rectangle. Five
pitches share one pattern pool via `kPitchPatternStartIndices`.

`patternIndex == UINT16_MAX` marks an **empty cell** — skipped entirely, not drawn
as transparent. The pitch is allowed to have holes.

The pitch coordinate space matches the physics: [BALL.md](BALL.md)'s barrier of
`x ∈ [53, 618]`, `y ∈ [100, 799]` sits comfortably inside 672 × 848, with the
margin being the surround.

---

## 5. Zoom — a port addition

**Not in the original.** SWOS had a fixed 320×256 (Amiga) view with hardware
scrolling ([LEGACY.md](LEGACY.md) §2). Everything in this section is the porters'.

```
kZoomIncrement        = 1/70/4      ≈ 0.00357
kMaxZoom              = 2.5
kMaxZoomVelocityFactor = 0.1
```

[getMinimumZoom()](../reference/swos-port/src/game/pitch/pitch.cpp#L400) is
`max(fieldWidth / 672, fieldHeight / 848)` — the zoom at which the pitch exactly
fills the view, so you can never zoom out past the pitch edges.

[getDefaultZoom()](../reference/swos-port/src/game/pitch/pitch.cpp#L412) targets a
notional `kTargetVgaWidth = 435` of visible pitch, scaled to the window.

[getZoomIncrement()](../reference/swos-port/src/game/pitch/pitch.cpp#L421) is
**non-linear**:

```
increment = (zoom - minZoom) / (kMaxZoom - minZoom) * 0.1 + kZoomIncrement
```

The step grows as you zoom in, so zooming feels proportional rather than
decelerating — a standard trick, and the right one.

`clipZoom()` runs at the top of every `drawPitch` because the window may have been
resized between frames.

---

## 6. Rendering — a port addition

[drawPitch()](../reference/swos-port/src/game/pitch/pitch.cpp#L286). The visible
window is computed as a pattern-aligned sub-rectangle:

```
widthNormalized  = fieldWidth / zoom
x = cameraX + (kVgaWidth - widthNormalized) / 2
clipPitch(...)                             // clamp to pitch, return leftover offsets
numPatternsX = ceil(min(42, widthNormalized/16 + 1))
column = x / 16;   xOfs = (frac + remainder) * scale * zoom
row    = y / 16 - 1                        // "skip the top invisible row"
```

Then every visible pattern is collected into a `RenderPattern` array, **sorted by
texture index**, and blitted:

```
std::sort(renderPatterns.begin(), renderPatterns.begin() + numPatterns);
// "reduce the number of texture switch calls"
```

A texture-batching optimisation with no 1994 equivalent. Note also
`clipPitch` returns the **leftover offsets** — how far the requested view fell
outside the pitch — which the caller uses to keep sprites aligned when the camera
is clamped.

`row = y/16 - 1` with the comment *"make sure to skip the top invisible row"*: the
grid has a row that is never drawn. Unexplained.

[DrawAnimatedPatterns()](../reference/swos-port/src/game/pitch/pitch.cpp#L351) is
**almost entirely stubbed out** — its whole body is a `showFansCounter` decrement
and a `//...`. Whatever animated pitch patterns the original had (crowd movement,
flags) is not ported. Note that this is also the only thing that decrements
`showFansCounter`, which [CAMERA.md](CAMERA.md) §2 shows freezes the camera.

---

## 7. Constants quick reference

| Constant | Value | Origin |
|---|---|---|
| `kPitchWidth` × `kPitchHeight` | 672 × 848 | original |
| `kSwosPatternSize` | 16 | original |
| Pattern grid | 42 × 53 | derived from the `static_assert` |
| `PitchTypes` | 0 Frozen … 6 Hard, `kNormal = 4` | original |
| `kPitchTypeSeasonalProbabilities` | 12 × 7, §2 | original |
| `kPitchTypeProbabilities` | `5,5,10,20,30,20,10` | original |
| `kPitchNumberProbabilities` | `0,0,0,0,0,1,1,1,1,1,4,4,2,2,3,3` | original |
| Career pitch hash | `name[0] \| name[1]<<8 ^ shirtCol ^ shortsCol & 0xF` | original |
| Empty cell marker | `UINT16_MAX` | original |
| `kZoomIncrement` | 1/70/4 | **port** |
| `kMaxZoom` | 2.5 | **port** |
| `kMaxZoomVelocityFactor` | 0.1 | **port** |
| `kTargetVgaWidth` | 435 | **port** |

---

## 8. What this resolves, and what still needs measurement

**Confirmed as structure:**

- Pitch type (surface, 7 values) and pitch number (artwork, 5 values) are
  independent. ✓
- Surface is rolled from a 12 × 7 seasonal probability table; a flat bell-curve
  table is the non-seasonal fallback. ✓
- Artwork is chosen by a hash of the home team's name and kit colours in career, so
  a ground always looks the same without storing anything. ✓
- Grid is 42 × 53 patterns of 16 px, with two-level indirection into a shared
  pattern pool and `UINT16_MAX` marking empty cells. ✓
- Surface couples to exactly three ball constants ([BALL.md](BALL.md) §9). ✓
- Zoom, multi-resolution assets and texture batching are port additions, not
  original behaviour. ✓

**Open (measurement targets, [LEGACY.md](LEGACY.md) §15):**

- **Does the surface affect anything besides the ball?** Player acceleration, top
  speed and turning are the obvious candidates and would be strongly felt.
  `pitchBallSpeedFactor` is the only consumer found so far; [LEGACY.md](LEGACY.md)
  §6 implies more. **Strengthened to a near-answer**: an independent trace of the
  Amiga original finds the same three constants latched once at kick-off and no
  other consumer of the pitch type anywhere in the match module
  ([amiga/BALL.md](amiga/BALL.md) §5). Two binaries, one consumer each. The surface
  affects the ball and nothing else.
- **Which index is which surface, in the Amiga binary.** The tables match this
  document's exactly, so the naming almost certainly carries over — but the Amiga's
  index comes from outside its match overlay and the mapping is inferred there, not
  read.
- `kPitchPatternWidth` / `kPitchPatternHeight` are used but not located — they live
  in the generated `pitchDatabase.h`. Confirm 42 × 53 directly rather than by
  arithmetic, and reconcile with `docs/rendering.txt`'s "55x42".
- **The invisible top row** (§6) — why the grid has a row that is never drawn.
- What `gamePitchTypeOrSeason` and `plg_D0_param` actually are, and which game modes
  set them. The friendly-vs-career split in §3 is inferred from a comment.
- `kTrainingPitchIndex`, `kNumPitches` — values not read.
- Whether `DrawAnimatedPatterns` had real content in the original, and what.
- Whether the original's pitch-type roll consumed the RNG in the same order — the
  friendly path draws and the career path does not, which matters for replay
  determinism.

---

## 9. Guidance for the reimplementation

- **Keep type and number separate from the start.** They are orthogonal, they are
  chosen by different mechanisms, and conflating them into one "pitch" value makes
  the seasonal weather model impossible to express.
- **Copy the seasonal probability table as data, not code.** Twelve rows of seven
  percentages is the entire weather system, it is trivially moddable, and it is one
  of the cheapest sources of "this feels like a real season" in the whole game.
- **Steal the derive-from-hash trick** (§3) for any per-entity cosmetic variation —
  ground appearance, crowd colour, whatever. It gives stable variety with zero
  storage and no save-file migration.
- **Model the surface as a small struct of physics coefficients**, looked up once at
  match start and passed to the ball, rather than as an enum consulted at every
  physics site. [BALL.md](BALL.md) §3 shows the friction site already branches on
  possession; it should not also branch on pitch type.
- **Do not couple the surface to physics beyond what is measured.** It is tempting
  to make Muddy slow the players too. That is a design change; make it deliberately
  and after the reference match feels right, not as part of the port.
- **Treat zoom as ours, not theirs.** There is no reference behaviour to match, so
  design it for our target resolutions. But keep `getMinimumZoom`'s constraint — never
  let the view exceed the pitch — because the renderer has no defined behaviour
  outside it.
- **Keep the empty-cell marker.** Holes in the grid cost nothing and make irregular
  pitch shapes possible later.
