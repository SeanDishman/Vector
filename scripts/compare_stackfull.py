import argparse
from collections import Counter
from concurrent.futures import ThreadPoolExecutor
from datetime import datetime, timezone
import hashlib
import ipaddress
import json
from pathlib import Path
import random
import re
import socket
import subprocess
import time
from urllib.error import HTTPError
from urllib.request import Request, build_opener, ProxyHandler


API = "https://www.stackfull.lol/api/scan"
POOL = ["ghost.org", "grafana.com", "plausible.io", "cal.com", "posthog.com",
        "docusaurus.io", "astro.build", "wordpress.org", "netlify.com", "sentry.io",
        "umami.is", "excalidraw.com", "mermaid.js.org", "obsidian.md", "strapi.io",
        "eleventy.dev", "nuxt.com", "svelte.dev", "joplinapp.org", "gohugo.io"]


def stamp():
    return datetime.now(timezone.utc).isoformat()


def save(path, value):
    path.write_text(json.dumps(value, indent=2, ensure_ascii=False), encoding="utf-8")


def public_domain(domain):
    if not re.fullmatch(r"[a-z0-9]+(?:[a-z0-9.-]*[a-z0-9])?", domain) or "." not in domain:
        raise ValueError("Expected a plain public hostname")
    addresses = sorted({item[4][0] for item in socket.getaddrinfo(domain, 443, type=socket.SOCK_STREAM)})
    if not addresses or not all(ipaddress.ip_address(a).is_global for a in addresses):
        raise ValueError("Benchmark target must resolve exclusively to public addresses")
    return addresses


def stackfull(domain, folder):
    meta = {"started_at": stamp(), "endpoint": API, "method": "POST", "domain": domain}
    begin = time.monotonic()
    request = Request(API, data=json.dumps({"domain": domain}).encode(),
                      headers={"Content-Type": "application/json", "Accept": "application/json",
                               "User-Agent": "Vector-Comparison/1.0"}, method="POST")
    try:
        with build_opener(ProxyHandler({})).open(request, timeout=75) as response:
            body = response.read(2 * 1024 * 1024 + 1)
            meta["http_status"] = response.status
        if len(body) > 2 * 1024 * 1024:
            raise ValueError("StackFull response exceeded 2 MiB")
        data = json.loads(body)
        if not isinstance(data.get("items"), list):
            raise ValueError("StackFull response has no items array")
        (folder / "stackfull.json").write_bytes(body)
        meta.update(sha256=hashlib.sha256(body).hexdigest(), bytes=len(body), state="complete")
    except HTTPError as error:
        meta.update(http_status=error.code, state="error", error=str(error))
    except Exception as error:
        meta.update(state="error", error=str(error))
    meta.update(finished_at=stamp(), elapsed_seconds=round(time.monotonic() - begin, 3))
    save(folder / "stackfull.meta.json", meta)
    return meta


def vector(exe, domain, folder):
    command = [str(exe), "--target", "https://" + domain + "/", "--web-only"]
    meta = {"started_at": stamp(), "command": command}
    begin = time.monotonic()
    process = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                               creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0))
    try:
        stdout, stderr = process.communicate(timeout=95)
        report = json.loads(stdout)
        (folder / "vector.json").write_bytes(stdout)
        (folder / "vector.stderr.txt").write_bytes(stderr)
        meta.update(state="complete", exit_code=process.returncode,
                    report_status=report.get("status"), sha256=hashlib.sha256(stdout).hexdigest())
    except subprocess.TimeoutExpired:
        process.terminate()
        stdout, stderr = process.communicate(timeout=10)
        (folder / "vector.stderr.txt").write_bytes(stderr)
        meta.update(state="timeout", error="95-second process deadline exceeded")
    except Exception as error:
        meta.update(state="error", error=str(error))
    meta.update(finished_at=stamp(), elapsed_seconds=round(time.monotonic() - begin, 3))
    save(folder / "vector.meta.json", meta)
    return meta


def normalized(name):
    key = re.sub(r"[^a-z0-9]", "", name.casefold())
    return {"nuxtjs": "nuxt", "vuejs": "vue", "plausibleanalytics": "plausible"}.get(key, key)


def observed_names(report):
    findings = [f for f in report.get("findings", [])
                if f.get("category") not in ("Fingerprint", "Service", "Package")
                and "hint" not in f.get("category", "").casefold()]
    vendors = [v for v in report.get("vendors", [])
               if v.get("name") not in (None, "Unknown", "Unattributed origin")
               and not v.get("published_by")
               and (v.get("evidence_level") in ("Observed response", "Response header")
                    or (v.get("evidence_level") == "Page reference" and v.get("relationship") in
                        ("Script", "Embedded frame", "Stylesheet", "Image / media", "Form destination"))
                    or (v.get("evidence_level") == "Form input" and v.get("relationship") == "Supported source"))]
    return {normalized(i["name"]): i["name"] for i in findings + vendors}


