import http.server
import json
from pathlib import Path
import socket
import socketserver
import subprocess
import sys
import tempfile
import threading
import time
from tls_fixture import server_context

EXE = sys.argv[1]
class Web(http.server.BaseHTTPRequestHandler):
    server_version = "nginx/1.26.2"
    sys_version = ""
    def log_message(self, *_): pass
    def do_GET(self):
        if self.path == "/slow":
            time.sleep(2)
        if self.path == "/redirect":
            self.send_response(302)
            self.send_header("Location", "/stack")
            self.end_headers()
            return
        if self.path == "/external":
            self.send_response(302)
            self.send_header("Location", "http://example.invalid/")
            self.end_headers()
            return
        if self.path == "/loop":
            self.send_response(302)
            self.send_header("Location", "/loop")
            self.end_headers()
            return
        if self.path == "/printer-redirect":
            self.send_response(302)
            self.send_header("Location", "http://127.0.0.1:9100/")
            self.end_headers()
            return
        if self.path in ("/robots.txt", "/sitemap.xml", "/sitemap_index.xml"):
            self.send_response(404)
            self.send_header("Content-Length", "0")
            self.end_headers()
            return
        if self.path == "/crawl":
            body = b'<title>crawl root</title><a href="/deep">deep</a><a href="https://cdn.example/x.js">ext</a>'
        elif self.path == "/deep":
            body = (b'<title>deep page</title>'
                    b'<script src="/wp-content/plugins/woocommerce/assets/js/frontend/cart.js?ver=8.2.1"></script>')
        else:
            body = (b"x" * 1200000 if self.path == "/large" else
                    b'<title>Vector test appliance</title><meta name="generator" content="WordPress 6.6.2">'
                    b'<script src="/wp-includes/js/jquery/jquery-3.7.1.min.js"></script>')
        self.send_response(200)
        self.send_header("Content-Type", "text/html")
        self.send_header("Content-Length", str(len(body)))
        self.send_header("X-Powered-By", "PHP/8.3.12")
        self.send_header("Set-Cookie", "session=PRIVATE_FIXTURE_VALUE; HttpOnly")
        self.end_headers()
        try: self.wfile.write(body)
        except (BrokenPipeError, ConnectionResetError, ConnectionAbortedError): pass

class Banner(socketserver.BaseRequestHandler):
    def handle(self): self.request.sendall(b"SSH-2.0-OpenSSH_9.6p1 Ubuntu-3ubuntu13\r\n")

class SilentHttp(socketserver.BaseRequestHandler):
    def handle(self):
        data = b""
        self.request.settimeout(2)
        try:
            while b"\r\n\r\n" not in data and len(data) < 2048:
                chunk = self.request.recv(1024)
                if not chunk: break
                data += chunk
        except socket.timeout:
            return
        if not data.startswith(b"GET "): return
        body = b"<title>probed</title>"
        hdr = (b"HTTP/1.0 200 OK\r\nServer: nginx/1.26.2\r\nContent-Length: " +
               str(len(body)).encode() + b"\r\nConnection: close\r\n\r\n" + body)
        try: self.request.sendall(hdr)
        except OSError: pass

def run(target, *args, expected=0, timeout=40):
    p = subprocess.run([EXE, "--no-browser", "--target", target, "--timeout", "300", "--http-timeout", "2000", *args], capture_output=True, text=True, timeout=timeout)
    assert p.returncode == expected, (p.returncode, p.stderr)
    return json.loads(p.stdout)

