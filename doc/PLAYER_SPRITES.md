# PLAYER_SPRITES.md

How SWOS renders an outfield player, traced end to end through the reference DOS
port in [reference/swos-port/](../reference/swos-port/). Two halves:

1. **Assets & compositing** — what is on disk and how a team's kit is baked into
   textures once per match.
2. **Animation** — how, every tick, the engine picks which of a player's frames
   to draw, in which of eight directions, from which of a set of animation
   scripts.

This is reference documentation for a reimplementation, not a description of
aftertouch's own code (our [assets/sprites/](assets/sprites/) is still empty).
Everything below is cited to a file and line in the reference tree. Companion to
[LEGACY.md](LEGACY.md) §2 and §13.

> **Provenance note.** The reference tree is a decompiled/ported copy of a
> copyrighted binary. Read it to understand the design, then write our own code
> and our own assets. Do not paste extracted art or decompiled source into the
> engine.

---

## 0. The one-paragraph version

A player is never stored as a finished sprite. On disk he is **six greyscale/mask
layers × 101 animation frames**. At match start the engine tints the layers by
the team's kit and each player's face and flattens them into **one texture set
per face** (≤3 per team), cached by kit. Each match tick, per player, the engine:
(a) turns his movement vector into one of **8 facing directions**, (b) selects an
**animation script** from his `PlayerState`, (c) steps a per-direction **frame
list** (a tiny bytecode of show/loop/hold/delay opcodes) to get a base image
index, then (d) adds the player's **face offset** to land on the correctly-tinted
frame. Sprites are finally drawn **Y-sorted** with a **height (z)** offset so
jumps and headers lift the body while the shadow stays down.

---

## 1. On-disk asset layout

`reference/swos-port/assets/sprites/game/player/` — six subdirectories, each a
**layer**, each holding **101 frames** on an identical **144×180 RGBA** canvas:

| Subdir | PNGs | Role |
|---|---|---|
| `background/` | 101 (+101 `.txt`) | Body parts never recolored + per-frame anchor metadata |
| `skin/` | 101 | Skin mask, tinted per face |
| `hair/` | 101 | Hair mask, tinted per face |
| `shirt/` | **303** | Shirt mask — 3 geometry sets × 101 (see §3) |
| `shorts/` | 101 | Shorts mask, tinted per team |
| `socks/` | 101 | Socks mask, tinted per team |

