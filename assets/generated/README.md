# assets/generated/

Output of `assetc`. **Everything in this directory except this README is gitignored and
must stay that way.**

These files are a converted form of the rightsholder's artwork, read from *your own*
installation. Converting it into our format changes nothing about who owns it. The
import-don't-redistribute rule is [doc/PLAN.md](../../doc/PLAN.md) §10, and `assetc`
enforces it in code: it refuses to write anywhere inside the tracked tree except here.

Delete the whole directory whenever you like. It is reproducible from your install in a
couple of seconds and the game falls back to placeholder art without it.

---

## How to regenerate

```
# Sprites, from an original installation -- the faithful source.
assetc --swos-dir "D:\SWOS 2020\swos" --out .\assets\generated

# Pitches, currently still from the reference port's extracted PNG tree.
assetc --source-dir "...\swos-port\assets" --out .\assets\generated --pitch 1
```

Binary at `build\win-debug\src\tools\assetc\assetc.exe`.

---

## What is here

| File | Contents | Source |
|---|---|---|
| `palette.atl` | 256-colour game palette + the kit-layer routing table | `pal.256` @ `0xFA00` |
| `charset.atp` | 227 sprites: fonts, menu glyphs, faces, trophies, stars | `charset.dat` |
| `score.atp` | 114 sprites: score and time digits | `score.dat` |
| `slotA_blk0..2.atp` | 3 × 101 outfield animation frames | team file in slot A (`team1.dat`) |
| `slotB_blk0..2.atp` | 3 × 101 outfield animation frames | team file in slot B (`team2.dat`) |
| `keepers.atp` | 58 goalkeeper frames | `goal1.dat` |
| `bench.atp` | 12 bench figures | `bench.dat` |
| `pitch1.atp` | 215 tiles of 16×16, plus the 42×55 tile index matrix | `pitches/pitch1/` PNGs |
| `pitch1.atl` | Palette for that pitch | — |

### Why the player banks are named after slots

The original loads **two** team files, one per playing side, and there are three to
choose from (`team1/2/3.dat`) holding the different shirt geometries. So band 644 is
"coloured sleeves" or "horizontal stripes" depending on which file was loaded — the name
would describe a *run*, not the data. Slot and block index are the only labels that are
always true. See [A4](../../doc/implementation/A4-asset-pipeline.md) §6.5; C3 resolves
which block a `ShirtType` selects.

### Two different fidelities in one directory

The sprite packs are **faithful**: 4-bit planar pixels read straight from the original,
verified by all 1334 sprite headers self-identifying correctly.

The pitch packs are **not**. They come from the reference port's extracted PNGs, which
are a 12× LANCZOS upscale — 53 % of tiles' blocks are blends, and downscaling recovers
175 colours from 16-colour art by plurality vote. Good enough to see a pitch; not good
enough for the pixel-identical comparison A4 exists for. Reading `pitch*.dat`/`.blk`
directly is outstanding work. `assetc` prints a warning whenever it produces one of
these, and the `source` byte in each pack header records which kind it is.

---

## The container format

One format for all packs: `.atp` and `.atl` differ by extension only, for human benefit.
Little-endian throughout, written byte by byte rather than by struct layout, so a pack
built on one platform loads on the other. Definition and validator:
[src/assets/include/assets/asset_pack.hpp](../../src/assets/include/assets/asset_pack.hpp).

### Header — 48 bytes

| Offset | Size | Field |
|---|---|---|
| 0 | 4 | magic `"ATAP"` |
| 4 | 2 | format version (currently 1) |
| 6 | 2 | kind: 0 sprites, 1 pitch, 2 palette |
| 8 | 4 | entry count |
| 12 | 4 | entry table offset (always 48) |
| 16 | 4 | blob offset |
| 20 | 4 | blob size |
| 24 | 4 | aux offset (0 if none) |
| 28 | 4 | aux size |
| 32 | 2 | aux width |
| 34 | 2 | aux height |
| 36 | 1 | source: 0 placeholder, 1 reference tree, 2 original install |
| 37 | 3 | reserved |
| 40 | 8 | fingerprint — FNV-1a over the inputs |

### Entry table — 16 bytes each, `entry_count` of them

| Offset | Size | Field |
|---|---|---|
| 0 | 2 | width in pixels |
| 2 | 2 | height in pixels |
| 4 | 2 | anchor x, **signed** |
| 6 | 2 | anchor y, **signed** |
| 8 | 4 | pixel offset, relative to blob offset |
| 12 | 4 | pixel byte count — always width × height |

The anchor is the sprite's visual centre, carried per frame because a player's centre
shifts as he turns. It is signed: a trimmed sprite's centre can sit outside its own box.

### Blob

One byte per pixel, row-major, no padding, no compression. **Values are palette
indices, not colours** — index 0 is transparent.

Storing indices rather than RGB is the whole kit system. In the original data the layer
split is a property of the index, so one byte carries the pixel *and* which tintable
part of the player it belongs to. Expanding to RGB here would throw that away and force
six near-empty masks per frame to get it back.

### Aux section

Kind-specific side data.

- **Pitch packs:** the tile index matrix, `aux_w` × `aux_h` cells, **2 bytes per cell**,
  little-endian. 42 × 55 for pitch 1. Note 55 rows, while the playable field is 42 × 53
  = 672 × 848 pixels; the surplus rows are crowd/border. Eight bits per cell would fit
  today's 215 tiles and would break on the first pitch with more than 256.
- **Palette pack:** the 16-entry kit-layer routing table (below).
- **Sprite packs:** unused.

---

## palette.atl

One entry: 256 colours × 4 bytes RGBA. The original stores 6-bit VGA components (0–63);
these are expanded as `v << 2 | v >> 4`, so 63 becomes 255. The reference port's PNGs use
a plain `<< 2`, which leaves white at 252 — a small thing that would quietly show up in
any pixel comparison against them.

The aux section holds the **kit-layer routing table**: 16 bytes, one per palette index.

| Value | Layer | Palette indices |
|---|---|---|
| 0 | background — never recoloured | 0, 1, 2, 3, 7, 8 |
| 1 | skin | 4, 5, 6 |
| 2 | hair | 9, 12, 13 |
| 3 | shirt base | 10 |
| 4 | shirt stripes | 11 |
| 5 | shorts | 14 |
| 6 | socks | 15 |

This is what makes a kit. At kickoff, C3 walks a frame's indices once, looks each up in
this table, and writes the team's colour for that layer — one pass, no masks, no
intermediate surfaces. The table ships in the data so the runtime never hardcodes it.

Kit colours in the team database are 0–9 and map non-contiguously onto palette ordinals
`1, 2, 3, 6, 10, 11, 12, 13, 14, 15` ([doc/RENDERING.md](../../doc/RENDERING.md) §5).

**Sprites imported from the original look violently red, blue, green and yellow.** That
is correct and is not a bug: those are the raw placeholder indices — 10 bright red,
11 bright blue, 14 green shorts, 15 yellow socks — sitting there untinted until a team's
colours are applied.
