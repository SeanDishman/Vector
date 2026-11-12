import http.server
import json
from pathlib import Path
import subprocess
import sys
import threading
import uuid

exe = Path(sys.argv[1]).resolve()
out = Path(sys.argv[2]).resolve()
out.mkdir(parents=True, exist_ok=True)
name = "Runtime Provider " + uuid.uuid4().hex
table = '<h1>Subprocessors</h1><table><tr><th>Company</th><th>Purpose</th></tr><tr><td>' + name + '</td><td>Storage</td></tr></table>'

class Handler(http.server.BaseHTTPRequestHandler):
    def log_message(self, *_): pass
    def do_GET(self):
        path = self.path.split("?")[0]
        body = {
            "/": '<a href="/trust">Trust center</a>',
            "/trust": '<div id="records"></div><script>setTimeout(()=>document.getElementById("records").innerHTML=' + json.dumps(table) + ',150)</script>',
            "/dynamic-links": '<div id="links"></div><script>setTimeout(()=>document.getElementById("links").innerHTML=\'<a href="/document/873">Subprocessors</a>\',150)</script>',
            "/document/873": table,
        }.get(path, "Not found").encode()
        self.send_response(404 if body == b"Not found" else 200)
        self.send_header("Content-Type", "text/html")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        try: self.wfile.write(body)
        except (BrokenPipeError, ConnectionResetError, ConnectionAbortedError): pass

server = http.server.ThreadingHTTPServer(("127.0.0.1", 0), Handler)
threading.Thread(target=server.serve_forever, daemon=True).start()
try:
    for label, path in (("rendered-provider", "/"), ("rendered-link", "/dynamic-links")):
        result = subprocess.run([str(exe), "--target", f"http://127.0.0.1:{server.server_port}{path}",
                                 "--browser-timeout", "4500", "--timeout", "150", "--http-timeout", "700"],
                                capture_output=True, text=True, encoding="utf-8", timeout=40)
        assert result.returncode == 0, result.stderr[-2000:]
        report = json.loads(result.stdout)
        (out / (label + ".json")).write_text(result.stdout, encoding="utf-8")
        if report["browser"]["state"] == "unavailable":
            raise SystemExit(77)
        providers = [v for v in report["vendors"] if v.get("published_by")]
        assert [v["name"] for v in providers] == [name], (label, report["disclosures"])
        assert any("Discovery:" in e["value"] for e in providers[0]["evidence"]), providers
        assert report["web_coverage"]["page_requests"] <= 12
        if label == "rendered-provider":
            assert any(p["purpose"] == "disclosure" and p["javascript_enabled"] for p in report["browser"]["pages"])
        else:
            assert any(d["state"] == "collected" and d["url"].endswith("/document/873") for d in report["disclosures"])
        print(label + ": passed")
finally:
    server.shutdown()
    server.server_close()
