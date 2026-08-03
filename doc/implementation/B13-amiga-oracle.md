# B13 — Amiga oracle incorporation

Reconciling the engine with [../AMIGA_CHANGES.md](../AMIGA_CHANGES.md): the two
corrections, the confirmed constants, the four mechanics we do not have, and the
six disagreements. It is a **values-and-details pass, not an architecture pass** —
§2.1 is the evidence for that claim, and it is the first thing to attack if this
plan is wrong.

Depends on: B6a, A5, A3   Blocks: the Wave-3 play-feel gate   Wave: 3

---

## 0. One-paragraph version

The Amiga disassembly confirmed the engine's architecture and corrected its
details. Every structural claim it touches — the `Sprite` offsets, the 50 Hz frame
order, the fixed-point conventions, the table-walk RNG, the sequential in-place
player sweep, the one-team-per-frame alternation — it *confirms*, and three of the
engine's own geometry constants turn out to match it character for character.
Nothing here justifies a rewrite. What changes is a set of constant values, one
table's length, the attribute range, and four mechanics we never implemented. The
real cost is not the code: **13 hash-pinned test files, two corpus chains and the
golden move every time state or the RNG stream moves**, so the work is batched by
blast radius into five commits with four re-pin cycles between them, rather than
twenty items each paying that toll separately.

---

## 1. Scope

**In:**

- R1–R5 below: doc propagation, the two corrections, value substitution, the four
  missing mechanics, and the disagreement A/B switches.
- Retiring the `[PROVISIONAL: LEGACY §15 …]` tags whose values the Amiga supplies,
  replacing them with a candidate-value tag that names the asm line.
- One `ATTR` version bump (v5 → v6) carrying the dribble touch counter.
- A re-pin discipline: one cycle per batch, each with a stated reason.

**Out:**

| Excluded | Owner |
|---|---|
| Arbitrating the six disagreements | R5 sets up the A/B; the **trace** decides — A3 item 4 |
| Confirming any candidate value against a real recording | Track M ([B6a](B6a-kick-fidelity.md) §6), now much smaller — see §2.3 |
| Passing system: target selection, receiver lock-out, interception | New part; [../AMIGA_CHANGES.md](../AMIGA_CHANGES.md) §6 items 9–10 |
| Feeding our four findings back into `doc/amiga/` | Documentation task, §6 item 6 of the ledger |
| Career-side attribute range beyond A5's projection | E1 |
| Presentation, camera, audio | The Amiga set does not cover them |

---

## 2. Design

### 2.1 Why this is not a rewrite

The ledger's structural claims were checked against the code, not assumed:

| Structural claim | Amiga verdict | Engine today |
|---|---|---|
| `Sprite` / `TeamGeneralInfo` offsets | "third vote confirming every offset" | [`match_state.hpp`](../../src/core/include/core/match_state.hpp) is built on them |
| Coordinate system, 16.16, 256-step heading | confirmed | A2, pinned with golden vectors |
| RNG = 256-byte table walk, period 65 536 | confirmed | `rng.hpp`, three streams |
| 50 Hz fixed step, one clock tick per VBL | confirmed | `match_clock.hpp`, reload 49 |
| Player sweep sees **partial state**, index order | listed as a gap we would miss | `movement.hpp` §`MovePlayers` already integrates in place, in index order |
| One team decides per frame | single-sourced, unconfirmed | `ApplyTeamControls` already alternates on `team_switch_counter & 1` |
| Ball gravity 4608 / ground 16 / air 10 | named literals | `profile.hpp` already carries all three |
| Player friction 96; speed tables 928…1250 / 1136…1248; injury ramp 0…−288 | confirmed | `movement.hpp` already carries all four, entry for entry |

The engine was built Amiga-first, with one named constant per reference table. That
is precisely the seam this pass plugs into. **If any of the first four rows had been
overturned, this document would be a rewrite plan instead.**

### 2.2 The batching principle — blast radius, not topic

Ordering by topic is the obvious mistake. Order by what a change *moves*:

| Class | Moves | Cost |
|---|---|---|
| Documentation, tags, comments | nothing | free — do first, in bulk |
| A constant read during play | the hash of any scenario that reaches it | one re-pin |
| Squad generation (`fictional.hpp`) | **every** kickoff state, so every hash | one re-pin, all of them |
| A new state field | `ATTR` version + every hash | one re-pin + format bump |
| An extra `Rand` call | the RNG stream, so **everything downstream of the first goal** | one re-pin, and it must land alone |

So: all free work first; then everything that moves squad data in one commit; then
values; then mechanics, with the RNG-consuming one last and by itself. Five commits,
four re-pin cycles.

### 2.3 What the Amiga settles for free

Three of the engine's geometry constants match the Amiga exactly:
`kPenBoxXMin/Max` = 193/478 and `kPenaltyBoxTopY/BotY` = 216/682, against the
ledger's *"penalty area X 193…478 × Y ≤ 216 / ≥ 682"*.

