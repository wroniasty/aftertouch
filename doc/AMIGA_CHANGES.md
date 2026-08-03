# AMIGA_CHANGES.md

What the Amiga oracle changed, and where.

The documents in [amiga/](amiga/) were traced through a 68000 disassembly of the
**Amiga SWOS 96/97 match module** — the build the DOS port was ported *from*. This
file is the ledger of what happened when every applicable document in [./](./) was
cross-checked against them: what got a value, what got corrected, what is now in
dispute, and what work that opens.

**If you are about to implement something, read the relevant "Amiga cross-check"
section in its own document.** This file is the index and the summary; the
derivations live there.

---

## 0. One-paragraph version

The DOS port's physics and tuning numbers live in an opaque data segment; in the
Amiga disassembly the same numbers are **named literals**. So the headline result is
that most of [LEGACY.md](LEGACY.md) §15's unmeasured constants now have a candidate
value — gravity, both friction constants, all three pitch tables, both player speed
tables, the full aftertouch curl and decay tables, the launch speeds, both shot bonus
tables, the contest odds, the dribble tables, the injury ramp. Where both oracles
carry a number they agree, table entry for table entry, which is strong mutual
confirmation and means `setAmigaModeEnabled()` in the port really does restore the
original's values. Three corrections came out of it, one of them consequential: **the
attribute range is 0–7, not 0–15**, which un-does a conclusion that had propagated
into five documents and had four engine tables marked as buggy when they are
correctly sized. Six genuine disagreements between the two readings are now on
record — two of them, the foul-from-behind test and the aftertouch side latch, come
out as exact complements and each inverts a mechanic players feel directly. And one
whole stage of shot resolution turns out to be missing from our reading of the
keeper.

---

## 1. Files changed

Every file below gained a cross-check section (usually the last numbered section)
and had its open-questions list reconciled.

| Document | Cross-check | What it mainly did |
|---|---|---|
| [BALL.md](BALL.md) | §12 | Confirmed every physics constant as a named literal; added the goalmouth scatter; opened the crossbar disagreement |
| [MOVEMENT.md](MOVEMENT.md) | §13 | Confirmed both speed tables and the movement kernel; opened four numeric divergences and the alternation question |
| [CONTROL.md](CONTROL.md) | §8 | Supplied both data tables it could only name; added the touch-count dribble-loss mechanic it was missing entirely |
| [SHOOTING.md](SHOOTING.md) | §9 | All five tables valued; shot-on-goal geometry made exact; hold-duration question answered (no for kicks, yes for passes) |
| [AFTERTOUCH.md](AFTERTOUCH.md) | §11 | All seven tables valued; three disagreements, two of which look like transcription errors here |
| [TACKLING.md](TACKLING.md) | §12 | Tables confirmed; attribute-bounds worry dissolved; the from-behind test and the flat-3 table both disputed |
| [HEADING.md](HEADING.md) | §10 | **§6's table corrected from 13 entries to 8**; jump duration and attribute range resolved |
| [SETPIECES.md](SETPIECES.md) | §12 | State enum completed; placement coordinates and six turn masks added; offside confirmed absent |
| [AI.md](AI.md) | §10 | Tactic record confirmed from both sides; answered the Amiga's biggest unknown; **found a missing shot-resolution stage** |
| [STATE.md](STATE.md) | §11 | Third vote confirming every `Sprite` offset; settled §4's dispute; refuted §5's attribute-range claim |
| [SIMULATION.md](SIMULATION.md) | §14 | Clock arithmetic confirmed; **wall-clock coupling shown to be a port artefact**; packed-decimal minutes; stoppage machine detail |
| [LEGACY.md](LEGACY.md) | §9, §15, §16 | §15 checklist reconciled — most items struck with a candidate value; §9 attribute effects filled in |
| [DATA.md](DATA.md) | §3 | **Corrected**: storage is 4 bits, used range is 0–7; the "four undersized tables" finding withdrawn |
| [PITCH.md](PITCH.md) | §1, §8 | Supplies the surface *names* the Amiga lacks; near-answer on "does the surface affect anything but the ball" |
| [REFEREE.md](REFEREE.md) | §8 | Three gameplay-side findings that bear on the referee, including the whistle-suppression flag |
| [INPUT.md](INPUT.md) | §4 | Flagged the one-team-per-frame claim as single-sourced and unconfirmed |
| [EXTRACTION.md](EXTRACTION.md) | §4, §6 | Recorded the Amiga wave; closed the goalkeeper audit; opened seven new debts |
| [README.md](README.md) | throughout | Index row, conventions corrected, known debts rewritten |

