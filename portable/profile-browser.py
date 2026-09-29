"""Profile the original Stage 1 through keyboard input, without changing game state.

Use --cpu for a separate sampled CPU run; the default measures unprofiled rAF
latency and native logic progress. A draw counter is not a presented-frame rate.
"""
from __future__ import annotations
import argparse
from collections import Counter
import json
from pathlib import Path
import time
from playwright.sync_api import sync_playwright

ROOT = Path(__file__).resolve().parents[1]
PROBE = r"""(() => {
  const p = window.wooPerf = {active:false, callbacks:[], gl:{}, previous:0};
  const request = window.requestAnimationFrame.bind(window);
  window.requestAnimationFrame = callback => request(timestamp => {
    if(!p.active) return callback(timestamp);
    const before=performance.now(), d=window.ph3Diagnostics;
    const frames=d?.frames||0, presents=d?.presentedFrames||0;
    try { return callback(timestamp); }
    finally {
      p.callbacks.push({timestamp,gap:p.previous ? timestamp-p.previous : 0,
        cpu:performance.now()-before, steps:(d?.frames||0)-frames,
        presents:(d?.presentedFrames||0)-presents, stage:d?.stage?.frame??null});
      p.previous=timestamp;
    }
  });
})();"""

def percentile(values: list[float], p: float) -> float:
    ordered=sorted(values)
    return ordered[min(len(ordered)-1, int((len(ordered)-1)*p))] if ordered else 0

def cpu_summary(profile: dict) -> dict:
    nodes={n['id']:n for n in profile['nodes']}
    parent={child:n['id'] for n in profile['nodes'] for child in n.get('children', [])}
    exclusive, inclusive=Counter(), Counter()
    total=sum(profile.get('timeDeltas', [])) or 1
    for node, duration in zip(profile.get('samples', []), profile.get('timeDeltas', [])):
        name=nodes[node]['callFrame']['functionName']
        exclusive[name]+=duration
        visited=set()
        while node in nodes:
            name=nodes[node]['callFrame']['functionName']
            if name not in visited: inclusive[name]+=duration; visited.add(name)
            node=parent.get(node)
    return {kind:[{'name':name,'percent':round(100*us/total,2),'ms':round(us/1000,2)}
                  for name,us in counts.most_common(35)]
            for kind,counts in [('exclusive',exclusive),('inclusive',inclusive)]}

