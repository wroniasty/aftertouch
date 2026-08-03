# CONTEST.md

Who has the ball and how that changes: the proximity and height bands recomputed
every frame, the dribble touch, the sliding tackle and its skill contest, the
recovery timers, and the foul test.

The launch that follows a won ball is [KICKING.md](KICKING.md); the ball physics the
dribble touch manipulates are [BALL.md](BALL.md); the keeper's separate claim on the
ball is [GOALKEEPER.md](GOALKEEPER.md).

> **Provenance.** `CalculateIfPlayerWinsBall` (asm:35144), `sub_110C04` (deflected
> tackle), `sub_110CD8` (recovery timer), `sub_113122` (foul test, asm:39106) and the
> banding block in `UpdatePlayersAndBall` (asm:42131) are read directly. All four
> tables are literals. The one place the reading is inferential is the exact
> semantics of `TeamGeneralInfo` +$8A, flagged in §4.

---

## 0. One-paragraph version

Every frame, for every player, the engine writes two sets of flags into that
player's `TeamGeneralInfo`: how close he is to the ball, in three bands, and how
high the ball is, in five. Those flags gate everything else — you cannot tackle,
head or control a ball outside the right band. A player in possession does not
"hold" the ball; instead, on two frames out of four, `CalculateIfPlayerWinsBall`
re-aims the ball one octant ahead of him and re-imposes a speed drawn from a Ball
Control table, which *is* the dribble. Every touch increments a counter, and when
the counter passes a Ball Control-dependent threshold the player loses control for
eight frames. A sliding tackle that reaches an opponent runs a single skill contest:
the average of the tackler's Tackling and Control minus the same average for the
carrier, looked up in an eight-entry table, compared against five bits of `Rand`.
Whoever wins gets a twelve-frame possession lock. The foul test that follows is
positional, not random: it depends on where the contact happened and whether the
tackle was a genuine attempt at the ball.

---

## 1. The bands

Written at the top of each player's turn in the sweep (asm:42120–42180) into the
flags at `TeamGeneralInfo` +$3D … +$44.

### Proximity — from `Sprite.ballDistance` ($4A, a squared distance)

| Flag | Offset | Condition |
|---|---|---|
| `plVeryCloseToBall` | +$3D | nearest band |
| `plCloseToBall` | +$3E | middle band |
| `plNotFarFromBall` | +$3F | outer band |

These are set by a descending comparison chain against `ballDistance`, so exactly
one is set per player per frame. Because the value is squared, the thresholds are
not directly readable as pixels; the comparable constants used elsewhere are 512
(≈ 23 px), 2048 (≈ 45 px) and 5000 (≈ 71 px) — see asm:42494, asm:42505, asm:42546.

### Height — from ball `z`

| Flag | Offset | Ball z |
|---|---|---|
| `ballLessEqual4` | +$40 | ≤ 4 |
| `ball4To8` | +$41 | 5 … 8 |
| `ball8To12` | +$42 | 9 … 12 |
| `ball12To17` | +$43 | 13 … 17 |
| `ballAbove17` | +$44 | > 17 |

Five bands, and the boundaries are pixel-exact. `ballAbove17` is the important one:
it is tested as a veto in the shot-resolution path (asm:42497, asm:42511, asm:42588)
— **a ball higher than 17 pixels cannot be played by an outfielder at all.** The
crossbar sits at 15–19 ([BALL.md](BALL.md) §6), so the playable ceiling is set just
under the bar.

The bands are the gating mechanism for the whole contest system. Nothing in this
document runs without the right band being set first.

`UpdateBallVariables` (asm:38711) is the companion routine: it publishes the ball's
position under three different filters — `ballDefensive*` (used by the keeper),
`ballNotHigh*` and `strikeDest*` — depending on which height band applies and
whether the ball is rising or falling. These are the pre-filtered ball positions the
AI chases rather than the raw one.

---

## 2. The dribble

