# STATE.md

The data underneath everything else: the three structures the match engine reads
and writes every frame, their byte offsets, the enums that drive the state
machines, and the coordinate and fixed-point conventions.

This document is offsets and layout only. What the fields *mean* dynamically lives
in the subsystem documents; each field below links onward where there is more to
say. The DOS-port equivalent is [../STATE.md](../STATE.md), and where the two
disagree this is a third vote.

> **Provenance.** The struct definitions are IDA `struc` blocks at asm:1–305, which
> are a human's reconstruction, not ground truth from a symbol table. Field *sizes*
> and *order* are trustworthy — they are constrained by the instruction widths at
> every access site. Field *names* are interpretation, and two of them are wrong in
> ways that matter (§5). Every offset below has been checked against at least one
> real access site, cited inline.

---

## 0. One-paragraph version

Everything on the pitch is a `Sprite` — the ball, the shadow, all twenty-two
players — a 110-byte record holding position, velocity, a destination, an
animation cursor and a state byte. Position and velocity are **16.16 fixed point**
stored as longwords; a "speed" is a separate 16-bit scalar in units of 1/512 pixel
per frame, converted to a velocity vector through the trig table. Each side owns a
`TeamGeneralInfo`, a 144-byte block holding the input state, who is controlled,
who has the ball, the proximity flags recomputed each frame, and the timers that
gate passing, switching and aftertouch. Per-player attributes live in a separate
80-byte `PlayerGame` record whose last nine bytes are the seven skills plus the
goalkeeper rating. Movement is **destination-driven throughout**: nothing sets a
velocity directly; code sets `destX`/`destY` and a `speed`, and a shared routine
derives the heading and the per-axis increments.

---

## 1. Coordinates, units and fixed point

### The pitch

| Quantity | Value | Evidence |
|---|---|---|
| Playable X range | 81 … 591 (width 510) | keeper interpolation divides by 510 from base 81, asm:36065 |
| Playable Y range | 129 … 770 (height 641) | keeper interpolation divides by 641 from base 129, asm:36085 |
| Pitch centre X | 336 | AI goal-vector origin, asm:45336 |
| Pitch centre Y | 449 | kick-off placement, asm:41528 |
| Top goal line Y | 129 | AI target for the right team, asm:45340 |
| Bottom goal line Y | 769 | AI target for the left team, asm:45336 |
| Goal mouth X | 302 … 366 (inner), 296 … 372 (posts) | goal-frame test, asm:21830 |
| Crossbar height Z | 15 … 19 | goal-frame test, asm:21833 |
| Dead-ball barrier X | 53 … 618 | asm:21786 |
| Dead-ball barrier Y | 100 … 799 | asm:21800 |
| Player movement clamp | X 81 … 590, Y 129 … 769 | asm:36030 |

Y increases *downward*. The left team defends the **top** goal (low Y) and attacks
the bottom; the right team is the mirror. "Left" and "right" in `leftTeamData` /
`rightTeamData` refer to the menu, not to a side of the pitch.

### Fixed point

Position and velocity are longwords in 16.16: the high word is the integer pixel,
the low word the fraction. Integration is a plain `add.l` of the delta longword
into the position longword.

| Quantity | Format | Example |
|---|---|---|
| `x`, `y`, `z` | 16.16, integer part is the pixel | ball at pitch centre → $0150_0000 |
| `deltaX/Y/Z` | 16.16 pixels per frame | `gravityConstant` 4608 = 0.0703 px/frame² |
| `speed` | plain int16, unit ≈ 1/512 px/frame | see below |

**Where 1/512 comes from.** `CalculateDeltaXAndY` (asm:20661) takes a `speed` in
`d5`, looks up a Q15 sine, arithmetic-shifts it right by 8 to Q7 (range ±127), then
multiplies: `delta = speed × sin_q7`. The product is used directly as a 16.16
longword, so the pixel value is `speed × 127 / 65536 ≈ speed / 516`. Rounding that
to 512 gives a clean mental model: **a `speed` of 512 is one pixel per frame**.

At 50 Hz this makes the conversions:

| Symbol | Raw | px/frame | px/s |
|---|---|---|---|
| `ballKickingSpeed` 2208 | 2208 | 4.31 | 216 |
| `playerSpeedsGameInProgress` low (Speed 0) | 928 | 1.81 | 91 |
| `playerSpeedsGameInProgress` high (Speed 7) | 1250 | 2.44 | 122 |
| `jumpHeaderSpeed` 2048 | 2048 | 4.0 | 200 |
| `ballGroundConstant` 16 (per frame) | 16 | 0.031 | 1.56 px/s² |