That closes [B6a](B6a-kick-fidelity.md) §6's last open item. B6a §5 (S5) recorded a
factor-of-two disagreement between SHOOTING §3's raw y thresholds (342/556) and the
engine's own area, kept `kRefShotBox*` as a fit target, and deferred the choice to a
coordinate-scale measurement. **The measurement has arrived and the engine was
right.** `kRefShotBoxMidTop` / `kRefShotBoxMidBot` become dead fit targets and are
deleted; `kRefShotBoxTopY` / `kRefShotBoxBotY` (204/694) survive as the
Finishing-box y gates, which the ledger confirms.

More generally, Track M shrinks from a **search** for values to a **confirmation** of
candidates. It stays open — §7 of the ledger is explicit that a candidate value is
not permission to skip the confirming — but it is no longer the same size of job,
and it stops being the reason B6 cannot return to *done*.

---

## 3. Interfaces

**`profile.hpp` stays the one switch.** Amiga-vs-PC values already live there and
new dual-valued constants join them. The disagreement switches (R5) go here too, as
named `constexpr bool`s defaulting to the *current* reading, so an A/B is a one-token
edit and a scripted trace run rather than a branch.

**The attribute range contract moves to 0–7.** The engine already clamps at 7 in
every consumer (`AttrIndex0to7`, and `tackling.hpp`'s "conscious clamp" comment,
which this pass makes honest rather than defensive). What changes is the *producer*
side: A5's fictional squads and the sandbox UI must stop generating 8–15. The clamps
stay — a defensive clamp that can no longer fire is still the right code — but their
comments change from "attrs 0–15, table has 8 entries" to a statement of the range.

**`ATTR` goes v5 → v6** for the dribble touch counter (R4). One bump for the batch,
not one per field.

**No new wall.** `check_walls.py` is unaffected: nothing here adds a float, a clock
read or an unseeded rand, and no career field becomes reachable from `src/core/`.

---

## 4. Work items

Ordered. Each is independently committable; the re-pin at the end of a batch is part
of that batch's commit, with the one-line reason [B6a](B6a-kick-fidelity.md) §3 rule
1 requires.

### R1 — Propagation and tags *(moves no hash)*

The ledger calls this "the most likely place for the error to reach code", and it is
right: [B7-contests.md](B7-contests.md) still specifies *"Heading attr table 13
entries, index `min(attr,12)`"*, which is the §2.2 mis-read already written into an
implementation spec — and already in the engine.

