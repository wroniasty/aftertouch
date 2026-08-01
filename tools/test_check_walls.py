#!/usr/bin/env python3
"""Tests for check_walls.py.

A wall check that has never been seen to fail is indistinguishable from one that
cannot fail. These build a throwaway tree per case, run the real checker over it,
and assert on the exit code -- including the negative cases, where the offending
token appears only in a comment or a string literal and must NOT be reported.
"""
import pathlib
import subprocess
import sys
import tempfile

HERE = pathlib.Path(__file__).resolve().parent
CHECKER = HERE / "check_walls.py"

CASES = [
    # (name, {relative path: contents}, should_fail)
    ("clean core", {
        "src/core/include/core/a.hpp": "#pragma once\nint f();\n",
    }, False),

    ("float in core", {
        "src/core/src/a.cpp": "float speed = 1;\n",
    }, True),

    ("double in core", {
        "src/core/src/a.cpp": "double g();\n",
    }, True),

    ("cmath in core", {
        "src/core/src/a.cpp": "#include <cmath>\n",
    }, True),

    ("chrono in core", {
        "src/core/src/a.cpp": "#include <chrono>\nauto t = std::chrono::steady_clock::now();\n",
    }, True),

    ("rand in core", {
        "src/core/src/a.cpp": "int r = rand();\n",
    }, True),

    ("SDL in core", {
        "src/core/src/a.cpp": "#include <SDL3/SDL.h>\n",
    }, True),

    ("imgui outside its directory", {
        "src/app/render/a.cpp": "#include <imgui.h>\n",
    }, True),

    ("imgui inside its directory", {
        "src/app/ui_imgui/a.cpp": "#include <imgui.h>\n",
    }, False),

    # The false-positive cases. These are why the scanner strips comments and
    # string literals: prose about floating point is not floating point.
    ("float in a line comment", {
        "src/core/src/a.cpp": "// no float, no double, ever\nint f();\n",
    }, False),

    ("float in a block comment", {
        "src/core/src/a.cpp": "/* rand() and time() are banned\n   and so is double */\nint f();\n",
    }, False),

    ("float in a string literal", {
        "src/core/src/a.cpp": 'const char* k = "float";\n',
    }, False),

    # ...and the case that proves stripping does not swallow real code that
    # follows a comment on the same line.
    ("float after a comment on the same line", {
        "src/core/src/a.cpp": "int a; /* fine */ float b;\n",
    }, True),
]


def run_case(name, files, should_fail):
    with tempfile.TemporaryDirectory() as tmp:
        root = pathlib.Path(tmp)
        for rel, contents in files.items():
            p = root / rel
            p.parent.mkdir(parents=True, exist_ok=True)
            p.write_text(contents, encoding="utf-8")

        proc = subprocess.run([sys.executable, str(CHECKER), str(root)],
                              capture_output=True, text=True)
        failed = proc.returncode != 0

        if failed != should_fail:
            want = "a violation" if should_fail else "no violations"
            print(f"FAIL  {name}: expected {want}, got exit {proc.returncode}")
            print("      " + proc.stdout.strip().replace("\n", "\n      "))
            return False
        print(f"ok    {name}")
        return True


def main():
    results = [run_case(*c) for c in CASES]
    bad = results.count(False)
    print(f"\n{len(results) - bad}/{len(results)} passed")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