### Angles

Two representations, both integers:

- **`fullDirection`** — 0 … 255, a full turn in 256 steps. This is what the trig
  routine returns and what curl and aiming work in.
- **`direction`** — 0 … 7, an octant, derived as `((fullDirection + 16) & 255) >> 5`
  (asm:21668). The `+16` centres each octant on its cardinal.

The octant mapping is fixed by `defaultPlayerDestinations` (asm:36496), which is
the ±1000 offset added to a position to make a destination:

| `direction` | (dx, dy) | Compass |
|---|---|---|
| 0 | (0, −1000) | up |
| 1 | (+1000, −1000) | up-right |
| 2 | (+1000, 0) | right |
| 3 | (+1000, +1000) | down-right |
| 4 | (0, +1000) | down |
| 5 | (−1000, +1000) | down-left |
| 6 | (−1000, 0) | left |
| 7 | (−1000, −1000) | up-left |

The joystick reader returns the same encoding, documented inline at asm:37307.

---

## 2. `Sprite` — 110 bytes ($6E)

Declared at asm:99. Offsets verified against access sites; the ones IDA names
wrongly are flagged and explained in §5.

| Offset | IDA name | Size | Meaning |
|---|---|---|---|
| $00 | `field_0` | w | Sprite kind / draw class |
| $02 | `shirtNumber` | w | 1 = goalkeeper. Indexes into `PlayerGame`. |
| $0C | `playerState` | b | `PL_*` enum, §3 |
| $0D | `playerDownTimer` | b | Frames remaining in the current non-normal state |
| $12 | `frameIndicesTable` | l | Pointer to the active animation table |
| $16 | `framePictureIndex` | w | Cursor into that table |
| $18 | `field_18` | w | Frame-cycle reload; set from speed, asm:35528 |
| $1A | `cycleFramesTimer` | w | Frame-cycle countdown |
| **$1E** | (`xFraction`) | **l** | **X, 16.16** — see §5 |
| **$22** | (`yFraction`) | **l** | **Y, 16.16** |
| **$26** | (`zFraction`) | **l** | **Z, 16.16** — height |
| $2A | `direction` | w | Octant 0–7 |
| $2C | `speed` | w | Scalar, 1/512 px/frame |
| $2E | `deltaX` | l | 16.16 per-frame X increment |
| $32 | `deltaY` | l | 16.16 |
| $36 | `deltaZ` | l | 16.16 |
| $3A | `destX` | w | Aim point X, whole pixels |
| $3C | `destY` | w | Aim point Y |
| $46 | `pictureIndex` | w | Resolved sprite; −1 hides |
| $4A | `ballDistance` | l | Squared distance to the ball. Recomputed for all eleven players by `sub_111B98` (asm:36990) during the input phase, so it lags the ball by one frame. |
| $52 | `fullDirection` | w | Fine heading 0–255. **On the ball** this is its heading. **On a player** it is the angle *from the ball to him*, rewritten every frame at asm:45206 — which is why the idle-facing code adds 128 to make a player look at the ball. The pass cone and the receiver-deviation test both depend on this reading; see [PASSING.md](PASSING.md) §1. |
| $60 | `isMoving` / `field_60` | w | Deflection flag on the ball; 0/1/2 |
| $62 | `field_62` | w | Header-in-progress marker |
| $64 | `field_64` | w | Restart freeze flag |
| $66 | `field_66` | w | Card counter for this player |
| $68 | `injuryLevel` | w | |
| $6A | `field_6A` | w | Tackle intent: −1 = deflecting tackle |
| $6C | `field_6C` | w | Sent-off / unavailable marker |

`destX`/`destY` are 16-bit whole pixels, deliberately allowed to run far outside
the pitch — the ±1000 direction deltas above mean "keep going that way", not "go
to that point".

`ballDistance` at $4A is a squared distance and is compared as a longword against
constants like 512, 2048 and 5000 (asm:42494, asm:42505, asm:42546). Do not read
those as pixels: √512 ≈ 23 px, √2048 ≈ 45 px, √5000 ≈ 71 px.

---

## 3. Enums

### `PL_*` — `Sprite.playerState` ($0C), asm:269

