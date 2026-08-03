# PASSING.md

The pass, end to end: how a target is chosen, how direction and strength are
derived from that target, how the pass is made deliberately inaccurate, what the
receiver does about it, and — the part with the most consequence for how the game
*feels* — why the receiver cannot be controlled until he has the ball, and every
way that state gets torn down.

The launch mechanics a pass shares with a kick are [KICKING.md](KICKING.md) §1; the
curl a human can apply afterwards is [AFTERTOUCH.md](AFTERTOUCH.md) §4; the dribble
that follows reception is [CONTEST.md](CONTEST.md) §2.

> **Provenance.** `DoPass` (asm:34859), `GetClosestNonControlledPlayerInDirection`
> (asm:39859), `sub_111B98` (the player-selection routine, asm:36982),
> `sub_118290` (asm:~48000) and the `player_expecting_pass` branch of
> `UpdatePlayersAndBall` (asm:44377–44700) are all read directly. Every table is a
> literal. **This document corrects two claims made in the first pass of this doc
> set** — the pass-strength ramp is distance-banded, not hold-duration-banded, and
> the Passing attribute *is* read in-match, twice. Both corrections are noted in
> [../AMIGA_CHANGES.md](../AMIGA_CHANGES.md).

---

## 0. One-paragraph version

Tapping fire runs `DoPass`, which scans your own ten outfielders for the nearest one
lying within ±22.5° of the direction you are facing **as measured from the ball**,
and makes him the designated receiver. Direction is then set by extending the ray
from ball to receiver, doubling it until the aim point leaves the pitch, so the ball
travels *through* him rather than stopping at him. Strength comes from an
eight-band table keyed on the **distance to the receiver**, in 50-pixel steps from
$600 to $8AA, plus a bonus of up to +384 from the passer's **Passing** attribute.
For CPU sides only, Passing also drives an accuracy roll: on a failure the aim
vector has one of its two axes halved, skewing the pass off the receiver. From that
moment the receiver is in a special state. He is excluded from the player-selection
routine, so **you cannot switch to him**; the engine gives him a destination that is
either his own position (pass is on target — stand still) or a point 90° off the
ball's flight line (pass is off target — step into its path). Only when the ball is
at his feet — height band ≤ 4 and inside a speed-dependent proximity band — does
control transfer, along with a 25-frame switch lock. Everything that tears this down
is a special case bolted onto that one state, which is why it feels clunky: there is
no interception test, only a rule that abandons the pass if the ball's heading
strays more than 90° from the receiver.

---

## 1. Choosing the target

`GetClosestNonControlledPlayerInDirection` (asm:39859) walks the passing side's
eleven sprites and keeps the best candidate.

### The cone

The acceptance test (asm:39883) is the part worth getting exactly right, because it
is not what it first looks like:

```
d0 = passerFacingOctant << 5          ; octant → 256ths of a turn
diff = (candidate.fullDirection - d0) as a signed byte
accept if -16 <= diff <= 16
```

`Sprite.fullDirection` (+$52) on a *player* is not his facing. It is written at the
end of every player's turn in the sweep (asm:45206) by calling
`CalculateDeltaXAndY` from the player toward the ball, and the idle-facing code at
asm:45168 adds 128 to it to make a player look at the ball. So:

> **`Sprite.fullDirection` on a player is the 256-step angle from the ball to that
> player.** ([STATE.md](STATE.md) §2 is corrected accordingly.)

The test therefore means: *the candidate must lie within ±16/256 = **±22.5°** of the
passer's facing, as seen from the ball.* It is a cone anchored at the ball, not at
the passer — which matters whenever the passer and the ball are not coincident, i.e.
during most of a dribble.

### The filters

A candidate is rejected if any of these hold (asm:39865–39880):

| Rejected when | Field |
|---|---|
| He is the currently controlled player | `== controlledPlayerSprite` |
| He is unavailable (sent off) | `Sprite` +$6C ≠ 0 |
| He is not in `PL_NORMAL` | `Sprite.playerState` ≠ 0 |
| He is outside the cone | as above |

Among survivors, **smallest `ballDistance` wins** (asm:39885). Note the goalkeeper is
*not* excluded — you can pass back to the keeper.

If nothing survives, `a0` returns −1 and the pass degrades to a directional
clearance (§3).

---

## 2. Direction

Once a target exists, `passToPlayerPtr`, `passingBall` and `passingToPlayer` are all
set (asm:34873–34875), and the aim point is computed one of two ways.

