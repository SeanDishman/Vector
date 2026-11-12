import base64
import datetime
import http.server
import ipaddress
import json
import os
from pathlib import Path
import select
import socket
import socketserver
import ssl
import subprocess
import sys
import tempfile
import threading
import time
import uuid
import psutil
from urllib.parse import quote

USER, PASSWORD = 'fixture-user', 'proxy:p@ssword'
REFUSAL = b'HTTP/1.1 400 Bad Request\r\nContent-Length: 0\r\nConnection: close\r\n\r\n'


class Web(http.server.BaseHTTPRequestHandler):
    def log_message(self, *_): pass

    def do_GET(self):
        self.server.hits.append((self.path, dict(self.headers)))
        status, mime = 200, 'text/html'
        if self.path == '/entry':
            body = (f'<title>Proxy fixture</title><meta name=generator content="WordPress 6.8">'
                    '<link rel=stylesheet href="/style.css"><script src="/static.js"></script>'
                    '<a href="/subprocessors">Subprocessors</a>'
                    f'<script>const s=document.createElement("script");s.src="http://dependency.invalid:80/runtime.js";document.head.appendChild(s);</script>').encode()
        elif self.path == '/static.js':
            mime, body = 'application/javascript', b'/*! jQuery v3.7.1 */'
        elif self.path == '/runtime.js':
            mime, body = 'application/javascript', b'/*! lodash 4.17.21 */; window.proxyLoaded=true;'
        elif self.path == '/style.css':
            mime, body = 'text/css', b'/*! tailwindcss v4.3.3 | MIT License | https://tailwindcss.com */ body{display:block}'
        elif self.path == '/subprocessors':
            body = f'<h1>Subprocessors</h1><table><tr><th>Name</th><th>Purpose</th></tr><tr><td>{self.server.provider}</td><td>Cloud storage</td></tr></table>'.encode()
        else:
            status, body = 404, b''
        self.send_response(status)
        self.send_header('Content-Type', mime)
        self.send_header('Content-Length', str(len(body)))
        self.end_headers()
        try: self.wfile.write(body)
        except (OSError, ssl.SSLError): pass


class ThreadServer(socketserver.ThreadingTCPServer):
    daemon_threads = True
    allow_reuse_address = True

    def handle_error(self, *_): pass


def exact(connection, count):
    data = b''
    while len(data) < count:
        part = connection.recv(count - len(data))
        if not part: raise ConnectionError('EOF')
        data += part
    return data


class Service(socketserver.BaseRequestHandler):
    def handle(self):
        self.request.settimeout(2)
        if self.server.kind == 'ssh': self.request.sendall(b'SSH-2.0-OpenSSH_9.8\r\n')
        try:
            data = self.request.recv(4096)
            self.server.hits.append(data)
            if self.server.kind == 'redis' and data == b'PING\r\n': self.request.sendall(b'+PONG\r\n')
        except OSError: pass