| Value | Name | Meaning |
|---|---|---|
| 0 | `PL_NORMAL` | Running or standing; the only state that accepts input freely |
| 1 | `PL_TACKLING` | Sliding |
| 3 | `PL_TACKLED` | Knocked over by a tackle |
| 4 | `PL_GOALIE_CATCHING_BALL` | |
| 5 | `PL_THROW_IN` | |
| 6 | `PL_GOALIE_DIVING_HIGH` | |
| 7 | `PL_GOALIE_DIVING_LOW` | |
| 8 | `PL_NORMAL2` | Settling after a static header; decelerating |
| 9 | `PL_HEADING` | Airborne in a jumping header |
| $A | `PL_DOWN` | On the ground |
| $B | `PL_GOALIE_CLAIMED` | Keeper holding the ball |
| $C | `PL_BOOKED` | Card animation |
| $D | `PL_INJURED` | |
| $E | `PL_SAD` | Celebration (conceded) |
| $F | `PL_HAPPY` | Celebration (scored) |

Value 2 is unused. States $C–$F are presentation-only and freeze movement.

### `gameState` and `gameStatePl`

Two separate globals. `gameStatePl` is the coarse gate:

| Value | Meaning |
|---|---|
| $64 (100) | Play is live |
| $65 (101) | Play is stopped, restart pending |
| $66 (102) | Restart positions being taken up |

`gameState` names *which* restart. Only six values have symbolic names in the
listing (asm:288, the `ST_*` block); the rest are recovered from `GameSetup`
(asm:41156) and `RunStoppageEventsAndSetAnimationTables` (asm:37250):

| Value | Meaning | Placement evidence |
|---|---|---|
| 0 | Goal scored → kick-off | asm:41525 |
| 1 | Goal kick, one half of the goal area | asm:41404 |
| 2 | Goal kick, the other half | asm:41419 |
| 3 | Keeper holds the ball | asm:37599 |
| 4 | `ST_CORNER_LEFT` | asm:41463 |
| 5 | `ST_CORNER_RIGHT` | asm:41449 |
| 6 … $C | Free kick, one per direction octant | asm:45426 groups them |
| $D | Foul awarded | asm:45419 |
| $E | `ST_PENALTY` | asm:290 |
| $F … $14 | Throw-in, six variants by side and third | asm:41520–41548 |
| $15 … $1E | Period transitions and result screens | asm:37298 |
| $1F | `ST_PENALTIES` (shootout) | asm:293 |

Values 1–2 and $F–$14 use a **relative** encoding: they describe the restart from the
taking team's point of view, so the same value maps to mirrored absolute geometry for
the two sides. [SETPIECES.md](SETPIECES.md) §2 gives the full cross-table. The six
throw-in variants pair a direction (forward/back, left/right — hence the names
`ST_THROW_IN_FORWARD_RIGHT` and `ST_THROW_IN_BACK_LEFT`) with a third of the pitch,
selected by the Y bands 342 and 556 (asm:41474).

---

## 4. `TeamGeneralInfo` — 144 bytes ($90)

Declared at asm:4. Two instances, `leftTeamData` and `rightTeamData` (asm:36275).
Register `a6` points at one of them throughout the per-player pipeline, and `(a6)`
— offset $00 — points at the *other*, which is how "the opponent" is reached.

