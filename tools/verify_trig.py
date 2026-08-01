#!/usr/bin/env python3
"""Re-derive trig tables and optionally diff against the reference port.

Run by hand when ../reference/swos-port is present. Not a CI gate — the reference
is not a checked-in dependency.
"""
from __future__ import annotations

import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
import gen_trig  # noqa: E402

REF = ROOT.parent / "reference" / "swos-port" / "src" / "sprites" / "updateSprite.cpp"


def parse_reference_tangent(text: str) -> list[list[int]]:
    m = re.search(
        r"kAngleTangent\[32\]\[32\]\s*=\s*\{(.*?)\};",
        text,
        re.S,
    )
    if not m:
        raise SystemExit("could not find kAngleTangent in reference")
    nums = [int(x) for x in re.findall(r"-?\d+", m.group(1))]
    if len(nums) != 1024:
        raise SystemExit(f"expected 1024 tangent entries, got {len(nums)}")
    return [nums[i * 32 : (i + 1) * 32] for i in range(32)]


def parse_reference_sine(text: str) -> list[int]:
    m = re.search(
        r"kSineCosineTable\s*=\s*\{(.*?)\};",
        text,
        re.S,
    )
    if not m:
        raise SystemExit("could not find kSineCosineTable in reference")
    nums = [int(x) for x in re.findall(r"-?\d+", m.group(1))]
    if len(nums) != 256:
        raise SystemExit(f"expected 256 sine entries, got {len(nums)}")
    return nums


def main() -> int:
    ours_t = gen_trig.tangent_table()
    ours_s = gen_trig.sine_table()
    print("self-check: tangent and sine regenerated OK")

    if not REF.is_file():
        print(f"reference not present at {REF}; skipping cross-check")
        return 0

    text = REF.read_text(encoding="utf-8", errors="ignore")
    ref_t = parse_reference_tangent(text)
    ref_s = parse_reference_sine(text)

    t_mism = sum(
        1
        for y in range(32)
        for x in range(32)
        if ours_t[y][x] != ref_t[y][x]
    )
    s_mism = sum(1 for i in range(256) if ours_s[i] != ref_s[i])
    s_hi_mism = sum(1 for i in range(256) if (ours_s[i] >> 8) != (ref_s[i] >> 8))

    print(f"tangent mismatches vs reference: {t_mism} (want 0)")
    print(f"sine mismatches vs reference:    {s_mism} (doc expects 14)")
    print(f"sine >>8 mismatches:             {s_hi_mism} (want 0)")

    if t_mism != 0 or s_hi_mism != 0:
        return 1
    if s_mism not in (0, 14):
        print("warning: sine mismatch count is not the documented 14")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
