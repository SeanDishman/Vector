import http.server
import json
from pathlib import Path
import subprocess
import sys
import threading
import tempfile

assets = Path(__file__).parent / 'fixtures/react-18.3.1'
hits = []
redirect_body = b'''<HTML><HEAD><meta http-equiv="content-type" content="text/html;charset=utf-8">
<TITLE>301 Moved</TITLE></HEAD><BODY>
<H1>301 Moved</H1>
The document has moved
<A HREF="DESTINATION">here</A>.

</BODY></HTML>
'''


class Web(http.server.BaseHTTPRequestHandler):
    def log_message(self, *_): pass
    def handle(self):
        try: super().handle()
        except (BrokenPipeError, ConnectionResetError, ConnectionAbortedError): pass
    def do_GET(self):
        host = self.headers.get('Host', '').split(':')[0]
        browser = 'Chrome/' in self.headers.get('User-Agent', '')
        hits.append((host, self.path, browser))
        port = self.server.server_port
        status, mime, body, location = 200, 'text/html', b'', ''
        if self.path == '/':
            location = f'http://www.localhost:{port}/final'
        elif self.path == '/reverse':
            location = f'http://www.localhost:{port}/reverse-two'
        elif self.path == '/reverse-two':
            location = f'http://localhost:{port}/final'
        elif self.path == '/loop':
            location = f'http://{"localhost" if host == "www.localhost" else "www.localhost"}:{port}/loop'
        elif self.path == '/external':
            location = f'http://unrelated.invalid:{port}/final'
        elif self.path == '/suffix':
            location = f'http://www.localhost.attacker.invalid:{port}/final'
        elif self.path == '/printer':
            location = 'http://www.localhost:9100/'
        elif self.path == '/browser-alias':
            body = f'<script>location.replace("http://www.localhost:{port}/final")</script>'.encode()
        elif self.path == '/final':
            body = b'<title>Canonical destination fixture</title><div id="root"></div><script type="module" src="/entry.mjs"></script>'
        elif self.path == '/entry.mjs':
            mime, body = 'application/javascript', b"import('/chunk.mjs');"
        elif self.path == '/chunk.mjs':
            mime, body = 'application/javascript', b'''
              const load=url=>new Promise(resolve=>{const s=document.createElement('script');s.src=url;s.onload=resolve;document.head.appendChild(s)});
              await load('/react.production.min.js');await load('/react-dom.production.min.js');
              ReactDOM.createRoot(document.getElementById('root')).render(React.createElement('span',null,'Executed at the redirected host'));
            '''
        elif self.path in ('/react.production.min.js', '/react-dom.production.min.js'):
            mime, body = 'application/javascript', (assets / self.path[1:]).read_bytes()
        else:
            status = 404
        if location:
            status, body = 301, redirect_body.replace(b'DESTINATION', location.encode())
        self.send_response_only(status)
        self.send_header('Server', 'gws' if location else 'Vector canonical fixture')
        self.send_header('Content-Type', mime)
        if location: self.send_header('Location', location)
        self.send_header('Content-Length', str(len(body)))
        self.end_headers()
        self.wfile.write(body)


def start():
    server = http.server.ThreadingHTTPServer(('127.0.0.1', 0), Web)
    threading.Thread(target=server.serve_forever, daemon=True).start()
    return server


def run(exe, server, path, browser=False):
    flags = [] if browser else ['--no-browser']
    process = subprocess.Popen([str(exe), '--target', f'http://localhost:{server.server_port}{path}',
                                '--timeout', '200', '--http-timeout', '1500', *flags],
                               stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    stdout, stderr = process.communicate(timeout=40)
    assert process.returncode == 0, stderr
    report = json.loads(stdout)
    leftovers = list(Path(tempfile.gettempdir()).glob(f'vector-browser-{process.pid}-*'))
    assert not leftovers, 'Browser profile retained: ' + str(leftovers)
    return report


if __name__ == '__main__':
    exe = Path(sys.argv[1]).resolve()
    baseline = '--baseline' in sys.argv
    artifact = Path(sys.argv[sys.argv.index('--artifacts') + 1]) if '--artifacts' in sys.argv else None
    if artifact: artifact.mkdir(parents=True, exist_ok=True)
    checks, failures = 0, []
    def check(value, name):
        global checks
        checks += 1
        if not value: failures.append(name)
    server = start()
    try:
        for name, path, browser in (('google-redirect', '/', True), ('reverse', '/reverse', False),
                                    ('browser-alias', '/browser-alias', True), ('loop', '/loop', False),
                                    ('external', '/external', False), ('suffix', '/suffix', False), ('printer', '/printer', False)):
            first = len(hits)
            report = run(exe, server, path, browser)
            if artifact: (artifact / (name + '.json')).write_text(json.dumps(report, indent=2), encoding='utf-8')
            check(report['address'] == '127.0.0.1' and report['target']['host'] == 'localhost', name + ': original endpoint preserved')
            check(report['web_coverage']['page_requests'] <= 12, name + ': shared request cap preserved')
            if name in ('google-redirect', 'reverse'):
                check(any(p['status'] == 200 and p['url'].endswith('/final') for p in report['web']), name + ': final document collected')
                check(any('Following www website redirect' in n for n in report['notes']), name + ': redirect scope disclosed')
            if browser:
                if not baseline and report['browser']['state'] == 'unavailable': sys.exit(77)
                check(report['script_coverage']['javascript_executed'], name + ': JavaScript executed')
                check(any(f['name'] == 'React' for f in report['findings']), name + ': actual production React identified')
                check(any(s['collection'] == 'browser' and s['analyzed'] and s['url'].endswith('/chunk.mjs') for s in report['scripts']), name + ': dynamic module retained')
                check(any(h[0] == 'www.localhost' and h[1] == '/final' and h[2] for h in hits[first:]), name + ': browser sent canonical Host')
            if name in ('external', 'suffix', 'printer'):
                check(not any(h[1] == '/final' for h in hits[first:]), name + ': out-of-scope destination not visited')
            if name == 'loop':
                check(sum(p['url'].endswith('/loop') for p in report['web']) == 2, 'loop: each hostname attempted once')
                check(any('redirect loop' in n for n in report['notes']), 'loop: reason retained')
        print(json.dumps({'scenarios': 7, 'checks': checks, 'failed': failures}, indent=2))
        if failures and not baseline: sys.exit(1)
    finally:
        server.shutdown();server.server_close()
