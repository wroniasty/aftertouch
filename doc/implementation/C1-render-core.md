# C1 — Render core

Real match pixels: 320×200 logical space, 672×848 pitch world, tile grid from
`IAssetSource`, ball sprite, crisp integer-scale presentation. Replaces the C1a
green fill. Camera modes stay C2; kit sheets stay C3; HUD chrome / replays stay C4.

Depends on: A4, B2, C1a   Blocks: C2, C3   Wave: 4

**Status.** In progress — tiled pitch, game-palette sprites for players/ball (static
octant frame; full anim tables are C3), C1a dest-line / yellow ball overlays removed.

---

## 0. One-paragraph version

C1 wires `IAssetSource` into the hot path. Indexed 16×16 pitch tiles and their
palette expand into SDL textures once; each frame the visible window of the 42×53
world grid is blitted through the existing pitch→screen mapper. The sacred 320×200
logical frame and integer-scale letterbox stay. Widescreen does **not** show more
pitch. C1a landmarks and player dots remain as overlays until C2/C3 retire them.

---

## 1. Scope

### In

- Consume `IAssetSource::Pitch` + pitch palette pack (`pitch1.atl`)
- 672×848 world ↔ 42×53 drawable rows (42×55 matrix = pad + 53 content + pad;
  world row `r` reads matrix row `r+1`)
- Empty cell `0xFFFF` skipped
- Ball sprite from `Ball()` when present; synthesised fallback if import omits `ball.atp`
- Upscale at draw via `PitchUniformScale` (nearest / integer-friendly)
- Offscreen-friendly pure helpers: grid index, row clamp, indexed→RGBA expand
- Retire solid green fill as the primary pitch (fallback only if packs missing)

### Out

| Excluded | Owner |
|---|---|
| Five camera modes, lead-ahead, dual clip | C2 |
| Kit bake, octant anim, Y-sort sprites | C3 |
| Scoreboard chrome, cards, replays | C4 |
| Audio | C5 |
| Bench UI | C6 |
| Seasonal surface roll / pitch-number hash (selection) | later; physics `MatchSurface` already exists |
| Porter zoom / “extra side tiles” | rejected — sacred frustum |

---

## 2. Design

### 2.1 Spaces

| Space | Size | Role |
|---|---|---|
| Pitch world | 672×848 | Tile grid; physics barrier sits inside |
| Tile | 16×16 | `kPitchTileSize` |
| Drawable grid | 42×53 | `row = y/16`, `col = x/16`; matrix row = world+1 when h=55 |
| Logical frame | 320×200 | Sacred; integer-scaled to window |
| View window | C1a `DebugView` until C2 | Follow or full dead-ball box |

### 2.2 Asset seam

`ImportedAssets` must open when `slotA_blk0` + `pitch1` exist even if `ball.atp` is
absent (current original import). Load `pitch1.atl` into `PitchTiles` palette spans.
Placeholder uses a small default green/grey ramp when no `.atl` is present.

### 2.3 Draw order

1. Clear / letterbox bars (caller)
2. Tiled pitch for visible cols/rows
3. C1a landmarks (light overlay; removable once tiles carry markings)
4. Player dots (until C3)
5. Ball sprite (anchor-centred) + dest guide / shadow

### 2.4 GPU cache

`PitchAtlas` owns per-tile `SDL_Texture*` built once from indexed pixels + palette.
Rebuild if the `PitchTiles*` identity changes. No SDL types in `at_asset_source`.

---

## 3. Interfaces

| Path | Role |
|---|---|
| `asset_source.hpp` | `PitchTiles` gains palette span; world-row constant |
| `imported_assets.cpp` | Optional ball; load `pitch1.atl` |
| `pitch_atlas.{hpp,cpp}` | Expand + cache + `Draw` |
| `match_renderer.*` | `DrawMatch(..., IAssetSource*, PitchAtlas*)` |
| `main.cpp` | Own atlas; pass assets |
| `test_pitch_tiles.cpp` | Grid / expand / row clamp (no SDL) |

Wall: no SDL in `src/core/` or `at_asset_source`.

---

## 4. Work items

1. Subfile + CURRENTSTATE — this document.
2. Palette on `PitchTiles`; `ImportedAssets` optional ball + `.atl`.
3. Pure tile helpers + unit tests.
4. `PitchAtlas` + wire `DrawMatch` / `main`.
5. Manual: MATCH shows imported grass tiles; V toggles follow/full; placeholder path still runs.

---

## 5. Tests and acceptance

**Automated**

- `GridIndex` / empty / row clamp for 55→53
- Indexed expand: known 2×2 + palette → RGBA bytes
- Existing `test_pitch_to_screen` unchanged
- Placeholder `OpenAssetSource` still succeeds without generated/

**Manual**

1. With `assets/generated/` present, log says `assets: imported` and MATCH shows tiled pitch (not flat green).
2. Placeholder-only clone still draws a tiled (blocky) pitch.
3. Integer scale stays crisp when resizing the window.

**Done when:** pitch renders crisply at arbitrary window sizes with square pixels from
real packs — PLAN.md C1 done-when.

---

## 6. Open questions

- A4 §6.2 surplus rows: **resolved** — row 0 and 54 are pad (all tile 0); content is
  rows 1–53.
- Faithful original pitch `.dat` import still outstanding in A4; C1 accepts ref-tree
  resampled tiles for now.
