import http.server
import json
from pathlib import Path
import subprocess
import sys
import threading

requests = []
desktop = "/s/desktop/fixture/jsbin/desktop_polymer.vflset/desktop_polymer.js"
player = "/s/player/fixture/player_ias.vflset/en_US/base.js"


class Web(http.server.BaseHTTPRequestHandler):
    server_version = "ESF"
    sys_version = ""

    def log_message(self, *_):
        pass

    def do_GET(self):
        requests.append(self.path)
        status, mime = 200, "application/javascript"
        if self.path == "/":
            mime = "text/html"
            body = f'''<title>Library detection fixture</title>
            <script src="{desktop}"></script><script src="{player}"></script>
            <script src="/runtime?build=fixture&amp;lang=en"></script>
            <script src="/large.js"></script><script src="/missing.js"></script>
            <script src="/html.js"></script><script src="/unknown.js"></script>
            <script src="https://cdn.example.invalid/not-fetched.js"></script>
            <script src="http://127.0.0.1:1/not-fetched.js"></script>
            <template><script src="/inactive.js"></script></template>
            <script type="application/json">{{"src":"/fake.js"}}</script>
            <script type="importmap">{{"imports":{{"tiny":"./mapped.mjs"}}}}</script>
            <link rel="preload" as="script" href="/preloaded">
            <link rel="stylesheet" href="https://fonts.googleapis.com/css?family=Example">
            '''.encode()
        elif self.path == "/budget":
            mime = "text/html"
            body = "".join(f'<script src="/budget-{i}.js"></script>' for i in range(41)).encode()
        elif self.path.startswith("/budget-"):
            body = b"const boundedFixture=42;"
        elif self.path == "/references":
            mime = "text/html"
            body = "".join(f'<script src="https://cdn.example.invalid/{i}.js"></script>' for i in range(513)).encode()
        elif self.path == desktop:
            body = b"Polymer.version='3.5.2';window.ShadyCSS.styleDocument();"
        elif self.path == player:
            body = b"/* @license Copyright The Closure Library Authors. */"
        elif self.path.startswith("/runtime?"):
            body = b"(globalThis.litHtmlVersions??=[]).push('3.3.1');"
        elif self.path == "/mapped.mjs":
            body = b"(globalThis.litElementVersions??=[]).push('4.2.0');"
        elif self.path == "/preloaded":
            body = b"Symbol.for('react.element');Symbol.for('react.fragment');other.version='99.9.9';"
        elif self.path == "/large.js":
            body = b" " * 300000 + b"/*! jQuery v3.7.1 */" + b" " * 300000 + b"Polymer.version='99.9.9';"
        elif self.path == "/html.js":
            mime = "text/html; note=javascript"
            body = b"<p>jQuery v99.9.9</p><script>Polymer.version='99.9.9';</script>"
        elif self.path == "/unknown.js":
            body = b"const privateImplementation=()=>42;"
        else:
            status, body = 404, b"/* jQuery v99.9.9 */"
        self.send_response(status)
        self.send_header("Content-Type", mime)
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        try:
            self.wfile.write(body)
        except (BrokenPipeError, ConnectionResetError, ConnectionAbortedError):
            pass


def validate(report):
    named = {(f["name"], f["version"]) for f in report["findings"]}
    required = {("Polymer", "3.5.2"), ("lit-html", "3.3.1"), ("LitElement", "4.2.0"),
                ("React", ""), ("jQuery", "3.7.1"), ("ShadyCSS", ""),
                ("YouTube web client", ""), ("YouTube player", ""), ("Google Closure Library", "")}
    assert required <= named, required - named
    assert not any(version in ("99.9.9", "fixture") for _, version in named), named
    scripts = report["scripts"]
    assert len(scripts) == 11, scripts
    assert sum(s["analyzed"] for s in scripts) == 7
    assert any(s["body_truncated"] and s["analyzed"] and s["body_bytes"] == 512 * 1024 for s in scripts)
    assert any(s["state"] == "failed" and s["status"] == 404 for s in scripts)
    assert any(s["state"] == "skipped" and "content type" in s["reason"] for s in scripts)
    assert any(s["state"] == "skipped" and "External host" in s["reason"] for s in scripts)
    assert any(s["state"] == "skipped" and "Different port" in s["reason"] for s in scripts)
    assert not any(x in requests for x in ("/inactive.js", "/fake.js", "/not-fetched.js")), requests
    assert report["script_coverage"]["javascript_executed"] is False


if __name__ == "__main__":
    exe = Path(sys.argv[1]).resolve()
    server = http.server.ThreadingHTTPServer(("127.0.0.1", 0), Web)
    threading.Thread(target=server.serve_forever, daemon=True).start()
    target = f"http://127.0.0.1:{server.server_port}/"
    try:
        completed = subprocess.run([str(exe), "--no-browser", "--target", target, "--timeout", "150", "--http-timeout", "2000"], capture_output=True, text=True, timeout=30)
        assert completed.returncode == 0, completed.stderr
        report = json.loads(completed.stdout)
        validate(report)
        if len(sys.argv) > 2:
            Path(sys.argv[2]).write_text(json.dumps(report, indent=2))
        def inspect(path):
            run = subprocess.run([str(exe), "--no-browser", "--target", target + path, "--timeout", "150"], capture_output=True, text=True, timeout=30)
            assert run.returncode == 0, run.stderr
            return json.loads(run.stdout)
        budget = inspect("budget")
        assert len(budget["scripts"]) == 41 and sum(s["analyzed"] for s in budget["scripts"]) == 40
        assert budget["scripts"][-1]["reason"] == "Same-host script limit reached"
        assert "/budget-40.js" not in requests
        references = inspect("references")
        assert len(references["scripts"]) == 512 and references["script_coverage"]["references_truncated"]
        assert all(s["state"] == "skipped" and s["status"] == 0 for s in references["scripts"])
        print("Library loopback: signatures, skipped/failed/truncated reads, 40-script budget and 512-reference cap passed.")
    finally:
        server.shutdown()
        server.server_close()
