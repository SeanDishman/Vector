import http.server
import json
from pathlib import Path
import subprocess
import sys
import threading

MOTION = b"const appearAttribute='data-'+dash('framerAppearId');function project(node,props){node.projection=new Projection(node.latestValues,props['data-framer-portal-id']?void 0:closest(node.parent));}"
BASE_UI = b"const inertMarker='data-base-ui-inert';const guard=create('span',{...rest,'data-base-ui-focus-guard':''});"
DATADOG = b"const intake=['ddsource','dd-api-key','dd-request-id'];function api(stub){return{version:'7.99.1',onReady(callback){callback()},...stub}}"
QUERY = b"function client(c){if(!c)throw new Error('No QueryClient set, use QueryClientProvider to set one');return c}const fn=o=>()=>Promise.reject(new Error(`Missing queryFn: '${o.queryHash}'`));"
TAILWIND = b"*,::before,::after{--tw-translate-x:0;--tw-ring-shadow:0 0 #0000;--tw-shadow:0 0 #0000}"


class Handler(http.server.BaseHTTPRequestHandler):
    def log_message(self, *_):
        pass

    def do_GET(self):
        self.server.paths.append(self.path)
        status, mime, body = self.server.route(self.path)
        self.send_response(status)
        self.send_header("Content-Type", mime)
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        try:
            self.wfile.write(body)
        except (BrokenPipeError, ConnectionResetError, ConnectionAbortedError):
            pass


def serve(address, route):
    server = http.server.ThreadingHTTPServer((address, 0), Handler)
    server.daemon_threads = True
    server.paths = []
    server.route = route
    threading.Thread(target=server.serve_forever, daemon=True).start()
    return server


def assets(path):
    files = {"/motion.js": MOTION, "/base.js": BASE_UI, "/datadog.js": DATADOG, "/shell-only.js": QUERY}
    if path == "/framework.css":
        return 200, "text/css", TAILWIND
    if path in files:
        return 200, "text/javascript", files[path]
    return 404, "text/plain", b"missing"


if __name__ == "__main__":
    exe = Path(sys.argv[1]).resolve()
    other = serve("127.0.0.2", assets)
    named = serve("127.0.0.1", assets)
    far = f"http://127.0.0.2:{other.server_port}"
    near = f"http://localhost:{named.server_port}"

    def site(path):
        base = f"http://127.0.0.1:{page.server_port}"
        if path == "/":
            return 200, "text/html", f'''<!doctype html><title>External asset fixture</title>
            <link rel="stylesheet" href="{far}/framework.css"><link rel="stylesheet" href="/own.css">
            <link rel="stylesheet" href="https://fonts.example.invalid/blocked.css">
            <script src="{far}/motion.js"></script><script type="module" src="{near}/base.js"></script>
            <link rel="modulepreload" href="{far}/datadog.js"><script src="/own.js"></script>
            <script src="https://cdn.example.invalid/blocked.js"></script>
            <script src="http://192.0.2.10/documentation-range.js"></script>
            <script src="{far}/missing.js"></script><script src="http://127.0.0.2:1/dead.js"></script>
            <script src="http://127.0.0.2:1/dead-again.js"></script>'''.encode()
        if path == "/own.js":
            return 200, "text/javascript", b"/*! jQuery v3.7.1 */"
        if path == "/own.css":
            return 200, "text/css", b".plain{color:red}"
        if path == "/robots.txt":
            return 200, "text/plain", f"Sitemap: {base}/sitemap-mislabeled.xml\n".encode()
        if path == "/sitemap-mislabeled.xml":
            return 200, "text/html", f'<?xml version="1.0"?><urlset><url><loc>{base}/from-mislabeled</loc></url></urlset>'.encode()
        if path == "/sitemap_index.xml":
            return 200, "text/html; charset=utf-8", f'<!doctype html><html><head><title>Shell</title><script src="{far}/shell-only.js"></script></head><body></body></html>'.encode()
        if path == "/from-mislabeled":
            return 200, "text/html", b"<title>Listed page</title>"
        return 404, "text/plain", b"missing"

    page = serve("127.0.0.1", site)
    try:
        run = subprocess.run([str(exe), "--no-browser", "--web-only", "--target", f"http://127.0.0.1:{page.server_port}/",
                              "--timeout", "150", "--http-timeout", "2500"], capture_output=True, text=True, timeout=50)
        assert run.returncode == 0, run.stderr
        report = json.loads(run.stdout)
        found = {f["name"]: f for f in report["findings"]}
        for name in ("Motion (Framer Motion)", "Base UI", "Datadog Browser SDK", "TanStack Query", "Tailwind CSS", "jQuery"):
            assert name in found, (name, sorted(found))
        assert found["Datadog Browser SDK"]["version"] == "7.99.1" and found["jQuery"]["version"] == "3.7.1"
        assert all(e["source"] == f"{far}/motion.js" for e in found["Motion (Framer Motion)"]["evidence"])
        assert all(e["source"] == f"{near}/base.js" for e in found["Base UI"]["evidence"])
        assert all(e["source"] == f"{far}/shell-only.js" for e in found["TanStack Query"]["evidence"])

        scripts = {s["url"]: s for s in report["scripts"]}
        order = [s["url"] for s in report["scripts"]]
        assert order.index(f"http://127.0.0.1:{page.server_port}/own.js") < order.index(f"{far}/motion.js"), order
        for url in (f"{far}/motion.js", f"{near}/base.js", f"{far}/datadog.js", f"{far}/shell-only.js"):
            assert scripts[url]["state"] == "analyzed" and scripts[url]["status"] == 200, scripts[url]
        for url in ("https://cdn.example.invalid/blocked.js", "http://192.0.2.10/documentation-range.js"):
            assert scripts[url]["state"] == "skipped" and scripts[url]["status"] == 0, scripts[url]
            assert scripts[url]["reason"] == "External host; URL signatures only", scripts[url]
        assert scripts[f"{far}/missing.js"]["state"] == "failed" and scripts[f"{far}/missing.js"]["status"] == 404
        assert scripts["http://127.0.0.2:1/dead.js"]["state"] == "failed" and scripts["http://127.0.0.2:1/dead.js"]["status"] == 0
        again = scripts["http://127.0.0.2:1/dead-again.js"]
        assert again["state"] == "skipped" and again["status"] == 0 and "not retried" in again["reason"], again

        sheets = {s["url"]: s for s in report["stylesheets"]}
        assert sheets[f"{far}/framework.css"]["state"] == "analyzed", sheets
        assert sheets[f"http://127.0.0.1:{page.server_port}/own.css"]["state"] == "analyzed", sheets
        blocked = sheets["https://fonts.example.invalid/blocked.css"]
        assert blocked["state"] == "skipped" and blocked["status"] == 0 and "outside the page host" in blocked["reason"], blocked

        assert "/from-mislabeled" in page.paths, page.paths
        assert report["web_coverage"]["script_requests"] == 7, report["web_coverage"]
        print("External assets on loopback: other-host scripts and stylesheet analyzed with their own URLs, outside references "
              "never requested, unanswered endpoint not retried, HTML at a sitemap path analyzed, mislabeled XML sitemap still followed.")
    finally:
        for server in (page, other, named):
            server.shutdown()
            server.server_close()
