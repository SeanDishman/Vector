import http.server
import json
from pathlib import Path
import subprocess
import sys
import threading

exe, out = Path(sys.argv[1]).resolve(), Path(sys.argv[2]).resolve()
out.mkdir(parents=True, exist_ok=True)


class Page(http.server.BaseHTTPRequestHandler):
    def log_message(self, *_):
        pass

    def do_GET(self):
        body = b'''<meta name="generator" content="Docusaurus v3.9.2">
<script src="https://cdn.usefathom.com/script.js?key=PRIVATE"></script>
<script src="https://plausible.io/js/script.js"></script>
<script src="https://cdn.cookielaw.org/scripttemplates/otSDKStub.js"></script>
<script src="https://www.google.com/recaptcha/api.js"></script>
<script src="https://snap.licdn.com/li.lms-analytics/insight.min.js"></script>'''
        self.send_response(200)
        self.send_header("Content-Type", "text/html")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)


server = http.server.ThreadingHTTPServer(("127.0.0.1", 0), Page)
threading.Thread(target=server.serve_forever, daemon=True).start()
try:
    for name, view, width in (("overview", 4, 1280), ("fathom-evidence", 3, 1000)):
        bmp, report, state = [out / (name + suffix) for suffix in (".bmp", ".json", ".state.json")]
        command = [str(exe), "--target", f"http://127.0.0.1:{server.server_port}/", "--no-browser", "--autorun",
                   "--view", str(view), "--width", str(width), "--height", "800" if width == 1280 else "700",
                   "--screenshot", str(bmp), "--report-json", str(report), "--ui-state", str(state)]
        if view == 3:
            command.extend(("--vendor-filter", "Fathom"))
        subprocess.run(command, check=True, timeout=25)
        r = json.loads(report.read_text())
        assert {v["name"] for v in r["vendors"]} >= {"Fathom Analytics", "Plausible Analytics", "OneTrust", "reCAPTCHA", "LinkedIn Insight Tag"}
        assert any(f["name"] == "Docusaurus" and f["version"] == "3.9.2" for f in r["findings"])
        fathom = next(v for v in r["vendors"] if v["name"] == "Fathom Analytics")
        assert fathom["evidence_level"] == "Page reference"
        assert any("attribution path: /script.js" in e["value"] for e in fathom["evidence"])
        assert "PRIVATE" not in json.dumps(r["vendors"]), "Provider evidence exposed a query value"
        assert json.loads(state.read_text())["tab"] == view
        assert bmp.read_bytes()[:2] == b"BM"
        subprocess.run(["ffmpeg", "-y", "-i", str(bmp), str(out / (name + ".png")), "-loglevel", "error"], check=True)
    print("Two native SDL scenarios passed: new named entries and retained path evidence at two sizes.")
finally:
    server.shutdown()
    server.server_close()