### Accurate — the ray extension

`calculate_pass_to_player_delta_x_y` (asm:34913):

```
d1 = target.x - ball.x
d2 = target.y - ball.y
if both are zero: d1 = 1                      ; degenerate guard
while (ball.x + d1) in [0, 672) and (ball.y + d2) in [0, 880):
    d1 <<= 1 ;  d2 <<= 1
destX = ball.x + d1 ;  destY = ball.y + d2
```

The vector is **doubled until the aim point leaves the pitch's bounding box**. The
direction is preserved exactly — doubling both components is a pure scale — but the
aim point ends up far beyond the receiver.

That is deliberate and it is the same idiom as everywhere else in the engine
([BALL.md](BALL.md) §1): the ball chases its aim point every frame, so an aim point
*at* the receiver would make the heading unstable as the ball closes, and any
overshoot would send the ball back. Extending the ray gives a stable heading for the
whole flight and lets the ball run past the receiver if he misses it.

### Inaccurate — the axis skew

Only reachable for CPU sides (§4). `loc_1107D6` (asm:34890):

```
dx = target.x - ball.x
dy = target.y - ball.y
if (stoppageTimer bit 5) == 0:  dx >>= 1        ; halve X
else:                           dy >>= 1        ; halve Y
destX = ball.x + (dx << 5)
destY = ball.y + (dy << 5)
```

Exactly one axis is halved, and which one alternates every 32 frames with the frame
counter. Halving one component of a direction vector **rotates it**, by up to 26.6°
when the two components were equal, and by nothing at all when the halved axis was
already zero. So a pass straight up the pitch is never skewed, and a diagonal pass
is skewed most — a quirk, but a consistent one.

The `<< 5` replaces the doubling loop with a flat ×32, achieving the same "aim well
beyond the target" effect.

---

## 3. Strength

`loc_110856` (asm:34941) is a descending comparison chain on the **receiver's
squared distance to the ball** (`Sprite.ballDistance`, +$4A):

| Squared distance | Approx. pixels | Base speed |
|---|---|---|
| < 2 500 | < 50 | $600 (1536) |
| < 10 000 | < 100 | $680 (1664) |
| < 22 500 | < 150 | $700 (1792) |
| < 40 000 | < 200 | $755 (1877) |
| < 62 500 | < 250 | $7AA (1962) |
| < 90 000 | < 300 | $800 (2048) |
| < 122 500 | < 350 | $855 (2133) |
| ≥ 122 500 | ≥ 350 | $8AA (2218) |

Then the Passing bonus (asm:34980), `unk_110670` indexed by `PlayerGame` +$45:

| Passing | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 |
|---|---|---|---|---|---|---|---|---|
| Bonus | 0 | +48 | +96 | +144 | +192 | +256 | +320 | +384 |

**Pass speed is a function of how far the ball has to travel, plus the passer's
Passing.** It is not affected by how long the button was held — that was a wrong
claim in the first pass of [KICKING.md](KICKING.md) and is corrected there.

The bands are 50 pixels wide and the speed steps are small — from 1536 at point-blank
to 2218 at 350 pixels, a 44 % range across the whole pitch. Combined with linear
friction ([BALL.md](BALL.md) §2), this is a crude but effective "pass it hard enough
to arrive" heuristic. A Passing-7 player adds 384, which is roughly four distance
bands' worth: **a good passer's short ball arrives with the pace of an average
player's long one.**

### No target

`no_closest_player` (asm:34994) falls back to `GetBallDestCoordinatesTable`, giving
the plain ±1000 directional offset ([KICKING.md](KICKING.md) §1), and a flat speed of
`word_110690` = **$700 (1792)**. A pass with nobody in the cone is a clearance in the
direction you were facing.

---

## 4. Accuracy, and who it applies to

The accuracy roll (asm:34877–34889) is guarded by two tests:

```
if gameStatePl != 100:            skip the roll (accurate)
if TeamGeneralInfo.playerNumber != 0:  skip the roll (accurate)
```

`playerNumber == 0` means the side is CPU-controlled — confirmed by asm:43326, where
the same test decides whether to call `DoAI` for the "controlled" player. So:

> **The pass-accuracy roll applies to CPU sides only. A human's pass is always aimed
> exactly at the chosen receiver.**

