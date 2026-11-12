import http.server
import json
from pathlib import Path
import subprocess
import sys
import threading

CASES = [
    ('Google Identity Services', 'https://accounts.google.com/gsi/client'),
    ('Contentful', 'https://images.ctfassets.net/space/asset/image.png'),
    ('Clearbit Reveal', 'https://reveal.clearbit.com/v1/companies/reveal'),
    ('Adobe Experience Platform Tags', 'https://assets.adobedtm.com/fresh/launch-fresh.min.js'),
    ('Marketo', 'https://munchkin.marketo.net/munchkin.js'),
    ('Reddit Ads', 'https://www.redditstatic.com/ads/pixel.js'),
    ('HubSpot Chat', 'https://js.usemessages.com/conversations-embed.js'),
    ('Meta Pixel', 'https://connect.facebook.net/en_US/fbevents.js'),
    ('Microsoft Advertising', 'https://bat.bing.com/bat.js'),
    ('X Ads', 'https://static.ads-twitter.com/uwt.js'),
    ('Hotjar', 'https://static.hotjar.com/c/hotjar-987654.js'),
    ('Contentsquare', 'https://t.contentsquare.net/uxa/fresh.js'),
    ('Quora Pixel', 'https://a.quora.com/qevents.js'),
    ('Qualified', 'https://js.qualified.com/qualified.js'),
    ('Datadog', 'https://rum.browser-intake-datadoghq.com/api/v2/rum'),
    ('Sentry', 'https://o123.ingest.us.sentry.io/api/456/envelope/'),
    ('Vimeo', 'https://player.vimeo.com/video/123456'),
    ('Vimeo', 'https://player.vimeo.com/progressive_redirect/playback/123456/rendition/1080p/file.mp4'),
    ('CloudFront', 'https://d123example.cloudfront.net/media/asset.png'),
]
NAMES = {name for name, _ in CASES}


class Handler(http.server.BaseHTTPRequestHandler):
    def log_message(self, *_):
        pass

    def do_GET(self):
        self.server.hits.append(self.path)
        if self.path == '/positive':
            body = '<!doctype html><title>Declared integration resources</title>' + ''.join(
                f'<script src="{url}?key=SECRET"></script>' for _, url in CASES)
        elif self.path == '/negative':
            body = '<!doctype html><title>Inert integration examples</title>' + ''.join(
                f'<template><script src="{url}"></script></template><a href="{url}">Example</a>'
                f'<link rel="preconnect" href="{url}"><p>{name}</p>' for name, url in CASES)
        else:
            body = '<!doctype html><title>No resources</title>'
        payload = body.encode()
        self.send_response(200)
        self.send_header('Content-Type', 'text/html')
        if self.path == '/positive':
            self.send_header('Via', '1.1 fixture.cloudfront.net (CloudFront)')
        self.send_header('Content-Length', str(len(payload)))
        self.end_headers()
        self.wfile.write(payload)


def main():
    exe, out = Path(sys.argv[1]).resolve(), Path(sys.argv[2]).resolve()
    record_only = '--record-only' in sys.argv
    out.mkdir(parents=True, exist_ok=True)
    server = http.server.ThreadingHTTPServer(('127.0.0.1', 0), Handler)
    server.hits = []
    threading.Thread(target=server.serve_forever, daemon=True).start()
    results = []
    try:
        for case in ('positive', 'negative'):
            command = [str(exe), '--target', f'http://127.0.0.1:{server.server_port}/{case}', '--web-only', '--no-browser']
            run = subprocess.run(command, capture_output=True, text=True, encoding='utf-8', timeout=30)
            assert run.returncode == 0, run.stderr
            (out / (case + '.json')).write_text(run.stdout, encoding='utf-8')
            (out / (case + '.stderr.txt')).write_text(run.stderr, encoding='utf-8')
            report = json.loads(run.stdout)
            rows = [v for v in report['vendors'] if v['name'] in NAMES and v['evidence_level'] == 'Page reference'
                    and v['relationship'] == 'Script']
            checks = {
                'names': {v['name'] for v in rows} == (NAMES if case == 'positive' else set()),
                'sources_retained': all(v['attribution_source'].startswith('https://') and v['evidence'] for v in rows),
                'no_query_values': all('SECRET' not in e['value'] for v in report['vendors'] for e in v['evidence']),
                'no_observed_contacts': not report['vendor_coverage']['vendor_requests_observed'],
                'browser_disabled': report['browser']['state'] == 'disabled',
                'bounded': report['web_coverage']['page_requests'] <= 12,
            }
            results.append({'case': case, 'checks': checks, 'names': sorted(v['name'] for v in rows)})
        (out / 'verification.json').write_text(json.dumps(results, indent=2), encoding='utf-8')
        failed = [r['case'] + ':' + k for r in results for k, v in r['checks'].items() if not v]
        print(json.dumps({'passed': sum(v for r in results for v in r['checks'].values()), 'failed': failed}))
        if not record_only:
            assert not failed, failed
    finally:
        server.shutdown()
        server.server_close()


if __name__ == "__main__":
    main()
