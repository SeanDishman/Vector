import json
from pathlib import Path
import subprocess
import sys

exe = Path(sys.argv[1]).resolve()
out = Path(sys.argv[2]).resolve()
out.mkdir(parents=True, exist_ok=True)


def capture(name, *args, actions=None):
    report, state, bmp = [out / (name + suffix) for suffix in ('.json', '.state.json', '.bmp')]
    command = [str(exe), '--demo', '--screenshot', str(bmp), '--report-json', str(report), '--ui-state', str(state), *args]
    if actions:
        action_file = out / (name + '.actions.json')
        action_file.write_text(json.dumps(actions))
        command += ['--ui-actions', str(action_file), '--capture-delay', '1800']
    subprocess.run(command, check=True, timeout=15)
    subprocess.run(['ffmpeg', '-y', '-i', str(bmp), str(out / (name + '.png')), '-loglevel', 'error'], check=True)
    r = json.loads(report.read_text())
    assert r['status'] == 'demo' and r['dns']['state'] == 'complete' and len(r['dns']['queries']) == 4
    assert {f['category'] for f in r['findings']} >= {'DNS hosting alias', 'Inbound email', 'Authoritative DNS', 'Email authorization'}
    assert all(q['source'] == 'Illustrative data' for q in r['dns']['queries'])
    return json.loads(state.read_text())


for width, height in ((1280, 800), (1000, 700)):
    for view in (4, 2):
        s = capture(f'dns-{view}-{width}', '--width', str(width), '--height', str(height), '--view', str(view))
        assert s['navigation_visible'] and s['tab'] == view
def click(x, y):
    return [{'at': 450, 'kind': 'focus'}, {'at': 490, 'kind': 'move', 'x': x, 'y': y},
            {'at': 530, 'kind': 'down'}, {'at': 620, 'kind': 'up'}]


s = capture('dns-selected', actions=click(336, 520))
assert s['show_all'] and s['tab'] == 1
selected = json.loads((out / 'dns-selected.json').read_text())['findings'][s['selected_finding']]
assert selected['name'] == 'Amazon CloudFront' and selected['category'] == 'DNS hosting alias'
capture('dns-record-expanded', '--view', '2', '--width', '1000', '--height', '700', actions=click(263, 389))
print('Six native DNS scenarios passed: both sizes, role selection and raw-record expansion.')