class ProxyHandler(socketserver.BaseRequestHandler):
    def handle(self):
        connection = self.request
        connection.settimeout(3)
        try:
            if self.server.tls:
                if connection.recv(1, socket.MSG_PEEK) != b'\x16':
                    self.server.plain.append(connection.recv(4096))
                    if self.server.banner: connection.sendall(REFUSAL)
                    return
                connection = self.server.tls.wrap_socket(connection, server_side=True)
            if self.server.stall:
                time.sleep(2)
                return
            if self.server.kind == 'socks5':
                version, count = exact(connection, 2)
                if version != 5:
                    if self.server.banner: connection.sendall(b'HTTP/1.0 501 Tor is not an HTTP Proxy\r\nContent-Type: text/html\r\n\r\n')
                    return
                methods = exact(connection, count)
                method = 2 if self.server.auth else 0
                if method not in methods: connection.sendall(b'\x05\xff'); return
                if self.server.delay: time.sleep(self.server.delay)
                connection.sendall(bytes([5, method]))
                if method == 2:
                    version, length = exact(connection, 2)
                    username = exact(connection, length).decode()
                    length = exact(connection, 1)[0]
                    password = exact(connection, length).decode()
                    valid = (username, password) == (USER, PASSWORD)
                    connection.sendall(bytes([1, 0 if valid else 1]))
                    if not valid: self.server.rejected += 1; return
                version, command, reserved, kind = exact(connection, 4)
                if command != 1: return
                if kind == 3: host = exact(connection, exact(connection, 1)[0]).decode()
                elif kind == 1: host = socket.inet_ntop(socket.AF_INET, exact(connection, 4))
                elif kind == 4: host = socket.inet_ntop(socket.AF_INET6, exact(connection, 16))
                else: return
                port = int.from_bytes(exact(connection, 2), 'big')
                tail = b''
            else:
                data = b''
                while b'\r\n\r\n' not in data and len(data) <= 65536:
                    part = connection.recv(4096)
                    if not part: return
                    data += part
                head, tail = data.split(b'\r\n\r\n', 1)
                lines = head.decode().split('\r\n')
                method, target, version = lines[0].split()
                if method != 'CONNECT':
                    self.server.other.append(method)
                    if self.server.banner: connection.sendall(REFUSAL)
                    return
                if self.server.delay: time.sleep(self.server.delay)
                host, port = target.rsplit(':', 1); host = host.strip('[]'); port = int(port)
                headers = dict((k.lower(), v.strip()) for k, v in (line.split(':', 1) for line in lines[1:]))
                expected = 'Basic ' + base64.b64encode(f'{USER}:{PASSWORD}'.encode()).decode()
                if self.server.auth and headers.get('proxy-authorization') != expected:
                    self.server.rejected += 1
                    connection.sendall(b'HTTP/1.1 407 Proxy Authentication Required\r\nProxy-Authenticate: Basic realm="fixture"\r\nContent-Length: 0\r\nConnection: close\r\n\r\n')
                    return
            self.server.hits.append((host, port))
            destination = self.server.routes.get(port)
            if not destination or self.server.deny:
                connection.sendall(b'\x05\x02\x00\x01'+b'\0'*6 if self.server.kind == 'socks5' else b'HTTP/1.1 403 Forbidden\r\nContent-Length: 0\r\n\r\n')
                return
            with socket.create_connection(('127.0.0.1', destination), timeout=3) as remote:
                connection.sendall(b'\x05\x00\x00\x01'+b'\0'*6 if self.server.kind == 'socks5' else b'HTTP/1.1 200 Connected\r\nServer: ProxyOnlyMustNotMatch\r\n\r\n')
                if tail: remote.sendall(tail)
                deadline = time.monotonic()+40
                while time.monotonic() < deadline:
                    readable, _, _ = select.select([connection, remote], [], [], .1)
                    for source in readable:
                        data = source.recv(65536)
                        if not data: return
                        (remote if source is connection else connection).sendall(data)
        except (OSError, ValueError, ConnectionError, ssl.SSLError): pass
        finally:
            if connection is not self.request: connection.close()


class Fixture:
    def __init__(self):
        self.servers = []
        self.web = http.server.ThreadingHTTPServer(('127.0.0.1', 0), Web)
        self.web.hits = []; self.web.provider = 'Generated ' + uuid.uuid4().hex[:12]
        self.start(self.web)
        self.routes = {80: self.web.server_port, self.web.server_port: self.web.server_port}
        self.services = {}
        for port, kind in [(22, 'ssh'), (6379, 'redis'), (9100, 'printer')]:
            service = ThreadServer(('127.0.0.1', 0), Service)
            service.kind, service.hits = kind, []
            self.start(service); self.routes[port] = service.server_address[1]; self.services[port] = service

    def start(self, server):
        self.servers.append(server)
        threading.Thread(target=server.serve_forever, daemon=True).start()
        return server

    def proxy(self, kind='http', auth=True, deny=False, stall=False, tls=None, banner=False, delay=0):
        server = ThreadServer(('127.0.0.1', 0), ProxyHandler)
        server.kind, server.auth, server.deny, server.stall, server.tls = kind, auth, deny, stall, tls
        server.banner, server.delay, server.plain, server.other = banner, delay, [], []
        server.routes, server.hits, server.rejected = self.routes, [], 0
        return self.start(server)

    def close(self):
        for server in self.servers: server.shutdown(); server.server_close()

    def tls_context(self, directory):
        from cryptography import x509
        from cryptography.hazmat.primitives import hashes, serialization
        from cryptography.hazmat.primitives.asymmetric import rsa
        from cryptography.x509.oid import NameOID
        key = rsa.generate_private_key(public_exponent=65537, key_size=2048)
        name = x509.Name([x509.NameAttribute(NameOID.COMMON_NAME, 'Vector local proxy fixture')])
        now = datetime.datetime.now(datetime.timezone.utc)
        cert = (x509.CertificateBuilder().subject_name(name).issuer_name(name).public_key(key.public_key())
                .serial_number(x509.random_serial_number()).not_valid_before(now-datetime.timedelta(days=1))
                .not_valid_after(now+datetime.timedelta(days=1))
                .add_extension(x509.BasicConstraints(ca=True, path_length=None), True)
                .add_extension(x509.SubjectAlternativeName([x509.IPAddress(ipaddress.ip_address('127.0.0.1'))]), False)
                .sign(key, hashes.SHA256()))
        key_path, cert_path = directory/'key.pem', directory/'cert.pem'
        key_path.write_bytes(key.private_bytes(serialization.Encoding.PEM, serialization.PrivateFormat.PKCS8, serialization.NoEncryption()))
        cert_path.write_bytes(cert.public_bytes(serialization.Encoding.PEM))
        context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
        context.load_cert_chain(cert_path, key_path)
        return context


