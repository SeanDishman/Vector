import argparse
import hashlib
from html.parser import HTMLParser
import json
from pathlib import Path
import re
import subprocess

from provider_discovery_integration import Fixture


def local_document(body):
    text = body.decode("utf-8")
    offsets = [0]
    offsets.extend(match.end() for match in re.finditer("\n", text))

    class Links(HTMLParser):
        def __init__(self):
            super().__init__(convert_charrefs=False)
            self.edits = []

        def handle_starttag(self, tag, attrs):
            raw = self.get_starttag_text()
            replacement = re.sub(r'''(?i)(\s+href\s*=\s*)(?:"[^"]*"|'[^']*'|[^\s>]+)''',
                                 r'\1"#"', raw)
            if replacement != raw:
                line, column = self.getpos()
                offset = offsets[line - 1] + column
                self.edits.append((offset, len(raw), replacement))

        handle_startendtag = handle_starttag

    links = Links()
    links.feed(text)
    for offset, length, replacement in reversed(links.edits):
        text = text[:offset] + replacement + text[offset + length:]
    return text.encode("utf-8"), len(links.edits)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("exe", type=Path)
    parser.add_argument("--capture", type=Path, default=Path("docs/reports/provider-discovery"))
    parser.add_argument("--out", type=Path, default=Path("docs/reports/provider-discovery/document-replay"))
    parser.add_argument("--record-only", action="store_true")
    args = parser.parse_args()
    args.out.mkdir(parents=True, exist_ok=True)
    metadata = json.loads((args.capture / "default-source.json").read_text(encoding="utf-8"))
    body = (args.capture / "default.html").read_bytes()
    assert hashlib.sha256(body).hexdigest() == metadata["sha256"], "Retained source changed"
    expected = set(metadata["headings"])
    assert len(expected) == 22, "Expected capture must be independently checked again if replaced"
    replay_body, disabled_links = local_document(body)
    fixture = Fixture()
    path = "/legal/subprocessors"
    fixture.routes = {"/": (200, '<a href="' + path + '">Subprocessors</a>', "text/html"),
                      path: (200, replay_body, "text/html")}
    result = {"source": metadata["url"], "source_sha256": metadata["sha256"],
              "replay_target": fixture.url(), "expected_count": len(expected),
              "replay_sha256": hashlib.sha256(replay_body).hexdigest(),
              "disabled_document_links": disabled_links,
              "replay_adaptation": "Only href attributes changed to #; provider text and structure retained",
              "executable_sha256": hashlib.sha256(args.exe.read_bytes()).hexdigest()}
    try:
        run = subprocess.run([str(args.exe.resolve()), "--target", fixture.url(), "--no-browser",
                              "--timeout", "150", "--http-timeout", "600"], capture_output=True,
                             text=True, encoding="utf-8", timeout=20)
        assert run.returncode == 0, run.stderr
        report = json.loads(run.stdout)
        result["requested_paths"] = list(fixture.hits)
        (args.out / "report.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
        published = [v for v in report["vendors"] if v.get("published_by")]
        names = {v["name"] for v in published}
        result.update(actual_count=len(names), missing=sorted(expected - names), unexpected=sorted(names - expected))
        assert names == expected, result
        assert all(v["evidence_level"] == "Published disclosure" and v.get("recorded_at") for v in published)
        for vendor in published:
            assert any(e["source"].endswith(path) and vendor["name"] in e["value"]
                       for e in vendor["evidence"]), vendor
        assert not expected & {f["name"] for f in report["findings"]}, "Published rows became detected software"
        assert report["web_coverage"]["page_requests"] <= 12
        result["passed"] = True
    except Exception as error:
        result["passed"] = False
        result["failure"] = str(error)
    finally:
        fixture.shutdown()
        fixture.server_close()
    (args.out / "verification.json").write_text(json.dumps(result, indent=2), encoding="utf-8")
    print(f"Captured public document: {result.get('actual_count', 0)}/{len(expected)} provider names; "
          + ("passed" if result["passed"] else "FAILED"))
    return 0 if args.record_only or result["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
