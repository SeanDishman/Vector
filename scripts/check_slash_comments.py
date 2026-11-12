#!/usr/bin/env python3
from __future__ import annotations

import argparse
import sys
from pathlib import Path

ROOTS = ("src", "tests", "scripts")
EXTS = {".cpp", ".hpp", ".h", ".c", ".cc", ".cxx", ".inl", ".py", ".ps1", ".cmake"}
SKIP_PARTS = {"build", "_deps", ".git", "node_modules", "third_party", "__pycache__", ".impeccable"}


def should_skip(path: Path) -> bool:
    return any(part in SKIP_PARTS for part in path.parts)


def scan_cpp_text(text: str) -> list[tuple[int, str]]:
    hits: list[tuple[int, str]] = []
    in_str = False
    in_char = False
    in_block = False
    raw_delim: str | None = None
    lines = text.splitlines()
    for lineno, line in enumerate(lines, 1):
        i = 0
        n = len(line)
        hit_at: int | None = None
        while i < n:
            ch = line[i]
            if in_block:
                end = line.find("*/", i)
                if end < 0:
                    i = n
                    continue
                i = end + 2
                in_block = False
                continue
            if raw_delim is not None:
                end = f'){raw_delim}"'
                at = line.find(end, i)
                if at < 0:
                    i = n
                    continue
                i = at + len(end)
                raw_delim = None
                continue
            if in_str:
                if ch == "\\" and i + 1 < n:
                    i += 2
                    continue
                if ch == '"':
                    in_str = False
                i += 1
                continue
            if in_char:
                if ch == "\\" and i + 1 < n:
                    i += 2
                    continue
                if ch == "'":
                    in_char = False
                i += 1
                continue
            if ch == '"' and i >= 1 and line[i - 1] == "R":
                j = i + 1
                while j < n and line[j] != "(":
                    j += 1
                if j < n and line[j] == "(":
                    raw_delim = line[i + 1 : j]
                    i = j + 1
                    continue
            if ch == '"':
                in_str = True
                i += 1
                continue
            if ch == "'":
                in_char = True
                i += 1
                continue
            if ch == "/" and i + 1 < n and line[i + 1] == "*":
                in_block = True
                i += 2
                continue
            if ch == "/" and i + 1 < n and line[i + 1] == "/":
                if i > 0 and line[i - 1] == ":":
                    i += 2
                    continue
                hit_at = i
                break
            i += 1
        if hit_at is not None:
            snippet = line.strip()
            if len(snippet) > 140:
                snippet = snippet[:137] + "..."
            hits.append((lineno, snippet))
    return hits


def scan_file(path: Path) -> list[tuple[int, str]]:
    if path.suffix.lower() in {".py", ".ps1"}:
        return []
    try:
        text = path.read_text(encoding="utf-8", errors="replace")
    except OSError:
        return []
    return scan_cpp_text(text)


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("roots", nargs="*", default=list(ROOTS))
    ap.add_argument("--limit", type=int, default=200)
    args = ap.parse_args()
    root = Path.cwd()
    all_hits: list[tuple[str, int, str]] = []
    for name in args.roots:
        base = root / name
        if not base.exists():
            continue
        for path in sorted(base.rglob("*")):
            if not path.is_file() or path.suffix.lower() not in EXTS or should_skip(path):
                continue
            if path.suffix.lower() == ".md":
                continue
            for lineno, snippet in scan_file(path):
                rel = path.relative_to(root).as_posix()
                all_hits.append((rel, lineno, snippet))
    shown = all_hits[: args.limit]
    for rel, lineno, snippet in shown:
        print(f"{rel}:{lineno}: {snippet}")
    print(f"residual_comment_hits={len(all_hits)} shown={len(shown)}")
    return 1 if all_hits else 0


if __name__ == "__main__":
    sys.exit(main())
