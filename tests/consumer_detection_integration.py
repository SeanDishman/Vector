import http.server
import json
import socket
from pathlib import Path
import subprocess
import sys
import tempfile
import threading
import time

CORE = b'''var sharedName="__core-js_shared__",shared=globalThis[sharedName]||(globalThis[sharedName]={});
(shared.versions||(shared.versions=[])).push({version:"3.49.7",mode:"global",license:"https://github.com/zloirock/core-js/blob/v3.49.7/LICENSE",source:"https://github.com/zloirock/core-js"});'''
SEGMENT = b'''(self.webpackChunk_segment_analytics_next=self.webpackChunk_segment_analytics_next||[]).push([[5],{}]);var plugin={name:"Segment.io",type:"destination"};'''


class Fixture(http.server.BaseHTTPRequestHandler):
    protocol_version = 'HTTP/1.1'
    def setup(self):
        super().setup()
        self.connection.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
    def log_message(self, *_): pass
    def do_GET(self):
        status, mime, body = 200, 'text/html', b''
        if self.path in ('/many', '/deadline'):
            body = b'<html><head><script src="/loader"></script></head><body>Local bundle capacity fixture</body></html>'
            if self.path == '/deadline': body = body.replace(b'/loader', b'/slow-loader')
        elif self.path in ('/loader', '/slow-loader'):
            mime = 'application/javascript'
            body = b'''(async()=>{for(let base=0;base<170;base+=10)await Promise.all(Array.from({length:10},(_,n)=>new Promise(resolve=>{let s=document.createElement('script');s.src='/opaque-'+(base+n);s.onload=s.onerror=resolve;document.head.appendChild(s);})));fetch('/done');})();'''
            if self.path == '/slow-loader': body = body.replace(b's.onload=s.onerror=resolve;', b's.onload=s.onerror=()=>setTimeout(resolve,250);')
        elif self.path.startswith('/opaque-'):
            mime = 'application/javascript'
            index = int(self.path.rsplit('-', 1)[1])
            body = CORE if index == 110 else SEGMENT if index == 169 else b'void 0;'
        elif self.path == '/done':
            self.server.done = True
            mime, body = 'text/plain', b'complete'
        elif self.path == '/real':
            body = b'<script src="/real-core"></script><script src="/real-segment"></script>'
        elif self.path == '/real-core':
            mime, body = 'application/javascript', self.server.core
        elif self.path == '/real-segment':
            mime, body = 'application/javascript', self.server.segment
        else:
            status = 404
        self.send_response(status)
        self.send_header('Content-Type', mime)
        self.send_header('Content-Length', str(len(body)))
        self.end_headers()
        try: self.wfile.write(body)
        except (BrokenPipeError, ConnectionResetError, ConnectionAbortedError): pass


def start():
    server = http.server.ThreadingHTTPServer(('127.0.0.1', 0), Fixture)
    server.daemon_threads = True
    server.done = False
    threading.Thread(target=server.serve_forever, daemon=True).start()
    return server


def run(exe, out, server, path, *args):
    before = time.monotonic()
    p = subprocess.Popen([str(exe), '--target', f'http://127.0.0.1:{server.server_port}{path}', '--web-only', *args], stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    stdout, stderr = p.communicate(timeout=50)
    (out / (path[1:] + '.json')).write_bytes(stdout)
    (out / (path[1:] + '.stderr.txt')).write_bytes(stderr)
    assert p.returncode == 0, stderr.decode(errors='replace')
    assert not list(Path(tempfile.gettempdir()).glob(f'vector-browser-{p.pid}-*')), 'own browser profile remains'
    return json.loads(stdout), time.monotonic() - before


if __name__ == '__main__':
    exe, out = Path(sys.argv[1]).resolve(), Path(sys.argv[2]).resolve()
    out.mkdir(parents=True, exist_ok=True)
    server = start()
    checks = []
    def check(value, label):
        assert value, label
        checks.append(label)
    try:
        r, elapsed = run(exe, out, server, '/many')
        if r['browser']['state'] == 'unavailable': sys.exit(77)
        scripts = [s for s in r['scripts'] if s['collection'] == 'browser']
        check(server.done, '170 scripts executed through the actual browser')
        check(any(s['url'].endswith('/opaque-110') and s['analyzed'] for s in scripts), 'script beyond former 80 limit analyzed')
        check(any(f['name'] == 'core-js' and f['version'] == '3.49.7' for f in r['findings']), 'late bundle technology and advertised version identified')
        check(not any(f['name'] == 'Twilio Segment' for f in r['findings']), 'no name inferred from code outside new cap')
        check(sum(s['analyzed'] for s in scripts) <= 160, '160 script analysis bound')
        check(any(s['state'] == 'skipped' and 'limit' in s['reason'] for s in scripts), 'new cap records skipped scripts')
        check(all(s['body_bytes'] <= 2*1024*1024 for s in scripts), 'individual 2 MiB bound')
        check(len(r['browser']['requests']) <= 256, 'request count remains within existing bound')
        check(r['web_coverage']['page_requests'] <= 12, 'shared page cap preserved')
        check(elapsed < 40, 'bounded collection and cleanup')
        r, elapsed = run(exe, out, server, '/deadline', '--browser-timeout', '1000')
        check(elapsed < 15, 'short browser timeout interrupts script load sequence')
        check(any(p['state'] in ('limited','timed_out') for p in r['browser']['pages']), 'timeout coverage explicitly limited')
        if len(sys.argv) == 5:
            server.core = Path(sys.argv[3]).read_bytes()
            server.segment = Path(sys.argv[4]).read_bytes()
            r, elapsed = run(exe, out, server, '/real', '--no-browser')
            check(any(f['name']=='core-js' and f['version']=='3.42.0' for f in r['findings']), 'full captured core-js bundle replay')
            check(any(f['name']=='Twilio Segment' for f in r['findings']), 'full captured proxied Segment bundle replay')
            check(all(s['analyzed'] and not s['body_truncated'] for s in r['scripts']), 'both saved assets collected completely')
        (out/'verification.json').write_text(json.dumps({'checks': checks, 'count':len(checks)},indent=2),encoding='utf-8')
        print(f'{len(checks)} consumer collection checks passed')
    finally:
        server.shutdown();server.server_close()
