import http.server
import json
from pathlib import Path
import subprocess
import sys
import threading
import uuid

exe, out = Path(sys.argv[1]).resolve(), Path(sys.argv[2]).resolve()
sections = set(sys.argv[3:]) or {"hosts", "dns"}
out.mkdir(parents=True, exist_ok=True)
fresh = "resource-" + uuid.uuid4().hex + ".services.example"
extra = [f"extra-{i}.{fresh}" for i in range(9)]
captures = 0


class Page(http.server.BaseHTTPRequestHandler):
    def log_message(self, *_): pass
    def do_GET(self):
        body = b"<html><title>Local endpoint fixture</title></html>"
        if self.path == "/":
            body = (f'<html><script src="https://assets.localhost/app.js"></script>'
                    f'<a href="https://docs.localhost/help?key=PRIVATE">Documentation</a>'
                    f'<script src="https://cdn.usefathom.com/script.js"></script>'
                    f'<img src="https://{fresh}/image.png">'
                    + ''.join(f'<img src="https://{host}/image.png">' for host in extra)
                    + '</html>').encode()
        self.send_response(200 if self.path in ("/", "/empty") else 404)
        self.send_header("Content-Type", "text/html")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers(); self.wfile.write(body)


def capture(name, args, actions=None):
    global captures
    bmp, report, state = [out / (name + s) for s in (".bmp", ".json", ".state.json")]
    command = [str(exe), "--no-browser", "--screenshot", str(bmp), "--report-json", str(report),
               "--ui-state", str(state), *args]
    if actions:
        if "--autorun" in args:
            actions = [dict(action, at=action["at"] + 1500) for action in actions]
        path = out / (name + ".actions.json"); path.write_text(json.dumps(actions))
        delay = max(a["at"] for a in actions) + 600
        command += ["--ui-actions", str(path), "--capture-delay", str(delay)]
    subprocess.run(command, check=True, timeout=45)
    subprocess.run(["ffmpeg", "-y", "-i", str(bmp), str(out / (name + ".png")), "-loglevel", "error"], check=True)
    captures += 1
    return (json.loads(report.read_text(encoding="utf-8")) if report.exists() else None,
            json.loads(state.read_text(encoding="utf-8")))


def click(row, at=700, double=False):
    x, y = round(row["x"] + row["width"] / 2), round(row["y"] + row["height"] / 2)
    actions = [{"at": at, "kind": "move", "x": x, "y": y},
               {"at": at + 50, "kind": "down"}, {"at": at + 100, "kind": "up"}]
    if double:
        actions += [{"at": at + 190, "kind": "down"}, {"at": at + 240, "kind": "up"}]
    return actions


def scroll(width, at=700):
    return [{"at": at, "kind": "move", "x": width - 100, "y": 550},
            {"at": at + 150, "kind": "wheel", "y": -30}]


def rows(state, kind):
    return [r for r in state["endpoint_rows"] if r["key"].startswith("host:") == (kind == "host")]


def control(state, key):
    return next(c for c in state["endpoint_controls"] if c["key"] == key)


