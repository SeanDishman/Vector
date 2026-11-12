import argparse
import http.server
import json
from pathlib import Path
import subprocess
import threading
import uuid


class Handler(http.server.BaseHTTPRequestHandler):
    def log_message(self, *_):
        pass

    def do_GET(self):
        self.server.hits.append(self.path)
        code, mime, text = self.server.routes.get(self.path, (404, "text/html", "Not found"))
        body = text.encode()
        try:
            self.send_response(code)
            self.send_header("Content-Type", mime)
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)
        except (BrokenPipeError, ConnectionError):
            pass


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("exe", type=Path)
    parser.add_argument("out", type=Path)
    parser.add_argument("--baseline", action="store_true")
    args = parser.parse_args()
    args.out.mkdir(parents=True, exist_ok=True)
    server = http.server.ThreadingHTTPServer(("127.0.0.1", 0), Handler)
    server.hits = []
    supplier = "Supplier " + uuid.uuid4().hex
    source = "/" + uuid.uuid4().hex
    noise = "".join(f'<a href="/product/{i}">Product {i}</a>' for i in range(60))
    server.routes = {
        "/": (200, "text/html", noise + '<a href="/docs">Documentation</a><a href="/compliance">Compliance</a>'),
        "/docs": (200, "text/html", '<meta name="generator" content="Docusaurus v3.9.2">'),
        "/compliance": (200, "text/html", f'<a href="{source}">Subprocessors</a>'),
        source: (200, "text/html", '<h1>Subprocessors</h1><table><tr><th>Company</th><th>Purpose</th></tr>'
                 f'<tr><td>{supplier}</td><td>Data hosting</td></tr></table>'),
        "/robots.txt": (200, "text/plain", "User-agent: *\nDisallow:\n"),
    }
    threading.Thread(target=server.serve_forever, daemon=True).start()
    target = f"http://127.0.0.1:{server.server_port}/"
    def run(label):
        p = subprocess.run([str(args.exe.resolve()), "--target", target, "--web-only", "--no-browser",
                            "--timeout", "150", "--http-timeout", "300"],
                           capture_output=True, text=True, encoding="utf-8", timeout=20)
        assert p.returncode == 0, p.stderr[-2000:]
        r = json.loads(p.stdout)
        (args.out / (label + ".json")).write_text(json.dumps(r, indent=2), encoding="utf-8")
        assert r["web_coverage"]["page_requests"] <= 12
        assert server.hits[0] == "/", server.hits
        return r
    try:
        report = run("linked")
        names = [v["name"] for v in report["vendors"] if v.get("published_by")]
        if args.baseline:
            assert "/docs" not in server.hits and supplier not in names, (server.hits, names)
            print("Baseline reproduced: documentation and generated provider source were missed.")
            return
        assert names == [supplier], names
        assert "/docs" in server.hits and server.hits.index("/docs") < server.hits.index("/product/0"), server.hits
        assert any(f["name"] == "Docusaurus" for f in report["findings"])
        assert any(d["discovery_depth"] == 2 and len(d["discovery_chain"]) == 2 and d["providers"] == 1 for d in report["disclosures"])
        assert not any(f["name"] == supplier for f in report["findings"])
        linked_hits = list(server.hits)
        server.hits.clear()
        server.routes = {
            "/": (200, "text/html", "<h1>Small site</h1>"),
            "/robots.txt": (200, "text/plain", "User-agent: *\nDisallow:\n"),
            "/sitemap.xml": (200, "application/xml", f"<urlset><url><loc>{target}map-only</loc></url></urlset>"),
            "/map-only": (200, "text/html", '<meta name="generator" content="Docusaurus v3.9.2">'),
        }
        report = run("sitemap-fallback")
        assert "/map-only" in server.hits and any(f["name"] == "Docusaurus" for f in report["findings"])
        (args.out / "verification.json").write_text(json.dumps({"generated_supplier": supplier,
            "linked_requests": linked_hits, "fallback_requests": server.hits, "scenarios": 2}, indent=2))
        print("Two loopback scenarios passed: documentation priority, generated provider discovery and sitemap fallback.")
    finally:
        server.shutdown()
        server.server_close()


if __name__ == "__main__":
    main()
