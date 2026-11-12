import json
from pathlib import Path
import subprocess
import sys
from disclosure_integration import start

exe=Path(sys.argv[1]).resolve();out=Path(sys.argv[2]).resolve();out.mkdir(parents=True,exist_ok=True)
server=start()
try:
    cases=[('google-providers',[]),('google-providers-compact',['--width','1000','--height','700']),
           ('google-provider-evidence',['--view','3','--vendor-filter','Accenture International Limited']),
           ('google-provider-evidence-compact',['--view','3','--vendor-filter','Accenture International Limited','--width','1000','--height','700']),
           ('google-provider-sources',['--view','2'])]
    for name,args in cases:
        bmp=out/(name+'.bmp');report=out/(name+'.json')
        subprocess.run([str(exe),'--target',f'http://127.0.0.1:{server.server_port}/google','--autorun','--no-browser',
                        '--screenshot',str(bmp),'--report-json',str(report),*args],check=True,timeout=40)
        r=json.loads(report.read_text(encoding='utf-8'))
        providers=[v for v in r['vendors'] if v['published_by']]
        assert len(providers)==169
        assert all(v['evidence_level']=='Published disclosure' and v['published_by']=='127.0.0.1' for v in providers)
        selected=next(v for v in providers if v['name']=='Accenture International Limited')
        assert len(selected['evidence'])==4
        assert any('Chrome Enterprise' in e['value'] for e in selected['evidence'])
        assert any('AppSheet' in e['value'] for e in selected['evidence'])
        subprocess.run(['ffmpeg','-y','-i',str(bmp),str(out/(name+'.png')),'-loglevel','error'],check=True)
    print('Five Google provider SDL scenarios passed with 169 factual names, source rows and service scopes on loopback.')
finally:server.shutdown()