**The 101 frames** are the complete outfield animation bank. They come from a
contiguous source range: [convertGameSprites.py:35-36](../reference/swos-port/assets/convertGameSprites.py#L35-L36)
defines `kPlayersStart = 341 … kPlayersEnd = 441` → exactly 101 sprites, covering
all eight facing directions × walk-cycle phases plus tackle/header/celebration/
booked/injured poses. They are **not** organized one-directory-per-direction; the
directional grouping lives in the animation tables (§6), which index *into* these
101 frames.

**The `.txt` files** (only in `background/`) hold each frame's **anchor / visual
center**, two integers e.g. `spr0000.txt = "48\n144"`. These are the original
SWOS center coords scaled ×12 — [convertGameSprites.py:274](../reference/swos-port/assets/convertGameSprites.py#L274)
does `int(l) * 12`. They vary per frame (24/36/48/60 horizontally) because a
player's visual center shifts as he faces different ways. At draw time this is
the `centerXF/centerYF` used to position the sprite on the player's `(x, y)` —
[gameSprites.cpp:217-222](../reference/swos-port/src/sprites/gameSprites.cpp#L217-L222).

---

## 2. Offline pipeline: how the layers were produced

[convertGameSprites.py](../reference/swos-port/assets/convertGameSprites.py) reads the
original 16-color paletted sprites and **routes palette indices to layers**
([lines 14-27, 55-63](../reference/swos-port/assets/convertGameSprites.py#L14-L63)):

| Palette index | Layer |
|---|---|
| 4, 5, 6 | skin |
| 12, 9, 13 | hair |
| 10 (shirt base), 11 (stripes) | shirt |
| 14 | shorts |
| 15 | socks |
| everything else | background |

[processSpriteLayers.py](../reference/swos-port/assets/processSpriteLayers.py) is only
an artist convenience: it merges the six PNG layers into layered TIFF/PSD and
back (`kLayers = ('background','hair','skin','shirt','shorts','socks')`,
[line 28](../reference/swos-port/assets/processSpriteLayers.py#L28)).

This split is the concrete form of `LEGACY.md`'s `[DATA]` claim: *"Kit rendering
is a body layer composited with separately tinted shirt, shorts and sock layers,
not a palette swap over a single flat sprite."*

---

## 3. Runtime compositing: kit → texture (once per match, cached)

In [colorizeSprites.cpp](../reference/swos-port/src/sprites/colorizeSprites.cpp).
Textures are cached by kit key (shirt type/colors, shorts, socks, resolution),
6 kits deep — [lines 29-71](../reference/swos-port/src/sprites/colorizeSprites.cpp#L29-L71).
For each team, once per distinct **face** present in the squad:

**a. Simple layers — SDL color-mod blit.** Skin, hair, shorts, socks are pasted
onto the background surface with a color modulation
([colorizeSprites.cpp:218-233](../reference/swos-port/src/sprites/colorizeSprites.cpp#L218-L233)):

```cpp
{ kSkinColor[i],  kPlayerSkin  },   // i = face type 0/1/2
{ kHairColor[i],  kPlayerHair  },
{ kGamePalette[team->prShortsCol], kPlayerShorts },
{ kGamePalette[team->prSocksCol],  kPlayerSocks  },
SDL_SetSurfaceColorMod(layerSurface, color.r, color.g, color.b);
pastePlayerLayer(...);   // SDL_LowerBlit each of the 101 frames
```

The **face** (`player.face`, 0–2) selects skin+hair color from
[color.h:31-36](../reference/swos-port/src/game/color.h#L31-L36):

| Face | Skin | Hair | Nickname |
|---|---|---|---|
| 0 | `{252,108,0}` pale | `{60,60,60}` dark | "White" |
| 1 | `{252,108,0}` pale | `{180,72,0}` ginger | "Ginger" |
| 2 | `{54,18,0}` dark | `{60,60,60}` dark | "Black" |

**b. Shirt — weighted two-color blend.** The shirt carries **two** colors (base +
stripes/sleeves). [copyShirtPixels](../reference/swos-port/src/sprites/colorizeSprites.cpp#L166-L200)
reads each mask pixel's **red channel as base-color weight** and **blue channel as
stripes-color weight** and blends:

```cpp
r = (baseRgb.r * rComponent + stripesRgb.r * bComponent + total/2) / total;
```

That is how two-tone shirts and vertical stripes fall out of a single mask frame.
The shirt **type** selects which 101-frame block to use
([pastePlayerShirtLayer:494-508](../reference/swos-port/src/sprites/colorizeSprites.cpp#L494-L508),
enum in [swos.h:429-433](../reference/swos-port/src/swos/swos.h#L429-L433)):

| `ShirtType` | Shirt block (offset into 303) | Note |
|---|---|---|
| `kShirtOrdinary` (0) | 0 | plain, uses base color |
| `kShirtVerticalStripes` (2) | 0 | same frames; stripes come from the R/B mask |
| `kShirtHorizontalStripes` (3) | 101 | base/stripes colors swapped |
| `kShirtColoredSleeves` (1) | 202 | |

**c. Upload & cache.** Finished per-face surfaces become GPU textures
([convertTextures:511-529](../reference/swos-port/src/sprites/colorizeSprites.cpp#L511-L529)).
Because a team's 11 players share ≤3 faces, at most 3 composited texture sets are
built per team, not 11.

---

## 4. The Sprite: fields that drive animation

[Sprite.h:44-137](../reference/swos-port/src/sprites/Sprite.h#L44-L137). The
animation-relevant members:

| Field | Meaning |
|---|---|
| `frameOffset` | **Face offset**, added to every base frame index (§5). `face × 101`. |
| `animTablePtr` | Current animation script (`PlayerAnimationTable`, §6). |
| `state` | `PlayerState` — normal, tackling, header, injured… (§6). Drives table choice. |
| `direction` | **0–7** facing octant (§5). |
| `frameIndicesTable` | Pointer to the frame list for `[type][team][direction]` in the current table. |
| `frameIndex` | Cursor into that frame list. |
| `frameDelay` / `cycleFramesTimer` | Ticks between frame advances; countdown. |
| `frameSwitchCounter` | Counts real (image-producing) frame advances. |
| `imageIndex` | **Result**: the absolute sprite index actually drawn (`<0` = none). |
| `x, y, z` | Fixed-point pitch position; `z` is height (jumps/headers). |
| `deltaX, deltaY`, `destX, destY`, `speed` | Movement; feed the direction calc. |

Init defaults: `frameDelay = 5`, `frameIndex = -1`, `state = kUnknown`
([Sprite.h:92-108](../reference/swos-port/src/sprites/Sprite.h#L92-L108)).

---

## 5. Direction: movement vector → one of 8 octants

[updateSpriteDirectionAndDeltas](../reference/swos-port/src/sprites/updateSprite.cpp#L101-L112)
computes a fine angle `fullDirection` in **0–255** (a full circle; 0 = up,
increasing clockwise) from the movement deltas via
[calculateDeltaXAndY](../reference/swos-port/src/sprites/updateSprite.cpp#L231-L336),
which uses an integer arctangent table (`kAngleTangent`) and a fixed-point
sine/cosine table. It then quantizes to the **8 sprite directions**:

```cpp
sprite.direction = ((fullDirection + 16) & 0xff) >> 5;   // +16 rounds, >>5 = /32
```

`+16` rounds to nearest octant, `>>5` divides the 256-step circle into 8 sectors:

| `direction` | Facing |
|---|---|
| 0 | up |
| 1 | up-right |
| 2 | right |
| 3 | down-right |
| 4 | down |
| 5 | down-left |
| 6 | left |
| 7 | up-left |

So the visible sprite has **8 directions**; the finer `fullDirection` is retained
for physics (ball launch angle, aftertouch), not for choosing the frame.

---

## 6. Animation tables: state → per-direction frame lists

### Structure

[Sprite.h:9-14](../reference/swos-port/src/sprites/Sprite.h#L9-L14):

```cpp
struct PlayerAnimationTable {
    uint16_t numCycles;
    // indexed as: indicesTable[player/goalkeeper][team1/2][direction]
    SwosDataPointer<ImageIndicesTable> indicesTable[2][2][8];
};
```

Each animation is **2 × 2 × 8 = 32 frame lists**: one per
(outfield-vs-goalkeeper) × (team 1 vs team 2) × (8 directions). The **team**
dimension exists because team 1 and team 2 use different sprite-index ranges
(§8) so the base frames differ.

### The scripts (one `PlayerState` → one table)

`PlayerState` ([Sprite.h:16-34](../reference/swos-port/src/sprites/Sprite.h#L16-L34))
selects the table. The tables found in the reference tree:

| State / event | Animation table |
|---|---|
| standing (idle) | `playerNormalStandingAnimTable` |
| running | `playerRunningAnimTable` |
| sliding tackle | `plTacklingAnimTable` |
| being tackled / knocked down | `playerTackledAnimTable` |
| static header attempt / hit | `staticHeaderAttemptAnimTable`, `staticHeaderHitAnimTable` |
| jumping header attempt / hit | `jumpHeaderAttemptAnimTable`, `jumpHeaderHitAnimTable` |
| throw-in | `aboutToThrowInAnimTable`, `throwInKickAnimTable`, `throwInPassAnimTable` |
| goal reaction | `playerWinningReactionAnimTable`, `playerLosingReactionAnimTable` |
| booked | `plGettingYellowCardAnimTable`, `plGettingRedCardAnimTable` |
| injured | `plInjuredAnimTable` |
| goalkeeper dives | `left/rightGoalieJumpingHigh/LowAnimTable`, `goalieCatchingBallAnimTable` |

The idle fallback is wired in
[updateAnimationTableAndDestinationReached](../reference/swos-port/src/sprites/updateSprite.cpp#L215-L229):
when a player is `kNormal` and `stationary()`, the engine switches him to
`playerNormalStandingAnimTable`. The reverse transitions (into running, tackling,
headers, …) are driven from the gameplay code in
[player.cpp](../reference/swos-port/src/game/player.cpp) /
[updatePlayers.cpp](../reference/swos-port/src/game/updatePlayers/updatePlayers.cpp),
e.g. `A0 = jumpHeaderHitAnimTable; setPlayerAnimationTableAndPictureIndex();`
([player.cpp:3231-3232](../reference/swos-port/src/game/player.cpp#L3231-L3232)).

---

## 7. The frame stepper: a tiny bytecode

Once state + direction have chosen a `frameIndicesTable`, the per-frame advance is
[updateSpriteAnimation](../reference/swos-port/src/sprites/updateSprite.cpp#L114-L143).
It fires only when the sprite is on-screen and `--cycleFramesTimer` hits 0, then
advances `frameIndex` and interprets the value there. **A frame list is a small
interpreted sequence: non-negative = show, negative = opcode.**

```cpp
frame = frameIndicesTable[frameIndex];
if (frame >= 0)              setImage(frame);       // show this base frame; frameSwitchCounter++
else if (frame == -999)      frameIndex = 0;        // kLastFrameLoopMarker: restart sequence
else if (frame == -101)      { frameIndex--; break; } // kLastFrameHoldMarker: freeze on last frame
else if (frame <= -100)      frameIndex += (frame + 100); // relative jump back (e.g. -102 -> -2)
else /* -1..-99 */           { frameDelay = -frame; cycleFramesTimer = -frame; frameIndex++; } // set speed
```

Markers ([updateSprite.cpp:5-7](../reference/swos-port/src/sprites/updateSprite.cpp#L5-L7)):

| Value | Name | Effect |
|---|---|---|
| `≥ 0` | — | Show that base frame index. |
| `-1 … -99` | (delay) | Set `frameDelay` to `-frame` for subsequent frames. |
| `-100`, `≤ -102` | `kFrameLoopbackMarker` | Relative jump: `frameIndex += frame + 100`. |
| `-101` | `kLastFrameHoldMarker` | Hold on the current frame (one-shot animations). |
| `-999` | `kLastFrameLoopMarker` | Loop back to the start (walk cycles). |

The `do…while(frame < 0)` loop means opcodes are consumed until a real image is
produced, so one call can adjust speed *and* emit a frame in the same tick.

---

## 8. Face offset: base frame → correctly-tinted frame

The frame list stores indices in **team 1's "white" base range**. The final image
index adds the sprite's `frameOffset` (its face). This is
`setPlayerAnimationTableAndPictureIndex` — in the decompiled
[player.cpp:3535-3547](../reference/swos-port/src/game/player.cpp#L3535-L3547) it reads
the base index, and if `≥ 0` adds `[esi+Sprite.frameOffset]` and stores it to
`[esi+Sprite.imageIndex]`. In C terms:

```
imageIndex = frameList[frameIndex] + frameOffset          // frameOffset = face * 101
```

`frameOffset` is assigned at init from the face:
[getPlayerSpriteOffsetFromFace](../reference/swos-port/src/sprites/gameSprites.cpp#L243-L250)
returns `face × (kTeam1GingerPlayerSpriteStart − kTeam1WhitePlayerSpriteStart)`
= `face × 101`.

Sprite-index ranges ([sprites.h:43-56](../reference/swos-port/src/sprites/sprites.h#L43-L56)):

| Range | Start | Meaning |
|---|---|---|
| `kTeam1WhitePlayerSpriteStart` | 341 | team 1, face 0 |
| `kTeam1GingerPlayerSpriteStart` | 442 | team 1, face 1 (+101) |
| `kTeam1BlackPlayerSpriteStart` | 542 | team 1, face 2 (+202) |
| `kTeam2WhitePlayerSpriteStart` | 644 | team 2 base (+303 from team 1) |
| `kTeam1MainGoalkeeperSpriteStart` | 947 | goalkeepers follow |

So: team stride = 303 (3 faces × 101), face stride = 101. The animation table's
`team` dimension picks team 1 vs team 2 base; `frameOffset` picks the face within
a team.

---

## 9. Per-tick flow

[movePlayers](../reference/swos-port/src/sprites/updateSprite.cpp#L145-L155) runs
every player each tick:

```cpp
for (each of the 22 player sprites) {
    SetNextPlayerFrame();                          // step §7 stepper, apply §8 offset -> imageIndex
    moveSprite(sprite);                            // integrate x/y toward dest (§movement)
    updateAnimationTableAndDestinationReached(...); // fall back to standing when idle (§6)
}
```

Direction (§5) is refreshed when the movement vector changes; state transitions
(§6) come from gameplay events. Net result per tick per player: an `imageIndex`
into the pre-baked, kit-tinted texture atlas.

---

## 10. Rendering: depth, camera, height, zoom

[drawSprites](../reference/swos-port/src/sprites/gameSprites.cpp#L188-L229):

1. **Y-sort** all visible sprites so higher-up-the-pitch entities draw behind
   ([sortDisplaySprites:339-344](../reference/swos-port/src/sprites/gameSprites.cpp#L339-L344)) —
   the depth rule from `LEGACY.md` §13.
2. Screen pos = `sprite.(x,y) − camera − (0, z)`: subtracting **z** lifts the body
   for jumps/headers while its shadow stays at ground Y
   ([gameSprites.cpp:210-214](../reference/swos-port/src/sprites/gameSprites.cpp#L210-L214)).
3. **Zoom** applies to players/goals/referee
   ([shouldZoomSprite:346-350](../reference/swos-port/src/sprites/gameSprites.cpp#L346-L350)).
4. Anchor is the per-frame center from the `.txt` metadata (§1).

The 22 player sprites are slots 4…25 of `kAllSprites`
([gameSprites.cpp:22-58](../reference/swos-port/src/sprites/gameSprites.cpp#L22-L58));
frame offsets are seeded in
[initializePlayerSpriteFrameIndices](../reference/swos-port/src/sprites/gameSprites.cpp#L113-L135).

---

## 11. Constants quick reference

| Constant | Value | Where |
|---|---|---|
| Frames per player animation bank | 101 | [convertGameSprites.py:35-36](../reference/swos-port/assets/convertGameSprites.py#L35-L36) |
| Layers per player | 6 | [processSpriteLayers.py:28](../reference/swos-port/assets/processSpriteLayers.py#L28) |
| Shirt geometry sets | 3 (303 frames) | [colorizeSprites.cpp:494-508](../reference/swos-port/src/sprites/colorizeSprites.cpp#L494-L508) |
| Faces | 3 | [color.h:31-36](../reference/swos-port/src/game/color.h#L31-L36) |
| Sprite directions | 8 | [updateSprite.cpp:111](../reference/swos-port/src/sprites/updateSprite.cpp#L111) |
| Fine directions (`fullDirection`) | 256 | [updateSprite.cpp:101-112](../reference/swos-port/src/sprites/updateSprite.cpp#L101-L112) |
| Face stride (`frameOffset` step) | 101 | [gameSprites.cpp:245](../reference/swos-port/src/sprites/gameSprites.cpp#L245) |
| Team stride | 303 | [sprites.h:43-47](../reference/swos-port/src/sprites/sprites.h#L43-L47) |
| Default `frameDelay` | 5 ticks | [Sprite.h:99](../reference/swos-port/src/sprites/Sprite.h#L99) |
| Anchor metadata scale | ×12 | [convertGameSprites.py:274](../reference/swos-port/assets/convertGameSprites.py#L274) |
| Kit texture cache depth | 6 | [colorizeSprites.cpp:70](../reference/swos-port/src/sprites/colorizeSprites.cpp#L70) |

---

## 12. Notes for the aftertouch reimplementation

- **Bake kits at match start, not per frame.** Compositing is per-kit and cached;
  the hot path only indexes an atlas. Mirror this: colorize on team load, draw by
  index.
- **Keep the layer split from day one.** Shirt (base + stripes via a two-channel
  mask), shorts, socks, skin, hair, background. It is what makes ~1500 club kits
  and 3 faces cheap.
- **Separate the fine direction from the sprite direction.** Physics and
  aftertouch want the 256-step `fullDirection`; only the *display* quantizes to 8.
  `LEGACY.md` §3 stresses aftertouch exists *because* input is 8-way — don't lose
  the fine angle at the movement layer.
- **The frame list is data, not code.** Reproduce the show/loop/hold/delay opcode
  set so animations stay editable. One-shots end in `kLastFrameHoldMarker (-101)`,
  loops in `kLastFrameLoopMarker (-999)`.
- **Y-sort + z-height is the whole depth model.** No z-buffer. Tie-breaking in the
  Y-sort is an open `[UNKNOWN]` in `LEGACY.md` §13 — decide it deterministically.
- **`frameSwitchCounter` gates gameplay**, not just visuals (e.g. header-contact
  timing keys off it in [player.cpp](../reference/swos-port/src/game/player.cpp#L3213-L3232));
  keep animation ticks inside the deterministic simulation, not the render layer.
