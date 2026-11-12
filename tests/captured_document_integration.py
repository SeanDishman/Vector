import http.server
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import threading
import time

assets = Path(__file__).parent / 'fixtures/react-18.3.1'
hits = []
artifact_dir = None
HTML = b'''<!doctype html><html><head><title>Collected document fixture</title>
<link rel="stylesheet" href="/assets/site.css">
<script nonce="fixture" data-tsr-stream-part="">
(self.$R=self.$R||{})["tsr"]=[];self.$_TSR={h(){},e(){},buffer:[]};$_TSR.router={matches:[]};
</script><script nonce="fixture">
!function(){
  var stylesheetHref='/assets/site.css',entryHref='/assets/entry.mjs';
  function inject(){var s=document.createElement('script');s.type='module';s.src=entryHref;document.head.appendChild(s)}
  var link=document.querySelector('link[rel="stylesheet"]');
  if(link.sheet)inject();else{link.addEventListener('load',inject,{once:true});link.addEventListener('error',inject,{once:true})}
}();document.currentScript.remove();
</script></head><body><div id="root"></div></body></html>'''


class Web(http.server.BaseHTTPRequestHandler):
    def log_message(self, *_): pass
    def handle(self):
        try: super().handle()
        except (BrokenPipeError, ConnectionResetError, ConnectionAbortedError): pass

    def do_GET(self):
        browser = 'Chrome/' in self.headers.get('User-Agent', '')
        hits.append((self.server.server_port, self.path, browser, self.headers.get('Cookie', '')))
        status, mime, body = 200, 'text/html', HTML
        location = ''
        path = self.path
        if path == '/' and getattr(self.server, 'redirect_port', 0):
            status, body, location = 302, b'', f'http://127.0.0.1:{self.server.redirect_port}/'
        elif path.startswith('/assets/'):
            if path.endswith('site.css'):
                time.sleep(.15)
                mime, body = 'text/css', b'body { color: black; }'
            elif path.endswith('entry.mjs'):
                mime, body = 'application/javascript', b"import('/assets/chunk.mjs');"
            elif path.endswith('chunk.mjs'):
                mime, body = 'application/javascript', b'''
                const load=url=>new Promise(resolve=>{const s=document.createElement('script');s.src=url;s.onload=resolve;document.head.appendChild(s)});
                await load('/assets/react.production.min.js');await load('/assets/react-dom.production.min.js');
                ReactDOM.createRoot(document.getElementById('root')).render(React.createElement('span',null,'Loaded through CSS-gated modules'));
                '''
            else:
                mime, body = 'application/javascript', (assets / path.rsplit('/', 1)[-1]).read_bytes()
        elif path == '/pending':
            time.sleep(12); mime, body = 'application/json', b'{}'
        elif path in ('/captured', '/csp', '/truncated', '/denied', '/transport', '/cancel'):
            if browser or path == '/denied':
                if browser and path == '/transport':
                    self.connection.close(); return
                status, mime, body = 403, 'text/plain', b'Document request rejected'
            elif path == '/truncated':
                body += b' ' * (520 * 1024)
            elif path == '/cancel':
                body += b'<script nonce="fixture">fetch("/pending")</script>'
        elif path != '/':
            status, body = 404, b''
        self.send_response(status)
        self.send_header('Content-Type', mime)
        if location: self.send_header('Location', location)
        if status == 200 and mime == 'text/html':
            self.send_header('Content-Security-Policy', "default-src 'none'; script-src 'none'; style-src 'self'" if path == '/csp' else
                             "default-src 'none'; script-src 'self' 'nonce-fixture'; style-src 'self'; connect-src 'self'")
            if not browser: self.send_header('Set-Cookie', 'static_session=CAPTURED_SECRET; SameSite=Lax')
        self.send_header('Content-Length', str(len(body)))
        self.end_headers(); self.wfile.write(body)


def start(port=0):
    server = http.server.ThreadingHTTPServer(('127.0.0.1', port), Web)
    threading.Thread(target=server.serve_forever, daemon=True).start()
    return server


