import json
from pathlib import Path
import subprocess
import sys
from disclosure_integration import start
exe=Path(sys.argv[1]).resolve();out=Path(sys.argv[2]).resolve();out.mkdir(parents=True,exist_ok=True)
server=start()
try:
    actions=[{'at':2000,'kind':'focus'},{'at':2040,'kind':'move','x':306,'y':498},{'at':2080,'kind':'down'},{'at':2180,'kind':'up'}]
    action_file=out/'provider-click.actions.json';action_file.write_text(json.dumps(actions))
    state=out/'provider-click.state.json'
    for name,args in [('overview',[]),('compact',['--width','1000','--height','700']),('provider',['--view','3']),('provider-compact',['--view','3','--width','1000','--height','700']),('sources',['--view','2']),('provider-click',['--no-browser','--capture-delay','3500','--ui-actions',str(action_file),'--ui-state',str(state)])]:
        bmp=out/(name+'.bmp');report=out/(name+'.json')
        subprocess.run([str(exe),'--target',f'http://127.0.0.1:{server.server_port}/','--autorun','--screenshot',str(bmp),'--report-json',str(report),*args],check=True,timeout=35)
        data=json.loads(report.read_text());assert sum(bool(v['published_by']) for v in data['vendors'])==42
        assert data['disclosures'][0]['state']=='collected'
        if name=='provider-click':
            ui=json.loads(state.read_text());assert ui['show_all'] and ui['tab']==3 and ui['selected_vendor']>=0,ui
        subprocess.run(['ffmpeg','-y','-i',str(bmp),str(out/(name+'.png')),'-loglevel','error'],check=True)
    print('6 SDL disclosure scenarios passed: overview and provider at two sizes, source coverage and provider-row navigation.')
finally:server.shutdown()
