#!/usr/bin/env python3
"""gop_targets.py - find the GOP coefficient giving a wanted M/D split, by
bisection rather than by sweeping.

The problem. A frame's type is decided in video.cc:120:

    frameZeros(i) <= H x W x C_gop   ->  M-frame
    otherwise                        ->  D-frame

C_gop therefore cannot be read as a proportion: it has to be tried to find out
how many M-frames it produces. gopCoefGenerate.sh answered that by sweeping
C_gop from 0.001 to 1 in steps of 0.001 - up to a thousand full encodings,
which is hours and can be days.

What this script does instead. The split is monotone in C_gop: raising it can
only turn D-frames into M-frames. Verified against the existing mobile_qcif
measurements - 1, 5, 9 then 12 M-frames for C_gop at 0.001, 0.011, 0.019 and
0.03. A monotone step function is characterised by its breakpoints, and finding
them by bisection costs on the order of log2(1/precision) evaluations per step
instead of an exhaustive sweep. Measured: 16 encodings resolved all twelve
levels of mobile_qcif.

Evaluations are cached and shared across targets, so asking for one M-frame and
then for 25 % does not recompute what they have in common.

The search encodes at P_max = 1, the cheapest setting, because frame typing does
not depend on the number of priority levels.

Usage. Targets are expressed as a percentage of M-frames, which does not depend
on how many frames the video holds:

    python3 tools/gop_targets.py -ip encoder.dat -vf video.avi -od /tmp/gopsearch
        -> 0, 25, 50, 75, 100 % M-frames (default)

    python3 tools/gop_targets.py ... -t 25,50,100
    python3 tools/gop_targets.py ... -t 3f,7f       # frame counts, f suffix
    python3 tools/gop_targets.py ... -t all         # every reachable step

One caveat to read before interpreting the output. On N frames only multiples of
1/N exist: on twelve frames you can have 25.0 % (three frames) or 33.3 % (four),
never 30 %. The script aims at the nearest step and prints what it actually
reached, never what was asked for.

The total frame count is read from st-frame.dat rather than computed as
imagesNb * FPS / 25, so this holds whatever the video contains.

Output: a table of target -> C_gop -> actual split, and the matching ipFile
lines, ready to paste.
"""

import argparse
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

STUDY = Path(__file__).resolve().parent.parent   # tools/ sits under experiments/
EVALVSN = STUDY.parent / "evalvsn" / "bin" / "evalvsn"

PRECISION = 1e-4  # below this, two C_gop values give the same split


class Encoder:
    """Evaluate the M/D split for a given C_gop, with caching."""

    def __init__(self, ip: Path, video: Path, workdir: Path):
        self.template = self._read_template(ip)
        self.video = video
        self.workdir = workdir
        self.cache: dict[float, tuple[int, int]] = {}
        self.calls = 0

    @staticmethod
    def _read_template(ip: Path) -> tuple[str, list[str]]:
        header, first = "", None
        for line in ip.read_text().splitlines():
            if line.startswith("#"):
                header = line
            elif line.strip() and first is None:
                first = line.split()
        if first is None:
            sys.exit(f"no configuration line in {ip}")
        return header, first

    def _ipfile(self, gop: float, path: Path) -> None:
        header, fields = self.template
        f = list(fields)
        f[0] = "search"          # simId
        f[4] = f"{gop:.6f}"      # gopCoef
        f[6] = "1"               # priorityNb: frame typing does not depend on it
        path.write_text(header + "\n" + " ".join(f) + "\n")

    def compose(self, gop: float) -> tuple[int, int]:
        key = round(gop, 6)
        if key in self.cache:
            return self.cache[key]

        with tempfile.TemporaryDirectory() as tmp:
            ip = Path(tmp) / "enc.dat"
            self._ipfile(gop, ip)
            out = self.workdir / f"g{key:.6f}"
            out.mkdir(parents=True, exist_ok=True)
            subprocess.run([str(EVALVSN), "-ip", str(ip), "-vf", str(self.video),
                            "-od", str(out)],
                           stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
                           check=False)
            self.calls += 1

            frame_files = list(out.glob("*/st-frame.dat"))
            if not frame_files:
                sys.exit(f"st-frame.dat not found after encoding at C_gop={gop}")
            m = d = 0
            for line in frame_files[0].read_text().splitlines():
                parts = line.split()
                if len(parts) >= 2 and not line.startswith("#"):
                    if parts[1] == "M":
                        m += 1
                    elif parts[1] == "D":
                        d += 1
            shutil.rmtree(out, ignore_errors=True)

        self.cache[key] = (m, d)
        return m, d


