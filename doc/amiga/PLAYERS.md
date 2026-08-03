# PLAYERS.md

The seven player attributes: what they are, where they live, how they are packed,
and — the reason this document exists — **which offset is which skill**, settled from
three independent access sites rather than from a label.

Every consumer of these attributes is documented elsewhere; this collects them into
one table so the effect of each rating can be read at a glance.

> **Provenance.** `AdjustPlayerSkills` (asm:102057) is read directly and gives the
> packing and the write order. The identification of individual skills comes from
> triangulating three read sites in the match engine against SWOS's own published
> attribute vocabulary. Two of IDA's field names contradict this; the reasoning is
> laid out in §1 so it can be checked rather than trusted.

---

## 0. One-paragraph version

A player's ratings are stored in the team record as **seven 4-bit nibbles packed into
one longword**, masked with `$07777777`. `AdjustPlayerSkills` unpacks them
most-significant-nibble first into seven consecutive bytes at `PlayerGame` +$45,
clamping each to 7, and then computes an eighth value — the goalkeeper rating — from
the player's transfer value rather than from any stored skill. All eight are on a
**0–7 scale**, which the interface displays offset by one. The seven, in the order
they unpack, are SWOS's canonical Passing, Velocity, Heading, Tackling, Ball Control,
Speed, Finishing. Velocity is shot *power* and governs long shots; Finishing is
separate and governs shots from inside the box. Every table in the engine indexed by
an attribute has exactly eight entries, which is the structural confirmation that the
scale is 0–7 and not 1–8 or 0–9.

---

## 1. Identifying the skills

The listing labels only four of the eight bytes, and one of those labels is
misleading. The identification below rests on read sites, not names.

### The packing tells us there are seven

`AdjustPlayerSkills` (asm:102092–102117):

```
d1 = teamRecord[$68]              ; longword
d1 &= 0x07777777                  ; keep seven nibbles, clear the eighth
d1 <<= 4
a0 = PlayerGame + 0x45
repeat 7 times:
    d1 = rol(d1, 4)
    v  = d1 & 0x0F
    v  = transform(v)             ; sub_126CEE — form/morale adjustment
    if v > 7: v = 7
    *a0++ = v
```

Seven iterations, seven consecutive bytes starting at $45, each clamped to 7. So the
skill block is **$45 … $4B** and the scale is **0–7**. The mask `$07777777` also
confirms the eighth nibble is not a skill.

> **This contradicts [../DATA.md](../DATA.md) §3**, which reads the file format's
> 4-bit nibbles as a 0–15 range and concludes that four attribute-indexed tables in
> the engine documents are undersized. Both readings are of real things: the
> *storage* is 4 bits wide, but the mask here is `7` per nibble, not `F`, and the
> clamp at asm:102117 is to 7. The **used range is 0–7**, and eight-entry tables are
> exactly the right size. Any value of 8–15 in a team file is truncated on load.
> This should be reconciled in `DATA.md` and in [../LEGACY.md](../LEGACY.md) §15.

The unpack order is most-significant nibble first: byte $45 receives bits 24–27,
$46 receives 20–23, and so on down to $4B receiving bits 0–3.

### The read sites fix which is which

Three routines read individual bytes of that block for a documented purpose:

| Site | Reads | Purpose | Document |
|---|---|---|---|
| asm:35107 | $46 | Bonus for a **long shot** | [KICKING.md](KICKING.md) §3 |
| asm:35100 | $4B | Bonus for a **close-range shot** | [KICKING.md](KICKING.md) §3 |
| asm:38106 | $47 | Bonus for a **header** | [KICKING.md](KICKING.md) §5 |
| asm:35159 | $48, $49 | Averaged for the **tackle contest** | [CONTEST.md](CONTEST.md) §3 |
| asm:35288 | $49 | **Dribble touch** magnitude | [CONTEST.md](CONTEST.md) §2 |
| asm:35306 | $49 | **Touches before losing the ball** | [CONTEST.md](CONTEST.md) §2 |
| asm:35001 | $48 | **Tackle recovery time** | [CONTEST.md](CONTEST.md) §3 |
| asm:35403 | $4A | **Running speed** | [MOVEMENT.md](MOVEMENT.md) §3 |
| asm:42602 | $4C | **Goalkeeper save odds** | [GOALKEEPER.md](GOALKEEPER.md) §4 |

That fixes $47 = Heading, $48 = Tackling, $49 = Ball Control, $4A = Speed,
$4B = Finishing, $4C = goalkeeper rating. Two remain: $45 and $46.

$46 is read for long-shot power. In SWOS's own vocabulary the shot-power attribute
is **Velocity** — the game has both Velocity and Finishing and they are distinct
ratings. $45, by elimination and by position, is **Passing**.

Laying the seven out in unpack order gives **P V H T C S F** — Passing, Velocity,
Heading, Tackling, Control, Speed, Finishing — which is exactly SWOS's published
attribute order. Seven independent constraints landing on the game's own documented
ordering is about as strong as this kind of identification gets.

