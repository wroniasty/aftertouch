# RENDERING.md

How pixels reach the screen: the 320×200 logical space everything is authored in,
the scale-and-letterbox mapping to a modern window, the sprite atlas pipeline, kit
colourisation by grey-scale layers, and the 256-colour palette the whole game is
built on. Traced through the reference DOS port in
[../reference/swos-port/](../reference/swos-port/).

> **Reference only — not an implementation basis.** aftertouch renders its own way
> ([PLAN.md](PLAN.md) §5–6: SDL plus an `IUiBackend` abstraction). This document is
> not a specification to follow. It is here for two things: the **palette and kit
> colour mapping** in §5, which we need if we ever render original-style kits or
> import team colours; and the **layer-based colourisation model** in §4, which is a
> genuinely good idea worth stealing regardless of renderer.
> [PLAYER_SPRITES.md](PLAYER_SPRITES.md) already covers sprite compositing and
> animation from the gameplay side and is the implementation-relevant document;
> this one is the plumbing underneath it.
>
> **Provenance.** Two eras mixed, and the split matters more here than anywhere
> else. **The 320×200 logical space, the palette, the sprite centring model and the
> kit layer scheme are the original's.** The atlas pipeline, three-resolution
> assets, SDL renderer, zoom, letterboxing and texture caching are entirely the
> porters' and have no 1994 counterpart.

---

## 0. One-paragraph version

Everything is authored in a **320×200 logical space** (`kVgaWidth`, `kVgaHeight`) and
mapped to the real window by a single uniform `m_gameScale` chosen as
`min(width/320, height/200)`, with the remainder centred — so aspect ratio is
preserved and the excess is filled with **extra pitch tiles horizontally and black
bars vertically**. Sprites live in PNG atlases up to 2048×2048, built offline by a
Python script from individual source images at **three resolutions** (4K, HD,
low-res), each sprite carrying its own centre point so that `drawSprite` can
position by logical anchor rather than corner. Player kits are not stored as
finished art: source sprites are **decomposed into grey-scale layers** — skin,
hair, shirt, shorts, socks — which are tinted and pasted together once per match
into a cached texture, with the shirt layer encoding two colours in the red and
blue channels of one image. Underneath it all is a **256-colour palette** whose
upper 128 entries are computed darkened copies of the lower 128.

---

## 1. The logical space

```
kVgaWidth  = 320
kVgaHeight = 200
```

Every coordinate in menus, HUD and sprite placement is in this space. The pitch is
the exception — it lives in its own 672×848 world space
([PITCH.md](PITCH.md) §4) and is mapped through the camera.

[gameFieldMapping.cpp](../reference/swos-port/src/video/gameFieldMapping.cpp)
does the mapping:

```
scaleX = width / 320
scaleY = height / 200
m_gameScale = min(scaleX, scaleY)          // uniform — no stretching

m_gameOffsetX = (m_fieldWidth  - 320) / 2 * scale
m_gameOffsetY = (m_fieldHeight - 200) / 2 * scale
```

**Uniform scale, centred.** `docs/rendering.txt` states the policy the porters
adopted: *"there is no more stretching, instead the original proportions are kept,
extra space on the sides is filled with additional pitch patterns; if there's still
space (screen is wider than the pitch), it's filled with black bars (same goes with
vertical space, except it's not filled with pitch patterns)"*.

So a widescreen window shows **more pitch**, not a stretched pitch — up to the
pitch's own edges, after which black bars. This is why
[CAMERA.md](CAMERA.md) §7's destination clipping uses `kVgaWidth / 2` for centring
and why [PITCH.md](PITCH.md) §5's minimum zoom exists.

---

## 2. The asset pipeline

Entirely the porters'. From `docs/rendering.txt`:

- Source sprites are **individual PNG files** in `assets/`.
- A sprite with a non-zero centre point carries a sibling `.txt` file: **first line
  centre x, second line centre y**.
- `assets/compileAssets.py` combines sprites and pitch tiles into **PNG atlases up
  to 2048×2048**, loaded as textures.
- **Three resolutions** are generated: 3840×2160, 1920×1080, 960×540. Sources are
  authored at 4K and downscaled into `4k`, `hd` and `low-res` directories.
- The menu background is `assets/<res>/swtitle.jpg`.
- Pitch tiles get the same atlas treatment, with a `pitch<num>.txt` matrix file
  saying which tile goes where ([PITCH.md](PITCH.md) §4).

