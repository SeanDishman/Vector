import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import time

ROOT = Path(__file__).resolve().parent.parent


def analyze(exe, asset):
    began = time.perf_counter()
    result = subprocess.run([str(exe), '--javascript', str(asset), 'https://fixture.invalid/opaque.js'],
                            capture_output=True, text=True, encoding='utf-8', timeout=30)
    assert result.returncode == 0, result.stderr
    return json.loads(result.stdout)['findings'], round(time.perf_counter() - began, 5)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('before', type=Path)
    parser.add_argument('after', type=Path)
    parser.add_argument('out', type=Path)
    args = parser.parse_args()
    args.out.mkdir(parents=True, exist_ok=True)
    fixtures = ROOT / 'docs/reports/stackfull-comparison-cobalt/asset-traces'
    assets = [fixtures / name for name in ['linkedin-utag.js', 'slack-swiper.decoded.js', 'slack-optimizely.js']]
    pinned = ROOT / 'tests/fixtures/web-signatures'
    assets += [pinned / name for name in ['leaflet.js', 'chart.js.js', 'handlebars.js']]
    mixed = args.out / 'mixed-case.js'
    mixed.write_text('''/*! JQUERY V3.7.1; bOoTsTrAp V5.3.2; @VUE/runtime-core V3.5.1;
    ReAcT V19.0.0; ALPINE.JS V3.14.1; HTMX version 2.0.1; LODASH V4.17.21;
    MOMENT.JS 2.30.1; AXIOS V1.7.1; SOCKET.IO V4.8.0; THREE.JS R170;
    GSAP 3.12.5; CORE-JS@3.42.0; FONT AWESOME FREE 6.7.0;
    copyright 2026 the closure library authors */
    Polymer.version="3.5.2";Polymer({is:"demo-widget"});
    (globalThis.litHtmlVersions??=[]).push("3.3.1");
    (globalThis.litElementVersions??=[]).push("4.2.0");
    (globalThis.reactiveElementVersions??=[]).push("2.1.0");
    React.version="19.0.0";jQuery.fn.jquery="3.7.1";
    ShadyCSS.prepareTemplate(template);goog.provide=function(){};
    window.webpackChunkdemo=[];
    ''', encoding='utf-8')
    assets.append(mixed)
    receipts = []
    for asset in assets:
        before, old_time = analyze(args.before.resolve(), asset)
        after, new_time = analyze(args.after.resolve(), asset)
        assert before == after, (asset.name, before, after)
        receipts.append({'asset': str(asset), 'sha256': hashlib.sha256(asset.read_bytes()).hexdigest(),
                         'evidence_identical': True, 'before_seconds': old_time, 'after_seconds': new_time,
                         'names': [f['name'] for f in after]})
    (args.out / 'verification.json').write_text(json.dumps(receipts, indent=2), encoding='utf-8')
    print(json.dumps({'assets': len(receipts), 'same_findings_and_evidence': True,
                      'before_seconds': round(sum(x['before_seconds'] for x in receipts), 3),
                      'after_seconds': round(sum(x['after_seconds'] for x in receipts), 3)}))


if __name__ == '__main__':
    main()
