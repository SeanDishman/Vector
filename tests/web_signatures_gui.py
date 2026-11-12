import json
from pathlib import Path
import subprocess
import sys
from PIL import Image
from web_signatures_integration import start, LIBRARIES

exe, out = Path(sys.argv[1]).resolve(), Path(sys.argv[2]).resolve()
out.mkdir(parents=True, exist_ok=True)
server = start()
try:
    for name, view, width in [('overview', 4, 1280), ('compact', 4, 1000), ('software', 1, 1280)]:
        bmp, report, state = [out / (name + suffix) for suffix in ('.bmp', '.json', '.state.json')]
        command = [str(exe), '--target', f'http://127.0.0.1:{server.server_port}/runtime-only', '--autorun',
                   '--screenshot', str(bmp), '--report-json', str(report), '--ui-state', str(state), '--view', str(view),
                   '--width', str(width), '--height', '800' if width == 1280 else '700']
        if view == 1:
            command += ['--filter', 'Leaflet']
        subprocess.run(command, check=True, timeout=45)
        data = json.loads(report.read_text())
        ui = json.loads(state.read_text())
        assert set(LIBRARIES) <= {f['name'] for f in data['findings']}
        assert any('rule leaflet-api @ ' in e['source'] for f in data['findings'] for e in f['evidence'])
        assert ui['navigation_visible'] and not ui['error']
        if view == 1:
            assert data['findings'][ui['selected_finding']]['name'] == 'Leaflet'
        Image.open(bmp).save(out / (name + '.png'))
    print('Three native web-signature scenarios passed: overview, compact and software evidence.')
finally:
    server.shutdown()
