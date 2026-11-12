import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile

package = Path(sys.argv[1]).resolve()
env = os.environ.copy()
env["PATH"] = str(Path(os.environ["SystemRoot"]) / "System32")
with tempfile.TemporaryDirectory() as temp:
    cli = subprocess.run([str(package / "vector-cli.exe"), "--demo"], cwd=temp, env=env, capture_output=True, text=True, timeout=10)
    assert cli.returncode == 0, cli.stderr
    assert json.loads(cli.stdout)["status"] == "demo"
    capture = Path(temp) / "packaged.bmp"
    gui = subprocess.run([str(package / "Vector.exe"), "--demo", "--screenshot", str(capture)], cwd=temp, env=env, timeout=15)
    assert gui.returncode == 0 and capture.stat().st_size > 100000
    assert capture.read_bytes()[:2] == b"BM"
print("Packaged CLI and SDL GUI launch with only Windows System32 on PATH.")
