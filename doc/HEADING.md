# HEADING.md

Aerial play: the two kinds of header, when each triggers, how the player gets into
the air, and — the part that matters most — how the direction you are holding at the
moment of contact selects between a driven header and a lob. Traced through the
reference DOS port in [../reference/swos-port/](../reference/swos-port/) and its
annotated disassembly ([swos/swos.asm](../reference/swos-port/swos/swos.asm)).

This document collects material previously scattered across
[MOVEMENT.md](MOVEMENT.md) §9 (jump arc and speeds), [AI.md](AI.md) §5.7 (the CPU's
trigger predicate) and [PLAYER_SPRITES.md](PLAYER_SPRITES.md) §6 (animation
tables). The other contest, the tackle, is [TACKLING.md](TACKLING.md); what happens
to the ball after it leaves the head is [BALL.md](BALL.md).

> **Provenance.** The heading routines are read from the annotated disassembly,
> which for this subsystem is considerably clearer than the decompiled C++ — the
> original labels (`@@lob_header`, `@@right_held`, `DoFlyingHeader`) survive and
> name the design directly. **All constants in this document are real literal
> values** from the data segment listing
> ([swos.asm:245768-245903](../reference/swos-port/swos/swos.asm#L245768)), not
> addresses. Read to understand the design; write our own code.

> **Second oracle — and a correction.** [amiga/KICKING.md](amiga/KICKING.md) §5
> traces the same routines through the Amiga original. It confirms the structure of
> §4 almost exactly, and it answers three of §8's open questions. It also shows that
> **§6's thirteen-entry Heading table is a mis-read of an eight-entry one** — which
> makes §6's headline conclusions (that Heading is the strongest attribute effect in
> the game, and that attributes range 0–12) both wrong. §10 has the details; read it
> before using anything from §6.

---

## 0. One-paragraph version

SWOS has **two** headers. A **static header** (`kStaticHeader`) is a jump in place
for a ball that is already close; a **jump header** (`kJumpHeader`) is a lunge at
`kJumpHeaderSpeed = 2048` — faster than a sliding tackle — for a ball that is
further away. Which one fires is decided purely by the ball-proximity band flags
described in [CONTROL.md](CONTROL.md) §2: near bands give the static header, the
`ball8To12` / `ball12To17` / `ballAbove17` bands give the jump. On contact, the
engine computes `(facing − held direction) & 7` — **the angle between where the
player is looking and where the stick is pushed** — and switches on it: hold
straight ahead and the ball goes straight; hold one octant off and you only steer
the aim; two octants off calls `DoFlyingHeader` (low, driven, 75 % speed); three
octants off or straight back calls `DoLobHeader` (high, 93.75 % speed). Ball speed
is **125 % of the player's speed** for a jump header but a **flat 1792** for a
static one, and in both cases the player's **Heading attribute is added as a signed
bonus** from a thirteen-entry table that runs from −336 to +2569.

---

## 1. The two states

| | Static header | Jump header |
|---|---|---|
| `Sprite.state` | `kStaticHeader` (8) | `kJumpHeader` (9) |
| Launch routine | [AttemptStaticHeader](../reference/swos-port/swos/swos.asm#L112420) | [PlayerAttemptingJumpHeader](../reference/swos-port/src/game/updatePlayers/updatePlayers.cpp#L15253) |
| Contact routine | [PlayerHittingStaticHeader](../reference/swos-port/src/game/player.cpp#L1146) | [PlayerHittingJumpHeader](../reference/swos-port/src/game/player.cpp#L1396) |
| Player speed | `kStaticHeaderPlayerSpeed = 256` | `kJumpHeaderSpeed = 2048` |
| Destination | `pos + kDefaultDestinations[dir]` | `pos + kDefaultDestinations[dir]` |
| Downtime | `playerDownTimer = 20` | see [MOVEMENT.md](MOVEMENT.md) §9 |
| Animation | `staticHeaderAttemptAnimTable` | `jumpHeaderAttemptAnimTable` |

**`kJumpHeaderSpeed = 2048` is the fastest a player ever moves** — above the
sliding tackle's 1792 and well above the 1250 running maximum
([MOVEMENT.md](MOVEMENT.md) §10). The static header's 256 is nearly stationary: it
is a jump, not a lunge.

Relevant `Sprite` fields:

| Field | Offset | Meaning |
|---|---|---|
| `state` | +12 | `kStaticHeader` / `kJumpHeader` |
| `playerDownTimer` | +13 | Recovery countdown |
| `heading` | +98 | 0 while attempting, **1 once contact is made** |
| `frameSwitchCounter` | +28 | Gates the hit-animation swap (§5) |

`Sprite.heading` at +98 is `unk009` in
[Sprite.h](../reference/swos-port/src/sprites/Sprite.h) — the disassembly names it,
the C++ header does not.

---

## 2. Which header fires

[updatePlayers.cpp:5560-5615](../reference/swos-port/src/game/updatePlayers/updatePlayers.cpp#L5560-L5615).
The decision reads the per-team proximity band flags from
[CONTROL.md](CONTROL.md) §2 — **not** a distance computation:

```
if (!plVeryCloseToBall && !plNotFarFromBall) → not a header
if (ball8To12 || ball12To17 || ballAbove17)  → JUMP HEADER
else                                          → static header path
```

The logic is worth stating plainly because it is inverted from intuition: **the
jump header is for the ball that is further away.** A player close to the ball
jumps in place; a player with ground to make lunges. The `ballAbove17` band is
included, so a jump header can be launched at a ball that is a long way off.

On the jump path the engine also sets
([:5600-5612](../reference/swos-port/src/game/updatePlayers/updatePlayers.cpp#L5600-L5612)):

```
team.headerOrTackle              = 1
team.lastHeadingTacklingPlayer   = this player
team.ballCanBeControlled         = 0      // no dribble capture during the lunge
```

`ballCanBeControlled = 0` is what stops a jump header from degenerating into a
dribble touch when the player arrives.

For the **CPU's** version of this decision — `ballDistance ≤ 648` with `z` in
`[8,14]` rising or `[12,20]` falling — see [AI.md](AI.md) §5.7. The CPU evaluates a
height window that the human path does not; the human path is band flags only.

---

## 3. The jump header's ball effect

[PlayerHittingJumpHeader](../reference/swos-port/src/game/player.cpp#L1396). Base
effect first, before any directional modifier:

```
team.passInProgress = 0
ball.deltaZ         = kBallJumpHeaderDeltaZ          // 0xA000
ball.speed          = player.speed + (player.speed >> 2)   // 125 %
```

Then the direction switch (§4) may overwrite `deltaZ` and trim `speed`, then:

```
ball.destX/Y  = ball.pos + kDefaultDestinations[finalDir]
ball.speed   += kPlayerHeaderSpeedIncrease[player.heading]   // signed, §6
player.speed >>= 1
player.heading(+98) = 1
PlayKickSample(); ResetBothTeamSpinTimers()
```

**A header kills aftertouch**, exactly like a tackle
([TACKLING.md](TACKLING.md) §4) and a frame hit ([BALL.md](BALL.md) §6).

---

## 4. The direction switch — the actual mechanic

[swos.asm, PlayerHittingJumpHeader](../reference/swos-port/swos/swos.asm#L108950).
This is the heart of the system:

```
held = team.currentAllowedDirection
if (held < 0) held = player.direction        // nothing pressed

D0 = (player.direction - held) & 7           // relative angle, in octants
```

| `D0` | Stick relative to facing | Trajectory | Aim |
|---|---|---|---|
| 0 | straight ahead | *(none — base)* | `allowedDirections` |
| 1 | 1 octant left | *(none)* | `dir − 1` |
| 7 | 1 octant right | *(none)* | `dir + 1` |
| 2 | 2 octants left | **`DoFlyingHeader`** | `dir − 1` |
| 6 | 2 octants right | **`DoFlyingHeader`** | `dir + 1` |
| 3 | 3 octants left | **`DoLobHeader`** | `dir − 1` |
| 5 | 3 octants right | **`DoLobHeader`** | `dir + 1` |
| 4 | straight back | **`DoLobHeader`** | `allowedDirections` |
| — | nothing held (`held < 0`) | **`DoFlyingHeader`** | `allowedDirections` |

Read the table as a dial. **Pull back for height, push across for a driven
header, nudge one octant to steer without changing the trajectory.** Note that the
aim adjustment is always a single octant regardless of how far the stick is from
facing — holding three octants off does not aim three octants off; it selects a lob
*and* nudges the aim by one.

This is the same input vocabulary as [AFTERTOUCH.md](AFTERTOUCH.md) — back for
loft, sideways for shape — but resolved **instantly at contact** rather than
accumulated over a multi-tick window. A player who knows the aftertouch idiom
already knows how to head the ball, which is presumably the point.

### The two trajectory routines

[DoFlyingHeader / DoLobHeader](../reference/swos-port/swos/swos.asm#L109188):

| | `deltaZ` | Speed |
|---|---|---|
| `DoFlyingHeader` | `kHeaderLowJumpHeight` = `0x20000` (2.0) | `speed − (speed >> 2)` = **75 %** |
| `DoLobHeader` | `kHeaderHighJumpHeight` = `0x24000` (2.25) | `speed − (speed >> 4)` = **93.75 %** |

Two things here are counterintuitive and both are real:

- **The heights barely differ** — 2.0 versus 2.25, a 12.5 % gap — against the base
  `kBallJumpHeaderDeltaZ` of `0xA000` (0.625). Both routines *more than triple* the
  base launch height; the choice between them is a fine adjustment on top of an
  already-lofted ball.
- **The lob keeps more horizontal speed than the drive** (93.75 % vs 75 %). The
  "flying header" is the one that gets damped. Whatever the intent, the effect is
  that a lobbed header travels further, not shorter.

Both then call `SetPlayerJumpHeaderHitAnimationTable`, which swaps to
`jumpHeaderHitAnimTable` **only if `frameSwitchCounter <= 2`**
([swos.asm](../reference/swos-port/swos/swos.asm#L109240)) — the hit animation
cannot interrupt itself late in a cycle.

---

## 5. The static header

**Launch** — [AttemptStaticHeader](../reference/swos-port/swos/swos.asm#L112420):

```
player.heading(+98) = 0
player.direction    = D0
destX/Y             = pos + kDefaultDestinations[dir]
speed               = kStaticHeaderPlayerSpeed        // 256
animation           = staticHeaderAttemptAnimTable
state               = PL_STATIC_HEADING
playerDownTimer     = 20
```

**Contact** — [PlayerHittingStaticHeader](../reference/swos-port/swos/swos.asm#L112300).
The direction handling is different from the jump header, and simpler:

```
D0 = (held - facing) & 7
if (D0 == 0 || D0 == 4) → no turn
else if (D0 < 4)  turn RIGHT, up to two octants
else              turn LEFT,  up to two octants
```

The disassembly comments this itself: *"only allowed to turn player twice toward
controls (max 90 degrees)"*. A standing header can be redirected by at most a right
angle. There is **no flying/lob selection** — the static header has one trajectory.

The ball effect:

```
ball.destX/Y = ball.pos + kDefaultDestinations[dir]
ball.speed   = kStaticHeaderBallSpeed                  // 1792 — FLAT
ball.speed  += kPlayerHeaderSpeedIncrease[heading]
ball.deltaZ  = -ball.deltaZ / 2
player.heading(+98) = 1
PlayKickSample(); ResetBothTeamSpinTimers()
```

Two important differences from the jump header:

- **Ball speed is a constant 1792**, not a function of the player's speed. A
  standing header always leaves at the same pace, modified only by the Heading
  attribute. This makes it predictable and, at 1792, genuinely powerful — the same
  figure as a sliding tackle's launch speed.
- **`deltaZ = -deltaZ / 2`** — the ball's incoming vertical velocity is *reflected
  and halved*, not replaced. A steeply dropping ball is headed up more than a
  shallow one. This is the only place in the heading code where the incoming ball's
  state affects the outgoing trajectory, and it means a static header is a genuine
  redirection rather than a fresh launch.

---

## 6. The Heading attribute

> ⚠️ **This section is superseded by §10.** The Amiga original has this table with
> **eight** entries — `−336, −288, −240, −192, −144, −96, −48, 0` — and clamps every
> attribute to 7 on load. Those are exactly the first eight values below; the five
> that follow belong to whatever data item sits next in the segment. The table is a
> **pure handicap ramp with no upside**, not the game's strongest attribute effect,
> and it settles nothing about the attribute range. The rest of this section is left
> in place because the mis-read propagated to several other documents and needs to
> be traceable, but **do not implement from it**.

`kPlayerHeaderSpeedIncrease`
([swos.asm:245768](../reference/swos-port/swos/swos.asm#L245768)), added to ball
speed by **both** header types:

```
dw -336, -288, -240, -192, -144, -96, -48, 0, 513, 1027, 1541, 2055, 2569
```

| Heading | 0 | 1 | 2 | 3 | 4 | 5 | 6 | **7** | 8 | 9 | 10 | 11 | 12 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| Δspeed | −336 | −288 | −240 | −192 | −144 | −96 | −48 | **0** | +513 | +1027 | +1541 | +2055 | +2569 |

**Heading 7 is the zero point**, and the curve is sharply asymmetric. Below 7 the
penalty grows in steps of 48; at and above 8 the bonus grows in steps of ~514 —
more than ten times the slope. Against a static header's base of 1792, a Heading-12
player adds +2569, more than **doubling** the ball speed, while a Heading-0 player
loses only 336.

**~~This is the strongest attribute effect found anywhere in the reference so far.~~**
*(Void — §10. It is the weakest: a handicap ramp topping out at zero. The attribute
that really shows player quality is Ball Control — [CONTROL.md](CONTROL.md) §8.)*
Compare the tackle contest ([TACKLING.md](TACKLING.md) §8), where the entire
attribute range buys a swing from 50 % to 72 %.

**~~It also settles the attribute range question.~~** *(Void — §10.)* The table has **13 entries**,
indexed directly by the raw attribute with no clamp. So `PlayerGameHeader.heading`
reaches at least 12, which means the 0–7 assumption behind several other tables is
wrong. This bears directly on the open bounds question in
[TACKLING.md](TACKLING.md) §10 — `kPlAvgTacklingBallControlDiffChance` has only 8
entries and is indexed by a *difference*, so if attributes reach 12 the difference
can reach 12 and that table is read out of bounds. Worth checking against
[LEGACY.md](LEGACY.md) §9 before either is reimplemented.

---

## 7. Constants quick reference

| Constant | Value | Meaning |
|---|---|---|
| `kJumpHeaderSpeed` | `2048` | Jump-header lunge speed — fastest in the game |
| `kStaticHeaderPlayerSpeed` | `256` | Static-header player speed (jump in place) |
| `kStaticHeaderBallSpeed` | `1792` | Static-header ball speed, **flat** |
| `kBallJumpHeaderDeltaZ` | `0xA000` (0.625) | Base launch height, jump header |
| `kHeaderLowJumpHeight` | `0x20000` (2.0) | `DoFlyingHeader` |
| `kHeaderHighJumpHeight` | `0x24000` (2.25) | `DoLobHeader` |
| `kPlayerHeaderSpeedIncrease` | **`−336 … 0`, 8 entries** (§10; not the 13 entries §6 records) | Heading attribute handicap |
| Jump-header ball speed | `player.speed × 1.25` | |
| `DoFlyingHeader` speed | `× 0.75` | |
| `DoLobHeader` speed | `× 0.9375` | |
| Player speed after contact | `× 0.5` | Jump header |
| Static-header `deltaZ` | `−deltaZ / 2` | Reflect and halve |
| Static-header downtime | `20` ticks | |
| Static-header max turn | 2 octants (90°) | |
| Anim-swap gate | `frameSwitchCounter ≤ 2` | |

---

## 8. What this resolves, and what still needs measurement

**Confirmed as structure:**

- Two distinct headers, selected by **ball-proximity band flags**, with the jump
  header used for the *further* ball. ✓
- Jump-header lunge at 2048 is the fastest player movement in the game. ✓
- Trajectory is selected by `(facing − held) & 7` at the instant of contact: back
  or 3 octants → lob, 2 octants → drive, 1 octant → aim only. ✓
- Aim adjustment is always exactly ±1 octant. ✓
- Jump-header ball speed is 125 % of player speed; static-header ball speed is a
  flat 1792. ✓
- Static header reflects and halves the ball's incoming `deltaZ` rather than
  replacing it. ✓
- Static header can turn at most 90° toward the held direction. ✓
- ~~Heading attribute is a signed ±table on ball speed, zero at 7, strongly
  asymmetric, with 13 entries.~~ **Retracted — see §10.** It is an eight-entry
  non-positive ramp, `−336 … 0`, and the positive tail was read past the end of the
  table.
- Both headers reset both teams' spin timers, killing aftertouch. ✓

**Resolved by the Amiga oracle** (see §10):

- ~~Jump-header recovery time.~~ **50 frames**, set at launch alongside the state
  and the animation table (asm:39694). The tackling dead-store worry does not apply
  here; the Amiga writes it once and it stands.
- ~~The true attribute range.~~ **0–7**, masked and clamped explicitly on load
  ([amiga/PLAYERS.md](amiga/PLAYERS.md) §1). Every attribute-indexed table in the
  engine has exactly eight entries, which is the structural confirmation.
- ~~Why the lob retains more speed than the drive.~~ Still not *explained*, but the
  Amiga confirms both branches exist and both cut speed, so it is not a swapped
  shift introduced by the port.

**Open (measurement targets, [LEGACY.md](LEGACY.md) §15):**

- **The jump arc itself.** [MOVEMENT.md](MOVEMENT.md) §9 has `kPlayerAirConstant`
  and the down-timer halving at 37/17 ticks; how those combine into the player's
  `z` trajectory during `kJumpHeader` is not traced here.
- **Contact detection.** *When* `PlayerHittingJumpHeader` fires during a lunge —
  the call-site predicate at
  [updatePlayers.cpp:7418](../reference/swos-port/src/game/updatePlayers/updatePlayers.cpp#L7418)
  is unread. The height window at which a player can actually reach the ball is the
  single most important missing number in this document.
- **Static-header trigger conditions.** §2 covers the jump branch; the exact
  entry test for `AttemptStaticHeader`
  ([:4914-4922](../reference/swos-port/src/game/updatePlayers/updatePlayers.cpp#L4914-L4922))
  is only partially read.
- Whether the CPU can select lob vs drive, or whether its virtual joystick
  ([AI.md](AI.md) §5) only ever produces the `held < 0` / straight-ahead cases.
  The Amiga has a dedicated `AIHeader` (asm:46576) that was not traced in detail.
- `TeamGeneralInfo.ballCanBeControlled` — set to 0 here, consumed elsewhere.
- **Whether the static header takes an attribute bonus at all.** §5 adds
  `kPlayerHeaderSpeedIncrease` on both paths; the Amiga is explicit that
  `DoStaticHeader` consults **no attribute** (§10).
- **The static header's flat 1792 ball speed.** The Amiga records the static
  header's `$100` player speed and its 20-frame duration but no ball speed at all,
  so 1792 is single-sourced (§10).

---

## 9. Guidance for the reimplementation

- **Build the direction switch first.** `(facing − held) & 7` into a seven-way
  table is the whole mechanic, it is ten lines of code, and everything else is
  parameters. Get this right and headers will feel like SWOS even with wrong
  constants.
- **Keep two header types, and keep the counterintuitive trigger** — jump for the
  distant ball, static for the near one. It reads oddly in code and correctly on
  screen.
- **Static headers redirect; jump headers launch.** Preserve the `-deltaZ/2`
  reflection for the static case and the flat 1792. A single unified "header"
  routine loses the distinction that makes standing headers reliable and lunging
  ones explosive.
- **Reproduce the Heading ramp as a pure handicap** — `−336 … 0`, eight entries,
  topping out at zero (§10). It looks like a bug and is not one: headers derive
  their pace from the jump, and the table exists to penalise poor headers rather
  than reward good ones. Do not "fix" it into a reward curve.
- **Model attributes as 0–7** (§10). Every table in the engine is eight entries
  wide; anything else means reindexing all of them.
- **Kill aftertouch on contact**, consistently with tackles and frame hits — one
  shared "possession event" hook rather than three scattered calls.
- **Do the whole thing inside the deterministic tick**, driven by the same
  `(direction, fire_state)` input for human and CPU alike
  ([MOVEMENT.md](MOVEMENT.md) §8).

---

## 10. Amiga cross-check

Traced independently through the Amiga original — [amiga/KICKING.md](amiga/KICKING.md)
§5 (`PlayerHeading` asm:39688, `PlayerDoingHeader` asm:38018, `DoStaticHeader`
asm:38285).

### The Heading table has eight entries, not thirteen

`playerStrongHeaderSpeedIncrease` (asm:34852):

| Heading | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 |
|---|---|---|---|---|---|---|---|---|
| Δspeed | −336 | −288 | −240 | −192 | −144 | −96 | −48 | **0** |

Those are §6's first eight values, exactly. The five that follow it there —
+513, +1027, +1541, +2055, +2569 — are not part of this table; they are whatever
data item is laid out next in the segment, and their near-constant stride of ~514
is characteristic of an offset or pointer block rather than a tuning curve. The
Amiga clamps every attribute to 7 on load and every attribute-indexed table in the
binary has exactly eight entries.

Three consequences, all of which reverse conclusions elsewhere in the docs:

- **Heading is a pure handicap ramp with no upside.** Heading 7 gets *no* bonus and
  every lesser value is a penalty. §6's "strongest attribute effect found anywhere"
  is exactly backwards: it is the *weakest*, and it is the only attribute whose
  table can never help you. Against a jump-header launch of 2048 × 5/4 = 2560, a
  Heading-0 player loses 336 — a 13 % cut, and that is the whole range.
- **It settles nothing about the attribute range**, and the 0–12 inference built on
  it is void. The range is 0–7 ([amiga/PLAYERS.md](amiga/PLAYERS.md) §1).
- **The knock-on bounds worry is void too.** [TACKLING.md](TACKLING.md) §10's fear
  that `kPlAvgTacklingBallControlDiffChance` is read out of bounds does not arise;
  an eight-entry table indexed by a difference of two 0–7 values is correctly sized.

This mis-read propagated into [DATA.md](DATA.md) §3, [STATE.md](STATE.md),
[TACKLING.md](TACKLING.md) §10 and [README.md](README.md)'s conventions list; all
are corrected in [AMIGA_CHANGES.md](AMIGA_CHANGES.md).

### Confirmed, exactly

| §7 constant | Amiga symbol | Line | Value |
|---|---|---|---|
| `kJumpHeaderSpeed` | `jumpHeaderSpeed` | asm:30707 | 2048 |
| `kStaticHeaderPlayerSpeed` | — | asm:38300 | $100 (256) |
| `kBallJumpHeaderDeltaZ` | `ballKickingDeltaZ_2` | asm:30587 | $A000 |
| Jump-header ball speed × 1.25 | — | asm:38027 | `speed × 5/4` |
| Player speed after contact × 0.5 | — | asm:38111 | halved |
| Static-header downtime 20 | — | asm:38303 | 20 frames |
| Jump-header duration | — | asm:39694 | **50 frames** |

The Amiga adds one framing worth keeping: the header rise `$A000` is exactly **half**
the kick's `$14000` ([SHOOTING.md](SHOOTING.md) §9). Headers stay lower than kicks
by construction.

### The direction switch is confirmed — and we answer the Amiga's question

§4's seven-way dial matches the Amiga's reading step for step: difference 0 heads
straight on, ±1 nudges the aim one octant, ±2 and ±3 route through two helper
routines that alter `deltaZ` and cut speed *before* also nudging the octant, and
**with no joystick input at all the ball goes straight on with the ±2 helper
applied** — which is exactly §4's `held < 0` → `DoFlyingHeader` row.

The Amiga names those helpers `sub_11280A` and `sub_112822` (asm:38230, asm:38248)
and lists "what they do to `deltaZ` in detail" as an open measurement target.
**§4 answers it**: `0x20000` with speed × 0.75 for the flying header, `0x24000`
with speed × 0.9375 for the lob. This is one of the few places our DOS-port
documents are ahead of the Amiga set, and it should be fed back.

### One disagreement

**The static header's attribute bonus.** §5 adds `kPlayerHeaderSpeedIncrease` on
both header paths. The Amiga is explicit that `DoStaticHeader` consults **no
attribute at all** — it sets the player's own speed to a flat 256, enters a
20-frame settle state, and that is the whole routine. It calls the static header
"the defensive nod, not a scoring header", which is a different design intent from
§5's "predictable and, at 1792, genuinely powerful".

Related: the Amiga records no static-header *ball* speed, so §5's flat 1792 and its
`deltaZ = −deltaZ / 2` reflection are single-sourced. The reflection in particular
is a distinctive mechanic — it is the only place the incoming ball's state affects
the outgoing trajectory — and it deserves confirmation rather than inheritance.
