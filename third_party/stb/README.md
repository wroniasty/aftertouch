# stb_image

Vendored **as a file, not as a submodule** — a deliberate exception to
[PLAN.md](../../doc/PLAN.md) §2's "everything is a git submodule" rule.

| | |
|---|---|
| **What** | `stb_image.h`, the public-domain single-header image loader |
| **Version** | v2.30 |
| **Source** | https://raw.githubusercontent.com/nothings/stb/master/stb_image.h |
| **SHA-256** | `594c2fe35d49488b4382dbfaec8f98366defca819d916ac95becf3e75f4200b3` |
| **Licence** | MIT **or** public domain (Unlicense), at our choice — see the tail of the header |
| **Modifications** | None. Byte-identical to upstream. |

## Why a file and not a submodule

The rule in §2 exists so that a dependency update cannot silently change our physics
and so that Windows and macOS build from identical sources. A single header pinned by
SHA-256 satisfies both more directly than a submodule does: there is no floating branch
to track, and the exact bytes are in our history. The `nothings/stb` repository also has
no release tags to pin to, which is the mechanism §2 actually relies on.

Verify at any time with:

```
sha256sum third_party/stb/stb_image.h
```

If that hash ever changes, the change is deliberate or the file is wrong; there is no
third possibility.

## Where it is used

`src/tools/assetc/` only — the asset importer (A4). It decodes the PNGs of an extracted
source tree into indexed pixels at original resolution.

It is **not** linked into `at_core` (Wall 1 forbids it) and **not** linked into the game
binary. The importer is an offline tool; the runtime reads our own format, which needs
no image decoder. If `stb_image.h` ever appears in a target other than `assetc`, that is
a design change and not a build convenience.

Compiled with `STB_IMAGE_STATIC` and with the formats we do not read switched off, so
the loader surface exposed to third-party binary data is as small as it can be. The
importer is fuzzed against malformed input (A4 work item 8) precisely because this is
the one place a crash is likely and a silent misparse is worse
([PLAN.md](../../doc/PLAN.md) §7).
