import http.server
import json
from pathlib import Path
import subprocess
import sys
import threading

hits = []
LIBRARY = b'/*! jQuery v3.99.1 | (c) OpenJS Foundation and other contributors | jquery.org/license */\n!function(){}();'
HOME = b'<!doctype html><title>Destination fixture</title><script src="/library.js"></script><a href="/about">About</a>'
APP = (b'<!doctype html><title>Destination application</title><div id="root"></div><script>'
       b'const s=document.createElement("script");s.src="/late.js";s.onload=()=>fetch("/proof");document.head.appendChild(s);</script>')


class Web(http.server.BaseHTTPRequestHandler):
    def log_message(self, *_): pass

    def handle(self):
        try: super().handle()
        except (BrokenPipeError, ConnectionResetError, ConnectionAbortedError): pass

    def do_GET(self):
        bound = self.server.server_address[0]
        port = self.server.server_port
        hits.append((bound, self.path))
        status, mime, body, location = 200, 'text/html', b'', ''
        other = lambda number, path: f'http://127.0.0.{number}:{port}{path}'
        if bound == '127.0.0.1':
            routes = {'/': other(2, '/home'), '/outside': f'http://192.0.2.10:{port}/home', '/named': f'http://unrelated.invalid:{port}/home',
                      '/chain': other(2, '/hop'), '/three': other(2, '/hop-three'), '/loop': other(2, '/back'),
                      '/port': f'http://127.0.0.2:{port + 1}/home', '/deep': other(2, '/home'), '/robots.txt': other(2, '/robots.txt'),
                      '/dynamic': other(2, '/app')}
            if self.path in routes:
                location = routes[self.path]
            elif self.path == '/links':
                body = b'<!doctype html><title>Entry stays here</title><a href="/deep">Moved section</a>'
            else:
                status = 404
        elif self.path == '/home':
            body = HOME
        elif self.path == '/about':
            body = b'<!doctype html><title>About the destination</title>'
        elif self.path == '/library.js':
            mime, body = 'application/javascript', LIBRARY
        elif self.path == '/app':
            body = APP
        elif self.path == '/late.js':
            mime, body = 'application/javascript', LIBRARY.replace(b'3.99.1', b'3.99.2')
        elif self.path == '/proof':
            mime, body = 'text/plain', b'Executed at the destination'
        elif self.path == '/robots.txt':
            mime, body = 'text/plain', b'User-agent: *\nDisallow:\n'
        elif self.path == '/hop':
            location = other(3, '/home')
        elif self.path == '/hop-three':
            location = other(3, '/hop-four')
        elif self.path == '/hop-four':
            location = other(4, '/home')
        elif self.path == '/back':
            location = other(1, '/loop')
        else:
            status = 404
        if location:
            status, body = 301, b'Moved'
        self.send_response_only(status)
        self.send_header('Server', 'Vector redirect fixture')
        self.send_header('Content-Type', mime)
        if location: self.send_header('Location', location)
        self.send_header('Content-Length', str(len(body)))
        self.end_headers()
        self.wfile.write(body)


def start():
    first = http.server.ThreadingHTTPServer(('127.0.0.1', 0), Web)
    servers = [first] + [http.server.ThreadingHTTPServer((f'127.0.0.{n}', first.server_port), Web) for n in (2, 3)]
    for server in servers:
        threading.Thread(target=server.serve_forever, daemon=True).start()
    return servers