def analyze(root):
    manifest = json.loads((root / "manifest.json").read_text(encoding="utf-8"))
    api_root = Path(manifest.get("api_capture_directory", root))
    results = []
    for domain in manifest["selected"]:
        folder = root / domain
        sfpath, vpath = api_root / domain / "stackfull.json", folder / "vector.json"
        if not sfpath.exists() or not vpath.exists():
            results.append({"domain": domain, "state": "incomplete"})
            continue
        sf, vr = [json.loads(p.read_text(encoding="utf-8")) for p in (sfpath, vpath)]
        detected = [i for i in sf["items"] if i.get("source") == "detected"]
        published = [i for i in sf["items"] if i.get("source") == "subprocessor"]
        vendors = vr.get("vendors", [])
        vector_names = observed_names(vr)
        sf_names = {normalized(i["name"]): i["name"] for i in detected}
        provider_names = {normalized(i["name"]): i["name"] for i in vendors if i.get("published_by")}
        results.append({"domain": domain, "state": "compared",
            "stackfull_total": len(sf["items"]),
            "stackfull_source_counts": dict(Counter(i.get("source", "unknown") for i in sf["items"])),
            "stackfull_detected": [i["name"] for i in detected],
            "vector_named_evidence": sorted(set(vector_names.values())),
            "vector_published": sorted(provider_names.values()),
            "matched_detected_names": [i["name"] for i in detected if normalized(i["name"]) in vector_names],
            "stackfull_only_detected_claims": [i for i in detected if normalized(i["name"]) not in vector_names],
            "vector_only_named_evidence": [name for key, name in vector_names.items() if key not in sf_names],
            "stackfull_only_published_claims": [i for i in published if normalized(i["name"]) not in provider_names],
            "sources": sf.get("sources", []),
            "vector_collection": {"status": vr.get("status"), "web": vr.get("web_coverage"),
                "browser_state": vr.get("browser", {}).get("state"),
                "pages": vr.get("browser", {}).get("pages", []),
                "disclosures": vr.get("disclosures", [])}})
    result = {"compared_at": stamp(),
              "name_aliases": {"Nuxt.js": "Nuxt", "Vue.js": "Vue", "Plausible Analytics": "Plausible"},
              "interpretation": "Name differences are candidates for investigation, not accuracy scores. "
                "StackFull claims can span other pages, company relationships and cached records; "
                "Vector includes DNS roles, protocol evidence and explicit collection limits.",
              "results": results}
    save(root / "comparison.json", result)
    for r in results:
        print(r["domain"], r.get("stackfull_source_counts", r["state"]),
              "Vector named:", len(r.get("vector_named_evidence", [])),
              "missing detected claims:", [i["name"] for i in r.get("stackfull_only_detected_claims", [])], flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("action", choices=("collect", "analyze", "rerun"))
    parser.add_argument("directory", type=Path)
    parser.add_argument("--exe", type=Path)
    parser.add_argument("--count", type=int, default=6)
    args = parser.parse_args()
    root = args.directory.resolve()
    if args.action == "analyze":
        analyze(root)
        return
    if not args.exe or not 1 <= args.count <= 6:
        parser.error("collect requires --exe and a count from 1 to 6")
    if args.action == "rerun":
        original = json.loads((root / "manifest.json").read_text(encoding="utf-8"))
        updated = root / "updated"
        if updated.exists():
            parser.error("Updated comparison already exists; inspect it before authorizing another run")
        updated.mkdir()
        exe = args.exe.resolve(strict=True)
        save(updated / "manifest.json", {"started_at": stamp(), "selected": original["selected"],
            "api_capture_directory": str(root), "vector_executable": str(exe),
            "vector_sha256": hashlib.sha256(exe.read_bytes()).hexdigest(),
            "scope": "One updated web-only inspection per original site; reuses saved StackFull responses."})
        for domain in original["selected"]:
            folder = updated / domain
            folder.mkdir()
            try:
                save(folder / "target.json", {"domain": domain, "addresses": public_domain(domain), "checked_at": stamp()})
            except Exception as error:
                save(folder / "target.json", {"state": "skipped", "error": str(error)})
                continue
            print("Updated inspection:", domain, flush=True)
            result = vector(exe, domain, folder)
            print(domain, result["state"], flush=True)
        analyze(updated)
        return
    root.mkdir(parents=True, exist_ok=True)
    if (root / "manifest.json").exists():
        parser.error("Choose a new output directory; collection never silently repeats a benchmark")
    exe = args.exe.resolve(strict=True)
    seed = random.SystemRandom().getrandbits(64)
    selected = random.Random(seed).sample(POOL, args.count)
    save(root / "manifest.json", {"started_at": stamp(), "selection": "random sample without replacement",
        "seed": seed, "pool": POOL, "selected": selected, "vector_executable": str(exe),
        "vector_sha256": hashlib.sha256(exe.read_bytes()).hexdigest(),
        "api": API, "authorization": "User explicitly requested public random-site comparison in this conversation",
        "scope": "One bounded web-only Vector inspection and one normal StackFull lookup per selected domain; "
                 "no authentication, exploit checks, LAN targets, automatic retry or production API integration."})
    print("Selected:", ", ".join(selected), flush=True)
    with ThreadPoolExecutor(max_workers=2) as pool:
        for domain in selected:
            folder = root / domain
            folder.mkdir(exist_ok=True)
            try:
                save(folder / "target.json", {"domain": domain, "addresses": public_domain(domain), "checked_at": stamp()})
            except Exception as error:
                save(folder / "target.json", {"domain": domain, "state": "skipped", "error": str(error)})
                continue
            print("Comparing", domain, flush=True)
            f_sf = pool.submit(stackfull, domain, folder)
            f_v = pool.submit(vector, exe, domain, folder)
            sf, vr = f_sf.result(), f_v.result()
            print(domain, "StackFull:", sf["state"], "Vector:", vr["state"], flush=True)
            if sf.get("http_status") == 429:
                print("Stopping further collection after API rate limit.", flush=True)
                break
    analyze(root)


if __name__ == "__main__":
    main()
