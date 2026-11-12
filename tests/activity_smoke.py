import http.server
import json
from pathlib import Path
import socket
import socketserver
import struct
import subprocess
import sys
import threading
import time

exe = Path(sys.argv[1]).resolve()
cli = exe.with_name("vector-cli.exe" if sys.platform == "win32" else "vector-cli")
out = Path(sys.argv[2]).resolve()
out.mkdir(parents=True, exist_ok=True)


def stages(report):
    return {e["stage"]: e for e in report["activity"] if not e["item"]}


def check_trace(report, finished=True):
    events = report["activity"]
    assert events, report
    times = [e["elapsed_ms"] for e in events]
    assert times == sorted(times), "Concurrent event timestamps must be ordered"
    assert all(0 <= e["completed"] <= e["total"] for e in events)
    if finished:
        assert stages(report)["report"]["state"] == "complete"
        latest = {(e["stage"], e["item"]): e for e in events}
        assert not any(e["state"] == "running" for e in latest.values())


def capture(name, *args):
    path, report_path = out / f"{name}.bmp", out / f"{name}.json"
    subprocess.run([str(exe), "--screenshot", str(path), "--report-json", str(report_path), *args], check=True, timeout=25)
    data = path.read_bytes()
    assert data[:2] == b"BM" and len(data) > 100000
    width, height = struct.unpack_from("<ii", data, 18)
    assert width >= 1000 and abs(height) >= 700
    subprocess.run(["ffmpeg", "-y", "-i", str(path), str(out / f"{name}.png"), "-loglevel", "error"], check=True)
    report = json.loads(report_path.read_text())
    check_trace(report, report["status"] != "running")
    return report


class Web(http.server.BaseHTTPRequestHandler):
    server_version = "Caddy"
    sys_version = ""

    def log_message(self, *_): pass

    def do_GET(self):
        if self.path.startswith("/slow"):
            time.sleep(1.3)
        body = b'<title>Activity fixture</title><script src="/_next/static/chunks/app.js"></script>'
        if self.path == "/script-page":
            body = b'<title>Script fixture</title><script src="/slow-script.js"></script>'
        self.send_response(200)
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        try:
            self.wfile.write(body)
        except (BrokenPipeError, ConnectionResetError, ConnectionAbortedError):
            pass


class Silent(socketserver.BaseRequestHandler):
    def handle(self):
        time.sleep(1.8)


class EmptyHTTP(socketserver.BaseRequestHandler):
    def handle(self):
        self.request.settimeout(2)
        try:
            self.request.recv(2048)
        except (TimeoutError, ConnectionError):
            pass


class Server(socketserver.ThreadingTCPServer):
    daemon_threads = True


web = http.server.ThreadingHTTPServer(("127.0.0.1", 0), Web)
broken = Server(("127.0.0.1", 0), EmptyHTTP)
servers = [Server(("127.0.0.1", 0), Silent) for _ in range(6)]
for server in [web, broken, *servers]:
    threading.Thread(target=server.serve_forever, daemon=True).start()
try:
    target = f"http://127.0.0.1:{web.server_port}"
    selected_ports = ",".join(str(s.server_address[1]) for s in servers)
    report = capture("activity-services", "--target", "127.0.0.1", "--ports", selected_ports, "--autorun", "--capture-stage", "services")
    assert report["status"] == "running" and stages(report)["services"]["state"] == "running"
    assert len({e["item"] for e in report["activity"] if e["item"] and e["stage"] == "services"}) == 6
    report = capture("activity-web", "--target", target + "/slow", "--autorun", "--capture-stage", "web")
    assert stages(report)["web"]["state"] == "running"
    report = capture("activity-compact", "--target", target + "/slow/" + "long-segment/" * 10,
                     "--autorun", "--capture-stage", "web", "--width", "1000", "--height", "700")
    assert stages(report)["web"]["state"] == "running"
    capture("activity-selected", "--target", target + "/slow", "--autorun", "--capture-stage", "web", "--activity-stage", "1")
    report = capture("activity-complete", "--target", target, "--autorun", "--activity-open", "--width", "1000", "--height", "700")
    assert report["status"] == "complete"
    assert stages(report)["resolve"]["state"] == stages(report)["services"]["state"] == stages(report)["web"]["state"] == "complete"
    assert stages(report)["discovery"]["state"] == "skipped"
    report = capture("activity-cancelled", "--target", "127.0.0.1", "--ports", selected_ports, "--autorun", "--cancel-after", "250", "--activity-open")
    assert report["status"] == "cancelled"
    assert stages(report)["services"]["state"] == "cancelled"
    assert stages(report)["services"]["completed"] == 0, "Interrupted response checks are not completed checks"
    assert stages(report)["web"]["state"] == stages(report)["discovery"]["state"] == "cancelled"
    assert any(e["item"] and e["state"] == "cancelled" for e in report["activity"])
    report = capture("activity-failed", "--target", "vector-activity-fixture.invalid", "--autorun", "--activity-open")
    assert report["status"] == "error" and stages(report)["resolve"]["state"] == "failed"
    assert stages(report)["services"]["state"] == "skipped"
    report = capture("activity-discovery", "--target", "127.0.0.2", "--autorun", "--capture-stage", "discovery")
    assert stages(report)["discovery"]["state"] == "running"
    assert any(e["stage"] == "discovery" and e["item"] for e in report["activity"])
    assert not any("within the time limit" in e["detail"] for e in report["activity"] if e["stage"] == "discovery")
    report = capture("activity-scripts", "--target", target + "/script-page", "--autorun", "--capture-stage", "web")
    latest = {e["item"]: e for e in report["activity"] if e["stage"] == "web" and e["item"]}
    page = next(e for item, e in latest.items() if item.endswith("/script-page"))
    assert "Response received; analyzing" in page["detail"]
    assert any(item.startswith("Script ") and e["state"] == "running" for item, e in latest.items())
    result = subprocess.run([str(cli), "--target", target], capture_output=True, text=True, timeout=20, check=True)
    report = json.loads(result.stdout)
    check_trace(report)
    assert stages(report)["web"]["state"] == "complete"
    with socket.socket() as unused:
        unused.bind(("127.0.0.1", 0))
        closed_port = unused.getsockname()[1]
    result = subprocess.run([str(cli), "--target", f"127.0.0.1:{closed_port}", "--timeout", "4000"], capture_output=True, text=True, timeout=20, check=True)
    report = json.loads(result.stdout)
    check_trace(report)
    assert stages(report)["web"]["state"] == "skipped"
    result = subprocess.run([str(cli), "--target", f"http://127.0.0.1:{broken.server_address[1]}"], capture_output=True, text=True, timeout=20, check=True)
    report = json.loads(result.stdout)
    check_trace(report)
    assert len(report["web"]) == 4
    assert all(item["status"] == 0 and item["error"] for item in report["web"])
    assert stages(report)["web"]["detail"] == "0 HTTP responses collected; 4 requests failed"
finally:
    for server in [web, broken, *servers]:
        server.shutdown()
        server.server_close()

print("12 activity scenarios passed: 9 native captures, live events, discovery, script analysis, cancellation, failures, and CLI traces.")
