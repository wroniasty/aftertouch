# INPUT.md

The device layer: how a physical button becomes the eight-way direction plus one
fire button the match engine consumes, the two-level event model the port
introduced, and the per-frame alternation that means each team's controls are read
only every other tick. Traced through the reference DOS port in
[../reference/swos-port/](../reference/swos-port/).

> **Reference only — not an implementation basis.** aftertouch will write its own
> input layer against SDL directly. Two things here *are* worth carrying across,
> though, and they are not about devices: **§4's per-frame team alternation** is a
> simulation behaviour that affects input latency and therefore feel, and **§2's
> two-level event model** is the right architecture for any game with configurable
> controls. The rest — hats, trackballs, ini files, touch overlays — is the porters'
> and is device plumbing.
>
> Note this document is about *devices*. [CONTROL.md](CONTROL.md) is about **ball**
> control and is unrelated despite the name; what the engine does with the resulting
> direction and fire state is [MOVEMENT.md](MOVEMENT.md) §3 and
> [SHOOTING.md](SHOOTING.md) §1.

---

## 0. One-paragraph version

The match engine consumes exactly two things per team: a direction and a fire
state. Everything in this subsystem exists to produce them. The port introduces a
**two-level mapping** — *controller events* (this axis crossed this threshold, this
hat matched this mask) are translated through a configurable table into *game
control events* (`kGameEventLeft`, `kGameEventKick`, `kGameEventBench`), which are
**bit-flags**, so one physical input can fire several game events at once. The
translated events are written into `TeamGeneralInfo`'s control fields — the same
fields the AI writes ([MOVEMENT.md](MOVEMENT.md) §8) — and, critically,
`updateTeamControls` **handles one team per frame, alternating**, so each team's
input is sampled every other tick.

---

## 1. What the engine actually needs

From [STATE.md](STATE.md) §3, the entire input surface of the match engine is seven
fields on `TeamGeneralInfo`:

| Field | Off | Meaning |
|---|---|---|
| `currentAllowedDirection` | 44 | Octant 0–7, or `-1` for nothing held |
| `allowedDirections` | 42 | |
| `quickFire` | 48 | Tap → pass ([SHOOTING.md](SHOOTING.md) §1) |
| `normalFire` | 49 | Hold → shot |
| `firePressed` | 50 | Raw button state |
| `fireThisFrame` | 51 | Edge |
| `fireCounter` | 54 | Hold duration, drives tap/hold classification |

**That is the whole interface.** A joypad, a keyboard, a touchscreen and the CPU
all converge on these seven fields, which is why [AI.md](AI.md) §5 can describe the
CPU as "a virtual joystick" and mean it literally.

`currentAllowedDirection = -1` meaning "nothing held" is load-bearing in gameplay
code, not just input code — [HEADING.md](HEADING.md) §4 branches on it to select a
flying header, and [TACKLING.md](TACKLING.md) §4 falls back to sprite facing when
it is negative.

---

## 2. The two-level event model

`docs/controls.txt` describes the port's redesign ("Controls v2.0"):

> *"Concepts of 'controller event' and 'game control event' were introduced.
> Controller events are specific to game controllers... Game control events are
> instructions to the game coming from controllers, e.g. 'left', or 'kick', or
> 'bench'. The game maintains a mapping from controller events to game control
> events. Game control events are implemented as bit-fields so a single controller
> event may trigger multiple game events."*

```
enum GameControlEvents : int32_t {
    kNoGameEvents        = 0,
    kGameEventUp         = 1,
    kGameEventDown       = 2,
    kGameEventLeft       = 4,
    kGameEventRight      = 8,
    kGameEventKick       = 16,
    kGameEventBench      = 32,
    kGameEventPause      = 64,
    kGameEventReplay     = 128,
    kGameEventSaveHighlight = 256,
    kGameEventZoomIn     = 512,
    kGameEventZoomOut    = 1024,
};
```

**Eleven game events, of which five are gameplay** (four directions and kick) and
six are meta (bench, pause, replay, save highlight, two zooms). `kGameEventBench` is
the dedicated alternative to the double-tap gesture ([BENCH.md](BENCH.md) §2), and
the zoom events drive the port-only zoom ([PITCH.md](PITCH.md) §5).

