# A4 — Asset pipeline

`src/tools/assetc/` — a command-line importer that reads art we are legally entitled to
read and writes **our** runtime format into a gitignored directory — plus `IAssetSource`
with placeholder and imported implementations, and the first-launch import prompt. It
excludes the renderer that consumes the output (C1), runtime kit compositing (C3), team
and player data (A5) and audio (C5). A4 delivers bytes on disk and one interface; it
draws nothing.

Depends on: A1   Blocks: C1, C3, and A3's ability to *see* a divergence   Wave: 1

---

## 0. One-paragraph version

[../PLAN.md](../PLAN.md) §10 pulls this into Wave 1 for one reason: during trace-diffing
you want pixel-identical sprites, because a number saying divergence began at tick 340
is worth much less than a picture of tick 340. That reason dictates the whole design.
**Original-resolution pixels are the deliverable, not pretty ones** — the reference's
extracted PNG tree is a 12× upscale, and importing it as-is would mean comparing our
frame against the porters' resampling rather than against the game. Investigation of the
tree turned up three things worth designing around: the 12× upscale is *nearly* but not
quite lossless (167 of 180 blocks uniform, every non-uniform block with an unambiguous
139-of-144 majority), the non-shirt layers were flattened to greyscale **and brightened
×2.2** to work around SDL's colour-mod only being able to darken, and palette values are
6-bit VGA scaled by `<<2`, so peak white is 252 rather than 255. Each of those is a
one-way transformation the porters applied for their renderer's benefit. So the tree is
a **bootstrap, not ground truth**, and `assetc` is built with two interchangeable source
adapters from the start — the tree we have today, and the original installation
[../PLAN.md](../PLAN.md) §10 requires — behind one output format, so that switching costs
a flag rather than a rewrite.

---

## 1. Scope

**In:**

- `src/tools/assetc/`: the importer, its two source adapters, and its output writer.
- Our runtime asset format: container layout, manifest, versioning.
- `IAssetSource` with `PlaceholderAssets` and `ImportedAssets`.
- The committed placeholder art, and the generator that produces it.
- First-launch detection and the import prompt.
- The gitignore and licence discipline that keeps original artwork out of history.
- Fuzzing the importer, per [../PLAN.md](../PLAN.md) §7.

**Out:**

| Excluded | Owner |
|---|---|
| Drawing anything — atlases, batching, the tile grid, camera | C1 |
| Runtime kit compositing and the animation frame stepper | C3 |
| Team, player and tactics data; the fictional default dataset | A5 |
| Audio assets and the commentary queue | C5 |
| Menu art beyond what the shell already needs | D1, D4 |

A4's line is that it converts and serves bytes. The moment a decision is about how
something *looks on screen*, it belongs to C.

---

## 2. Design

### 2.1 What is actually in the reference tree

Measured, not assumed. `../reference/swos-port/assets/`:

| Path | Contents |
|---|---|
| `sprites/game/` | 215 loose `spr####.png` (+ `.txt` sidecars) and four subtrees |
| `sprites/game/player/` | Six layer directories — `background`, `hair`, `skin`, `shirt`, `shorts`, `socks` |
| `sprites/game/goalkeeper/` | The same, minus `shirt` |
| `sprites/game/bench/` | `background` and `shirt` only |
| `sprites/game/stadium/` | Four composed stadium-crowd variants |
| `sprites/menu/` | Menu art |
| `pitches/pitch1…6/` | 215 tile PNGs plus a `pitch<n>.txt` index matrix each |

Concrete numbers, all verified against the files:

- **Player frames: 101**, matching `kPlayersStart = 341 … kPlayersEnd = 441`. Each layer
  directory holds 101 frames on an identical 144×180 RGBA canvas.
- **Original sprite size is 12×15.** 144×180 is a **12× upscale** —
  `convertGameSprites.py` resizes by `kResMultipliers`-compatible 12 before saving.
- **`shirt/` holds 303 = 3 × 101**, the three geometry blocks
  ([../PLAYER_SPRITES.md](../PLAYER_SPRITES.md) §3): offset 0 ordinary and vertical
  stripes, 101 horizontal stripes, 202 coloured sleeves.
