import json
from pathlib import Path
import subprocess
import sys
from PIL import Image
from frontend_integration import Fixture
from export_formats import leaves, read_csv, text_fields

def click(x, y, at):
    return [{'at': at, 'kind': 'focus'}, {'at': at + 30, 'kind': 'move', 'x': x, 'y': y},
            {'at': at + 70, 'kind': 'down'}, {'at': at + 130, 'kind': 'up'}]

def key(value, at):
    return [{'at': at, 'kind': 'key-down', 'key': value}, {'at': at + 60, 'kind': 'key-up', 'key': value}]

exe, out = Path(sys.argv[1]).resolve(), Path(sys.argv[2]).resolve()
out.mkdir(parents=True, exist_ok=True)
fixture = Fixture()
try:
    evidence_actions = click(350, 281, 2000) + click(350, 311, 2300) + click(350, 433, 2600)
    cases = [(f'{view}-{width}', view, width, '/entry', evidence_actions if view == 2 else None) for width in (1280, 1000) for view in (4, 2)]
    cases += [('selected', 4, 1280, '/entry', [
        {'at': 2000, 'kind': 'focus'}, {'at': 2050, 'kind': 'move', 'x': 350, 'y': 390},
        {'at': 2090, 'kind': 'down'}, {'at': 2150, 'kind': 'up'}]),
        ('truncated', 2, 1000, '/large', click(350, 281, 2000) + click(350, 311, 2300) + click(350, 457, 2600))]
    for index, extension in ((1, 'csv'), (2, 'txt')):
        actions = click(1222, 110, 2000) + key(1073741898, 2300)
        for step in range(index):
            actions += key(1073741905, 2450 + step * 150)
        actions += key(13, 2900)
        cases.append(('export-' + extension, 4, 1280, '/entry', actions))
    if len(sys.argv) > 3:
        cases = [case for case in cases if case[0] in sys.argv[3:]]
    for name, view, width, path, actions in cases:
        bmp, report, state = [out / (name + suffix) for suffix in ('.bmp', '.json', '.state.json')]
        command = [str(exe), '--target', fixture.url(path), '--autorun', '--no-browser', '--screenshot', str(bmp),
                   '--report-json', str(report), '--ui-state', str(state), '--view', str(view),
                   '--width', str(width), '--height', '800' if width == 1280 else '700']
        if actions:
            action_file = out / (name + '.actions.json');action_file.write_text(json.dumps(actions))
            command += ['--ui-actions', str(action_file), '--capture-delay', '3400']
        subprocess.run(command, check=True, timeout=30)
        r, s = json.loads(report.read_text()), json.loads(state.read_text())
        assert s['navigation_visible']
        if path == '/entry':
            assert {'Radix UI', 'React Router', 'Statsig', 'Tailwind CSS'} <= {f['name'] for f in r['findings']}
        if name == 'selected':
            assert s['tab'] == 1 and s['selected_finding'] >= 0, s
            assert r['findings'][s['selected_finding']]['name'] == 'Radix UI'
        if name == 'truncated':
            assert r['stylesheets'][0]['body_truncated'] and r['stylesheets'][0]['analyzed']
        if name.startswith('export-'):
            assert not s['error'] and s['notice'].startswith('Saved: '), s
            saved = Path(s['notice'].removeprefix('Saved: '))
            actual = read_csv(saved) if name.endswith('csv') else text_fields(saved.read_text(encoding='utf-8'))
            assert actual == dict(leaves(r))
        Image.open(bmp).save(out / (name + '.png'))
    print(f'{len(cases)} native frontend cases passed: both sizes, finding selection, CSS outcomes, truncation and complete CSV/TXT export.')
finally:
    fixture.shutdown();fixture.server_close()
