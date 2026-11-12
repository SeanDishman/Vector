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
def click(x, y, at=450):
    return [{"at": at, "kind": "focus"}, {"at": at+40, "kind": "move", "x": x, "y": y},
            {"at": at+80, "kind": "down"}, {"at": at+180, "kind": "up"}]

def key(code, at):
    return [{"at": at, "kind": "key-down", "key": code},
            {"at": at+80, "kind": "key-up", "key": code}]

def enter(value):
    return click(400, 374)+[{"at": 800, "kind": "text", "value": value}]+key(13, 1000)

def capture(name, *args, actions=None):
    shot, state, report = (out / (name + ext) for ext in (".bmp", ".state.json", ".json"))
    command = [str(exe), "--no-browser", "--screenshot", str(shot), "--ui-state", str(state), "--report-json", str(report), *args]
    if actions:
        script = out / (name + ".actions.json")
        script.write_text(json.dumps(actions))
        command += ["--capture-delay", "1800", "--ui-actions", str(script)]
    process = subprocess.Popen(command)
    try:
        assert process.wait(timeout=30) == 0, name
    finally:
        if process.poll() is None:
            process.terminate()
    assert shot.read_bytes()[:2] == b"BM" and shot.stat().st_size > 100000
    subprocess.run(["ffmpeg", "-y", "-i", str(shot), str(out / (name + ".png")), "-loglevel", "error"], check=True)
    return json.loads(state.read_text()), json.loads(report.read_text()) if report.exists() else None

class Web(http.server.BaseHTTPRequestHandler):
    server_version = "Caddy"
    sys_version = ""
    def log_message(self, *_): pass
    def do_GET(self):
        if self.path.startswith("/slow"):
            time.sleep(2)
        body = b'<title>Native UI fixture</title><script src="https://js.stripe.com/v3"></script>'
        self.send_response(200)
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Content-Type", "text/html")
        self.send_header("X-Powered-By", "Next.js")
        self.end_headers()
        try:
            self.wfile.write(body)
        except (BrokenPipeError, ConnectionResetError, ConnectionAbortedError):
            pass

capture("ready")
state, _ = capture("overview", "--demo")
assert state["show_all"] and state["navigation_visible"] and state["tab"] == 4
capture("overview-compact", "--demo", "--width", "1000", "--height", "700")
for index, (name, tab) in enumerate((("overview", 4), ("software", 1), ("services", 0),
                                    ("vendors", 3), ("evidence", 2), ("activity", 5))):
    state, _ = capture("navigate-" + name, "--demo", "--view", "1", actions=click(110, 157+46*index))
    assert state["navigation_visible"] and state["tab"] == tab, state
state, _ = capture("new-search", "--demo", actions=click(1066, 27))
assert state["status"] == "empty" and not state["navigation_visible"], state
capture("vendors", "--demo", "--view", "3")
capture("vendors-compact", "--demo", "--view", "3", "--width", "1000", "--height", "700")
capture("evidence", "--demo", "--view", "2", "--width", "1000", "--height", "700")
state, _ = capture("filter-empty", "--demo", "--filter", "no-such-software")
assert state["selected_finding"] == -1
state, _ = capture("input-error", actions=enter("file:///invalid"))
assert state["error"] and state["status"] == "empty", state
web = http.server.ThreadingHTTPServer(("127.0.0.1", 0), Web)
threading.Thread(target=web.serve_forever, daemon=True).start()
try:
    target = f"http://127.0.0.1:{web.server_port}"
    state, report = capture("enter-inspection", actions=enter(target))
    assert report["status"] == "complete" and state["history_count"] == 0, state
    assert state["navigation_visible"] and state["tab"] == 4, state
    assert {"Caddy", "Next.js"} <= {f["name"] for f in report["findings"]}
    assert any(v["name"] == "Stripe" for v in report["vendors"])
    state, report = capture("live-overview", "--target", target + "/slow", "--autorun", "--capture-stage", "web")
    assert state["running"] and state["show_all"] and not state["navigation_visible"] and report["activity"]
    state, report = capture("live-all", "--target", target + "/slow", "--autorun", "--capture-stage", "web", "--view", "5")
    assert state["running"] and state["show_all"]
    state, report = capture("cancel", actions=enter(target + "/slow")+key(27, 1300))
    assert report["status"] == "cancelled" and state["navigation_visible"] and state["tab"] == 4, state
finally:
    web.shutdown()
    web.server_close()
print("19 native navigation scenarios passed: default Overview, every section, New search, compact views, input error, Enter and Escape cancellation.")