- **`.txt` sidecars are the anchor**, two integers, centre x then centre y, stored at
  **original × 12** — so `spr0326.txt` reading `408 / 60` means an original anchor of
  `(34, 5)`.
- **Pitch tiles are 192×192**, a 12× upscale of **16×16**. `pitch<n>.txt` is a
  **42-column × 55-row** matrix of tile indices, against `compileAssets.py`'s
  `kPitchWidth = 42`, `kPitchHeight = 53`. 42 × 53 × 16 = **672 × 848**, which is exactly
  the pitch world space [../PITCH.md](../PITCH.md) §4 and
  [PLAN.md](PLAN.md) C1 both name. The two surplus rows are §6.2.
- **Palette-index → layer routing**, from `convertGameSprites.py`: skin 4/5/6, hair
  12/9/13, shirt 10 (base) and 11 (stripes), shorts 14, socks 15, index 7 a marker, all
  else background.

### 2.2 The tree is a bootstrap, not ground truth

Three transformations were applied on the way out of the original data, and all three
are one-way:

**The 12× upscale is lossy, and much more so than a first sample suggests.** Default
`convertGameSprites.py` resizes with `Image.LANCZOS`; only its `--blocky` mode uses
`NEAREST`.

On sprites it looks survivable. `player/background/spr0000.png`: 167 of 180 12×12
blocks are uniform, and each of the 13 that are not has a dominant colour holding
119–143 of its 144 pixels, strays confined to a single row.

**On pitch tiles it is not.** Measured across all 215 tiles of `pitch1`, not a sample:

| | |
|---|---|
| Fully lossless tiles | **4 of 215** (1.9 %) |
| Non-uniform blocks | **29 166 of 55 040** (53.0 %) |
| Weakest block decision | **36 of 144 pixels** — a 25 % plurality, not a majority |
| Distinct colours recovered | **175**, from art that is 16-colour |

The four lossless tiles are the flat-colour ones, which is exactly why sampling the
first two tiles gives the wrong answer: they are flat. A majority-vote downscale
resolving a block on 36 of 144 pixels is not recovering an original pixel, it is
picking the most common LANCZOS blend, and 175 colours out of a 16-colour palette is
the same fact stated another way.

**So the tree cannot serve the purpose A4 was pulled into Wave 1 for.** It is good
enough to get a pitch on screen and to build and prove the entire pipeline — which is
real value, and the reason the work is not wasted — but the pixels it yields are the
porters' resampling, not the game's, so comparing our frame against a reference frame
would compare us against LANCZOS. `assetc` therefore *measures and reports* this every
run and warns when a source looks blended, rather than leaving the caller to assume
fidelity it does not have (§6.1).

**The non-shirt layers are greyscale and brightened ×2.2.** `saveImage(...,
grayscale=layer != Layer.kShirt)` flattens colour, then applies
`ImageEnhance.Brightness(2.2)` with the comment *"since SDL's color mod only subtracts
color and the image ends up too dark"*. These are therefore not the masks the original
had; they are masks pre-distorted for one specific renderer's tinting operation.
Dividing by 2.2 to undo it clips wherever the multiply saturated.

**Palette values are 6-bit VGA scaled by `<<2`.** Peak channel across every sampled file
is 252, not 255 ([../RENDERING.md](../RENDERING.md) §5: 256 colours, components 0–63).
Correct expansion to 8 bits is `v << 2 | v >> 4`; the tree used `v << 2`, so whites are
1.2 % dark.

None of this makes the tree useless — it makes it a **bootstrap**. It is what we have,
it is enough to build the whole pipeline and see a match on screen, and every measurement
taken off it that claims to be *the original's pixels* has to be re-derived later. So
the design does not treat it as authoritative for a single moment.

### 2.3 Two source adapters, one output

```
ISpriteSource
  ├── RefTreeSource        reads ../reference/swos-port/assets/  (today)
  └── OriginalSource       reads an owned installation           (PLAN.md section 10)
```

Both produce the same intermediate: **indexed pixels at original resolution, plus a
palette, plus an anchor**. `RefTreeSource` gets there by per-block majority downscale
(§2.2) and by inverting the `<<2` palette scaling; `OriginalSource` gets there by reading
the 16-colour source data directly, which is what the format was in the first place.
`assetc --source=reftree|original` selects, and the output is byte-identical wherever the
tree is faithful — which is a **testable claim**, and §5 tests it.

