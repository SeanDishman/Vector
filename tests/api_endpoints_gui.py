import http.server
import json
from pathlib import Path
import subprocess
import sys
import threading

exe, out = Path(sys.argv[1]).resolve(), Path(sys.argv[2]).resolve()
sections = set(sys.argv[3:]) or {"demo", "static", "browser"}
out.mkdir(parents=True, exist_ok=True)
LONG = "/api/v1/workspaces/reporting/exports/scheduled/deliveries/recipients/preferences/notifications/settings"
PATHS = ["/api/v1/items", "/api/v1/session", "/api/v1/orders", "/api/v1/prices", "/api/v1/alerts", LONG]


class Page(http.server.BaseHTTPRequestHandler):
    def log_message(self, *_):
        pass

    def reply(self, status, mime, body):
        data = body.encode()
        self.send_response(status)
        self.send_header("Content-Type", mime)
        self.send_header("Content-Length", str(len(data)))
        self.end_headers()
        self.wfile.write(data)

    def do_GET(self):
        path = self.path.split("?", 1)[0]
        if path == "/":
            calls = "".join(f'fetch("{p}?key=PRIVATE");' for p in PATHS)
            self.reply(200, "text/html", '<!doctype html><html><head><title>API list fixture</title>'
                       '<link rel="https://api.w.org/" href="/wp-json/"></head><body>'
                       f'<script>{calls}</script></body></html>')
        elif path == "/empty":
            self.reply(200, "text/html", "<!doctype html><html><title>No endpoints</title><p>Plain page.</p></html>")
        elif path.startswith("/api/"):
            self.reply(200, "application/json", '{"ok":true}')
        else:
            self.reply(404, "text/html", "<!doctype html><html><title>Not found</title></html>")


def capture(name, args, actions=None, timeout=60):
    bmp, report, state = [out / (name + s) for s in (".bmp", ".json", ".state.json")]
    for old in (bmp, report, state):
        old.unlink(missing_ok=True)
    command = [str(exe), "--screenshot", str(bmp), "--report-json", str(report), "--ui-state", str(state), *args]
    if actions:
        if "--autorun" in args:
            actions = [dict(action, at=action["at"] + 1500) for action in actions]
        path = out / (name + ".actions.json")
        path.write_text(json.dumps(actions))
        command += ["--ui-actions", str(path), "--capture-delay", str(max(a["at"] for a in actions) + 600)]
    subprocess.run(command, check=True, timeout=timeout)
    subprocess.run(["ffmpeg", "-y", "-i", str(bmp), str(out / (name + ".png")), "-loglevel", "error"], check=True)
    return (json.loads(report.read_text(encoding="utf-8")) if report.exists() else None,
            json.loads(state.read_text(encoding="utf-8")))


def click(box, at=1000, double=False):
    x, y = round(box["x"] + box["width"] / 2), round(box["y"] + box["height"] / 2)
    actions = [{"at": at, "kind": "move", "x": x, "y": y}, {"at": at + 50, "kind": "down"}, {"at": at + 100, "kind": "up"}]
    if double:
        actions += [{"at": at + 190, "kind": "down"}, {"at": at + 240, "kind": "up"}]
    return actions


def control(state, key):
    return next(c for c in state["api_controls"] if c["key"] == key)


def hosts(state):
    return [r for r in state["endpoint_rows"] if r["key"].startswith("host:")]


def on_screen(box, height):
    return 210 < box["y"] and box["y"] + box["height"] <= height - 34


focus = [{"at": 350, "kind": "focus"}]
wheel = focus + [{"at": 450, "kind": "move", "x": 300, "y": 400}, {"at": 550, "kind": "wheel", "y": -2}]


def reach(name, args, state, height):
    if all(on_screen(box, height) for box in state["api_rows"] + [control(state, "api:more")]):
        return focus, state
    _, moved = capture(name, args, wheel)
    assert [r["key"] for r in moved["api_rows"]] == [r["key"] for r in state["api_rows"]], "scrolling keeps the same rows"
    assert moved["api_rows"][0]["y"] < state["api_rows"][0]["y"], "the list scrolls with the report"
    assert all(on_screen(box, height) for box in moved["api_rows"] + [control(moved, "api:more")]), moved["api_rows"]
    return wheel, moved


