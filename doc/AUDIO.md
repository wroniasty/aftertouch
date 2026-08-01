# AUDIO.md

Commentary, crowd chants, music and effects: how match events become spoken lines,
how the crowd knows the score, and the modding layer that lets both be replaced
with folders of audio files. Traced through the reference DOS port in
[../reference/swos-port/](../reference/swos-port/).

> **Reference only — not an implementation basis.** aftertouch will build its own
> audio system. This document is here because **the crowd and commentary logic is
> game design, not plumbing** — §3 and §4 describe rules about *when* a football
> game should make a noise, and those rules are worth having whatever the
> implementation. The file loading and SDL_mixer mechanics in §6 are not.
>
> **Provenance.** The audio subsystem is modern C++ with named constants, and
> `docs/sound-modding.txt` documents the asset layout from the modder's side. The
> **event taxonomy in §2 is the original's** — those 28 commentary categories are
> what SWOS actually reacts to. Most of the loading machinery is the porters'.

---

## 0. One-paragraph version

Commentary is **event-driven and queued, not streamed**: gameplay code calls
`enqueueThrowInSample()` or `enqueueYellowCardSample()`, which set a countdown
timer, and `playEnqueuedSamples()` fires exactly **one** pending line per tick in a
fixed priority order. There are **28 commentary categories**, each backed by a
directory of interchangeable audio files, one chosen at random per event — so the
same situation never sounds identical twice. Crowd chants are separately driven by
the **scoreline**: a chant index is derived from the goal difference, with a
deliberate 50 % chance of instead playing the "cheer up the losing team" sample, and
an equaliser gets its own chant. The **intro chant is chosen by the home team's
shirt colour** through a 16-entry lookup, so red teams and blue teams sing different
songs. All of it is replaceable by dropping files into an `audio/` directory tree.

---

## 1. Structure

| File | Lines | Role |
|---|---|---|
| [comments.cpp](../reference/swos-port/src/audio/comments.cpp) | 562 | Commentary: queue, categories, custom packs |
| [chants.cpp](../reference/swos-port/src/audio/chants.cpp) | 358 | Crowd, driven by scoreline |
| [music.cpp](../reference/swos-port/src/audio/music.cpp) | 250 | |
| [SoundSample.cpp](../reference/swos-port/src/audio/SoundSample.cpp) | 244 | One loaded sample |
| [audio.cpp](../reference/swos-port/src/audio/audio.cpp) | 221 | Init, master volume (`kDefaultVolume = 64`) |
| [SampleTable.cpp](../reference/swos-port/src/audio/SampleTable.cpp) | 178 | Pools of interchangeable samples |
| [sfx.cpp](../reference/swos-port/src/audio/sfx.cpp) | 137 | `SfxSampleIndex` — kicks, whistles, bounces |

The gameplay code never touches any of this directly. It calls one of the
`enqueue*` functions, and everything else is this subsystem's problem — a clean
separation worth preserving.

---

## 2. The commentary event taxonomy