| Item | Where |
|---|---|
| 0–15 → 0–7 in the implementation specs | [A5-game-data.md](A5-game-data.md), [B1-state-layout.md](B1-state-layout.md) §88, [B7-contests.md](B7-contests.md) §108, [C1b-sandbox-mode.md](C1b-sandbox-mode.md), [PLAN.md](PLAN.md) §3 (A5's row) |
| Strike B7's 13-entry heading instruction | [B7-contests.md](B7-contests.md) |
| Retag the eight `[PROVISIONAL: LEGACY §15 …]` sites that now have a candidate | `shooting.hpp`, `aftertouch.hpp` |
| Add the B13 row | [PLAN.md](PLAN.md) §3, [PLAN-CURRENTSTATE.md](PLAN-CURRENTSTATE.md) |

**Test:** none. Nothing executes. `ctest` must be unchanged — if a hash moves in R1,
something was miscategorised.

### R2 — The two corrections and the attribute range *(one re-pin, the widest)*

| Item | Where | Note |
|---|---|---|
| `kPlayerHeaderSpeedIncrease` 13 → 8 entries | `heading.hpp` | Drop the trailing `513…2569`; they are the next data item. Heading becomes a pure handicap ramp with no upside |
| Remove the `min(attr,12)` index path | `heading.hpp` | `AttrIndex0to7` is now the only indexer |
| Squad generation stops exceeding 7 | `fictional.hpp` — `bump()` and the 16 `MakePlayer` rows | **This is what moves every hash.** Rescale the base values; do not just clamp, or every strong squad flattens to a wall of 7s |
| `kAttrMax` 15 → 7 | `sandbox_menu.cpp` | Plus the "Attributes (0-15)" label |
| Range comments | `game_data.hpp`, `tackling.hpp` | Statement of range, not apology for a clamp |
| Near miss clears the whistle flag | `referee.hpp` / `out_of_play.hpp` | Ledger §2.3: the test already exists, the fourth consequence was missed |

**Tests:** `test_heading_attr.cpp` (new) — the ramp is monotonic, index 7 is the
zero-penalty entry, the full range is a ~13 % cut and never a gain. Extend
`data_tests` to assert no generated attribute exceeds 7, and `test_sandbox_setup.cpp`
for the UI bound.

### R3 — Value substitution *(one re-pin)*

Drop-in on tables that already exist by name. Every one gets a candidate tag citing
its asm line.

| Constant | Now | Amiga |
|---|---|---|
| `kBallKickingSpeed` | 2800 | 2208 |
| `kBallKickingDeltaZRaw` | 70000 | `$14000` = 81920 |
| `kSpinMultiplierFactor` | `8,7,6,5,4,3,2,2,1,1` (**Σ 39**) | `5,4,3,2,2,2,2,1,1,1` (**Σ 23**) |
| `kKickSpinFactor` magnitudes | 12 / 8 | 32 / 23 |
| `kHigh` / `kNormalKick` pairs | 220000/2200, 40000/3000 | `$20000`/2688, `$16000`/2560 |
| `kKeeperSaveDistance` | 16 | 24 |
| Velocity / Finishing bonus tables | 0…280, 0…336 | −384…+384, −288…+608 |
| Pass launch ramp | flat `kBallPassingSpeed` | `$600…$8AA`, banded by **distance to receiver** |
| Contest odds, recovery, dribble impulse, foul radius | B7's fitted values | 16…23 of 32; 30…9; 130…32 inverted on 2 frames in 4; 32² |

**Flag loudly, do not land silently:** the decay ramp roughly halves total curl
(Σ 39 → Σ 23) and the bonus tables become *signed* — a low-Velocity player now
loses speed rather than merely gaining none. Both are felt immediately. They belong
against the play-feel gate, played, not just re-pinned.

**Tests:** by design, none should need editing.
[B6a](B6a-kick-fidelity.md) §3 rule 2 made the aftertouch suite assert properties
rather than table values precisely so that the day the tables are fitted is the day
they must not need editing. **If `test_aftertouch.cpp` needs changing in R3, that
rule failed and the failure is the more interesting finding.** Add signed-bonus
coverage to `test_shot_bonus.cpp`.

### R4 — The four missing mechanics *(one re-pin, plus one alone)*

Additive, each its own module and its own commit:

1. **Dribble touch-count** — a Control-derived limit on direction changes before the
   ball is lost: 4 at Control 0, 21 at Control 7, accelerating. New counter in
   `MatchState` → **`ATTR` v6**. This is the ledger's claim for why Ball Control is
   the most consequential attribute in the game, and [../CONTROL.md](../CONTROL.md)
   §4 had losing the ball happening only through a tackle. Test:
   `test_dribble_touches.cpp` — the count matches the table at both ends, a straight
   run never loses it, the loss is a release and not a foul.
2. **Goal-versus-save resolution** — `Finishing − goalieSkill + 7` into a 16-entry
   chance table, compared against frame-counter bits, **before** the dive decision.
   Consumes no RNG, which is why it can share a re-pin. Slots into
   `goalkeeper.hpp` ahead of the existing rest/claim/dive path; §4.6's catch-versus-
   parry roll is a different decision at a different point and stays. Test:
   `test_shot_resolution.cpp` — 50/50 when level, 6.25 % … 93.75 % across the range,
   and `HashState` unchanged by the roll itself.
3. **Goalmouth scatter** — `((stoppage_timer & 31) << 4) − 256` lateral jitter on
   goal-area rebounds. Deterministic, no RNG. Test: `test_goalmouth_scatter.cpp`.
4. **Restart placement coordinates + the six turn masks** — `set_pieces.hpp` already
   has the shape; this supplies the numbers. Test: extend `test_restart_cycle.cpp`.

Then, **alone and last**, because it moves the RNG stream and therefore everything
after the first goal:

5. **Celebration length consumes two `Rand` calls per goal**, decided inside the
   simulation from the score context. Skipping them desynchronises every roll
   downstream — which is exactly the class of divergence A3 exists to catch, and
   exactly the class that is invisible until a trace runs long enough to score.

### R5 — The six disagreements: switches, then traces

Do **not** pick a side in code. Each disputed reading gets a `constexpr bool` in
`profile.hpp` defaulting to the current behaviour:

| # | Switch | Flip changes |
|---|---|---|
| 1 | `kFoulFromBehindInverted` | `tackling.hpp` octant test `≤ 1` → `> 1` — exact complements |
| 2 | `kAftertouchLatchInverted` | `(joy − kick) & 7` → `(kick − joy) & 7`, at **three sites** in `aftertouch.hpp`, not one |
| 3 | `kCrossbarSetsSpeed` | `ball.hpp`: bar stops scaling `speed >> 2` and instead **sets** 512 with the aim point pushed 1000 out of goal |
| 4 | `kFlat3IsDeflection` | `tackling.hpp`: whose recovery the flat-3 table is |
| 5 | `kPassLoftEnabled` | `aftertouch.hpp`: whether a pass can be lofted at all, or only gets `+1/8` on speed |
| 6 | `kTeamInfoFieldsSwapped` | Documentation-only — two of our three readings already agree with the Amiga; AFTERTOUCH §2 is simply swapped and should be corrected in place |

Items 1 and 2 are exact complements, which makes them the cheapest A/B available:
one corpus scenario, run twice, differing in one token. **On (4), test the ledger's
hypothesis explicitly** — that early fire release *is* the deflecting tackle. If the
`−1` sentinel [../TACKLING.md](../TACKLING.md) §3 produces is the same `−1` the
Amiga cannot explain, one trace closes an open question on both sides at once.

**Test:** no new unit tests. This is corpus and `tracediff` work, and it is the one
batch that genuinely blocks on A3 item 4.

---

## 5. Tests and acceptance

**Technique** ([PLAN.md](PLAN.md) §7): golden traces and reference diffs answer
*did we change* versus *are we faithful*. R2–R4 are the first; R5 is the second and
cannot be faked with the former.

**Re-pin discipline.** Four cycles, each covering `tests/golden/kickoff.attr`, both
corpus chains under `tests/corpus/`, and the 13 hash-pinned files:

```
test_ai_b9  test_ball_trajectory  test_contest_sequence  test_curled_shot
test_determinism  test_dribble_turn  test_hash  test_move_players
test_performance_rating  test_restart_cycle  test_camera  test_result_sim
test_sandbox_setup
```

Each re-pin commit states why the hash moved, in one line, per
[B6a](B6a-kick-fidelity.md) §3 rule 1. A re-pin whose reason is "R3" and not
"the decay ramp halved total curl" is not a reason.

**New invariants for the always-on set:**

- No attribute anywhere in a projected `MatchState` exceeds 7.
- Every attribute-indexed table has exactly 8 entries. Worth a `static_assert` per
  table — it is free and it is the exact error this pass is correcting.
- `HashState` is unchanged by the goal/save roll (it consumes no RNG).

**Acceptance — the part closes when all four hold:**

1. `ctest` green with all four re-pins committed and reasoned.
2. No 0–15 attribute range survives in `src/`, `doc/implementation/` or the sandbox
   UI, and the heading table has 8 entries.
3. A dribble that changes direction past the Control-derived limit loses the ball,
   in a committed scenario.
4. All six §3 switches exist, default to the current reading, and each has a corpus
   scenario that *reaches* its mechanic — the [B6a](B6a-kick-fidelity.md) §5 trap:
   a corpus entry that never reaches its mechanic cannot report a divergence in it,
   and `shot_curl` recorded a walk for exactly that reason.

Note what acceptance deliberately does **not** include: that any disagreement is
settled. R5 delivers the instrument, not the verdict.

---

## 5a. What landing it actually did — and where the plan was wrong

R1–R5 are implemented; `ctest` is green on all seven suites. Four deviations from
§4 and six findings, recorded because a plan that is quietly edited to match what
happened is not a plan.

### Deviations

| § | Planned | Done | Why |
|---|---|---|---|
| R1 | Retag the eight `[PROVISIONAL]` sites | Folded into R3 | A tag reading `[CANDIDATE: asm:30730]` beside the value 2800 would have been false for as long as R1 and R3 were separate commits. Tags move with their values. |
| R2 | Near-miss whistle flag | Moved to R4 | It needs a `MatchGlobals` field, and R4 already owns the `ATTR` bump. Landing it in R2 would have cost a second format version for one byte. |
| R4 | New test files per mechanic | One file, `test_touch_count.cpp` | Four mechanics, ~15 cases; four files of five lines of fixture each is worse to read, not better. |
| R4 | Restart placement coordinates | Turn masks only | The masks were wrong and are fixed. The goal-kick X pair (396/276) and the symmetric throw-in state encoding are **not** done — see §6. |

### Findings that were not in the plan

1. **The corpus was already stale, and could not report it.** The committed
   `engine.attr`/`reference.attr` were `ATTR` **v4**; the engine has emitted v5
   since [B6a](B6a-kick-fidelity.md) split out `restart_shortfall`. The pair was
   never regenerated. `corpus_python` passed throughout because it checks a
   committed `.chain` against a committed `.attr` — both stale together, so it is
   structurally incapable of detecting staleness. **A regeneration step belongs in
   the format-bump checklist, and the corpus check should compare against a fresh
   engine run the way `test_golden_trace` does.**
2. **`attempt_latched` never reached the wire.** B12 added it to `MatchGlobals`
   and did not serialise it, so [B1](B1-state-layout.md)'s "full match state
   round-trips losslessly" was not quite true. Fixed alongside
   `whistle_suppressed`.
3. **The engine's penalty area is the Amiga's, exactly** — `kPenBoxXMin/Max` =
   193/478 and `kPenaltyBoxTopY/BotY` = 216/682. This settles B6a §6's
   coordinate-scale question in the engine's favour; `kRefShotBoxMidTop/MidBot`
   are retired as dead fit targets.
4. **The corner turn masks were wrong in a way nothing could see.** Both top
   corners shared one mask reused from the penalty arcs, so a top-left taker was
   permitted octants facing out of the pitch. There are four masks, one per flag.
5. **No pinned scenario scores a goal.** Adding the celebration's two `Rand`
   draws moved *no* committed hash — which is exactly the ledger's warning about
   post-goal desynchronisation, and it means the corpus has no coverage of
   anything downstream of a goal. Pinned directly instead, by asserting the draw
   count.
6. **The aftertouch "drive" no longer flattens a shot.** On the provisional pair
   the drive sat below the flat launch (40000 < 70000); on the Amiga's it sits
   above it (`$16000` > `$14000`). Both tick-4 outcomes lift the ball. There is no
   way to aftertouch a shot flatter than it left the boot.

### Re-pins, with reasons

| Cycle | Hashes moved | Reason |
|---|---|---|
| R2 | `test_contest_sequence` | Scenario used Heading 8 — illegal, and indexing the phantom 13-entry table for a +513 bonus |
| R3 | `test_ai_b9`, `test_contest_sequence`, `test_curled_shot` | Amiga launch/curl values; signed shot bonuses; keeper reach 16 → 24 |
| R4 | `test_ai_b9`, `test_dribble_turn`, + golden and both corpus pairs (`ATTR` v6) | Touch-count live; goal/save stage; new state field |
| R4b | `test_restart_cycle` | Four corner masks, plus the CPU horizontal-axis denial |

Net: **five hash pins** and the golden plus both corpus pairs, across four cycles.
`test_move_players`, `test_ball_trajectory`, `test_determinism`, `test_hash`,
`test_performance_rating`, `test_camera`, `test_result_sim` and
`test_sandbox_setup` never moved.

---

## 5b. R6 — the passing pass

Play-testing found four defects. All four are fixed; each traces to a specific
place where our reading and [../amiga/PASSING.md](../amiga/PASSING.md) differ, and
three of them were *invented* constraints with no counterpart in the original.

### "Passes just go where you're facing"

Three compounding defects in target selection, in descending order of blame:

| Was | Is |
|---|---|
| A maximum pass range of ~70–126 units (`PassTargetMaxDistSq`) | **No range limit.** The original filters on the cone and the candidate's state, nothing else |
| The cone anchored at the **passer** | Anchored at the **ball** — the two differ throughout a dribble, which is when passes are played |
| ±1 octant (±45°), snapped to octants | **±16/256 = ±22.5°**, tested on the raw 256-step angle |

The range cap did the damage. On a 510×641 pitch almost no team-mate qualified, so
nearly every pass fell through to the no-target clearance — which is a facing-
direction kick. The symptom was the fallback firing almost every time.

Also corrected: the strength table and the Passing bonus are now the sourced
values (`1536, 1664, 1792, 1877, 1962, 2048, 2133, 2218` against squared-distance
thresholds; bonus `0…384` in steps of 48/64), replacing R3's interpolations, and
a targetless pass is a flat 1792 clearance with no Passing bonus. §6's "our pass
range and the Amiga's banding disagree by 3×" is **resolved** — the range was ours
and it was wrong.

### The aim point was at the receiver, not through him

`calculate_pass_to_player_delta_x_y` doubles the ball→receiver vector **until the
aim point leaves the pitch's bounding box**. We aimed at the receiver's exact
position. The Amiga comment explains why that is wrong and it is worth repeating:
the ball chases its aim point every frame, so an aim point *at* the receiver makes
the heading unstable as the ball closes.

It also made aftertouch catastrophic. The curl nudges `dest` by up to 16 per tick
and ~368 over a window; on a 40-unit pass vector that swamps the direction
entirely and can reverse it. **That is the reported "aftertouch turns the ball
180°".** With the ray extended the same nudge is a few degrees.

### Nothing closed the aftertouch window on capture

The sharpest bug of the four, and the reason a tap could put the ball into the
sky. Capture clears `pass_in_progress` but left `spin_timer` running, so a pass
collected before the tick-4 vertical sample then took that sample's **shot**
branch — setting `deltaZ` to the lob pair at speed 2688 and launching a tapped
ball off the receiver's own foot. The still-open window also kept rewriting the
dest of a ball supposedly under control, which is the "it bounces off him" and
the residual "spin". Collecting the ball now ends the window.

### The post-kick lockout was applied to the whole side

`pass_kick_timer` exists to stop the **kicker** re-capturing his own kick. It was
checked against the side's shared `TeamControl`, and since a side is served every
other frame, 25 ticks is 50 real frames — so for a full second **no team-mate
could receive the pass either**. Scoped to the kicker. The arrival band is also
speed-dependent now, because a fixed 5.7-unit radius can be stepped clean over by
a ball moving ~8.7 units between two of that side's frames.

### An uncontrolled team-mate chasing a loose ball

`UpdatePlayerBeingPassedTo` fell back to "nearest player to the ball" when nobody
was in the cone, and assigned that man to `pass_to_slot` — permanently, during
open play, with no pass in flight. Two consumers treat `pass_to_slot` as a
*committed receiver*: `ApplyOffBallDestination` freezes him, and the selection
routines exclude him. So the man nearest a loose ball was frozen *and* made
unselectable, and control went to the second-nearest, who set off after it.

There is no such fallback in the original: no cone candidate means no receiver.
Removed, and both consumers now require `pass_in_progress`. The receiver of a real
pass additionally steps into the ball's path when it is off target
([../amiga/PASSING.md](../amiga/PASSING.md) §5), which we had never implemented —
he only ever stood still.

### The kicker exclusion was permanent

Found by reading `traces/match_C1B00001.txt`. `passing_kicking_slot` is written
on every kick and was **never cleared**, while three sites use it as a hard
exclusion: the kicker cannot be selected (`UpdateControlledPlayer`,
`RefineControlledSelection`) and cannot be passed to
(`UpdatePlayerBeingPassedTo`). So the last man to touch the ball was permanently
unselectable — for the rest of the match, unless a team-mate happened to kick.

That is the "nobody closes on the ball" report. At t=196 of that trace the ball
is at (382,375) with slot 10 three units away and slot 8 a hundred and twenty
away; slot 8 got control, because slot 10 had just kicked. It now expires with
the kick lockout, which is the only window its purpose justifies.

### The kick telemetry never reached a played trace

[B6a](B6a-kick-fidelity.md) §4 added a `kick:` transcript line so that a
"shooting feels off" report arrives as a tick count rather than an adjective. It
went into **tracekit's** transcript writer — the one `tracegen` uses. The app's
live MATCH / SANDBOX capture is a separate writer (`MatchRecorder` in
`main.cpp`) which had no probe and emitted no such line. Every trace actually
played and reported therefore carried no telemetry, and the C1B trace could not
answer whether a given strike was a tap or a hold.

Both writers now share `FormatKickLine` / `FormatCurlLine`, so they cannot drift
apart again, and the line carries what the question needs:

```
kick: side=1 shot dir=N press->strike=11 hold=12 target=none aim=(336,-461)
curl: side=1 latch=cw@2 vert=flat
```

`target` and `aim` are new. A transcript that says a pass happened but not where
it was aimed cannot answer "the pass direction is off". The curl line is separate
because the latch and vertical decision are not final until the window closes —
printed on the strike tick they would read "none" every time.

**Re-pins:** eight, all in one cycle — `test_ai_b9`, `test_ball_trajectory`,
`test_contest_sequence`, `test_curled_shot`, `test_determinism`,
`test_dribble_turn`, `test_restart_cycle`, plus the golden and both corpus pairs.

---

## 5c. R7 — the goalkeeper

Reported as "the keeper moves towards the ball across the whole pitch" and
"re-appears in weird places when the ball goes out". Two defects, both confirmed
against [../amiga/GOALKEEPER.md](../amiga/GOALKEEPER.md) §1.

### There were two keeper positioning rules, and the wrong one ran

`OffBallDestination` (ordinal 1) held a near-correct copy of the Amiga map. But
the keeper is served by `ApplyGoalkeeperAI`, not `ApplyOffBallDestination`, and
that function implemented something else entirely:

| | Was (the rule that ran) | Amiga |
|---|---|---|
| `dest_x` | `336 + (ballX−336)/2` — a **254px** arc | `285 + (ballX−81)×103/510` — a **103px** arc |
| `dest_y` | midpoint between ball and own goal line | `base + (ballY−129)×27/641` — a **27px** band |

The `dest_y` rule is the headline. With the ball on the halfway line the keeper's
destination was 160 units off his line, outside his own box; with the ball at the
far byline he was sent to the centre circle. That is the whole of the report.

The Amiga's is *"four lines of arithmetic and no logic"* — no angle narrowing, no
sweeping. Now one `KeeperRestDestination`, called by both sites, so the two
cannot diverge again. The old `OffBallDestination` copy also used a span of 26
rather than 27, which is how you can tell it had been transcribed twice.

### A stranded claim latched the keeper forever

`GoalieClaimed` / `GoalieCatchingBall` were cleared in exactly one place —
`ApplyRestartTake`, and only for the player *taking* the restart. A keeper who
claimed and then did not take it (ball out at the far end, a team-mate restarts,
a sandbox reset) stayed latched, and `ApplyGoalkeeperAI`'s early return then
froze him wherever he stood for the rest of the match. Open play with the ball no
longer his now clears it.

The dead-ball branch also used to freeze him in place; he now walks back to his
post, so an out-of-play while he was upfield no longer strands him.

**Tests:** `test_keeper_position.cpp` — the arc and band hold for every ball
position on the pitch and both ends, he stays inside his own penalty area, he
comes off his line as the ball retreats, both callers agree, and a stranded claim
clears. Properties, not coordinates.

**Re-pins:** eight, plus golden and both corpus pairs. One behavioural test moved:
`test_sandbox_setup` asserted `pos.y > kPenaltyBoxTopY` as "not in the far box",
which only reads that way while side 1 defends the bottom — the same
fixture-dependence already fixed in its sibling assertion. Ends swap at half time.

---

## 5d. R8 — the off-ball shape had a dead zone over half the pitch

From `traces/match_C1A00001_01.txt`: both teams' off-ball players tracked the
ball in **x** and never moved in **y**. At t=408 the ball is at y=695 and the
away side's outfielders sit at y=349…549; 240 ticks later the ball is at 681 and
they are in the same places, with only their x having shifted.

The cause is in the tactics data, not the movement code. `MakeTactic` pushed the
shape with `if (row > 3) y += (row - 3)`, so the four ball rows from the far
byline to just past halfway produced an **identical** formation:

```
away role0 cells, column 2, ball rows 0…6:
    22 22 22 22 23 24 25
    ^^^^^^^^^^^ four rows, one shape
```

Columns had no such dead zone — `x` uses `(col - 2) * 2` across the full range —
which is exactly the asymmetry the trace shows. Now `y += (row - 3)` across the
whole grid, bounded to 1…14 so nobody is placed on his own goal line. The
residual flattening of the deepest defenders at the extreme rows is a property
of this placeholder formation data, not of the movement code.

### Why no test caught it

**Not one hash-pinned scenario uses the real tactics data.** Every scripted core
scenario hand-builds its grid as `((r % 15) << 4) | (q % 16)`, a synthetic
pattern that varies by row and therefore cannot express this bug; neither
`tracegen` nor `tracekit` touches `MakeFictionalLeague` at all. The only core
test that loads the fictional league is `test_kickoff_halves`, which does not
hash. So the formation data the game actually runs on had **zero** pinned
coverage.

This fix moved no hash for exactly that reason — the same shape of gap as "no
pinned scenario scores a goal" (§5a finding 5). Both are now covered by
assertions instead: `test_game_data.cpp` pins that the grid is monotonic in the
ball row, spans at least four cells, has no four consecutive rows sharing a
depth, and never places a player on his own goal line.

---

## 5e. R8 — goal kicks

From `traces/match_C1A00001_01.txt`, ticks 2043-2189: the ball crosses the bottom
byline at (439,770), and the restart sits there for **145 ticks** with an
outfielder (slot 3) teleported onto the goal line to take it while the keeper
stands on his six-yard line.

### The ball was restarted where it crossed the line

`CompleteOopRestart` passed `foul_x/foul_y` — the exit point — straight into
`BeginRestart`. The Amiga writes a **placement** into those globals instead
(SETPIECES §2, asm:41573-41583), and only a throw-in keeps the ball's own
coordinate:

| Restart | X | Y |
|---|---|---|
| Goal kick, top / bottom | 396 or 276 | 154 / 744 |
| Corner | 86 or 585 | 134 / 764 |
| Throw-in | 81 or 590 | ball Y |

So a goal kick was being taken from a point *off the pitch*, and a corner from
wherever the ball happened to leave rather than from the flag. This is the
"restart placement coordinates" item §5a left outstanding; it is now closed.

### The keeper did not take his own goal kick

`PickRestartTaker` special-cased only `KeeperHoldsBall`; a goal kick fell through
to "nearest available", which is an outfielder.

That was never a sourced decision. [B8](B8-set-pieces.md) §7 lists *"goal-kick /
keeper-holds release nuance"* as an open question and [../SETPIECES.md](../SETPIECES.md)
records the release as unread — so the outfielder was the fallback branch, not a
finding. The Amiga's placement puts the ball in the six-yard box, which is where a
keeper takes it from. `test_restart_aim_freeze.cpp` asserted the old behaviour and
has been reversed with the reasoning recorded in place rather than deleted.

**Re-pins:** two, plus golden and corpus. One further test corrected:
`test_ball_oop_wire` asserted `foul_x < kPlayableMinX` — i.e. that the ball was
restarted from off the pitch — which only held because the placement was missing.

---

## 5f. R9 — the camera froze on the goal (a regression from R4)

Reported as: after a goal the camera lingered on the goal that was scored and
stopped following the ball. **Self-inflicted, by R4.**

`show_fans_counter` is the celebration length. Before R4 nothing in the engine
ever wrote it, so the camera's highest-priority branch — `show_fans_counter > 0
→ frozen` — could never fire. R4 started writing it at every goal (the two
match-stream draws, §5a finding 5) and **nothing ticked it down**. A counter
written but never decremented is a latch, not a timer: from the first goal
onward the camera was frozen for the rest of the match, pointing at the goal
that had just been scored.

It now counts down in `UpdateTime` alongside the other match timers,
unconditionally, so a goal on a period boundary still releases it.

**The lesson is narrower than "add a decrement".** R4 wired a value into a field
that an existing consumer already read, and reviewed the write without checking
what read it. The write was correct; the field's contract was not honoured. Any
change that starts populating a previously-dead field needs its consumers read,
not just its producer.

### A second latch, found on the way

`Camera::Update` did `mode_ = p.frozen ? Frozen : mode_` — never restoring a
non-frozen mode, so `Mode()` reported `Frozen` permanently once anything had
frozen it. Position was unaffected (`Apply` tests `p.frozen`, not `mode_`), so
this was cosmetic; `Mode()` has no consumer outside tests. But it is the kind of
defect that makes a test *look* like it covers something.

### Why the tests missed both

`test_camera.cpp`'s freeze case sets `show_fans_counter` to a constant and
asserts the camera stops. Nothing asserted it starts again. **A freeze test
without a thaw test is half a test**, and it is the half that cannot catch a
latch. Both halves are now present, and the thaw case runs the counter down
through `UpdateTime` rather than clearing it by hand — so it exercises the real
release path.

---

## 6. Open questions

- **Which side of each of the six disagreements is right.** Resolved by: a trace,
  A3 item 4. Not by argument, and not by whichever reading is currently in the code.
- **Whether the one-team-per-frame alternation is real.** The engine implements it;
  [../INPUT.md](../INPUT.md) §4 flags it as single-sourced and the Amiga does not
  confirm it. It doubles input latency and halves the player-selection rate, so if
  it is wrong the whole game feels wrong in a way no other item here can cause.
  **This is the highest-consequence unconfirmed claim in the engine** and it is
  cheap to test: a scripted input, two runs, count the ticks to first response.
- **How to rescale `fictional.hpp` into 0–7** without flattening the squads.
  A design decision, not a measurement — but make it once, explicitly, rather than
  letting a clamp make it silently.
- **Whether `Rand2` is original** ([../SIMULATION.md](../SIMULATION.md) §10). B11's
  isolation of career result generation from the match RNG stream depends on it. Not
  blocking here, but it is the same class of question as R4 item 5 and worth
  answering while the RNG stream is already under the microscope.
- **The pass-strength banding.** R3 substitutes distance-banded launch, but the
  Amiga set corrected *itself* on this (ledger §6a) after `DoPass` was skimmed for
  targeting and counted as swept. Treat the banding as the least-settled value in R3
  and confirm it before the others.
- **Unmeasured constants remain [../LEGACY.md](../LEGACY.md) §15's**, not this
  file's. A candidate value from a disassembly is a measurement of the original, not
  a specification we are obliged to match, and not permission to skip confirming it.

### Opened by the implementation, and left open deliberately

- **The Velocity bonus interior is ours.** Only the endpoints (−384, +384) are
  sourced, and 768 over seven steps is 109.71 — not a clean stride. The interior is
  a linear interpolation and is tagged as such in `shooting.hpp`. Finishing, by
  contrast, has an exact stride of **128** between its sourced endpoints and is
  very likely right as written. Do not cite `kBallSpeedKicking`'s interior as
  measured.
- **Our pass range and the Amiga's pass banding disagree by 3×.**
  `PassTargetMaxDistSq` caps a Passing-7 pass at 126 units; the ledger's eight
  50-unit bands imply passes out to ~400. Bands 3–7 are therefore unreachable
  through the targeted path. One of the two is wrong — most likely our range, but
  possibly a unit-scale difference — and widening either one silently would hide
  the question. Recorded, not resolved.
- **A targetless quick-fire is not covered by the banding rule.** With no receiver
  there is no receiver-distance, and the ±1000 facing ray must not be read as one
  or a blind clearance gets maximum power. `kClearanceBand` is a continuity choice
  (nearest band to the pre-B13 flat speed), not a measurement.
- **The celebration *length* formula is ours.** Two draws from the match stream is
  the sourced part and is what matters for stream alignment; the length derived
  from them is invented and bounded.
- **Restart placement coordinates are still outstanding** — the goal-kick X pair
  (396/276) and the symmetric taking-team-relative state encoding for goal kicks
  and throw-ins, where the same `gameState` value means opposite absolute geometry
  for the two sides. [../amiga/SETPIECES.md](../amiga/SETPIECES.md) §"The state
  values are relative, not absolute" warns this is easy to get wrong, which is why
  it was not rushed in behind the turn masks.
- **Whether the corpus check should regenerate.** As written it cannot detect its
  own staleness (§5a finding 1). Making it diff against a fresh engine run would
  have caught a stale corpus the moment B6a bumped the format. That is an
  [A3](A3-trace-harness.md) change, not a B13 one.