def run(exe, target, *args):
    before = set(Path(tempfile.gettempdir()).glob('vector-browser-*'))
    process = subprocess.Popen([str(exe), '--target', target, '--timeout', '200', '--http-timeout', '600', *args],
                               stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    stdout, stderr = process.communicate(timeout=40)
    assert process.returncode == 0, stderr
    report = json.loads(stdout)
    if artifact_dir:
        artifact_dir.mkdir(parents=True, exist_ok=True)
        (artifact_dir / (target.rsplit('/', 1)[-1].replace(':', '-') + '.json')).write_text(json.dumps(report, indent=2), encoding='utf-8')
    leftovers = [p for p in Path(tempfile.gettempdir()).glob(f'vector-browser-{process.pid}-*') if p not in before]
    if leftovers:
        out = Path('docs/reports/coverage-diagnosis/runtime-cleanup')
        out.mkdir(parents=True, exist_ok=True)
        (out / f'{process.pid}.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
        (out / f'{process.pid}.stderr').write_text(stderr, encoding='utf-8')
    assert not leftovers, 'Own browser profile was not cleaned up: ' + str([p.name for p in leftovers])
    return report


if __name__ == '__main__':
    exe = Path(sys.argv[1]).resolve()
    baseline = '--baseline' in sys.argv
    if '--artifacts' in sys.argv: artifact_dir = Path(sys.argv[sys.argv.index('--artifacts') + 1])
    checks, failures = 0, []
    def check(value, description):
        global checks
        checks += 1
        if not value:
            failures.append(description)
            if not baseline: raise AssertionError(description)
    server = start(); services = [server]
    base = f'http://127.0.0.1:{server.server_port}'
    try:
        for path in ('/captured', '/transport'):
            first = len(hits)
            report = run(exe, base + path)
            if report['browser']['state'] == 'unavailable': sys.exit(77)
            check(any(p.get('document_source') == 'captured_http' and p['javascript_enabled'] and p['scripts_analyzed'] >= 4
                      for p in report['browser']['pages']), path + ': successful HTML executes CSS-gated module chain')
            check(any(p.get('document_source', 'network') == 'network' and p['state'] in ('http_error', 'failed')
                      for p in report['browser']['pages']), path + ': original navigation failure retained')
            check(any(f['name'] == 'TanStack Router' for f in report['findings']), path + ': paired SSR markers detected')
            check(any(f['name'] == 'React' and any('captured HTTP page' in e['source'] for e in f['evidence'])
                      for f in report['findings']), path + ': real React identified with document provenance')
            check(any(q.get('collection') == 'captured_http' and q['type'] == 'Document' and q['status'] == 200
                      and q['transferred_bytes'] == 0
                      for q in report['browser']['requests']), path + ': supplied document is not a new network response')
            if path == '/captured':
                check(sum(h[1] == path and h[2] for h in hits[first:]) == 1, path + ': fallback avoids a second document transfer')
            check(not any(h[3] for h in hits[first:]) and 'CAPTURED_SECRET' not in json.dumps(report), path + ': redacted static cookies are not replayed')
        first = len(hits)
        report = run(exe, base + '/csp')
        check(any(p.get('document_source') == 'captured_http' for p in report['browser']['pages']), 'CSP fixture uses captured response')
        check(not any(h[1].endswith(('entry.mjs', 'chunk.mjs', '.js')) for h in hits[first:]), 'captured Content-Security-Policy still blocks scripts')
        for path in ('/truncated', '/denied'):
            report = run(exe, base + path)
            check(not any(p.get('document_source') == 'captured_http' for p in report['browser']['pages']), path + ': incomplete or rejected HTML is not replayed')
            check(all(p['status'] == 403 and p['state'] == 'http_error' and p['scripts_analyzed'] == 0 for p in report['browser']['pages']),
                  path + ': browser rejection remains explicit with no analyzed scripts')
        for port in (8080, 8000, 8008, 8081, 8888):
            try: canonical = start(port); break
            except OSError: continue
        else: raise RuntimeError('No loopback fixture web port available')
        services.append(canonical); server.redirect_port = canonical.server_port
        first = len(hits)
        report = run(exe, '127.0.0.1', '--ports', f'{canonical.server_port},{server.server_port}', '--no-browser')
        entry_hits = [h for h in hits[first:] if h[0] == canonical.server_port and h[1] == '/']
        check(len(entry_hits) == 1, 'cross-port redirect does not reread the canonical entry')
        urls = [h['url'] for h in report['web']]
        check(len(set(urls)) == len(urls), 'static attempts are deduplicated across endpoint queues')
        if baseline: print(f'Previous release: {len(failures)} of {checks} assertions failed: ' + json.dumps(failures))
        else: print(f'6 captured-document scenarios passed; {checks} checks covering real module execution, provenance, policy, failure and deduplication.')
    finally:
        for service in services: service.shutdown()
