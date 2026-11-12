import http.server
import json
from pathlib import Path
import subprocess
import sys
import threading
import time

exe = Path(sys.argv[1]).resolve()
out = Path(sys.argv[2]).resolve()
out.mkdir(parents=True, exist_ok=True)

GEAR_X, GEAR_Y = 1153, 26
NEW_SEARCH_X, NEW_SEARCH_Y = 1071, 26


def click(x, y, at=450):
    return [{"at": at, "kind": "focus"}, {"at": at + 40, "kind": "move", "x": x, "y": y},
            {"at": at + 80, "kind": "down"}, {"at": at + 180, "kind": "up"}]


def capture(name, *args, actions=None, delay=1800, timeout=45):
    shot, state, report = (out / (name + ext) for ext in (".bmp", ".state.json", ".json"))
    command = [str(exe), "--no-browser", "--screenshot", str(shot), "--ui-state", str(state),
               "--report-json", str(report), *args]
    if actions:
        script = out / (name + ".actions.json")
        script.write_text(json.dumps(actions))
        command += ["--capture-delay", str(delay), "--ui-actions", str(script)]
    process = subprocess.Popen(command)
    try:
        assert process.wait(timeout=timeout) == 0, f"{name} exit={process.returncode}"
    finally:
        if process.poll() is None:
            process.terminate()
    assert shot.read_bytes()[:2] == b"BM" and shot.stat().st_size > 100000, name
    subprocess.run(["ffmpeg", "-y", "-i", str(shot), str(out / (name + ".png")), "-loglevel", "error"], check=True)
    return json.loads(state.read_text()), json.loads(report.read_text()) if report.exists() else None


def crop_header(name, left=980):
    src = out / f"{name}.png"
    dst = out / f"{name}-header.png"
    subprocess.run(["ffmpeg", "-y", "-i", str(src), "-vf", f"crop=300:56:{left}:0",
                    str(dst), "-loglevel", "error"], check=True)
    return dst


class Web(http.server.BaseHTTPRequestHandler):
    server_version = "Caddy"
    sys_version = ""

    def log_message(self, *_):
        pass

    def do_GET(self):
        if self.path.startswith("/slow"):
            time.sleep(3)
        body = b'<title>Header chrome fixture</title>'
        self.send_response(200)
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Content-Type", "text/html")
        self.end_headers()
        try:
            self.wfile.write(body)
        except (BrokenPipeError, ConnectionResetError, ConnectionAbortedError):
            pass


failures = []

state, _ = capture("empty")
assert state["status"] == "empty", state
crop_header("empty", left=1080)
print("empty gear crop ok")

state, _ = capture("report", "--demo")
assert state.get("navigation_visible"), state
crop_header("report", left=980)
print("report header crop ok")

state, _ = capture("new-search-click", "--demo", actions=click(NEW_SEARCH_X, NEW_SEARCH_Y))
if state["status"] != "empty" or state.get("navigation_visible"):
    failures.append(f"new-search-click -> {state}")
else:
    print("new-search click -> empty ok")

web = http.server.ThreadingHTTPServer(("127.0.0.1", 0), Web)
threading.Thread(target=web.serve_forever, daemon=True).start()
try:
    target = f"http://127.0.0.1:{web.server_port}/slow"
    state, report = capture(
        "settings-mid-run",
        "--target", target, "--autorun", "--capture-stage", "web",
        actions=click(GEAR_X, GEAR_Y, at=200),
        delay=2500,
        timeout=60,
    )
    if not state.get("running"):
        failures.append(f"settings-mid-run not running: {state}")
    elif not state.get("settings_open"):
        failures.append(f"settings-mid-run gear click missed: settings_open={state.get('settings_open')} controls={state.get('proxy_controls')}")
    else:
        print("settings mid-run open + still running ok")
        back = next((c for c in state.get("proxy_controls", []) if c.get("key") == "back"), None)
        if not back:
            failures.append("settings-mid-run missing proxy_controls back")
        else:
            bx, by = int(back["x"]), int(back["y"])
            state2, _ = capture(
                "settings-back",
                "--target", target, "--autorun", "--capture-stage", "web",
                actions=click(GEAR_X, GEAR_Y, at=200) + click(bx, by, at=900),
                delay=2800,
                timeout=60,
            )
            if not state2.get("running"):
                failures.append(f"settings-back cancelled run: {state2}")
            elif state2.get("settings_open"):
                failures.append(f"settings-back still open: {state2}")
            else:
                print("settings Back keeps running ok")
    crop_header("settings-mid-run", left=1080)
finally:
    web.shutdown()
    web.server_close()

if failures:
    print("FAILED:")
    for f in failures:
        print(" -", f)
    sys.exit(1)
print("header chrome validate passed")
