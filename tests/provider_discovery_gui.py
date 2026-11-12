import json
from pathlib import Path
import subprocess
import sys
from PIL import Image
from provider_discovery_integration import Fixture, provider_table

exe, out = Path(sys.argv[1]).resolve(), Path(sys.argv[2]).resolve()
out.mkdir(parents=True, exist_ok=True)
fixture = Fixture()
names = ["Cloud supplier from this document", "Messaging supplier from this document"]
fixture.routes = {"/": (200, '<a href="/legal">Legal</a>', "text/html"),
                  "/legal": (200, '<a aria-label="Open to Subprocessors" href="/document/827">Open</a>', "text/html"),
                  "/document/827": (200, provider_table(names), "text/html")}
try:
    for label, args in (("overview", []), ("compact", ["--width", "1000", "--height", "700"]),
                        ("provider-evidence", ["--view", "3", "--vendor-filter", "Cloud supplier"]),
                        ("source-outcomes", ["--view", "2"])):
        bmp, report = out / (label + ".bmp"), out / (label + ".json")
        subprocess.run([str(exe), "--target", fixture.url(), "--autorun", "--no-browser",
                        "--screenshot", str(bmp), "--report-json", str(report), *args], check=True, timeout=35)
        result = json.loads(report.read_text(encoding="utf-8"))
        providers = [v for v in result["vendors"] if v.get("published_by")]
        assert {v["name"] for v in providers} == set(names)
        assert all(v["evidence_level"] == "Published disclosure" for v in providers)
        assert all("Discovery:" in v["evidence"][0]["value"] for v in providers)
        Image.open(bmp).save(out / (label + ".png"))
    print("Four native discovery views passed; providers came from the linked document.")
finally:
    fixture.shutdown()
    fixture.server_close()