**IDA labels $46 `shooting`.** That is the misleading one: it suggests the same thing
as Finishing at $4B. It is Velocity. Flagged again in [STATE.md](STATE.md) §5 because
it is the easiest error to inherit.

---

## 2. What each attribute does

| Attribute | Offset | Consumers | Effect at 0 vs 7 |
|---|---|---|---|
| **Passing** | $45 | Pass power bonus; CPU pass-accuracy roll | +0 → +384 speed; 37.5 % → 0 % misplaced |
| **Velocity** | $46 | Long-shot speed bonus | 1824 → 2592 (+42 %) |
| **Heading** | $47 | Jumping-header speed bonus | −336 → 0 (handicap only) |
| **Tackling** | $48 | Tackle contest (averaged with Control); recovery time | Recovery 30 → 9 frames |
| **Ball Control** | $49 | Tackle contest (averaged with Tackling); dribble touch size; touches before loss | 4 → 21 touches |
| **Speed** | $4A | Running speed | 928 → 1250 (+35 %) |
| **Finishing** | $4B | Close-range shot bonus; goal-vs-save roll | 1920 → 2816; 6 % → 94 % goal odds |
| Goalkeeper | $4C | Goal-vs-save roll | 94 % → 6 % goal odds |

Reading the right-hand column, the ranking of attributes by in-match consequence is
not subtle:

1. **Finishing** appears twice and dominates both. It sets close-range shot power
   *and* is the striker's half of the goal/save roll, where each point is worth 6.25
   percentage points of goal probability.
2. **Ball Control** appears three times and its touch-count effect has an
   accelerating curve — Control 7 changes direction more than five times as often as
   Control 0 before losing the ball.
3. **Passing** is two-sided and saturating: it adds up to +384 to pass speed for
   everyone, and for CPU sides it also drives the accuracy roll, which reaches zero
   failure rate at Passing 5. See [PASSING.md](PASSING.md) §3–§4.
4. **Tackling** is two-sided: it feeds the contest and it sets the cost of failure.
5. **Speed** is a flat 35 % band and is the *least* differentiating of the four core
   physical ratings.
6. **Velocity** matters only outside the box.
7. **Heading** is a pure handicap ramp with no upside.

> **Correction.** The first pass of this document reported no in-match read site for
> Passing and floated it as a career-only attribute. That was wrong: `DoPass` reads
> `PlayerGame` +$45 twice — at asm:34881 for the accuracy threshold and at asm:34980
> for the power bonus. The routines checked in the first sweep (`PlayerKickingBall`,
> `UpdatePlayerSpeed`, `CalculateIfPlayerWinsBall`, the header paths) genuinely do not
> read it; `DoPass` was the one that does, and it was read for its targeting logic
> rather than its attribute use. Recorded in [../AMIGA_CHANGES.md](../AMIGA_CHANGES.md).

---

## 3. `AdjustPlayerSkills` and the goalkeeper rating

The routine (asm:102057) is the bridge from stored team data into the per-match
`PlayerGame` record. Besides unpacking the seven skills it does two other things.

### A value-scaled adjustment

Before unpacking (asm:102080–102095):

```
d3 = teamRecord[$6C]                          ; player value, 0 if unset → forced to 1
d4 = (d4 × 100) / d3
if a competition flag is set:  d4 = 100
... conditional -12 with a floor of 0 ...
```

`d4` is then passed to `sub_126CA4` and thence into `sub_126CEE`, the per-nibble
transform applied to every skill during unpacking. So the stored nibbles are **not**
the final ratings: they are modulated by a factor derived from the player's value
relative to something, with a −12 penalty under one condition and a flat 100 override
under another. This is the form / morale / condition system, and its inputs come from
outside the match module.

**Consequence for us:** the numbers in a team file are not directly the numbers the
engine uses. Any tuning done against extracted team data must go through this
transform or it will be systematically off.

### The goalkeeper rating is derived, not stored

Only for players whose position field (`teamRecord[$66]`, top three bits) is 0 —
goalkeepers (asm:102125):

```
d0 = teamRecord[$6C]                          ; player value
d0 = (d0 + 3) / 7
d1 = one bit of a global, +1                  ; 1 or 2
d0 += d1
... two competition-context adjustments, each ±1 ...
clamp d0 to 0 … 7
PlayerGame[$4C] = d0
```

A keeper's shot-stopping is computed from **his transfer value**, divided by seven,
plus one or two. Everyone else gets $4C cleared to zero (asm:102119).

This is a genuinely surprising design and it is unambiguous in the code. It means a
keeper has no stored goalkeeping rating at all — value *is* the rating. It also means
the two ±1 adjustments, driven by competition context, shift keeper quality by up to
two points depending on the fixture, which is a substantial swing on a scale of eight
(each point is 6.25 percentage points of goal probability,
[GOALKEEPER.md](GOALKEEPER.md) §4).

---

## 4. Injuries

