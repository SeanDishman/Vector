from __future__ import annotations

import argparse
import re
from pathlib import Path

SKIP_DIR_PARTS = {
    "third_party", "build", "_deps", ".git", "node_modules",
    "__pycache__", "reports", ".impeccable", "build-animated-outline",
}

def should_skip(path: Path) -> bool:
    parts = {p.lower() for p in path.parts}
    if parts & SKIP_DIR_PARTS:
        return True
    if path.suffix.lower() in {".md", ".svg", ".json", ".png", ".bmp", ".ico", ".pem"}:
        return True
    return False

def strip_cpp_style(text: str) -> str:
    out: list[str] = []
    i = 0
    n = len(text)
    while i < n:
        ch = text[i]
        if ch == "/" and i + 1 < n and text[i + 1] == "/":
            j = i
            while j < n and text[j] not in "\r\n":
                j += 1
            while out and out[-1] in " \t":
                out.pop()
            i = j
            continue
        if ch == "/" and i + 1 < n and text[i + 1] == "*":
            out.append(ch)
            i += 1
            continue
        if ch in "\"'":
            quote = ch
            out.append(ch)
            i += 1
            if quote == '"' and out[-2:] == ['R', '"'] or (len(out) >= 2 and out[-1] == '"' and out[-2] == "R"):
                pass
            while i < n:
                c = text[i]
                out.append(c)
                if c == "\\" and i + 1 < n:
                    out.append(text[i + 1])
                    i += 2
                    continue
                if c == quote:
                    i += 1
                    break
                i += 1
            continue
        out.append(ch)
        i += 1
    lines = "".join(out).splitlines(keepends=True)
    cleaned: list[str] = []
    for line in lines:
        if line.strip() == "":
            if cleaned and cleaned[-1].strip() == "":
                continue
            cleaned.append("\n" if line.endswith("\n") else line)
        else:
            cleaned.append(line)
    return "".join(cleaned)

def strip_file_simple(text: str) -> str:
    out: list[str] = []
    i = 0
    n = len(text)
    while i < n:
        if text[i] == "R" and i + 1 < n and text[i + 1] == '"':
            m = re.match(r'R"([^\s()]*)\(', text[i:])
            if m:
                delim = m.group(1)
                start = m.end()
                end_token = ")" + delim + '"'
                end = text.find(end_token, i + start)
                if end != -1:
                    out.append(text[i : end + len(end_token)])
                    i = end + len(end_token)
                    continue
        ch = text[i]
        if ch in "\"'":
            quote = ch
            out.append(ch)
            i += 1
            while i < n:
                c = text[i]
                out.append(c)
                if c == "\\" and quote != "'" and i + 1 < n:
                    out.append(text[i + 1])
                    i += 2
                    continue
                if c == quote:
                    i += 1
                    break
                i += 1
            continue
        if ch == "/" and i + 1 < n and text[i + 1] == "/":
            if out and out[-1] == ":":
                out.append("//")
                i += 2
                continue
            while out and out[-1] in " \t":
                out.pop()
            while i < n and text[i] not in "\r\n":
                i += 1
            continue
        out.append(ch)
        i += 1
    result_lines: list[str] = []
    blank_run = 0
    for line in "".join(out).splitlines(keepends=True):
        if line.strip() == "":
            blank_run += 1
            if blank_run <= 1:
                result_lines.append("\n" if "\n" in line or "\r" in line else "")
            continue
        blank_run = 0
        if line.endswith("\r\n"):
            result_lines.append(line[:-2].rstrip() + "\r\n")
        elif line.endswith("\n"):
            result_lines.append(line[:-1].rstrip() + "\n")
        else:
            result_lines.append(line.rstrip())
    return "".join(result_lines)

def process(path: Path, dry_run: bool) -> tuple[bool, int]:
    original = path.read_text(encoding="utf-8", errors="surrogateescape")
    if "//" not in original:
        return False, 0
    updated = strip_file_simple(original)
    if updated == original:
        return False, 0
    removed = original.count("//") - updated.count("//")
    if not dry_run:
        path.write_text(updated, encoding="utf-8", errors="surrogateescape", newline="")
    return True, max(removed, 0)

def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("roots", nargs="+", type=Path)
    ap.add_argument("--dry-run", action="store_true")
    ap.add_argument("--ext", default=".cpp,.hpp,.h,.c,.cc,.cxx,.cmake,.ps1,.py,.mjs")
    args = ap.parse_args()
    exts = {e if e.startswith(".") else f".{e}" for e in args.ext.split(",")}
    changed = 0
    total_removed = 0
    for root in args.roots:
        root = root.resolve()
        files = [root] if root.is_file() else sorted(p for p in root.rglob("*") if p.is_file())
        for path in files:
            if should_skip(path):
                continue
            if path.suffix.lower() not in exts and path.name not in {"CMakeLists.txt"}:
                continue
            ok, rem = process(path, args.dry_run)
            if ok:
                changed += 1
                total_removed += rem
                print(f"{'DRY ' if args.dry_run else ''}{path}")
    print(f"files={changed} approx_//_removed={total_removed}")

if __name__ == "__main__":
    main()
