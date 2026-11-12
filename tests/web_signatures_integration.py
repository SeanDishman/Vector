import http.server
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile
import threading
import time

ASSETS = Path(__file__).parent / 'fixtures' / 'web-signatures'
LIBRARIES = {'Leaflet': ('leaflet.js', '1.9.4'), 'Chart.js': ('chart.js.js', '4.4.8'), 'Handlebars': ('handlebars.js', '4.7.8')}
HITS = []


class Fixture(http.server.BaseHTTPRequestHandler):
    def log_message(self, *_):
        pass

    def do_GET(self):
        HITS.append(self.path)
        path = self.path.split('?', 1)[0]
        mime, status, body = 'text/html', 200, b''
        cookies = []
        if path in ('/runtime-only', '/script-only'):
            if path == '/runtime-only':
                body = b'''<title>Real libraries</title><body><script src="/loader"></script></body>'''
                cookies = ['laravel_session=PRIVATE_SESSION_VALUE; HttpOnly', 'XSRF-TOKEN=PRIVATE_CSRF_VALUE']
            else:
                body = ''.join(f'<script src="/full-{i}"></script>' for i in range(3)).encode()
        elif path == '/loader':
            mime = 'application/javascript'
            body = b'''setTimeout(async () => {
                for(let i=0;i<3;i++) await new Promise(resolve=>{const s=document.createElement('script');s.src='/opaque-'+i;s.onload=resolve;document.head.appendChild(s);});
                document.body.appendChild(document.createTextNode('x'.repeat(3200000)));
                const island=document.createElement('astro-island');island.setAttribute('component-url','/secret?token=PRIVATE_DOM_VALUE');island.setAttribute('renderer-url','/renderer');document.body.appendChild(island);
                const component=document.createElement('div');component.setAttribute('wire:id','PRIVATE_ID');component.setAttribute('wire:snapshot','PRIVATE_SNAPSHOT');component.setAttribute('wire:effects','{}');document.body.appendChild(component);
                document.documentElement.setAttribute('data-wf-site','0123456789abcdef01234567');document.documentElement.setAttribute('data-wf-page','abcdef0123456789abcdef01');
                fetch('/ready',{method:'GET'});
            },100);'''
        elif path.startswith(('/opaque-', '/full-')):
            i = int(path.rsplit('-', 1)[1])
            mime = 'application/javascript'
            body = (ASSETS / list(LIBRARIES.values())[i][0]).read_bytes()
            if path.startswith('/opaque-'):
                body = re.sub(br'\A/\*.*?\*/', b'', body, count=1, flags=re.S)
        elif path == '/negative':
            body = b'''<title>Misleading evidence</title><body><pre><astro-island component-url=x renderer-url=y></astro-island></pre>
              <template><div wire:id=a wire:snapshot=b wire:effects=c></div></template>
              <script>
              Object.defineProperty(globalThis,'Chart',{get(){fetch('/getter-used');throw Error('getter must not run');}});
              globalThis.L={version:'1.9.4'};
              globalThis.Handlebars={VERSION:'4.7.8',compile:'function',template:'function'};
              globalThis.Plotly=Object.create({version:'2.0.0',newPlot(){},react(){}});
              const prose='/*! Chart.js v4.4.8 https://www.chartjs.org */';
              </script></body>'''
            cookies = ['XSRF-TOKEN=PRIVATE_CSRF_VALUE', 'tracking=laravel_session; Path=/']
        elif path == '/limited':
            body = b'<script>for(let i=0;i<80;i++){const e=document.createElement("astro-island");e.setAttribute("component-url","/x");e.setAttribute("renderer-url","/y");document.body.appendChild(e);}</script>'
            body = b'<body>' + body + b'</body>'
        elif path == '/rejected':
            status, body = 403, b'<title>Rejected</title>'
            cookies = ['laravel_session=PRIVATE', 'XSRF-TOKEN=PRIVATE']
        elif path == '/ready':
            mime, body = 'text/plain', b'ready'
        else:
            status = 404
        self.send_response(status)
        self.send_header('Content-Type', mime)
        self.send_header('Content-Length', str(len(body)))
        for cookie in cookies:
            self.send_header('Set-Cookie', cookie)
        self.end_headers()
        try:
            self.wfile.write(body)
        except (BrokenPipeError, ConnectionResetError, ConnectionAbortedError):
            pass


