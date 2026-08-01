# MENUS.md

The menu system: SWOS's binary menu format, the entry model that makes every screen
in the game one data structure, the explicit four-way navigation graph, and the
`mnu2h` mini-language the porters built to author it. Traced through the reference
DOS port in [../reference/swos-port/](../reference/swos-port/).

> **Reference only — not an implementation basis.** aftertouch builds its own UI:
> ImGui behind an `IUiBackend` abstraction now, a hand-rolled widget layer later
> ([PLAN.md](PLAN.md) §6, §9 Phase 3). None of this format will be implemented.
>
> It is documented anyway for one reason. SWOS is *"a very menu-heavy game
> containing a plethora of menus, many of them quite elaborate"* — and its answer
> was **a declarative data format plus a compiler**, not screens of layout code.
> [PLAN.md](PLAN.md) §9 Phase 3 says "you will be styling a dozen widget types you
> know you need rather than designing a toolkit speculatively", and this document is
> evidence about what that dozen turns out to be (§3) and about how the authoring
> problem gets solved twice by two different teams thirty years apart (§5).

---

## 0. One-paragraph version

A SWOS menu is a **binary structure of entries**, each with a position, a size, a
background, a foreground, and four explicit neighbour links — `upEntry`,
`downEntry`, `leftEntry`, `rightEntry`. There is no layout engine and no automatic
focus order: **navigation is an authored graph**. Each entry's foreground is one of
eight content kinds (a string, a sprite, a number, a string table, a callback that
draws it), and its background one of four, with colour drawn from a small named
palette. Authoring this by hand meant hex-editing; SWOS++ improved it to NASM
macros; the porters wrote **`mnu2h`**, a Python mini-compiler turning readable
`.mnu` files into C++ headers that compile to the same binary layout, with
variables, arithmetic, defaults and a `@prevEndY` cursor so entries can be laid out
relative to each other.

---

## 1. The entry model

[MenuEntry.h](../reference/swos-port/src/menus/engine/MenuEntry.h).

**Foreground** — what the entry shows:

```
kEntryNoForeground        = 0
kEntryContentFunction     = 1     // a callback draws it
kEntryString              = 2
kEntrySprite2             = 3
kEntryStringTable         = 4     // indexed string, for cycling options
kEntryMultilineText       = 5
kEntryNumber              = 6
kEntryColorConvertedSprite= 7     // team-coloured, RENDERING.md §4

// porters' extensions:
kEntryMenuSpecificSprite
kEntryBoolOption
```

**Background** — how it is framed:

```
kEntryNoBackground     = 0
kEntryBackgroundFunction = 1
kEntryFrameAndBackColor= 2
kEntrySprite1          = 3
```

**Eight content kinds cover the entire game.** A football manager with squad lists,
tactics grids, league tables, transfer screens and match options needs: text, a
number, a sprite, an indexed string, multi-line text, and an escape hatch
(`kEntryContentFunction`) for anything else. That is a useful data point for
[PLAN.md](PLAN.md) §9 Phase 3's "a dozen widget types you know you need".

The `kEntryStringTable` kind deserves note: an option that cycles through fixed
values is not a widget, it is a content *type*. Most of SWOS's options screens are
built from it.

---

## 2. Colour

Backgrounds and frames are separate small enums, combined by OR:

```
kNoBackground = 0   kGray = 7    kDarkBlue = 3
kLightBrownWithOrangeFrame = 4   kLightBrownWithYellowFrame = 6
kRed = 10   kPurple = 11   kLightBlue = 13   kGreen = 14   kYellow = 15

kGrayFrame = 0x10   kWhiteFrame = 0x20   kBlackFrame = 0x30
kBrownFrame = 0x40  kLightBrownFrame = 0x50  kOrangeFrame = 0x60
kDarkGrayFrame = 0x70
```

So `kGray | kGrayFrame` is one byte: low nibble background, high nibble frame.
**Ten backgrounds and seven frame colours**, named rather than numeric, drawn from
the menu palette ([RENDERING.md](RENDERING.md) §5).

Note two of the background names encode a frame in the name itself
(`kLightBrownWithOrangeFrame`) — an inconsistency inherited from whatever the
original called them.

---

## 3. Navigation is an authored graph

From `main.mnu`:

```
Entry editTactics {
    y: kStartY
    color: @kDarkBlue
    downEntry: editCustomTeams
    ...
}
```

Every entry names its neighbours explicitly. **There is no focus-order inference,
no spatial navigation, no tab index.** If an entry does not name a `downEntry`,
pressing down does nothing.

This is laborious to author and completely predictable at runtime, which for a
game driven by an eight-way stick and one button is the right trade. It also means
the navigation graph can be *non-spatial* — a menu can wrap, skip, or jump across
columns wherever the designer wants, with no fighting against a layout system.

Menus also carry:

```
Menu mainMenu {
    y: 1
    onInit: mainMenuOnInit
    initialEntry: friendly
}
```

`initialEntry` — where focus starts — and `onInit`, a C++ callback. The menu file
declares structure; behaviour is a named function the including `.cpp` must
provide.

---

## 4. The engine

[menus/engine/](../reference/swos-port/src/menus/engine/), ~2,400 lines:

