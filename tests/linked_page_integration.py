import http.server
import json
from pathlib import Path
import subprocess
import sys
import threading

hits = []
LIBRARY = b'/*! jQuery v3.99.1 | (c) OpenJS Foundation and other contributors | jquery.org/license */\n!function(){}();'
ENTRY = ('<!doctype html><title>Entered site</title><meta name="generator" content="WordPress 6.8">'
         '<script>fetch("/api/site/list").catch(()=>0);</script>{{links}}').replace('{{links}}', '{links}')
DOCS_LIBRARY = LIBRARY + b'\nfetch("/api/docs-only/list").catch(()=>0);'
GUIDE = (b'<!doctype html><title>Guide</title><meta name="generator" content="Mintlify">'
         b'<script src="/_mintlify/assets/app.js"></script><a href="/guide/more">More</a><a href="/docs/next">Next</a>')
DYNAMIC = (b'<!doctype html><title>Entered application</title><div id="menu"></div><script>'
           b'const a=document.createElement("a");a.href="/docs/intro";a.textContent="Documentation";'
           b'document.getElementById("menu").appendChild(a);</script>')


def link(*paths):
    return ''.join(f'<a href="{path}">Link</a>' for path in paths)


class Web(http.server.BaseHTTPRequestHandler):
    def log_message(self, *_): pass

    def handle(self):
        try: super().handle()
        except (BrokenPipeError, ConnectionResetError, ConnectionAbortedError): pass

    def do_GET(self):
        bound = self.server.server_address[0]
        port = self.server.server_port
        hits.append((bound, self.path))
        status, mime, body, location, extra = 200, 'text/html', b'', '', {}
        if bound == '127.0.0.1':
            pages = {
                '/': ENTRY.format(links=link('/docs/intro', '/pricing', '/about')),
                '/samehost': ENTRY.format(links=link('/documentation/start', '/about')),
                '/external': ENTRY.format(links=link(f'http://192.0.2.10:{port}/docs', f'http://127.0.0.3:{port}/docs', '/documents/annual')),
                '/outside': ENTRY.format(links=link('/docs/away')),
                '/many': ENTRY.format(links=link('/docs/intro', *[f'/p/{n}' for n in range(1, 21)])),
                '/none': ENTRY.format(links=link('/pricing', '/about')),
            }
            if self.path in pages:
                body = pages[self.path].encode()
            elif self.path == '/dynamic':
                body = DYNAMIC
            elif self.path == '/docs/intro':
                location = f'http://127.0.0.2:{port}/guide'
            elif self.path == '/docs/away':
                location = f'http://192.0.2.10:{port}/guide'
            elif self.path == '/documentation/start':
                body = GUIDE
            elif self.path == '/_mintlify/assets/app.js':
                mime, body = 'application/javascript', LIBRARY
            elif self.path in ('/pricing', '/about') or self.path.startswith('/p/'):
                body = b'<!doctype html><title>Ordinary page</title>'
            elif self.path == '/robots.txt':
                mime, body = 'text/plain', b'User-agent: *\nDisallow:\n'
            else:
                status = 404
        elif bound == '127.0.0.2' and self.path == '/guide':
            body, extra = GUIDE, {'X-Vercel-Id': 'iad1::fixture-1700000000000-0123456789ab'}
        elif bound == '127.0.0.2' and self.path == '/_mintlify/assets/app.js':
            mime, body = 'application/javascript', DOCS_LIBRARY
        else:
            status = 404
        if location:
            status, body = 302, b'Moved'
        self.send_response_only(status)
        self.send_header('Server', 'Vector linked page fixture')
        self.send_header('Content-Type', mime)
        if location: self.send_header('Location', location)
        for name, value in extra.items(): self.send_header(name, value)
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
    stdout, stderr = process.communicate(timeout=90)
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
    home, docs = f'http://127.0.0.1:{port}', f'http://127.0.0.2:{port}'
    try:
        for name, path in (('redirected', '/'), ('samehost', '/samehost'), ('external', '/external'), ('outside', '/outside'),
                           ('many', '/many'), ('none', '/none')):
            first = len(hits)
            report = run(exe, port, path)
            seen = hits[first:]
            if artifact: (artifact / (name + '.json')).write_text(json.dumps(report, indent=2), encoding='utf-8')
            linked = report['linked_pages']
            names = {f['name'] for f in report['findings']}
            away = [h for h in seen if h[0] != '127.0.0.1']
            check(report['address'] == '127.0.0.1' and report['target']['host'] == '127.0.0.1', name + ': entered endpoint preserved')
            check('WordPress' in names, name + ': entered page still analyzed')
            if name == 'redirected':
                check(len(linked) == 1 and linked[0]['state'] == 'collected' and linked[0]['role'] == 'documentation', name + ': linked page collected')
                check(linked and linked[0]['requested_url'] == home + '/docs/intro' and linked[0]['url'] == docs + '/guide', name + ': link and final address kept')
                check(linked and linked[0]['discovered_from'] == home + '/' and linked[0]['status'] == 200, name + ': source page and status kept')
                check('Mintlify' in names and linked and 'Mintlify' in linked[0]['first_seen_here'], name + ': linked page technology named and marked')
                check(linked and 'WordPress' not in linked[0]['first_seen_here'], name + ': entered site technology not marked')
                check(sorted(set(h[1] for h in away)) == ['/_mintlify/assets/app.js', '/guide'] and all(h[0] == '127.0.0.2' for h in away),
                      name + ': one page and its script, nothing else on the linked host')
                check(report['web'][-1]['url'] == docs + '/guide' and [h[1] for h in seen].count('/docs/intro') == 1, name + ': linked page read last, its link read once')
                check(any('links to its documentation' in note for note in report['notes']), name + ': note explains the extra page')
                check(any(v['name'] == 'Vercel' for v in report['vendors']) or 'Vercel' in names, name + ': linked page response header analyzed')
                check(any(e['item'].startswith('Linked page ') and e['state'] == 'complete' for e in report['activity']), name + ': activity records the linked page')
                paths = {e['path'] for e in report['api_endpoints']}
                check('/api/site/list' in paths and '/api/docs-only/list' not in paths, name + ': API endpoints of the linked page are not listed under the entered site')
            elif name == 'samehost':
                paths = [h[1] for h in seen]
                check(linked == [] and 'Mintlify' in names, name + ': a documentation page on the entered host is an ordinary page of the site')
                check(not away and paths.count('/documentation/start') == 1 and paths.index('/documentation/start') < paths.index('/about'),
                      name + ': read once, before other links, no other host')
            elif name == 'external':
                check(linked == [] and not away, name + ': links to other hosts and a documents path are not documentation links')
            elif name == 'outside':
                check(len(linked) == 1 and linked[0]['state'] == 'skipped' and 'not requested' in linked[0]['reason'] and '192.0.2.10' in linked[0]['reason'],
                      name + ': redirect outside the scope recorded and refused')
                check(not away and linked and linked[0]['requested_url'] == home + '/docs/away', name + ': no request outside the address scope')
            elif name == 'many':
                ordinary = [h for h in seen if h[1].startswith('/p/')]
                check(len(linked) == 1 and linked[0]['state'] == 'collected', name + ': linked page still read when the crawl fills its budget')
                check(report['web_coverage']['page_requests'] <= 12 and len(ordinary) <= 11, name + ': the twelve-page limit holds for the entered site')
                check(linked and 1 <= linked[0]['requests'] <= 3 and linked[0]['request_limit'] == 3, name + ': the linked page stays inside its own three requests')
                check(any('12-page' in reason for reason in report['web_coverage']['reasons']), name + ': the limit is reported')
            else:
                check(linked == [] and not away, name + ': no documentation link, no extra page')
        first = len(hits)
        report = run(exe, port, '/dynamic', browser=True)
        if artifact: (artifact / 'dynamic.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
        if report['browser']['state'] == 'unavailable' or not report['browser']['pages']:
            print('Rendered link scenario skipped: no browser available.')
        else:
            linked = report['linked_pages']
            check(len(linked) == 1 and linked[0]['state'] == 'collected' and linked[0]['url'] == docs + '/guide', 'dynamic: a link added by page code is followed')
            check(any(p.get('url', '').startswith(docs + '/guide') or p.get('final_url', '').startswith(docs + '/guide') for p in report['browser']['pages']),
                  'dynamic: the linked page runs in the browser')
    finally:
        for server in servers:
            server.shutdown(); server.server_close()
    if failures:
        raise SystemExit('Failed: ' + '; '.join(failures))
    print(f'{checks} linked page checks passed: redirected and same-host documentation pages, scope refusal, other hosts, page budget and a rendered link.')