That is a defensible design rather than an oversight: a human already chose the
direction with the joystick, so injecting error would read as the game fighting the
input. For the CPU, whose target is picked automatically, inaccuracy is the only
place player skill can show.

The roll itself:

```
threshold = dseg_17E286[Passing]
roll      = (stoppageTimer & 0x1E) >> 1        ; 0 … 15
if roll >= threshold:  accurate
else:                  inaccurate
```

`dseg_17E286` (asm:34807):

| Passing | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 |
|---|---|---|---|---|---|---|---|---|
| Threshold (of 16) | 6 | 4 | 3 | 2 | 1 | 0 | 0 | 0 |
| Chance of a bad pass | 37.5 % | 25 % | 18.75 % | 12.5 % | 6.25 % | 0 % | 0 % | 0 % |

**Passing 5 and above never misplaces a pass.** The curve is steep and it saturates
early, which is why CPU teams with good midfielders keep the ball so much better in
SWOS than the 0–7 scale would suggest.

Like the goal/save roll ([GOALKEEPER.md](GOALKEEPER.md) §4), the source of randomness
is the **frame counter, not `Rand`** — the pass is deterministic given the match
state.

### Aftertouch on passes is human-only

At asm:35009, after the launch:

```
ResetLeftAndRightSpinTimers()               ; both sides → -1
if TeamGeneralInfo.playerNumber != 0:
    spinTimer = 0                           ; open the aftertouch window
passInProgress = 1
```

So a CPU pass gets **no aftertouch window at all**. Kicks are different — 
`PlayerKickingBall` (asm:35127) opens the window unconditionally — so the CPU can
curl a shot but never a pass. [AI.md](AI.md) §3 is corrected accordingly.

---

## 5. The passer gives up control

Immediately after `DoPass` returns, the firing block (asm:43642–43658) does this:

```
controlledPlayerSprite ($20) = 0            ; nobody is controlled
passingKickingPlayer   ($68) = the passer
passKickTimer          ($66) = 25 frames
ballCanBeControlled    ($6E) = 0
opponent.passingToPlayer     = 0            ; cancels their pending pass
opponent.passingKickingPlayer = 0
opponent.spinTimer           = -1
→ stop_player                                ; the passer's destination = his position
```

Three things happen at once: the passer is **stopped dead**, control is **released**,
and the opponent's pending pass state is **wiped**. The kick path (asm:43653) is
identical except that it does not set a receiver.

`passKickTimer` counts down 25 frames and then clears `passingKickingPlayer`
(asm:42007). During that window the passer is barred from being re-selected (§6) —
this is what stops the ball rebounding straight back to the player who just hit it.

---

## 6. Why the receiver is not controllable

This is the mechanic the whole document exists for, and it is implemented in one
line of `sub_111B98` (asm:36982), the player-selection routine called from
`Joystick_Wait` every frame.

The routine recomputes `ballDistance` for all eleven players, then picks the nearest
*eligible* one and assigns it to `controlledPlayerSprite`. The eligibility filter:

| Rejected when | Field |
|---|---|
| Inactive during live play | `Sprite` +$54 == 0 |
| Sent off / unavailable | `Sprite` +$6C ≠ 0 |
| Is the goalkeeper, unless the keeper is out | `shirtNumber` == 1 and `goaliePlayingOrOut` == 0 |
| **Is the player who just passed or kicked** | `== passingKickingPlayer` (+$68) |
| Is tackling, tackled, heading, settling or injured | `playerState` ∈ {1, 3, 8, 9, $D} |
| **Is the designated pass receiver** | `== passToPlayerPtr` (+$24) |

> The receiver is excluded from selection outright. He is not "controlled but
> locked" — he is invisible to the thing that decides who you control.

So after a pass, `controlledPlayerSprite` is 0 and the nearest *other* player is
picked instead. You are moved onto a third player while the ball is in the air.
The receiver runs on his own logic (§7) until he actually gets the ball, at which
point control transfers to him (§8).

Two further details of the routine:

- The whole assignment is gated on `ballOutOfPlay` (+$60) being non-zero — a flag
  that is set to 1 on every kick and pass (asm:43648).
- When selection *changes*, the previously controlled player is **stopped** (his
  destination is set to his own position, asm:37059). Letting go of a player leaves
  him standing still, not coasting.

The trailing block at asm:37070–37180, which would clear `passToPlayerPtr` if the
selected player were the receiver, is unreachable — the receiver was excluded from
the scan, so the equality can never hold. It reads as a leftover from an earlier
design where the receiver *was* selectable.

