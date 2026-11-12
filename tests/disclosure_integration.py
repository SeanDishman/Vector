import html
import http.server
import json
from pathlib import Path
import subprocess
import sys
import threading
import time

fixtures=json.loads((Path(__file__).parent/'fixtures/disclosures/provider-tables.json').read_text(encoding='utf-8'))['records']
source=fixtures[0]
table='<h1>Our Service Partners</h1><table><tr><th>Company</th><th>Country/Region</th></tr>'+''.join('<tr><td>'+html.escape(name)+'</td><td>'+html.escape(country)+'</td></tr>' for name,country in source['rows'])+'</table>'
google_records=[r for r in fixtures if r['publisher']=='google.com']
google_tables=[]
for record in google_records:
    parts=[]
    for item in record['tables']:
        parts.append('<h4>'+html.escape(item['heading'])+'</h4><table>')
        if 'context' in item:parts.append('<tr><th colspan="'+str(len(item['columns']))+'">'+html.escape(item['context'])+'</th></tr>')
        parts.append('<tr>'+''.join('<th>'+html.escape(c)+'</th>' for c in item['columns'])+'</tr>')
        parts.extend('<tr>'+''.join('<td>'+html.escape(c)+'</td>' for c in row)+'</tr>' for row in item['rows'])
        parts.append('</table>')
    google_tables.append(''.join(parts))
requests=[]
class Handler(http.server.BaseHTTPRequestHandler):
    def log_message(self,*args):pass
    def do_GET(self):
        requests.append(self.path)
        status=200
        if self.path=='/':body="<title>Local disclosure fixture</title><a href='/subprocessors'>Service partners</a><a href='/subprocessors'>Again</a>"
        elif self.path=='/google':body="<title>Google provider table fixture — loopback</title>"+''.join(f"<a href='/subprocessors-google-{i}'>Official table fixture {i}</a>" for i in range(len(google_tables)))
        elif self.path.startswith('/subprocessors-google-'):body=google_tables[int(self.path.rsplit('-',1)[1])]
        elif self.path=='/large-disclosure':body="<a href='/subprocessors-large'>Large provider document</a>"
        elif self.path=='/subprocessors-large':body='<!--'+('x'*(1250*1024))+'-->'+google_tables[0]
        elif self.path=='/oversize-disclosure':body="<a href='/subprocessors-oversize'>Oversized provider document</a>"
        elif self.path=='/subprocessors-oversize':body='<!--'+('x'*(3*1024*1024+1024))+'-->'+table
        elif self.path=='/subprocessors':body=table
        elif self.path=='/reject':body="<a href='/subprocessors-denied'>Service partners</a>"
        elif self.path=='/subprocessors-denied':status=403;body=table
        elif self.path=='/false':body="<script>const fake=`<a href='/subprocessors-evil'>x</a>`;</script>"
        elif self.path=='/redirects':body="<a href='/subprocessors-redirect'>Providers</a>"
        elif self.path=='/subprocessors-redirect':
            self.send_response(302);self.send_header('Location','/subprocessors');self.send_header('Content-Length','0');self.end_headers();return
        elif self.path=='/limits':body=''.join(f"<a href='/subprocessors-hop-{i}'>Providers</a>" for i in range(4))
        elif self.path.startswith('/subprocessors-hop-'):
            self.send_response(302);self.send_header('Location',self.path.replace('-hop-','-final-'));self.send_header('Content-Length','0');self.end_headers();return
        elif self.path.startswith('/subprocessors-final-'):body=table
        elif self.path=='/slow':body=''.join(f"<a href='/subprocessors-slow-{i}'>Providers</a>" for i in range(4))
        elif self.path.startswith('/subprocessors-slow-'):time.sleep(3);body=table
        else:status=404;body='Not found'
        encoded=body.encode()
        try:
            self.send_response(status);self.send_header('Content-Type','text/html');self.send_header('Content-Length',str(len(encoded)));self.end_headers();self.wfile.write(encoded)
        except (BrokenPipeError,ConnectionResetError,ConnectionAbortedError):pass

