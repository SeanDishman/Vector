import argparse
import http.server
import json
from pathlib import Path
import subprocess
import threading

SCRIPTS = 320
BANNER = b'/*! jQuery v3.98.7 | (c) OpenJS Foundation and other contributors | jquery.org/license */\n'
PAGE = ('<!doctype html><title>Request budget fixture</title><body><script>'
        'for(let i=0;i<' + str(SCRIPTS) + ';i++){const s=document.createElement("script");s.src="/part-"+i+".js";'
        's.async=false;document.head.appendChild(s);}</script></body>').encode()


class Handler(http.server.BaseHTTPRequestHandler):
    def log_message(self, *_):
        pass

    def do_GET(self):
        self.server.hits.append(self.path)
        status, mime = 200, 'text/html'
        if self.path == '/':
            body = PAGE
        elif self.path.startswith('/part-') and self.path.endswith('.js'):
            number = self.path[6:-3]
            mime = 'application/javascript'
            body = (BANNER if number == '3' else b'') + ('globalThis.partCount=(globalThis.partCount||0)+1;/* ' + number + ' */').encode()
        else:
            status, body = 404, b''
        try:
            self.send_response(status)
            self.send_header('Content-Type', mime)
            self.send_header('Content-Length', str(len(body)))
            self.end_headers()
            self.wfile.write(body)
        except (ConnectionError, OSError):
            pass


def run(exe, out, record_only):
    out.mkdir(parents=True, exist_ok=True)
    server = http.server.ThreadingHTTPServer(('127.0.0.1', 0), Handler)
    server.hits = []
    threading.Thread(target=server.serve_forever, daemon=True).start()
    try:
        process = subprocess.run([str(exe), '--target', f'http://127.0.0.1:{server.server_port}/',
                                  '--web-only', '--browser-timeout', '8000'],
                                 capture_output=True, text=True, encoding='utf-8', timeout=60)
        assert process.returncode == 0, process.stderr
        report = json.loads(process.stdout)
        (out / 'report.json').write_text(process.stdout, encoding='utf-8')
        if report['browser']['state'] == 'unavailable':
            print('Installed browser unavailable')
            return 77
        page = report['browser']['pages'][0]
        requests = report['browser']['requests']
        parts = [s for s in report['scripts'] if s['collection'] == 'browser' and '/part-' in s['url']]
        analyzed = [s for s in parts if s['analyzed']]
        banner = [f for f in report['findings'] if f['name'] == 'jQuery' and f['version'] == '3.98.7'
                  and any('Browser script' in e['source'] for e in f['evidence'])]
        served = sum(1 for p in server.hits if p.startswith('/part-'))
        checks = {
            'budget_reported': page['state'] == 'limited' and 'request or transfer limit' in page['reason'],
            'request_rows_capped': len(requests) <= 256 and report['browser']['requests_truncated'],
            'excess_never_served': served <= 256,
            'admitted_requests_finished': sum(1 for r in requests if r['state'] == 'complete') >= 200,
            'admitted_scripts_analyzed': len(analyzed) >= 150,
            'few_scripts_cut_off_at_the_deadline': sum('contiguous resource prefix' in s['reason'] for s in parts) <= 40,
            'early_script_named': bool(banner),
            'analysis_cap_kept': len(analyzed) <= 160 and page['scripts_analyzed'] <= 160,
            'shared_deadline_preserved': report['web_coverage']['time_limit_ms'] == 30000,
        }
        receipt = {'checks': checks, 'served_parts': served, 'request_rows': len(requests), 'analyzed': len(analyzed),
                   'states': sorted({r['state'] for r in requests}), 'page': page}
        (out / 'verification.json').write_text(json.dumps(receipt, indent=2), encoding='utf-8')
        print(json.dumps({'checks': checks, 'served_parts': served, 'analyzed': len(analyzed)}))
        if not record_only:
            assert all(checks.values()), checks
    finally:
        server.shutdown()
        server.server_close()


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('exe', type=Path)
    parser.add_argument('out', type=Path)
    parser.add_argument('--record-only', action='store_true')
    args = parser.parse_args()
    raise SystemExit(run(args.exe.resolve(), args.out.resolve(), args.record_only))
