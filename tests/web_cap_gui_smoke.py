import json
from pathlib import Path
import subprocess
import sys
from web_cap_integration import start

exe=Path(sys.argv[1]).resolve()
out=Path(sys.argv[2]).resolve()
out.mkdir(parents=True,exist_ok=True)
server=start()
target=f"http://127.0.0.1:{server.server_port}/many"
try:
    for name,args in (
        ("cap-overview",()),
        ("cap-compact",("--width","1000","--height","700")),
        ("cap-evidence",("--view","2")),
        ("cap-live",("--capture-stage","web")),
    ):
        bmp=out/(name+".bmp")
        report=out/(name+".json")
        current_target=target.rsplit("/",1)[0]+"/slow" if name=="cap-live" else target
        subprocess.run([str(exe),"--no-browser","--target",current_target,"--autorun","--screenshot",str(bmp),"--report-json",str(report),*args],check=True,timeout=20)
        data=json.loads(report.read_text())
        assert bmp.read_bytes()[:2]==b"BM"
        subprocess.run(["ffmpeg","-y","-i",str(bmp),str(out/(name+".png")),"-loglevel","error"],check=True)
        if name=="cap-live":
            assert data["status"]=="running"
            assert any(e["stage"]=="web" and not e["item"] and e["total"]==12 for e in data["activity"])
        else:
            assert data["web_coverage"]["limited"] and data["web_coverage"]["page_requests"]==12,data["web_coverage"]
            assert any(f["name"]=="nginx" for f in data["findings"])
    print("4 SDL web-cap scenarios passed: overview, compact, evidence and live cap counter.")
finally:
    server.shutdown()