`PlayerGame.injuriesBitfield` ($4D) carries the injury level in its top three bits.
`UpdatePlayerSpeed` (asm:35434) reads it, shifts down by four, and indexes
`injuriesSpeedPenalty`:

| Level | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 |
|---|---|---|---|---|---|---|---|---|
| Speed penalty | 0 | −96 | −128 | −160 | −192 | −224 | −256 | −288 |

At level 7 the penalty exceeds the entire Speed-attribute range of 322, so a fully
injured fast player is slower than an uninjured slow one.

The penalty is applied **only when the side is human-controlled** — the test at
asm:35434 requires `TeamGeneralInfo.playerNumber` or `plCoachNum` to be set. A CPU
side's injured players run at full speed. Whether that is intentional is not
answerable from the binary; it is recorded as an asymmetry.

`Sprite.injuryLevel` ($68) is the in-match counterpart, and `PlayerGame` $4E is the
half-played marker promoted 0 → 1 → 2 by the clock routines
([TIMING.md](TIMING.md) §3).

---

## 5. Constants quick reference

| Symbol | Line | Value | Meaning |
|---|---|---|---|
| Skill block | 102106 | `PlayerGame` $45 … $4B | Seven bytes |
| Packing mask | 102094 | `$07777777` | Seven nibbles |
| Unpack order | 102113 | MSN first | $45 gets bits 24–27 |
| Skill clamp | 102117 | ≤ 7 | |
| Scale | — | 0 … 7 | Every attribute table has 8 entries |
| Goalkeeper rating source | 102130 | `(value + 3) / 7` | Plus 1–2, ±1, clamped 0–7 |
| Non-keeper $4C | 102119 | 0 | Cleared |
| Value scaling | 102083 | `× 100 / value` | Feeds the per-skill transform |
| Value scaling penalty | 102101 | −12, floor 0 | Conditional |
| `injuriesSpeedPenalty` | 35536 | 0 … −288 | Injury level 0–7 |

---

## 6. What this resolves, and what still needs measurement

Confirmed:

- ✓ Seven attributes, packed as nibbles in one longword, unpacked MSN-first.
- ✓ The scale is 0–7 for all eight ratings, confirmed structurally and by an explicit
  clamp.
- ✓ The skill block occupies `PlayerGame` $45 … $4C.
- ✓ The order is Passing, Velocity, Heading, Tackling, Control, Speed, Finishing —
  SWOS's own P V H T C S F.
- ✓ IDA's `shooting` label at $46 is Velocity, not Finishing.
- ✓ Velocity governs long shots, Finishing governs close-range shots and the
  goal/save roll.
- ✓ Ball Control is the most-consumed attribute; Finishing is the most consequential.
- ✓ Passing is read in-match twice, in `DoPass` only, and saturates at 5.
- ✓ The goalkeeper rating is derived from transfer value and is not stored.
- ✓ Stored nibbles are modulated by a value-derived transform before use.
- ✓ Injury penalties apply only to human-controlled sides.

Open (measurement targets, [../LEGACY.md](../LEGACY.md) §15):

- Whether Passing has any consumer beyond `DoPass` — the two sites there are
  confirmed, but a fully exhaustive sweep of the 3 400-line player pipeline has not
  been done for any attribute.
- The exact behaviour of `sub_126CEE`, the per-nibble transform. It is the difference
  between a team file's numbers and the engine's numbers and it is currently a black
  box.
- What `d4` is scaled against in `AdjustPlayerSkills` — the `× 100 / value` divides by
  the player's own value, so the numerator is the interesting term and it arrives in
  a register from the caller.
- The two competition-context adjustments to the goalkeeper rating (asm:102145,
  asm:102155): they compare bytes at `(a2)+4` and `(a3)+4` against 1 and 2, which are
  presumably home/away or leg indicators.
- `PlayerGame` $44 — one byte immediately before the skill block, never written by
  the unpack and never read by the match engine.

---

## 7. Guidance for the reimplementation

- **Name our fields after the roles, not after the labels.** `velocity`, not
  `shooting`. The mislabelling documented here is exactly the kind of thing that
  propagates silently into a clone and then into its tuning.
- **Keep the 0–7 scale internally** and offset only for display. Every table in the
  engine is eight entries wide.
- **Model the value→skill transform explicitly**, even as a stub, rather than reading
  team-file nibbles as final ratings. Otherwise every fit we do against extracted data
  will carry a systematic bias we cannot see.
- **Derive the goalkeeper rating from value.** It looks wrong and it is what the game
  does. If we later decide to give keepers a stored rating that is a deliberate
  departure and belongs in [../implementation/PLAN.md](../implementation/PLAN.md)
  alongside the other career-layer decisions — not a silent fix.
- **Treat Passing as a real in-match attribute.** It sets pass power for both sides
  and CPU pass accuracy, and it saturates at 5 — which is a tuning decision we should
  make consciously rather than inherit by accident.
- **Decide deliberately about the injury asymmetry.** Applying the penalty to both
  sides is more defensible and is a one-line change; leaving it asymmetric is more
  faithful. Either is fine, but it should be a recorded choice.