Writing the second adapter is not deferred vaguely to "later": it is work item 6, in this
part, because a source abstraction with one implementation is a guess and a source
abstraction with two is a design. What is deferred is *using* it, which needs an
installation nobody in this repo is required to own.

**We store palette indices, not layer masks.** This supersedes the six-coverage-masks
design this section originally specified, and implementing the original reader is what
showed why.

In the original data **the layer split is not stored anywhere** — it is a property of
the palette index. Index 14 is shorts because index 14 is shorts. Verified: the player
bank uses indices `0 1 2 3 4 5 6 8 9 10 11 12 13 14 15`, exactly the documented routing
(skin 4/5/6, hair 9/12/13, shirt 10, stripes 11, shorts 14, socks 15, the rest
untouched), with 7 absent.

So one byte per pixel carries both the pixel *and* its layer, losslessly. Splitting it
into six masks would store the same information six times, mostly as empty space, and
would need a decision about what a mask's value means. The reference had to do that
because SDL colour-mod needs separate surfaces to blit; we composite from indices in a
single pass and have no such constraint.

The format therefore stores indexed pixels, the 256-colour palette, and a 16-entry
**kit-layer routing table** in the palette pack's aux section — so the runtime reads the
mapping rather than hardcoding it. The reference's greyscale-and-×2.2 simply never
happens: there is nothing to undo, because we never leave the index domain until C3
tints.

The shirt's two tintable regions ([../RENDERING.md](../RENDERING.md) §4) fall out for
free as indices 10 and 11, rather than needing the reference's red-and-blue-channel
encoding to reconstruct them.

### 2.4 Our runtime format

`assets/generated/`, gitignored, written by `assetc` and read by `ImportedAssets`:

```
manifest.atm        format version, source kind, source fingerprint, table of contents
sprites.atp         sprite bank: per entry -> size, anchor, layer count, pixel data
players.atp         101 frames x 6 layers, coverage masks
keepers.atp         101 frames x 5 layers
pitch<n>.atp        tile bank + the 42x53 index matrix
palette.atl         the 16-colour game palette and the ten kit-colour ordinals
```

Four properties the format is designed for, in priority order:

1. **Original resolution.** Stored at 1×; any upscaling is C1's decision at load or draw
   time. This is the whole reason A4 is in Wave 1, and storing the porters' 12× would
   throw it away on the first commit.
2. **Loadable with one read and no parsing.** Fixed-width tables, offsets not pointers,
   so `ImportedAssets` maps a file and indexes it.
3. **Fingerprinted.** The manifest records which source produced it and a hash of the
   inputs, so a stale `assets/generated/` after a source change is detected rather than
   silently used — the asset equivalent of A3's hash chain.
4. **Versioned from the first write**, because the second version always arrives sooner
   than expected.

The **ten kit colours map non-contiguously** onto palette ordinals `1, 2, 3, 6, 10, 11,
12, 13, 14, 15` ([../RENDERING.md](../RENDERING.md) §5). That table is the only bridge
between [../DATA.md](../DATA.md)'s colour indices and actual RGB, so it lives in
`palette.atl` rather than in renderer code where A5 cannot reach it.

### 2.5 `IAssetSource` and the placeholder path

[../PLAN.md](../PLAN.md) §10 point 4 is not a formality: *"if the build only works on
machines that happen to have a game installed, you have made the project fragile"*.

```cpp
class IAssetSource {
public:
    virtual ~IAssetSource() = default;
    virtual const SpriteSheet* Player(TeamSlot, Dir, int frame) const = 0;
    virtual const SpriteSheet* Ball() const = 0;
    virtual const PitchTiles*  Pitch(PitchType) const = 0;
    virtual bool               IsPlaceholder() const = 0;
};
```

`PlaceholderAssets` is **generated, not drawn**: `tools/gen_placeholder.py` emits
coloured rectangles at the correct sizes with the correct anchors, and its output is
committed under `assets/placeholder/`. Generated because hand-maintained placeholder art
rots, and committed because the build must not need Python to produce a runnable game —
the same rule [A2](A2-determinism-primitives.md) §2.3 applies to the trig tables.

