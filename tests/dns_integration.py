import http.server
import json
import subprocess
import sys
import threading
import time
import uuid
from pathlib import Path
from urllib.parse import parse_qs, urlsplit

exe = Path(sys.argv[1]).resolve()
out = Path(sys.argv[2] if len(sys.argv) > 2 else 'docs/reports/dns-infrastructure/network')
out.mkdir(parents=True, exist_ok=True)
requests = []
lock = threading.Lock()


class Handler(http.server.BaseHTTPRequestHandler):
    def log_message(self, *_):
        pass

    def do_GET(self):
        parsed = urlsplit(self.path)
        query = parse_qs(parsed.query)
        name, kind = query['name'][0], query['type'][0]
        with lock:
            requests.append((parsed.path, name, kind, query.get('edns_client_subnet')))
        code = {'CNAME': 5, 'MX': 15, 'NS': 2, 'TXT': 16}[kind]
        value = {'CNAME': 'd123.cloudfront.net.', 'MX': '0 fixture.mail.protection.outlook.com.',
                 'NS': 'ada.ns.cloudflare.com.', 'TXT': '"v=spf1 include:_spf.google.com -all"'}[kind]
        data = {'Status': 0, 'Question': [{'name': name + '.', 'type': code}],
                'Answer': [{'name': name + '.', 'type': code, 'TTL': 600, 'data': value}]}
        status, body = 200, None
        if parsed.path == '/unknown':
            data['Answer'][0]['data'] = '"verification-only=abc"' if kind == 'TXT' else (
                '10 mail.unfamiliar.example.' if kind == 'MX' else 'unfamiliar.example.')
        elif parsed.path == '/denied':
            status = 403
        elif parsed.path == '/redirect':
            status = 302
        elif parsed.path == '/mismatch':
            data['Question'][0]['name'] = 'unrelated.example.'
        elif parsed.path == '/empty':
            data.pop('Answer')
        elif parsed.path == '/nxdomain':
            data['Status'] = 3
        elif parsed.path == '/malformed':
            body = b'not json'
        elif parsed.path == '/oversized':
            body = b'x' * 65537
        elif parsed.path == '/slow' or (parsed.path == '/partial' and kind == 'TXT'):
            time.sleep(1.2)
        if body is None:
            body = json.dumps(data).encode()
        try:
            self.send_response(status)
            self.send_header('Content-Type', 'application/json')
            self.send_header('Content-Length', str(len(body)))
            if status == 302:
                self.send_header('Location', '/must-not-follow')
            self.end_headers()
            self.wfile.write(body)
        except (BrokenPipeError, ConnectionResetError, ConnectionAbortedError):
            pass


server = http.server.ThreadingHTTPServer(('127.0.0.1', 0), Handler)
threading.Thread(target=server.serve_forever, daemon=True).start()
host = 'www.' + uuid.uuid4().hex + '.example.com'


def run(mode, cancel_ms=0, timeout_ms=3000):
    with lock:
        requests.clear()
    start = time.monotonic()
    process = subprocess.run([str(exe), '--fixture', f'http://127.0.0.1:{server.server_port}/{mode}',
                              host, str(cancel_ms), str(timeout_ms)], capture_output=True, text=True, timeout=8)
    elapsed = time.monotonic() - start
    assert process.returncode == 0, process.stderr
    report = json.loads(process.stdout)
    (out / f'{mode}.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
    return report, elapsed


try:
    r, elapsed = run('normal')
    assert r['dns']['state'] == 'complete' and len(r['dns']['queries']) == 4
    names = {f['name'] for f in r['findings']}
    assert names == {'Amazon CloudFront', 'Microsoft 365 mail', 'Cloudflare DNS', 'Google mail authorization'}, names
    assert r['services'] == [] and r['reachability']['state'] == 'not_checked'
    assert len(requests) == 4 and all(entry[3] == ['0.0.0.0/0'] for entry in requests)
    for q in r['dns']['queries']:
        assert q['name'] == (host if q['type'] == 'CNAME' else host.removeprefix('www.'))
        assert q['records'][0]['ttl'] == 600 and q['checked_at'] and q['source'].startswith('http://127.0.0.1:')
    r, _ = run('unknown')
    assert not r['findings'] and sum(len(q['records']) for q in r['dns']['queries']) == 4
    for mode in ('denied', 'redirect', 'mismatch', 'malformed', 'oversized'):
        r, _ = run(mode)
        assert not r['findings'] and r['dns']['state'] == 'unavailable', mode
        assert len(requests) == 4 and not any(p == '/must-not-follow' for p, *_ in requests)
    for mode, state in (('empty', 'no_records'), ('nxdomain', 'nxdomain')):
        r, _ = run(mode)
        assert not r['findings'] and all(q['state'] == state for q in r['dns']['queries'])
    r, elapsed = run('partial', timeout_ms=300)
    assert r['dns']['state'] == 'partial' and len(r['findings']) == 3
    assert next(q for q in r['dns']['queries'] if q['type'] == 'TXT')['state'] == 'timeout'
    assert 250 <= r['dns']['elapsed_ms'] < 900 and elapsed < 2
    r, elapsed = run('slow', cancel_ms=100)
    assert r['dns']['state'] == 'cancelled' and not r['findings']
    assert r['dns']['elapsed_ms'] < 700 and elapsed < 2
    assert len(requests) == 4, 'Cancellation test must interrupt active requests'
    (out / 'verification.json').write_text(json.dumps({'binary': str(exe), 'fresh_hostname': host,
        'scenarios': 11, 'public_requests': 0, 'cancel_elapsed_ms': r['dns']['elapsed_ms']}, indent=2))
    print('11 DNS network scenarios passed: records, unknown providers, errors, scope, bounds and active cancellation.')
finally:
    server.shutdown()
    server.server_close()
