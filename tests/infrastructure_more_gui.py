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

REFERENCES = [
    'https://js.stripe.com/v3/',
    'https://www.googletagmanager.com/gtm.js?id=GTM-FIXTURE',
    'https://js.hcaptcha.com/1/api.js',
    'https://www.google.com/recaptcha/api.js',
    'https://snap.licdn.com/li.lms-analytics/insight.min.js',
    'https://plausible.io/js/script.js',
    'https://cdn.cookielaw.org/consent/fixture/OtAutoBlock.js',
    'https://cdn.usefathom.com/script.js',
    'https://connect.facebook.net/en_US/fbevents.js',
    'https://static.hotjar.com/c/hotjar-1.js',
    'https://widget.intercom.io/widget/fixture',
    'https://js.sentry-cdn.com/fixture.min.js',
]
PAGE = ('<!doctype html><title>Load more fixture</title>' +
        ''.join(f'<script src="{url}"></script>' for url in REFERENCES)).encode()


class Site(http.server.BaseHTTPRequestHandler):
    server_version = 'nginx/1.24.0'
    sys_version = ''

    def log_message(self, *_): pass

    def do_GET(self):
        if self.path != '/':
            self.send_error(404)
            return
        self.send_response(200)
        self.send_header('Content-Type', 'text/html')
        self.send_header('Content-Length', str(len(PAGE)))
        self.end_headers()
        self.wfile.write(PAGE)


server = http.server.ThreadingHTTPServer(('127.0.0.1', 0), Site)
threading.Thread(target=server.serve_forever, daemon=True).start()


def click(at, control):
    x, y = control['x'] + control['width'] / 2, control['y'] + control['height'] / 2
    return [{'at': at, 'kind': 'move', 'x': x, 'y': y}, {'at': at + 100, 'kind': 'down'}, {'at': at + 180, 'kind': 'up'}]


def capture(name, actions=None, delay=2400):
    base = out / name
    command = [str(exe), '--target', f'http://127.0.0.1:{server.server_port}/', '--autorun', '--no-browser',
               '--width', '1280', '--height', '900', '--screenshot', str(base.with_suffix('.bmp')),
               '--report-json', str(base.with_suffix('.json')), '--ui-state', str(base.with_suffix('.state.json'))]
    if actions:
        script = base.with_suffix('.actions.json')
        script.write_text(json.dumps([{'at': 1300, 'kind': 'focus'}, *actions]), encoding='utf-8')
        command += ['--ui-actions', str(script), '--capture-delay', str(delay)]
    subprocess.run(command, check=True, timeout=40, creationflags=hidden)
    subprocess.run(['ffmpeg', '-y', '-i', str(base.with_suffix('.bmp')), str(base.with_suffix('.png')), '-loglevel', 'error'],
                   check=True, creationflags=hidden)
    state = json.loads(base.with_suffix('.state.json').read_text(encoding='utf-8'))
    assert state['window_flags'] & 8, 'SDL window must stay hidden'
    return state, json.loads(base.with_suffix('.json').read_text(encoding='utf-8'))


def more(state):
    return [c for c in state['endpoint_controls'] if c['key'] == 'infrastructure:more']


try:
    state, report = capture('collapsed')
    total = len(state['infrastructure'])
    assert total > 10, ('fixture must yield more than ten rows', [e['name'] for e in state['infrastructure']])
    assert state['infrastructure_limit'] == 5 and state['tab'] == 4, (state['infrastructure_limit'], state['tab'])
    first = more(state)
    assert len(first) == 1, state['endpoint_controls']
    print(f'PASS collapsed: {total} entries, five shown, one Load more control.')

    state, _ = capture('one-click', click(1400, first[0]))
    assert state['infrastructure_limit'] == 10 and len(state['infrastructure']) == total, state['infrastructure_limit']
    second = more(state)
    assert len(second) == 1 and second[0]['y'] > first[0]['y'], (first, second)
    assert state['tab'] == 4 and state['selected_finding'] == -1 and state['selected_vendor'] == -1, 'Load more must stay on the Overview'
    print('PASS one click: ten shown, control moved below the new rows, Overview retained.')

    state, _ = capture('two-clicks', click(1400, first[0]) + click(1900, second[0]), delay=3000)
    assert state['infrastructure_limit'] == min(total, 15), state['infrastructure_limit']
    assert bool(more(state)) == (total > 15), state['endpoint_controls']
    print(f'PASS two clicks: {state["infrastructure_limit"]} of {total} shown; control present only while rows remain.')

    row_height = (second[0]['y'] - first[0]['y']) / 5
    sixth = {'x': first[0]['x'] + 120, 'y': first[0]['y'] + row_height * 0.5 - 2, 'width': 0, 'height': 0}
    state, report = capture('open-sixth', click(1400, first[0]) + click(1900, sixth), delay=3000)
    expected = state['infrastructure'][5]
    if expected['vendor'] >= 0:
        assert state['tab'] == 3 and state['selected_vendor'] == expected['vendor'], (state['tab'], state['selected_vendor'], expected)
    else:
        assert state['tab'] == 1 and state['selected_finding'] == expected['finding'], (state['tab'], state['selected_finding'], expected)
    print(f'PASS a row revealed by Load more ({expected["name"]}) opens its own evidence.')
finally:
    server.shutdown()
    server.server_close()
