import argparse
import http.server
import json
from pathlib import Path
import subprocess
import threading
import time

COUNT = 12
CHUNK = (b'void 0;\n' * (768 * 1024 // 8))
PAGE = b'''<title>Browser message pressure</title><body><script>
let completed=0;
for(let i=0;i<12;i++){
  const script=document.createElement('script');script.src='/chunk-'+i;
  script.onload=()=>{if(++completed===12){
    Object.defineProperty(globalThis,['lit','Html','Versions'].join(''),{value:['3.3.1']});
    fetch('/proof');
  }};
  document.head.appendChild(script);
}
</script></body>'''


class Handler(http.server.BaseHTTPRequestHandler):
    def log_message(self, *_):
        pass

    def do_GET(self):
        self.server.hits.append((self.path, time.monotonic()))
        status, mime = 200, 'text/html'
        if self.path == '/':
            body = PAGE
        elif self.path.startswith('/chunk-'):
            mime, body = 'application/javascript', CHUNK
        elif self.path == '/proof':
            mime, body = 'text/plain', b'All scripts executed'
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
    began = time.monotonic()
    try:
        process = subprocess.run([str(exe), '--target', f'http://127.0.0.1:{server.server_port}/',
                                  '--web-only', '--browser-timeout', '8000'],
                                 capture_output=True, text=True, encoding='utf-8', timeout=50)
        assert process.returncode == 0, process.stderr
        report = json.loads(process.stdout)
        (out / 'report.json').write_text(process.stdout, encoding='utf-8')
        (out / 'stderr.txt').write_text(process.stderr, encoding='utf-8')
        if report['browser']['state'] == 'unavailable':
            print('Installed browser unavailable')
            return 77
        scripts = [s for s in report['scripts'] if s['collection'] == 'browser' and '/chunk-' in s['url']]
        runtime = [f for f in report['findings'] if f['name'] == 'lit-html' and f['version'] == '3.3.1'
                   and any('Browser runtime' in e['source'] for e in f['evidence'])]
        checks = {
            'all_scripts_analyzed': len(scripts) == COUNT and all(s['analyzed'] for s in scripts),
            'all_scripts_executed': any(p == '/proof' for p, _ in server.hits),
            'runtime_only_evidence': bool(runtime),
            'runtime_response_received': all('Runtime.evaluate timed out' not in p['reason'] for p in report['browser']['pages']),
            'request_limit_preserved': len(report['browser']['requests']) <= 256,
            'shared_deadline_preserved': report['web_coverage']['time_limit_ms'] == 30000,
        }
        receipt = {'checks': checks, 'seconds': round(time.monotonic() - began, 3),
                   'analyzed_scripts': sum(s['analyzed'] for s in scripts),
                   'hits': [{'path': p, 'seconds': round(t - began, 3)} for p, t in server.hits],
                   'pages': report['browser']['pages']}
        (out / 'verification.json').write_text(json.dumps(receipt, indent=2), encoding='utf-8')
        print(json.dumps({'checks': checks, 'seconds': receipt['seconds']}))
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
