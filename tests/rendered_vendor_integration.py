import argparse
import http.server
import json
from pathlib import Path
import subprocess
import threading
from urllib.parse import urlsplit

CSP = "default-src 'none'; script-src 'self' 'unsafe-inline'; frame-src 'none'; connect-src 'self'"
SCRIPT = '''
const stripe=['https://js.','stripe.com','/v3/?visitor=SECRET'].join('');
const youtube=['https://www.','youtube.com','/embed/example?visitor=SECRET'].join('');
const codeOnly='https://widget.intercom.io/widget/example';
const inert=document.createElement('template');
inert.innerHTML='<script src="'+stripe+'"><'+'/script><iframe src="'+youtube+'"></iframe>';
document.body.appendChild(inert);
if(location.pathname=='/padded'){
  const padding=document.createElement('div');padding.textContent='x'.repeat(600*1024);document.body.appendChild(padding);
}
if(location.pathname!='/inert'){
  const script=document.createElement('script');script.src=stripe;document.body.appendChild(script);
  const frame=document.createElement('iframe');frame.src=youtube;document.body.appendChild(frame);
}
fetch('/proof');
'''
PAGE = '<!doctype html><title>Dynamic reference fixture</title><body><script>' + SCRIPT + '</script></body>'


class Handler(http.server.BaseHTTPRequestHandler):
    def log_message(self, *_):
        pass

    def do_GET(self):
        self.server.hits.append(self.path)
        status, body, mime = 200, '', 'text/html'
        if self.path in ('/', '/inert', '/privacy', '/padded'):
            body = PAGE
        elif self.path == '/source':
            body = '<title>Endpoint without integrations</title><a href="/privacy">Privacy policy</a>'
        elif self.path == '/proof':
            body, mime = 'DOM updated', 'text/plain'
        else:
            status = 404
        payload = body.encode()
        try:
            self.send_response(status)
            self.send_header('Content-Type', mime)
            self.send_header('Content-Security-Policy', CSP)
            self.send_header('Content-Length', str(len(payload)))
            self.end_headers()
            self.wfile.write(payload)
        except (ConnectionError, OSError):
            pass


def eligible(vendor):
    return vendor['evidence_level'] == 'Page reference' and vendor['relationship'] in (
        'Script', 'Embedded frame', 'Stylesheet', 'Image / media', 'Form destination')


def run(exe, out, record_only):
    out.mkdir(parents=True, exist_ok=True)
    server = http.server.ThreadingHTTPServer(('127.0.0.1', 0), Handler)
    server.hits = []
    threading.Thread(target=server.serve_forever, daemon=True).start()
    results = []
    try:
        for case, path, flags in [('dynamic', '/', []), ('padded', '/padded', []), ('static', '/', ['--no-browser']),
                                  ('inert', '/inert', []), ('disclosure', '/source', [])]:
            server.hits.clear()
            command = [str(exe), '--target', f'http://127.0.0.1:{server.server_port}{path}',
                       '--web-only', '--browser-timeout', '1800', *flags]
            process = subprocess.run(command, capture_output=True, text=True, encoding='utf-8', timeout=40)
            assert process.returncode == 0, process.stderr
            report = json.loads(process.stdout)
            (out / (case + '.json')).write_text(process.stdout, encoding='utf-8')
            (out / (case + '.stderr.txt')).write_text(process.stderr, encoding='utf-8')
            if case != 'static' and report['browser']['state'] == 'unavailable':
                return 77
            references = [v for v in report['vendors'] if eligible(v)]
            names = {v['name'] for v in references}
            external = [r for r in report['browser']['requests'] if urlsplit(r['url']).hostname != '127.0.0.1']
            checks = {
                'exact_reference_names': names == ({'Stripe', 'YouTube'} if case in ('dynamic', 'padded') else set()),
                'no_successful_external_contact': not any(r['status'] > 0 for r in external),
                'no_observed_provider_claim': not any(v['evidence_level'] == 'Observed response' for v in report['vendors']),
                'query_redacted': all('SECRET' not in e['value'] and 'SECRET' not in e['source']
                                      for v in report['vendors'] for e in v['evidence']),
                'inert_code_excluded': not any(v['name'] == 'Intercom' for v in references),
            }
            if case in ('dynamic', 'padded'):
                checks['rendered_provenance'] = len(references) == 2 and all(
                    e['source'].startswith('Rendered DOM / ') for v in references for e in v['evidence'])
                checks['page_executed'] = '/proof' in server.hits
                checks['blocked_script_retained'] = any(urlsplit(r['url']).hostname == 'js.stripe.com'
                                                        and r['state'] == 'failed' for r in external)
            if case == 'padded':
                checks['rendered_document_not_truncated'] = all(not p['dom_truncated'] for p in report['browser']['pages'])
            if case == 'disclosure':
                checks['disclosure_was_rendered'] = any(p['purpose'] == 'disclosure' and p['javascript_enabled']
                                                        for p in report['browser']['pages'])
            results.append({'case': case, 'checks': checks, 'names': sorted(names), 'hits': list(server.hits)})
        (out / 'verification.json').write_text(json.dumps(results, indent=2), encoding='utf-8')
        failures = [r['case'] + ': ' + k for r in results for k, v in r['checks'].items() if not v]
        print(json.dumps({'passed': sum(v for r in results for v in r['checks'].values()), 'failed': failures}))
        if not record_only:
            assert not failures, failures
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
