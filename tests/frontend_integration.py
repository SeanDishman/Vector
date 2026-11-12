import http.server
import json
from pathlib import Path
import subprocess
import sys
import threading
import time
import uuid

TOKEN = uuid.uuid4().hex
CSS = b'/*! tailwindcss v4.3.3 | MIT License | https://tailwindcss.com */\n.a{display:flex}'
SDK = {'sdkInfo': {'sdkType': 'statsig-server-core-node'}, 'feature_gates': {}, 'dynamic_configs': {}, 'layer_configs': {}}
BOOT = ('<span data-radix-focus-start></span><script type=application/json>' + json.dumps({'statsigPayload': json.dumps(SDK)}) +
        '</script><script>window.__reactRouterContext={ssr:true};</script><script type=module>window.__reactRouterRouteModules={root:{}};</script>')


class Fixture(http.server.ThreadingHTTPServer):
    def __init__(self):
        self.requests = []
        super().__init__(('127.0.0.1', 0), Handler)
        threading.Thread(target=self.serve_forever, daemon=True).start()

    def url(self, path='/entry'):
        return f'http://127.0.0.1:{self.server_port}{path}'


class Handler(http.server.BaseHTTPRequestHandler):
    def log_message(self, *_):
        pass

    def do_GET(self):
        self.server.requests.append(self.path)
        status, mime, body = 200, 'text/html', b''
        if self.path == '/entry':
            body = (f'<title>Frontend fixture</title><link rel=stylesheet href="/{TOKEN}">' +
                    '<script type=application/json>' + json.dumps({'padding': 'x' * 650000}) + '</script>' + BOOT).encode()
        elif self.path in ('/wrong', '/large', '/redirect', '/slow', '/denied', '/json'):
            body = f'<link rel=stylesheet href="/asset{self.path}">'.encode()
            if self.path == '/denied':
                status = 403
            if self.path == '/json':
                mime = 'application/json'
                body = json.dumps({'example': body.decode()}).encode()
        elif self.path == '/caps':
            body = (''.join(f'<link rel=stylesheet href="/{TOKEN}?n={i}">' for i in range(12)) +
                    f'<link rel=stylesheet href="http://192.0.2.10:{self.server.server_port}/must-not-follow">' +
                    '<template><link rel=stylesheet href="/inert"></template>').encode()
        elif self.path.startswith('/' + TOKEN):
            mime, body = 'text/css', CSS
        elif self.path.startswith('/asset/'):
            mime, body = 'text/css', CSS
            if self.path.endswith('/wrong'):
                mime = 'text/html'
            elif self.path.endswith('/large'):
                body += b'x' * 600000
            elif self.path.endswith('/redirect'):
                status = 302
            elif self.path.endswith('/slow'):
                time.sleep(2)
        else:
            status = 404
        try:
            self.send_response(status)
            self.send_header('Content-Type', mime)
            self.send_header('Content-Length', str(len(body)))
            if status == 302:
                self.send_header('Location', '/must-not-follow')
            self.end_headers()
            self.wfile.write(body)
        except (BrokenPipeError, ConnectionResetError, ConnectionAbortedError):
            pass


def main():
    exe, helper, out = Path(sys.argv[1]).resolve(), Path(sys.argv[2]).resolve(), Path(sys.argv[3]).resolve()
    out.mkdir(parents=True, exist_ok=True)
    server = Fixture()

    def run(path, cancel=False):
        server.requests.clear()
        command = [str(helper), '--cancel', server.url(path)] if cancel else [str(exe), '--target', server.url(path), '--web-only', '--no-browser']
        start = time.monotonic()
        p = subprocess.run(command, capture_output=True, text=True, timeout=12)
        assert p.returncode == 0, p.stderr
        r = json.loads(p.stdout)
        (out / (path[1:] + ('-cancel' if cancel else '') + '.json')).write_text(json.dumps(r, indent=2))
        assert r['dns']['state'] == 'skipped'
        return r, time.monotonic() - start

    try:
        r, _ = run('/entry')
        names = {f['name'] for f in r['findings']}
        assert {'React Router', 'Radix UI', 'Statsig', 'Tailwind CSS'} <= names, names
        assert not r['web'][0]['body_truncated'] and r['web'][0]['body_bytes'] > 512 * 1024
        assert r['stylesheets'][0]['analyzed'] and '/' + TOKEN in r['stylesheets'][0]['url']
        r, _ = run('/caps')
        assert sum(s['analyzed'] for s in r['stylesheets']) == 8
        assert '/must-not-follow' not in server.requests and '/inert' not in server.requests
        assert any('Eight-stylesheet' in reason for reason in r['web_coverage']['reasons'])
        outside = [s for s in r['stylesheets'] if 'outside' in s['reason']]
        assert len(outside) == 1 and outside[0]['state'] == 'skipped' and outside[0]['status'] == 0, outside
        r, _ = run('/wrong')
        assert not r['stylesheets'][0]['analyzed'] and all(f['name'] != 'Tailwind CSS' for f in r['findings'])
        r, _ = run('/large')
        assert r['stylesheets'][0]['analyzed'] and r['stylesheets'][0]['body_truncated']
        assert r['stylesheets'][0]['body_bytes'] == 512 * 1024
        r, _ = run('/redirect')
        assert r['stylesheets'][0]['status'] == 302 and not r['stylesheets'][0]['analyzed']
        assert '/must-not-follow' not in server.requests
        r, _ = run('/denied')
        assert not r['stylesheets'] and '/asset/denied' not in server.requests
        r, _ = run('/json')
        assert not r['stylesheets'] and '/asset/json' not in server.requests
        r, elapsed = run('/slow')
        assert r['stylesheets'][0]['state'] == 'failed' and elapsed < 3.5
        r, elapsed = run('/slow', cancel=True)
        assert r['status'] == 'cancelled' and r['stylesheets'][0]['state'] == 'cancelled'
        events = [e for e in r['activity'] if e['item'].startswith('Stylesheet ')]
        transfer_ms = events[-1]['elapsed_ms'] - events[0]['elapsed_ms']
        assert '/asset/slow' in server.requests and transfer_ms < 1300 and elapsed < 3
        print('Nine frontend network scenarios passed: large bootstrap, opaque stylesheet, scope, MIME, caps, rejection, timeout and active cancellation.')
    finally:
        server.shutdown()
        server.server_close()


if __name__ == '__main__':
    main()
