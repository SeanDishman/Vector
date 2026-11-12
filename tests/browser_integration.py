import http.server
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import threading
import time

exe = Path(sys.argv[1]).resolve()
hits = []


class Fixture(http.server.BaseHTTPRequestHandler):
    def log_message(self, *_):
        pass

    def handle(self):
        try:
            super().handle()
        except (BrokenPipeError, ConnectionResetError, ConnectionAbortedError):
            pass

    def do_GET(self):
        hits.append((self.server.server_port, self.path, self.headers.get("Cookie", "")))
        path = self.path.split("?", 1)[0]
        status, mime, content = 200, "text/html", ""
        if path == "/":
            content = '<title>Runtime fixture</title><script src="/loader.js"></script>'
        elif path == "/loader.js":
            mime = "application/javascript"
            content = '''setTimeout(() => {
                const s=document.createElement('script');s.src='/stage-'+'two.js';document.head.appendChild(s);
            }, 250);'''
        elif path == "/stage-two.js":
            mime = "application/javascript"
            content = f'''import('http://localhost:{external.server_port}/'+'dependency.mjs').then(() => {{
                const meta=document.createElement('meta');meta.name='generator';meta.content='WordPress 6.7';document.head.appendChild(meta);
                document.body.dataset.runtime='complete';
                fetch('/proof?token=SECRET_RUNTIME_QUERY',{{method:'POST',body:'fixture only'}});
            }});'''
        elif path == "/dependency.mjs":
            mime = "application/javascript"
            content = "(globalThis.litHtmlVersions??=[]).push('3.3.1'); export const loaded=true;"
        elif path == "/static":
            content = '<title>Static fixture</title><script src="/loader.js"></script>'
        elif path == "/children":
            content = f'''<script>
                new Worker('/worker.js');
                const frame=document.createElement('iframe');frame.src='http://localhost:{external.server_port}/frame';
                document.documentElement.appendChild(frame);
            </script>'''
        elif path == "/worker.js":
            mime, content = "application/javascript", f"importScripts('http://localhost:{external.server_port}/worker-child.js');"
        elif path == "/worker-child.js":
            mime, content = "application/javascript", "/*! jQuery v3.7.1 */ const workerLoaded=true;"
        elif path == "/frame":
            content = '<script>const s=document.createElement("script");s.src="/frame-child.js";document.head.appendChild(s)</script>'
        elif path == "/frame-child.js":
            mime, content = "application/javascript", "(globalThis.litElementVersions??=[]).push('4.2.0');"
        elif path == "/navigation":
            content = f'<script>setTimeout(()=>location.href="http://localhost:{external.server_port}/outside",100)</script>'
        elif path == "/request-limit":
            content = '<script>for(let i=0;i<300;i++){const s=document.createElement("script");s.src="/limit-"+i+".js";document.head.appendChild(s)}</script>'
        elif path.startswith("/limit-"):
            mime, content = "application/javascript", "const x=1;"
        elif path == "/failure":
            content = '<script>const s=document.createElement("script");s.src="/missing.js";document.head.appendChild(s)</script>'
        elif path == "/never":
            content = '<script>fetch("/pending");setInterval(()=>{},200)</script>'
        elif path == "/pending":
            time.sleep(10)
            mime, content = "application/json", "{}"
        elif path == "/large":
            content = '<script>const s=document.createElement("script");s.src="/large.js";document.head.appendChild(s)</script>'
        elif path == "/large.js":
            mime, content = "application/javascript", " " * (2 * 1024 * 1024 + 10) + "/* bounded */"
        else:
            status = 404
        self.send_response(status)
        self.send_header("Content-Type", mime)
        if path == "/":
            self.send_header("Set-Cookie", "runtime_session=SESSION_FIXTURE_SECRET; SameSite=Lax")
        self.send_header("Access-Control-Allow-Origin", "*")
        self.send_header("Content-Length", str(len(content.encode())))
        try:
            self.end_headers()
            self.wfile.write(content.encode())
        except (BrokenPipeError, ConnectionResetError, ConnectionAbortedError):
            pass

    def do_POST(self):
        hits.append((self.server.server_port, self.path, self.headers.get("Cookie", "")))
        self.rfile.read(int(self.headers.get("Content-Length", 0)))
        self.send_response(204)
        self.end_headers()


page = http.server.ThreadingHTTPServer(("127.0.0.1", 0), Fixture)
external = http.server.ThreadingHTTPServer(("127.0.0.1", 0), Fixture)
for service in (page, external):
    threading.Thread(target=service.serve_forever, daemon=True).start()
url = f"http://127.0.0.1:{page.server_port}"


