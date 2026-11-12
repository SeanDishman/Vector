import ctypes
from ctypes import wintypes
import json
from pathlib import Path
import subprocess
import sys
import time

exe = Path(sys.argv[1]).resolve()
out = Path(sys.argv[2]).resolve()
out.mkdir(parents=True, exist_ok=True)
u = ctypes.windll.user32
u.SetProcessDpiAwarenessContext(ctypes.c_void_p(-4))
u.SendMessageW.argtypes = [wintypes.HWND, wintypes.UINT, wintypes.WPARAM, wintypes.LPARAM]
u.SendMessageW.restype = ctypes.c_ssize_t
u.SetWindowPos.argtypes = [wintypes.HWND, wintypes.HWND, ctypes.c_int, ctypes.c_int, ctypes.c_int, ctypes.c_int, wintypes.UINT]


def find_window(pid):
    callback = ctypes.WINFUNCTYPE(wintypes.BOOL, wintypes.HWND, wintypes.LPARAM)
    deadline = time.monotonic() + 8
    while time.monotonic() < deadline:
        found = []
        @callback
        def visit(hwnd, _):
            owner = wintypes.DWORD()
            u.GetWindowThreadProcessId(hwnd, ctypes.byref(owner))
            title = ctypes.create_unicode_buffer(128)
            u.GetWindowTextW(hwnd, title, 128)
            if owner.value == pid and title.value.startswith("Vector"):
                found.append(hwnd)
            return True
        u.EnumWindows(visit, 0)
        if found:
            time.sleep(.3)
            return found[0]
        time.sleep(.05)
    raise AssertionError("SDL window missing")


def click(x, y, at=450):
    return [{"at": at, "kind": "focus"}, {"at": at+40, "kind": "move", "x": x, "y": y},
            {"at": at+80, "kind": "down"}, {"at": at+180, "kind": "up"}]


def run(name, actions, inspect=None, closes=False):
    script = out / f"{name}.actions.json"
    state = out / f"{name}.state.json"
    script.write_text(json.dumps(actions))
    process = subprocess.Popen([str(exe), "--screenshot", str(out / f"{name}.bmp"),
                                "--visible-capture", "--capture-delay", "2300",
                                "--ui-state", str(state), "--ui-actions", str(script)])
    try:
        if inspect:
            inspect(find_window(process.pid))
        assert process.wait(timeout=10) == 0, name
        if closes:
            assert not state.exists(), "Close must exit before the delayed capture"
            return None
        return json.loads(state.read_text())
    finally:
        if process.poll() is None:
            process.terminate()


def geometry(hwnd):
    rect = wintypes.RECT()
    u.GetWindowRect(hwnd, ctypes.byref(rect))
    def hit(x, y):
        px, py = rect.left+x, rect.top+y
        return u.SendMessageW(hwnd, 0x84, 0, ((py & 0xffff) << 16) | (px & 0xffff))
    assert hit(100, 25) == 2, "Header must be a native drag target"
    assert hit(1, 200) == 10, "Left edge must resize"
    assert hit(1279, 200) == 11, "Right edge must resize"
    assert hit(1200, 25) == 1, "Caption buttons must receive client input"
    corner = ctypes.c_uint()
    dwm = ctypes.windll.dwmapi
    dwm.DwmGetWindowAttribute.argtypes = [wintypes.HWND, wintypes.DWORD, ctypes.c_void_p, wintypes.DWORD]
    rounded = dwm.DwmGetWindowAttribute(hwnd, 33, ctypes.byref(corner), ctypes.sizeof(corner)) == 0 and corner.value == 2
    if not rounded:
        gdi = ctypes.windll.gdi32
        gdi.CreateRectRgn.restype = wintypes.HANDLE
        gdi.PtInRegion.argtypes = [wintypes.HANDLE, ctypes.c_int, ctypes.c_int]
        gdi.DeleteObject.argtypes = [wintypes.HANDLE]
        u.GetWindowRgn.argtypes = [wintypes.HWND, wintypes.HANDLE]
        region = gdi.CreateRectRgn(0, 0, 0, 0)
        try:
            assert u.GetWindowRgn(hwnd, region) > 1
            assert not gdi.PtInRegion(region, 0, 0) and gdi.PtInRegion(region, 20, 20)
        finally:
            gdi.DeleteObject(region)
    assert u.SetWindowPos(hwnd, None, 0, 0, 1000, 700, 0x0002 | 0x0004 | 0x0010)


state = run("window-resize", [], inspect=geometry)
assert state["display"] == [1000, 700], state
state = run("window-maximize", click(1200, 27))
assert state["window_flags"] & 0x80, state
max_width = int(state["display"][0])
state = run("window-restore", click(1200, 27) + click(max_width-80, 27, at=1100))
assert not state["window_flags"] & 0x80 and state["display"] == [1280, 800], state
state = run("window-minimize", click(1156, 27))
assert state["window_flags"] & 0x40, state
run("window-close", click(1244, 27), closes=True)
print("Five native window checks passed: rounded geometry, drag/resize hits, resize, maximize/restore, minimize and close.")
