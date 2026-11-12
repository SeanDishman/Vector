import json
from pathlib import Path
import subprocess
import sys
import time
import browser_integration as fixture

exe=Path(sys.argv[1]).resolve()
out=Path(sys.argv[2]).resolve()
out.mkdir(parents=True,exist_ok=True)


def capture(name,*args,path="/"):
    bmp=out/(name+".bmp")
    report=out/(name+".json")
    state=out/(name+".state.json")
    if name=="runtime-evidence":
        actions=[]
        for at,x,y in ((13000,110,343),):
            actions += [{"at":at,"kind":"focus"},{"at":at+40,"kind":"move","x":x,"y":y},
                        {"at":at+80,"kind":"down"},{"at":at+180,"kind":"up"}]
        action_file=out/(name+".actions.json")
        action_file.write_text(json.dumps(actions))
        args += ("--capture-delay","14500","--ui-actions",str(action_file))
    began=time.monotonic()
    subprocess.run([str(exe),"--target",fixture.url+path,"--autorun","--screenshot",str(bmp),"--report-json",str(report),"--ui-state",str(state),*args],check=True,timeout=40)
    data=json.loads(report.read_text())
    assert bmp.read_bytes()[:2]==b"BM"
    if name=="runtime-evidence":
        ui=json.loads(state.read_text())
        assert ui["show_all"] and ui["tab"]==2,ui
    subprocess.run(["ffmpeg","-y","-i",str(bmp),str(out/(name+".png")),"-loglevel","error"],check=True)
    return data,time.monotonic()-began


try:
    cases=(
        ("runtime-overview",()),
        ("runtime-compact",("--width","1000","--height","700")),
        ("runtime-evidence",()),
        ("runtime-vendors",("--view","3")),
        ("runtime-activity",("--activity-open",)),
    )
    for name,args in (() if "--cancel-only" in sys.argv else cases):
        r,_=capture(name,*args)
        assert r["browser"]["state"]=="complete",r["browser"]
        assert r["script_coverage"]["javascript_executed"]
        assert any(f["name"]=="lit-html" for f in r["findings"])
    cancelled,elapsed=capture("runtime-cancelled","--cancel-after","6500",path="/never")
    assert cancelled["status"]=="cancelled",cancelled["status"]
    assert cancelled["browser"]["state"]=="cancelled",cancelled["browser"]
    assert cancelled["script_coverage"]["javascript_executed"],"Cancellation happened before the browser opened the page"
    assert any(q["url"].endswith("/pending") for q in cancelled["browser"]["requests"]),cancelled["browser"]
    assert elapsed<10,elapsed
    assert any(e["item"].startswith("Browser ") for e in cancelled["activity"])
    print("Active browser cancellation passed." if "--cancel-only" in sys.argv else "6 runtime SDL scenarios passed: overview, compact, evidence, vendors, activity and browser cancellation.")
finally:
    fixture.page.shutdown()
    fixture.external.shutdown()