def main():
    exe, helper, out = Path(sys.argv[1]).resolve(), Path(sys.argv[2]).resolve(), Path(sys.argv[3]).resolve()
    out.mkdir(parents=True, exist_ok=True)
    fixture = Fixture(); results = []
    def run(name, proxy, extra=(), target='http://unresolvable-proxy-fixture.invalid:80/entry', password=PASSWORD, browser=False, auto=False):
        scheme = 'https' if proxy.tls else proxy.kind
        address = f'127.0.0.1:{proxy.server_address[1]}'
        command = [str(exe), '--target', target, '--proxy', address if auto else f'{scheme}://{address}',
                   '--timeout', '700', '--http-timeout', '1500', '--browser-timeout', '2500']
        if proxy.auth: command += ['--proxy-user', USER, '--proxy-password', password]
        if not browser: command += ['--no-browser']
        env = dict(os.environ, NO_PROXY='*', no_proxy='*')
        observed_arguments = []
        begin = time.monotonic(); process = subprocess.Popen(command+list(extra), stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, env=env)
        watching = threading.Event()
        def watch():
            while not watching.wait(.1):
                try:
                    for child in psutil.Process(process.pid).children(recursive=True):
                        arguments = child.cmdline()
                        if '--remote-debugging-pipe' in arguments and not any(arg.startswith('--type=') for arg in arguments): observed_arguments.append(arguments)
                except (psutil.Error, OSError): pass
        watcher = threading.Thread(target=watch, daemon=True); watcher.start()
        try: stdout, stderr = process.communicate(timeout=35)
        finally:
            watching.set();watcher.join(2)
            if process.poll() is None: process.kill();process.wait()
        elapsed = time.monotonic()-begin
        assert process.returncode == 0, (name, stderr, stdout[:200])
        report = json.loads(stdout); (out/(name+'.json')).write_text(stdout)
        assert PASSWORD not in stdout and USER not in stdout and PASSWORD not in stderr
        if browser and report['browser']['pages']:
            assert observed_arguments, 'Browser was not observed'
            for args in observed_arguments:
                assert PASSWORD not in ' '.join(args) and USER not in ' '.join(args)
                assert any(arg.startswith('--proxy-server=http://127.0.0.1:') for arg in args)
                assert '--no-proxy-server' not in args and '--proxy-bypass-list=<-loopback>' in args
        assert report['proxy']['enabled'] and report['reachability']['icmp']['state'] == 'skipped'
        assert report['proxy']['endpoint'] == f'{scheme}://{address}', report['proxy']
        detected = [note for note in report['notes'] if note.startswith('Proxy type detected automatically. ')]
        assert len(detected) == (1 if auto else 0), report['notes']
        assert set(proxy.other) <= ({'GET'} if auto else set()), proxy.other
        assert all(not row['address'] for row in report['web'])
        assert all(not row['address'] for row in report['browser']['requests'])
        results.append({'name': name, 'seconds': round(elapsed, 3), 'proxy_connections': len(proxy.hits)})
        return report
    try:
        for kind in ['http', 'socks5']:
            proxy = fixture.proxy(kind)
            r = run(kind+'-static', proxy)
            names = {f['name'] for f in r['findings']}
            assert {'WordPress', 'jQuery', 'Tailwind CSS'} <= names, names
            assert any(v['name'] == fixture.web.provider for v in r['vendors']), r['vendors']
            assert r['address'] == '' and any(host.endswith('.invalid') for host, _ in proxy.hits)
            r = run(kind+'-browser', proxy, browser=True)
            assert any(q['url'].endswith('/runtime.js') and q['status'] == 200 for q in r['browser']['requests']), r['browser']
            assert ('dependency.invalid', 80) in proxy.hits
        proxy = fixture.proxy(auth=False)
        run('http-no-auth', proxy)
        for kind, auth in [('http', True), ('http', False), ('socks5', True), ('socks5', False)]:
            proxy = fixture.proxy(kind, auth=auth)
            r = run(f'auto-{kind}-{"authenticated" if auth else "open"}', proxy, auto=True)
            assert {'WordPress', 'jQuery', 'Tailwind CSS'} <= {f['name'] for f in r['findings']}, r['findings']
            assert r['address'] == '' and any(host.endswith('.invalid') for host, _ in proxy.hits)
        proxy = fixture.proxy('socks5')
        r = run('auto-socks5-browser', proxy, auto=True, browser=True)
        assert any(q['url'].endswith('/runtime.js') and q['status'] == 200 for q in r['browser']['requests']), r['browser']
        assert ('dependency.invalid', 80) in proxy.hits

        def detect(name, port, expected, least, most, error=''):
            p = subprocess.run([str(helper), '--detect', f'127.0.0.1:{port}', 'detect-fixture.invalid', '80'], capture_output=True, text=True, timeout=20)
            found = json.loads(p.stdout)
            assert found['scheme'] == expected and least <= found['milliseconds'] < most, (name, found, p.stderr)
            assert p.returncode == (0 if expected else 3) and error in found.get('error', ''), (name, found)
            results.append({'name': name, 'detected': expected or 'none', 'milliseconds': round(found['milliseconds']), 'detail': found.get('detail') or found.get('error')})

        def closed_port():
            with socket.socket() as probe:
                probe.bind(('127.0.0.1', 0))
                return probe.getsockname()[1]

        detect('detect-http-authenticated', fixture.proxy().server_address[1], 'http', 0, 5000)
        detect('detect-http-open', fixture.proxy(auth=False).server_address[1], 'http', 0, 5000)
        detect('detect-http-slow-tunnel', fixture.proxy(auth=False, delay=2.5).server_address[1], 'http', 2400, 8000)
        detect('detect-http-refuses-other-requests', fixture.proxy(auth=False, banner=True, delay=60).server_address[1], 'http', 1400, 6000)
        detect('detect-socks5-authenticated', fixture.proxy('socks5').server_address[1], 'socks5', 0, 5000)
        detect('detect-socks5-open', fixture.proxy('socks5', auth=False).server_address[1], 'socks5', 0, 5000)
        detect('detect-socks5-answering-web-requests', fixture.proxy('socks5', auth=False, banner=True, delay=.2).server_address[1], 'socks5', 150, 5000)
        detect('detect-silent-server', fixture.proxy(auth=False, stall=True).server_address[1], '', 1500, 8000, 'did not answer as an HTTP, HTTPS or SOCKS5 proxy')
        detect('detect-other-service', fixture.services[6379].server_address[1], '', 0, 8000, 'did not answer as an HTTP, HTTPS or SOCKS5 proxy')
        detect('detect-closed-port', closed_port(), '', 0, 8000, 'Could not connect to the proxy')

        def refused(name, port, expected):
            before = len(fixture.web.hits)
            command = [str(exe), '--target', f'http://127.0.0.1:{fixture.web.server_port}/entry', '--proxy', f'127.0.0.1:{port}',
                       '--proxy-user', USER, '--proxy-password', PASSWORD, '--no-browser', '--timeout', '700', '--http-timeout', '1500']
            p = subprocess.run(command, capture_output=True, text=True, timeout=30, env=dict(os.environ, NO_PROXY='*', no_proxy='*'))
            report = json.loads(p.stdout); (out/(name+'.json')).write_text(p.stdout)
            assert p.returncode == 1 and report['status'] == 'error' and expected in report['error'], (name, p.returncode, report['error'])
            assert len(fixture.web.hits) == before, 'A failed proxy check fell back to a direct connection'
            assert not report['findings'] and not report['web'] and not report['services'] and not report['address']
            assert PASSWORD not in p.stdout and USER not in p.stdout and PASSWORD not in p.stderr
            results.append({'name': name, 'error': report['error']})

        refused('auto-closed-port', closed_port(), 'Could not connect to the proxy at 127.0.0.1:')
        refused('auto-other-service', fixture.services[6379].server_address[1], 'did not answer as an HTTP, HTTPS or SOCKS5 proxy')
        proxy = fixture.proxy()
        r = run('tcp-services', proxy, ['--ports', '22,6379,9100'], target='fixture.invalid')
        services = {s['port']: s for s in r['services']}
        assert services[22]['identified_service'] == 'SSH'
        assert services[6379]['identified_service'] == 'Redis'
        assert not services[9100]['active_probe'] and not any(fixture.services[9100].hits)
        assert not any(s['transport'] == 'udp' for s in r['services'])
        before = len(fixture.web.hits); proxy = fixture.proxy()
        r = run('wrong-password', proxy, target=f'http://127.0.0.1:{fixture.web.server_port}/entry', password='wrong', browser=True)
        assert len(fixture.web.hits) == before and proxy.rejected and not r['findings']
        proxy = fixture.proxy(deny=True)
        r = run('denied-tunnel', proxy, target=f'http://127.0.0.1:{fixture.web.server_port}/entry')
        assert len(fixture.web.hits) == before and all(s['state'] != 'closed' for s in r['services'])
        proxy = fixture.proxy(auth=False, deny=True)
        endpoint = f'http://127.0.0.1:{proxy.server_address[1]}'
        p = subprocess.run([str(helper), '--metadata', endpoint], capture_output=True, text=True, timeout=10)
        assert p.returncode == 0, p.stderr
        assert {'dns.google', 'rdap.arin.net', 'ipwho.is'} <= {host for host, _ in proxy.hits}, proxy.hits
        results.append({'name': 'metadata-routing', 'proxy_connections': len(proxy.hits)})
        proxy = fixture.proxy(auth=False, stall=True)
        p = subprocess.run([str(helper), '--cancel', f'http://127.0.0.1:{proxy.server_address[1]}'], capture_output=True, text=True, timeout=5)
        assert p.returncode == 0, p.stderr
        results.append(json.loads(p.stdout))
        proxy = fixture.proxy(auth=False, stall=True)
        p = subprocess.run([str(helper), '--cancel', f'127.0.0.1:{proxy.server_address[1]}'], capture_output=True, text=True, timeout=5)
        assert p.returncode == 0, p.stderr
        results.append(dict(json.loads(p.stdout), name='cancelled-type-detection'))
        proxy = fixture.proxy(auth=False)
        p = subprocess.run([str(helper), '--cancel-browser', f'http://127.0.0.1:{proxy.server_address[1]}'], capture_output=True, text=True, timeout=12)
        assert p.returncode == 0, p.stderr
        results.append(json.loads(p.stdout))
        with tempfile.TemporaryDirectory(prefix='vector-proxy-tls-') as temporary:
            context = fixture.tls_context(Path(temporary))
            tls_web = http.server.ThreadingHTTPServer(('127.0.0.1', 0), Web)
            tls_web.socket = context.wrap_socket(tls_web.socket, server_side=True)
            tls_web.hits = []; tls_web.provider = fixture.web.provider
            fixture.start(tls_web); fixture.routes[443] = tls_web.server_port
            proxy = fixture.proxy()
            r = run('target-tls-rejected', proxy, target='https://fixture.invalid:443/entry')
            assert not r['findings'] and not tls_web.hits
            r = run('target-tls-opt-in', proxy, ['--allow-untrusted-tls'], target='https://fixture.invalid:443/entry', browser=True)
            assert any(f['name'] == 'WordPress' for f in r['findings'])
            assert all(not w['tls_verified'] for w in r['web'])
            proxy = fixture.proxy(tls=context)
            r = run('untrusted-https-proxy', proxy, ['--allow-untrusted-tls'])
            assert not proxy.hits and not r['findings'], 'Target TLS opt-out must not disable proxy TLS verification'
            proxy = fixture.proxy(tls=context)
            r = run('auto-untrusted-https-proxy', proxy, ['--allow-untrusted-tls'], auto=True)
            assert not proxy.hits and not r['findings'], 'A detected HTTPS proxy must still have its certificate verified'
            secrets = [USER.encode(), PASSWORD.encode(), base64.b64encode(f'{USER}:{PASSWORD}'.encode())]
            assert proxy.plain and not any(secret in data for secret in secrets for data in proxy.plain), 'Proxy credentials were sent before TLS'
            detect('detect-https', fixture.proxy(tls=context).server_address[1], 'https', 0, 5000)
            detect('detect-https-answering-plain-text', fixture.proxy(tls=context, banner=True).server_address[1], 'https', 0, 5000)
            proxy = fixture.proxy(tls=context)
            endpoint = f'https://{quote(USER, safe="")}:{quote(PASSWORD, safe="")}@127.0.0.1:{proxy.server_address[1]}'
            p = subprocess.run([str(helper), '--trusted-proxy', endpoint, str(Path(temporary)/'cert.pem')], capture_output=True, text=True, timeout=10)
            assert p.returncode == 0, p.stderr
            results.append({'name': 'trusted-https-proxy', 'trust': 'Fixture CA supplied to test handle only; system trust unchanged'})
        for _, headers in fixture.web.hits:
            assert 'Proxy-Authorization' not in headers, headers
        (out/'verification.json').write_text(json.dumps(results, indent=2))
        print(f'{len(results)} proxy scenarios passed: authenticated routing, automatic type detection, browser dependencies, metadata, TCP, cancellation and no direct fallback.')
    finally: fixture.close()


if __name__ == '__main__': main()
