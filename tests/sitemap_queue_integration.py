import http.server
import json
from pathlib import Path
import subprocess
import sys
import threading
import time

class Handler(http.server.BaseHTTPRequestHandler):
    def log_message(self,*_):pass
    def do_GET(self):
        self.server.paths.append(self.path)
        status,mime,body=404,'text/plain',b'unknown'
        if self.path=='/':status,mime,body=200,'text/html',b'<title>Queue fixture</title>'
        elif self.path=='/sitemap.xml':
            status,mime=200,'application/xml'
            body=('<urlset>'+''.join(f'<url><loc>/story/{i:05d}</loc></url>' for i in range(8000))+
                  '<url><loc>/docs</loc></url><url><loc>/docs</loc></url></urlset>').encode()
        elif self.path=='/docs' or self.path.startswith('/story/'):
            status,mime,body=200,'text/html',b'<title>Linked document</title>'
        self.send_response(status);self.send_header('Content-Type',mime);self.send_header('Content-Length',str(len(body)));self.end_headers()
        try:self.wfile.write(body)
        except (ConnectionResetError,ConnectionAbortedError,BrokenPipeError):pass

server=http.server.ThreadingHTTPServer(('127.0.0.1',0),Handler);server.daemon_threads=True;server.paths=[]
threading.Thread(target=server.serve_forever,daemon=True).start()
try:
    begin=time.monotonic()
    p=subprocess.run([str(Path(sys.argv[1]).resolve()),'--target',f'http://127.0.0.1:{server.server_port}/','--web-only','--no-browser'],capture_output=True,check=True,timeout=15)
    r=json.loads(p.stdout);elapsed=time.monotonic()-begin
    assert elapsed<10,elapsed
    assert r['status']=='complete' and r['web_coverage']['page_requests']==12
    assert r['browser']['state']=='disabled'
    assert all(w['url'].startswith('http://127.0.0.1:') for w in r['web'])
    assert server.paths.count('/docs')==1
    assert server.paths.index('/docs')<server.paths.index('/story/00000')
    stories=[p for p in server.paths if p.startswith('/story/')]
    assert stories==[f'/story/{i:05d}' for i in range(len(stories))]
    print(f'8,000-link sitemap passed in {elapsed:.3f}s: priority, duplicate suppression, FIFO ties and 12-page cap.')
finally:server.shutdown();server.server_close()