def start():
    server=http.server.ThreadingHTTPServer(('127.0.0.1',0),Handler)
    threading.Thread(target=server.serve_forever,daemon=True).start();return server

if __name__=='__main__':
    exe=Path(sys.argv[1]).resolve();server=start();base=f'http://127.0.0.1:{server.server_port}'
    try:
        def run(path):
            result=subprocess.run([str(exe),'--target',base+path,'--no-browser'],capture_output=True,text=True,encoding='utf-8',timeout=15)
            assert result.returncode==0,result.stderr
            return json.loads(result.stdout)
        report=run('/')
        providers=[v for v in report['vendors'] if v['published_by']]
        assert {v['name'] for v in providers}=={row[0] for row in source['rows']}
        assert all(v['evidence_level']=='Published disclosure' and v['published_by']=='127.0.0.1' for v in providers)
        assert requests.count('/subprocessors')==1,requests
        assert report['disclosures'][0]['state']=='collected' and report['disclosures'][0]['providers']==42
        assert report['web_coverage']['page_requests']<=12
        assert not any(f['name'] in {row[0] for row in source['rows']} for f in report['findings'])
        rejected=run('/reject')
        assert not any(v['published_by'] for v in rejected['vendors'])
        assert rejected['disclosures'][0]['http_status']==403 and rejected['disclosures'][0]['state']=='failed'
        run('/false');assert '/subprocessors-evil' not in requests
        redirected=run('/redirects');source=next(d for d in redirected['disclosures'] if d['state']=='collected')
        hop=next(d for d in redirected['disclosures'] if d['state']=='redirect')
        assert hop['url'].endswith('/subprocessors-redirect') and hop['requested_url']==hop['url']
        assert source['url'].endswith('/subprocessors') and source['requested_url']==source['url']
        assert source['discovered_from']==hop['url'] and len(source['discovery_chain'])==2
        assert source['discovery_chain'][0]['source'].endswith('/redirects')
        assert source['discovery_chain'][-1]['source']==hop['url'] and 'HTTP 302 redirect -> '+source['url'] in source['discovery_chain'][-1]['value']
        requests.clear();limited=run('/limits')
        assert len([p for p in requests if p.startswith('/subprocessors-')])==4,requests
        assert any(d['state']=='skipped' and 'Four disclosure' in d['reason'] for d in limited['disclosures'])
        requests.clear();began=time.monotonic();slow=run('/slow')
        assert time.monotonic()-began<9
        assert len([p for p in requests if p.startswith('/subprocessors-slow-')])<=3,requests
        assert any(d['state']=='skipped' and 'Five-second' in d['reason'] for d in slow['disclosures']),slow['disclosures']
        google=run('/google')
        providers=[v for v in google['vendors'] if v['published_by']]
        expected={row[0].lower() for record in google_records for table in record['tables'] for row in table['rows']}
        assert {v['name'].lower() for v in providers}==expected and len(providers)==169
        assert {d['providers'] for d in google['disclosures'] if d['state']=='collected'}=={112,106,76}
        assert any(v['relationship']=='Group subprocessor' for v in providers)
        assert all(v['evidence_level']=='Published disclosure' for v in providers)
        assert not any(f['name'].lower() in expected for f in google['findings'])
        assert google['web_coverage']['page_requests']<=12
        large=run('/large-disclosure')
        assert any(d['state']=='collected' and d['providers']==112 for d in large['disclosures'])
        oversize=run('/oversize-disclosure')
        assert not any(v['published_by'] for v in oversize['vendors'])
        assert any('3 MiB' in d['reason'] for d in oversize['disclosures'])
        print('9 disclosure network scenarios passed: prior disclosure behaviors, all Google table formats, a table beyond 1 MiB, and the 3 MiB disclosure cap.')
    finally:server.shutdown()
