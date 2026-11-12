import http.server
import json
from pathlib import Path
import subprocess
import sys
import threading
from PIL import Image
from integration_attribution import Handler, NAMES

exe, out = Path(sys.argv[1]).resolve(), Path(sys.argv[2]).resolve()
out.mkdir(parents=True, exist_ok=True)
server = http.server.ThreadingHTTPServer(('127.0.0.1', 0), Handler)
server.hits = []
threading.Thread(target=server.serve_forever, daemon=True).start()
results = []
try:
    for name, width, height, args in [('overview', 1280, 800, []), ('compact', 1000, 700, []),
                                    ('evidence', 1280, 800, ['--view', '3', '--vendor-filter', 'Google Identity Services'])]:
        report_file, state_file, bmp = [out / (name + suffix) for suffix in ('.json', '.state.json', '.bmp')]
        command = [str(exe), '--target', f'http://127.0.0.1:{server.server_port}/positive', '--autorun', '--no-browser',
                   '--settings-path', str(out / 'absent-settings.json'), '--screenshot', str(bmp),
                   '--report-json', str(report_file), '--ui-state', str(state_file),
                   '--width', str(width), '--height', str(height), *args]
        process = subprocess.run(command, capture_output=True, text=True, encoding='utf-8', timeout=30)
        assert process.returncode == 0, process.stderr
        report = json.loads(report_file.read_text(encoding='utf-8'))
        state = json.loads(state_file.read_text(encoding='utf-8'))
        assert NAMES <= {r['name'] for r in state['infrastructure']}, state['infrastructure']
        assert sum(r['name'] == 'CloudFront' for r in state['infrastructure']) == 1
        assert not any(r['name'] == 'Amazon CloudFront' for r in state['infrastructure'])
        assert any(f['name'] == 'Amazon CloudFront' for f in report['findings'])
        assert report['browser']['state'] == 'disabled' and not state['error']
        if name == 'evidence':
            vendor = report['vendors'][state['selected_vendor']]
            assert state['tab'] == 3 and vendor['name'] == 'Google Identity Services'
            assert vendor['evidence_level'] == 'Page reference' and 'developers.google.com/identity/' in vendor['attribution_source']
        Image.open(bmp).save(out / (name + '.png'))
        results.append({'case': name, 'passed': True, 'names': sorted(NAMES)})
    (out / 'verification.json').write_text(json.dumps(results, indent=2), encoding='utf-8')
    print('Three native integration cases passed: overview, compact and source evidence.')
finally:
    server.shutdown()
    server.server_close()
