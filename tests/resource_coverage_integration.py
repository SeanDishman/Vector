import gzip
import http.server
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import threading
import time

react = (Path(__file__).parent / "fixtures/react-18.3.1/react.production.min.js").read_bytes()
paths = {
    "/web-animations-next-lite.min.vflset/web-animations-next-lite.min.js": "Web Animations polyfill",
    "/fetch-polyfill.vflset/fetch-polyfill.js": "Fetch polyfill",
    "/intersection-observer.min.vflset/intersection-observer.min.js": "Intersection Observer polyfill",
    "/spf.vflset/spf.js": "SPF",
    "/webcomponents-all-noPatch.vflset/webcomponents-all-noPatch.js": "Web Components polyfills",
    "/s/_/ytmainappweb/_/js/k=ytmainappweb.kevlar_base.en_US.hash.es5.O/d=1/m=kevlar_base_module": "YouTube web client",
}
prefix = b"/*" + b"p" * (900 * 1024) + b"*/\n" + react
large = prefix + b"\n/*" + b"t" * (5 * 1024 * 1024) + b"*/\ndelete window.React;fetch('/proof');"
late = b"/*" + b"t" * (2200 * 1024) + b"*/\n" + react + b"\ndelete window.React;"
stop = threading.Event()


class Fixture(http.server.BaseHTTPRequestHandler):
    def log_message(self, *_): pass
    def handle(self):
        try: super().handle()
        except (BrokenPipeError, ConnectionResetError, ConnectionAbortedError): pass

    def do_GET(self):
        path = self.path.split("?", 1)[0]
        status, mime, body = 200, "text/html", b""
        if path in ("/fast", "/gzip", "/chunked", "/late", "/slow", "/denied", "/redirect"):
            body = b'<title>Collected resource fixture</title><script src="/fixture-loader.js?mode=' + path[1:].encode() + b'"></script>'
        elif path == "/fixture-loader.js":
            mode = self.path.split("mode=", 1)[1]
            urls = [*paths, "/bundle-" + mode + ".js"] if mode == "fast" else ["/bundle-" + mode + ".js"]
            body = ("for(const u of " + json.dumps(urls) + "){const s=document.createElement('script');s.src=u;document.head.appendChild(s);}").encode()
            if mode == "slow": body = b"fetch('/bundle-slow.js').then(r=>r.text());"
            mime = "application/javascript"
        elif path in paths:
            mime, body = "application/javascript", b"/* resource URL fixture */"
        elif path.startswith("/bundle-"):
            mime, body = "application/javascript", late if path == "/bundle-late.js" else large
            if path == "/bundle-denied.js": status = 403
        elif path == "/proof":
            self.server.executed = True
            status = 204
        else: status = 404
        encoded = gzip.compress(body) if path == "/bundle-gzip.js" else body
        if path == "/bundle-redirect.js":
            self.send_response(302)
            self.send_header("Location", "/bundle-fast.js")
            self.send_header("Content-Length", "0")
            self.end_headers()
            return
        self.send_response(status)
        self.send_header("Content-Type", mime)
        self.send_header("Content-Length", str(len(encoded)))
        if path == "/bundle-gzip.js": self.send_header("Content-Encoding", "gzip")
        self.end_headers()
        if path == "/bundle-slow.js":
            self.wfile.write(encoded[:1100 * 1024]); self.wfile.flush()
            stop.wait(20)
            return
        if path == "/bundle-chunked.js":
            for i in range(0, len(encoded), 64 * 1024):
                self.wfile.write(encoded[i:i + 64 * 1024]); self.wfile.flush(); time.sleep(.003)
        else: self.wfile.write(encoded)


def start():
    server = http.server.ThreadingHTTPServer(("127.0.0.1", 0), Fixture)
    server.executed = False
    threading.Thread(target=server.serve_forever, daemon=True).start()
    return server


def run(exe, server, path, *args):
    server.executed = False
    proc = subprocess.Popen([str(exe), "--target", f"http://127.0.0.1:{server.server_port}{path}", "--web-only", *args], stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    output, error = proc.communicate(timeout=40)
    assert proc.returncode == 0, error
    assert not list(Path(tempfile.gettempdir()).glob(f"vector-browser-{proc.pid}-*")), "Own browser profile retained"
    return json.loads(output)


if __name__ == "__main__":
    exe = Path(sys.argv[1]).resolve()
    baseline = "--baseline" in sys.argv
    out = Path(sys.argv[2]).resolve() if len(sys.argv) > 2 and not sys.argv[2].startswith("--") else None
    if out: out.mkdir(parents=True, exist_ok=True)
    server = start(); checked = 0; failed = []
    def check(value, label):
        global checked
        checked += 1
        if not value: failed.append(label)
        if not value and not baseline: raise AssertionError(label)
    try:
        for path in ("/fast", "/gzip", "/chunked", "/redirect", "/late", "/denied", "/slow"):
            args = ("--browser-timeout", "1800") if path == "/slow" else ()
            report = run(exe, server, path, *args)
            if report["browser"]["state"] == "unavailable": sys.exit(77)
            if out: (out / (path[1:] + ".json")).write_text(json.dumps(report, indent=2), encoding="utf-8")
            names = {f["name"] for f in report["findings"]}
            if path == "/fast":
                for name in paths.values(): check(name in names, "resource path: " + name)
            scripts = [s for s in report["scripts"] if s["collection"] == "browser" and "/bundle-" in s["url"]]
            if path in ("/fast", "/gzip", "/chunked", "/redirect"):
                check(any(s["analyzed"] and s["body_truncated"] and s["body_bytes"] == 2 * 1024 * 1024 for s in scripts), path + " retains bounded prefix of >4 MiB script")
                check(any(f["name"] == "React" and any("Browser script" in e["source"] for e in f["evidence"]) for f in report["findings"]), path + " identifies real production React in prefix")
                check(server.executed, path + " still executes the complete browser bundle")
            elif path in ("/late", "/denied"):
                check("React" not in names, path + " does not infer a library beyond bounds or from rejected bytes")
            else:
                check(any(s["analyzed"] and s["body_truncated"] and 0 < s["body_bytes"] < 2 * 1024 * 1024 for s in scripts), "interrupted response retains a contiguous prefix")
                check("React" in names, "interrupted response still supplies received library evidence")
                check(not server.executed, "partial bytes are not reported as full execution")
            check(report["web_coverage"]["page_requests"] <= 12, path + " keeps shared page cap")
        print(f"{checked} resource coverage checks; {len(failed)} failures: {failed}")
    finally:
        stop.set(); server.shutdown()