From `docs/sound-modding.txt`, the directories scanned under
`audio\commentary\`:

```
corner              dirty_tackle        end_game_rout       end_game_sensational
end_game_so_close   free_kick           goal                good_play
good_tackle         header              hit_bar             hit_post
injury              keeper_claimed      keeper_saved        near_miss
own_goal            penalty             penalty_missed      penalty_scored
penalty_saved       red_card            substitution        tactic_changed
throw_in            yellow_card
```

**Twenty-eight categories. This is the complete list of things SWOS notices.**
Read as a design document it is more informative than any amount of code: the game
distinguishes a missed penalty from a saved one, a rout from a sensational finish
from a narrow one, a dirty tackle from a good one, and the post from the bar.

Every one of these has a call site already documented elsewhere:

| Category | Fired from |
|---|---|
| `hit_bar`, `hit_post` | [BALL.md](BALL.md) §6 — **chosen by coin flip, not by what was hit** |
| `dirty_tackle` | [TACKLING.md](TACKLING.md) §5, on `TS_GOOD_TACKLE` — the "dangerous play" line |
| `good_tackle` | [TACKLING.md](TACKLING.md) §4 |
| `throw_in`, `corner` | [SETPIECES.md](SETPIECES.md) |
| `penalty` | [SETPIECES.md](SETPIECES.md) §4 |
| `yellow_card`, `red_card` | [REFEREE.md](REFEREE.md) §1 |
| `substitution`, `tactic_changed` | [BENCH.md](BENCH.md) §5, §6 |
| `header` | [HEADING.md](HEADING.md) |
| `injury` | [SIMULATION.md](SIMULATION.md) §6 |

**Each directory holds many files and one is picked at random per event.** A
missing or empty directory simply means that event is silent — no fallback, no
error.

---

## 3. The queue

[playEnqueuedSamples()](../reference/swos-port/src/audio/comments.cpp#L180), called
once per tick:

```
if      (yellowCardTimer  >= 0 && !--yellowCardTimer)  playYellowCard()
else if (redCardTimer     >= 0 && !--redCardTimer)     playRedCard()
else if (goodPassTimer    >= 0 && !--goodPassTimer)    playGoodPass()
else if (throwInTimer     >= 0 && !--throwInTimer)     playThrowIn()
else if (cornerTimer      >= 0 && !--cornerTimer)      playCorner()
else if (substituteTimer  >= 0 && !--substituteTimer)  playSubstitute()
else if (tacticsTimer     >= 0 && !--tacticsTimer)     playTacticsChange()

if (goalCounter) goalCounter--;      // "strange place to decrement this..."
playCrowdChants();
```

Three properties fall out of that `else if` chain:

- **At most one line starts per tick.** No overlapping commentary.
- **Priority is the chain order**, fixed at compile time: cards outrank passes
  outrank set pieces outrank management. Sensible, and never configurable.
- **Only the first pending timer decrements.** Everything below it in the chain is
  frozen while a higher-priority line is counting down, so its delay is not a
  deadline but a queue position.

Delays are `kEnqueuedSampleDelay = 70` and `kEnqueuedLongerSampleDelay = 100` ticks
— commentary is deliberately **late**, arriving well after the event, the way real
commentary does.

The porters left `// strange place to decrement this...` on the `goalCounter` line.
It is unrelated to audio and lives here because the original put it here.

Interruption is handled by `playComment(chunk, interrupt = true)` and
`commenteryOnChannelFinished()` [sic] — a new line can cut off a playing one.

---

## 4. Crowd chants — the crowd reads the scoreboard

