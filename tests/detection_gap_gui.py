import http.server
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import threading
from PIL import Image
from detection_gap_integration import Handler

exe,out=Path(sys.argv[1]).resolve(),Path(sys.argv[2]).resolve()
out.mkdir(parents=True,exist_ok=True)
server=http.server.ThreadingHTTPServer(('127.0.0.1',0),Handler)
threading.Thread(target=server.serve_forever,daemon=True).start()
try:
    cases=[('overview',1280,800,[]),('compact',1000,700,[]),('evidence',1280,800,['--filter','Tealium']),('cancelled',1280,800,['--cancel-after','5000'])]
    if len(sys.argv)>3:cases=[c for c in cases if c[0] in sys.argv[3:]]
    for name,width,height,options in cases:
        bmp,report,state=[out/(name+ext) for ext in ['.bmp','.json','.state.json']]
        command=[str(exe),'--target',f'http://127.0.0.1:{server.server_port}/'+('late' if name=='cancelled' else ''),'--autorun','--screenshot',str(bmp),
                 '--report-json',str(report),'--ui-state',str(state),'--width',str(width),'--height',str(height),*options]
        proc=subprocess.Popen(command,stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True,encoding='utf-8')
        _,stderr=proc.communicate(timeout=45);assert proc.returncode==0,stderr
        (out/(name+'.stderr')).write_text(stderr,encoding='utf-8')
        r=json.loads(report.read_text(encoding='utf-8'));s=json.loads(state.read_text(encoding='utf-8'))
        if name=='cancelled':
            assert r['status']=='cancelled' and r['browser']['state']=='cancelled',r['browser']
        else:
            expected={'Tealium','Swiper','Optimizely'}
            assert expected<={f['name'] for f in r['findings']}
            assert expected<={f['name'] for f in s['infrastructure']}
            assert any(v['name']=='Fixture Storage' and v['published_by'] for v in r['vendors'])
            assert s['navigation_visible'] and not s['error']
        assert not list(Path(tempfile.gettempdir()).glob(f'vector-browser-{proc.pid}-*')),stderr
        Image.open(bmp).save(out/(name+'.png'))
    print(f'{len(cases)} SDL cases passed: new names, complete evidence, both sizes and active cancellation.')
finally:server.shutdown();server.server_close()