| File | Lines | Role |
|---|---|---|
| `menuItemRenderer.cpp` | 738 | Draws entries by content/background kind |
| `menuMouse.cpp` | 563 | Mouse support — an addition; the original was stick-only |
| `unpackMenu.cpp` | 553 | Binary menu → runtime structure |
| `drawMenu.cpp` | 467 | |
| `menuCodes.h` | 361 | |
| `menus.cpp` | 270 | |
| `MenuEntry.cpp` | 173 | |
| `menuProc.cpp` | 165 | |
| `menuControls.cpp` | 162 | Stick/button navigation over the graph |

The API is `showMenu()` / `activateMenu()` / `unpackMenu()`. The **unpack step** is
the interesting one: menus are stored packed and expanded into a runtime structure
on display, which on a 1994 memory budget is the difference between having many
menus and not.

`docs/SWOS/menus.txt` is the porters' notes on the original binary format and is
the place to start if the byte layout is ever needed.

---

## 5. `mnu2h`

The porters' mini-compiler (`docs/mnu2h.md`). Its motivation section is a compact
history of the same problem being solved three times:

> *"At first the only option was manual modification of bytes in hex editor. Those
> edits were slow, painful and error-prone. More often than not it would all end in
> a shameful defeat (i.e. crash). SWOS++ upgraded it to a set of NASM macros...
> `mnu2h` builds upon this foundation and takes it further."*

`python mnu2h.py <input.mnu> <output.mnu.h>` produces a C++ header defining a
structure in SWOS's binary menu layout, included once by the `.cpp` that provides
the handlers. Twenty-two `.mnu` files exist in
[src/menus/mnu/](../reference/swos-port/src/menus/mnu/).

The language is **deliberately not Turing-complete** and supports:

- **Variables and arithmetic** — `kBaseX = (@kScreenWidth - kBigButtonWidth) / 2 - kMenuOffset`
- **A shared include** (`common.mh`) of cross-menu constants
- **Defaults** — `defaultX:`, `defaultWidth:`, `defaultTextFlags:` set once per menu
- **A layout cursor** — `defaultY: @prevEndY + 4`, so entries stack relative to the
  previous one without hardcoded coordinates
- **`@`-prefixed built-ins** — `@kScreenWidth`, `@kGreen`, `@kBigText`, `@prevEndY`

`@prevEndY` is the whole layout engine: one relative cursor. Combined with
per-menu defaults it removes most of the coordinate arithmetic while keeping the
output a flat, fully-resolved binary structure with no runtime layout cost.

That is a genuinely good design point for a constrained UI, and it is the part of
this document most worth remembering.

---

## 6. The menus themselves

Twenty-two definitions, which is a fair census of what a game like this needs:

```
main            versus          stadium         continue      continueAbort
quit            options         audioOptions    videoOptions  windowMode
controlOptions  selectMatchControls             selectGameControlEvents
setupKeyboard   joypadConfig    configureAxis   configureHat  configureTrackball
replays         replayExit      selectFiles
```

Note **eight of the twenty-two are input configuration** ([INPUT.md](INPUT.md) §5)
— by count, the single largest category. Most are the porters' additions; the
original had far fewer.

Screens implemented outside this list, in
[src/menus/](../reference/swos-port/src/menus/): `mainMenu.cpp`, `versusMenu.cpp`,
`stadiumMenu.cpp` (279 lines), `continueMenu.cpp`, `continueAbortMenu.cpp`.

Absent entirely from this port: **the career and management screens** — squad
selection, transfers, league tables, the whole manager game. The port is a match
engine plus a shell around it, not the full SWOS.

---

## 7. What this tells us

**Confirmed:**

- A menu is a flat binary structure of entries; navigation is an **explicitly
  authored four-way graph** with no inference. ✓
- Eight foreground content kinds and four background kinds cover the entire game. ✓
- One of those kinds is an escape hatch to a draw callback. ✓
- Colour is one byte: background nibble OR frame nibble, from named enums. ✓
- Menus declare `onInit` and `initialEntry`; behaviour lives in C++. ✓
- Menus are stored packed and expanded at display time. ✓
- `mnu2h` is a non-Turing-complete declarative language with variables, arithmetic,
  per-menu defaults and a single `@prevEndY` relative-layout cursor. ✓
- Twenty-two menus, eight of them input configuration. ✓
- Career/management screens are not in this port. ✓

**Open:**

- The **binary menu format itself** — byte layout is in `docs/SWOS/menus.txt`,
  unread here.
- `menuCodes.h` (361 lines) — what the codes are.
- How `kEntryStringTable` options bind to the values they cycle.
- Whether the original supported mouse at all, or whether `menuMouse.cpp` is
  entirely new.
- The full `mnu2h` grammar, and whether it does any validation of the navigation
  graph (dangling `downEntry` references, unreachable entries).

---

## 8. Guidance

None of this becomes code. Four things are worth carrying into our own UI work:

- **Author screens as data, compile them, and keep behaviour in code.** Both the
  original team and the porters converged on this after trying the alternatives.
  Whatever our widget layer looks like, screens should be declarations.
- **A single relative-layout cursor goes a long way.** `@prevEndY + 4` plus per-menu
  defaults eliminates most coordinate arithmetic without needing a real layout
  engine. If we hand-roll widgets in Phase 3, start there rather than with flexbox.
- **Explicit navigation links are correct for gamepad-first UI.** Spatial navigation
  is clever and unpredictable; a graph is tedious and always right. Given SWOS is
  played on a stick, this is not the place to be clever.
- **Eight content kinds was enough for a whole manager game.** When [PLAN.md](PLAN.md)
  §9 Phase 3 comes round, that is the scale to aim at — and the "one of them is a
  draw callback" escape hatch is what keeps the number that low.