[getChantSampleIndex()](../reference/swos-port/src/audio/chants.cpp#L284):

```
if (one team is on zero) {
    if (scores differ) {
        goalDiff = max(team1Goals, team2Goals)
        if (goalDiff <= 6) {
            if (!getRandomInRange(0, 1))
                sampleIndex = 6            // "cheer up the losing team"
            else
                sampleIndex = goalDiff - 1 // the "x-nil" chant, x = 1..6
        }
    }
} else if (team1Goals == team2Goals) {
    sampleIndex = 7                        // equaliser chant
}
```

The porters' own debug logs give the intent away: `"Let's cheer up the losing
team"`, `"Choosing %d-0 chant"`, `"Choosing equalizer chant"`.

So the crowd has **eight chants** keyed to match state:

| Index | When |
|---|---|
| 0–5 | Leading by 1–6 with the opponent on nil — "one-nil", "two-nil"… |
| 6 | Consolation for the losing side, **50 % of the time** |
| 7 | The equaliser |

**A 6–0 lead has a specific chant. A 2–1 has none** — the whole system only engages
while one side is on zero, or when the scores have just levelled. That is a small
amount of logic producing a surprisingly readable crowd, and the coin flip between
gloating and consolation is what stops it becoming mechanical.

`fadeOutChantsIfGameTurnedBoring(bool wasInteresting)` exists, which is the other
half: the crowd goes quiet when nothing is happening.

`kChantsVolume = 55` against a `kDefaultVolume` of 64 — chants sit under everything
else.

### The intro chant is chosen by kit colour

[loadIntroChant()](../reference/swos-port/src/audio/chants.cpp#L41):

```
color = topTeamInGame.prShirtCol           // 0..15
kIntroTeamChantIndices[16] = { -1,-1,3,-1,0,0,0,1,-1,1,2,5,-1,5,1,4 }
chantIndex = kIntroTeamChantIndices[color]

if (chantIndex >= 0) load introTeamChantsTable[chantIndex]
else                 fall back to the generic 4-line fans chant
```

**Six distinct intro chants, assigned by the home team's shirt colour**, with five
of the sixteen colours getting no chant at all and falling back to the generic one.
Colours 4, 5 and 6 share chant 0; colours 7, 9 and 14 share chant 1.

This is the same trick as [PITCH.md](PITCH.md) §3's career pitch hash: **derive
per-team flavour from immutable team data rather than storing it**. Zero storage,
stable across sessions, and it means the team in red always gets the same song.

---

## 5. Modding

From `docs/sound-modding.txt`: if an `audio` directory exists, all audio is loaded
from there. Commentary goes under `audio\commentary\<category>\`, and
`loadCustomCommentary()` / `loadZipComments()`
([comments.cpp:245-302](../reference/swos-port/src/audio/comments.cpp#L245-L302))
support both loose files and zip archives.

`toggleMuteCommentary()` exists as a runtime control.

This is entirely the porters' work, but the *shape* — commentary as a set of
named, randomly-sampled event pools — is the original's, and it is the right shape.

---

## 6. Constants quick reference

| Constant | Value | Meaning |
|---|---|---|
| `kEnqueuedSampleDelay` | 70 ticks | Standard commentary delay |
| `kEnqueuedLongerSampleDelay` | 100 ticks | |
| `kDefaultVolume` | 64 | Master |
| `kChantsVolume` | 55 | Crowd sits under commentary |
| Commentary categories | 28 | §2 |
| Crowd chants | 8 (indices 0–7) | §4 |
| Intro chants | 6, by shirt colour | §4 |
| `kModValue` | 97 | In `loadZipComments`, unexplained |

---

## 7. What this tells us

**Confirmed:**

- Commentary is enqueue-and-delay, one line per tick maximum, priority fixed by
  `else if` order. ✓
- Delays are 70–100 ticks: commentary deliberately lags the event. ✓
- 28 event categories, each a pool of interchangeable files sampled at random. ✓
- A missing category is silently mute. ✓
- Crowd chants are driven by the scoreline, engaging only when one side is on nil
  or the scores have just levelled. ✓
- 50 % chance of consoling the loser instead of celebrating the leader. ✓
- Intro chant is selected by the home team's shirt colour via a 16-entry table with
  five no-chant entries. ✓
- Chants fade out when the game gets boring. ✓

**Open:**

- `SfxSampleIndex` contents — the full effects list (kick, whistle, bounce, crowd
  noise) was not enumerated.
- `music.cpp` — what plays, when, and whether it is original.
- What "interesting" means to `fadeOutChantsIfGameTurnedBoring`.
- `kModValue = 97` in the zip loader.
- Whether the original's commentary delays were also 70/100, or whether these are
  the porters' values.
- `end_game_rout` / `end_game_sensational` / `end_game_so_close` — the thresholds
  that pick between them.
- Whether the commentary priority order matches the original's.

---

## 8. Guidance

- **Copy the event taxonomy** (§2). Those 28 categories are a specification for what
  a football game should notice, arrived at by people who shipped one. Getting this
  list right matters more than any audio code.
- **Enqueue with a delay; never play immediately.** The 70-tick lag is why SWOS's
  commentary sounds like commentary rather than a sound effect.
- **One line at a time, with an explicit priority order** — but make ours data, not
  an `else if` chain.
- **Pool interchangeable clips per event and sample randomly.** It is the cheapest
  possible defence against repetition and it makes the audio moddable for free.
- **Drive the crowd from match state, not from events.** §4's scoreline model is
  eight samples doing the work of a much larger system, and the consolation coin
  flip is the detail that sells it.
- **Derive per-team flavour from team data** (§4, intro chants) rather than storing
  it — same argument as [PITCH.md](PITCH.md) §9.
- **Keep the gameplay/audio boundary where the reference has it.** Gameplay calls
  `enqueueX()`; it knows nothing about channels, volumes or files. That line is
  correctly drawn and cost nothing to maintain.
