import argparse
import json
from pathlib import Path
import sys


def verify(source_root, package=None, binary=None):
    problems = []
    wiring_tokens = (
        "VECTOR_DISCLOSURE_CATALOG", "disclosure_catalog.hpp", "apply_disclosure_references",
        "disclosure_reference_urls", "data/published-vendors.json",
    )
    wiring_paths = [source_root / "CMakeLists.txt", source_root / "scripts/package.ps1"]
    wiring_paths += sorted((source_root / "src").glob("*.cpp"))
    wiring_paths += sorted((source_root / "src/include/vector").glob("*.hpp"))
    for path in wiring_paths:
        content = path.read_text(encoding="utf-8")
        for token in wiring_tokens:
            if token in content:
                problems.append(f"Runtime catalog wiring remains: {path.relative_to(source_root)}: {token}")

    fixtures = json.loads((source_root / "tests/fixtures/disclosures/provider-tables.json").read_text(encoding="utf-8"))
    sentinels = sorted({row[0] for record in fixtures["records"]
                        for table in record.get("tables", []) for row in table["rows"]
                        if len(row[0]) >= 60})
    assert len(sentinels) >= 3, "The artifact check needs distinctive former-catalog strings"
    icon_aliases = {}
    icon_manifest = source_root / "src/assets/technology/manifest.json"
    if icon_manifest.is_file():
        for icon in json.loads(icon_manifest.read_text(encoding="utf-8"))["icons"]:
            for alias in icon["names"]:
                icon_aliases[alias] = icon["key"]
    visual_aliases = {}
    catalog_markers = [b'"publisher": "google.com"', b'"publisher": "x.com"',
                       b'"publisher":"google.com"', b'"publisher":"x.com"',
                       b"apply_disclosure_references", b"disclosure_reference_urls"]
    binaries = []
    if package:
        for name in ("Vector.exe", "vector-cli.exe"):
            path = package / name
            if not path.is_file():
                problems.append(f"Missing packaged executable: {name}")
            else:
                binaries.append(path)
        for path in package.rglob("*"):
            if path.is_file() and path.name in {"published-vendors.json", "disclosure_catalog.hpp", "provider-tables.json"}:
                problems.append(f"Test or catalog data shipped: {path.relative_to(package)}")
    if binary:
        binaries.append(binary)
    for path in binaries:
        content = path.read_bytes()
        for marker in catalog_markers:
            if marker in content:
                problems.append(f"Compiled catalog marker in {path.name}: {marker.decode('ascii')}")
        for name in sentinels:
            if name.encode("utf-8") in content or name.encode("utf-16-le") in content:
                if path.name == "Vector.exe" and name in icon_aliases:
                    visual_aliases[name] = icon_aliases[name]
                else:
                    problems.append(f"Former provider catalog name embedded in {path.name}: {name}")
    if problems:
        raise AssertionError("\n".join(problems))
    return {"runtime_wiring_files_checked": len(wiring_paths),
            "executables_checked": [str(path) for path in binaries],
            "distinctive_catalog_names_checked": len(sentinels),
            "gui_display_aliases_accounted_for": visual_aliases,
            "package_contains_provider_catalog": False if package else None,
            "network_requests": 0}


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--package", type=Path)
    parser.add_argument("--binary", type=Path)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    try:
        result = verify(Path(__file__).resolve().parents[1],
                        args.package.resolve() if args.package else None,
                        args.binary.resolve() if args.binary else None)
    except (AssertionError, OSError) as error:
        print(f"FAIL: {error}", file=sys.stderr)
        raise SystemExit(1)
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(json.dumps(result, indent=2), encoding="utf-8")
    print(json.dumps(result, indent=2))