def main() -> None:
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--url', default='http://127.0.0.1:8797/')
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--seconds', type=int, default=15)
    parser.add_argument('--cpu', action='store_true')
    parser.add_argument('--require-smooth', action='store_true',
                        help='Fail if the steady Stage 1 window misses the 60 Hz performance budget')
    parser.add_argument('--headed', action='store_true')
    args=parser.parse_args()
    args.out.mkdir(parents=True, exist_ok=True)
    report={'cpuProfiling':args.cpu,'errors':[], 'consoleErrors':[], 'samples':[]}
    with sync_playwright() as pw:
        browser=pw.chromium.launch(channel='msedge',headless=not args.headed)
        page=browser.new_page(viewport={'width':1100,'height':950})
        page.add_init_script(PROBE)
        page.on('pageerror',lambda e:report['errors'].append(str(e)))
        def console(event):
            if event.type=='error' or 'GL_INVALID_' in event.text:
                report['consoleErrors'].append(event.text)
            with (args.out/'console.log').open('a',encoding='utf-8') as stream:
                stream.write(event.type+': '+event.text+'\n')
        page.on('console',console)
        try:
            page.goto(args.url,wait_until='domcontentloaded')
            report['build']=page.evaluate("async()=>await (await fetch('./build.json')).json()")
            # Stage/object diagnostics are an explicit test observer. Keep them
            # on only while navigating to the measurement window; the actual
            # performance sample must measure the game, not the observer.
            page.evaluate("ph3Diagnostics.observeStage=true")
            page.locator('#start').click()
            page.wait_for_function("['ready','error'].includes(window.ph3Diagnostics.phase)",timeout=180000)
            if page.evaluate("ph3Diagnostics.phase")!='ready':raise RuntimeError('Asset preparation failed')
            page.locator('#start').click()
            page.wait_for_timeout(12000)
            for _ in range(4):
                page.keyboard.press('z',delay=150)
                page.wait_for_timeout(2500)
            page.wait_for_function("ph3Diagnostics.phase==='error'||ph3Diagnostics.stage?.frame>=280",timeout=180000)
            if page.evaluate("ph3Diagnostics.phase")=='error':raise RuntimeError('Stage startup failed')
            page.evaluate("ph3Diagnostics.observeStage=false")
            page.screenshot(path=str(args.out/'start.png'))
            report['renderer']=page.evaluate("""() => {const g=document.getElementById('canvas').getContext('webgl2');
                const e=g.getExtension('WEBGL_debug_renderer_info');return e?g.getParameter(e.UNMASKED_RENDERER_WEBGL):g.getParameter(g.RENDERER);}""")
            session=page.context.new_cdp_session(page)
            if args.cpu:
                session.send('Profiler.enable');session.send('Profiler.setSamplingInterval',{'interval':1000});session.send('Profiler.start')
            page.keyboard.down('z')
            report['start']=page.evaluate("() => {wooPerf.active=true;return {now:performance.now(),...ph3Diagnostics};}")
            deadline=time.monotonic()+max(1,min(args.seconds,120))
            while time.monotonic()<deadline:
                page.wait_for_timeout(1000)
                sample=page.evaluate("() => ({now:performance.now(),frames:ph3Diagnostics.frames,stage:ph3Diagnostics.stage,phase:ph3Diagnostics.phase,perf:ph3Diagnostics.performance})")
                report['samples'].append(sample)
                if sample['phase']=='error':break
            report['end']=page.evaluate("() => {wooPerf.active=false;return {now:performance.now(),...ph3Diagnostics};}")
            if args.cpu:
                profile=session.send('Profiler.stop')['profile']
                (args.out/'cpu.cpuprofile').write_text(json.dumps(profile),encoding='utf-8')
                report['cpu']=cpu_summary(profile)
            page.keyboard.up('z')
            report['callbacks']=page.evaluate('wooPerf.callbacks')
            elapsed=(report['end']['now']-report['start']['now'])/1000
            calls=report['callbacks'];cpu=[c['cpu'] for c in calls];gaps=[c['gap'] for c in calls if c['gap']>0]
            report['summary']={'seconds':elapsed,'logicHz':(report['end']['frames']-report['start']['frames'])/elapsed,
                'rafHz':len(calls)/elapsed,'callbackMsP50':percentile(cpu,.5),'callbackMsP95':percentile(cpu,.95),
                'gapMsP50':percentile(gaps,.5),'gapMsP95':percentile(gaps,.95),
                'callbacksOver50ms':sum(c>50 for c in cpu),'maxStepsPerCallback':max((c['steps'] for c in calls),default=0),
                'presentsPerLogicStep':(report['end']['presentedFrames']-report['start']['presentedFrames'])/max(1,report['end']['frames']-report['start']['frames'])}
            presented=[c for c in calls if c['presents']>0]
            intervals=[b['timestamp']-a['timestamp'] for a,b in zip(presented,presented[1:])]
            costs=[c['cpu'] for c in presented]
            report['summary'].update({'displayUpdateHz':len(presented)/elapsed,
                'presentedGapMsP95':percentile(intervals,.95),'presentedGapMsMax':max(intervals,default=0),
                'activeCallbackMsP50':percentile(costs,.5),'activeCallbackMsP95':percentile(costs,.95)})
            if args.require_smooth:
                s=report['summary']
                report['performanceBudget']={'logicHzMin':57,'displayUpdateHzMin':55,
                    'activeCallbackMsP95Max':25,'presentedGapMsMaxLimit':100}
                if (s['logicHz']<57 or s['displayUpdateHz']<55 or s['activeCallbackMsP95']>25
                        or s['presentedGapMsMax']>100):
                    raise RuntimeError('Steady Stage 1 missed the performance budget; see summary')
                report['performancePassed']=True
            page.screenshot(path=str(args.out/'end.png'))
        except Exception as error:
            report['failure']=str(error)
            try:report['diagnostics']=page.evaluate('window.ph3Diagnostics');page.screenshot(path=str(args.out/'failure.png'))
            except Exception:pass
        finally:
            (args.out/'report.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
            browser.close()
    print(json.dumps({k:report[k] for k in ['summary','cpu','failure','renderer'] if k in report},ensure_ascii=True,indent=2),flush=True)
    if report.get('failure') or report['errors'] or report.get('end',{}).get('errors'):raise SystemExit(1)

if __name__=='__main__':main()