Not changed, and deliberately: [CAMERA.md](CAMERA.md), [BENCH.md](BENCH.md),
[PLAYER_SPRITES.md](PLAYER_SPRITES.md), [RENDERING.md](RENDERING.md),
[AUDIO.md](AUDIO.md), [MENUS.md](MENUS.md), [PLAN.md](PLAN.md). The Amiga set does
not cover presentation, and nothing in it bears on those subsystems.

---

## 2. Corrections — things we had wrong

Three, in descending order of consequence.

### 2.1 The attribute range is 0–7

**Was:** [DATA.md](DATA.md) §3 read the file format's 4-bit nibbles as giving
attributes a 0–15 range, and concluded that four attribute-indexed tables in the
engine are undersized and read out of bounds. This propagated into
[STATE.md](STATE.md) §5, [TACKLING.md](TACKLING.md) §10, [HEADING.md](HEADING.md) §8
and [README.md](README.md)'s conventions list, and generated an instruction to give
every attribute table an explicit bounds policy.

**Is:** `AdjustPlayerSkills` (asm:102092) masks the packed longword with
**`$07777777`** — three bits per nibble — then unpacks seven bytes and clamps each to
7. The high bit is discarded on load, which is exactly why a stored 8 reads as 0, the
long-standing community observation in [LEGACY.md](LEGACY.md) §9. Every
attribute-indexed table has eight entries because eight is right.

**Where:** [amiga/PLAYERS.md](amiga/PLAYERS.md) §1; corrected in
[DATA.md](DATA.md) §3, [STATE.md](STATE.md) §5, [TACKLING.md](TACKLING.md) §10,
[HEADING.md](HEADING.md) §10, [LEGACY.md](LEGACY.md) §9, [README.md](README.md).

### 2.2 The Heading table has eight entries, not thirteen

**Was:** [HEADING.md](HEADING.md) §6 recorded `kPlayerHeaderSpeedIncrease` as
`−336, −288, −240, −192, −144, −96, −48, 0, 513, 1027, 1541, 2055, 2569` and drew two
conclusions: that Heading is "the strongest attribute effect found anywhere in the
reference", and that a table with five meaningful entries above index 7 cannot belong
to a 0–7 attribute. The second was the main evidence for 2.1.

**Is:** the Amiga's `playerStrongHeaderSpeedIncrease` (asm:34852) is those first
**eight** values and stops. The five that follow in the DOS listing belong to the next
data item — their near-constant stride of ~514 is characteristic of an offset block,
not a tuning curve. Heading is a **pure handicap ramp with no upside**: 7 gets
nothing, everything below is a penalty, and the whole range is a 13 % cut on a jump
header's launch speed. It is the *weakest* attribute effect in the game, not the
strongest.

**Where:** [HEADING.md](HEADING.md) §10, with §6 left in place under a warning so the
error stays traceable.

### 2.3 The near miss has a gameplay effect

**Was:** [SIMULATION.md](SIMULATION.md) §5.3 recorded the near-miss test — ball
crossing the byline with speed ≥ 768, x ∈ [290, 381], z + 2 ≤ 25 — as "no gameplay
effect, just commentary".

**Is:** the identical test on the Amiga (speed ≥ $300, X 290 … 381, Z < 25) also
**clears the referee-whistle flag**, so a shot that comes back off the frame and out
is not whistled as an ordinary out-of-play. Three identical constants confirm the
reading; the fourth consequence was simply not noticed.

**Where:** [amiga/SETPIECES.md](amiga/SETPIECES.md) §1;
[SIMULATION.md](SIMULATION.md) §14, [SETPIECES.md](SETPIECES.md) §12,
[REFEREE.md](REFEREE.md) §8.

---

## 3. Disagreements — open, and each one matters

Recorded rather than arbitrated. Where the two oracles differ, that is a finding
about which game we are cloning. Each of these is cheap to settle with one targeted
trace, and each changes behaviour a player would notice.

