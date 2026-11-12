import json
from pathlib import Path
import subprocess
import sys
import tempfile
import time
import resource_coverage_integration as fixture

exe, out = Path(sys.argv[1]).resolve(), Path(sys.argv[2]).resolve()
out.mkdir(parents=True, exist_ok=True)
server = fixture.start()
try:
    for name, args in (("resource-overview", ()), ("resource-compact", ("--width", "1000", "--height", "700")), ("resource-software", ("--view", "1"))):
        report, bmp = out / (name + ".json"), out / (name + ".bmp")
        proc = subprocess.Popen([str(exe), "--target", f"http://127.0.0.1:{server.server_port}/fast", "--autorun", "--screenshot", str(bmp), "--report-json", str(report), *args])
        assert proc.wait(timeout=40) == 0
        data = json.loads(report.read_text())
        assert set(fixture.paths.values()) <= {f["name"] for f in data["findings"]}
        assert any(s["collection"] == "browser" and s["analyzed"] and s["body_bytes"] == 2 * 1024 * 1024 and s["body_truncated"] for s in data["scripts"])
        assert not list(Path(tempfile.gettempdir()).glob(f"vector-browser-{proc.pid}-*"))
        subprocess.run(["ffmpeg", "-y", "-i", str(bmp), str(out / (name + ".png")), "-loglevel", "error"], check=True)
    report, bmp = out / "resource-cancelled.json", out / "resource-cancelled.bmp"
    began = time.monotonic()
    proc = subprocess.Popen([str(exe), "--target", f"http://127.0.0.1:{server.server_port}/slow", "--autorun", "--screenshot", str(bmp), "--report-json", str(report), "--cancel-after", "5000"])
    assert proc.wait(timeout=15) == 0
    data = json.loads(report.read_text())
    assert data["status"] == "cancelled" and data["browser"]["state"] == "cancelled"
    assert any(r["url"].endswith("/bundle-slow.js") for r in data["browser"]["requests"])
    assert time.monotonic() - began < 9
    assert not list(Path(tempfile.gettempdir()).glob(f"vector-browser-{proc.pid}-*"))
    print("Four native resource scenarios passed: overview, compact, software evidence and active streaming cancellation.")
finally:
    fixture.stop.set(); server.shutdown()
