import http.server
import json
from pathlib import Path
import socket
import subprocess
import sys
import tempfile
import threading
import time

hits = []


class Web(http.server.BaseHTTPRequestHandler):
    server_version = "nginx/1.26.2"
    sys_version = ""

    def log_message(self, *_):
        pass

    def handle(self):
        try:
            super().handle()
        except (BrokenPipeError, ConnectionResetError, ConnectionAbortedError):
            pass

    def do_GET(self):
        hits.append((self.server.server_port, self.path, self.headers.get("User-Agent", "")))
        path = self.server.mode if self.path == "/" else self.path
        status, mime, body = 200, "text/html", "<title>Web budget fixture</title>"
        if path == "/many":
            body += "".join(f'<a href="/item/{i}">item</a>' for i in range(1000))
        elif path == "/small":
            body += "".join(f'<a href="/item/{i}">item</a>' for i in range(3))
        elif path == "/failures":
            body += "".join(f'<a href="/disconnect/{i}">item</a>' for i in range(100))
        elif path.startswith("/disconnect/"):
            self.connection.shutdown(socket.SHUT_RDWR)
            self.connection.close()
            return
        elif path == "/redirects":
            body += "".join(f'<a href="/redirect/{i}">item</a>' for i in range(100))
        elif path.startswith("/redirect/"):
            self.send_response(302)
            self.send_header("Location", path + "/hop")
            self.send_header("Content-Length", "0")
            self.end_headers()
            return
        elif path == "/scripts":
            body += "".join(f'<script src="/lib-{i}.js"></script>' for i in range(24))
        elif path.startswith("/lib-"):
            mime, body = "application/javascript", "/*! jQuery v3.7.1 */"
        elif path.startswith("/slow"):
            time.sleep(4)
            body += "".join(f'<a href="/slow/item/{i}">item</a>' for i in range(100))
        elif path == "/runtime":
            body += '<script>fetch("/wait");</script>'
        elif path == "/wait":
            time.sleep(60)
            mime, body = "application/json", "{}"
        elif path in ("/robots.txt", "/sitemap.xml", "/sitemap_index.xml"):
            status, body = 404, ""
        self.send_response(status)
        self.send_header("Content-Type", mime)
        self.send_header("Content-Length", str(len(body.encode())))
        self.end_headers()
        self.wfile.write(body.encode())


def start():
    server = http.server.ThreadingHTTPServer(("127.0.0.1", 0), Web)
    server.mode = "/small"
    threading.Thread(target=server.serve_forever, daemon=True).start()
    return server


def run(exe, target, *args):
    process = subprocess.Popen([str(exe), "--target", target, "--timeout", "150", *args],
                               stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    try:
        stdout, stderr = process.communicate(timeout=40)
    except subprocess.TimeoutExpired:
        process.kill()
        process.communicate()
        raise
    assert process.returncode == 0, stderr
    leftovers = list(Path(tempfile.gettempdir()).glob(f"vector-browser-{process.pid}-*"))
    assert not leftovers, f"Temporary browser profiles leaked: {[p.name for p in leftovers]}\n{stderr}"
    return json.loads(stdout)


def validate_cap(report, text):
    coverage = report["web_coverage"]
    assert report["status"] == "complete"
    assert coverage["limited"] and any(text in r for r in coverage["reasons"]), coverage
    assert coverage["page_requests"] == len(report["web"]) <= 12, coverage
    assert coverage["script_requests"] <= 40, coverage
    assert any(e["item"] == "Web evidence cap" for e in report["activity"])
    assert any(f["name"] == "nginx" for f in report["findings"]), "Existing evidence was discarded"


if __name__ == "__main__":
    exe = Path(sys.argv[1]).resolve()
    primary, secondary = start(), start()
    base = f"http://127.0.0.1:{primary.server_port}"
    ports = f"{primary.server_port},{secondary.server_port}"
    try:
        for path in ("/many", "/redirects", "/failures"):
            before = len(hits)
            report = run(exe, base + path, "--no-browser")
            validate_cap(report, "12-page")
            assert len(report["web"]) == 12
            assert sum(h[2].startswith("Vector/") for h in hits[before:]) == 12
            if path == "/redirects":
                assert any(h["status"] == 302 for h in report["web"])
            if path == "/failures":
                assert any(h["status"] == 0 for h in report["web"])
        shared = run(exe, "127.0.0.1", "--no-browser", "--ports", ports)
        validate_cap(shared, "12-page")
        assert len({h["url"].split(":")[2].split("/")[0] for h in shared["web"]}) == 2
        primary.mode = secondary.mode = "/scripts"
        scripts = run(exe, "127.0.0.1", "--no-browser", "--ports", ports)
        validate_cap(scripts, "40-script")
        assert scripts["web_coverage"]["script_requests"] == 40
        assert sum(s["analyzed"] for s in scripts["scripts"]) == 40
        assert sum(s["state"] == "skipped" for s in scripts["scripts"]) == 8
        primary.mode = "/small"
        slow = run(exe, base + "/slow", "--no-browser")
        validate_cap(slow, "30-second")
        assert 29000 <= slow["web_coverage"]["elapsed_ms"] < 34000, slow["web_coverage"]
        runtime = run(exe, base + "/runtime", "--browser-timeout", "30000")
        if runtime["browser"]["state"] != "unavailable":
            validate_cap(runtime, "30-second")
            assert runtime["browser"]["state"] == "limited", runtime["browser"]
            assert runtime["script_coverage"]["javascript_executed"]
            assert 29000 <= runtime["web_coverage"]["elapsed_ms"] < 34000, runtime["web_coverage"]
            print("7 web-cap scenarios passed: page flood, redirects, failed reads, shared page/script budgets, HTTP deadline and active browser deadline.")
        else:
            print("6 web-cap scenarios passed; installed browser unavailable for runtime deadline fixture.")
    finally:
        primary.shutdown()
        secondary.shutdown()
