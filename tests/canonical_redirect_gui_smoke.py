import json
from pathlib import Path
import subprocess
import sys
import tempfile
from canonical_redirect_integration import start

exe = Path(sys.argv[1]).resolve()
out = Path(sys.argv[2]).resolve(); out.mkdir(parents=True, exist_ok=True)
server = start()
try:
    for name, args in [('overview', ()), ('evidence', ('--view', '2'))]:
        report = out / (name + '.json')
        bitmap = out / (name + '.bmp')
        process = subprocess.Popen([str(exe), '--target', f'http://localhost:{server.server_port}/', '--autorun',
                                    '--screenshot', str(bitmap), '--report-json', str(report), *args])
        assert process.wait(timeout=40) == 0
        assert not list(Path(tempfile.gettempdir()).glob(f'vector-browser-{process.pid}-*'))
        r = json.loads(report.read_text(encoding='utf-8'))
        assert r['script_coverage']['javascript_executed']
        assert any(f['name'] == 'React' for f in r['findings'])
        assert any(p['url'].startswith('http://www.localhost:') and p['status'] == 200 for p in r['web'])
        assert r['target']['host'] == 'localhost' and r['address'] == '127.0.0.1'
        subprocess.run(['ffmpeg', '-y', '-i', str(bitmap), str(out / (name + '.png')), '-loglevel', 'error'], check=True)
    print('Two native SDL cases passed: redirected runtime findings and full evidence.')
finally:
    server.shutdown(); server.server_close()