def run(exe, port, path, browser=False):
    flags = ['--browser-timeout', '6000'] if browser else ['--no-browser']
    process = subprocess.Popen([str(exe), '--target', f'http://127.0.0.1:{port}{path}', '--timeout', '200', '--http-timeout', '1500', *flags],
                               stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    stdout, stderr = process.communicate(timeout=60)
    assert process.returncode == 0, stderr
    return json.loads(stdout)


if __name__ == '__main__':
    exe = Path(sys.argv[1]).resolve()
    artifact = Path(sys.argv[sys.argv.index('--artifacts') + 1]) if '--artifacts' in sys.argv else None
    if artifact: artifact.mkdir(parents=True, exist_ok=True)
    checks, failures = 0, []

    def check(value, name):
        global checks
        checks += 1
        if not value: failures.append(name)

    servers = start()
    port = servers[0].server_port
    try:
        for name, path in (('followed', '/'), ('outside', '/outside'), ('named', '/named'), ('chain', '/chain'), ('three', '/three'),
                           ('loop', '/loop'), ('port', '/port'), ('links', '/links')):
            first = len(hits)
            report = run(exe, port, path)
            seen = hits[first:]
            notes = report['notes']
            if artifact: (artifact / (name + '.json')).write_text(json.dumps(report, indent=2), encoding='utf-8')
            check(report['address'] == '127.0.0.1' and report['target']['host'] == '127.0.0.1', name + ': entered endpoint preserved')
            check(report['web_coverage']['page_requests'] <= 12, name + ': shared page cap preserved')
            check(all(s['port'] == port for s in report['services']), name + ': no service row for a destination')
            pages = {p['url']: p for p in report['web']}
            if name == 'followed':
                home = pages.get(f'http://127.0.0.2:{port}/home')
                check(home is not None and home['status'] == 200 and home['address'] == '127.0.0.2', 'followed: destination page keeps its own URL and address')
                check(any("Following the site's redirect to 127.0.0.2" in n for n in notes), 'followed: redirect disclosed')
                check(not any('www website redirect' in n for n in notes), 'followed: not described as a www alias')
                check(any(f['name'] == 'jQuery' and f['version'] == '3.99.1' for f in report['findings']), 'followed: destination script analyzed')
                check(any(s['url'].endswith('/library.js') and s['analyzed'] and '127.0.0.2' in s['url'] for s in report['scripts']), 'followed: script evidence keeps the destination URL')
                check(('127.0.0.2', '/robots.txt') in seen, 'followed: standard paths are read on the followed host')
                check(('127.0.0.1', '/robots.txt') not in seen, 'followed: standard paths no longer spend an attempt on the moved host')
                check(('127.0.0.2', '/about') in seen, 'followed: the destination is crawled as the site')
            if name in ('outside', 'named'):
                check(not any(h[0] != '127.0.0.1' for h in seen), name + ': out-of-scope destination never requested')
                check(any('not followed (outside' in n for n in notes), name + ': refusal disclosed')
                check(not any('Following' in n for n in notes), name + ': nothing reported as followed')
                check(len([p for p in report['web'] if '127.0.0.1' not in p['url']]) == 0, name + ': no page entry for the refused destination')
            if name == 'chain':
                check(('127.0.0.3', '/home') in seen, 'chain: second hostname followed')
                check(sum("Following the site's redirect" in n for n in notes) == 2, 'chain: both hostnames disclosed')
                check(any(f['name'] == 'jQuery' for f in report['findings']), 'chain: final page analyzed')
            if name == 'three':
                check(('127.0.0.3', '/hop-four') in seen, 'three: second hostname reached')
                check(any('two-host limit' in n for n in notes), 'three: third hostname refused')
                check(not any('127.0.0.4' in p['url'] for p in report['web']), 'three: third hostname never requested')
            if name == 'loop':
                check(any('redirect loop' in n for n in notes), 'loop: loop stopped with a reason')
                check(sum(1 for h in seen if h[1] in ('/loop', '/back')) <= 3, 'loop: no repeated requests')
            if name == 'port':
                check(any('non-web port' in n for n in notes), 'port: other port refused')
                check(not any(h[0] != '127.0.0.1' for h in seen), 'port: destination never requested')
            if name == 'links':
                check(any(p['url'].endswith('/links') and p['status'] == 200 for p in report['web']), 'links: entry page collected')
                check(not any(h == ('127.0.0.2', '/home') for h in seen), 'links: a later page redirect to another host is not followed')
                check(not any('Following' in n for n in notes), 'links: nothing reported as followed')
        scenarios = 8
        first = len(hits)
        report = run(exe, port, '/dynamic', browser=True)
        seen = hits[first:]
        if artifact: (artifact / 'browser.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
        if report['browser']['state'] == 'unavailable':
            print('Installed browser unavailable; browser scenario skipped')
        else:
            scenarios += 1
            executed = report['browser']['pages']
            check(any('127.0.0.2' in p['url'] and p['url'].endswith('/app') and p['status'] == 200 for p in executed), 'browser: page entry carries the destination URL')
            check(not any('127.0.0.1' in p['url'] and p['status'] == 200 for p in executed), 'browser: no executed page is attributed to the entered host')
            check(('127.0.0.2', '/proof') in seen, 'browser: JavaScript ran on the followed host')
            check(any(s['collection'] == 'browser' and s['analyzed'] and s['url'].endswith('/late.js') and '127.0.0.2' in s['url'] for s in report['scripts']),
                  'browser: dynamically loaded script keeps the destination URL')
            check(any(f['name'] == 'jQuery' and f['version'] == '3.99.2' and any('Browser script' in e['source'] for e in f['evidence']) for f in report['findings']),
                  'browser: dynamically loaded script analyzed')
            check(report['address'] == '127.0.0.1' and report['target']['host'] == '127.0.0.1' and all(s['port'] == port for s in report['services']),
                  'browser: entered endpoint and its services unchanged')
            check(not any(h.get('host') == '127.0.0.1' for h in report.get('endpoint_hosts', [])), 'browser: the entered endpoint is not listed as its own third party')
        print(json.dumps({'scenarios': scenarios, 'checks': checks, 'failed': failures}, indent=2))
        if failures: sys.exit(1)
    finally:
        for server in servers:
            server.shutdown(); server.server_close()