def start():
    server = http.server.ThreadingHTTPServer(('127.0.0.1', 0), Fixture)
    threading.Thread(target=server.serve_forever, daemon=True).start()
    return server


def run(exe, out, path, server, no_browser=False, env=None):
    args = [str(exe), '--target', f'http://127.0.0.1:{server.server_port}{path}', '--web-only']
    if no_browser:
        args.append('--no-browser')
    proc = subprocess.Popen(args, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, env=env)
    stdout, stderr = proc.communicate(timeout=45)
    assert proc.returncode == 0, stderr
    report = json.loads(stdout)
    (out / (path.removeprefix('/') + '.json')).write_text(stdout, encoding='utf-8')
    leftovers = list(Path(tempfile.gettempdir()).glob(f'vector-browser-{proc.pid}-*'))
    assert not leftovers, f'browser profile retained: {[p.name for p in leftovers]}\n{stderr}'
    return report


def main():
    exe, out = Path(sys.argv[1]).resolve(), Path(sys.argv[2]).resolve()
    out.mkdir(parents=True, exist_ok=True)
    baseline = '--baseline' in sys.argv
    server = start()
    checks, misses = 0, []
    def check(ok, label):
        nonlocal checks
        checks += 1
        if not ok:
            misses.append(label)
    try:
        static = run(exe, out, '/script-only', server, True)
        for name, (_, version) in LIBRARIES.items():
            found = [f for f in static['findings'] if f['name'] == name and f['version'] == version]
            check(bool(found), 'real source: ' + name)
            check(any('rule ' in e['source'] for f in found for e in f['evidence']), 'source evidence: ' + name)
        report = run(exe, out, '/runtime-only', server)
        if report['browser']['state'] == 'unavailable':
            print('Installed browser unavailable');return 77
        for name, (_, version) in LIBRARIES.items():
            found = [f for f in report['findings'] if f['name'] == name and f['version'] == version]
            check(bool(found), 'executed library: ' + name)
            check(any('Browser runtime' in e['source'] and 'rule ' in e['source'] for f in found for e in f['evidence']), 'runtime evidence: ' + name)
        names = {f['name'] for f in report['findings']}
        for name in ('Astro', 'Livewire', 'Webflow', 'Laravel'):
            check(name in names, 'structured marker: ' + name)
        check(any(p['dom_truncated'] for p in report['browser']['pages']), 'DOM cap exercised')
        check('/ready' in HITS, 'real loader completed')
        check('PRIVATE_' not in json.dumps(report), 'secrets absent from report')
        check(report['web_coverage']['page_requests'] <= 12, 'shared page cap')
        negative = run(exe, out, '/negative', server)
        check(not ({'Astro', 'Livewire', 'Chart.js', 'Leaflet', 'Handlebars', 'Plotly.js', 'Laravel'} & {f['name'] for f in negative['findings']}), 'misleading examples rejected')
        check('/getter-used' not in HITS, 'accessor not invoked')
        limited = run(exe, out, '/limited', server)
        check('Astro' in {f['name'] for f in limited['findings']}, 'limited DOM retains findings')
        check(any('per-selector limit' in p['reason'] for p in limited['browser']['pages']), 'DOM limit explicit')
        rejected = run(exe, out, '/rejected', server, True)
        check('Laravel' not in {f['name'] for f in rejected['findings']}, 'rejected cookies do not identify framework')
    finally:
        server.shutdown()
    (out / 'verification.json').write_text(json.dumps({'checks': checks, 'failures': misses, 'baseline': baseline}, indent=2), encoding='utf-8')
    print(f'{checks} web signature collection checks; {len(misses)} failures: {misses}')
    return 0 if baseline or not misses else 1


if __name__ == '__main__':
    sys.exit(main())
