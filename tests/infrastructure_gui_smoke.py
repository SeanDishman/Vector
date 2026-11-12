import http.server
import json
from pathlib import Path
import subprocess
import sys
import threading

exe = Path(sys.argv[1]).resolve()
out = Path(sys.argv[2]).resolve()
out.mkdir(parents=True, exist_ok=True)


class Site(http.server.BaseHTTPRequestHandler):
    server_version = 'nginx/1.24.0 (Ubuntu)'
    sys_version = ''

    def log_message(self, *_): pass

    def do_GET(self):
        if self.path != '/':
            self.send_error(404)
            return
        body = b'<title>Local converter fixture</title><form method="post"><input type="text" name="link" placeholder="youtube.com/watch?v=" required></form>'
        self.send_response(200)
        self.send_header('Content-Type', 'text/html')
        self.send_header('Content-Length', str(len(body)))
        self.end_headers()
        self.wfile.write(body)


server = http.server.ThreadingHTTPServer(('127.0.0.1', 0), Site)
threading.Thread(target=server.serve_forever, daemon=True).start()


def capture(name, width, height, actions=None):
    base = out / name
    command = [str(exe), '--target', f'http://127.0.0.1:{server.server_port}/', '--autorun', '--no-browser',
               '--width', str(width), '--height', str(height), '--screenshot', str(base.with_suffix('.bmp')),
               '--report-json', str(base.with_suffix('.json')), '--ui-state', str(base.with_suffix('.state.json'))]
    if actions:
        script = base.with_suffix('.actions.json')
        script.write_text(json.dumps(actions), encoding='utf-8')
        command += ['--ui-actions', str(script), '--capture-delay', '2400']
    subprocess.run(command, check=True, timeout=30, creationflags=getattr(subprocess, 'CREATE_NO_WINDOW', 0))
    subprocess.run(['ffmpeg', '-y', '-i', str(base.with_suffix('.bmp')), str(base.with_suffix('.png')), '-loglevel', 'error'],
                   check=True, creationflags=getattr(subprocess, 'CREATE_NO_WINDOW', 0))
    state = json.loads(base.with_suffix('.state.json').read_text(encoding='utf-8'))
    report = json.loads(base.with_suffix('.json').read_text(encoding='utf-8'))
    assert state['window_flags'] & 8, 'SDL window must stay hidden'
    assert [entry['name'] for entry in state['infrastructure']] == ['nginx', 'YouTube'], state
    assert any(f['category'] == 'OS hint' for f in report['findings'])
    assert report['services'], 'technical information must remain available'
    return state, report


try:
    if '--interactions-only' not in sys.argv:
        for name, width, height in [('desktop', 1280, 800), ('compact', 1000, 700)]:
            state, _ = capture(name, width, height)
            assert state['navigation_visible']
        print('PASS hidden desktop and compact overviews show nginx and YouTube; technical info remains in the report.')
    for name, width, height, y, tab, selection in [
        ('youtube-desktop', 1280, 800, 370, 3, 'selected_vendor'),
        ('youtube-compact', 1000, 700, 370, 3, 'selected_vendor'),
        ('nginx-desktop', 1280, 800, 315, 1, 'selected_finding'),
        ('info-compact', 1000, 700, 500, 0, None),
    ]:
        actions = [
            {'at': 1300, 'kind': 'focus'}, {'at': 1350, 'kind': 'move', 'x': 356, 'y': y},
            {'at': 1400, 'kind': 'down'}, {'at': 1480, 'kind': 'up'},
        ]
        state, report = capture(name, width, height, actions)
        assert state['show_all'] and state['tab'] == tab, state
        if selection:
            assert state[selection] == 0, state
        if tab == 3:
            assert report['vendors'][state['selected_vendor']]['name'] == 'YouTube'
            assert report['vendors'][state['selected_vendor']]['evidence_level'] == 'Form input'
        print('PASS', name, 'opens the correct evidence view and stays hidden.')
finally:
    server.shutdown()
    server.server_close()