---

## 7. What the receiver does

`player_expecting_pass` (asm:44377) is entered for whichever player equals
`passToPlayerPtr`. In order:

### Bounds

If the receiver is outside X 81…590 or Y 129…769, `passingToPlayer` and
`passingBall` are cleared and he is stopped (asm:44389). **A receiver who leaves the
pitch cancels the pass.**

### The adjustment — running to meet an inaccurate pass

Gated on three conditions (asm:44402–44412):

1. `playerNumber != 0` — **human sides only**;
2. the pass was lofted or curled: `longPass` (+$7C) or `leftSpin` or `rightSpin` set;
3. ball speed ≥ $200, and `passingToPlayer` still set.

Then the deviation is measured:

```
d0 = ball.fullDirection - receiver.fullDirection      ; both 256ths, signed byte
```

Since a player's `fullDirection` is the angle from the ball to him (§1), this is
**how far the ball's actual heading has strayed from the line to the receiver**.

| Deviation | Outcome |
|---|---|
| beyond ±64 (±90°) | **Pass abandoned** — clear `passingToPlayer` and `passingBall` |
| within ±4 (±5.6°) | On target — the receiver's destination is *his own position*; he stands and waits |
| between | **Step into the path** — see below |

The intercept point (asm:44445):

```
offset = 0xC0 (= -64, i.e. 90°)
if longPass:  offset = 0xE0 (= -32, i.e. 45°)
if deviation >= 0:  offset = -offset            ; lean toward the ball's side
dir = (ball.fullDirection + offset + 4) & 255
target = receiver.position + unk_11169A[dir >> 3]
```

`unk_11169A` (asm:36368) is a **32-direction offset table** — the eight-octant ±1000
table refined to 32 steps, with intermediate components 199, 414 and 668
(= 1000·tan of 11.25°, 22.5° and 33.75°). So the receiver is sent 1 000 pixels in a
direction **perpendicular to the ball's flight** (or 45° off it for a lofted pass),
on the side the ball is passing. In practice he takes a step or two sideways into
the ball's line and stops when it arrives.

That is exactly the behaviour the mechanic is known for. Note it is a **fixed 90°
sidestep**, not a computed interception — the engine does not solve for where the
ball will be. It just pushes the receiver sideways and lets the reception test (§8)
decide whether he got there.

The target is stored in `TeamGeneralInfo` +$62 / +$64, two fields previously
unaccounted for in [STATE.md](STATE.md) §4.

### Chasing

Further down (asm:44625), whether the receiver actually runs at that target depends
on `passingBall` (+$58), which acts as a "chase now" latch:

```
if passingBall set:                          use $62/$64 as destination
else if no controlled player                 → set passingBall = 1
else if controlled.ballDistance > 3200       → set passingBall = 1
else if controlled.ballDistance > receiver.ballDistance → set passingBall = 1
else if receiver.ballDistance > 3200         → use $62/$64
else                                          stand still
```

So the receiver only commits to chasing once he is genuinely the closest man, or the
controlled player is more than about 57 pixels off the ball. Before that he holds
position — which is why a short pass to a nearby player looks like he simply waits
for it.

### Intervening

A human is not entirely locked out. At asm:44529, if fire is triggered while the
receiver is `plNotFarFromBall` with the ball low and he is closer to it than the
currently controlled player, `header` is set and a slide/challenge runs. You can
still commit the incoming receiver to a challenge; you just cannot walk him around.

### The CPU receiver

For CPU sides, `sub_118290` runs instead (asm:44631). If the receiver is within
`ballDistance` ≤ 200 (≈ 14 px) and the opponent's controlled player is facing within
±45° of him, it synthesises fire and a direction — **a first-time pass under
pressure**. That is the entire CPU one-touch game.

---

## 8. Reception, and the transfer of control

`loc_116D30` (asm:44557). The test:

```
require ballLessEqual4                       ; ball height z <= 4
and    ( plVeryCloseToBall
         or (ball.speed >= 0x600 and plCloseToBall) )
```

A **fast ball is caught from the wider band, a slow ball only from the near band** —
which is right, since a fast ball crosses the near band in fewer frames.

On success:

