import http.server
import json
from pathlib import Path
import subprocess
import sys
import threading

exe = Path(sys.argv[1]).resolve()
out = Path(sys.argv[2]).resolve()
out.mkdir(parents=True, exist_ok=True)
hidden = getattr(subprocess, 'CREATE_NO_WINDOW', 0)


class Site(http.server.BaseHTTPRequestHandler):
    server_version = 'nginx/1.24.0'
    sys_version = ''

    def log_message(self, *_): pass

    def do_GET(self):
        if self.path != '/':
            self.send_error(404)
            return
        body = b'<title>Icon fixture</title><form method="post"><input type="text" name="link" placeholder="youtube.com/watch?v=" required></form>'
        self.send_response(200)
        self.send_header('Content-Type', 'text/html')
        self.send_header('Content-Length', str(len(body)))
        self.end_headers()
        self.wfile.write(body)


def capture(name, width=1280, height=800, args=(), click=None):
    base = out / name
    command = [str(exe), '--no-browser', '--screenshot', str(base.with_suffix('.bmp')),
               '--width', str(width), '--height', str(height), '--ui-state', str(base.with_suffix('.state.json')),
               '--report-json', str(base.with_suffix('.json')), *args]
    if click:
        x, y = click
        actions = [{'at': 300, 'kind': 'focus'}, {'at': 1700, 'kind': 'move', 'x': x, 'y': y},
                   {'at': 1800, 'kind': 'down'}, {'at': 1880, 'kind': 'up'}]
        base.with_suffix('.actions.json').write_text(json.dumps(actions), encoding='utf-8')
        command += ['--ui-actions', str(base.with_suffix('.actions.json')), '--capture-delay', '2600']
    subprocess.run(command, check=True, timeout=35, creationflags=hidden)
    subprocess.run(['ffmpeg', '-y', '-i', str(base.with_suffix('.bmp')), str(base.with_suffix('.png')),
                    '-loglevel', 'error'], check=True, creationflags=hidden)
    state = json.loads(base.with_suffix('.state.json').read_text(encoding='utf-8'))
    assert state['window_flags'] & 8, 'Capture must remain hidden'
    report = json.loads(base.with_suffix('.json').read_text(encoding='utf-8'))
    return state, report


server = http.server.ThreadingHTTPServer(('127.0.0.1', 0), Site)
threading.Thread(target=server.serve_forever, daemon=True).start()
site = ('--target', f'http://127.0.0.1:{server.server_port}/', '--autorun')
try:
    for suffix, width, height in [('desktop', 1280, 800), ('compact', 1000, 700)]:
        state, _ = capture('overview-' + suffix, width, height, args=site)
        assert [e['name'] for e in state['infrastructure']] == ['nginx', 'YouTube']
        state, _ = capture('software-' + suffix, width, height, args=('--demo', '--view', '1'))
        assert state['tab'] == 1 and state['selected_finding'] >= 0
        state, report = capture('vendors-' + suffix, width, height, args=(*site, '--view', '3'))
        assert report['vendors'][state['selected_vendor']]['name'] == 'YouTube'
    state, report = capture('overview-icon-click', args=site, click=(72, 319))
    assert state['tab'] == 1 and report['findings'][state['selected_finding']]['name'] == 'nginx', state
    state, report = capture('table-icon-click', args=('--demo', '--view', '1'), click=(256, 604))
    assert state['tab'] == 1 and report['findings'][state['selected_finding']]['name'] == 'PHP', state
    print('PASS: 8 hidden UI cases; both sizes, software/vendor details and logo clicks preserve evidence selection.')
finally:
    server.shutdown()
    server.server_close()