| Offset | IDA name | Size | Meaning |
|---|---|---|---|
| $00 | `opponentsTeam` | l | The other `TeamGeneralInfo` |
| $04 | `playerNumber` | w | **0 = this side is CPU-controlled**, non-zero = human. Confirmed at asm:43326, where a zero routes the "controlled" player through `DoAI`. Gates the pass-accuracy roll, pass aftertouch, injury penalties and the restart turn mask. |
| $06 | `plCoachNum` | w | Which joystick coaches this side |
| $0A | `inGameTeamPtr` | l | Team record (stats live at +2, +4) |
| $14 | `spritesTable` | l | Array of 11 `Sprite*` |
| $18 | `teamPlOfs24Table` | l | `sField_18` tuning block for this side |
| $1C | `field_1C` | w | Active tactic index |
| $1E | `updatePlayerIndex` | w | Cursor in the per-player sweep |
| $20 | `controlledPlayerSprite` | l | The player under control |
| $24 | `passToPlayerPtr` | l | Intended pass receiver. While set, **this player is excluded from control selection** — the mechanism behind "you cannot steer the receiver". See [PASSING.md](PASSING.md) §6. |
| $28 | `playerHasBall` | w | Non-zero when this side is in possession |
| $2A | `allowedDirections` | w | Turn-restriction mask |
| $2C | `currentDirection` | w | Joystick octant this frame; **−1 = neutral** |
| $30 | `quickFire` | b | Fire tapped |
| $31 | `normalFire` | b | Fire held past the tap threshold |
| $32 | `joyIsFiring` | b | Fire down now |
| $33 | `joyTriggered` | b | Fire edge this frame |
| $34 | `header` | w | Header requested |
| $38 | `field_38` | w | **Direction the ball was kicked in** — the axis curl is measured against |
| $3A | `shooting` | w | Shot in progress |
| $3D | `plVeryCloseToBall` | b | Proximity band, see [CONTEST.md](CONTEST.md) §1 |
| $3E | `plCloseToBall` | b | " |
| $3F | `plNotFarFromBall` | b | " |
| $40 | `ballLessEqual4` | b | Height band: ball z ≤ 4 |
| $41 | `ball4To8` | b | " |
| $42 | `ball8To12` | b | " |
| $43 | `ball12To17` | b | " |
| $44 | `ballAbove17` | b | " — too high to play |
| $48 | `lastHeadingPlayer` | l | |
| $4C | `goalkeeperSavedCommentTimer` | w | Save/goal latch; +5 = saved, −5 = conceded |
| $50 | `goalkeeperDivingRight` | w | |
| $52 | `goalkeeperDivingLeft` | w | |
| $56 | `goaliePlayingOrOut` | w | |
| $58 | `passingBall` | w | "The receiver is now actively chasing." Latched once he is closest to the ball. |
| $5A | `passingToPlayer` | w | "A pass is live." Cleared by every teardown path in [PASSING.md](PASSING.md) §9. |
| $5C | `playerSwitchTimer` | w | Gates control switching **and** ticks aftertouch. Set to 25 on reception. |
| $5E | `ballInPlay` | w | |
| $60 | `ballOutOfPlay` | w | Gates control selection entirely (asm:37050); set to 1 on every kick and pass |
| $62 | — | w | **Receiver's adjusted destination X** ([PASSING.md](PASSING.md) §7) |
| $64 | — | w | **Receiver's adjusted destination Y** |
| $66 | `passKickTimer` | w | 25-frame countdown after a kick or pass |
| $68 | `passingKickingPlayer` | l | The player who just kicked; **excluded from control selection** while set |
| $6C | `unkBallTimer` | w | Dribble touch counter, see [CONTEST.md](CONTEST.md) §2 |
| $6E | `ballCanBeControlled` | w | |
| $76 | `spinTimer` | w | **Aftertouch frame counter, 0…9; −1 = inactive** |
| $78 | `leftSpin` | w | Curl side latched left |
| $7A | `rightSpin` | w | Curl side latched right |
| $7C | `longPass` | l | Used as two independent word flags in the pass path |
| $80 | `passInProgress` | w | Selects pass curl tables over kick curl tables |
| $82 | `AITimer` | w | |
| $8A | `wonBallTimer` | w | Frames of possession lock after winning the ball |
| $8C | `goalkeeperPlaying` | w | |
| $8E | `resetControls` | w | Suppresses all input |

`GetPlayerPointerFromShirtNumber` (asm:35640) is the bridge from a `Sprite` to its
attributes: it reads `shirtNumber` and returns a `PlayerGame*` in `a4`.

### `sField_18` — 60 bytes ($3C)

The per-side tuning block reached through `teamPlOfs24Table`. Selected by
`UpdateTeamOfs24Table` (asm:35548), which uses a different block for goalkeepers
(asm:35566). Fields used by documented code:

| Offset | IDA name | Used for |
|---|---|---|
| $06 | `speed_unk` | Chase speed when closing on a loose ball, asm:42527 |
| $0A | `index_dseg_11061B` | 16 entries; indexes the keeper reaction table, asm:38637 |

---

## 5. Label skew — two places IDA's names mislead

These are the traps. Both were found by checking every offset against a real
access site.

### Position fields are named one word early

IDA declares the position block as `x` ($1C), `xFraction` ($1E), `y` ($20),
`yFraction` ($22), `z` ($24), `zFraction` ($26) — i.e. it believed each coordinate
is an integer word followed by a fraction word.

The code disagrees. `UpdateBall` integrates with `add.l d1, Sprite.xFraction(a0)`
(asm:21707) — a *longword* add into $1E. For that to be a 16.16 accumulate, $1E
must be the high word. And the pitch-boundary tests compare `$1E(a0)` against 53
and 618 (asm:21786), which are pixel values, not fractions.

**The true layout is $1E = X.16, $22 = Y.16, $26 = Z.16.** IDA's `xFraction`,
`yFraction` and `zFraction` labels are in fact the *whole 16.16 accessor* and its
integer part; its `x`, `y`, `z` labels sit one word early and are not the
coordinate. Read `Sprite.xFraction` in the listing as "X".

The corollary is that `tst.w Sprite.zFraction(a0)` (asm:21692), which decides
between ground and air friction, tests the **integer height** — friction switches
the moment the ball leaves the turf, not on a fractional threshold.

