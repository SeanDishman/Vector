import http.server
import json
from pathlib import Path
import subprocess
import sys
import threading

exe = str(Path(sys.argv[1]).resolve())
requests = []

class Destination(http.server.BaseHTTPRequestHandler):
    def log_message(self, *_): pass
    def do_GET(self):
        requests.append(self.path)
        self.send_response(200)
        self.end_headers()

destination = http.server.ThreadingHTTPServer(("127.0.0.1", 0), Destination)
threading.Thread(target=destination.serve_forever, daemon=True).start()

class Page(http.server.BaseHTTPRequestHandler):
    def log_message(self, *_): pass
    def do_GET(self):
        origin = f"http://127.0.0.1:{destination.server_port}"
        body = f'''<title>Vendor fixture</title>
          <script src="https://js.stripe.com/v3/?key=SHOULD_NOT_EXPORT"></script>
          <script>s.src='https://widget.intercom.io/widget/' + app;</script>
          <iframe src="{origin}/frame?private=SHOULD_NOT_EXPORT" sandbox></iframe>
          <form action="{origin}/submit"></form>
          <img src="{origin}/pixel">
          <link rel=preconnect href="{origin}">
          <script type="application/json">{{"fake":"https://js.hs-scripts.com/"}}</script>
        '''.encode()
        self.send_response(200)
        self.send_header("Content-Type", "text/html")
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Content-Security-Policy-Report-Only", "connect-src https://*.clarity.ms")
        self.send_header("CF-Ray", "illustrative-marker")
        self.end_headers()
        self.wfile.write(body)

page = http.server.ThreadingHTTPServer(("127.0.0.1", 0), Page)
threading.Thread(target=page.serve_forever, daemon=True).start()
try:
    run = subprocess.run([exe, "--no-browser", "--target", f"http://127.0.0.1:{page.server_port}/", "--web-only", "--timeout", "400"], text=True, capture_output=True, timeout=25)
    assert run.returncode == 0, run.stderr
    report = json.loads(run.stdout)
    rows = report["vendors"]
    assert {"Stripe", "Intercom", "Microsoft Clarity", "Cloudflare", "Unattributed origin"} <= {v["name"] for v in rows}
    assert "HubSpot" not in {v["name"] for v in rows}
    assert any(v["evidence_level"] == "CSP report-only" for v in rows)
    assert all(v["access_status"] == "unverified" and v["evidence"] for v in rows)
    assert "SHOULD_NOT_EXPORT" not in json.dumps(rows)
    assert not report["vendor_coverage"]["vendor_requests_observed"]
    assert requests == [], f"Unexpected requests to vendor fixture: {requests}"
    print("Vendor loopback: attributed and unknown relationships, export semantics and zero third-party requests passed.")
finally:
    page.shutdown(); page.server_close()
    destination.shutdown(); destination.server_close()
