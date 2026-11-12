import argparse
from collections import Counter
import hashlib
import json
import math
from pathlib import Path
import re
import unicodedata

from bs4 import BeautifulSoup


def flight_strings(html):
    chunks = []
    for match in re.finditer(r"self\.__next_f\.push\(", html):
        try:
            value, _ = json.JSONDecoder().raw_decode(html[match.end():])
        except ValueError:
            continue
        if isinstance(value, list) and len(value) > 1 and isinstance(value[1], str):
            chunks.append(value[1])
    return "".join(chunks)


def logo_slugs(html):
    flight = flight_strings(html)
    marker = '"logos":'
    start = flight.index(marker) + len(marker)
    logos, _ = json.JSONDecoder().raw_decode(flight[start:])
    return {logo["slug"] for logo in logos}


def ranked_items(items, logos):
    def score(item):
        return item["confidence"] + 0.5 * (item["slug"] in logos) + 0.3 * (item["source"] != "detected")

    unique = {}
    for item in sorted(items, key=score, reverse=True):
        unique.setdefault(item["slug"], {**item, "client_score": score(item)})
    return list(unique.values())


def layout_capacity(count, viewport_width, anchor_top):
    if count <= 0:
        return 0
    width = min(viewport_width - (400 if viewport_width >= 1024 else 24), 1120) - 36
    height = max(0, anchor_top - 18 - 84 - 36)

    def columns(side):
        return max(1, math.floor((width + 14) / (side + 14)))

    def rows_height(rows, side):
        return rows * (side + 22) + (rows - 1) * 10

    side = 34
    preferred = math.floor(max(48, min(84, viewport_width / 14)) + 0.5)
    for candidate in range(preferred, 33, -2):
        if rows_height(math.ceil(count / columns(candidate)), candidate) <= height:
            side = candidate
            break
    return min(count, columns(side) * max(1, math.floor((height + 10) / (side + 22 + 10))))


def name_key(value):
    value = unicodedata.normalize("NFKD", value).encode("ascii", "ignore").decode()
    words = re.findall(r"[a-z0-9]+", value.lower())
    while words and words[-1] in {"limited", "ltd", "inc", "incorporated", "corporation", "corp", "llc"}:
        words.pop()
    return " ".join(words)


def primary_rows(capture):
    manifest = json.loads((capture / "primary-sources.json").read_text(encoding="utf-8"))
    sources, rows = [], []
    for entry in manifest["sources"]:
        if "error" in entry:
            sources.append(entry)
            continue
        raw = (capture / entry["file"]).read_bytes()
        assert hashlib.sha256(raw).hexdigest() == entry["sha256"], entry["file"]
        soup = BeautifulSoup(raw, "html.parser")
        for element in soup(["script", "style", "noscript"]):
            element.decompose()
        tables = soup.find_all("table")
        start_count = len(rows)
        for table_number, table in enumerate(tables, 1):
            for row_number, row in enumerate(table.find_all("tr"), 1):
                cells = [" ".join(cell.stripped_strings) for cell in row.find_all(["th", "td"], recursive=False)]
                if len(cells) < 3 or not cells[0]:
                    continue
                if name_key(cells[0]) in {"entity name", "name", "company", "provider"}:
                    continue
                if not any("country" in header.get_text(" ", strip=True).lower() for header in table.find_all("th")):
                    if "country" not in table.find("tr").get_text(" ", strip=True).lower():
                        continue
                rows.append({"name": cells[0], "cells": cells, "url": entry["url"],
                             "table": table_number, "row": row_number})
        sources.append({**entry, "tables": len(tables), "provider_rows": len(rows) - start_count,
                        "first_table_byte": raw.lower().find(b"<table")})
    return sources, rows


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("capture", type=Path)
    parser.add_argument("--viewport-width", type=int, default=1540)
    parser.add_argument("--anchor-top", type=int, default=420)
    args = parser.parse_args()
    capture = args.capture
    response = json.loads((capture / "google-lookup.json").read_text(encoding="utf-8"))
    items = response["items"]
    assert response["toolCount"] == len(items)
    logos = logo_slugs((capture / "home.html").read_text(encoding="utf-8"))
    ranked = ranked_items(items, logos)
    sources, rows = primary_rows(capture)
    by_name = {}
    for row in rows:
        by_name.setdefault(name_key(row["name"]), []).append(row)
    provider_items = [item for item in items if item["source"] == "subprocessor"]
    comparisons = [{"name": item["name"], "slug": item["slug"],
                    "matches": by_name.get(name_key(item["name"]), [])} for item in provider_items]
    shown = layout_capacity(len(ranked), args.viewport_width, args.anchor_top)
    output = {
        "domain": response["domain"], "items": len(items), "unique_slugs": len(ranked),
        "source_counts": dict(Counter(item["source"] for item in items)),
        "detected_claims": [item for item in items if item["source"] == "detected"],
        "local_logo_count": len(logos), "primary_sources": sources,
        "normalized_name_matches": sum(bool(item["matches"]) for item in comparisons),
        "unmatched_names": [item["name"] for item in comparisons if not item["matches"]],
        "header_like_claims": [item for item in items if name_key(item["name"]) == "entity name"],
        "layout_example": {"viewport_width": args.viewport_width, "anchor_top": args.anchor_top,
                           "shown": shown, "note": "Illustrative dimensions, not measured from the user's cropped screenshot."},
        "ranked_items": ranked,
        "comparison_method": "Normalized names locate candidate source rows; this is not an item-to-source map supplied by StackFull. Unmatched names are not automatically false.",
    }
    (capture / "analysis.json").write_text(json.dumps(output, indent=2, ensure_ascii=False), encoding="utf-8")
    (capture / "provider-source-matches.json").write_text(json.dumps(comparisons, indent=2, ensure_ascii=False), encoding="utf-8")
    print(json.dumps({key: output[key] for key in ("items", "source_counts", "normalized_name_matches", "layout_example")}, indent=2))
    for source in sources:
        print(source["file"], source.get("provider_rows"), "rows; first table at byte", source.get("first_table_byte"))


if __name__ == "__main__":
    main()
