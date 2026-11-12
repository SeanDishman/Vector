import http.server
import json
from pathlib import Path
import struct
import subprocess
import sys
import threading

exe = str(Path(sys.argv[1]).resolve())
out = Path(sys.argv[2]).resolve()
out.mkdir(parents=True, exist_ok=True)

def capture(name, *args):
    path = out / f"{name}.bmp"
    process = subprocess.run([exe, "--screenshot", str(path), *args], timeout=30)
    assert process.returncode == 0, (name, process.returncode)
    data = path.read_bytes()
    assert data[:2] == b"BM" and len(data) > 100000, name
    width, height = struct.unpack_from("<ii", data, 18)
    assert width >= 1000 and abs(height) >= 700
    subprocess.run(["ffmpeg", "-y", "-i", str(path), str(out / f"{name}.png"), "-loglevel", "error"], check=True)

class Fixture(http.server.BaseHTTPRequestHandler):
    server_version = "Caddy"
    sys_version = ""
    def log_message(self, *_): pass
    def do_GET(self):
        body = b'<title>Vector loopback fixture</title><script src="/_next/static/chunks/app.js"></script>'
        self.send_response(200)
        self.send_header("Content-Type", "text/html")
        self.send_header("X-Powered-By", "Next.js")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

capture("desktop", "--demo")
capture("compact", "--demo", "--width", "1000", "--height", "700")
capture("empty")
capture("services", "--demo", "--view", "0")
capture("evidence", "--demo", "--view", "2", "--width", "1000", "--height", "700")
capture("help", "--help-view")
capture("invalid", "--target", "file:///not-a-network-target", "--autorun")
capture("filter-match", "--demo", "--filter", "PHP")
capture("filter-empty", "--demo", "--filter", "no-such-finding")
web = http.server.ThreadingHTTPServer(("127.0.0.1", 0), Fixture)
threading.Thread(target=web.serve_forever, daemon=True).start()
try:
    target = f"http://127.0.0.1:{web.server_port}/"
    report = out / "live-report.json"
    capture("live", "--target", target, "--autorun", "--report-json", str(report))
    result = json.loads(report.read_text())
    assert result["status"] == "complete"
    assert {"Caddy", "Next.js"} <= {f["name"] for f in result["findings"]}
    report = out / "cancelled-report.json"
    capture("cancelled", "--target", target, "--autorun", "--cancel-after", "100", "--report-json", str(report))
    assert json.loads(report.read_text())["status"] == "cancelled"
finally:
    web.shutdown()
    web.server_close()
print("11 native GUI scenarios passed; captures and real loopback reports saved.")