| # | What | Reading A (this directory) | Reading B (Amiga) | Why it matters |
|---|---|---|---|---|
| 1 | **Foul from behind** | Foul when the two players' octants differ by **≤ 1** | Foul when they differ by **> 1** | Exact complements. Inverts the refereeing of every challenge. [TACKLING.md](TACKLING.md) §12 |
| 2 | **Aftertouch side latch** | `diff = (joystick − kick) & 7` | `diff = (kick − joystick) & 7` | Same guard, opposite curl side in every non-trivial case. One line, total effect on feel. [AFTERTOUCH.md](AFTERTOUCH.md) §11 |
| 3 | **The crossbar** | Bar and post converge on `speed >> 2` with `deltaZ` negated | Three distinct treatments; the bar does not reflect at all — speed is **set** to a flat 512 and the aim point pushed 1000 out of goal | Set-versus-scaled is behavioural, not rounding. It is the flat bounce-out everyone remembers. [BALL.md](BALL.md) §12 |
| 4 | **The flat-3 recovery table** | `kComputerTacklingDownTime` — a CPU-versus-human fairness asymmetry | `unk_1106C2` — the **deflecting tackle's** recovery, for anybody | Different balance entirely. The two readings may in fact fit together; see below. [TACKLING.md](TACKLING.md) §12 |
| 5 | **Pass loft** | `longPass`/`longSpinPass` gate a lob on the pass path | Passes get **no `deltaZ` change at all**, only a one-shot `+1/8` on speed | Whether a pass can be lofted by aftertouch. [AFTERTOUCH.md](AFTERTOUCH.md) §11 |
| 6 | **`TeamGeneralInfo` +44 / +56** | AFTERTOUCH §2: +44 is the launch direction, +56 the live joystick | +44 is the live joystick, +56 the kick direction | Two of our three readings agree with B — [MOVEMENT.md](MOVEMENT.md) §3.1 does too. AFTERTOUCH §2 is probably swapped. [STATE.md](STATE.md) §11 |

**On (4), a hypothesis worth testing.** The Amiga cannot say what marks a tackle as a
deflection (`Sprite` +$6A = −1) and flags it as important and player-facing.
[TACKLING.md](TACKLING.md) §3 has a mechanism that produces exactly a `−1` sentinel
and selects exactly the flat-3 table: **releasing fire early**. If they are the same
thing, then early release *is* the deflecting tackle — you commit to disturbing the
ball rather than winning it, forfeit the skill contest, and get up almost
immediately. That would make it a designed risk/reward choice rather than the exploit
§6 suspects, and it would close an open question on both sides at once.

---

## 4. Gaps — things one oracle has and the other does not

### 4.1 Missing from our reading of the port

- **The goal-versus-save resolution stage.** Before the keeper decides whether to
  dive, the Amiga runs a roll: `Finishing − goalieSkill + 7` indexes a sixteen-entry
  chance table, compared against frame-counter bits. Exactly 50/50 when level, 6.25 %
  to 93.75 % across the range, and it consumes **no RNG**. Nothing in
  [AI.md](AI.md) §4 corresponds to it, and §4.6's catch-versus-parry roll is a
  different decision at a different point. **Largest single gap opened by this
  pass.** [AI.md](AI.md) §10.
- **The dribble touch-count.** A player loses the ball after a Control-derived number
  of direction changes — 4 at Control 0, 21 at Control 7, with an accelerating curve.
  [CONTROL.md](CONTROL.md) §4 described losing the ball only through a tackle. This
  is what makes Ball Control the most consequential attribute in the game.
  [CONTROL.md](CONTROL.md) §8.
- **The goalmouth scatter.** A deterministic lateral jitter on goal-area rebounds,
  derived from the frame counter (`((stoppageTimer & 31) << 4) − 256`). Unmentioned
  anywhere here. [BALL.md](BALL.md) §12.
- **Restart placement coordinates and four corner / two throw-in turn masks.**
  [SETPIECES.md](SETPIECES.md) §12.
- **Celebration length consumes two `Rand` calls per goal**, decided inside the
  simulation from the score context. Skipping them desynchronises every roll after
  the first goal. [SETPIECES.md](SETPIECES.md) §12.
- **The player sweep sees partial state.** Players update in index order and each
  sees the already-updated positions of those before it. Real, asymmetric, and easy
  to diverge on. [SIMULATION.md](SIMULATION.md) §14.
- **Minutes are packed decimal** — four digits in a longword, which is why the
  comparison points look like `$405` and `$900`. [SIMULATION.md](SIMULATION.md) §14.

