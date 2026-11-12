import json
from pathlib import Path
import subprocess
import sys
from coverage_integration import start

exe=Path(sys.argv[1]).resolve();out=Path(sys.argv[2]).resolve();out.mkdir(parents=True,exist_ok=True)
server=start('/stack')
try:
    for name,path,args in (('production-react','/react-dom',()),('browser-rejection','/denied',('--view','2'))):
        bmp=out/(name+'.bmp');report=out/(name+'.json')
        subprocess.run([str(exe),'--target',f'http://127.0.0.1:{server.server_port}{path}','--autorun',
                        '--screenshot',str(bmp),'--report-json',str(report),*args],check=True,timeout=35)
        r=json.loads(report.read_text());assert r['status']=='complete'
        if name=='production-react':
            assert any(f['name']=='React' and any('__reactFiber$' in e['value'] for e in f['evidence']) for f in r['findings'])
        else: assert r['browser']['state']=='http_error' and r['browser']['pages'][0]['status']==403
        subprocess.run(['ffmpeg','-y','-i',str(bmp),str(out/(name+'.png')),'-loglevel','error'],check=True)
    print('Two native coverage scenarios passed: actual production React and explicit HTTP rejection.')
finally: server.shutdown()
