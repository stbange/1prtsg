#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Generates manifest.json listing loose external files for zero-baked-assets Emscripten Web loading."""

import json
from pathlib import Path


def main() -> None:
    root = Path(__file__).resolve().parent.parent
    files: list[str] = []
    for folder in ("config", "assets", "scripts", "mods"):
        base = root / folder
        if not base.exists():
            continue
        for p in sorted(base.rglob("*")):
            if p.is_file():
                files.append(p.relative_to(root).as_posix())
    (root / "manifest.json").write_text(json.dumps({"files": files}, indent=2) + "\n", encoding="utf-8")
    print(f"Wrote manifest.json with {len(files)} external files.")


if __name__ == "__main__":
    main()