```
controlledPlayerSprite ($20) = passToPlayerPtr ($24)
passToPlayerPtr              = 0
playerSwitchTimer      ($5C) = 25 frames
passingBall            ($58) = 0
passingToPlayer        ($5A) = 0
shooting               ($3A) = 0
receiver.dest = receiver.position            ; he stops dead
sub_11381A()                                 ; snap the ball to his feet
  (or UpdateBallWithControllingGoalkeeper if he is the keeper)
passKickTimer          ($66) = 0
passingKickingPlayer   ($68) = 0
lastTeamPlayed / lastPlayerPlayed = this player
```

Two consequences worth calling out.

**The receiver stops.** His destination is set to his own position, so the frame you
gain control he is stationary. The momentum you had running onto the pass is gone.
This is very SWOS and it is a single line.

**The ball is snapped to his feet** by `sub_11381A` (asm:39905), which places it at
the player's position plus a ±1 offset in his facing octant, zeroes its speed and
halves its `deltaZ`. Reception is not a physical capture; it is a teleport.

`playerSwitchTimer` is then 25 frames. It is decremented at asm:42022 and, in this
engine, its expiry is what drives the per-frame call to `ApplyBallAfterTouch`
(asm:42026) — the same field gates both control switching and the aftertouch tick.

---

## 9. Termination — every way the pass state comes down

The user-visible clunkiness comes from there being no single owner of this state.
Nine different sites tear it down:

| # | Trigger | What is cleared | Line |
|---|---|---|---|
| 1 | Reception completes | `$24`, `$58`, `$5A`; sets `$5C` = 25 | 44557 |
| 2 | Receiver leaves the pitch | `$5A`, `$58` | 44389 |
| 3 | Ball heading strays > ±90° from the receiver | `$5A`, `$58` | 44425 |
| 4 | Keeper receiver would leave his area | `$24`, `$58`, `$5A` | 44700 |
| 5 | Goal-out with the receiver in the goal region | stops the player | 44712 |
| 6 | **This** side passes or kicks again | opponent's `$5A`, `$68` | 43601, 43662 |
| 7 | Goal scored | `$66`, `$68` | 42642 |
| 8 | Keeper catches or claims | `$66`, `$68` | 42699 |
| 9 | `passKickTimer` expires (25 frames) | `$68` only | 42007 |

**There is no interception test.** If an opponent takes the ball, nothing directly
cancels your pass. It ends only indirectly — via #3 when the ball's heading changes
enough, or via #6 when the opponent themselves kicks. Until one of those fires, your
receiver keeps running at a ball that is no longer yours and *you still cannot
select him*.

That is the clunk. It is not a bug so much as an emergent gap: the design assumed
the ball keeps travelling roughly toward the receiver, and every path that breaks
that assumption got its own patch rather than a shared "is this pass still live?"
predicate.

---

## 10. Constants quick reference

| Symbol | Line | Value | Meaning |
|---|---|---|---|
| Pass cone | 39883 | ±16 of 256 | ±22.5°, measured from the ball |
| Ray-extension bounds | 34930 | X < 672, Y < 880 | Doubling stops here |
| Inaccurate skew | 34893 | one axis `>> 1`, then `<< 5` | Axis alternates on `stoppageTimer` bit 5 |
| Distance bands | 34943–34970 | 50 … 350 px in 50s | Eight bands |
| Base speeds | 34680–34691 | $600 … $8AA | By distance band |
| No-target speed | 34691 | $700 (1792) | Clearance |
| `unk_110670` | 34701 | 0 … +384 | Passing power bonus |
| `dseg_17E286` | 34807 | 6, 4, 3, 2, 1, 0, 0, 0 | Bad-pass chance of 16, by Passing |
| Accuracy roll source | 34884 | `(stoppageTimer & $1E) >> 1` | Not `Rand` |
| Accuracy roll scope | 34878 | `playerNumber == 0` | CPU sides only |
| Pass aftertouch scope | 35011 | `playerNumber != 0` | Human sides only |
| `passKickTimer` | 43658 | 25 frames | Passer excluded from selection |
| `playerSwitchTimer` | 44563 | 25 frames | Set on reception |
| Abandon threshold | 44427 | ±64 of 256 | ±90° heading deviation |
| On-target threshold | 44437 | ±4 of 256 | ±5.6°; receiver stands still |
| Sidestep angle | 44445 | 90°, or 45° if lofted | |
| `unk_11169A` | 36368 | 32 directions × ±1000 | Intercept offsets |
| Chase commit distance | 44636 | 3200 squared ≈ 57 px | |
| Reception height | 44557 | `ballLessEqual4` (z ≤ 4) | |
| Reception speed split | 44559 | $600 | Fast ball → wider band |
| CPU one-touch range | 44639 | 200 squared ≈ 14 px | |
| CPU one-touch pressure arc | 44649 | ±32 of 256 (±45°) | |

