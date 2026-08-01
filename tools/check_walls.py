#!/usr/bin/env python3
"""Enforce the architectural walls described in PLAN.md section 0."""
import sys, pathlib, re

ROOT = pathlib.Path(sys.argv[1] if len(sys.argv) > 1 else ".").resolve()
INCLUDE = re.compile(r'^\s*#\s*include\s*[<"]([^>"]+)[>"]', re.M)

FORBIDDEN = [
    # (directory, list of forbidden include substrings, human explanation)
    ("src/core", ["SDL", "imgui", "app/"],
     "core must not depend on SDL, ImGui, or the shell (wall 1)"),
    ("src/app/ui",  ["imgui", "SDL"],
     "the UI interface must stay toolkit- and platform-agnostic"),
    ("src/app/screens", ["imgui"],
     "screen logic must not include imgui.h (wall 2)"),
]

ALLOWED_IMGUI_DIR = "src/app/ui_imgui"

# Wall 1 is not only about includes. Rule 1 also forbids floating point, the
# clock and unseeded randomness, and PLAN.md section 7 lists "no float anywhere
# in src/core/" as a wall check rather than a test -- a float that reaches the
# engine is not a bug that fails somewhere, it is a divergence that appears
# hundreds of ticks later on one platform only.
#
# See doc/implementation/A2-determinism-primitives.md section 2.7.
BANNED_TOKENS = [
    # (regex, human explanation)
    (r'\bfloat\b',        "floating point is forbidden in src/core (rule 1); use core/fixed.hpp"),
    (r'\bdouble\b',       "floating point is forbidden in src/core (rule 1); use core/fixed.hpp"),
    (r'\brand\s*\(',      "unseeded randomness is forbidden in src/core (rule 1); use core/rng.hpp"),
    (r'\bsrand\s*\(',     "unseeded randomness is forbidden in src/core (rule 1); use core/rng.hpp"),
    (r'\btime\s*\(',      "src/core may not read the clock (rule 1)"),
    (r'\bclock\s*\(',     "src/core may not read the clock (rule 1)"),
    (r'std::chrono',      "src/core may not read the clock (rule 1)"),
]

BANNED_INCLUDES_CORE = ["cmath", "math.h", "random", "chrono", "ctime", "time.h"]


def sources(d):
    p = ROOT / d
    if not p.exists():
        return []
    return [f for f in p.rglob("*")
            if f.suffix in (".h", ".hpp", ".c", ".cpp", ".cc")]


def strip_comments_and_strings(text):
    """Blank out comments and string/char literals, preserving line structure.

    Enough to keep the token scan free of false positives from prose without
    pretending to parse C++. Newlines are preserved so line numbers stay honest.
    """
    out = []
    i, n = 0, len(text)
    while i < n:
        c = text[i]
        two = text[i:i + 2]
        if two == "//":
            j = text.find("\n", i)
            j = n if j < 0 else j
            out.append(" " * (j - i))
            i = j
        elif two == "/*":
            j = text.find("*/", i + 2)
            j = n if j < 0 else j + 2
            out.append("".join(ch if ch == "\n" else " " for ch in text[i:j]))
            i = j
        elif c in "\"'":
            j = i + 1
            while j < n and text[j] != c:
                j += 2 if text[j] == "\\" else 1
            j = min(j + 1, n)
            out.append("".join(ch if ch == "\n" else " " for ch in text[i:j]))
            i = j
        else:
            out.append(c)
            i += 1
    return "".join(out)


errors = []

for directory, banned, why in FORBIDDEN:
    for f in sources(directory):
        text = f.read_text(encoding="utf-8", errors="ignore")
        for inc in INCLUDE.findall(text):
            for b in banned:
                if b.lower() in inc.lower():
                    errors.append(
                        f"{f.relative_to(ROOT)}: includes '{inc}' -- {why}")

# Wall 2, the other direction: imgui.h anywhere outside its one directory.
for f in sources("src"):
    rel = f.relative_to(ROOT).as_posix()
    if rel.startswith(ALLOWED_IMGUI_DIR):
        continue
    if rel.startswith("src/app/main.cpp"):
        continue   # TEMPORARY: remove once the UI backend interface lands
    # A3: trace_viewer is a tools/ GUI that overlays ATTR traces; same ImGui stack
    # as the shell, outside Wall 1 (src/core) by design.
    if rel.startswith("src/tools/trace_viewer/"):
        continue
    text = f.read_text(encoding="utf-8", errors="ignore")
    for inc in INCLUDE.findall(text):
        if "imgui" in inc.lower():
            errors.append(
                f"{rel}: includes '{inc}' outside {ALLOWED_IMGUI_DIR} (wall 2)")

# Wall 1, the determinism half: no floats, no clock, no unseeded randomness.
for f in sources("src/core"):
    rel = f.relative_to(ROOT).as_posix()
    raw = f.read_text(encoding="utf-8", errors="ignore")

    for inc in INCLUDE.findall(raw):
        if inc.strip().lower() in BANNED_INCLUDES_CORE:
            errors.append(
                f"{rel}: includes '{inc}' -- floating point, the clock and "
                f"unseeded randomness are forbidden in src/core (rule 1)")

    code = strip_comments_and_strings(raw)
    for pattern, why in BANNED_TOKENS:
        for m in re.finditer(pattern, code):
            line = code.count("\n", 0, m.start()) + 1
            errors.append(f"{rel}:{line}: '{m.group(0)}' -- {why}")

if errors:
    print("Architectural wall violations:\n")
    for e in errors:
        print("  " + e)
    print(f"\n{len(errors)} violation(s). See PLAN.md section 0.")
    sys.exit(1)

print("Walls OK.")