The critical property is that placeholder art is **dimensionally identical** to imported
art: same frame count, same 12×15 canvas, same anchors, same 101-frame indexing. A bug
that only appears with real assets is a bug the placeholder path was supposed to catch,
and it is only caught if the two are interchangeable in every respect except colour.

`IsPlaceholder()` exists so the shell can say so. A player who has imported nothing
should be told that, once, rather than concluding the game looks like that.

### 2.6 First launch

On startup, if `assets/generated/manifest.atm` is absent or its version is stale, the
shell offers the import and otherwise proceeds on placeholder art. It never blocks,
never refuses to start, and never writes into the repository. The prompt is a screen
behind `IUiBackend` like any other ([../PLAN.md](../PLAN.md) §6), which means D1 owns
its appearance and A4 owns only the `Intent` it returns.

### 2.7 The licence discipline

[../PLAN.md](../PLAN.md) §10 is unambiguous and this part is where it is enforced:

- `assets/generated/` and `*.swosdata` are gitignored — already true, extended to our
  own extensions.
- `assetc` **refuses to write anywhere inside the repository's tracked tree**, checked at
  runtime rather than trusted. The failure mode this prevents — an importer that quietly
  fills a tracked directory with the rightsholder's artwork — is not recoverable by
  `git rm`.
- The three reference Python scripts are read as format documentation and **not
  vendored**; where this document quotes their constants, it quotes behaviour, which is
  the same standing the engine documents already have.
- A test asserts that no file under `assets/generated/` is tracked.

---

## 3. Interfaces

New under `src/tools/assetc/`: `main.cpp`, `source_reftree.cpp`, `source_original.cpp`,
`writer.cpp`, `sprite_decode.cpp`, `pitch_decode.cpp`.
New under `src/app/render/`: `asset_source.hpp`, `placeholder_assets.cpp`,
`imported_assets.cpp`.
New under `tools/`: `gen_placeholder.py`.
New under `assets/`: `placeholder/` (committed), `generated/` (ignored).

What other parts see:

- **C1** consumes `IAssetSource::Pitch` and the tile index matrix; it never learns where
  the art came from.
- **C3** consumes the layer masks and does the compositing. A4 hands it separated layers
  and an anchor; the kit model, the cache and the face variants are C3's.
- **A5** reads `palette.atl` for the kit-colour ordinals, which is the only overlap
  between the art pipeline and the data pipeline.
- **A3** benefits without depending: pixel-identical sprites are what make a divergence
  visible rather than merely counted.

The wall: none of this is in `src/core/`. `IAssetSource` lives in `src/app/render/` and
mentions no SDL type in its interface, so a future non-SDL backend implements it
unchanged.

---

## 4. Work items

1. **The format and the writer.** `manifest.atm`, the container layout, versioning,
   fingerprinting. Written before either reader, so neither reader defines it by
   accident. → `test_asset_format.cpp`.
2. **`RefTreeSource`: pitch tiles.** The easy half — tiles downscale exactly (§2.2) and
   the index matrix is plain text. Produces a complete, loadable `pitch1.atp` and proves
   the format end to end. → `test_pitch_import.cpp`.
3. **`RefTreeSource`: sprites and layers.** Majority-vote downscale, anchor recovery from
   the `.txt` sidecars, the six-layer split, the three shirt blocks. → `test_sprite_import.cpp`.
4. **`PlaceholderAssets` and the generator.** Dimensionally identical to imported art.
   → `test_placeholder_parity.cpp`.
5. **`IAssetSource` and `ImportedAssets`.** Loading, the stale-fingerprint check, the
   `IsPlaceholder` path.
6. **`OriginalSource`.** The clean-hands adapter (§2.3). Cannot be fully tested without
   an installation; its tests are the ones that can run without one.
7. **First-launch prompt and the write-location guard** (§2.6, §2.7).
8. **Fuzzing.** [../PLAN.md](../PLAN.md) §7: the importer reads third-party binary data
   and is the one place a crash is likely and a silent misparse is worse.

Items 1–5 are unblocked today. Item 6 is unblocked in code and untestable in full.

---

## 5. Tests and acceptance