server = http.server.ThreadingHTTPServer(("127.0.0.1", 0), Page)
threading.Thread(target=server.serve_forever, daemon=True).start()
base = f"http://127.0.0.1:{server.server_port}"
try:
    for width, height in (((1280, 800), (1000, 700)) if "demo" in sections else ()):
        args = ["--demo", "--width", str(width), "--height", str(height)]
        report, state = capture(f"demo-{width}", args)
        listed = report["api_endpoints"]
        assert len(listed) == 7 == state["api_endpoint_count"] and state["api_endpoint_limit"] == 5, state["api_endpoint_count"]
        first = state["api_rows"]
        assert len(first) == 5 and state["api_section_shown"] and state["api_detail"] is None
        assert [r["title"] for r in first] == [(e["method"] + " " + e["url"]).strip() for e in listed[:5]], first
        assert first[0]["tag"] == "Declared" and {r["tag"] for r in first} <= {"Declared", "robots.txt", "In code"}, first
        assert all(r["height"] == 24 and r["x"] < width / 2 and r["x"] + r["width"] < width * 0.62 for r in first), first
        assert all(b["y"] - a["y"] == 24 for a, b in zip(first, first[1:])), first
        assert on_screen(first[0], height), first[0]
        assert first[0]["shown"].strip() == "/wp-json/", first[0]
        assert control(state, "api:more")["y"] > first[-1]["y"] and not any(c["key"].startswith("api:") for c in state["endpoint_controls"])
        assert len(hosts(state)) == 5 and all(r["x"] > width / 2 for r in hosts(state)), "host list keeps its column"
        assert not any(r["key"].startswith("api:") for r in state["endpoint_rows"]), "API rows stay out of the connection rows"
        if width == 1280:
            assert all(on_screen(box, height) for box in first + [control(state, "api:more")]), "five rows and Load more fit the default window"

        lead, shown = reach(f"demo-scrolled-{width}", args, state, height)
        rows, more = shown["api_rows"], control(shown, "api:more")

        _, single = capture(f"demo-single-{width}", args, lead + click(rows[1]))
        assert single["api_detail"] is None and len(single["api_rows"]) == 5

        opened = lead + click(rows[0], double=True)
        _, detail = capture(f"demo-detail-{width}", args, opened)
        proof = detail["api_detail"]
        assert proof["key"] == rows[0]["key"] and proof["title"] == listed[0]["url"], proof
        assert proof["detail"] == "Same host / Page declaration", proof
        assert proof["evidence"][:-1] == listed[0]["evidence"] and proof["evidence"][-1]["source"] == "Meaning", proof
        assert "not requested" in proof["evidence"][-1]["value"], proof
        assert detail["tab"] == 4 and detail["endpoint_detail"] is None and len(hosts(detail)) == 5
        assert not detail["api_rows"] and on_screen(control(detail, "api:back"), height)

        _, back = capture(f"demo-back-{width}", args, opened + click(control(detail, "api:back"), 1900))
        assert back["api_detail"] is None and [r["key"] for r in back["api_rows"]] == [r["key"] for r in rows]
        assert all(abs(a["y"] - b["y"]) <= 1 for a, b in zip(back["api_rows"], rows)), "Back restores the scroll position"

        _, keyboard = capture(f"demo-keyboard-{width}", args, lead + click(rows[2]) + [
            {"at": 1300, "kind": "key-down", "key": 13}, {"at": 1400, "kind": "key-up", "key": 13}])
        assert keyboard["api_detail"]["key"] == rows[2]["key"]

        loaded = lead + click(more)
        _, expanded = capture(f"demo-more-{width}", args, loaded)
        assert len(expanded["api_rows"]) == 7 and expanded["api_endpoint_limit"] == 7
        assert not any(c["key"] == "api:more" for c in expanded["api_controls"]), "Load more disappears when exhausted"
        assert len(hosts(expanded)) == 5 and expanded["endpoint_host_limit"] == 5, "other lists keep their own counts"
        assert expanded["infrastructure_limit"] == 5
        assert [r["tag"] for r in expanded["api_rows"]][-1] == "In code"
        if width == 1280:
            _, empty = capture("demo-new-search", args, loaded + click({"x": width - 308, "y": 10, "width": 154, "height": 34}, 1800))
            assert empty["status"] == "empty" and empty["api_endpoint_limit"] == 5 and not empty["api_rows"]
            assert empty["api_detail"] is None and empty["api_endpoint_count"] == 0
        print(f"Example-data API endpoint list passed at {width}x{height}.", flush=True)

    for width, height in (((1280, 800), (1000, 700)) if "static" in sections else ()):
        args = ["--target", base + "/", "--autorun", "--no-browser", "--width", str(width), "--height", str(height)]
        report, state = capture(f"static-{width}", args)
        listed = report["api_endpoints"]
        assert len(listed) == 7 and state["api_endpoint_count"] == 7, [e["url"] for e in listed]
        assert "PRIVATE" not in json.dumps(listed) and "PRIVATE" not in json.dumps(state["api_rows"])
        first = state["api_rows"]
        assert len(first) == 5 and first[0]["tag"] == "Declared" and first[1]["tag"] == "In code", first
        assert first[0]["shown"].strip() == "/wp-json/" and first[0]["title"] == base + "/wp-json/", first[0]
        assert not any(e["basis"] == "Observed request" for e in listed), "no browser, nothing observed"
        lead, shown = reach(f"static-scrolled-{width}", args, state, height)
        loaded = lead + click(control(shown, "api:more"))
        _, expanded = capture(f"static-more-{width}", args, loaded)
        long_row = next(r for r in expanded["api_rows"] if r["title"] == LONG)
        assert long_row["shown"].endswith("...") and long_row["tag"] == "In code" and on_screen(long_row, height), long_row
        _, detail = capture(f"static-long-detail-{width}", args, loaded + click(long_row, 1600, double=True))
        assert detail["api_detail"]["title"] == LONG and detail["api_detail"]["detail"] == "Host not stated / Code reference"
        assert any("fetch() argument" in e["value"] and "(query omitted)" in e["value"] for e in detail["api_detail"]["evidence"])
        print(f"Static collection API endpoint list passed at {width}x{height}.", flush=True)

    if "static" in sections:
        report, state = capture("static-empty", ["--target", base + "/empty", "--autorun", "--no-browser"])
        assert report["api_endpoints"] == [] and state["api_section_shown"] and not state["api_rows"] and not state["api_controls"]
        report, state = capture("no-web", ["--target", "127.0.0.1:9", "--autorun", "--no-browser"])
        assert report["api_endpoints"] == [] and not state["api_section_shown"], "no web collection, no API section"
        print("Empty states passed.", flush=True)

    if "browser" in sections:
        report, state = capture("browser-1280", ["--target", base + "/", "--autorun"], timeout=90)
        if report["browser"]["state"] == "unavailable":
            print("Browser fixture unavailable:", report["browser"]["reason"])
            sys.exit(77)
        listed = report["api_endpoints"]
        observed = [e for e in listed if e["basis"] == "Observed request"]
        assert {e["url"] for e in observed} == {base + p for p in PATHS}, [e["url"] for e in listed]
        assert all(e["method"] == "GET" and e["status"] == 200 and e["scope"] == "Same host" for e in observed)
        rows = state["api_rows"]
        assert len(rows) == 5 and all(r["tag"] == "Observed" for r in rows), rows
        assert rows[0]["shown"].startswith("GET    /api/v1/"), rows[0]
        assert "PRIVATE" not in json.dumps(listed)
        after_scan = [{"at": 28000, "kind": "focus"}] + click(rows[0], 29000, double=True)
        _, detail = capture("browser-detail", ["--target", base + "/", "--autorun"], after_scan, timeout=150)
        proof = detail["api_detail"]
        assert proof["detail"] == "Same host / Observed request / HTTP 200", proof
        assert any(e["source"].startswith("Browser request / ") and " / Fetch / " in e["value"] for e in proof["evidence"]), proof
        assert any("fetch() argument" in e["value"] for e in proof["evidence"]), "the code that names the URL joins the observed row"
        assert "did not call" in proof["evidence"][-1]["value"]
        print("Browser-observed API endpoint list passed.", flush=True)
finally:
    server.shutdown()
