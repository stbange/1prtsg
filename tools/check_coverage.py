#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Parse gcov JSON coverage for rts_core C sources and enforce >= 95% line coverage."""

import argparse
import gzip
import json
import os
import subprocess
import sys
from pathlib import Path


def collect_coverage(build_dir: Path, source_root: Path, min_line: float) -> int:
    gcno_files = sorted(build_dir.rglob("*.gcno"))
    # Filter to rts_core object directory only
    core_gcno = [p for p in gcno_files if "rts_core.dir" in str(p)]
    if not core_gcno:
        print(f"ERROR: No .gcno files found for rts_core under {build_dir}", file=sys.stderr)
        return 2

    file_lines: dict[str, dict[int, int]] = {}
    tmp_dir = build_dir / "gcov_json"
    tmp_dir.mkdir(parents=True, exist_ok=True)

    for gcno in core_gcno:
        cmd = ["gcov", "-j", str(gcno.resolve())]
        res = subprocess.run(cmd, cwd=tmp_dir, capture_output=True, text=True)
        if res.returncode != 0:
            print(f"gcov failed on {gcno}:\n{res.stderr}", file=sys.stderr)
            return 2

    src_prefix = str((source_root / "src").resolve())
    inc_prefix = str((source_root / "include").resolve())

    for gz_path in sorted(tmp_dir.glob("*.gcov.json.gz")):
        with gzip.open(gz_path, "rt", encoding="utf-8") as fh:
            data = json.load(fh)
        for fentry in data.get("files", []):
            fname = os.path.realpath(fentry["file"])
            if not (fname.startswith(src_prefix) or fname.startswith(inc_prefix)):
                continue
            rel = os.path.relpath(fname, source_root)
            if rel not in file_lines:
                file_lines[rel] = {}
            for line in fentry.get("lines", []):
                ln = int(line["line_number"])
                cnt = int(line["count"])
                file_lines[rel][ln] = file_lines[rel].get(ln, 0) + cnt

    if not file_lines:
        print("ERROR: No covered source files found in gcov JSON output", file=sys.stderr)
        return 2

    total_exec = 0
    total_hit = 0
    print("\n=== C11 Core Engine Line Coverage Report ===")
    print(f"{'File':<36} {'Covered':>8} {'Total':>8} {'Coverage':>10}")
    print("-" * 66)
    for rel in sorted(file_lines.keys()):
        lines = file_lines[rel]
        total = len(lines)
        hit = sum(1 for c in lines.values() if c > 0)
        pct = (100.0 * hit / total) if total > 0 else 100.0
        total_exec += total
        total_hit += hit
        print(f"{rel:<36} {hit:>8} {total:>8} {pct:>9.2f}%")
        if pct < min_line:
            missed = sorted(ln for ln, c in lines.items() if c == 0)
            preview = ", ".join(str(x) for x in missed[:20])
            suffix = " ..." if len(missed) > 20 else ""
            print(f"   missed lines: {preview}{suffix}")
    print("-" * 66)
    overall = (100.0 * total_hit / total_exec) if total_exec > 0 else 0.0
    print(f"{'TOTAL':<36} {total_hit:>8} {total_exec:>8} {overall:>9.2f}%")
    print(f"Required minimum line coverage: {min_line:.2f}%\n")

    if overall < min_line:
        print(f"FAIL: C coverage {overall:.2f}% is below {min_line:.2f}%", file=sys.stderr)
        return 1
    print(f"PASS: C coverage {overall:.2f}% >= {min_line:.2f}%")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--build-dir", required=True)
    parser.add_argument("--source-root", required=True)
    parser.add_argument("--min-line", type=float, default=95.0)
    args = parser.parse_args()
    return collect_coverage(Path(args.build_dir), Path(args.source_root), args.min_line)


if __name__ == "__main__":
    raise SystemExit(main())
