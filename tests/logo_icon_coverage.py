from __future__ import annotations

import json
import sys
from pathlib import Path

root = Path(__file__).resolve().parents[1]
manifest = json.loads((root / "src" / "assets" / "technology" / "manifest.json").read_text(encoding="utf-8"))
aliases: dict[str, str] = {}
for icon in manifest["icons"]:
    key = icon["key"]
    for name in icon.get("names", []):
        aliases[name.casefold().strip()] = key

DEFAULT_NAMES = [
    "OpenSSH",
    "Ubuntu Linux",
    "Ubuntu OpenSSH package",
    "nginx",
    "PHP",
    "WordPress",
    "jQuery",
    "HTTP header order",
    "Amazon CloudFront",
    "Microsoft 365 mail",
    "Cloudflare DNS",
    "Microsoft 365 mail authorization",
    "Microsoft Clarity",
    "Stripe",
    "Google tagging",
    "Intercom",
    "Google Fonts",
    "Unattributed origin",
]


def check(names: list[str]) -> tuple[list[tuple[str, str]], list[str]]:
    ok, miss = [], []
    for name in names:
        key = aliases.get(name.casefold().strip())
        if key:
            ok.append((name, key))
        else:
            miss.append(name)
    return ok, miss


def main() -> int:
    names = list(DEFAULT_NAMES)
    if len(sys.argv) > 1:
        report = json.loads(Path(sys.argv[1]).read_text(encoding="utf-8"))
        names = []
        for f in report.get("findings", []):
            names.append(f.get("name", ""))
        for v in report.get("vendors", []):
            names.append(v.get("name", ""))
        names = [n for n in names if n]

    ok, miss = check(names)
    print(f"aliases={len(aliases)} checked={len(names)} ok={len(ok)} miss={len(miss)}")
    for name, key in ok:
        print(f"OK  {name} -> {key}")
    for name in miss:
        print(f"MISS {name}")
    out = root / ".impeccable" / "review" / "logo-icons" / "coverage.json"
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(json.dumps({"ok": ok, "miss": miss}, indent=2), encoding="utf-8")
    print(f"wrote {out}")
    return 1 if miss else 0


if __name__ == "__main__":
    raise SystemExit(main())
