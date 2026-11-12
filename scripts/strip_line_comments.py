from pathlib import Path

def strip_cpp_line_comments(text: str) -> str:
    out = []
    i = 0
    n = len(text)
    state = "code"
    raw_delim = ""
    while i < n:
        c = text[i]
        nxt = text[i + 1] if i + 1 < n else ""
        if state == "code":
            if c == "/" and nxt == "/":
                state = "line"
                i += 2
                continue
            if c == "/" and nxt == "*":
                out.append("/*")
                state = "block"
                i += 2
                continue
            if c == "'":
                out.append(c)
                state = "sq"
                i += 1
                continue
            if c == '"':
                k = i - 1
                while k >= 0 and text[k] in "uUL8":
                    k -= 1
                if k >= 0 and text[k] == "R":
                    m = i + 1
                    delim = ""
                    while m < n and text[m] != "(" and text[m] not in "\r\n":
                        delim += text[m]
                        m += 1
                    if m < n and text[m] == "(":
                        out.append(text[i : m + 1])
                        raw_delim = ")" + delim + '"'
                        state = "raw"
                        i = m + 1
                        continue
                out.append(c)
                state = "dq"
                i += 1
                continue
            out.append(c)
            i += 1
        elif state == "line":
            if c == "\n":
                out.append(c)
                state = "code"
            i += 1
        elif state == "block":
            out.append(c)
            if c == "*" and nxt == "/":
                out.append("/")
                i += 2
                state = "code"
            else:
                i += 1
        elif state == "sq":
            out.append(c)
            if c == "\\" and nxt:
                out.append(nxt)
                i += 2
                continue
            if c == "'":
                state = "code"
            i += 1
        elif state == "dq":
            out.append(c)
            if c == "\\" and nxt:
                out.append(nxt)
                i += 2
                continue
            if c == '"':
                state = "code"
            i += 1
        elif state == "raw":
            if text.startswith(raw_delim, i):
                out.append(raw_delim)
                i += len(raw_delim)
                state = "code"
            else:
                out.append(c)
                i += 1
    lines = "".join(out)
    cleaned = []
    for line in lines.splitlines(keepends=True):
        if line.endswith("\r\n"):
            body, end = line[:-2], "\r\n"
        elif line.endswith("\n"):
            body, end = line[:-1], "\n"
        else:
            body, end = line, ""
        if body.strip() == "":
            if body == "":
                cleaned.append(end)
        else:
            cleaned.append(body.rstrip() + end)
    return "".join(cleaned)

files = []
for r in (Path("tests"), Path("scripts")):
    if r.exists():
        files.extend(r.rglob("*.cpp"))
        files.extend(r.rglob("*.hpp"))
        files.extend(r.rglob("*.h"))
        files.extend(r.rglob("*.c"))
        files.extend(r.rglob("*.ps1"))
small_src = [
    "src/target.cpp", "src/report.cpp", "src/oslib.cpp", "src/discovery.cpp",
    "src/fingerprint.cpp", "src/snmp_router.cpp", "src/vendors.cpp",
    "src/technology_icons.cpp", "src/technology_icons.hpp", "src/reachability.cpp",
    "src/proxy_transport.cpp", "src/proxy_transport.hpp", "src/export.cpp",
    "src/endpoint.cpp", "src/endpoint_connections.cpp", "src/frontend.cpp",
    "src/progress_view.cpp", "src/progress_view.hpp", "src/markup.hpp",
    "src/ntlm.cpp", "src/web_signatures.cpp", "src/web_signatures.hpp",
    "src/upnp_igd.cpp", "src/embedded_libraries.hpp", "src/crawl_priority.hpp",
    "src/disclosures.cpp", "src/disclosure_rows.hpp", "src/dns.cpp", "src/dns.hpp",
    "src/asset_scope.hpp", "src/bundle_signatures.hpp",
]
for p in small_src:
    path = Path(p)
    if path.exists():
        files.append(path)

changed = 0
for path in sorted(set(files)):
    if "third_party" in path.parts or path.suffix == ".md":
        continue
    raw = path.read_text(encoding="utf-8", errors="surrogateescape")
    if "//" not in raw:
        continue
    new = strip_cpp_line_comments(raw)
    if new != raw:
        path.write_text(new, encoding="utf-8", newline="\n")
        changed += 1
        print(f"stripped {path}")
print(f"DONE files_changed={changed}")
