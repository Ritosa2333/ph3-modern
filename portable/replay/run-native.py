"""Run the native source oracle with an isolated game copy and real keyboard input.

The original installation and replay bytes remain untouched. The only replay
selection aid is copying one fixture into the disposable replay directory.
"""
from __future__ import annotations
import argparse
import ctypes as c
from ctypes import wintypes as w
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import time

from PIL import ImageGrab
from corpus import GAME, ROOT, inspect

user = c.WinDLL('user32', use_last_error=True)
user.EnumWindows.argtypes = [c.WINFUNCTYPE(w.BOOL, w.HWND, w.LPARAM), w.LPARAM]
user.GetWindowThreadProcessId.argtypes = [w.HWND, c.POINTER(w.DWORD)]
user.GetWindowThreadProcessId.restype = w.DWORD
user.IsWindowVisible.argtypes = [w.HWND]
user.GetForegroundWindow.restype = w.HWND
user.SetForegroundWindow.argtypes = [w.HWND]
user.ShowWindow.argtypes = [w.HWND, c.c_int]
user.PostMessageW.argtypes = [w.HWND, w.UINT, w.WPARAM, w.LPARAM]
user.GetWindowRect.argtypes = [w.HWND, c.POINTER(w.RECT)]
user.GetWindowTextW.argtypes = [w.HWND, w.LPWSTR, c.c_int]
user.AttachThreadInput.argtypes = [w.DWORD, w.DWORD, w.BOOL]
user.BringWindowToTop.argtypes = [w.HWND]
user.MapVirtualKeyW.argtypes = [w.UINT, w.UINT]
user.MapVirtualKeyW.restype = w.UINT
ULONG_PTR = c.c_size_t


class MOUSEINPUT(c.Structure):
    _fields_ = [('dx', w.LONG), ('dy', w.LONG), ('mouseData', w.DWORD), ('dwFlags', w.DWORD), ('time', w.DWORD), ('dwExtraInfo', ULONG_PTR)]
class KEYBDINPUT(c.Structure):
    _fields_ = [('wVk', w.WORD), ('wScan', w.WORD), ('dwFlags', w.DWORD), ('time', w.DWORD), ('dwExtraInfo', ULONG_PTR)]
class HARDWAREINPUT(c.Structure):
    _fields_ = [('uMsg', w.DWORD), ('wParamL', w.WORD), ('wParamH', w.WORD)]
class INPUTUNION(c.Union):
    _fields_ = [('mi', MOUSEINPUT), ('ki', KEYBDINPUT), ('hi', HARDWAREINPUT)]
class INPUT(c.Structure):
    _anonymous_ = ('data',)
    _fields_ = [('type', w.DWORD), ('data', INPUTUNION)]
user.SendInput.argtypes = [w.UINT, c.POINTER(INPUT), c.c_int]
user.SendInput.restype = w.UINT


def windows(pid: int) -> list[int]:
    result = []
    @c.WINFUNCTYPE(w.BOOL, w.HWND, w.LPARAM)
    def callback(hwnd, _) -> bool:
        owner = w.DWORD()
        user.GetWindowThreadProcessId(hwnd, c.byref(owner))
        if owner.value == pid and user.IsWindowVisible(hwnd):
            result.append(hwnd)
        return True
    user.EnumWindows(callback, 0)
    return result


def focus(hwnd: int) -> None:
    user.ShowWindow(hwnd, 5)
    if user.GetForegroundWindow() != hwnd:
        user.SetForegroundWindow(hwnd)
        time.sleep(.15)
    if user.GetForegroundWindow() != hwnd:
        # Both windows are in this interactive desktop. Pair input queues only
        # for the focus transition, and always detach immediately afterwards.
        foreground = user.GetForegroundWindow()
        foreground_thread = user.GetWindowThreadProcessId(foreground, None)
        current_thread = c.windll.kernel32.GetCurrentThreadId()
        attached = foreground_thread != current_thread and user.AttachThreadInput(current_thread, foreground_thread, True)
        try:
            user.BringWindowToTop(hwnd)
            user.SetForegroundWindow(hwnd)
        finally:
            if attached:
                user.AttachThreadInput(current_thread, foreground_thread, False)
        time.sleep(.15)
    if user.GetForegroundWindow() != hwnd:
        raise RuntimeError('Oracle window cannot receive foreground keyboard input')


