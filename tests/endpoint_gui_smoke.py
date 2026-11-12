import http.server
import json
from pathlib import Path
import subprocess
import sys
import threading

exe=Path(sys.argv[1]).resolve()
out=Path(sys.argv[2]).resolve();out.mkdir(parents=True,exist_ok=True)

def capture(name,*args,actions=None):
    shot=out/(name+'.bmp');report=out/(name+'.json');state=out/(name+'.state.json')
    cmd=[str(exe),'--no-browser','--screenshot',str(shot),'--report-json',str(report),'--ui-state',str(state),*args]
    if actions:
        path=out/(name+'.actions.json');path.write_text(json.dumps(actions));cmd+=['--ui-actions',str(path),'--capture-delay','1800']
    subprocess.run(cmd,check=True,timeout=30)
    assert shot.stat().st_size>100000
    subprocess.run(['ffmpeg','-y','-i',str(shot),str(out/(name+'.png')),'-loglevel','error'],check=True)
    return json.loads(report.read_text()),json.loads(state.read_text())

class Router(http.server.BaseHTTPRequestHandler):
    def log_message(self,*_):pass
    def do_GET(self):
        body=b'<html><title>MikroTik RouterOS</title><p>Loopback fixture</p></html>'
        self.send_response(200);self.send_header('Content-Type','text/html');self.send_header('Content-Length',str(len(body)));self.end_headers();self.wfile.write(body)

server=http.server.ThreadingHTTPServer(('127.0.0.1',0),Router)
threading.Thread(target=server.serve_forever,daemon=True).start()
try:
    for name,size in [('endpoint-demo',[]),('endpoint-compact',['--width','1000','--height','700'])]:
        report,state=capture(name,'--demo',*size)
        assert report['status']=='demo' and report['geolocation']['city']=='Example City'
        assert report['reachability']['icmp']['state']=='reply' and state['navigation_visible']
    url=f'http://127.0.0.1:{server.server_port}/'
    for name,args in [('endpoint-router',[]),('endpoint-router-compact',['--width','1000','--height','700']),('endpoint-evidence',['--view','2'])]:
        report,state=capture(name,'--target',url,'--autorun',*args)
        assert report['geolocation']['state']=='skipped'
        assert report['reachability']['icmp']['state']=='reply'
        assert report['reachability']['icmp']['reply_address']=='127.0.0.1'
        assert report['reachability']['tcp_open']==1
        assert any(f['category']=='Device hint' and f['name']=='MikroTik RouterOS' for f in report['findings'])
    actions=[{'at':450,'kind':'focus'},{'at':490,'kind':'move','x':1030,'y':422},
             {'at':530,'kind':'down'},{'at':620,'kind':'up'}]
    report,state=capture('endpoint-identity-selected','--demo',actions=actions)
    assert state['show_all'] and state['tab']==1 and state['selected_finding']>=0,state
    print('Six endpoint SDL scenarios passed: location, compact, live IPv4 echo, device hints, evidence and selection.')
finally:server.shutdown();server.server_close()
