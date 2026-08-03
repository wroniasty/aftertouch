# B7 — Contests

Slide tackle (entry, deflect, foul, recovery), possession-contest RNG, and
static/jump headers with the driven/lob stick switch. Kick launch remains B6;
foul consequences (cards, free kicks, injury) are B8; CPU trigger brains are B9.

Depends on: B6   Blocks: B8   Wave: 3

---

## 0. One-paragraph version

Fire without exclusive possession forks away from kick: high ball bands start a
jump or static header; otherwise a slide at 1792 with locked facing. While
sliding, early fire-release shortens the attempt; ball reach (provisional
`dist² ≤ 32`) either deflects at 125 % of current speed in the held direction or
runs a near-coin-flip possession contest when the opponent is also close.
Player–player contact runs the foul ladder (`tackle_state` + facing). Headers
resolve `(facing − held) & 7` into drive/lob/aim at contact. Acceptance is a
scripted HashState pin — corpus contest distribution remains an A3 follow-up.

---

## 1. Scope

**In:**

- Fire fork: kick only with `player_has_ball`; else slide/header entry.
- Slide lifecycle: begin, early release, ball contact, good-tackle, foul test,
  recovery tables.
- Possession contest via `resolve_rng` + clamped chance table.
- Static / jump header entry + contact (flying/lob switch, Heading table).
- `ProcessContestContacts` after `MovePlayers`.
- Unit tests + `test_contest_sequence` HashState pin.

**Out:**

| Excluded | Owner |
|---|---|
| Cards, free-kick placement, injury rolls | B8 |
| CPU header/tackle decision windows | B9 |
| Running (non-slide) tackle if distinct | Measurement |
| Real SWOS corpus contest distribution | A3 follow-up |
| Player aerial `z` jump arc polish | Measurement |
| Reviving dead `fasterTackle` | Leave dead |

---

## 2. Design

### 2.1 Fire fork

After possession, before kick: if `player_has_ball` → `ApplyKickOrPass`; else on
`fire_this_frame` (or leftover fire pulse) while `Normal` / InProgress →
`TryBeginSlideOrHeader`. Consume kick pulses on contest entry.

### 2.2 Header vs slide

Bands (HEADING §2): need `pl_very_close || pl_not_far`. High z bands
(`8–12` / `12–17` / `>17`) → JumpHeader @ 2048; else if not those high bands and
fire without ball on ground path → slide @ 1792. Near + low z without high bands
and not slide? Static header when near bands and not high: static for close low
ball. Exact split:

- High bands + proximity → jump header.
- Else if (`pl_very_close || pl_close`) and not high → static header when ball is
  off the ground a little (`ball_4_to_8` or `ball_less_equal_4` with not-far)?  

Locked from plan: high → jump; else → static when header proximity; ground path
(no high bands) + fire without ball → **slide**. So static only when
`pl_very_close || pl_not_far`, not high bands, and we treat "else" of the header
branch as static (HEADING §2 else path) — meaning fire without ball near a
low/medium ball starts static header rather than slide when in header proximity.

Re-read plan decision 2 carefully:

> if not (`pl_very_close || pl_not_far`) → no header entry; if high bands →
> JumpHeader; else → StaticHeader. Ground path (no high bands) + fire without
> ball → slide.

There's tension: "else → StaticHeader" vs "ground path → slide". Resolve as:

1. If high bands and (`pl_very_close || pl_not_far`) → jump.
2. Else if (`pl_very_close || pl_not_far`) and (`ball_4_to_8` or above low ground
   but not high)?  

HEADING §2 else of high = static header path. So when in proximity and NOT high
bands → static header. That leaves slide for when NOT in header proximity
(`!pl_very_close && !pl_not_far`) — a chase slide from further away.

Locked implementation:

1. High + (`very_close || not_far`) → JumpHeader.
2. Else if (`very_close || not_far`) → StaticHeader.
3. Else → BeginSlide (chase from further out).

### 2.3 Slide / foul / contest

Constants and ladders from TACKLING.md. Ball reach provisional `≤ 32` sq.
Challenged ball (`opponent.ball_distance < 9`) → contest instead of free
deflect. `won_the_ball_timer = 12`; tick down; while >0 keep
`ball_can_be_controlled` on winner. Chance table clamp `|diff|` to 7.
`player_down_timer` only counts down while `> 0` (avoid int8 wrap from −1).

### 2.4 Headers

Jump: direction switch + flying/lob. Static: max 90° turn, flat 1792,
`deltaZ = −deltaZ/2`. Heading attr table **8 entries**, index `AttrIndex0to7`.

> **Corrected by [B13](B13-amiga-oracle.md) / R2.** This line used to read *"13
> entries, index `min(attr,12)`"* and that mis-read reached the engine. The Amiga's
> `playerStrongHeaderSpeedIncrease` stops after eight values; the five that followed
> in the DOS listing belong to the next data item, and their near-constant ~514 stride
> is characteristic of an offset block rather than a tuning curve. Heading is a pure
> handicap ramp with no upside — 7 gets nothing, everything below is a penalty. See
> [../AMIGA_CHANGES.md](../AMIGA_CHANGES.md) §2.2.

### 2.5 Tick order

`ApplyTeamControls`: possession → contest entry / kick → dest/speed/dribble.
Every Step after `MovePlayers`: `ProcessContestContacts` (timers, ball/header
contact, foul).

---

## 3. Interfaces

| Path | Role |
|---|---|
| `tackling.hpp` | Slide, foul, recovery, contest roll, `ProcessContestContacts` |
| `heading.hpp` | Begin + contact |
| `shooting.hpp` | Gate kick on `player_has_ball` |
| `movement.hpp` | Wire fork; speed→0 recovery hook |
| `match_engine.cpp` | Call contacts after integrate |

---

## 4. Work items

1. Subfile  
2. Fire fork + BeginSlide + test  
3. Ball contact + deflect + good-tackle  
4. Foul + recovery  
5. Possession contest  
6. Headers  
7. Acceptance hash + re-pins + CURRENTSTATE  

---

## 5. Tests and acceptance

| Test | Pins |
|---|---|
| `test_slide_begin.cpp` | Fire without ball → Tackling @ 1792 |
| `test_slide_deflect.cpp` | Contact → 125 %, input dir, tackle_state |
| `test_foul_test.cpp` | From-behind foul; keeper; good-tackle |
| `test_possession_contest.cpp` | Equal ~50 %; clamp at 7 |
| `test_header_select.cpp` | Band → static/jump; lob/drive |
| `test_header_launch.cpp` | Static 1792; jump 125 % + attr |
| `test_contest_sequence.cpp` | HashState acceptance |

**Done when:** scripted contest-sequence `HashState` stable under Amiga profile.

---

## 6. Open questions

- Exact ball/header contact call-site predicates (LEGACY §15).  
- What else `won_the_ball_timer` gates in the reference.  
- Jump-header recovery interval (dead-store risk like tackle).  
- Possession-contest invocation site beyond challenged tackle contact.  
