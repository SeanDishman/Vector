import json
from pathlib import Path
import subprocess
import sys
import tempfile
import time
from captured_document_integration import start

exe = Path(sys.argv[1]).resolve()
out = Path(sys.argv[2]).resolve(); out.mkdir(parents=True, exist_ok=True)
server = start()
try:
    for name, path, args in (
        ('overview', '/captured', ()),
        ('evidence', '/captured', ('--view', '2')),
        ('compact-evidence', '/captured', ('--view', '2', '--width', '1000', '--height', '700')),
        ('cancelled-fallback', '/cancel', ('--view', '2', '--cancel-after', '9000')),
    ):
        bitmap, report = out / (name + '.bmp'), out / (name + '.json')
        before = set(Path(tempfile.gettempdir()).glob('vector-browser-*'))
        began = time.monotonic()
        process = subprocess.Popen([str(exe), '--target', f'http://127.0.0.1:{server.server_port}{path}', '--autorun',
                                    '--screenshot', str(bitmap), '--report-json', str(report), *args])
        assert process.wait(timeout=40) == 0
        elapsed = time.monotonic() - began
        r = json.loads(report.read_text())
        assert any(p['document_source'] == 'captured_http' for p in r['browser']['pages']), r['browser']
        if name == 'cancelled-fallback':
            assert r['status'] == 'cancelled' and r['browser']['state'] == 'cancelled', r['browser']
            assert r['browser']['pages'][-1]['document_source'] == 'captured_http' and r['browser']['pages'][-1]['state'] == 'cancelled'
            assert any(q['url'].endswith('/pending') for q in r['browser']['requests']), r['browser']
            assert elapsed < 14, elapsed
        else:
            assert {'React', 'TanStack Router'} <= {f['name'] for f in r['findings']}, r['findings']
            assert r['browser']['pages'][0]['status'] == 403 and r['browser']['pages'][0]['state'] == 'http_error'
            assert r['browser']['pages'][-1]['scripts_analyzed'] >= 4, r['browser']
        assert not [p for p in Path(tempfile.gettempdir()).glob(f'vector-browser-{process.pid}-*') if p not in before]
        subprocess.run(['ffmpeg', '-y', '-i', str(bitmap), str(out / (name + '.png')), '-loglevel', 'error'], check=True)
    print('Four SDL scenarios passed: overview, evidence at two sizes, and cancellation during captured-document execution.')
finally:
    server.shutdown()
