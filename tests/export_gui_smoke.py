import json
from pathlib import Path
import subprocess
import sys
import shutil
import tempfile
from export_formats import leaves, read_csv, text_fields

exe, out = Path(sys.argv[1]).resolve(), Path(sys.argv[2]).resolve()
out.mkdir(parents=True, exist_ok=True)

def click(x, y, at=400):
    return [{"at": at, "kind": "focus"}, {"at": at+40, "kind": "move", "x": x, "y": y},
            {"at": at+100, "kind": "down"}, {"at": at+200, "kind": "up"}]

def key(code, at):
    return [{"at": at, "kind": "key-down", "key": code}, {"at": at+90, "kind": "key-up", "key": code}]

def capture(name, width, actions, *extra, executable=None):
    script = out / (name + ".actions.json")
    script.write_text(json.dumps(actions), encoding="utf-8")
    state, report, shot = (out / (name + ext) for ext in (".state.json", ".json", ".bmp"))
    command = [str(executable or exe), "--demo", "--width", str(width), "--height", "700" if width == 1000 else "800",
               "--screenshot", str(shot), "--ui-state", str(state), "--report-json", str(report),
               "--capture-delay", "2600", "--ui-actions", str(script), *extra]
    subprocess.run(command, check=True, timeout=20)
    subprocess.run(["ffmpeg", "-y", "-i", str(shot), str(out / (name + ".png")), "-loglevel", "error"], check=True)
    return json.loads(state.read_text(encoding="utf-8")), json.loads(report.read_text(encoding="utf-8"))

for width in (1280, 1000):
    state, _ = capture(f"menu-{width}", width, click(width-58, 110))
    assert not state["notice"] and not state["error"]
    for index, extension in enumerate(("json", "csv", "txt")):
        actions = click(width-58, 110) + key(1073741898, 850)
        for step in range(index):
            actions += key(1073741905, 1100 + step*180)
        actions += key(13, 1750)
        name = f"export-{extension}-{width}"
        extra = ("--filter", "no-matching-software") if extension == "txt" else ()
        state, report = capture(name, width, actions, *extra)
        assert not state["error"] and state["notice"].startswith("Saved: "), state
        saved = Path(state["notice"].removeprefix("Saved: "))
        assert saved.suffix == "." + extension, (name, saved)
        expected = dict(leaves(report))
        if extension == "json":
            assert json.loads(saved.read_text(encoding="utf-8")) == report
        elif extension == "csv":
            assert read_csv(saved) == expected
        else:
            assert text_fields(saved.read_text(encoding="utf-8")) == expected
            assert state["selected_finding"] == -1
        assert state["status"] == "demo" and not state["running"] and state["history_count"] == 0
failure_dir = Path(tempfile.mkdtemp(prefix="export-write-error-", dir=out))
shutil.copy2(exe, failure_dir / "Vector.exe")
(failure_dir / "reports").write_text("This fixture blocks directory creation.", encoding="utf-8")
state, _ = capture("write-error", 1000, click(942, 110) + key(1073741898, 850) + key(13, 1750),
                   executable=failure_dir / "Vector.exe")
assert state["error"] and not state["notice"] and state["status"] == "demo", state
print("Nine SDL export scenarios passed: both menu sizes, all formats, full data through filters, write failure, and no inspection started.")