There is no "attached ball" state. A player in possession is a player whose feet the
ball keeps being re-aimed at, and the mechanism is `CalculateIfPlayerWinsBall`
(asm:35144), which despite the name runs on every controlled contact.

### The touch

When contact occurs (asm:35228 onward):

```
ball.destX = player.x + defaultPlayerDestinations[dir].dx
ball.destY = player.y + defaultPlayerDestinations[dir].dy
if currentTick bit 1 set:
    impulse = ballSpeedDeltaWhenControlled[BallControl]
else:
    impulse = 0
ball.speed = player.speed + impulse
```

Two consequences.

**The impulse only fires on two frames in four** (asm:35286). On the other two the
ball is simply given the player's speed, so it decelerates under friction between
touches. This is what produces the visible bobble of a SWOS dribble.

**`ballSpeedDeltaWhenControlled` is inverted** (asm:34783):

| Ball Control | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 |
|---|---|---|---|---|---|---|---|---|
| Impulse | 130 | 116 | 102 | 88 | 74 | 60 | 46 | 32 |

A *low* Control player pushes the ball further ahead of himself. Good control means
a smaller touch and a tighter ball. This reads backwards until you see it in motion,
and it is the correct sign — poor players knock the ball away from their feet.

There is also a direction-change bonus (asm:35293–35300): if the ball's fine heading
has drifted more than a quarter-turn from the player's octant, speed gets +256. That
is the extra push needed to bring a ball round on a turn.

### Losing the ball

Every touch where the player's octant differs from the stored kick direction
increments `unkBallTimer` (+$6C, asm:35305). When it exceeds a Control-derived
threshold, `wonBallTimer` is set to 8 and possession is interrupted (asm:35314).

`unk_1106EA` (asm:34791), the threshold table:

| Ball Control | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 |
|---|---|---|---|---|---|---|---|---|
| Touches allowed | 4 | 5 | 6 | 8 | 11 | 14 | 17 | 21 |

A Control-7 player can change direction more than five times as often as a Control-0
player before losing the ball. Note the accelerating spacing — the top of the scale
is worth much more than the bottom. This is the single clearest attribute effect in
the game.

---

## 3. The tackle contest

Entered from `player_tackling` (asm:44014) when a sliding player reaches an opponent.

### The skill difference

`CalculateIfPlayerWinsBall` opens with the contest (asm:35155–35190):

```
tacklerRating = (tackler.Tackling + tackler.BallControl) / 2
carrierRating = (carrier.Tackling + carrier.BallControl) / 2
d = tacklerRating - carrierRating
if d < 0:  d = -d,  and the *carrier* becomes the reference team
r = Rand() & 31
if r < plAvgTacklingBallControlDiffChance[d]:  the reference team keeps possession
```

The averaging of **Tackling and Ball Control together** is the notable part — a
tackle is not decided by Tackling alone on either side. IDA's label for the table
says as much.

`plAvgTacklingBallControlDiffChance` (asm:34774):

| Difference | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 |
|---|---|---|---|---|---|---|---|---|
| Threshold (of 32) | 16 | 17 | 18 | 19 | 20 | 21 | 22 | 23 |

An even contest is **exactly 50/50**. Every point of advantage adds 3.125
percentage points, topping out at 71.9 % for a maximum seven-point gap. The favourite
never becomes a certainty, and the underdog always has better than a one-in-four
chance. That flatness is why SWOS tackles feel like a gamble rather than a
calculation.

Note the sign handling: when the difference is negative the code swaps which team is
the "reference" and then uses the *same* table on the absolute difference. The table
is therefore read as "probability the better player wins", applied symmetrically.

Winner gets `wonBallTimer` = **12 frames** (asm:35194) — a possession lock during
which the contest cannot re-run.

### Deflected tackles

If the tackler's intent flag (`Sprite` $6A) is −1, the tackle is a deflection rather
than a possession attempt and `sub_110C04` runs instead (asm:34920 region). It:

