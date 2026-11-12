import http.server
import json
from pathlib import Path
import subprocess
import sys
import threading

exe = Path(sys.argv[1]).resolve()
out = Path(sys.argv[2]).resolve(); out.mkdir(parents=True, exist_ok=True)


class Router(http.server.BaseHTTPRequestHandler):
    def log_message(self, *_): pass
    def do_GET(self):
        body = b'<title>MikroTik RouterOS</title><p>Local layout fixture</p>'
        self.send_response(200)
        self.send_header('Content-Type', 'text/html')
        self.send_header('Content-Length', str(len(body)))
        self.end_headers(); self.wfile.write(body)


def capture(name, *args, actions=None):
    bitmap, report, state = [out / (name + suffix) for suffix in ('.bmp', '.json', '.state.json')]
    command = [str(exe), '--no-browser', '--screenshot', str(bitmap), '--report-json', str(report),
               '--ui-state', str(state), *args]
    if actions:
        action_path = out / (name + '.actions.json'); action_path.write_text(json.dumps(actions))
        command += ['--ui-actions', str(action_path), '--capture-delay', '1600']
    subprocess.run(command, check=True, timeout=45)
    subprocess.run(['ffmpeg', '-y', '-i', str(bitmap), str(out / (name + '.png')), '-loglevel', 'error'], check=True)
    return json.loads(report.read_text(encoding='utf-8')), json.loads(state.read_text(encoding='utf-8'))


server = http.server.ThreadingHTTPServer(('127.0.0.1', 0), Router)
threading.Thread(target=server.serve_forever, daemon=True).start()
try:
    for name, size in [('desktop', ()), ('compact', ('--width', '1000', '--height', '700'))]:
        report, state = capture(name, '--demo', *size)
        assert report['status'] == 'demo' and state['navigation_visible']
        assert report['geolocation']['city'] == 'Example City'
        assert any(f['category'] == 'OS hint' for f in report['findings'])
        report, state = capture('router-' + name, '--target', f'http://127.0.0.1:{server.server_port}/', '--autorun', *size)
        assert report['target']['host'] == '127.0.0.1' and state['navigation_visible']
        assert report['reachability']['tcp_open'] == 1
        assert report['geolocation']['state'] == 'skipped'
        assert any(f['name'] == 'MikroTik RouterOS' and f['category'] == 'Device hint' for f in report['findings'])
    actions = [
        {'at': 350, 'kind': 'focus'},
        {'at': 800, 'kind': 'move', 'x': 100, 'y': 157},
        {'at': 850, 'kind': 'down'}, {'at': 920, 'kind': 'up'},
    ]
    report, state = capture('compact-with-navigation', '--demo', '--width', '1000', '--height', '700', actions=actions)
    assert state['show_all'] and state['tab'] == 4, state
    for name, args, x, y in [
        ('selected-os', ('--demo',), 1030, 422),
        ('selected-device-compact', ('--target', f'http://127.0.0.1:{server.server_port}/', '--autorun', '--width', '1000', '--height', '700'), 820, 435),
    ]:
        actions = [
            {'at': 350, 'kind': 'focus'},
            {'at': 1200, 'kind': 'move', 'x': x, 'y': y},
            {'at': 1250, 'kind': 'down'}, {'at': 1320, 'kind': 'up'},
        ]
        report, state = capture(name, *args, actions=actions)
        assert state['show_all'] and state['tab'] == 1 and state['selected_finding'] >= 0, state
        selected = report['findings'][state['selected_finding']]
        assert selected['category'] == ('OS hint' if name == 'selected-os' else 'Device hint'), selected
        assert selected['evidence']
    print('Seven endpoint layout cases passed: both sizes, compact navigation, and OS/device evidence selection.')
finally:
    server.shutdown(); server.server_close()