One authoring constraint is called out: **font character sprites must keep the
original proportions**, because the menus were laid out around a fixed font size.
Other sprites may deviate, "but any larger deviations will have to be thoroughly
tested".

`m_res` threads through everything — `kPatterns[m_res]`, `m_pitchTextures[m_res]`,
`kPatternSizes[m_res]` — and a resolution change fires
`registerAssetResolutionChangeHandler` callbacks that free and reload textures
([PITCH.md](PITCH.md) §6).

---

## 3. Drawing a sprite

[drawSprite()](../reference/swos-port/src/sprites/renderSprites.cpp#L37):

```
x = x + sprite.xOffsetF - xOffset
if (!ignoreCenter) x -= sprite.centerXF
xDest = getGameScreenOffsetX() + x * scale
// same for y

destWidth  = sprite.widthF * scale
destHeight = sprite.heightF * scale
setAlpha(texture, alpha)
```

Three coordinate contributions per axis: the caller's position, the sprite's own
**atlas offset**, and its **centre point** — the last skippable via `ignoreCenter`,
which the menu path uses.

Four entry points wrap it:

| Function | Use |
|---|---|
| `drawMenuSprite` | Resets menu sprite colour first |
| `drawGameSprite` | Plain |
| `drawCharSprite` | Text glyphs; alpha, no colour reset |
| `drawSprite` | The general form; returns whether anything was on screen |

The **centre-point model is the original's** and is what makes
[PLAYER_SPRITES.md](PLAYER_SPRITES.md) §10's depth and height rendering work: a
player is positioned by his feet, and the `z` lift subtracts from screen y.

---

## 4. Kit colourisation by layers

The best idea in this document, and it is the original's.

Player sprites are **not stored as finished art**. From `docs/rendering.txt`:
*"sprites that need to be colorized (containing variable colors for skin, hair,
shirt, shorts, socks...) are broken into grey-scale layers, before the game each
layer is colored to proper color and pasted onto a resulting sprite"*.

[colorizeSprites.cpp](../reference/swos-port/src/sprites/colorizeSprites.cpp)
does this once per match:

```
colorizeGameSprites(res, topTeam, bottomTeam)
  colorizePlayers(...)        → PlayerTextures per face type
  colorizeGoalkeepers(...)    → separate, keepers have their own kit
  colorizeBenchPlayers(...)   → 2 surfaces
```

with `pastePlayerLayer` for ordinary layers and a special
`pastePlayerShirtLayer`, because:

> *"only exception is shirts layer, which contain two colors, they are represented
> with pixels containing red and blue components (shirt and sleeve color)"*

**One grey-scale image encodes two independently tintable regions** by using the red
channel for one and the blue for the other. That is how the four `ShirtTypes`
(plain, coloured sleeves, vertical stripes, horizontal stripes —
[DATA.md](DATA.md) §1) are produced from a small number of source images.

Results are cached: `kPlayerTextureCacheSize = 6` team entries in an LRU
(`std::list` + `trimCache()`), keyed by `TeamGame*`. Three face variants
(`kNumFaces` = white, ginger, black) are generated per team, and
`determineGoalkeeperFaces` handles keepers separately since a team has exactly one.

**Why this matters even though we render differently:** it is a data-compression
strategy that also happens to be a *content* strategy. One set of grey-scale layers
plus a five-colour palette per team yields every kit in the game. Any football game
needs this; SWOS's version is close to minimal.

---

## 5. The palette

From `docs/SWOS/gamePalette.txt`:

- Game palette lives in `pal.256` at offset `0xFA00`.
- **256 colours, 3 bytes each (R, G, B), values 0–63** — VGA 6-bit DAC.
- **The upper 128 colours are discarded.** The game recomputes them as darkened
  versions of the lower half, used for dark overlays such as the result and
  statistics screens.

**Kit colour → palette index:**

| Kit colour | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 |
|---|---|---|---|---|---|---|---|---|---|---|
| Palette ordinal | 1 | 2 | 3 | 6 | 10 | 11 | 12 | 13 | 14 | 15 |

**Ten kit colours**, non-contiguously mapped — indices 4, 5, 7, 8, 9 are skipped.
This is the table needed to interpret the `prShirtCol` / `prStripesCol` /
`prShortsCol` / `prSocksCol` fields in [DATA.md](DATA.md) §1, and it is the one
piece of this document we would need verbatim to render authentic kits.

