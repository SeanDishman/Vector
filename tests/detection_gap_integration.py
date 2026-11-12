import argparse
import http.server
import json
from pathlib import Path
import subprocess
import threading
import time

BUNDLE = '''
const utag={loader:{ready_q:[]}};
utag.cfg={template:"ut4.54."};
const data={};data.tealium_library_name="utag.js";
utag.loader.ready_q.forEach(()=>{});
const defaults={wrapperClass:"swiper-wrapper",slideClass:"swiper-slide"};
const swiper={slidesPerViewDynamic(){return 1}};swiper.slidesPerViewDynamic();
window.optimizely={};window.optimizely.initialized=true;
const api={getActiveExperimentIds:function(){return []},getVariationMap:function(){return {}}};
fetch('/main-loaded');
'''
hits=[]


class Handler(http.server.BaseHTTPRequestHandler):
    def log_message(self,*args):pass
    def do_GET(self):
        hits.append(self.path)
        status,mime=200,'text/html'
        if self.path=='/late':
            body='<title>Delayed loader fixture</title><script src="/loader-late.js"></script><a href="/subprocessors">Service providers</a>'
        elif self.path=='/':
            body='<title>Renamed bundle fixture</title><script src="/loader.js"></script><a href="/subprocessors">Service providers</a>'
        elif self.path=='/loader.js':
            mime='application/javascript';body="setTimeout(()=>import('/assets/a9e7.mjs'),80);"
        elif self.path=='/loader-late.js':
            mime='application/javascript';body="setTimeout(()=>import('/assets/a9e7.mjs'),5500);"
        elif self.path=='/assets/a9e7.mjs':
            mime='application/javascript';body=BUNDLE
        elif self.path=='/subprocessors':
            time.sleep(.25)
            body='<h1>Subprocessors</h1><table><tr><th>Company</th><th>Service</th></tr><tr><td>Fixture Storage</td><td>Hosting</td></tr></table>'
        elif self.path=='/main-loaded':
            mime='text/plain';body='executed'
        else:status=404;body='Not found'
        payload=body.encode()
        try:
            self.send_response(status);self.send_header('Content-Type',mime)
            self.send_header('Content-Length',str(len(payload)));self.end_headers();self.wfile.write(payload)
        except (BrokenPipeError,ConnectionResetError,ConnectionAbortedError):pass


def run(exe,out,record_only=False):
    out.mkdir(parents=True,exist_ok=True)
    server=http.server.ThreadingHTTPServer(('127.0.0.1',0),Handler)
    threading.Thread(target=server.serve_forever,daemon=True).start()
    target=f'http://127.0.0.1:{server.server_port}/'
    expected={'Tealium','Swiper','Optimizely'}
    results=[]
    try:
        for name,options in [('browser',[]),('static',['--no-browser']),('late',['--browser-timeout','10000'])]:
            hits.clear()
            result=subprocess.run([str(exe),'--target',target+('late' if name=='late' else ''),'--browser-timeout','3000',*options],capture_output=True,text=True,encoding='utf-8',timeout=45)
            assert result.returncode==0,result.stderr
            report=json.loads(result.stdout)
            (out/(name+'.json')).write_text(result.stdout,encoding='utf-8')
            (out/(name+'.stderr')).write_text(result.stderr,encoding='utf-8')
            request_order=list(hits)
            findings=[f for f in report['findings'] if f['name'] in expected]
            names={f['name'] for f in findings}
            passed={
                'expected_names':names==expected if name!='static' else not names,
                'source_bytes':all(len(f['evidence'])>=3 and not f['version'] for f in findings),
                'provider_retained':any(v['name']=='Fixture Storage' and v['published_by']=='127.0.0.1' for v in report['vendors']),
                'page_cap':report['web_coverage']['page_requests']<=12,
                'script_cap':report['web_coverage']['script_requests']<=40,
            }
            if name!='static':
                passed['javascript_executed']='/main-loaded' in request_order
                passed['endpoint_before_disclosure']='/main-loaded' in request_order and request_order.index('/main-loaded')<request_order.index('/subprocessors')
                passed['dynamic_body_analyzed']=any(s['url'].endswith('/assets/a9e7.mjs') and s['analyzed'] and s['collection']=='browser' for s in report['scripts'])
                passed['endpoint_browser_first']=bool(report['browser']['pages']) and report['browser']['pages'][0]['purpose']=='endpoint'
            else:passed['no_browser']=not report['browser']['pages']
            results.append({'case':name,'checks':passed,'names':sorted(names),'request_order':request_order})
        (out/'verification.json').write_text(json.dumps(results,indent=2),encoding='utf-8')
        failures=[r['case']+': '+k for r in results for k,v in r['checks'].items() if not v]
        if not record_only:assert not failures,failures
        print(json.dumps({'passed':sum(v for r in results for v in r['checks'].values()),'failed':failures}))
    finally:server.shutdown();server.server_close()


if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('exe',type=Path);parser.add_argument('out',type=Path);parser.add_argument('--record-only',action='store_true')
    args=parser.parse_args();run(args.exe.resolve(),args.out.resolve(),args.record_only)
