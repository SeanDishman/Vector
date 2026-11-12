import argparse
import hashlib
import json
from pathlib import Path
import subprocess
from urllib.parse import urlsplit


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("exe", type=Path)
    parser.add_argument("documents", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    exe, documents, output = args.exe.resolve(), args.documents.resolve(), args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    fixtures = json.loads((Path(__file__).parent / "fixtures/disclosures/provider-tables.json").read_text(encoding="utf-8"))
    manifest = json.loads((documents / "manifest.json").read_text(encoding="utf-8"))
    summary = []
    for source in manifest["sources"]:
        path = documents / (source["name"] + ".html")
        assert hashlib.sha256(path.read_bytes()).hexdigest() == source["sha256"]
        target = urlsplit(source["url"]).hostname
        run = subprocess.run([str(exe), "--document", str(path), source["url"], target],
                             capture_output=True, timeout=30)
        assert run.returncode == 0, run.stderr.decode("utf-8", "replace")
        report = json.loads(run.stdout)
        record = next(record for record in fixtures["records"] if record["source"] == source["url"])
        expected = {row[0].casefold() for table in record["tables"] for row in table["rows"]}
        actual = {provider["name"].casefold() for provider in report["vendors"]}
        assert actual == expected, {"source": source["name"], "missing": sorted(expected - actual), "extra": sorted(actual - expected)}
        assert not report["findings"]
        assert all(provider["published_by"] and provider["evidence_level"] == "Published disclosure"
                   for provider in report["vendors"])
        matched_rows = 0
        for table in record["tables"]:
            for row in table["rows"]:
                provider = next(provider for provider in report["vendors"] if provider["name"].casefold() == row[0].casefold())
                assert any(evidence["source"] == source["url"] and
                           "Heading: " + table["heading"] in evidence["value"] and
                           all("\n" + column + ": " + cell in evidence["value"]
                               for column, cell in zip(table["columns"], row))
                           for evidence in provider["evidence"]), (source["name"], row)
                matched_rows += 1
        (output / (source["name"] + ".json")).write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
        summary.append({"source": source["url"], "document_sha256": source["sha256"],
                        "matched_names": len(actual), "matched_complete_rows": matched_rows})
    (output / "verification.json").write_text(json.dumps(summary, indent=2), encoding="utf-8")
    print(json.dumps({"document_extraction": summary, "network_requests": 0}, indent=2))


if __name__ == "__main__":
    main()
