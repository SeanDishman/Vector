import csv
import json
from pathlib import Path
import subprocess
import sys

def leaves(value, path=""):
    if isinstance(value, dict) and value:
        for key, item in value.items():
            yield from leaves(item, path + "/" + key.replace("~", "~0").replace("/", "~1"))
    elif isinstance(value, list) and value:
        for index, item in enumerate(value):
            yield from leaves(item, path + "/" + str(index))
    else:
        yield path, value

def text_fields(text):
    assert text.startswith("Vector inspection report\n")
    parents, fields = [], {}
    for line in text.splitlines()[1:]:
        if not line:
            continue
        depth = (len(line) - len(line.lstrip(" "))) // 2
        key, delimiter, literal = line.strip().partition(":")
        assert delimiter
        if key.startswith("["):
            assert key.endswith("]")
            key = key[1:-1]
        key = key.replace("~", "~0").replace("/", "~1")
        parents = parents[:depth] + [key]
        if literal:
            path = "/" + "/".join(parents)
            assert path not in fields, path
            fields[path] = json.loads(literal)
    return fields

def read_csv(path):
    with path.open(encoding="utf-8", newline="") as file:
        rows = list(csv.DictReader(file))
    actual = {}
    for row in rows:
        assert row.keys() == {"Field", "Type", "Value"}
        assert row["Field"] not in actual
        value = json.loads(row["Value"])
        actual[row["Field"]] = value
        if row["Type"] == "string":
            assert row["Value"].startswith('"'), "CSV text must not become a spreadsheet formula"
    return actual

if __name__ == "__main__":
    exe, out = Path(sys.argv[1]).resolve(), Path(sys.argv[2]).resolve()
    subprocess.run([str(exe), str(out)], check=True)
    count = 0
    for name in ("complete", "partial", "empty"):
        expected = dict(leaves(json.loads((out / (name + ".json")).read_text(encoding="utf-8"))))
        assert read_csv(out / (name + ".csv")) == expected, f"{name}: CSV dropped or altered fields"
        assert text_fields((out / (name + ".txt")).read_text(encoding="utf-8")) == expected, f"{name}: TXT dropped or altered fields"
        count += len(expected)
    print(f"CSV and TXT preserve all {count} JSON fields across complete, partial and empty reports.")