---

## 11. What this resolves, and what still needs measurement

Confirmed:

- ✓ The pass cone is ±22.5°, anchored at the **ball**, not the passer.
- ✓ `Sprite.fullDirection` on a player is the angle **from the ball to him**.
- ✓ Direction is set by extending the ball→receiver ray until it leaves the pitch.
- ✓ Strength is banded by **distance to the receiver**, in eight 50-pixel steps —
  correcting the first pass of [KICKING.md](KICKING.md) §4.
- ✓ **Passing is read in-match, twice**: as a power bonus up to +384, and as the
  accuracy threshold — correcting [PLAYERS.md](PLAYERS.md) §2.
- ✓ Passing 5+ never misplaces a pass.
- ✓ The accuracy roll applies to CPU sides only; aftertouch on passes to human sides
  only.
- ✓ Inaccuracy is implemented by halving one axis of the aim vector, alternating on
  the frame counter.
- ✓ The receiver is excluded from player selection outright — this is why he cannot
  be controlled.
- ✓ The passer is also excluded, for 25 frames.
- ✓ On-target passes make the receiver stand still; off-target passes send him 90°
  (or 45°) into the ball's path via a 32-direction offset table.
- ✓ The receiver only commits to chasing once he is closest, or the controlled player
  is > 57 px from the ball.
- ✓ Reception requires ball height ≤ 4 plus a speed-dependent proximity band.
- ✓ On reception the receiver **stops dead** and the ball is **snapped to his feet**.
- ✓ There is no interception test; the pass ends only via the ±90° heading rule or
  when someone kicks.
- ✓ `TeamGeneralInfo` +$62/+$64 hold the receiver's adjusted destination.

Open (measurement targets, [../LEGACY.md](../LEGACY.md) §15):

- What sets `Sprite` +$54, the "active" flag that gates selection eligibility.
- What `TeamGeneralInfo` +$74 is; it is cleared on reception alongside the pass
  state.
- Whether `ballOutOfPlay` (+$60) really gates all selection, or whether it is set
  often enough that the gate is vacuous. The name and the usage do not agree.
- `sub_1138CC`, called on the stopped-play reception path only.
- Whether the unreachable block at asm:37070 was live in an earlier build — it would
  indicate the receiver used to be selectable, which would be useful design history.
- The tap-versus-hold threshold that routes to `DoPass` rather than
  `PlayerKickingBall`. `quickFire` (+$30) and `normalFire` (+$31) are the two flags;
  where the frame threshold between them is set was not traced.

---

## 12. Guidance for the reimplementation

- **Build the pass as a small explicit state machine with one owner**, holding:
  target, phase (`aimed` → `chasing` → `received` → `dead`), and a single
  `isStillLive()` predicate. The original's nine independent teardown sites are the
  source of every complaint about SWOS passing, and consolidating them is the one
  place where a faithful clone should *not* be bug-for-bug faithful.
- **Add the missing interception rule** to that predicate. "An opponent has touched
  the ball" is one line and it removes the worst of the clunk. Record it in
  [../implementation/PLAN.md](../implementation/PLAN.md) as a deliberate departure,
  and keep the original behaviour behind a flag so traces still compare.
- **Keep the receiver un-selectable.** This is the mechanic, not a limitation. It is
  what makes SWOS passing a commitment rather than a teleport, and it is the reason
  the pass cone and the accuracy roll matter at all. Implement it exactly as the
  original does — as an exclusion in the selection filter, not as an input lock,
  because the two differ when the ball changes hands.
- **Keep the reception snap.** Stopping the receiver and teleporting the ball to his
  feet looks crude written down and is completely right in motion.
- **Keep the ±90° abandon rule even after adding interception.** It is what handles
  deflections and rebounds, which an interception test alone would miss.
- **Anchor the pass cone at the ball.** Anchoring it at the passer is the obvious
  reading and it is wrong; it changes which player is selected whenever the ball is
  ahead of the dribbler, which is most of the time.
- **Preserve the human/CPU asymmetries deliberately.** Human passes accurate and
  curlable, CPU passes fallible and straight, is a coherent design. If we change it,
  change it knowingly.