focus = [{"at": 350, "kind": "focus"}]
server = http.server.ThreadingHTTPServer(("127.0.0.1", 0), Page)
threading.Thread(target=server.serve_forever, daemon=True).start()
try:
    for width, height in (((1280, 800), (1000, 700)) if "hosts" in sections else ()):
        args = ["--target", f"http://localhost:{server.server_port}/", "--autorun", "--width", str(width), "--height", str(height)]
        r, s = capture(f"overview-{width}", args)
        hosts = {h["host"]: h for h in r["endpoint_hosts"]}
        assert len(hosts) == 13 and len(rows(s, "host")) == 5
        assert hosts["assets.localhost"]["relation"] == hosts["docs.localhost"]["relation"] == "Subdomain"
        assert hosts["cdn.usefathom.com"]["providers"] == ["Fathom Analytics"]
        assert hosts[fresh]["providers"] == [] and hosts[fresh]["relation"] == "External host"
        assert "PRIVATE" not in json.dumps(r["endpoint_hosts"])
        assert s["tab"] == 4 and s["navigation_visible"] and s["endpoint_detail"] is None
        assert all(row["height"] == 24 for row in s["endpoint_rows"])
        assert any(row["shown"].endswith("...") for row in rows(s, "host"))
        row = next(x for x in s["endpoint_rows"] if x["key"] == "host:assets.localhost")
        assert 210 < row["y"] < height - 65 and row["x"] > width / 2
        assert rows(s, "dns") and len(rows(s, "dns")) <= 5
        _, single = capture(f"single-{width}", args, focus + click(row))
        assert single["endpoint_detail"] is None and len(rows(single, "host")) == 5
        opened = focus + click(row, double=True)
        _, detail = capture(f"detail-{width}", args, opened)
        assert detail["endpoint_detail"]["key"] == row["key"] and detail["tab"] == 4
        assert detail["endpoint_detail"]["evidence"] == hosts["assets.localhost"]["evidence"]
        _, back = capture(f"back-{width}", args, opened + click(control(detail, "detail:back"), 1500))
        assert back["endpoint_detail"] is None and len(rows(back, "host")) == 5
        assert [r["key"] for r in back["endpoint_rows"]] == [r["key"] for r in s["endpoint_rows"]]
        _, keyboard = capture(f"keyboard-{width}", args, focus + click(row) + [
            {"at": 1000, "kind": "key-down", "key": 13}, {"at": 1100, "kind": "key-up", "key": 13}])
        assert keyboard["endpoint_detail"]["key"] == row["key"]
        more = focus + click(control(s, "hosts:more")) + scroll(width, 1200)
        _, expanded = capture(f"more-{width}", args, more)
        assert len(rows(expanded, "host")) == 10 and len(rows(expanded, "dns")) == len(rows(s, "dns"))
        final_actions = more + click(control(expanded, "hosts:more"), 1800)
        _, all_hosts = capture(f"all-{width}", args, final_actions)
        assert len(rows(all_hosts, "host")) == 13 and not any(c["key"] == "hosts:more" for c in all_hosts["endpoint_controls"])
        if width == 1280:
            reset = final_actions + click({"x": width - 308, "y": 10, "width": 154, "height": 34}, 2400)
            _, empty = capture("new-inspection", args, reset)
            assert empty["status"] == "empty" and empty["endpoint_host_limit"] == empty["endpoint_dns_limit"] == 5
            assert empty["endpoint_detail"] is None and not empty["endpoint_rows"]
        print(f"Host interactions passed at {width}x{height}.", flush=True)

    if "hosts" in sections:
        r, s = capture("empty", ["--target", f"http://127.0.0.1:{server.server_port}/empty", "--autorun"])
        assert not r["endpoint_hosts"] and not s["endpoint_rows"] and not s["endpoint_controls"]
    for width, height in (((1280, 800), (1000, 700)) if "dns" in sections else ()):
        args = ["--demo", "--width", str(width), "--height", str(height)]
        scrolled = focus + scroll(width)
        r, s = capture(f"dns-{width}", args, scrolled)
        assert sum(len(q["records"]) for q in r["dns"]["queries"]) == 6
        assert len(rows(s, "dns")) == len(rows(s, "host")) == 5
        if width == 1000:
            assert any(row["shown"].endswith("...") for row in rows(s, "dns"))
        all_actions = scrolled + click(control(s, "dns:more"), 1400) + scroll(width, 1900)
        _, loaded = capture(f"dns-more-{width}", args, all_actions)
        assert len(rows(loaded, "dns")) == 6 and len(rows(loaded, "host")) == 5
        assert not any(c["key"] == "dns:more" for c in loaded["endpoint_controls"])
        row = next(x for x in loaded["endpoint_rows"] if x["key"] == "dns:1:0")
        assert "MX" in row["title"] and "Microsoft 365 mail" in row["detail"]
        assert 210 < row["y"] < height - 65, row
        opened = all_actions + click(row, 2400, double=True)
        _, detail = capture(f"dns-detail-{width}", args, opened)
        assert detail["endpoint_detail"]["key"] == "dns:1:0" and detail["tab"] == 4
        assert 212 <= control(detail, "detail:back")["y"] < height - 65
        fields = {e["source"]: e["value"] for e in detail["endpoint_detail"]["evidence"]}
        assert fields["Record"] == "0 fixture.mail.protection.outlook.com."
        assert fields["Owner"] == "example.test" and fields["Resolver"] == "Illustrative data"
        _, back = capture(f"dns-back-{width}", args, opened + click(control(detail, "detail:back"), 3100))
        assert back["endpoint_detail"] is None and len(rows(back, "dns")) == 6
        assert len(rows(back, "host")) == 5 and back["endpoint_dns_limit"] == 6
        assert all(abs(a["y"] - b["y"]) <= 1 for a, b in zip(back["endpoint_rows"], loaded["endpoint_rows"]))
        assert r["status"] == "demo"
        print(f"DNS interactions passed at {width}x{height}.", flush=True)
    (out / "verification.json").write_text(json.dumps({"scenarios": captures, "sections": sorted(sections), "sizes": [[1280, 800], [1000, 700]],
        "network": "loopback only; browser disabled", "checks": ["five rows each", "independent Load more", "single click stays in list",
        "double-click evidence", "Enter evidence", "back retains limits", "new inspection resets limits", "long values", "empty state"]}, indent=2))
    print(f"{captures} native compact endpoint scenarios passed.")
finally:
    server.shutdown(); server.server_close()
