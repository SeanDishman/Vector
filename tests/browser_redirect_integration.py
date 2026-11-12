import hashlib
import http.server
import json
from pathlib import Path
import subprocess
import sys
import threading


class Handler(http.server.BaseHTTPRequestHandler):
    def log_message(self, *_):
        pass

    def do_GET(self):
        path = self.path.split('?', 1)[0]
        self.server.hits.append(path)
        status, mime, body, location = 200, 'text/html', '', None
        if path == '/':
            body = f'''<title>Redirect fixture</title><script>
            const script = document.createElement('script');
            script.src = 'http://localhost:{external.server_port}/first?token=SECRET';
            document.head.appendChild(script);
            </script>'''
        elif path == '/first':
            status, location = 302, f'http://localhost:{external.server_port}/second?token=SECRET'
        elif path == '/second':
            status, location = 307, f'http://127.0.0.1:{page.server_port}/final.js'
        elif path == '/final.js':
            mime, body = 'application/javascript', 'document.documentElement.dataset.fixtureComplete="yes";'
        else:
            status = 404
        data = body.encode()
        self.send_response(status)
        self.send_header('Content-Type', mime)
        self.send_header('Content-Length', str(len(data)))
        if location:
            self.send_header('Location', location)
        self.end_headers()
        try:
            self.wfile.write(data)
        except (BrokenPipeError, ConnectionResetError, ConnectionAbortedError):
            pass


exe, out = Path(sys.argv[1]).resolve(), Path(sys.argv[2]).resolve()
out.mkdir(parents=True, exist_ok=True)
page = http.server.ThreadingHTTPServer(('127.0.0.1', 0), Handler)
external = http.server.ThreadingHTTPServer(('127.0.0.1', 0), Handler)
for server in (page, external):
    server.hits = []
    threading.Thread(target=server.serve_forever, daemon=True).start()
try:
    command = [str(exe), '--target', f'http://127.0.0.1:{page.server_port}/', '--web-only',
               '--timeout', '150', '--http-timeout', '300']
    process = subprocess.run(command, capture_output=True, text=True, encoding='utf-8', timeout=35)
    (out / 'report.json').write_text(process.stdout, encoding='utf-8')
    (out / 'stderr.txt').write_text(process.stderr, encoding='utf-8')
    assert process.returncode == 0, process.stderr
    report = json.loads(process.stdout)
    if report['browser']['state'] == 'unavailable':
        print('Browser unavailable:', report['browser']['reason'])
        sys.exit(77)
    requests = report['browser']['requests']
    redirects = [q for q in requests if q['state'] == 'redirect']
    rows = [v for v in report['vendors'] if v['domain'] == 'localhost'
            and v['evidence_level'] == 'Observed response']
    evidence = [e['value'] for row in rows for e in row['evidence']]
    checks = {
        'actual_redirect_chain': all(path in external.hits for path in ('/first', '/second')),
        'both_response_statuses_retained': {q['status'] for q in redirects} == {302, 307},
        'first_redirect_attributed': any('/first' in e and 'HTTP 302' in e for e in evidence),
        'second_redirect_attributed': any('/second' in e and 'HTTP 307' in e for e in evidence),
        'redirects_are_not_script_bodies': bool(rows) and all('received this script' not in row['potential_access'] for row in rows),
        'redirects_not_analyzed_as_scripts': not any(s['analyzed'] and 'localhost:' in s['url'] for s in report['scripts']),
        'final_script_collected': any(q['url'].endswith('/final.js') and q['status'] == 200 for q in requests),
        'query_values_not_exported': 'SECRET' not in process.stdout,
        'no_account_access_claim': bool(rows) and all(row['access_status'] == 'unverified' for row in rows),
    }
    result = {'binary_sha256': hashlib.sha256(exe.read_bytes()).hexdigest(), 'checks': checks}
    (out / 'verification.json').write_text(json.dumps(result, indent=2), encoding='utf-8')
    assert all(checks.values()), result
    print(f'{len(checks)} actual browser redirect checks passed.')
finally:
    for server in (page, external):
        server.shutdown()
        server.server_close()
