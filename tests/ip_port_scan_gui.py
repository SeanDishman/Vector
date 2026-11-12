import json
from pathlib import Path
import subprocess
import sys
from ip_port_scan_integration import Fixture, tcp_rows, validate_deep

exe, out = Path(sys.argv[1]).resolve(), Path(sys.argv[2]).resolve()
out.mkdir(parents=True, exist_ok=True)
fixture = Fixture()


def capture(name, target, *extra):
    shot, report, state = (out / (name + suffix) for suffix in (".bmp", ".json", ".state.json"))
    subprocess.run([str(exe), "--no-browser", "--target", target, "--autorun", "--screenshot", str(shot),
                    "--report-json", str(report), "--ui-state", str(state), *extra], check=True, timeout=95)
    assert shot.read_bytes()[:2] == b"BM" and shot.stat().st_size > 100000
    subprocess.run(["ffmpeg", "-y", "-i", str(shot), str(out / (name + ".png")), "-loglevel", "error"], check=True)
    return json.loads(report.read_text()), json.loads(state.read_text())


try:
    report, state = capture("ip-complete", fixture.address, "--view", "0")
    validate_deep(report)
    assert state["status"] == "complete" and not state["running"]
    report, state = capture("ip-live", fixture.address, "--capture-stage", "services")
    assert state["running"] and report["status"] == "running"
    stages = [e for e in report["activity"] if e["stage"] == "services" and not e["item"]]
    assert stages and stages[-1]["total"] > 1024
    report, state = capture("endpoint-compact", fixture.address + ":81", "--width", "1000", "--height", "700")
    assert report["tcp_coverage"]["profile"] == "endpoint" and set(tcp_rows(report)) == {81}
    assert state["status"] == "complete" and state["navigation_visible"]
    print("Three native SDL scenarios passed: deep IP result, live port count and explicit endpoint at compact size.")
finally:
    fixture.close()
