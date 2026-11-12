import argparse
import errno
import json
from pathlib import Path
import socket
import subprocess
import threading
import time

def fixture(mode):
    server=socket.socket(socket.AF_INET,socket.SOCK_DGRAM)
    for octet in range(2,34):
        try:server.bind((f'127.0.1.{octet}',1900));break
        except OSError as e:
            if e.errno!=errno.EADDRINUSE and getattr(e,'winerror',None)!=10048:raise
    else:raise RuntimeError('No free loopback fixture address')
    server.settimeout(.1)
    stop=threading.Event();hits=[]
    root=b'HTTP/1.1 200 OK\r\nSERVER: Linux/5.15 UPnP/1.1\r\nST: upnp:rootdevice\r\n\r\n'
    gateway=b'HTTP/1.1 200 OK\r\nST: urn:schemas-upnp-org:device:InternetGatewayDevice:2\r\n\r\n'
    def run():
        while not stop.is_set():
            try:data,peer=server.recvfrom(2048)
            except socket.timeout:continue
            except OSError:return
            hits.append(data)
            try:
                server.sendto(root,peer)
                if mode=='late':
                    if stop.wait(.95):return
                    server.sendto(gateway,peer)
                elif mode=='wrong-sender':
                    with socket.socket(socket.AF_INET,socket.SOCK_DGRAM) as other:
                        other.bind(('127.0.0.3',0));other.sendto(gateway,peer)
                elif mode=='burst':
                    for i in range(15):server.sendto(root.replace(b'upnp:rootdevice',f'uuid:fixture-{i}'.encode()),peer)
            except OSError:return
    thread=threading.Thread(target=run,daemon=True);thread.start()
    return server,stop,thread,hits

def inspect(exe,mode,gui=None,name=None,out=None,extra=()):
    server,stop,thread,hits=fixture(mode)
    address=server.getsockname()[0]
    began=time.monotonic()
    try:
        if gui:
            report=out/(name+'.json');capture=out/(name+'.bmp')
            args=[str(gui),'--target',address,'--autorun','--no-browser','--screenshot',str(capture),'--report-json',str(report),*extra]
        else:args=[str(exe),'--target',address,'--no-browser','--timeout','1600','--http-timeout','1000']
        p=subprocess.run(args,capture_output=True,text=True,timeout=95)
        assert p.returncode==0,(p.returncode,p.stderr)
        r=json.loads(report.read_text() if gui else p.stdout)
        assert 1 <= len(hits) <= 2 and len(set(hits)) == len(hits), hits
        allowed = {b'ssdp:all', b'urn:schemas-upnp-org:device:InternetGatewayDevice:1'}
        for request in hits:
            assert request.startswith(b'M-SEARCH'), request
            assert request.split(b'\r\nST: ', 1)[1].split(b'\r\n', 1)[0] in allowed, request
        if not gui:assert time.monotonic()-began<85,'inspection did not stay bounded'
        discovery=[e for e in r['activity'] if e['stage']=='discovery' and e['item']=='SSDP / UDP 1900']
        if len(discovery)>1:
            assert discovery[-1]['elapsed_ms']-discovery[0]['elapsed_ms']<1900,'SSDP exceeded its collection window'
        return r
    finally:
        stop.set();server.close();thread.join(2)

if __name__=='__main__':
    ap=argparse.ArgumentParser();ap.add_argument('exe',type=Path);ap.add_argument('--expect-missing',action='store_true')
    ap.add_argument('--gui',type=Path);ap.add_argument('--out',type=Path,default=Path('.impeccable/review/router'))
    args=ap.parse_args();exe=args.exe.resolve();args.out.mkdir(parents=True,exist_ok=True)
    report=inspect(exe,'late')
    (args.out/('old-late.json' if args.expect_missing else 'late.json')).write_text(json.dumps(report,indent=2))
    found=[f for f in report['findings'] if f['name']=='Router / gateway']
    if args.expect_missing:
        assert not found,'Baseline unexpectedly already handles the case'
        print('Previous package misses the delayed gateway advertisement after the first root-device response.')
        raise SystemExit(0)
    assert len(found)==1 and found[0]['evidence'][0]['value'].endswith('InternetGatewayDevice:2'),found
    assert any(e['stage']=='discovery' and '2 datagrams' in e['detail'] for e in report['activity'])
    print('Delayed gateway scenario passed.',flush=True)
    for mode in ('generic','wrong-sender','burst'):
        report=inspect(exe,mode)
        assert not any(f['category']=='Device hint' for f in report['findings']),report['findings']
        if mode=='burst':assert any('8 datagrams' in e['detail'] for e in report['activity'])
        print(mode+' scenario passed.',flush=True)
    if args.gui:
        discovery_started=0
        for name,extra in [('overview',()),('compact-evidence',('--view','1','--filter','gateway','--width','1000','--height','700'))]:
            report=inspect(exe,'late',args.gui.resolve(),name,args.out,extra)
            assert any(f['name']=='Router / gateway' for f in report['findings'])
            discovery_started=next(e['elapsed_ms'] for e in report['activity'] if e['stage']=='discovery' and e['item']=='SSDP / UDP 1900' and e['state']=='running')
            subprocess.run(['ffmpeg','-y','-i',str(args.out/(name+'.bmp')),str(args.out/(name+'.png')),'-loglevel','error'],check=True)
        report=inspect(exe,'late',args.gui.resolve(),'cancelled',args.out,('--cancel-after',str(int(discovery_started)+500),'--view','5'))
        assert report['status']=='cancelled',report['status']
        assert any(e['stage']=='discovery' and e['state']=='cancelled' for e in report['activity']),report['activity']
    print('Four UDP scenarios passed: delayed gateway, generic-only, wrong sender, and eight-response cap.'+(' Two SDL captures and active cancellation passed.' if args.gui else ''))