def breakpoints(enc: Encoder, lo: float, hi: float) -> dict[int, float]:
    """The smallest C_gop reaching each M-frame count, by bisection.

    A naive search would rerun the encoder for every target. Here the interval is
    subdivided only where the split changes, and the cache is shared, so
    neighbouring steps pay for one another.
    """
    found: dict[int, float] = {}

    def visit(a: float, b: float) -> None:
        ma, _ = enc.compose(a)
        mb, _ = enc.compose(b)
        found.setdefault(ma, a)
        found.setdefault(mb, b)
        # Nothing new between the two, or the interval is exhausted.
        if mb - ma <= 1 or (b - a) < PRECISION:
            return
        mid = (a + b) / 2
        visit(a, mid)
        visit(mid, b)

    visit(lo, hi)
    # The smallest C_gop of each step, refined
    for m in sorted(found):
        found[m] = min(g for g, (mm, _) in
                       ((g, enc.cache[g]) for g in enc.cache) if mm == m)
    return found


def parse_targets(spec: str, total: int) -> list[tuple[str, int]]:
    """Return a list of (requested label, target M-frame count).

    The label is kept as requested so the output can set intent against result:
    asking for 30 % on twelve frames gives four frames, that is 33.3 %, and that
    gap has to be visible.
    """
    if spec.strip() == "all":
        return [(f"{100 * m / total:.1f} %", m) for m in range(0, total + 1)]

    out: list[tuple[str, int]] = []
    for tok in spec.split(","):
        tok = tok.strip().rstrip("%")
        if not tok:
            continue
        if tok.endswith("f"):                       # 3f = three M-frames
            n = int(tok[:-1])
            out.append((f"{n} frame(s)", max(0, min(total, n))))
        else:                                       # otherwise, a percentage
            pct = float(tok)
            out.append((f"{pct:g} %", max(0, min(total, round(total * pct / 100)))))
    return out


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("-ip", required=True, type=Path, help="reference encoder file")
    ap.add_argument("-vf", required=True, type=Path, help=".avi video")
    ap.add_argument("-od", required=True, type=Path, help="working directory")
    ap.add_argument("-t", "--targets", default="0,25,50,75,100",
                    help="percentages of M-frames (default 0,25,50,75,100); "
                         "f suffix for a frame count; or 'all'")
    ap.add_argument("--lo", type=float, default=0.0005)
    ap.add_argument("--hi", type=float, default=0.05)
    args = ap.parse_args()

    if not EVALVSN.exists():
        sys.exit(f"binary not found: {EVALVSN}")
    args.od.mkdir(parents=True, exist_ok=True)

    enc = Encoder(args.ip, args.vf, args.od)

    print(f"Searching between C_gop = {args.lo} and {args.hi}.")
    print("The split is monotone in C_gop; we look for its breakpoints.\n")

    table = breakpoints(enc, args.lo, args.hi)
    total = max(m + d for m, d in enc.cache.values())
    targets = parse_targets(args.targets, total)

    print(f"{total} frames encoded: the reachable steps are the multiples of "
          f"{100 / total:.1f} %.\n")
    print(f"{'asked':>10} {'C_gop':>9} | {'M-frames':>8} {'D-frames':>8} | "
          f"{'% M':>6} {'% D':>6}   gap")
    print("-" * 66)

    lines = []
    for label, want in targets:
        # The exact step may not exist on the interval searched; we then take
        # the nearest one, and say so rather than pass it over.
        if want in table:
            got = want
        elif table:
            got = min(table, key=lambda m: (abs(m - want), m))
        else:
            print(f"{label:>10}   no step found on the interval")
            continue

        gop = table[got]
        m, d = enc.compose(gop)
        pct_m = 100 * m / (m + d)
        # The gap is read against what was asked for, in points.
        asked = float(label.rstrip(" %")) if label.endswith("%") else 100 * want / total
        gap = pct_m - asked
        note = "exact" if abs(gap) < 0.05 else f"{gap:+.1f} pt"
        print(f"{label:>10} {gop:>9.5f} | {m:>8} {d:>8} | "
              f"{pct_m:>5.1f}% {100 - pct_m:>5.1f}%   {note}")
        lines.append((gop, m, d, pct_m))

    print(f"\n{enc.calls} encoding(s), where a sweep in steps of 0.001 would "
          f"need up to {int((args.hi - args.lo) / 0.001)}.")

    if lines:
        header, fields = enc.template
        print("\nMatching ipFile lines:")
        for gop, m, d, pct_m in lines:
            f = list(fields)
            f[4] = f"{gop:.5f}"
            print(f"  {' '.join(f)}    # {m} M / {d} D  ({pct_m:.1f} % M)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