- picks an octant one step away from the carrier's, toward the tackler;
- aims the ball there;
- halves the carrier's speed, then gives the ball `carrier.speed × 3/2`;
- sets `Sprite.field_60` to 1, or to 2 if the tackler was more than ~5 px from the
  ball, marking how clean the deflection was.

No skill contest runs. A deflecting tackle always succeeds in disturbing the ball and
never wins it outright.

### Recovery

`sub_110CD8` sets the tackler's `playerDownTimer` from his own Tackling:

| Tackling | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 |
|---|---|---|---|---|---|---|---|---|
| `unk_1106B2` — normal tackle | 30 | 27 | 24 | 21 | 18 | 15 | 12 | 9 |
| `unk_1106C2` — deflecting tackle | 3 | 3 | 3 | 3 | 3 | 3 | 3 | 3 |

A Tackling-0 player is on the floor for **30 frames** (0.6 s); a Tackling-7 player for
9 (0.18 s). A deflecting tackle costs a flat 3 frames regardless. The cost of a
mistimed slide is the entire downside of tackling, and it is attribute-scaled while
the reward is not.

Player friction during the slide is `playerGroundConstant` = 96 per frame
(asm:44035) — six times the ball's, so a slide launched at `playerTacklingSpeed`
1792 covers about 17 pixels.

---

## 4. Fouls

`sub_113122` (asm:39106) runs on tackle contact and decides whether it was a foul.
It is **entirely positional** — no `Rand` call anywhere in the decision.

The chain:

1. Squared distance between tackler and victim must be ≤ 32 (≈ 5.7 px) — asm:39118.
2. If the victim is the goalkeeper (`shirtNumber` 1), the tackler is just slowed
   (`speed >> 2`) and nothing else happens. **Keepers cannot be fouled.**
3. The victim must be inside X 81 … 590, Y 129 … 769 — a tackle on a player already
   off the pitch is not a foul.
4. Tackler speed is cut to a quarter, and `sub_111388` runs the card escalation.
5. If the victim's `ballDistance` exceeds 800 (≈ 28 px), no foul — the tackler was
   nowhere near the ball but neither was the victim.
6. If `Sprite.field_60` is set (a deflection happened) and equals 2, no foul. If it
   is 1, the foul only stands when the two players' octants differ by more than one
   (asm:39178) — a tackle from behind or across is a foul; one from alongside going
   the same way is not.
7. Otherwise: increment the team's foul count and award the free kick.

Then a penalty-area test (asm:39197–39232): if the victim is inside X 193 … 478 and
Y ≤ 216 (top box) or Y ≥ 682 (bottom box), it becomes a penalty. Those bounds give a
box **285 pixels wide and 87 deep**, centred on the goal.

The free-kick position is chosen by scanning the fouled side's outfielders for the
one nearest the opponent's goal (asm:39248–39292) — the taker is picked by position,
not by role.

`sub_111388` (asm:36096) handles cards, gated on a global at asm:36098 and on a
`Rand() & 3` — so cards are the one random element in the foul system, at roughly a
one-in-four rate, and only when a difficulty flag permits.

### The `wonBallTimer` caveat

`TeamGeneralInfo` +$8A is set to 12 by the contest, to 8 by the touch-count
overflow, and to 12 again by the tackle path at asm:44182. It is read as a general
"possession is locked, do not re-run contests" gate. Whether the two values (8 and
12) differ in any consumer, or whether it is simply two writers agreeing on a
concept, was not established. Treated here as one timer with two reload values.

---

## 5. Constants quick reference