### 4.2 Ours to feed back into [amiga/](amiga/)

The flow goes both ways. Four places where our reading is ahead:

- **The keeper's dive-rate index table.** [amiga/GOALKEEPER.md](amiga/GOALKEEPER.md)
  §3 calls this "the single highest-value unknown in this document" — a 16-entry
  per-side table selecting one of eight dive rates, with nothing in the match module
  writing it. [AI.md](AI.md) §4.1 has it: words 5–20 of the `goalieSkill` row of the
  shot-chance table. It is not a difficulty knob; it is the keeper's skill, expressed
  as anticipation.
- **The header helper routines.** The Amiga lists "what `sub_11280A` and
  `sub_112822` do to `deltaZ`" as a measurement target;
  [HEADING.md](HEADING.md) §4 has both — `0x20000` at ×0.75 and `0x24000` at ×0.9375.
- **Free-kick zone selection.** The Amiga guesses the seven free-kick states are one
  per octant of the foul's facing; [SETPIECES.md](SETPIECES.md) §4 has the real
  rule — two Y bands and six X slices, mirrored by the offending team.
- **The three planar proximity thresholds.** The Amiga could not isolate them;
  [MOVEMENT.md](MOVEMENT.md) §6 has `≤ 32 / 72 / 2450` squared.

---

## 5. What is now settled

Values recovered and, where both oracles carry them, confirmed entry for entry. This
is the practical output of the pass.

**Ball** — gravity 4608 (0.0703 px/frame², 176 px/s²); ground friction 16; air
friction 10; all seven entries of all three pitch tables; bounce cut-off `$A000`;
barrier `[53, 618] × [100, 799]` with speed halved on breach.

**Movement** — both speed tables (928…1250 live, 1136…1248 stopped); the injury ramp
0…−288; `±1000` octant destinations; tackle slide 1792; jump header 2048; substitute
1536; player friction 96; the animation-cadence formula. Speed unit ≈ 1/512 px/frame,
so 512 ≈ 1 px/frame at 50 Hz.

**Kicking** — flat launch 2208 / `$14000` for everyone; Velocity bonus −384…+384;
Finishing bonus −288…+608; the shot-on-goal geometry exactly (Y gates 342/556,
Finishing box X 241…431 × Y < 204 or ≥ 694); pass cone ±22.5°; pass launch ramp
`$600…$8AA` by hold duration.

**Aftertouch** — window 10 ticks; decay ramp `5,4,3,2,2,2,2,1,1,1` summing to 23 with
over half the curl in three ticks; the full 32-entry curl table (magnitudes 0/23/32),
half-strength for passes; lob `$20000`/2688 and drive `$16000`/2560 at tick 4 exactly;
the per-axis speed correction ×3/4 and ×7/8.

**Contests** — contest odds 16…23 of 32 (exactly 50/50 when level, 71.9 % maximum);
recovery 30…9 by Tackling; dribble impulse 130…32 by Control, **inverted**, on 2
frames in 4; touches-before-loss 4…21; foul radius 32 squared; penalty area
X 193…478 × Y ≤ 216 / ≥ 682.

**Keeper** — positioning as a two-axis linear interpolation; reflex window 10 px;
save distance 24; penalty reaches 20 and 12 on a 3:1 split; dive rates 3.0…6.5
px/frame; `goalieSkill = clamp((value + 3)/7 + b, 0, 7)`.

**Timing** — 50 Hz, fixed step, one `UpdateTime` per vertical blank; `timeDelta`
30/18/12/9 with reload 49; period boundaries 45/90/105/120; injury-time grace 50
frames; break 50 frames, 75 after a goal; CPU restart waits 350/600/750.

**Structure** — every `Sprite` offset confirmed by a third vote; the skill block
`P V H T C S F` at +27…+33 fixed from read sites; the tactic record reconstructed
identically from both sides (name[9], 10 × 35, unkTable[10], set-piece link at
`$171`, 370 bytes total); the RNG as a 256-byte table walk with period 65 536, and
the crucial negative that the two biggest rolls in the game bypass it entirely.

**Absences, confirmed twice** — there is **no offside rule**, there is **no wall
code**, and **off-screen result simulation is not in the match engine** at all.

---

## 6. Follow-up work this opens

Roughly in priority order.

1. **Settle the six disagreements in §3.** One targeted trace each. Items 1 and 2 —
   the foul test and the curl latch — affect feel most and are one line of code
   apiece.
