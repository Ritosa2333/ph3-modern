"""Play one unmodified Woo replay through the original package menus.

Only the disposable browser filesystem is isolated to one replay. No game
state, player resources, scripts, RNG or fixture bytes are modified. A natural
stage end (not simply reaching a timeout/frame count) is required for coverage.
"""
from __future__ import annotations

import argparse
import json
from pathlib import Path
import time

from playwright.sync_api import sync_playwright
from corpus import GAME, ROOT, inspect


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--url', default='http://127.0.0.1:8797/')
    parser.add_argument('--fixture', default='Main_replay01.dat')
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--seconds', type=int, default=0, help='Explicit partial probe; never marked complete')
    parser.add_argument('--headed', action='store_true')
    parser.add_argument('--trace', action='store_true', help='Diagnostic build only; not a performance measurement')
    parser.add_argument('--fast-steps', type=int, default=0,
                        help='Diagnostic Replay only: original logical ticks per browser callback (1–32)')
    parser.add_argument('--detail-tick', type=int, default=-1)
    parser.add_argument('--stages', type=int, default=0, help='Explicit prefix scope; zero selects the whole fixture')
    parser.add_argument('--stop-at-frame', type=int, default=-1,
                        help='Diagnostic partial probe: stop after the observed stage reaches this logical frame')
    parser.add_argument('--perf-fast-steps', type=int, default=0,
                        help='Diagnostic PH3_PERF_SCAN build: logic ticks per browser callback (2-64)')
    parser.add_argument('--allow-mismatch', action='store_true',
                        help='Keep traversing natural replay boundaries after score/frame divergence')
    args = parser.parse_args()
    if not 0 <= args.fast_steps <= 32:
        raise ValueError('--fast-steps must be between 1 and 32, or omitted')
    if args.perf_fast_steps and not 2 <= args.perf_fast_steps <= 64:
        raise ValueError('perf-fast-steps must be 2..64')
    if args.fast_steps and args.perf_fast_steps:
        raise ValueError('--fast-steps and --perf-fast-steps are mutually exclusive')
    fixture_path = GAME / 'script/th_hsob/replay' / args.fixture
    if fixture_path.name != args.fixture or not fixture_path.is_file():
        raise ValueError('Choose an existing corpus replay basename')
    fixture = inspect(fixture_path)
    stage_count = args.stages or len(fixture['stages'])
    if not 1 <= stage_count <= len(fixture['stages']):raise ValueError('Invalid stage prefix')
    args.out.mkdir(parents=True, exist_ok=True)
    report = {'fixture': fixture, 'errors': [], 'consoleErrors': [], 'failedRequests': [],
              'checkpoints': [], 'completed': False, 'requestedStages':stage_count,
              'fullFixture':stage_count==len(fixture['stages']),
              'fastSteps':args.fast_steps, 'perfFastSteps':args.perf_fast_steps,
              'allowMismatch':args.allow_mismatch}
    with sync_playwright() as pw:
        browser = pw.chromium.launch(channel='msedge', headless=not args.headed)
        page = browser.new_page(viewport={'width': 1100, 'height': 1000})
        page.on('pageerror', lambda error: report['errors'].append(str(error)))
        page.on('requestfailed', lambda req: report['failedRequests'].append({'url': req.url, 'reason': req.failure}))

        def log(event) -> None:
            if event.type == 'error' or 'GL_INVALID_' in event.text:
                report['consoleErrors'].append(event.text)
            with (args.out / 'console.log').open('a', encoding='utf-8') as stream:
                stream.write(event.type + ': ' + event.text + '\n')
        page.on('console', log)
        trace = (args.out / 'trace.raw.jsonl').open('w', encoding='utf-8') if args.trace else None

        def drain_trace(flush: bool = False) -> None:
            if trace is None:
                return
            chunks = page.evaluate('''flush => {
                const r=window.ph3Runtime;if(!r)return [];
                if(flush && r._woo_trace_flush)r._woo_trace_flush();
                const chunks=r.wooTraceChunks||[];r.wooTraceChunks=[];r.wooTraceBytes=0;return chunks;
            }''', flush)
            for chunk in chunks:
                trace.write(chunk)
            trace.flush()

        def save() -> None:
            (args.out / 'report.json').write_text(json.dumps(report, ensure_ascii=True, indent=2), encoding='utf-8')

        def checkpoint(label: str) -> dict:
            drain_trace()
            state = page.evaluate('() => {const {messages,...d}=ph3Diagnostics;return d;}')
            report['checkpoints'].append({'label': label, 'state': state})
            page.screenshot(path=str(args.out / (label + '.png')))
            save()
            print(json.dumps({'label': label, 'phase': state.get('phase'), 'stage': state.get('stage')}, ensure_ascii=True), flush=True)
            if state.get('phase') == 'error':
                raise RuntimeError(str(state.get('errors')))
            return state

        def press(key: str, settle: int = 250) -> None:
            page.keyboard.press(key, delay=100)
            page.wait_for_timeout(settle)

        try:
            page.goto(args.url, wait_until='domcontentloaded')
            report['build'] = page.evaluate("async() => await (await fetch('./build.json')).json()")
            if args.trace and report['build'].get('variant') != 'verify':
                raise RuntimeError('Trace requested against a non-verifier build')
            if args.fast_steps and report['build'].get('variant') not in ('verify', 'boundary'):
                raise RuntimeError('Fast Replay requested against a production build')
            page.locator('#start').click()
            page.wait_for_function("['ready','error'].includes(ph3Diagnostics.phase)", timeout=180000)
            checkpoint('00-ready')
            # Replay automation needs fresh native stage state, but it is not a
            # performance run. Opt into the low-frequency stage observer only;
            # keep per-frame performance observation disabled so the harness
            # does not perturb playback timing.
            page.evaluate('''() => {
                ph3Diagnostics.observeStage = true;
                ph3Diagnostics.observePerformance = false;
            }''')
            # These are in-memory copies in a fresh browser context, never disk
            # originals. One fixture avoids relying on user menu-memory indices.
            page.evaluate(r'''name => {
                const fs=ph3Runtime.FS, root='/game/script/th_hsob/replay';
                for(const file of fs.readdir(root)) {
                    if(file!==name && /^Main_replay\d+\.dat$/.test(file)) fs.unlink(root+'/'+file);
                }
                if(!fs.analyzePath(root+'/'+name).exists) throw new Error('Fixture not mounted');
            }''', args.fixture)
            if args.trace:
                page.evaluate('count => {ph3Runtime.wooTraceEnabled=true;ph3Runtime.wooTraceStageCount=count;}', stage_count)
                page.evaluate('tick => {ph3Runtime.wooTraceDetailTick=tick;}', args.detail_tick)
            page.locator('#start').click()
            page.wait_for_function("ph3Diagnostics.phase==='error'||ph3Diagnostics.messages.some(s=>s.includes('title00b3.png'))", timeout=180000)
            page.wait_for_timeout(1600)
            checkpoint('01-title')
            for _ in range(4):
                press('ArrowDown')
            press('z', 1800)
            checkpoint('02-replay-menu')
            press('z', 1000)
            checkpoint('03-replay-selected')
            for _ in range(4):
                if page.evaluate('!!ph3Diagnostics.stage?.replay'):
                    break
                # Slow diagnostic builds can leave the replay selection menu
                # open after one short key press. Restore canvas focus and
                # retry the original confirm input without altering game data.
                page.locator('#canvas').focus()
                press('z', 1200)
                page.wait_for_timeout(2000)
            page.wait_for_function("ph3Diagnostics.phase==='error'||ph3Diagnostics.stage?.replay", timeout=180000)
            state = checkpoint('04-playing')
            if not state.get('stage', {}).get('replay'):
                raise RuntimeError('Did not enter native replay mode')
            if args.fast_steps:
                page.evaluate('count => {ph3Runtime.wooReplayFastSteps=count;}', args.fast_steps)
            if args.perf_fast_steps:
                page.evaluate('steps => {ph3Runtime.ph3PerfFastSteps=steps;}', args.perf_fast_steps)
            started = time.monotonic()
            speed = args.perf_fast_steps or args.fast_steps or 1
            deadline = started + (args.seconds or fixture['declaredFrames'] / 60 / speed * 4 + 240)
            last_boundary_count = -1
            checked_ends = 0
            with (args.out / 'samples.jsonl').open('w', encoding='utf-8') as samples:
                while time.monotonic() < deadline:
                    page.wait_for_timeout(1000)
                    drain_trace()
                    state = page.evaluate('() => {const {messages,...d}=ph3Diagnostics;return {time:performance.now(),...d};}')
                    samples.write(json.dumps(state, ensure_ascii=True) + '\n')
                    samples.flush()
                    if state.get('phase') == 'error':
                        raise RuntimeError(str(state.get('errors')))
                    if args.stop_at_frame >= 0 and state.get('stage', {}).get('frame', -1) >= args.stop_at_frame:
                        break
                    boundaries = state.get('replayBoundaries', [])
                    if len(boundaries) != last_boundary_count:
                        last_boundary_count = len(boundaries)
                        report['boundaries'] = boundaries
                        checkpoint(f'boundary-{len(boundaries):03d}')
                    ends = [e for e in boundaries if e['kind'] == 'end']
                    for end in ends[checked_ends:]:
                        result = dict(end, recordedScoreMatches=end['score']==end['expectedScore'],
                                           recordedFrameMatches=end['frame']==end['expectedFrame'])
                        report.setdefault('stageResults', []).append(result)
                        checked_ends += 1
                        save()
                        if (not args.allow_mismatch and
                                (not result['recordedScoreMatches'] or not result['recordedFrameMatches'])):
                            raise RuntimeError(f"First replay mismatch at stage {end['index']}: "
                                               f"frame {end['frame']}/{end['expectedFrame']}, "
                                               f"score {end['score']}/{end['expectedScore']}")
                    if len(ends) >= stage_count:
                        report['completed'] = True
                        break
            report['elapsedSeconds'] = time.monotonic() - started
            drain_trace(True)
            report['final'] = checkpoint('99-final')
            ends = [e for e in report['final'].get('replayBoundaries', []) if e['kind'] == 'end']
            report['stageResults'] = [dict(end, recordedScoreMatches=end['score']==end['expectedScore'],
                                              recordedFrameMatches=end['frame']==end['expectedFrame']) for end in ends]
            if not args.seconds and not report['completed']:
                raise RuntimeError('Replay did not reach all natural stage-end boundaries')
        except Exception as error:
            report['failure'] = str(error)
            try:
                report['diagnostics'] = page.evaluate('window.ph3Diagnostics')
                page.screenshot(path=str(args.out / 'failure.png'))
            except Exception:
                pass
        finally:
            if trace is not None:
                try:
                    drain_trace(True)
                except Exception as error:
                    report['traceFlushError'] = str(error)
                trace.close()
            save()
            browser.close()
    print(json.dumps({k: report[k] for k in ('completed','failure','stageResults') if k in report}), flush=True)
    if report.get('failure') or report['errors'] or report['consoleErrors'] or report['failedRequests']:
        raise SystemExit(1)


if __name__ == '__main__':
    main()