| Symbol | Line | Value | Meaning |
|---|---|---|---|
| Height bands | 42164 | 4, 8, 12, 17 | `z` boundaries |
| Playable ceiling | 42497 | z ≤ 17 | Above this, unplayable |
| `ballSpeedDeltaWhenControlled` | 34783 | 130 … 32 | Dribble touch, by Control (inverted) |
| Dribble impulse phase | 35286 | `currentTick` bit 1 | 2 frames in 4 |
| Turn bonus | 35296 | +256 | Heading drift > quarter turn |
| `unk_1106EA` | 34791 | 4 … 21 | Touches before loss, by Control |
| `plAvgTacklingBallControlDiffChance` | 34774 | 16 … 23 of 32 | Contest odds |
| Contest RNG mask | 35187 | `Rand() & 31` | |
| `wonBallTimer` after contest | 35194 | 12 frames | |
| `wonBallTimer` after touch overflow | 35314 | 8 frames | |
| `unk_1106B2` | 34747 | 30 … 9 | Tackle recovery, by Tackling |
| `unk_1106C2` | 34763 | 3 | Deflection recovery, flat |
| `playerTacklingSpeed` | 30706 | 1792 | Slide launch |
| `playerGroundConstant` | 30584 | 96 | Player friction per frame |
| Foul contact radius | 39118 | 32 squared | ≈ 5.7 px |
| Foul ball-distance limit | 39190 | 800 squared | ≈ 28 px |
| Penalty area | 39205 | X 193…478, Y ≤216 / ≥682 | 285 × 87 px |
| Card rate | 36100 | `Rand() & 3` | ≈ 1 in 4, when enabled |

---

## 6. What this resolves, and what still needs measurement

Confirmed:

- ✓ Five height bands with exact pixel boundaries, and a hard playable ceiling at 17.
- ✓ Three proximity bands, exactly one set per player per frame.
- ✓ Possession is re-imposed by re-aiming, not by attachment.
- ✓ The dribble impulse fires on a fixed 2-in-4 frame phase.
- ✓ The dribble impulse is inverted — low Control pushes the ball further.
- ✓ Touch-count loss with a Control table that accelerates sharply at the top.
- ✓ The tackle contest averages Tackling and Ball Control on both sides.
- ✓ An even contest is exactly 50/50; maximum advantage is 71.9 %.
- ✓ Tackle recovery is Tackling-scaled, 30 down to 9 frames.
- ✓ Deflecting tackles skip the contest entirely and cost a flat 3 frames.
- ✓ Fouls are positional with no RNG; cards are the only random element.
- ✓ Keepers cannot be fouled.
- ✓ Penalty-area dimensions.

Open (measurement targets, [../LEGACY.md](../LEGACY.md) §15):

- The three proximity-band thresholds. They are a comparison chain against a squared
  distance and the constants were not isolated cleanly.
- Whether `wonBallTimer`'s two reload values (8 and 12) are distinguished anywhere.
- What sets `Sprite.field_6A` to −1 to mark a deflecting tackle — i.e. what makes a
  slide a deflection rather than a challenge. This is player-facing and matters.
- The card escalation inside `sub_111388`: which offence yields yellow versus red,
  and whether a second yellow is tracked.
- Whether `Sprite.field_60`'s value of 2 (distant deflection) has consumers beyond
  the foul test.

---

## 7. Guidance for the reimplementation

- **Compute the bands once per player per frame and gate on them**, exactly as the
  original does. Every contest routine assumes they are already correct; deriving
  them lazily inside each will drift.
- **Do not model possession as attachment.** The re-aim-and-re-speed model is what
  produces the loose, contestable SWOS ball. A parented ball plays completely
  differently and cannot be tuned back.
- **Keep the 2-in-4 impulse phase and keep it tied to the tick counter.** It is
  deterministic and it is the visible texture of dribbling.
- **Preserve the inverted Control table's sign.** It will look like a bug in review;
  it is not.
- **Make the contest exactly 50/50 at zero difference.** This is a design statement
  and it is cheap to get right.
- **Charge the recovery cost.** Tackling is balanced by the downside, not by the odds
  — the odds barely move. If our tackles are cheap the whole game breaks.
- **Keep fouls deterministic.** Only the card draw touches `Rand`. Resisting the urge
  to randomise the foul itself keeps replays and traces stable and matches the
  original.
