"""Exercise the real browser shell and keyboard path; never patch game state."""
from __future__ import annotations
import argparse
import json
from pathlib import Path
from playwright.sync_api import sync_playwright

ROOT = Path(__file__).resolve().parents[1]

def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--url', default='http://127.0.0.1:8797/')
    parser.add_argument('--out', type=Path, default=ROOT / 'build/browser-smoke')
    parser.add_argument('--actions', default='')
    parser.add_argument('--initial-wait', type=int, default=12000)
    parser.add_argument('--step-wait', type=int, default=2200)
    parser.add_argument('--final-wait', type=int, default=6000)
    parser.add_argument('--gameplay-seconds', type=int, default=0)
    parser.add_argument('--verify-controls', action='store_true')
    args = parser.parse_args()
    args.out.mkdir(parents=True, exist_ok=True)
    report = {'url': args.url, 'pageErrors': [], 'consoleErrors': [], 'graphicsErrors': [], 'checkpoints': []}
    with sync_playwright() as playwright:
        browser = playwright.chromium.launch(channel='msedge', headless=True)
        page = browser.new_page(viewport={'width': 1100, 'height': 950})
        page.on('pageerror', lambda error: report['pageErrors'].append(str(error)))
        def console_event(event) -> None:
            if event.type == 'error':
                report['consoleErrors'].append(event.text)
            if any(term in event.text for term in ['GL_INVALID_', 'GL_OUT_OF_MEMORY', 'WebGL rendering error']):
                report['graphicsErrors'].append(event.text)
            with (args.out / 'console.log').open('a', encoding='utf-8') as stream:
                stream.write(event.type + ': ' + event.text + '\n')
        page.on('console', console_event)
        def checkpoint(label: str) -> bool:
            state = page.evaluate('window.ph3Diagnostics || {}')
            state['canvas'] = page.evaluate('''() => {
                const c=document.getElementById('canvas'),s=document.querySelector('.screen');
                // Never create a context before SDL chooses its attributes.
                const gl=globalThis.ph3Diagnostics?.presentedFrames?c.getContext('webgl2'):null;
                const info=gl?.getExtension('WEBGL_debug_renderer_info');
                return {width:c.width,height:c.height,clientWidth:c.clientWidth,clientHeight:c.clientHeight,
                    screenWidth:s.clientWidth,screenHeight:s.clientHeight,
                    viewport:gl?Array.from(gl.getParameter(gl.VIEWPORT)):null,
                    renderer:info?gl.getParameter(info.UNMASKED_RENDERER_WEBGL):null};
            }''')
            state['label'] = label
            state['status'] = page.locator('#status').inner_text()
            state['messages'] = state.get('messages', [])[-45:]
            report['checkpoints'].append(state)
            page.screenshot(path=str(args.out / (label + '.png')))
            (args.out / 'report.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
            print(json.dumps({k: v for k, v in state.items() if k != 'messages'}, ensure_ascii=True), flush=True)
            return state.get('phase') != 'error'
        try:
            page.goto(args.url, wait_until='domcontentloaded')
            page.locator('#start').click()
            page.wait_for_function("['ready','error'].includes(window.ph3Diagnostics.phase)", timeout=180000)
            if not checkpoint('01-loaded'):
                raise RuntimeError('Resource preparation failed')
            page.evaluate("ph3Diagnostics.observeStage=true")
            page.locator('#start').click()
            page.wait_for_timeout(args.initial_wait)
            if checkpoint('02-started'):
                for index, key in enumerate(filter(None, args.actions.split(',')), 3):
                    page.keyboard.down(key)
                    page.wait_for_timeout(150)
                    page.keyboard.up(key)
                    page.wait_for_timeout(args.step_wait)
                    if not checkpoint(f'{index:02d}-{key}'):
                        break
                page.wait_for_timeout(args.final_wait)
                healthy = checkpoint('90-settled')
                if healthy and args.gameplay_seconds:
                    page.wait_for_function("window.ph3Diagnostics.phase==='error' || window.ph3Diagnostics.stage?.frame>=250", timeout=90000)
                    stage = page.evaluate('window.ph3Diagnostics.stage')
                    if not stage or not stage['playerPresent']:
                        raise RuntimeError('The native stage/player did not start')
                    initial = dict(stage)
                    page.keyboard.down('ArrowLeft')
                    page.wait_for_timeout(350)
                    page.keyboard.up('ArrowLeft')
                    page.wait_for_timeout(100)
                    left = page.evaluate('window.ph3Diagnostics.stage')
                    checkpoint('91-moved-left')
                    if not left or left['x'] >= initial['x']:
                        raise RuntimeError('Keyboard movement did not move the native player left')
                    page.keyboard.down('ArrowRight')
                    page.wait_for_timeout(350)
                    page.keyboard.up('ArrowRight')
                    page.keyboard.down('z')
                    samples = []
                    for _ in range(min(120, args.gameplay_seconds)):
                        page.wait_for_timeout(1000)
                        sample = page.evaluate('''() => {const d=window.ph3Diagnostics;
                            return {phase:d.phase,stage:d.stage,peak:d.audioPeak,voices:d.audioVoices};}''')
                        samples.append(sample)
                        if sample['phase'] == 'error':
                            break
                    page.keyboard.up('z')
                    report['gameplay'] = {'initial': initial, 'left': left, 'samples': samples}
                    checkpoint('92-gameplay')
                    if not any(s.get('stage') and s['stage']['playerShots'] > 0 for s in samples):
                        raise RuntimeError('No real player projectiles observed while holding Z')
                    if not any(s.get('stage') and s['stage']['enemyShots'] > 0 for s in samples):
                        raise RuntimeError('No enemy projectiles observed during the stage run')
                    if not any(s.get('peak', 0) > 0 for s in samples):
                        raise RuntimeError('No nonzero game audio observed during the stage run')
                    if args.verify_controls:
                        async_js = "window.ph3Diagnostics.phase==='error' || window.ph3Diagnostics.stage?.paused"
                        page.keyboard.press('Escape', delay=150)
                        page.wait_for_function(async_js, timeout=15000)
                        paused = page.evaluate('window.ph3Diagnostics.stage')
                        page.wait_for_timeout(650)
                        paused_later = page.evaluate('window.ph3Diagnostics.stage')
                        checkpoint('93-paused')
                        if not paused or not paused['paused'] or paused['frame'] != paused_later['frame']:
                            raise RuntimeError('Native stage does not freeze while paused')
                        page.keyboard.press('z', delay=150)
                        page.wait_for_function("window.ph3Diagnostics.phase==='error' || (window.ph3Diagnostics.stage && !window.ph3Diagnostics.stage.paused)", timeout=15000)
                        page.wait_for_timeout(350)
                        resumed = page.evaluate('window.ph3Diagnostics.stage')
                        if not resumed or resumed['frame'] <= paused['frame']:
                            raise RuntimeError('Native stage did not resume from pause menu')
                        # A real hit/death may straddle pause and resume. Pressing
                        # X while STATE_DOWN is not a valid test of the bomb key.
                        # Wait for the original player's normal state; never
                        # change lives, invulnerability, or the stage state.
                        page.wait_for_function("window.ph3Diagnostics.phase==='error' || (window.ph3Diagnostics.stage?.playerState===0 && !window.ph3Diagnostics.stage.paused)", timeout=15000)
                        bomb_before = page.evaluate('window.ph3Diagnostics.stage')
                        before_bomb = bomb_before['spells']
                        page.keyboard.press('x', delay=180)
                        page.wait_for_timeout(1200)
                        bomb = page.evaluate('window.ph3Diagnostics.stage')
                        report['controls'] = {'paused': paused, 'pausedLater': paused_later,
                                              'resumed': resumed, 'bombBefore': bomb_before, 'bomb': bomb}
                        checkpoint('94-bomb')
                        if before_bomb > 0 and (not bomb or bomb['spells'] >= before_bomb):
                            raise RuntimeError('X did not consume a native spell')
                        page.locator('#fullscreen').click()
                        page.wait_for_timeout(600)
                        report['controls']['fullscreen'] = page.evaluate('!!document.fullscreenElement')
                        checkpoint('95-fullscreen')
                        if not report['controls']['fullscreen']:
                            raise RuntimeError('Fullscreen control did not enter fullscreen')
        except Exception as error:
            report['harnessError'] = str(error)
            try:
                checkpoint('99-failure')
            except Exception:
                pass
        finally:
            (args.out / 'report.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
            browser.close()
    last = report['checkpoints'][-1] if report['checkpoints'] else {}
    if report.get('harnessError') or report['pageErrors'] or report['graphicsErrors'] or last.get('errors') or last.get('phase') != 'running':
        raise SystemExit(1)

if __name__ == '__main__':
    main()
