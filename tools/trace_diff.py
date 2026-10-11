#!/usr/bin/env python3
"""Compare two per-tick trace CSVs (simref `trace` vs C++ `s3trace`).

usage: trace_diff.py <reference.csv> <candidate.csv> [--tol 1e-4] [--columns N] [--quiet]

Reports:
  * the first tick whose row differs at all (exact text compare), with the first differing column;
  * the first tick where a numeric column exceeds the tolerance, or a text column differs;
  * per numeric column: max |a - b| (and the tick where it occurs), first tick with any difference;
  * per text column: first differing tick and both values.
Exit status 0 when every row is textually identical, 1 otherwise.
"""
import csv
import math
import sys


def parse_float(s):
    try:
        return float(s)
    except ValueError:
        return None


def main(argv):
    args = [a for a in argv[1:]]
    tol = 1e-4
    limit = 40
    quiet = False
    if "--tol" in args:
        i = args.index("--tol")
        tol = float(args[i + 1])
        del args[i : i + 2]
    if "--columns" in args:
        i = args.index("--columns")
        limit = int(args[i + 1])
        del args[i : i + 2]
    if "--quiet" in args:
        args.remove("--quiet")
        quiet = True
    if len(args) != 2:
        print(__doc__)
        return 2
    with open(args[0], newline="") as f:
        ref = list(csv.reader(f))
    with open(args[1], newline="") as f:
        cand = list(csv.reader(f))
    if not ref or not cand:
        print("empty input")
        return 2
    header = ref[0]
    if cand[0] != header:
        print("HEADER DIFFERS")
        for i, (a, b) in enumerate(zip(header, cand[0])):
            if a != b:
                print(f"  column {i}: {a!r} vs {b!r}")
        if len(header) != len(cand[0]):
            print(f"  lengths {len(header)} vs {len(cand[0])}")
        return 1
    rows_ref, rows_cand = ref[1:], cand[1:]
    n = min(len(rows_ref), len(rows_cand))
    if len(rows_ref) != len(rows_cand):
        print(f"row counts differ: reference {len(rows_ref)}, candidate {len(rows_cand)} (comparing {n})")

    numeric = []
    for c, name in enumerate(header):
        vals = [r[c] for r in rows_ref[: max(n, 1)] if c < len(r)]
        numeric.append(all(parse_float(v) is not None for v in vals) and name not in ("buttons",))

    first_text = None  # (tick, column)
    first_tol = None
    max_err = {}  # col -> (err, tick)
    first_diff_col = {}  # col -> tick
    text_diffs = {}
    identical_rows = 0
    for t in range(n):
        a, b = rows_ref[t], rows_cand[t]
        if a == b:
            identical_rows += 1
            continue
        for c, name in enumerate(header):
            va = a[c] if c < len(a) else ""
            vb = b[c] if c < len(b) else ""
            if va == vb:
                continue
            if first_text is None:
                first_text = (t, name, va, vb)
            first_diff_col.setdefault(c, t)
            if numeric[c]:
                fa, fb = parse_float(va), parse_float(vb)
                if fa is None or fb is None:
                    err = math.inf
                elif math.isnan(fa) and math.isnan(fb):
                    err = 0.0
                else:
                    err = abs(fa - fb)
                    if math.isnan(err):
                        err = math.inf
                if err > max_err.get(c, (-1.0, 0))[0]:
                    max_err[c] = (err, t)
                if err > tol and first_tol is None:
                    first_tol = (t, name, va, vb, err)
            else:
                text_diffs.setdefault(c, (t, va, vb))
                if first_tol is None:
                    first_tol = (t, name, va, vb, None)

    print(f"rows compared: {n}; identical rows: {identical_rows}")
    if first_text is None:
        print("IDENTICAL")
        return 0 if len(rows_ref) == len(rows_cand) else 1
    t, name, va, vb = first_text
    print(f"first difference: tick {rows_ref[t][0]} (row {t}) column {name}: ref={va} cand={vb}")
    if first_tol is not None:
        t, name, va, vb, err = first_tol
        e = "text" if err is None else f"|err|={err:.3g}"
        print(f"first beyond tol {tol:g}: tick {rows_ref[t][0]} column {name}: ref={va} cand={vb} ({e})")
    else:
        print(f"all numeric differences within tol {tol:g}, no text differences")
    if not quiet:
        if text_diffs:
            print("text columns (first difference):")
            for c in sorted(text_diffs, key=lambda c: text_diffs[c][0]):
                t, va, vb = text_diffs[c]
                print(f"  {header[c]:>14}: tick {rows_ref[t][0]}: ref={va!r} cand={vb!r}")
        if max_err:
            print(f"numeric columns (max |err|, tick of max, first differing tick), worst {limit}:")
            order = sorted(max_err, key=lambda c: -max_err[c][0])
            for c in order[:limit]:
                err, t = max_err[c]
                print(f"  {header[c]:>14}: max {err:.6g} at tick {rows_ref[t][0]}; first diff tick "
                      f"{rows_ref[first_diff_col[c]][0]}")
    return 1


if __name__ == "__main__":
    sys.exit(main(sys.argv))
