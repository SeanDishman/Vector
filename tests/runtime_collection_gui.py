import http.server
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import threading
import time
from PIL import Image
from browser_stylesheet_integration import Handler, TAILWIND


class Assets(Handler):
    def do_GET(self):
        if not self.server.slow:
            return super().do_GET()
        payload = TAILWIND + b'/* slow */\n' * 100000
        try:
            self.send_response(200)
            self.send_header('Content-Type', 'text/css')
            self.send_header('Content-Length', str(len(payload)))
            self.end_headers()
            for offset in range(0, len(payload), 8192):
                self.wfile.write(payload[offset:offset + 8192])
                self.wfile.flush()
                time.sleep(.12)
        except (ConnectionError, OSError):
            pass


exe, out = Path(sys.argv[1]).resolve(), Path(sys.argv[2]).resolve()
out.mkdir(parents=True, exist_ok=True)
assets = http.server.ThreadingHTTPServer(('127.0.0.1', 0), Assets)
assets.asset, assets.css, assets.slow = True, TAILWIND, False
if len(sys.argv) > 3:
    assets.css = Path(sys.argv[3]).read_bytes()
pages = http.server.ThreadingHTTPServer(('127.0.0.1', 0), Handler)
pages.asset, pages.asset_port = False, assets.server_port
for server in (assets, pages):
    threading.Thread(target=server.serve_forever, daemon=True).start()
results = []
try:
    for name, width, height, flags in [
        ('overview', 1280, 800, []), ('compact', 1000, 700, []),
        ('evidence', 1280, 800, ['--filter', 'Tailwind CSS']),
        ('cancelled', 1280, 800, ['--cancel-after', '5000']),
    ]:
        assets.slow = name == 'cancelled'
        bmp, report_path, state_path = [out / (name + suffix) for suffix in ('.bmp', '.json', '.state.json')]
        command = [str(exe), '--target', f'http://127.0.0.1:{pages.server_port}/', '--autorun',
                   '--screenshot', str(bmp), '--report-json', str(report_path), '--ui-state', str(state_path),
                   '--settings-path', str(out / 'absent-proxy-settings.json'),
                   '--width', str(width), '--height', str(height), *flags]
        process = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, encoding='utf-8')
        _, stderr = process.communicate(timeout=60)
        assert process.returncode == 0, stderr
        (out / (name + '.stderr.txt')).write_text(stderr, encoding='utf-8')
        report = json.loads(report_path.read_text(encoding='utf-8'))
        state = json.loads(state_path.read_text(encoding='utf-8'))
        if name == 'cancelled':
            assert report['status'] == 'cancelled' and report['browser']['state'] == 'cancelled', report['browser']
            assert any(s['collection'] == 'browser' and s['state'] == 'cancelled' for s in report['stylesheets']), report['stylesheets']
        else:
            assert 'Tailwind CSS' in {f['name'] for f in state['infrastructure']}
            assert any(s['collection'] == 'browser' and s['analyzed'] for s in report['stylesheets'])
            assert state['navigation_visible'] and not state['error']
            if name == 'evidence':
                finding = report['findings'][state['selected_finding']]
                assert state['tab'] == 1 and finding['name'] == 'Tailwind CSS'
                assert len(finding['evidence']) >= 3 and all('Browser stylesheet' in e['source'] for e in finding['evidence'])
        assert not list(Path(tempfile.gettempdir()).glob(f'vector-browser-{process.pid}-*')), stderr
        Image.open(bmp).save(out / (name + '.png'))
        results.append({'case': name, 'status': report['status'], 'passed': True})
    (out / 'verification.json').write_text(json.dumps(results, indent=2), encoding='utf-8')
    print('Four native runtime collection cases passed: overview, compact, exact CSS evidence and active CSS cancellation.')
finally:
    for server in (pages, assets):
        server.shutdown()
        server.server_close()
