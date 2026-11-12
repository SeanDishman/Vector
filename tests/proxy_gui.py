import json
from pathlib import Path
import socket
import subprocess
import sys
from PIL import Image
from proxy_integration import Fixture, USER, PASSWORD


def click(point, at):
    x, y = round(point['x']), round(point['y'])
    return [{'at': at, 'kind': 'focus'}, {'at': at+20, 'kind': 'move', 'x': x, 'y': y},
            {'at': at+60, 'kind': 'down'}, {'at': at+120, 'kind': 'up'}]


def fill(point, value, at):
    actions = click(point, at)+[{'at': at+150, 'kind': 'key-down', 'key': 1073741901},
                                {'at': at+170, 'kind': 'key-up', 'key': 1073741901}]
    for i in range(8):
        actions += [{'at': at+190+i*22, 'kind': 'key-down', 'key': 8},
                    {'at': at+200+i*22, 'kind': 'key-up', 'key': 8}]
    return actions+[{'at': at+390, 'kind': 'text', 'value': value}]


def main():
    exe, out = Path(sys.argv[1]).resolve(), Path(sys.argv[2]).resolve()
    out.mkdir(parents=True, exist_ok=True)
    settings = out/'settings.dat'
    if settings.exists(): raise RuntimeError('Use a fresh output directory to preserve previous evidence.')
    fixture = Fixture(); proxy = fixture.proxy(); cases = []
    def capture(name, *args, width=1280, height=800, actions=(), store=None):
        shot, state, report = [out/(name+ext) for ext in ['.bmp', '.state.json', '.json']]
        command = [str(exe), '--settings-path', str(store or settings), '--screenshot', str(shot), '--ui-state', str(state),
                   '--report-json', str(report), '--width', str(width), '--height', str(height), '--no-browser', *args]
        if actions:
            path = out/(name+'.actions.json'); path.write_text(json.dumps(actions))
            command += ['--ui-actions', str(path), '--capture-delay', str(max(a['at'] for a in actions)+500)]
        process = subprocess.run(command, capture_output=True, text=True, timeout=40)
        assert process.returncode == 0, (name, process.stderr)
        s = json.loads(state.read_text()); r = json.loads(report.read_text()) if report.exists() else None
        Image.open(shot).save(out/(name+'.png'))
        assert PASSWORD not in state.read_text() and USER not in state.read_text()
        assert not r or PASSWORD not in report.read_text()
        cases.append(name)
        return s, r
    try:
        first, _ = capture('empty-settings', '--settings-view')
        controls = {c['key']: c for c in first['proxy_controls']}
        compact, _ = capture('compact-settings', '--settings-view', width=1000, height=720)
        compact_controls = {c['key']: c for c in compact['proxy_controls']}
        assert compact_controls['save']['y']+compact_controls['save']['height']/2 < 704
        assert not first['proxy_enabled'] and first['settings_open'] and not settings.exists()
        actions = click(controls['enabled'], 400)
        for offset, key, value in [(800, 'host', '127.0.0.1'), (1250, 'port', str(proxy.server_address[1])),
                                   (1700, 'username', USER), (2150, 'password', PASSWORD)]:
            actions += fill(controls[key], value, offset)
        actions += click(controls['save'], 2700)
        saved, _ = capture('saved-settings', '--settings-view', actions=actions)
        assert saved['proxy_enabled'] and saved['proxy_has_credentials'] and not saved['proxy_error'], saved
        assert saved['proxy_endpoint'] == f'127.0.0.1:{proxy.server_address[1]}'
        assert 'type' not in controls and 'type' not in {c['key'] for c in saved['proxy_controls']}
        assert PASSWORD.encode() not in settings.read_bytes() and USER.encode() not in settings.read_bytes()
        assert not proxy.hits and not fixture.web.hits, 'Saving settings started network requests'
        restored, _ = capture('restored-settings', '--settings-view', width=1000, height=720)
        assert restored['proxy_enabled'] and restored['proxy_has_credentials']
        ready, _ = capture('header-open', actions=click(controls['settings'], 400))
        assert ready['settings_open']
        _, r = capture('proxied-inspection', '--target', 'http://gui-proxy-fixture.invalid:80/entry', '--autorun')
        assert r['proxy']['enabled'] and r['reachability']['icmp']['state'] == 'skipped'
        assert r['proxy']['endpoint'] == f'http://127.0.0.1:{proxy.server_address[1]}', r['proxy']
        assert any(note.startswith('Proxy type detected automatically. HTTP proxy') for note in r['notes']), r['notes']
        assert any(f['name'] == 'WordPress' for f in r['findings']) and any(v['name'] == fixture.web.provider for v in r['vendors'])
        assert ('gui-proxy-fixture.invalid', 80) in proxy.hits
        bad, _ = capture('invalid-port', '--settings-view', actions=fill(controls['port'], '0', 400)+click(controls['save'], 950))
        assert bad['proxy_error'] and bad['proxy_enabled']
        disabled, _ = capture('disabled-settings', '--settings-view', actions=click(controls['enabled'], 400)+click(controls['save'], 800))
        assert not disabled['proxy_enabled'] and not disabled['proxy_error']
        before = len(proxy.hits)
        _, r = capture('direct-inspection', '--target', f'http://127.0.0.1:{fixture.web.server_port}/entry', '--autorun')
        assert not r['proxy']['enabled'] and len(proxy.hits) == before
        assert any(f['name'] == 'WordPress' for f in r['findings'])
        pasted, address = out/'pasted.dat', f'127.0.0.1:{proxy.server_address[1]}'
        typing = click(controls['enabled'], 400)+fill(controls['host'], f'{address}:{USER}:{PASSWORD}', 800)+click(controls['save'], 1500)
        line, _ = capture('pasted-line', '--settings-view', actions=typing, store=pasted)
        assert line['proxy_enabled'] and line['proxy_has_credentials'] and not line['proxy_error'], line
        assert line['proxy_endpoint'] == address and PASSWORD.encode() not in pasted.read_bytes()
        _, r = capture('pasted-line-inspection', '--target', 'http://gui-pasted-fixture.invalid:80/entry', '--autorun', store=pasted)
        assert any(f['name'] == 'WordPress' for f in r['findings']) and ('gui-pasted-fixture.invalid', 80) in proxy.hits
        with socket.socket() as probe:
            probe.bind(('127.0.0.1', 0))
            closed = probe.getsockname()[1]
        unreachable = out/'unreachable.dat'
        typing = click(controls['enabled'], 400)+fill(controls['host'], f'127.0.0.1:{closed}', 800)+click(controls['save'], 1500)
        capture('unreachable-saved', '--settings-view', actions=typing, store=unreachable)
        before = len(fixture.web.hits)
        _, r = capture('unreachable-proxy', '--target', f'http://127.0.0.1:{fixture.web.server_port}/entry', '--autorun', store=unreachable)
        assert r['status'] == 'error' and f'Could not connect to the proxy at 127.0.0.1:{closed}' in r['error'], r['error']
        assert len(fixture.web.hits) == before and not r['findings'], 'A failed proxy check fell back to a direct connection'
        (out/'verification.json').write_text(json.dumps({'cases': cases, 'protected_storage': True, 'no_traffic_on_save': True}, indent=2))
        print(f'{len(cases)} native proxy cases passed: both sizes, editing, save/reload, validation, routed scans, a pasted proxy line, an unreachable proxy and disable.')
    finally: fixture.close()


if __name__ == '__main__': main()
