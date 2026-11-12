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
    run = subprocess.run([exe, "--no-browser", "--view", "3", "--screenshot", str(path), *args], timeout=35)
    assert run.returncode == 0, name
    raw = path.read_bytes()
    assert raw[:2] == b"BM" and len(raw) > 100000
    width, height = struct.unpack_from("<ii", raw, 18)
    assert width >= 1000 and abs(height) >= 700
    subprocess.run(["ffmpeg", "-y", "-i", str(path), str(out / f"{name}.png"), "-loglevel", "error"], check=True)

capture("vendors-desktop", "--demo")
capture("vendors-compact", "--demo", "--width", "1000", "--height", "700")
capture("vendors-policy", "--demo", "--vendor-filter", "Clarity")
capture("vendors-unknown", "--demo", "--vendor-filter", "forms.example")
capture("vendors-inline", "--demo", "--vendor-filter", "Intercom")
capture("vendors-filter-empty", "--demo", "--vendor-filter", "not-a-provider")
capture("vendors-known-only", "--demo", "--known-vendors-only")

class Page(http.server.BaseHTTPRequestHandler):
    def log_message(self, *_): pass
    def do_GET(self):
        body = b'<title>Loopback</title>' if self.path == '/empty' else b'<script src="https://js.stripe.com/v3"></script>'
        self.send_response(200)
        self.send_header("Content-Type", "text/html")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

web = http.server.ThreadingHTTPServer(("127.0.0.1", 0), Page)
threading.Thread(target=web.serve_forever, daemon=True).start()
try:
    target = f"http://127.0.0.1:{web.server_port}"
    report = out / "vendors-live-report.json"
    capture("vendors-live", "--target", target, "--autorun", "--report-json", str(report))
    data = json.loads(report.read_text())
    assert data["status"] == "complete" and any(v["name"] == "Stripe" for v in data["vendors"])
    capture("vendors-empty", "--target", target + "/empty", "--autorun")
    capture("vendors-cancelled", "--target", target, "--autorun", "--cancel-after", "100")
finally:
    web.shutdown(); web.server_close()
print("10 vendor GUI captures and live vendor report passed.")
