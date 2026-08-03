# B6a — Kick & aftertouch fidelity

Structural repairs to B6's kick/aftertouch path, the behavioural tests that
catch them, and the control telemetry that makes a feel complaint reportable as
a tick count. The timing *constants* are not in scope: they are fit targets and
need the oracle ([A3](A3-trace-harness.md) item 4).

Depends on: B6, A3   Blocks: the Wave-3 play-feel gate   Wave: 3

---

## 0. One-paragraph version

B6 shipped a structurally faithful aftertouch with six defects that are wrong
under *any* value of its provisional constants: two spin-table rows held the
wrong perpendicular, the window's strongest tick was unreachable by
construction, `normal_fire` was an edge where the reference has a level, the
fire charge dribbled the ball away from its own carrier, the shot-on-goal bonus
ignored position, and the pass-loft flags were dead and aliased. All six are
fixed, each behind a test that asserts a property rather than a table value.
The constants stay tagged `[PROVISIONAL: LEGACY §15 …]` and unfitted.

---

## 1. Scope

**In:**

- S1–S6 below, one behavioural test file each, driven through `MatchEngine::Step`.
- `test_aftertouch.cpp` rewritten property-style and parametrised over all eight
  kick octants.
- `KickProbe` (tracekit) + the C1a control HUD line + a `kick:` transcript line.
- The `shot_curl` corpus scenario, which never reached the ball.
- Provenance tags on every provisional kick/aftertouch constant.

**Out:**

| Excluded | Owner |
|---|---|
| Fitting `kFireHoldThreshold`, the spin tables, the launch pairs | Track M — needs the oracle |
| Whether the tick-4 speed trim is really unconditional | Track M — read the decompile |
| Whether a kick preserves ball height (volleys) | Track M |
| Reference recording / instrumented `swos-port` build | A3 item 4 |

---

## 2. The six structural defects

### S1 — curl geometry, and the naming that caused it

`kKickSpinFactor` / `kPassingSpinFactor` hold a lateral nudge per kick octant
per side. Six octants held the clockwise perpendicular of the kick direction;
**E and W held the counter-clockwise one**, so a horizontal kick curled against
the stick. Root cause is the flag naming: `left_spin` / `right_spin` describe a
*screen* side, while the latch condition is `(joy − kick) & 7 ∈ 1..3` — a
*rotation* relative to the kick. The two readings coincide for N and invert for
E. Fixed the rows and renamed the fields to `spin_cw` / `spin_ccw`; the rename
is the part that stops it recurring.

### S2 — the window's first tick was unreachable

`ApplyKickOrPass` opened `spin_timer = 0` during team controls and `UpdateBall`
sampled it in the same Step, where the stick is the kick direction by
construction (the kick reads that same field). `kSpinMultiplierFactor[0] = 8` —
the strongest fifth of the decay curve — could never be spent. The launch now
sets `kSpinArmed`; the window opens on the next Step. `kSpinArmed` never
survives the Step it is set on, so traces and the HUD still see `0..9`.

### S3 — `normal_fire` was an edge, not a level

It was raised only on the exact tick the hold counter equalled the threshold,
and dropped if the strike was refused — so a hold that was not strikeable on
that one tick was swallowed and never re-armed while the button stayed down.
SHOOTING §1's dispatch re-reads the flag every frame. It is now a level while
held. `WantContestEntry` drops to the press edge so a held button does not
re-enter a slide every tick; the CPU raises `fire_this_frame` alongside
`normal_fire`, so its tackles are unaffected.

### S4 — the charge threw the ball away

`ApplyTeamControls` suppressed `ApplyDribble` while fire was held, so the ball
stayed put while the carrier ran on, left the close band (8.5 u) inside a
12-tick hold, and the queued strike arrived as a slide or a header. Possession
during a charge is now ordinary possession; the strike tick short-circuits the
dribble anyway.

### S5 — the shot-on-goal bonus ignored position

`KickIsGoalward` tested the octant alone, so a hoof upfield from inside your own
area was paid as a shot on goal; `InPenaltyBox` ORed two incompatible box
definitions into a region a third of the pitch deep. Replaced by one predicate,
`ClassifyShotOnGoal`: goalward octant **and** inside the corridor **and** in the
attacking half → `LongShot`, plus the engine's own penalty area →
`Finishing`, otherwise `None`. SHOOTING §3's raw y thresholds (204/342/556/694)
disagree with `kPenaltyBoxTopY/BotY` by a factor of two in depth; they are kept
as `kRefShotBox*` fit targets and the engine's geometry is authoritative until
the coordinate scale is measured.