Delta fields are *not* skewed: `deltaX` at $2E genuinely is the high word of the
16.16 delta, with IDA's `field_30` as its fraction. Same for `deltaY`/`field_34`
and `deltaZ`/`controlledPlDirection`.

### `PlayerGame.shooting` is Velocity

`PlayerGame` $46 is labelled `shooting` and is read with `move.b` to index
`ballSpeedKicking` for long shots (asm:35107). In SWOS's own vocabulary this
attribute is **Velocity**, the shot-power rating — not the separate Finishing
rating at $4B, which is what the label suggests. [PLAYERS.md](PLAYERS.md) §1 works
the whole skill block out; the short version is that $45–$4B is SWOS's canonical
**P V H T C S F** order and the labels drifted.

---

## 6. `PlayerGame` — 80 bytes ($50)

Declared at asm:167. One per squad member; reached by shirt number. Only the tail
is interpreted; $00–$2D is roster data (name, nationality, value) that the match
engine does not read.

| Offset | IDA name | Size | Meaning |
|---|---|---|---|
| $2E | `position` | b | Role |
| $44 | `field_44` | b | Unread by the match engine |
| $45 | `passing` | b | **Passing** (0–7) |
| $46 | `shooting` | b | **Velocity** — shot power (0–7), see §5 |
| $47 | — | b | **Heading** (0–7) |
| $48 | — | b | **Tackling** (0–7) |
| $49 | — | b | **Ball Control** (0–7) |
| $4A | `speed` | b | **Speed** (0–7) |
| $4B | `finishing` | b | **Finishing** (0–7) |
| $4C | `goalieSkill` | b | Goalkeeper rating (0–7), derived not stored |
| $4D | `injuriesBitfield` | b | Top three bits index `injuriesSpeedPenalty` |
| $4E | `field_4E` | w | Half-played marker: 0 → 1 → 2 |

All eight ratings are **0–7**, confirmed twice: every table indexed by one of them
has exactly eight entries, and `AdjustPlayerSkills` clamps to 7 explicitly
(asm:102113). The 1–8 the interface shows is a display offset.

---

## 7. What this resolves, and what still needs measurement

Confirmed as structure:

- ✓ 16.16 fixed point for position and velocity; separate int16 scalar speed.
- ✓ Speed unit is 1/512 px/frame, derived from the Q15→Q7 shift in the trig routine.
- ✓ Octant encoding and its exact mapping to screen directions.
- ✓ Playable pitch 510 × 641 at origin (81, 129).
- ✓ Skill scale is 0–7 for all eight ratings.
- ✓ The skill block order, settled by three independent access sites.
- ✓ `spinTimer` is a 0…9 counter with −1 as the inactive sentinel.

Open (measurement targets, [../LEGACY.md](../LEGACY.md) §15):

- `Sprite` $00, $04–$0B, $3E–$44, $48, $4C–$51, $54–$5E are unread by any routine
  documented here. Some are certainly presentation state; a few may matter.
- `sField_18` is 60 bytes and only two fields are understood. It is per-side and
  differs for keepers, so it is likely a difficulty or tactic modifier block —
  worth a dedicated pass.
- `TeamGeneralInfo` $70–$75, $86–$89 are unaccounted for. ($62/$64 were resolved as
  the pass receiver's adjusted destination; $74 is cleared alongside the pass state
  on reception but its meaning is still unknown.)
- `Sprite` +$54, the "active" flag `sub_111B98` requires for selection eligibility,
  has no located writer.
- Whether `field_1C` (tactic index) is the same enumeration as the tactics file
  order has not been checked against the data.

---

## 8. Guidance for the reimplementation

- **Keep the destination idiom.** It is not an artefact; the whole engine — kicks,
  curl, rebounds, AI positioning — expresses "change of motion" as "move the aim
  point". Reproducing SWOS's feel with a velocity-first model means fighting it at
  every turn. Our `at_core` should carry `dest` on every moving body.
- **Keep integer speed separate from the velocity vector.** Friction, bonuses and
  penalties all operate on the scalar; the vector is derived. This is why a shot
  bonus of −384 on a base of 2208 is meaningful and composable.
- **Use 16.16 for position, and integrate by addition.** Do not be tempted into
  floats. The bounce, the barrier mirror and the landing predictor all depend on
  exact re-derivation from stored fractions.
- **Model the skill scale as 0–7 internally** and offset only at the presentation
  boundary. Every table in the engine is eight entries wide; anything else means
  reindexing every one of them.
- **Do not port the label names.** Name our fields after what the code does with
  them — `velocity` not `shooting`, `x` not `xFraction`. The skew documented in §5
  exists because someone did the opposite.