`kGameEventMovementMask` masks the four direction bits — used by the bench
double-tap detector to compare directions while ignoring fire.

**Bit-flags rather than an enum of states** is the right call: diagonals are just
`kGameEventUp | kGameEventLeft`, and no separate diagonal handling exists anywhere.

---

## 3. Device element types

Four kinds of event-producing element, per `docs/controls.txt`:

| Element | Model |
|---|---|
| **Button** | Pressed / not, plus an **inverted** flag |
| **Axis** | Full 16-bit range; an arbitrary number of **intervals**, each firing an event while the value is inside it |
| **Hat** | Four OR-able states; bindings have a **mask** and an inverted flag |
| **Trackball** | Mouse-like; events on ±x and ±y movement |

The interval model for axes is the good part: rather than special-casing
positive-only axes, inverted axes and dead zones, all of it falls out of "define
the ranges that should fire". The porters note trackballs are **untested and
"probably doesn't work"** — they could not obtain hardware.

Implementation lives in [controls/joypads/](../reference/swos-port/src/controls/joypads/):
`Joypad.cpp`, `JoypadConfig.cpp` (900 lines — the persistence and matching logic),
and `VirtualJoypad.cpp` (596 lines, the touchscreen overlay).

**Keyboard** ([controls/keyboard/](../reference/swos-port/src/controls/keyboard/)):
key-sets extended from the original's fixed six mappings (four directions, kick,
bench) to arbitrary key → event maps, and **PC scancodes replaced with SDL
scancodes** to reach keys the original could not.

**Auto-configuration** gets real attention. If a newly connected controller has no
saved profile the game attempts a default mapping; a *quick config* menu covers the
common case; and full per-element configuration exists for troubleshooting. There
is also **noise filtering**: if events are detected firing continuously (an
off-centre axis), the user is asked to leave the controller idle and press a
button so those events can be excluded. That is a thoughtful touch and a real
problem with old hardware.

---

## 4. One team per frame