There is a separate menu palette (`docs/menu-palette.png` alongside
`docs/game-palette.png`).

---

## 6. Frame flow and the rest

[render.cpp](../reference/swos-port/src/video/render.cpp) is thin:

| Function | Notes |
|---|---|
| `initRendering` | |
| `finishRendering` | |
| `updateScreen(delay)` | Present |
| `fadeIn` / `fadeOut` | `kFadeDelayMs = 900`, alpha ramp over a caller-supplied render callback |
| `setLinearFiltering` | Default **on** |
| `setClearScreen` | Default on |
| `makeScreenshot` | Deferred via `m_pendingScreenshot`, executed at frame end |

The fade functions take a `std::function<void()> render` and drive it repeatedly
with a changing alpha — a callback-based transition rather than a state machine.

Other pieces not detailed here: `overlay.cpp` (HUD compositing),
`drawPrimitives.cpp`, `assetManager.cpp`, `windowManager.cpp` (~477 lines, window
and display-mode handling), `windowModeMenu.cpp`, `videoOptionsMenu.cpp`.

Sprite draw ordering — depth sorting by y, the `z` lift, shadow placement — is
[PLAYER_SPRITES.md](PLAYER_SPRITES.md) §10, and pitch-tile batching by texture is
[PITCH.md](PITCH.md) §6.

---

## 7. Constants quick reference

| Constant | Value | Origin |
|---|---|---|
| `kVgaWidth` × `kVgaHeight` | 320 × 200 | original |
| Palette | 256 × RGB, 0–63; upper 128 derived | original |
| Kit colour → palette | `1,2,3,6,10,11,12,13,14,15` | original |
| `kNumFaces` | 3 (white, ginger, black) | original |
| `kNumShirtTypes` | 4 | original |
| Atlas size | ≤ 2048 × 2048 | **port** |
| Asset resolutions | 3840×2160, 1920×1080, 960×540 | **port** |
| `kPlayerTextureCacheSize` | 6 teams, LRU | **port** |
| `kFadeDelayMs` | 900 | **port** |
| Scale | `min(w/320, h/200)`, uniform | **port** |

---

## 8. What this tells us

**Confirmed:**

- Everything outside the pitch is authored in 320×200 and uniformly scaled. ✓
- Widescreen shows more pitch, not stretched pitch; leftover space is black bars. ✓
- Sprites carry their own centre point; three coordinate contributions per axis. ✓
- Kits are grey-scale layers tinted and composited once per match, cached six teams
  deep. ✓
- The shirt layer packs **two** tintable colours into the red and blue channels of
  one image. ✓
- 256-colour palette, 6-bit components, upper half computed as darkened lower half. ✓
- Ten kit colours map non-contiguously onto palette indices. ✓
- Atlas pipeline, multi-resolution assets and texture caching are port additions. ✓

**Open:**

- The **menu palette** mapping, as distinct from the game palette.
- How the original did any of this on Amiga hardware — planar bitmaps, blitter bobs
  and cookie-cut masks per [LEGACY.md](LEGACY.md) §2 — versus the DOS build. The
  port describes only its own approach.
- `overlay.cpp`, `drawPrimitives.cpp` and `windowManager.cpp` are unread.
- Exactly which sprites are layered versus flat, and how many source layers a player
  actually has.
- Where the `xOffsetF` atlas offsets come from in the compile step.

---

## 9. Guidance

- **Steal the layer-based kit model** (§4), including the two-colours-in-one-image
  trick if we author our own kit art. It is the right answer for any game with
  team colours and it is independent of renderer.
- **Keep the palette mapping table** (§5) if we want authentic kits or plan to
  import original team data — it is the only bridge between
  [DATA.md](DATA.md)'s colour indices and actual RGB.
- **Author in one logical space and scale uniformly.** Not because SWOS did, but
  because it is correct, and 320×200 is a reasonable choice if we want the original
  proportions.
- **Cache composited team textures**, keyed by team, sized to a handful. Doing
  colourisation per frame is the obvious mistake; the reference does it once per
  match.
- **Do not copy the frame flow.** `render.cpp` is thin because it sits on SDL2's
  renderer; our [PLAN.md](PLAN.md) §6 architecture puts an `IUiBackend` in the way
  and that is a better structure.
- **Ignore the atlas pipeline as a design.** Three fixed resolutions and a Python
  pre-build step is a reasonable 2010s answer; we should decide ours from our own
  constraints.