### S6 — the pass-loft flags were dead and aliased

`long_pass` / `long_spin_pass` are AFTERTOUCH §6's pass-loft gate, but the kick
path only ever wrote them as 0 while B8 reused `long_pass` as its restart
shortfall counter. Split into `restart_shortfall` (B8) and `long_pass` (loft),
and the pass path now lofts on a back-push at the vertical sample, measured
against the ball's travel direction the same way its curl table is indexed.

---

## 3. Test practice (why a green suite proved nothing)

B6's acceptance was a `HashState` pin. A hash proves determinism, not
correctness: every defect above survived a green `ctest`. Three rules now apply
to this area, and are worth applying to the next part:

1. **A hash pin is a determinism gate, never an acceptance criterion.** Each
   re-pin carries a one-line comment saying why the hash moved.
2. **Assert properties, not table values.** `test_aftertouch.cpp` used to assert
   `dest_x == 336 + 96`; it now asserts the nudge is perpendicular to the kick
   and on the side of the push. Those assertions survive the day the tables are
   fitted — which is the one day they must not need editing.
3. **Drive `MatchEngine::Step`, and loop over the octants.** Every old case used
   kick direction N on a hand-built state, which is exactly why S1, S2 and S4
   were invisible: two of them only exist in the Step order.

| File | Pins |
|---|---|
| `test_aftertouch.cpp` | Curl geometry, latch rules, decay shape, vertical mapping, window length — all eight octants |
| `test_aftertouch_window.cpp` | First sample reachable; authority decays; a late push does nothing; curl follows the stick for every octant |
| `test_fire_hold.cpp` | Tap vs hold; a hold across ineligibility still fires; exactly one strike per hold |
| `test_charge_possession.cpp` | 8 directions × {rolling, at rest} keep the ball through the charge |
| `test_shot_zone.cpp` | Own half / wide / long shot / in-area bonus ordering |
| `test_pass_loft.cpp` | Back-push lofts; curl alone does not; `restart_shortfall` untouched |
| `test_kick_probe.cpp` | The telemetry the HUD and transcript report |
| `test_tracekit.cpp` | The `shot_curl` corpus scenario actually strikes and curls |

---

## 4. Telemetry (I4)

`tracekit::KickProbe` observes post-Step state plus the input that produced it
and derives: press→strike latency, hold length, tap/hold outcome, the
`spin_timer` the curl latched on and its side, the vertical decision, and
whether a pass was lofted. Pure, no I/O, no SDL. Consumed by the C1a HUD
(a control line under the existing debug row) and by the sparse transcript,
which now emits `kick: side=N shot press->strike=… hold=…`.

The point is not convenience: it is that the next "shooting feels off" report
arrives as a tick count instead of an adjective.

---

## 5. The corpus entry that measured nothing

`shot_curl` — the entry [A3](A3-trace-harness.md) §2.7 names for aftertouch —
scripted `fire+NE` for 35 ticks while every home player was ~200 units from the
ball. It recorded a walk. A corpus entry that never reaches its mechanic cannot
report a divergence in it.

Scenarios now carry a `ScenarioSetup` id (a named starting state) which is
**serialised into the `.atin`**, so the committed input log plus the enum still
reproduce the trace with nothing implicit — A3 §2.7's rule that the log is the
part that cannot be regenerated is preserved. `ATIN` is v2; `ATTR` is v5
(`restart_shortfall`).

---

## 6. Open — Track M, and it needs the oracle

Everything below is a **measurement**, not a decision. None of it should be
guessed at, and `[PROVISIONAL: LEGACY §15 …]` tags in `shooting.hpp` /
`aftertouch.hpp` mark each one in code.

- `kFireHoldThreshold` (12; B6 §2.1 and LEGACY §8 both say ~4).
- `kSpinMultiplierFactor`, `kKickSpinFactor`, `kPassingSpinFactor` magnitudes.
- `kNormal/HighKickDeltaZ`, `kNormal/HighKickBallSpeed`, `kLongPass*`, and
  whether the sample really lands on tick 4.
- Whether the tick-4 facing trim is inside or outside the offset branch. As
  implemented it costs 25 % of a shot's speed whenever the stick is still held
  in the kick direction, which is the default input — worth reading the
  decompile before it is called correct.
- Whether a kick preserves the ball's height (no volleys today).
- The pitch coordinate scale that decides between `kRefShotBox*` and the
  engine's own penalty area.
