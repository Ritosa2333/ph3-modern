"""Play the unmodified packaged Windows executable for visual Replay evidence.

This executable has no read-only trace hook. Its screenshots are observations,
not a golden or a substitute for fixed-tick state comparison.
"""
from __future__ import annotations

import argparse
import ctypes as c
import importlib.util
import json
from pathlib import Path
import shutil
import subprocess
import time

from PIL import ImageGrab
from corpus import GAME, ROOT, inspect

spec = importlib.util.spec_from_file_location('native_replay_ui', Path(__file__).with_name('run-native.py'))
assert spec and spec.loader
ui = importlib.util.module_from_spec(spec)
spec.loader.exec_module(ui)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--fixture', default='Main_replay01.dat')
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--seconds', type=int, default=240)
    parser.add_argument('--interval', type=int, default=20)
    parser.add_argument('--fast', action='store_true', help='Hold the original Left Ctrl fast-play key')
    args = parser.parse_args()
    out = args.out.resolve()
    if not out.is_relative_to((ROOT / 'build').resolve()):
        raise ValueError('Official test copy must stay under repository build/')
    fixture = GAME / 'script/th_hsob/replay' / args.fixture
    if fixture.name != args.fixture or not fixture.is_file():
        raise ValueError('Unknown replay fixture')
    identity = inspect(fixture)
    out.mkdir(parents=True, exist_ok=True)
    game = out / 'game'
    def ignore(_directory: str, names: list[str]) -> list[str]:
        return [name for name in names if name.startswith('Main_replay') and name.endswith('.dat') and name != args.fixture]
    shutil.copytree(GAME, game, dirs_exist_ok=True, ignore=ignore)
    launcher = GAME.parent / 'launcher.ini'
    if launcher.is_file():
        shutil.copyfile(launcher, out / 'launcher.ini')
    executable = game / 'th_dnh.exe'
    import hashlib
    if hashlib.sha256((game / 'script/th_hsob/replay' / args.fixture).read_bytes()).hexdigest() != identity['sha256']:
        raise ValueError('Copied fixture differs from original')
    report = {'fixture': {'file': args.fixture, 'sha256': identity['sha256']},
              'officialExeSha256': hashlib.sha256(executable.read_bytes()).hexdigest(),
              'fast': args.fast, 'screens': []}
    try:
        ui.user.SetProcessDpiAwarenessContext(c.c_void_p(-4))
    except Exception:
        pass
    process = subprocess.Popen([str(executable)], cwd=game)
    hwnd = None
    held = False
    def hold_fast() -> None:
        nonlocal held
        if not args.fast:
            return
        scan = ui.user.MapVirtualKeyW(0xA2, 0)
        key = ui.INPUT(type=1, ki=ui.KEYBDINPUT(wScan=scan, dwFlags=8))
        if ui.user.SendInput(1, c.byref(key), c.sizeof(ui.INPUT)) != 1:
            raise c.WinError(c.get_last_error())
        held = True

    def screen(label: str) -> None:
        visible = ui.windows(process.pid)
        for number, window in enumerate(visible):
            if number == 0:
                ui.focus(window)
                # Keep the capture region on the desktop; Windows sometimes
                # restores the oversized legacy window partly off screen.
                ui.user.SetWindowPos(window, None, 0, 0, 0, 0, 0x0001 | 0x0004)
            rect = ui.w.RECT()
            ui.user.GetWindowRect(window, c.byref(rect))
            if rect.right <= rect.left or rect.bottom <= rect.top:
                continue
            title = c.create_unicode_buffer(256)
            ui.user.GetWindowTextW(window, title, len(title))
            path = out / f'{label}-{number}.png'
            ImageGrab.grab(bbox=(rect.left, rect.top, rect.right, rect.bottom)).save(path)
            report['screens'].append({'label': label, 'title': title.value,
                                      'rect': [rect.left, rect.top, rect.right, rect.bottom],
                                      'file': path.name})
        (out / 'report.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
        # Focusing a window can clear DirectInput's held-key state. Restore it
        # after every capture so the game's documented fast Replay stays held.
        hold_fast()
        print(json.dumps({'label': label, 'windows': visible}), flush=True)
    try:
        deadline = time.monotonic() + 60
        while time.monotonic() < deadline and process.poll() is None:
            visible = ui.windows(process.pid)
            if visible:
                hwnd = visible[0]
                break
            time.sleep(.2)
        if hwnd is None:
            raise RuntimeError('Official game window did not open')
        ui.focus(hwnd)
        time.sleep(18)
        screen('01-title')
        for _ in range(4):
            ui.press(hwnd, 0x28)
        ui.press(hwnd, 0x5a, 1.5)
        screen('02-replay-menu')
        ui.press(hwnd, 0x5a, 1.0)
        screen('03-selected')
        ui.press(hwnd, 0x5a, 2.0)
        screen('04-started')
        if args.fast:
            ui.focus(hwnd)
            hold_fast()
        started = time.monotonic()
        index = 0
        while time.monotonic() - started < args.seconds and process.poll() is None:
            time.sleep(args.interval)
            index += 1
            screen(f'play-{index:03d}')
        report['elapsedSeconds'] = time.monotonic() - started
    except Exception as error:
        report['failure'] = str(error)
    finally:
        if held:
            scan = ui.user.MapVirtualKeyW(0xA2, 0)
            key = ui.INPUT(type=1, ki=ui.KEYBDINPUT(wScan=scan, dwFlags=8 | 2))
            ui.user.SendInput(1, c.byref(key), c.sizeof(ui.INPUT))
        if hwnd and process.poll() is None:
            ui.user.PostMessageW(hwnd, 0x0010, 0, 0)
        try:
            process.wait(timeout=8)
        except subprocess.TimeoutExpired:
            process.terminate()
            process.wait(timeout=8)
        report['processExitCode'] = process.returncode
        (out / 'report.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
    print(json.dumps({k: report[k] for k in ('failure', 'elapsedSeconds', 'processExitCode') if k in report}), flush=True)


if __name__ == '__main__':
    main()