def press(hwnd: int, key: int, delay: float = .3) -> None:
    focus(hwnd)
    scan = user.MapVirtualKeyW(key, 0)
    flags = 8 | (1 if key in (0x25,0x26,0x27,0x28) else 0)
    down = INPUT(type=1, ki=KEYBDINPUT(wScan=scan, dwFlags=flags))
    up = INPUT(type=1, ki=KEYBDINPUT(wScan=scan, dwFlags=flags | 2))
    if user.SendInput(1, c.byref(down), c.sizeof(INPUT)) != 1:
        raise c.WinError(c.get_last_error())
    try:
        time.sleep(.12)
    finally:
        user.SendInput(1, c.byref(up), c.sizeof(INPUT))
    time.sleep(delay)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--fixture', default='Main_replay01.dat')
    parser.add_argument('--oracle', type=Path, default=ROOT/'build/native-verify/bin/DnhExecutor.exe')
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--seconds', type=int, default=0)
    parser.add_argument('--detail-tick', type=int, default=-1)
    parser.add_argument('--stages', type=int, default=0, help='Explicit prefix scope; zero selects the whole fixture')
    args = parser.parse_args()
    out = args.out.resolve()
    if not out.is_relative_to((ROOT/'build').resolve()):
        raise ValueError('Native test data must remain under repository build/')
    replay = GAME/'script/th_hsob/replay'/args.fixture
    if replay.name != args.fixture or not replay.is_file():
        raise ValueError('Unknown replay fixture')
    fixture = inspect(replay)
    stage_count = args.stages or len(fixture['stages'])
    if not 1 <= stage_count <= len(fixture['stages']):raise ValueError('Invalid stage prefix')
    out.mkdir(parents=True, exist_ok=True)
    game = out/'game'
    def ignore(directory: str, names: list[str]) -> list[str]:
        return [name for name in names if name.lower().endswith(('.exe','.lnk','.log')) or
                (name.startswith('Main_replay') and name.endswith('.dat') and name != args.fixture)]
    shutil.copytree(GAME, game, dirs_exist_ok=True, ignore=ignore)
    launcher = GAME.parent / 'launcher.ini'
    if launcher.is_file():
        shutil.copyfile(launcher, out / 'launcher.ini')
    executable = game/'th_dnh.exe'
    shutil.copyfile(args.oracle, executable)
    if hashlib.sha256((game/'script/th_hsob/replay'/args.fixture).read_bytes()).hexdigest() != fixture['sha256']:
        raise ValueError('Fixture changed during isolation')
    trace = out/'trace.raw.jsonl'
    env = dict(os.environ, WOO_TRACE_OUTPUT=str(trace), WOO_TRACE_STAGE_COUNT=str(stage_count),
               WOO_TRACE_DETAIL_TICK=str(args.detail_tick), WOO_ORACLE_BACKGROUND='1')
    report = {'fixture':fixture,'oracleSha256':hashlib.sha256(executable.read_bytes()).hexdigest(),
              'complete':False,'screens':[], 'requestedStages':stage_count, 'fullFixture':stage_count==len(fixture['stages']),
              'focusPolicy':'diagnostic background execution after real keyboard menu selection'}
    try:
        user.SetProcessDpiAwarenessContext(c.c_void_p(-4))
    except Exception:
        pass
    process = subprocess.Popen([str(executable)], cwd=game, env=env)
    hwnd = None
    def screen(label: str) -> None:
        visible = windows(process.pid)
        entries = []
        for i, window in enumerate(visible):
            rect=w.RECT();user.GetWindowRect(window,c.byref(rect))
            title=c.create_unicode_buffer(1024);user.GetWindowTextW(window,title,len(title))
            entries.append({'title':title.value,'rect':[rect.left,rect.top,rect.right,rect.bottom]})
            if rect.right>rect.left and rect.bottom>rect.top:
                ImageGrab.grab(bbox=(rect.left,rect.top,rect.right,rect.bottom)).save(out/f'{label}-{i}.png')
        report['screens'].append({'label':label,'windows':entries})
        (out/'report.json').write_text(json.dumps(report,ensure_ascii=True,indent=2),encoding='utf-8')
        print(json.dumps({'label':label,'windows':entries}),flush=True)
        if any(entry['title']=='ErrorWindow' for entry in entries):
            raise RuntimeError('Native engine displayed ErrorWindow; see captured error image')
    try:
        deadline=time.monotonic()+60
        while time.monotonic()<deadline and process.poll() is None:
            visible=windows(process.pid)
            if visible:
                hwnd=visible[0];break
            time.sleep(.2)
        if hwnd is None:
            raise RuntimeError(f'No native oracle window; exit={process.poll()}')
        focus(hwnd);time.sleep(18);screen('01-title')
        visible = windows(process.pid)
        if not visible:raise RuntimeError('Native window closed during initialization')
        hwnd = visible[0]
        for _ in range(4):press(hwnd,0x28)
        press(hwnd,0x5a,1.5);screen('02-replay-menu')
        press(hwnd,0x5a,1.0);screen('03-selected')
        press(hwnd,0x5a,2.0);screen('04-started')
        started=time.monotonic()
        deadline=started+(args.seconds or fixture['declaredFrames']/60*2+240)
        reader=None
        while time.monotonic()<deadline and process.poll() is None:
            for window in windows(process.pid):
                title=c.create_unicode_buffer(256);user.GetWindowTextW(window,title,len(title))
                if title.value == 'ErrorWindow':screen('native-error')
            if trace.is_file() and reader is None:reader=trace.open(encoding='utf-8')
            if reader:
                while True:
                    pos=reader.tell();line=reader.readline()
                    if not line or not line.endswith('\n'):
                        reader.seek(pos);break
                    event=json.loads(line)
                    if event['kind']=='end':
                        report.setdefault('stageResults',[]).append(event);print(json.dumps(event),flush=True)
                        expected = fixture['stages'][len(report['stageResults'])-1]
                        if (event['stage'] != expected['index'] or event['frame'] != expected['frameEnd']
                                or event['score'] != str(expected['scoreLast'])):
                            raise RuntimeError('Native replay diverged from its recorded stage result; not a golden')
                    if event['kind']=='run-end':report['complete']=True
            if report['complete']:break
            time.sleep(.5)
        if reader:reader.close()
        report['elapsedSeconds']=time.monotonic()-started
        screen('99-final')
        if not args.seconds and not report['complete']:
            raise RuntimeError('Native replay did not complete naturally')
    except Exception as error:
        report['failure']=str(error)
        if hwnd:
            try:screen('failure')
            except Exception:pass
    finally:
        if hwnd and process.poll() is None:
            user.PostMessageW(hwnd,0x0010,0,0)
        try:process.wait(timeout=8)
        except subprocess.TimeoutExpired:
            process.terminate();process.wait(timeout=8)
        report['processExitCode']=process.returncode
        (out/'report.json').write_text(json.dumps(report,ensure_ascii=True,indent=2),encoding='utf-8')
    print(json.dumps({k:report[k] for k in ('complete','failure','processExitCode') if k in report}),flush=True)
    if report.get('failure'):raise SystemExit(1)


if __name__=='__main__':main()