2. **Find the goal-versus-save stage in the port** (§4.1), or establish that it is
   absent and the two builds resolve shots differently.
3. **Settle whether team decisions alternate one team per frame.** Single-sourced,
   doubles input latency, halves the player-selection rate, and nothing in the Amiga
   reading confirms it. [INPUT.md](INPUT.md) §4.
4. **Propagate the 0–7 attribute range into `doc/implementation/`.** Out of scope for
   this pass, but [A5-game-data.md](implementation/A5-game-data.md),
   [B1-state-layout.md](implementation/B1-state-layout.md) §88,
   [B7-contests.md](implementation/B7-contests.md) §108,
   [C1b-sandbox-mode.md](implementation/C1b-sandbox-mode.md) and
   [PLAN.md](implementation/PLAN.md) all specify **0–15**, and B7 explicitly says
   *"Heading attr table 13 entries, index `min(attr,12)`"* — which is the mis-read of
   §2.2 already written into an implementation spec. **This is the most likely place
   for the error to reach code.**
5. **Re-tag the aftertouch constants in code** from `[PROVISIONAL: LEGACY §15 …]` to
   a candidate-value tag, since every one of them now has a number.
6. **Feed §4.2 back into [amiga/](amiga/).**
7. **Check whether `Rand2` is original.** [SIMULATION.md](SIMULATION.md) §10's
   isolation of career result generation from the match RNG stream depends on it.
8. ~~**Resolve whether Passing is read in-match at all.**~~ **Resolved: it is.**
   `DoPass` reads `PlayerGame` +$45 twice — as a pass-power bonus of up to +384
   (asm:34980) and as the CPU pass-accuracy threshold (asm:34881). The earlier
   "no reader found" claim was a sweep that missed `DoPass`. See
   [amiga/PASSING.md](amiga/PASSING.md) §3–§4; [LEGACY.md](LEGACY.md) §9 can be
   updated.
9. **Document the passing system in the DOS-port set.** Nothing in
   [SHOOTING.md](SHOOTING.md), [CONTROL.md](CONTROL.md) or [AI.md](AI.md) covers pass
   target selection, pass strength, the receiver's off-target sidestep, or the
   control lock-out on the receiver. [amiga/PASSING.md](amiga/PASSING.md) is the
   Amiga reading; the port needs a matching pass and a `PASSING.md` of its own.
10. **Decide on the interception gap.** The Amiga has no test that cancels a pass when
    an opponent takes the ball — it relies on a ±90° heading rule and on the opponent
    kicking. The receiver keeps chasing and stays unselectable in the meantime. This
    is the single most-felt piece of SWOS clunkiness and it is a candidate for a
    deliberate, recorded departure. [amiga/PASSING.md](amiga/PASSING.md) §9, §12.

---

## 6a. Corrections made *to* the Amiga set

The Amiga documents are not exempt from this ledger. Two claims in their first pass
were wrong and have been corrected in place:

| Claim | Was | Is | Where |
|---|---|---|---|
| Pass strength | "selected by how long fire was held" | Banded by **distance to the receiver**, eight 50-px bands $600 … $8AA, plus a Passing bonus | [amiga/PASSING.md](amiga/PASSING.md) §3 |
| Passing attribute | "no in-match reader found" | Read twice, both in `DoPass` | [amiga/PLAYERS.md](amiga/PLAYERS.md) §2 |

Both came from the same root cause: `DoPass` was read for its *targeting* logic in
the first pass and its attribute use was not followed through. Worth noting as a
process lesson — a routine skimmed for one purpose should not be counted as swept.

---

## 7. The standing caveat

Every constant recovered here is a value read out of a disassembled binary. That
makes it a **measurement of the original**, not a specification we are obliged to
match, and two things follow.

A value being present does not make the surrounding interpretation right. The chain
"this word is 4608" → "therefore gravity is 0.0703 px/frame²" passes through
assumptions about the fixed-point format and the frame rate; each document states its
chain so the assumption can be attacked, and that is what a trace should attack.

And [LEGACY.md](LEGACY.md)'s rule stands unchanged: no numeric constant should be
pasted into the engine as fact. What changed is the *cost* of confirming one. The
trace corpus's job for most of §15 is now confirmation rather than search, which is a
large saving — but it is not permission to skip the confirming.
