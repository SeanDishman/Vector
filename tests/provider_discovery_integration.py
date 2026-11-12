import argparse
import hashlib
import html
import http.server
import json
from pathlib import Path
import subprocess
import threading
import uuid


def provider_table(names, heading="Subprocessors"):
    return ("<h1>" + heading + "</h1><table><tr><th>Company</th><th>Purpose</th>"
            "<th>Country</th></tr>" + "".join(
                "<tr><td>" + html.escape(name) + "</td><td>Hosting customer data</td>"
                "<td>Canada</td></tr>" for name in names) + "</table>")


class Fixture(http.server.ThreadingHTTPServer):
    def __init__(self):
        super().__init__(("127.0.0.1", 0), Handler)
        self.routes = {}
        self.hits = []
        self.host = "provider-" + uuid.uuid4().hex[:16] + ".localhost"
        threading.Thread(target=self.serve_forever, daemon=True).start()

    def url(self, path="/"):
        return f"http://{self.host}:{self.server_port}{path}"


class Handler(http.server.BaseHTTPRequestHandler):
    def log_message(self, *_):
        pass

    def do_GET(self):
        self.server.hits.append((self.headers.get("Host", ""), self.path))
        status, body, mime = self.server.routes.get(self.path, (404, "Not found", "text/html"))
        body = body.encode("utf-8") if isinstance(body, str) else body
        try:
            self.send_response(status)
            self.send_header("Content-Type", mime)
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)
        except (BrokenPipeError, ConnectionResetError, ConnectionAbortedError):
            pass


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("exe", type=Path)
    parser.add_argument("--out", type=Path, default=Path("docs/reports/provider-discovery/integration"))
    parser.add_argument("--record-only", action="store_true")
    args = parser.parse_args()
    exe = args.exe.resolve()
    args.out.mkdir(parents=True, exist_ok=True)
    nonce = uuid.uuid4().hex
    names = ["Hosting " + nonce, "Delivery " + nonce]
    changed_names = ["Replacement " + uuid.uuid4().hex]
    noise = "".join(f'<a href="/products/{i}">Product {i}</a>' for i in range(80))
    results = []

    def exercise(label, routes, expected, source_path=None, same_fixture=None, forbidden=None, error_status=None, chain_labels=None):
        fixture = same_fixture or Fixture()
        if callable(routes):
            routes = routes(fixture)
        fixture.routes = {path: value if isinstance(value, tuple) else (200, value, "text/html")
                          for path, value in routes.items()}
        fixture.hits.clear()
        result = {"scenario": label, "target": fixture.url(), "expected_names": expected}
        try:
            run = subprocess.run([str(exe), "--target", fixture.url(), "--no-browser",
                                  "--timeout", "150", "--http-timeout", "600"],
                                 capture_output=True, text=True, encoding="utf-8", timeout=15)
            assert run.returncode == 0, run.stderr[-2000:]
            report = json.loads(run.stdout)
            (args.out / (label + ".json")).write_text(json.dumps(report, indent=2), encoding="utf-8")
            published = [v for v in report["vendors"] if v.get("published_by")]
            result["actual_names"] = [v["name"] for v in published]
            result["requested_paths"] = list(fixture.hits)
            assert set(result["actual_names"]) == set(expected), result["actual_names"]
            assert not any(v["evidence_level"] != "Published disclosure" for v in published), published
            assert not {f["name"] for f in report["findings"]} & set(expected), "Provider rows became software findings"
            assert report["web_coverage"]["page_requests"] <= 12, report["web_coverage"]
            assert report["web_coverage"]["elapsed_ms"] < 31000, report["web_coverage"]
            for provider in published:
                assert provider.get("recorded_at") and provider.get("evidence"), provider
                assert any(provider["name"] in e["value"] for e in provider["evidence"]), provider
                if source_path:
                    assert any(e["source"].endswith(source_path) for e in provider["evidence"]), provider
                    assert any("Hosting customer data" in e["value"] for e in provider["evidence"]), provider
            if source_path and expected:
                assert sum(path == source_path for _, path in fixture.hits) == 1, fixture.hits
                assert any(d["state"] == "collected" and d["url"].endswith(source_path)
                           for d in report["disclosures"]), report["disclosures"]
            if chain_labels:
                document = next(d for d in report["disclosures"] if d["url"].endswith(source_path))
                chain = document.get("discovery_chain", [])
                assert len(chain) == len(chain_labels), chain
                for step, label_text in zip(chain, chain_labels):
                    assert "Link: " + label_text + " -> " in step["value"], step
                    assert step["source"].startswith(fixture.url()), step
                    assert all(any(step["source"] in e["value"] and step["value"] in e["value"]
                                   for e in provider["evidence"]) for provider in published), published
            if forbidden:
                assert not any(forbidden in host + path for host, path in fixture.hits), fixture.hits
            if error_status:
                assert any(d["http_status"] == error_status and d["state"] == "failed"
                           for d in report["disclosures"]), report["disclosures"]
            result["passed"] = True
        except Exception as error:
            result["passed"] = False
            result["failure"] = str(error)
        finally:
            if same_fixture is None:
                fixture.shutdown()
                fixture.server_close()
        results.append(result)
        print(label + ": " + ("passed" if result["passed"] else "FAILED: " + result["failure"][:220]))

    direct = "/subprocessors/" + nonce
    exercise("direct-unseen-provider", {"/": '<a href="' + direct + '">Our processors</a>',
                                       direct: provider_table(names)}, names, direct)
    opaque = "/documents/" + nonce
    exercise("opaque-link-label", {"/": noise + '<footer><a href="' + opaque + '">Subprocessor list</a></footer>',
                                   opaque: provider_table(names)}, names, opaque)
    exercise("legal-link-chain", {"/": noise + '<footer><a href="/legal">Legal</a></footer>',
                                  "/legal": '<h1>Legal</h1><a href="/policies">Privacy policy</a>',
                                  "/policies": '<h1>Data protection</h1><a href="' + opaque + '">Subprocessors</a>',
                                  opaque: provider_table(names)}, names, opaque,
             chain_labels=["Legal", "Privacy policy", "Subprocessors"])
    duplicate = '<a href="' + opaque + '">Subprocessors</a>'
    exercise("duplicate-source", {"/": duplicate + duplicate, opaque: provider_table(names)}, names, opaque)

    refresh = Fixture()
    try:
        exercise("first-document", {"/": '<a href="' + direct + '">Subprocessors</a>', direct: provider_table(names)},
                 names, direct, same_fixture=refresh)
        exercise("changed-document", {"/": '<a href="' + direct + '">Subprocessors</a>', direct: provider_table(changed_names)},
                 changed_names, direct, same_fixture=refresh)
    finally:
        refresh.shutdown()
        refresh.server_close()

    exercise("rejected-source", {"/": '<a href="' + direct + '">Subprocessors</a>',
                                 direct: (403, provider_table(names), "text/html")}, [], error_status=403)
    exercise("customer-table", {"/": '<a href="' + direct + '">Subprocessors</a>',
                                direct: provider_table(names, "Our customers")}, [])
    exercise("hidden-table", {"/": '<a href="' + direct + '">Subprocessors</a>',
                              direct: '<div hidden>' + provider_table(names) + '</div>'}, [])
    exercise("script-example", {"/": '<script>const example=`<a href="/subprocessors-fake">Subprocessors</a>`;</script>',
                                "/subprocessors-fake": provider_table(names)}, [], forbidden="/subprocessors-fake")
    exercise("unrelated-source", lambda fixture: {"/": '<a href="http://unrelated.localhost:' + str(fixture.server_port) + '/subprocessors">Subprocessors</a>'},
             [], forbidden="unrelated.localhost")
    exercise("no-disclosure", {"/": '<h1>Company homepage</h1><p>Private infrastructure information is not published here.</p>'}, [])
    summary = {"exe": str(exe), "executable_sha256": hashlib.sha256(exe.read_bytes()).hexdigest(),
               "run_nonce": nonce, "passed": sum(r["passed"] for r in results),
               "total": len(results), "results": results, "network_scope": "Loopback only; browser disabled"}
    (args.out / "verification.json").write_text(json.dumps(summary, indent=2), encoding="utf-8")
    print(f"{summary['passed']}/{len(results)} generic discovery scenarios passed.")
    return 0 if args.record_only or all(r["passed"] for r in results) else 1


if __name__ == "__main__":
    raise SystemExit(main())