web = http.server.ThreadingHTTPServer(("127.0.0.1", 0), Web)
ssh = socketserver.ThreadingTCPServer(("127.0.0.1", 0), Banner)
tls = http.server.ThreadingHTTPServer(("127.0.0.1", 0), Web)
context = server_context()
tls.socket = context.wrap_socket(tls.socket, server_side=True)
for server in (web, ssh, tls): threading.Thread(target=server.serve_forever, daemon=True).start()
base = f"http://127.0.0.1:{web.server_port}"
try:
    r = run(base + "/stack")
    assert r["services"][0]["state"] == "open"
    assert r["services"][0]["identified_service"] == "HTTP"
    assert r["web"][0]["title"] == "Vector test appliance"
    names = {f["name"] for f in r["findings"]}
    assert {"nginx", "PHP", "WordPress", "jQuery"} <= names, names
    assert all(f["evidence"] for f in r["findings"])
    assert "PRIVATE_FIXTURE_VALUE" not in json.dumps(r)
    r = run("127.0.0.1", "--ports", str(ssh.server_address[1]))
    assert r["services"][0]["identified_service"] == "SSH"
    assert any(f["name"] == "OpenSSH" and f["version"] == "9.6p1" for f in r["findings"])
    r = run(base + "/redirect")
    assert 302 in [h["status"] for h in r["web"]] and 200 in [h["status"] for h in r["web"]]
    r = run(base + "/external")
    assert any(h["status"] == 302 for h in r["web"]) and any("not followed" in n for n in r["notes"])
    r = run(base + "/loop")
    assert any("loop" in n for n in r["notes"])
    r = run(base + "/printer-redirect")
    assert any("non-web port" in n for n in r["notes"])
    r = run(base + "/large")
    assert any(h.get("body_truncated") and h.get("body_bytes") == 1024 * 1024 for h in r["web"])
    r = run(base + "/slow", "--http-timeout", "200")
    assert any(h["error"] and h["status"] == 0 for h in r["web"])
    r = run(base + "/crawl")
    names = {f["name"] for f in r["findings"]}
    assert "WooCommerce" in names and any(f["name"] == "WooCommerce" and f["version"] == "8.2.1" for f in r["findings"]), names
    assert any("/deep" in h.get("url", "") for h in r["web"])
    coverage = r["web_coverage"]
    assert coverage["started"], coverage
    assert not coverage["limited"] or (coverage["page_requests"] == coverage["page_limit"]
        and any("12-page" in reason for reason in coverage["reasons"])), coverage
    assert coverage["page_requests"] == len(r["web"]) <= coverage["page_limit"]
    secure = f"https://127.0.0.1:{tls.server_port}/stack"
    r = run(secure)
    assert not r["web"][0]["tls_verified"] and r["web"][0]["error"]
    assert r["findings"] == []
    r = run(secure, "--allow-untrusted-tls")
    assert r["web"][0]["status"] == 200 and not r["web"][0]["tls_verified"]
    assert r["web"][0]["certificate"] and any("disabled" in n for n in r["notes"])
    with tempfile.TemporaryDirectory() as temp:
        path = Path(temp) / "report.json"
        r = run(base + "/stack", "--output", str(path))
        assert json.loads(path.read_text())["findings"] == r["findings"]
    with socket.socket() as unused:
        unused.bind(("127.0.0.1", 0))
        port = unused.getsockname()[1]
    r = run(f"127.0.0.1:{port}", "--timeout", "4000")
    assert r["services"][0]["state"] == "closed" and r["findings"] == []
    r = run("this-host-does-not-exist.invalid", expected=1)
    assert r["status"] == "error" and "DNS" in r["error"]
    class IPv6Web(http.server.ThreadingHTTPServer):
        address_family = socket.AF_INET6
    ipv6 = IPv6Web(("::1", 0), Web)
    threading.Thread(target=ipv6.serve_forever, daemon=True).start()
    try:
        r = run(f"http://[::1]:{ipv6.server_port}/stack")
        assert r["address"] == "::1" and r["web"][0]["status"] == 200
        assert any(f["name"] == "WordPress" for f in r["findings"])
    finally:
        ipv6.shutdown(); ipv6.server_close()
    silent = socketserver.ThreadingTCPServer(("127.0.0.1", 0), SilentHttp)
    threading.Thread(target=silent.serve_forever, daemon=True).start()
    try:
        r = run("127.0.0.1", "--ports", str(silent.server_address[1]))
        svc = r["services"][0]
        assert svc["state"] == "open" and svc["active_probe"] is True and svc["identified_service"] == "HTTP"
        assert any(f["name"] == "nginx" and f["version"] == "1.26.2" for f in r["findings"])
        assert r["web"] and r["web"][0]["status"] == 200 and r["web"][0]["title"] == "probed"
    finally:
        silent.shutdown(); silent.server_close()
    print("16 loopback integration scenarios passed.")
finally:
    for server in (web, ssh, tls): server.shutdown(); server.server_close()
