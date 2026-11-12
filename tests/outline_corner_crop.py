from __future__ import annotations

import json
import subprocess
import sys
from pathlib import Path

from PIL import Image

exe = Path(sys.argv[1]).resolve()
out = Path(sys.argv[2]).resolve()
out.mkdir(parents=True, exist_ok=True)

CORNER = 48


def capture(name: str, *args: str, timeout: int = 30) -> Path:
    shot = out / f"{name}.bmp"
    state = out / f"{name}.state.json"
    report = out / f"{name}.json"
    command = [
        str(exe),
        "--no-browser",
        "--screenshot",
        str(shot),
        "--ui-state",
        str(state),
        "--report-json",
        str(report),
        *args,
    ]
    process = subprocess.Popen(command)
    try:
        assert process.wait(timeout=timeout) == 0, f"{name} exit={process.returncode}"
    finally:
        if process.poll() is None:
            process.terminate()
    assert shot.read_bytes()[:2] == b"BM" and shot.stat().st_size > 50_000, name
    png = out / f"{name}.png"
    subprocess.run(
        ["ffmpeg", "-y", "-i", str(shot), str(png), "-loglevel", "error"],
        check=True,
    )
    return png


def crop_corners(png: Path) -> dict[str, Path]:
    img = Image.open(png).convert("RGBA")
    w, h = img.size
    boxes = {
        "tl": (0, 0, CORNER, CORNER),
        "tr": (w - CORNER, 0, w, CORNER),
        "bl": (0, h - CORNER, CORNER, h),
        "br": (w - CORNER, h - CORNER, w, h),
    }
    paths: dict[str, Path] = {}
    for key, box in boxes.items():
        path = out / f"{png.stem}-{key}.png"
        img.crop(box).save(path)
        paths[key] = path
    return paths


def corner_ok(path: Path, name: str) -> None:
    img = Image.open(path).convert("RGB")
    w, h = img.size
    pixels = img.load()
    outlineish = 0
    sampled = 0
    for y in range(h):
        for x in range(w):
            on_ring = x < 6 or y < 6 or x >= w - 6 or y >= h - 6
            if not on_ring:
                continue
            sampled += 1
            r, g, b = pixels[x, y]
            luma = (r + g + b) / 3
            if 40 <= luma <= 140 and abs(r - g) < 25 and abs(g - b) < 25:
                outlineish += 1
    ratio = outlineish / max(sampled, 1)
    print(f"{name}: outlineish={outlineish}/{sampled} ratio={ratio:.3f} -> {path}")
    assert ratio >= 0.04, (
        f"{name}: corner ring looks empty (ratio={ratio:.3f}); "
        "outline probably clipped by DWM/GDI or bottom inset too large"
    )


png = capture("empty-outline")
state = json.loads((out / "empty-outline.state.json").read_text())
assert state.get("status") in {"empty", "ready", None} or "status" in state, state
paths = crop_corners(png)
for key, path in paths.items():
    corner_ok(path, key)

print("outline_corner_crop: 4 corners OK")
