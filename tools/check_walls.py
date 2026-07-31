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

def sources(d):
    p = ROOT / d
    if not p.exists():
        return []
    return [f for f in p.rglob("*")
            if f.suffix in (".h", ".hpp", ".c", ".cpp", ".cc")]

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
    text = f.read_text(encoding="utf-8", errors="ignore")
    for inc in INCLUDE.findall(text):
        if "imgui" in inc.lower():
            errors.append(
                f"{rel}: includes '{inc}' outside {ALLOWED_IMGUI_DIR} (wall 2)")

if errors:
    print("Architectural wall violations:\n")
    for e in errors:
        print("  " + e)
    print(f"\n{len(errors)} violation(s). See PLAN.md section 0.")
    sys.exit(1)

print("Walls OK.")
