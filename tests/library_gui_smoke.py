import http.server
import json
from pathlib import Path
import subprocess
import sys
import threading

from library_integration import Web, validate

exe = Path(sys.argv[1]).resolve()
out = Path(sys.argv[2]).resolve()
out.mkdir(parents=True, exist_ok=True)


class Unknown(http.server.BaseHTTPRequestHandler):
    def log_message(self, *_):
        pass

    def do_GET(self):
        body = b"const originalCode=()=>42;" if self.path == "/private.js" else b'<script src="/private.js"></script>'
        self.send_response_only(200)
        self.send_header("Content-Type", "application/javascript" if self.path == "/private.js" else "text/html")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)


def capture(name, handler, *args):
    server = http.server.ThreadingHTTPServer(("127.0.0.1", 0), handler)
    threading.Thread(target=server.serve_forever, daemon=True).start()
    bmp, report = out / (name + ".bmp"), out / (name + ".json")
    try:
        command = [str(exe), "--no-browser", "--target", f"http://127.0.0.1:{server.server_port}/", "--autorun",
                   "--screenshot", str(bmp), "--report-json", str(report), *args]
        result = subprocess.run(command, timeout=30, capture_output=True, text=True)
        assert result.returncode == 0, (name, result.stderr)
        data = json.loads(report.read_text())
        if handler is Web:
            validate(data)
        else:
            assert data["scripts"][0]["analyzed"]
            assert data["findings"] and all(f["category"] == "Fingerprint" for f in data["findings"])
        assert bmp.read_bytes()[:2] == b"BM"
        subprocess.run(["ffmpeg", "-y", "-i", str(bmp), str(out / (name + ".png")), "-loglevel", "error"], check=True)
    finally:
        server.shutdown()
        server.server_close()


capture("libraries-overview", Web)
capture("libraries-compact", Web, "--width", "1000", "--height", "700")
capture("libraries-evidence", Web, "--view", "1")
capture("libraries-coverage", Web, "--view", "2", "--width", "1000", "--height", "700")
capture("libraries-unknown", Unknown, "--width", "1000", "--height", "700")
print("Five native library scenarios passed: overview, compact, evidence, coverage, and unnamed bundles.")
