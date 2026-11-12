import http.server
import json
from pathlib import Path
import subprocess
import sys
import threading
import uuid

exe = Path(sys.argv[1]).resolve()
suffix = uuid.uuid4().hex
names = [f"Fresh {suffix} {i}" for i in range(11)]
cards = (
    f"<article><h3>{names[0]}</h3><p>Purpose: Managed queues</p></article>"
    f"<ul><li><b>{names[1]}</b><p>Service: Mail delivery</p></li></ul>"
    f"<dl><dt>{names[2]}</dt><dd><p>Location: Canada</p></dd></dl>"
    f"<div><img alt='{names[3]}'><div><div>{names[3]}</div><p>Storage infrastructure services.</p></div></div>"
    f"<ul><li><strong>{names[4]}</strong>: Event delivery</li></ul>"
)
panels = ("<button id='provider-tab' role='tab'>Subprocessors</button>"
          "<div role='tabpanel' aria-labelledby='provider-tab'>" + cards + "</div>"
          "<h2>Subprocessors</h2><table><tr><th>Entity</th><th>Purpose for Processing</th></tr>"
          f"<tr><td>{names[5]}</td><td>Hosting</td></tr></table>"
          "<table><tr><th>Sub-processor Name</th><th>Processing Activities</th></tr>"
          f"<tr><td>{names[6]}</td><td>Support services</td></tr></table>"
          "<header><div>"
          f"<div><div><p>{names[7]}</p></div><div><p>Infrastructure hosting</p></div></div>"
          f"<div><div><p>{names[8]}</p></div><div><p>Event processing</p></div></div>"
          "</div></header><h1>Subprocessors</h1><h2>Entity Name &amp; Location</h2>"
          f"<table><tr><th>Entity</th><th>Location</th></tr><tr><td>{names[9]}</td><td>Canada</td></tr></table>"
          "<h1>Subprocessors</h1><h2></h2>"
          f"<table><tr><th>Subprocessor</th><th>Purpose</th></tr><tr><td>{names[10]}</td><td>Queues</td></tr></table>")


class Handler(http.server.BaseHTTPRequestHandler):
    def log_message(self, *_):
        pass

    def do_GET(self):
        status, body = 200, ""
        if self.path == "/subprocessors":
            body = panels
        elif self.path == "/subprocessors-denied":
            status, body = 403, panels
        elif self.path == "/subprocessors-hidden":
            body = "<div hidden>" + panels + "</div><h2>Customers</h2>" + cards
        else:
            status = 404
        data = body.encode()
        self.send_response(status)
        self.send_header("Content-Type", "text/html; charset=utf-8")
        self.send_header("Content-Length", str(len(data)))
        self.end_headers()
        self.wfile.write(data)


server = http.server.ThreadingHTTPServer(("127.0.0.1", 0), Handler)
threading.Thread(target=server.serve_forever, daemon=True).start()
try:
    for path in ("/subprocessors", "/subprocessors-denied", "/subprocessors-hidden"):
        target = f"http://127.0.0.1:{server.server_port}{path}"
        completed = subprocess.run([str(exe), "--target", target, "--web-only", "--no-browser",
                                    "--timeout", "200", "--http-timeout", "500"],
                                   capture_output=True, text=True, encoding="utf-8", timeout=15)
        assert completed.returncode == 0, completed.stderr
        report = json.loads(completed.stdout)
        published = [v for v in report["vendors"] if v.get("published_by")]
        assert not any(f["name"] in names for f in report["findings"]), report["findings"]
        if path == "/subprocessors":
            assert {v["name"] for v in published} == set(names), published
            for v in published:
                assert v["evidence_level"] == "Published disclosure" and v["recorded_at"], v
                assert any(e["source"] == target and "Subprocessors" in e["value"] and v["name"] in e["value"]
                           for e in v["evidence"]), v
        else:
            assert not published, published
    print("Three loopback structured-disclosure scenarios passed with runtime-generated provider names.")
finally:
    server.shutdown()
    server.server_close()