def run(path="/", *args, env=None):
    process = subprocess.Popen([str(exe), "--target", url + path, "--timeout", "150", "--http-timeout", "300", *args],
                               stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, env=env)
    try:
        stdout, stderr = process.communicate(timeout=35)
    except subprocess.TimeoutExpired:
        process.kill()
        process.communicate()
        raise
    assert process.returncode == 0, stderr
    leftovers = list(Path(tempfile.gettempdir()).glob(f"vector-browser-{process.pid}-*"))
    assert not leftovers, f"Temporary browser profiles leaked: {[p.name for p in leftovers]}\n{stderr}"
    return json.loads(stdout)


def main():
    try:
        report = run()
        if report["browser"]["state"] == "unavailable":
            print("Browser fixture unavailable:", report["browser"]["reason"])
            sys.exit(77)
        assert report["script_coverage"]["javascript_executed"], report["browser"]
        assert report["browser"]["state"] == "complete", report["browser"]
        scripts = [s for s in report["scripts"] if s["collection"] == "browser" and s["analyzed"]]
        assert any(s["url"].endswith("/stage-two.js") for s in scripts), scripts
        assert any(s["url"].endswith("/dependency.mjs") for s in scripts), scripts
        names = {f["name"] for f in report["findings"]}
        assert {"lit-html", "WordPress"} <= names, report["findings"]
        assert any("Browser runtime" in e["source"] for f in report["findings"] if f["name"] == "lit-html" for e in f["evidence"])
        assert any(v["evidence_level"] == "Observed response" and v["domain"] == "localhost" for v in report["vendors"]), report["vendors"]
        requests = report["browser"]["requests"]
        assert any(q["url"].endswith("/proof") and q["method"] == "POST" and q["status"] == 204 for q in requests), requests
        assert any(q["url"].endswith("/dependency.mjs") and q["initiator"].endswith("/stage-two.js") for q in requests)
        assert "SECRET_RUNTIME_QUERY" not in json.dumps(report)
        assert "SESSION_FIXTURE_SECRET" not in json.dumps(report)
        assert any(h[1].startswith("/proof") and "SESSION_FIXTURE_SECRET" in h[2] for h in hits)
        if len(sys.argv) > 2:
            Path(sys.argv[2]).write_text(json.dumps(report, indent=2), encoding="utf-8")
        count = len(hits)
        static = run("/static", "--no-browser")
        assert static["browser"]["state"] == "disabled"
        assert not static["script_coverage"]["javascript_executed"]
        assert not any("stage-two" in h[1] or "dependency" in h[1] for h in hits[count:]), hits[count:]
        absent_env = dict(os.environ, VECTOR_BROWSER=str(exe.parent / "missing-chromium.exe"))
        absent = run("/", env=absent_env)
        assert absent["browser"]["state"] == "unavailable" and not absent["script_coverage"]["javascript_executed"]
        count = len(hits)
        failure = run("/failure")
        assert all(not h[2] for h in hits[count:]), "Temporary cookies persisted into a new inspection"
        assert any(q["url"].endswith("/missing.js") and q["status"] == 404 for q in failure["browser"]["requests"])
        assert not any(s["url"].endswith("/missing.js") and s["analyzed"] for s in failure["scripts"])
        timed = run("/never", "--browser-timeout", "1200")
        assert timed["browser"]["state"] == "timed_out", timed["browser"]
        large = run("/large")
        assert any(s["url"].endswith("/large.js") and s["collection"] == "browser" and s["analyzed"] and s["body_truncated"] for s in large["scripts"])
        children = run("/children")
        loaded = {s["url"].rsplit("/", 1)[-1] for s in children["scripts"] if s["collection"] == "browser" and s["analyzed"]}
        assert {"worker-child.js", "frame-child.js"} <= loaded, children["browser"]
        count = len(hits)
        navigation = run("/navigation")
        assert not any(h[1].startswith("/outside") for h in hits[count:]), hits[count:]
        assert any(q["url"].endswith("/outside") and q["state"] == "blocked" for q in navigation["browser"]["requests"]), navigation["browser"]
        assert all(q["url"].startswith(("http://", "https://")) for q in navigation["browser"]["requests"])
        assert navigation["script_coverage"]["javascript_executed"], "A later blocked navigation erased observed execution"
        count = len(hits)
        budget = run("/request-limit")
        assert budget["browser"]["state"] == "limited", budget["browser"]
        assert len(budget["browser"]["requests"]) <= 256
        assert sum(h[1].startswith("/limit-") for h in hits[count:]) <= 255
        print("9 browser scenarios passed: nested loaders, external modules, workers/frames, evidence, fallback, failures, timeout, request/body limits and navigation scope.")
    finally:
        page.shutdown()
        external.shutdown()


if __name__ == "__main__":
    main()
