import http.server
import json
from pathlib import Path
import socket
import sys
import threading
from web_cap_integration import run

assets=Path(__file__).parent/'fixtures/react-18.3.1'
react=(assets/'react.production.min.js').read_bytes()
react_dom=(assets/'react-dom.production.min.js').read_bytes()
hits=[]


class Web(http.server.BaseHTTPRequestHandler):
    def log_message(self,*_): pass
    def handle(self):
        try: super().handle()
        except (BrokenPipeError,ConnectionResetError,ConnectionAbortedError): pass
    def do_GET(self):
        path=self.server.mode if self.path=='/' else self.path
        browser='Chrome/' in self.headers.get('User-Agent','')
        hits.append((self.server.server_port,self.path,browser))
        status,mime,body=200,'text/html',b'<title>Coverage fixture</title>'
        if path=='/flood':
            body+=b''.join(f'<a href="/item/{i}">item</a>'.encode() for i in range(1000))
        elif path=='/stack':
            body+=b'<meta name="generator" content="WordPress 6.7">'
        elif path in ('/blocked','/disconnected'):
            if not browser:
                if path=='/disconnected':
                    self.connection.shutdown(socket.SHUT_RDWR);self.connection.close();return
                status,mime,body=403,'text/plain',b'Static client response'
            else: body+=b'<script src="/late.js"></script>'
        elif path=='/late': body+=b'<script src="/late.js"></script>'
        elif path=='/partial': body+=b'<script src="/partial.js"></script>'
        elif path=='/late.js':
            mime,body='application/javascript',b'/*'+b'x'*(600*1024)+b'*/\n'+react
        elif path=='/partial.js':
            mime,body='application/javascript',react+b'\n/*'+b'x'*(2200*1024)+b'*/'
        elif path=='/react-dom':
            body+=b'''<div id="root"></div><script src="/opaque.js"></script><script>
                const root=ReactDOM.createRoot(document.getElementById('root'));
                root.render(React.createElement('span',null,'Production renderer'));
                delete window.React;delete window.ReactDOM;
            </script>'''
        elif path=='/opaque.js':
            mime,body='application/javascript',b'/*'+b'x'*(4*1024*1024)+b'*/\n'+react+b'\n'+react_dom
        elif path=='/fake-react':
            body+=b'''<div id="root" data-react="React"></div><script>
                const n=document.getElementById('root');
                n['__reactFiber$fake']={tag:5,stateNode:document.body};
                n['__reactProps$fake']={};
            </script>'''
        elif path=='/denied': status,body=403,b'<title>Request denied</title>'
        elif path=='/headers': pass
        elif path.startswith('/item/'): pass
        else: status,body=404,b''
        self.send_response(status)
        self.send_header('Content-Type',mime)
        if path=='/headers' and browser:
            self.send_header('X-Powered-By','Express')
            self.send_header('X-Served-By','cache-dfw-kdfw8210022-DFW')
        self.send_header('Content-Length',str(len(body)))
        self.end_headers();self.wfile.write(body)


def start(mode):
    server=http.server.ThreadingHTTPServer(('127.0.0.1',0),Web);server.mode=mode
    threading.Thread(target=server.serve_forever,daemon=True).start();return server


if __name__=='__main__':
    exe=Path(sys.argv[1]).resolve();baseline='--baseline' in sys.argv
    first,second=start('/flood'),start('/stack')
    if first.server_port>second.server_port: first,second=second,first
    first.mode,second.mode='/flood','/stack'
    base=f'http://127.0.0.1:{first.server_port}'
    failed=[];checked=0
    def check(value,name):
        global checked
        checked+=1
        if not value: failed.append(name)
        if not value and not baseline: raise AssertionError(name)
    try:
        report=run(exe,'127.0.0.1','--ports',f'{first.server_port},{second.server_port}','--no-browser')
        check(any(f['name']=='WordPress' for f in report['findings']),'entry on second port survives first-port link flood')
        check(report['web_coverage']['page_requests']<=12,'shared request cap preserved')
        for path in ('/blocked','/disconnected','/late'):
            report=run(exe,base+path)
            if report['browser']['state']=='unavailable':
                print('Installed browser unavailable for coverage fixtures');sys.exit(77)
            check(any(s['collection']=='browser' and s['analyzed'] and s['body_bytes']>512*1024 for s in report['scripts']),path+' collects the actual browser bundle')
            check(any(f['name']=='React' and any('Browser script' in e['source'] for e in f['evidence']) for f in report['findings']),path+' identifies React after the first 512 KiB')
        report=run(exe,base+'/partial')
        check(any(s['collection']=='browser' and s['analyzed'] and s['body_truncated'] and s['body_bytes']==2*1024*1024 for s in report['scripts']),'large bundle retains analyzed prefix')
        report=run(exe,base+'/react-dom')
        check(any(f['name']=='React' and not f['version'] and any('__reactFiber$' in e['value'] and 'Browser runtime' in e['source'] for e in f['evidence']) for f in report['findings']),'real production renderer detected without globals or downloaded source')
        report=run(exe,base+'/fake-react')
        check(not any(f['name']=='React' for f in report['findings']),'mismatched DOM properties do not identify React')
        report=run(exe,base+'/denied')
        check(report['browser']['state']=='http_error' and any(p.get('status')==403 for p in report['browser']['pages']),'browser rejection remains explicit')
        report=run(exe,base+'/headers')
        check({'Express','Fastly'}<={f['name'] for f in report['findings']},'browser-only response headers feed named findings')
        check(all(any('Browser response' in e['source'] for e in f['evidence']) for f in report['findings'] if f['name'] in ('Express','Fastly')),'browser header names retain their observed source')
        if baseline: print('Baseline gaps:',json.dumps(failed),';',len(failed),'of',checked,'checks failed')
        else: print('9 coverage scenarios passed;',checked,'checks with real React production code, rejected static reads, multiple ports and negative evidence.')
    finally:
        first.shutdown();second.shutdown()
