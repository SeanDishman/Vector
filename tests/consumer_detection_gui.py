import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import time
from PIL import Image
from consumer_detection_integration import start, CORE, SEGMENT

exe,out=Path(sys.argv[1]).resolve(),Path(sys.argv[2]).resolve()
out.mkdir(parents=True,exist_ok=True)
env=os.environ.copy()
for root in ('ProgramFiles(x86)','ProgramFiles','LOCALAPPDATA'):
    for tail in ('Microsoft/Edge/Application/msedge.exe','Google/Chrome/Application/chrome.exe'):
        browser=Path(env.get(root,''))/tail
        if browser.is_file():env.setdefault('VECTOR_BROWSER',str(browser))
env['LOCALAPPDATA']=str(out/'isolated-settings')
server=start();server.core=CORE;server.segment=SEGMENT
try:
    for name,view,width in [('overview',4,1280),('compact',4,1000),('core-evidence',1,1280)]:
        bmp,report,state=[out/(name+suffix) for suffix in ('.bmp','.json','.state.json')]
        command=[str(exe),'--target',f'http://127.0.0.1:{server.server_port}/real','--autorun','--no-browser',
                 '--screenshot',str(bmp),'--report-json',str(report),'--ui-state',str(state),'--view',str(view),
                 '--width',str(width),'--height','800' if width==1280 else '700']
        if view==1:command+=['--filter','core-js']
        subprocess.run(command,check=True,timeout=30,env=env)
        r,s=json.loads(report.read_text()),json.loads(state.read_text())
        assert {'core-js','Twilio Segment'}<={f['name'] for f in r['findings']}
        assert s['navigation_visible'] and not s['error']
        if view==1:
            f=r['findings'][s['selected_finding']]
            assert f['name']=='core-js' and f['version']=='3.49.7' and len(f['evidence'])==2
        Image.open(bmp).save(out/(name+'.png'))
    name='cancelled';report=out/(name+'.json');bmp=out/(name+'.bmp')
    before=time.monotonic()
    p=subprocess.Popen([str(exe),'--target',f'http://127.0.0.1:{server.server_port}/deadline','--autorun',
        '--screenshot',str(bmp),'--report-json',str(report),'--cancel-after','2500'],env=env)
    assert p.wait(timeout=15)==0
    r=json.loads(report.read_text())
    assert r['status']=='cancelled' and r['browser']['state']=='cancelled'
    assert time.monotonic()-before<10
    assert not list(Path(tempfile.gettempdir()).glob(f'vector-browser-{p.pid}-*'))
    (out/'verification.json').write_text(json.dumps({'cases':4,'evidence_names':['core-js','Twilio Segment'],'cancellation':r['status']},indent=2))
    print('Four native consumer cases passed: both sizes, exact evidence and active cancellation.')
finally:
    server.shutdown();server.server_close()
