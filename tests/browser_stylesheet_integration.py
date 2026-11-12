import argparse
import http.server
import json
from pathlib import Path
import subprocess
import threading

TAILWIND = b':root{--tw-translate-x:0;--tw-ring-shadow:0 0 #0000;--tw-shadow:0 0 #0000}'
BOOTSTRAP = b':root{--bs-body-font-family:system-ui;--bs-gutter-x:1px;--bs-btn-padding-x:1px}'
LARGE = TAILWIND + b'/* padding */\n' * 45000 + BOOTSTRAP
MISLEADING = b'.note::after{content:"--tw-translate-x:0;--tw-ring-shadow:0;--tw-shadow:0"}'


class Handler(http.server.BaseHTTPRequestHandler):
    def log_message(self, *_):
        pass

    def do_GET(self):
        route = self.path.split('?', 1)[0]
        mime, status, payload = 'text/html', 200, b''
        if self.server.asset:
            mime = 'text/css'
            if route == '/actual':
                payload = self.server.css
            elif route == '/large':
                payload = LARGE
            elif route == '/wrong-mime':
                mime, payload = 'text/html', b'<pre>' + TAILWIND + b'</pre>'
            elif route == '/inert':
                payload = MISLEADING
            else:
                payload = b'body{color:green}'
        elif route in ('/', '/negative', '/many'):
            resources = ['/actual'] if route == '/' else ['/wrong-mime', '/inert'] if route == '/negative' else ['/large'] + [f'/small-{i}' for i in range(9)]
            payload = ('''<!doctype html><title>Dynamic stylesheet fixture</title><body><script>
              const resources=RESOURCES;
              for(const path of resources){
                const link=document.createElement('link');link.rel='stylesheet';
                link.href='http://127.0.0.1:PORT'+path+'?fixture=SECRET';
                document.head.appendChild(link);
              }
            </script></body>'''.replace('RESOURCES', json.dumps(resources)).replace('PORT', str(self.server.asset_port))).encode()
        else:
            status = 404
        try:
            self.send_response(status)
            self.send_header('Content-Type', mime)
            self.send_header('Content-Length', str(len(payload)))
            self.end_headers()
            self.wfile.write(payload)
        except (ConnectionError, OSError):
            pass


def run(exe, out, css, record_only):
    out.mkdir(parents=True, exist_ok=True)
    assets = http.server.ThreadingHTTPServer(('127.0.0.1', 0), Handler)
    assets.asset, assets.css = True, css.read_bytes() if css else TAILWIND
    pages = http.server.ThreadingHTTPServer(('127.0.0.1', 0), Handler)
    pages.asset, pages.asset_port = False, assets.server_port
    for server in (assets, pages):
        threading.Thread(target=server.serve_forever, daemon=True).start()
    results = []
    try:
        for case, path, flags in [('dynamic', '/', []), ('static', '/', ['--no-browser']),
                                  ('negative', '/negative', []), ('many', '/many', [])]:
            p = subprocess.run([str(exe), '--target', f'http://127.0.0.1:{pages.server_port}{path}',
                                '--web-only', '--browser-timeout', '3000', *flags],
                               capture_output=True, text=True, encoding='utf-8', timeout=40)
            assert p.returncode == 0, p.stderr
            report = json.loads(p.stdout)
            (out / (case + '.json')).write_text(p.stdout, encoding='utf-8')
            (out / (case + '.stderr.txt')).write_text(p.stderr, encoding='utf-8')
            if case != 'static' and report['browser']['state'] == 'unavailable':
                return 77
            styles = [s for s in report['stylesheets'] if s['collection'] == 'browser']
            analyzed = [s for s in styles if s['analyzed']]
            matches = [f for f in report['findings'] if f['name'] == 'Tailwind CSS']
            checks = {
                'expected_match': bool(matches) == (case in ('dynamic', 'many')),
                'no_false_bootstrap': not any(f['name'] == 'Bootstrap' for f in report['findings']),
                'bounded_bodies': all(s['body_bytes'] <= 512 * 1024 for s in styles),
                'bounded_count': len(analyzed) <= 8,
                'redacted_source': all('SECRET' not in s['url'] for s in styles),
                'no_script_misclassification': not any(str(assets.server_port) in s['url'] for s in report['scripts']),
            }
            if case == 'dynamic':
                checks['browser_stylesheet_read'] = len(analyzed) == 1 and analyzed[0]['status'] == 200
                checks['exact_evidence'] = bool(matches) and all(len(f['evidence']) >= 3 and all(
                    'Browser stylesheet' in e['source'] for e in f['evidence']) for f in matches)
            elif case == 'negative':
                checks['wrong_mime_excluded'] = not any(s['url'].endswith('/wrong-mime') and s['analyzed'] for s in styles) and (
                    any(s['url'].endswith('/wrong-mime') and 'text/css' in s['reason'] for s in styles) or
                    any(r['url'].endswith('/wrong-mime') and r['state'] == 'failed' for r in report['browser']['requests']))
                checks['literal_css_analyzed'] = any(s['url'].endswith('/inert') and s['analyzed'] for s in styles)
            elif case == 'many':
                checks['eight_analyzed_two_skipped'] = len(analyzed) == 8 and sum(s['state'] == 'skipped' for s in styles) == 2
                checks['large_prefix_retained'] = any(s['url'].endswith('/large') and s['analyzed'] and s['body_truncated'] and s['body_bytes'] == 512 * 1024 for s in styles)
            results.append({'case': case, 'checks': checks, 'stylesheets': styles})
        (out / 'verification.json').write_text(json.dumps(results, indent=2), encoding='utf-8')
        failures = [r['case'] + ': ' + key for r in results for key, ok in r['checks'].items() if not ok]
        print(json.dumps({'passed': sum(ok for r in results for ok in r['checks'].values()), 'failed': failures}))
        if not record_only:
            assert not failures, failures
    finally:
        for server in (pages, assets):
            server.shutdown()
            server.server_close()


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('exe', type=Path)
    parser.add_argument('out', type=Path)
    parser.add_argument('--css', type=Path)
    parser.add_argument('--record-only', action='store_true')
    args = parser.parse_args()
    raise SystemExit(run(args.exe.resolve(), args.out.resolve(), args.css, args.record_only))