The behaviour worth carrying across.
[gameControls.cpp](../reference/swos-port/src/controls/gameControls.cpp#L58):

```
// Sets control related fields in team structure. Called once per frame.
// Handles one team per frame (next team next frame).
auto team = ++m_teamSwitchCounter & 1 ? &swos.topTeamData : &swos.bottomTeamData;
```

```
void updateTeamControls(TeamGeneralInfo *team)
{
    A6 = team;
    UpdateControlledPlayer();          // AI.md §2 — selection
    UpdatePlayerBeingPassedTo();

    if (team->playerNumber) {          // 0 = CPU team
        auto player = team->playerNumber == 2 ? kPlayer2 : kPlayer1;
        auto events = getPlayerEvents(player);
        updateGameControls(player, events);
        updateTeamControls(team, player, events);
    }

    if (!team->resetControls && inBench()) {
        team->currentAllowedDirection = kNoDirection;
        team->quickFire = team->normalFire = 0;
        team->firePressed = team->fireThisFrame = 0;
        team->fireCounter = 0;
    }
}
```

**Each team's controls — and its player selection — are updated only every other
tick.** This is not an optimisation detail; it is a gameplay property:

- Input latency is up to **two ticks**, not one.
- `UpdateControlledPlayer` (the "which player am I driving" logic,
  [AI.md](AI.md) §2) also runs at half rate, so player switching is coarser than
  the tick rate suggests.
- The two teams are **out of phase**, so in a two-player match the players do not
  have symmetric latency on any given tick.

[MOVEMENT.md](MOVEMENT.md) §1 describes the per-tick pipeline; this is the piece
that says the input stage of that pipeline alternates.

**Bench zeroing**: while the bench is open, all seven control fields are forced to
neutral unless `resetControls` is set — so a player cannot steer his team while
browsing substitutions.

`postUpdateTeamControls` clears `headerOrTackle` after the main update, closing the
one-tick contest flag ([TACKLING.md](TACKLING.md) §1).

---

## 5. Configuration UI

Not detailed here, but for orientation — roughly 3,500 lines across:

| File | Role |
|---|---|
| `selectMatchControls.cpp` (535) | Who plays with what, per match |
| `controlOptionsMenu.cpp` (329) | |
| `selectGameControlEventsMenu.cpp` (301) | Bind events |
| `quickConfigMenu.cpp` (290) | The common path |
| `joypads/ui/joypadConfigMenu.cpp` (467) | Full per-element config |
| `joypads/ui/configureAxisMenu.cpp` (358) | Interval editing |
| `joypads/ui/configureHatMenu.cpp` (299) | Mask editing |
| `keyboard/setupKeyboardMenu.cpp` (266) | |

Profiles persist to an ini file keyed by controller identity.

`VirtualJoypad` is an on-screen touch pad for Android
([src/android/](../reference/swos-port/src/android/)): a `kPadSegmentWidth = 31` ×
`kPadSegmentHeight = 40` eight-way pad at the bottom-left, a 40 px fire button at
the bottom-right, `kMaxFingers = 5`, with optional touch trails and transparent
buttons. Positioned in the 320×200 logical space ([RENDERING.md](RENDERING.md) §1).

---

## 6. Constants quick reference

| Constant | Value | Meaning |
|---|---|---|
| `GameControlEvents` | 11 flags, `1`…`1024` | §2 |
| Axis range | −32,768…32,767 | 16-bit, interval-matched |
| Hat states | 4, OR-able, mask + inverted | |
| Team update | **1 team per frame, alternating** | §4 |
| `kShowTouchMarkDelay` | 2000 ms | Virtual pad |
| `kMaxFingers` | 5 | |
| `kPadSegmentWidth/Height` | 31 / 40 | |
| `kFireButtonSize` | 40 | |

---

## 7. What this tells us

**Confirmed:**

- The match engine's entire input surface is seven fields on `TeamGeneralInfo`,
  shared by human and CPU. ✓
- `currentAllowedDirection == -1` ("nothing held") is read by gameplay code, not
  just input code. ✓
- Two-level mapping: controller events → game control events, the latter as
  bit-flags so one input can fire several. ✓
- Eleven game events: five gameplay, six meta. ✓
- Axis configuration is interval-based, which subsumes inverted and half-range axes
  without special cases. ✓
- **Controls and player selection update one team per frame, alternating** —
  two-tick worst-case latency, teams out of phase. ✓
- Bench open forces all control fields neutral. ✓
- Keyboard uses SDL scancodes; the original was limited to six fixed mappings. ✓

**Open:**

- **Whether the original also alternated teams per frame**, or whether that is the
  porters'. This matters — it is the one item here with gameplay consequences, and
  §4's comment reads like ported behaviour but is not proven. A trace would settle
  it.
- How `fireCounter` converts to the tap/hold threshold ([SHOOTING.md](SHOOTING.md)
  §7 lists the community estimate of ~4 ticks as unmeasured).
- `getPlayerEvents` / `updateGameControls` internals — the direction-combining step
  that turns four bit-flags into an octant.
- `team->resetControls` — set by whom, and why bench zeroing is conditional on it.
- The original's control scheme as documented in [LEGACY.md](LEGACY.md) §3 versus
  this; the port clearly extends it.

---

## 8. Guidance

- **Keep the seven-field interface.** One struct that both the human input path and
  the AI write to is why SWOS gets identical behaviour from both, and it is what
  makes replays and headless simulation possible ([PLAN.md](PLAN.md) §0). This is
  the single most important thing in this document.
- **Adopt the two-level event model.** Device events → game events as bit-flags,
  with a configurable table in between. It is more architecture than a small game
  needs on day one and it is much harder to retrofit.
- **Use interval-based axis configuration** if we support gamepads at all. It
  handles dead zones, inverted axes and half-range triggers with no special cases.
- **Decide the per-frame alternation deliberately** (§4). Either match it because
  the reference does and it affects feel, or update both teams every tick and know
  that input latency will differ from the original. Do not inherit it by accident.
- **Keep meta events in the same enum as gameplay events.** Pause, replay and bench
  flowing through one path means one place to block input during stoppages, which
  is exactly what the bench zeroing in §4 relies on.
- **Do not build the configuration UI early.** 3,500 lines of it exists here; almost
  none of it is needed until real users have real hardware problems.