[../PLAN.md](../PLAN.md) §7 does not give A4 a layer technique of its own — it is a tool,
not the engine or the presentation. What it does give A4 is an explicit instruction to
fuzz it, and a demonstrable done-when that is really two claims.

| Test | What it pins |
|---|---|
| `test_asset_format.cpp` | Round-trip of every container; a version mismatch is rejected; a stale fingerprint is detected rather than used; offsets stay in bounds on a truncated file. |
| `test_pitch_import.cpp` | Every tile downscales to 16×16; the index matrix parses to 42 columns and every cell references a tile that exists. The fidelity counters are asserted **at their measured values** (4 lossless tiles of 215, 175 palette colours) rather than at the values we would like — a test that asserted exactness would have to be deleted, and a test pinned to reality turns any change in the source tree into a signal. |
| `test_sprite_import.cpp` | 101 frames per layer; `shirt` yields exactly 3 × 101; anchors divide by 12 exactly; the majority-vote downscale is deterministic and produces 12×15. |
| `test_placeholder_parity.cpp` | **The important one.** Placeholder and imported sources agree on every dimension, frame count, anchor and index — everything except pixel colour. This is what makes the no-assets build a real test rather than a different one. |
| `test_import_guard.cpp` | `assetc` refuses a write target inside the tracked tree; no file under `assets/generated/` is tracked. |
| `fuzz_sprite_decode.cpp` | Truncated, malformed and adversarial inputs to both adapters. Entry point here, corpus and CI in A6. |

**Golden data:** a handful of small expected outputs — one pitch tile, one player frame
per layer — committed as our own format, which contains no original artwork because it
is generated from placeholder inputs. Original-derived bytes are never committed, which
means the import path's correctness is asserted structurally rather than by golden image
comparison. That is a real limitation and it is why §2.3's byte-identity claim between
the two adapters carries weight: it is the one check that catches a wrong import without
committing the thing being imported.

**The demonstration that closes the part**, both halves required, exactly as
[PLAN.md](PLAN.md) §3 states it:

1. A clean clone with no original data and no reference checkout **builds, runs and
   passes its tests** on placeholder art.
2. With the reference tree present, `assetc` imports it into `assets/generated/`,
   the app loads it, and `git status` is clean afterwards.

---

## 6. Open questions

**6.5 — Which shirt geometry is which bank? — RESOLVED (C3)**
The geometry is the team FILE, and the three blocks inside one file are the three faces,
not three geometries: `slotA_blk0/1/2` are byte-identical across all 101 frames, while
`team1/2/3.dat` differ in every frame. Reading the shirt indices off a front standing
frame names them — team1 alternates 10/11 across the torso (vertical stripes), team2 is
uniform per row (horizontal stripes), team3 puts 11 on the sleeve columns (coloured
sleeves), which is what `docs/SWOS/sprites.txt` said and not what
`convertGameSprites.py`'s block names say. `assetc` now imports 101 frames per geometry
as `kit_vstripe` / `kit_hstripe` / `kit_sleeves`, and faces cost nothing because they
are a palette. The original question, for the record:

A team file holds 303 sprites — three distinct 101-frame blocks, verified not to be
copies — and the game loads two team files, one per playing side.
`docs/SWOS/sprites.txt` says the three *files* are the geometries (team1 vertical
stripes, team2 coloured sleeves, team3 horizontal stripes);
[../PLAYER_SPRITES.md](../PLAYER_SPRITES.md) §3 says the three *blocks within a bank*
are (offset 0 ordinary/vertical, 101 horizontal, 202 coloured sleeves). Both cannot be
the whole story. Rendering settles part of it: with team1/team2 loaded, band 644 shows
contrasting sleeves, not the horizontal stripes `convertGameSprites.py` names it — that
script's constants encode the configuration of whichever extraction produced its BMPs.
**Resolved by:** C3, which is what actually needs to pick a block from a `ShirtType`.
Until then `assetc` names banks by slot and block only, so no run's configuration is
baked into a filename.

**6.1 — Where do faithful pixels come from? — RESOLVED, pitches included**
The sprites came from an original installation, and so does the pitch: `pitchN.blk` is
863 chunky 8-bit 16×16 tiles at 256 bytes each and `pitchN.dat` is the 42×55 matrix of
BYTE OFFSETS into it. The reference tree's resampled pitch art is no longer used by
anything. Original text follows:

The answer is an original installation, and one is now in use. `assetc --swos-dir`
reads `sprite.dat`, `charset.dat`, `score.dat`, the team files, `goal1.dat` and
`bench.dat` directly: 4 bits per pixel, planar, against `pal.256`. No resampling, no
plurality vote, no ×2.2. **1334 of 1334 sprite headers self-identify correctly**, which
is a decisive check rather than an impression — each header carries its own ordinal at
+22, so the offset table, the block assembly, the header layout and the planar decode
all have to be right simultaneously for that to hold.

The measured blockers below are kept as the record of *why* the reference tree was
abandoned as a source, not as outstanding work:
§2.2 settles the measurement and the answer is bad: on pitch tiles the reference tree
resolves 53 % of blocks by vote, some on a 25 % plurality, yielding 175 colours from
16-colour art. **Majority-vote recovery is not viable on this source.** Two ways out and
one dead end:

- *An original installation* (work item 6) — reads 16-colour data directly and the
  question disappears.
- *Regenerating the tree with `convertGameSprites.py --blocky`*, whose `NEAREST` upscale
  downscales exactly. This was the cheap option and **it is currently blocked**: the
  script reads `sprites/src/*.bmp`, and that directory is not in the reference tree we
  have. Only the converted output was kept, so the conversion cannot be re-run.
- *Dead end:* snapping the 175 recovered colours back onto a 16-colour palette. Without
  knowing the true palette that is guesswork dressed as precision, and it would produce
  something that looks right and is not.

**Resolved by:** obtaining either an original installation or the source BMPs. Until one
of those exists, imported art is explicitly a bootstrap and the importer says so on every
run. Nothing else in A4 is blocked by this — the format, the writer, the guard, the
placeholder path and the runtime `IAssetSource` are all independent of source fidelity,
which is why they were built first.

**6.2 — Why is the matrix 55 rows when `kPitchHeight` is 53? — RESOLVED (C3)**
Two different artefacts. The reference tree's matrix has a leading pad row; **the
original's does not** — rows 0 and 54 carry the stands, and mapping matrix row to world
row 1:1 puts the engine's own constants exactly on the painted markings (centre spot
`(336,449)`, touchlines `x = 81 / 590`, goal lines `y = 129 / 769`). The drawable world
is therefore 672×880, which CAMERA.md §9 confirms independently: its hard camera limit
`kCameraMaxY = 680` is `880 − 200`. `PitchTiles::from_original` picks the mapping per
pack from its `SourceKind`. Original text follows:

The matrix parses as 42 × 55; `compileAssets.py` declares 42 × 53; 42 × 53 × 16 is
exactly the 672 × 848 world space every other document agrees on. So two rows are
surplus, and the plausible readings — off-pitch padding for the camera's overscan, or a
tail the compiler ignores — have different consequences for C1's tile grid.
**Resolved by:** reading `getPitches()` / `outputPitchIndices()` in `compileAssets.py`
when C1 needs it. Recorded now because a wrong guess here shifts the entire pitch by two
tiles, which looks like a camera bug.

**6.3 — Is 12×15 the sprite size, or the sprite *canvas* size?**
144×180 is uniform across all 101 frames and all six layers, which is consistent with a
fixed canvas holding variable-sized art positioned by the per-frame anchor. If so, our
format should store trimmed sprites plus offsets rather than 101 mostly-empty canvases —
`background/spr0000.png` is 23188 of 25920 pixels fully transparent, so the waste is
real. **Resolved by:** measuring bounding boxes across all 101 frames during item 3; it
is a format decision and item 1 should leave room for it.

**6.4 — Which resolution the game actually ships at.**
The reference generates three (`kResMultipliers = (12, 6, 3)`) and threads `m_res`
through everything. We store 1× and defer, which is right for A4 and postpones a
question C1 must answer: upscale at load into textures, or draw scaled. **Resolved by:**
C1, which is where it belongs — recorded here only so that storing 1× is understood as a
deliberate deferral rather than an oversight.

Unmeasured constants encountered here go to [../LEGACY.md](../LEGACY.md) §15. None were:
everything above is measured off the tree or read from a named script.
