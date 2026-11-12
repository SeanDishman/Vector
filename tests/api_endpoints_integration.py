import base64
import hashlib
import http.server
import json
from pathlib import Path
import subprocess
import sys
import threading
import time

exe = Path(sys.argv[1]).resolve()
hits = []
SECRETS = ("QUERY_SECRET", "BODY_SECRET", "SOCKET_SECRET", "FORM_SECRET", "INLINE_SECRET", "SCRIPT_SECRET")
OPENAPI = {"openapi": "3.0.3", "paths": {"/pets": {"get": {"operationId": "listPets"}},
                                         "/pets/{petId}": {"delete": {"operationId": "deletePet"}}}}


class Fixture(http.server.BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"

    def log_message(self, *_):
        pass

    def handle(self):
        try:
            super().handle()
        except (BrokenPipeError, ConnectionResetError, ConnectionAbortedError):
            pass

    def reply(self, status, mime, content):
        body = content.encode()
        self.send_response(status)
        self.send_header("Content-Type", mime)
        self.send_header("Access-Control-Allow-Origin", "*")
        self.send_header("Content-Length", str(len(body)))
        try:
            self.end_headers()
            self.wfile.write(body)
        except (BrokenPipeError, ConnectionResetError, ConnectionAbortedError):
            pass

    def do_GET(self):
        hits.append((self.command, self.server.server_port, self.path))
        path = self.path.split("?", 1)[0]
        if path == "/socket" and self.headers.get("Sec-WebSocket-Key"):
            accept = base64.b64encode(hashlib.sha1(
                (self.headers["Sec-WebSocket-Key"] + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11").encode()).digest()).decode()
            self.send_response(101, "Switching Protocols")
            self.send_header("Upgrade", "websocket")
            self.send_header("Connection", "Upgrade")
            self.send_header("Sec-WebSocket-Accept", accept)
            self.end_headers()
            time.sleep(1.5)
            self.close_connection = True
        elif path == "/":
            self.reply(200, "text/html", '''<!doctype html><html><head><title>API fixture</title>
                <link rel="https://api.w.org/" href="/wp-json/"></head><body>
                <form action="/api/subscribe?list=FORM_SECRET" method="post"></form>
                <button hx-post="/items/add">Add</button>
                <script src="/app.js?build=SCRIPT_SECRET"></script>
                <script>fetch('/api/inline?token=INLINE_SECRET')</script></body></html>''')
        elif path == "/app.js":
            self.reply(200, "application/javascript", f'''
                fetch('/api/v1/items?token=QUERY_SECRET').then(r => r.json());
                const x = new XMLHttpRequest(); x.open('POST', '/api/v1/session'); x.send('{{"password":"BODY_SECRET"}}');
                fetch('http://localhost:{other.server_port}/v2/quotes/8f14e45f-ceea-467f-a0e6-5c1d9e2b7a11');
                fetch('http://localhost:{other.server_port}/v2/quotes/11111111-2222-3333-4444-555555555555');
                new WebSocket('ws://127.0.0.1:{page.server_port}/socket?auth=SOCKET_SECRET');
                new EventSource('/api/v1/stream');
                navigator.sendBeacon('/collect', 'x');
                fetch('/pricing'); fetch('/data/config.json');
                const unused = "/api/v1/unused/report", docs = "/docs/api/reference", partner = "https://api.partner-fixture.dev/v1/track";
                function keys(base, id) {{ return `${{base}}/api/v1/admin/${{id}}/keys`; }}
                /* fetch("/api/v1/commented") */''')
        elif path in ("/api/v1/items", "/api/inline", "/data/config.json") or path.startswith("/v2/quotes/"):
            self.reply(200, "application/json", '{"ok":true}')
        elif path == "/api/v1/stream":
            self.reply(200, "text/event-stream", "data: fixture\n\n")
        elif path == "/pricing":
            self.reply(200, "text/html", "<!doctype html><html><title>Pricing</title></html>")
        elif path == "/robots.txt":
            self.reply(200, "text/plain", "User-agent: *\nDisallow: /api/private/\nDisallow: /tmp/\n")
        elif path == "/openapi.json":
            self.reply(200, "application/json", json.dumps(OPENAPI))
        elif path == "/graphql":
            self.reply(400, "application/json", '{"errors":[{"message":"Must provide query string."}]}')
        else:
            self.reply(404, "text/html", "<!doctype html><html><title>Not found</title></html>")

    def do_POST(self):
        hits.append((self.command, self.server.server_port, self.path))
        self.rfile.read(int(self.headers.get("Content-Length", 0)))
        path = self.path.split("?", 1)[0]
        if path == "/api/v1/session":
            self.reply(201, "application/json", '{"session":true}')
        else:
            self.reply(204, "text/plain", "")


page = http.server.ThreadingHTTPServer(("127.0.0.1", 0), Fixture)
other = http.server.ThreadingHTTPServer(("127.0.0.1", 0), Fixture)
for service in (page, other):
    threading.Thread(target=service.serve_forever, daemon=True).start()
here = f"http://127.0.0.1:{page.server_port}"
there = f"http://localhost:{other.server_port}"


def run(*args):
    process = subprocess.run([str(exe), "--target", here + "/", "--timeout", "150", "--http-timeout", "400",
                              "--browser-timeout", "5000", *args], capture_output=True, text=True, timeout=60)
    assert process.returncode == 0, process.stderr
    report = json.loads(process.stdout)
    rows = {(row["method"], row["url"]): row for row in report["api_endpoints"]}
    assert len(rows) == len(report["api_endpoints"]), "endpoint rows are unique by method and URL"
    return report, rows


def proof(row, text):
    return any(text in item["source"] + " | " + item["value"] for item in row["evidence"])


def never_called(since, *paths):
    called = [hit for hit in hits[since:] if hit[2].split("?", 1)[0] in paths]
    assert not called, f"Vector or the page requested an endpoint that was only named: {called}"


def shared(report, rows):
    text = json.dumps(report)
    for secret in SECRETS:
        assert secret not in json.dumps(report["api_endpoints"]), secret
        assert secret == "SCRIPT_SECRET" or secret not in text, secret
    coverage = report["api_endpoint_coverage"]
    assert coverage["endpoints_requested_by_vector"] is False and coverage["truncated"] is False, coverage
    assert rows[("", here + "/wp-json/")]["basis"] == "Page declaration"
    assert rows[("POST", here + "/api/subscribe")]["basis"] == "Page declaration"
    assert rows[("POST", here + "/items/add")]["basis"] == "Page declaration"
    assert rows[("", here + "/api/private/")]["basis"] == "robots.txt"
    assert rows[("GET", here + "/openapi.json")]["basis"] == "Collected response"
    assert proof(rows[("GET", here + "/openapi.json")], "API description document")
    assert rows[("GET", here + "/pets")]["basis"] == "API description"
    assert proof(rows[("DELETE", here + "/pets/{petId}")], "operationId deletePet")
    assert proof(rows[("GET", here + "/graphql")], "GraphQL-style JSON error body")
    unused = rows[("", "/api/v1/unused/report")]
    assert unused["basis"] == "Code reference" and unused["scope"] == "Host not stated" and unused["host"] == "", unused
    assert ("", "/api/v1/admin/{}/keys") in rows, sorted(rows)
    partner = rows[("", "https://api.partner-fixture.dev/v1/track")]
    assert partner["basis"] == "Code reference" and partner["scope"] == "Other host", partner
    for absent in ("/docs/api", "/api/v1/commented", "/tmp/", "config.json", "swagger"):
        assert not any(absent in url for _, url in rows), (absent, sorted(rows))


def main():
    start = len(hits)
    report, rows = run("--no-browser")
    shared(report, rows)
    assert not any(row["basis"] in ("Observed request", "Request attempt") for row in rows.values()), "no browser, no observed requests"
    items = rows[("", "/api/v1/items")]
    assert items["basis"] == "Code reference" and proof(items, "fetch() argument") and proof(items, "Script " + here + "/app.js"), items
    assert rows[("POST", "/api/v1/session")]["basis"] == "Code reference"
    assert rows[("", "/pricing")]["basis"] == "Code reference", "a fetch argument is a request URL until a response shows otherwise"
    assert not any("socket" in url or "quotes" in url for _, url in rows), "literals naming an IP address or localhost are not listed"
    never_called(start, "/api/v1/items", "/api/v1/session", "/socket", "/api/v1/stream", "/collect", "/api/inline",
                 "/api/v1/unused/report", "/pets", "/api/private/", "/api/subscribe", "/items/add", "/wp-json/")
    print("Static API endpoint collection passed:", len(rows), "rows.", flush=True)

    start = len(hits)
    report, rows = run()
    if report["browser"]["state"] == "unavailable":
        print("Browser fixture unavailable:", report["browser"]["reason"])
        sys.exit(77)
    shared(report, rows)
    items = rows[("GET", here + "/api/v1/items")]
    assert items["basis"] == "Observed request" and items["scope"] == "Same host" and items["status"] == 200, items
    assert proof(items, "Browser request / " + here + "/") and proof(items, "fetch() argument"), items
    assert "did not call" in items["meaning"]
    session = rows[("POST", here + "/api/v1/session")]
    assert session["basis"] == "Observed request" and session["status"] == 201, session
    quotes = rows[("GET", there + "/v2/quotes/{id}")]
    assert quotes["scope"] == "Other host" and quotes["basis"] == "Observed request", quotes
    assert sum("Browser request" in item["source"] for item in quotes["evidence"]) == 2, quotes
    socket = rows[("GET", "ws://127.0.0.1:%d/socket" % page.server_port)]
    assert socket["basis"] == "Observed request" and socket["status"] == 101 and proof(socket, "WebSocket"), socket
    assert rows[("GET", here + "/api/v1/stream")]["basis"] == "Observed request"
    assert rows[("POST", here + "/collect")]["basis"] == "Observed request"
    assert rows[("GET", here + "/api/inline")]["basis"] == "Observed request"
    assert not any("/pricing" in url for _, url in rows), "a fetch answered with an HTML page is not an API endpoint"
    sockets = [q for q in report["browser"]["requests"] if q["type"] == "WebSocket"]
    assert len(sockets) == 1 and sockets[0]["status"] == 101 and sockets[0]["state"] in ("open", "closed"), sockets
    assert sockets[0]["url"] == "ws://127.0.0.1:%d/socket" % page.server_port and sockets[0]["initiator"].endswith("/app.js"), sockets
    order = [row["basis"] for row in report["api_endpoints"] if row["scope"] == "Same host"]
    assert order == sorted(order, key=["Observed request", "Collected response", "Request attempt", "API description",
                                       "Page declaration", "Code reference", "robots.txt"].index), order
    assert report["api_endpoints"][0]["scope"] == "Same host" and report["api_endpoints"][0]["basis"] == "Observed request"
    assert any(hit[2].startswith("/api/v1/items") for hit in hits[start:]), "the page itself made its request"
    never_called(start, "/api/v1/unused/report", "/pets", "/api/private/", "/api/subscribe", "/items/add", "/wp-json/")
    if len(sys.argv) > 2:
        Path(sys.argv[2]).mkdir(parents=True, exist_ok=True)
        (Path(sys.argv[2]) / "report.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
    print("Browser API endpoint collection passed:", len(rows), "rows.", flush=True)


try:
    main()
finally:
    page.shutdown()
    other.shutdown()
