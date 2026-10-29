"""Summarize original-menu browser runs without confusing diagnostics with FPS.

frameMsP95 is the runtime's per-second callback-window P95. Its median is NOT
the pooled P95 of every frame. GL bytes are logical allocation estimates and
exclude browser/driver residency and transient orphaned backing buffers.
"""
from __future__ import annotations
import argparse
import json
from pathlib import Path
import statistics

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('runs', nargs='+', type=Path)
parser.add_argument('--out', type=Path, required=True)
parser.add_argument('--window', action='append', default=[], help='stage:lo:hi:label; half-open frame range')
args = parser.parse_args()
windows = []
for spec in args.window or ['1:5000:7400:stage1-road', '1:8200:12800:stage1-boss',
                             '6:1000:7400:stage6-road', '6:10500:14500:stage6-boss']:
    stage, lo, hi, label = spec.split(':', 3)
    windows.append((int(stage), int(lo), int(hi), label))

def values(samples, owner, field):
    return [sample[owner][field] for sample in samples
            if isinstance(sample.get(owner), dict) and isinstance(sample[owner].get(field), (float, int))]

def summarize(samples):
    result = {'samples': len(samples), 'observedCpuThrottleRates': sorted({sample.get('cpuThrottle', 1) for sample in samples})}
    for field in ['frameMsP95', 'frameMsMax', 'callbackMsP95', 'callbackMsP99', 'callbackMsMax',
                  'queuedLoadMsMax', 'logicHz', 'displayFps']:
        data = values(samples, 'performance', field)
        if data:
            result[field] = {'median': statistics.median(data), 'min': min(data), 'max': max(data)}
    # A browser timer may retain the same completed window more than once.
    # Weight actual callback/refresh counts, never averages of window rates.
    measured = {sample['performance']['sequence']: sample['performance'] for sample in samples
                if sample.get('performance', {}).get('timingVersion') == 3}
    if measured:
        sequences = sorted(measured)
        callbacks = sum(p['callbackCount'] for p in measured.values())
        slow = sum(p['callbackOverBudget'] for p in measured.values())
        presents = sum(p['presentIntervalCount'] for p in measured.values())
        missed = sum(p['missedRefreshCount'] for p in measured.values())
        duration = sum(p['windowMs'] for p in measured.values())
        result['deadline60Hz'] = {'windows': len(measured), 'callbackCount': callbacks,
            'callbackOverBudget': slow, 'cpuWithinBudgetPercent': 100 * (1 - slow / callbacks) if callbacks else None,
            'presentIntervalCount': presents, 'missedRefreshCount': missed,
            'deliveredRefreshPercent': 100 * presents / (presents + missed) if presents + missed else None,
            'logicHz': sum(p['logicHz'] * p['windowMs'] for p in measured.values()) / duration if duration else None,
            'missingWindows': sequences[-1] - sequences[0] + 1 - len(sequences)}
    for field in ['drawCalls', 'actualDrawCalls', 'drawVertices', 'wasmBytes', 'jsUsedBytes']:
        data = [sample[field] for sample in samples if isinstance(sample.get(field), (int, float))]
        if data:
            result[field] = {'median': statistics.median(data), 'min': min(data), 'max': max(data)}
    for owner, fields in [('graphicsMemory', ['textureBytes', 'peakTextureBytes', 'textureCount',
                                            'peakCombinedBytes', 'bufferBytes', 'renderbufferBytes']),
                          ('textureCache', ['bytes', 'entries', 'evictions'])]:
        for field in fields:
            data = values(samples, owner, field)
            if data:
                result.setdefault(owner, {})[field] = {'min': min(data), 'max': max(data), 'last': data[-1]}
    return result

result = {'scope': __doc__, 'runs': []}
for path in args.runs:
    report = json.loads((path / 'report.json').read_text(encoding='utf8'))
    samples = [json.loads(line) for line in (path / 'samples.jsonl').read_text(encoding='utf8').splitlines()]
    build = report.get('build', {})
    timing_versions = sorted({sample['performance'].get('timingVersion', 1)
                              for sample in samples if isinstance(sample.get('performance'), dict)})
    intervals = [b['time'] - a['time'] for a, b in zip(samples, samples[1:])]
    sampling_method = report.get('samplingMethod', 'host-polling')
    sampling = dict(report.get('sampling', {}), method=sampling_method,
                    maxRecordedIntervalMs=max(intervals, default=0),
                    recordedGapsOver1500Ms=sum(value > 1500 for value in intervals),
                    error=report.get('samplingError'))
    # Old polling cannot distinguish a delayed CDP/Node read from a browser
    # stall. Keep its values visible, but never silently accept a missing Boss
    # window as a faster candidate. Browser-side stalls remain genuine data.
    sampling_complete = not sampling.get('dropped', 0) and not sampling['error']
    if sampling_method == 'host-polling' and sampling['maxRecordedIntervalMs'] > 1500:
        sampling_complete = False
    entry = {'directory': str(path), 'outcome': report['outcome'], 'errors': report.get('errors'),
             'failure': report.get('failure'), 'fixtureSha256': report['fixtureSha256'],
             'variant': build.get('variant'), 'builtAt': build.get('builtAt'),
             'exceptionMode': build.get('exceptionMode'), 'renderer': report.get('renderer'),
             'browserVersion': report.get('browserVersion'),
             'managed': report.get('managed'), 'cpuProfiling': report.get('cpuProfiling'),
             'vmInstructionProfiling': report.get('vmInstructionProfiling'),
             'throttle': report.get('throttle'), 'glMemoryObserver': report.get('glMemoryObserver'),
             'cpuAffinity': report.get('cpuAffinity'),
             'throttleFromFrame': report.get('throttleFromFrame', 0), 'throttleAppliedAt': report.get('throttleAppliedAt'),
             'fast': report.get('fast', 0), 'trace': report.get('trace', False),
             'sampling': sampling,
             'timingSchemaVersions': timing_versions,
             'timingComparable': (not report.get('fast') and not report.get('trace')
                                  and not report.get('cpuProfiling')
                                  and not report.get('vmInstructionProfiling')
                                  and not report.get('errors') and not report.get('failure')
                                  and build.get('variant') == 'standard'
                                  and not report.get('glMemoryObserver') and not report.get('requestedCaptures')
                                  and sampling_complete and len(timing_versions) == 1),
             'lastStage': samples[-1].get('stage') if samples else None,
             'allSamples': summarize(samples), 'windows': {}}
    if entry['allSamples'].get('deadline60Hz', {}).get('missingWindows', 0):
        entry['timingComparable'] = False
    for stage, lo, hi, label in windows:
        selected = [sample for sample in samples if sample.get('stage') and sample['stage']['index'] == stage
                    and lo <= sample['stage']['frame'] < hi]
        if selected:
            entry['windows'][label] = {'stage': stage, 'fromFrame': lo, 'untilFrame': hi, **summarize(selected)}
    result['runs'].append(entry)
args.out.parent.mkdir(parents=True, exist_ok=True)
args.out.write_text(json.dumps(result, indent=2) + '\n', encoding='utf8')
for run in result['runs']:
    print(run['directory'], run['outcome'], 'timingComparable:', run['timingComparable'])
    for label, window in run['windows'].items():
        print(' ', label, 'n=', window['samples'], 'presented callback P95 median=', window.get('frameMsP95', {}).get('median'),
              'draw median=', window.get('actualDrawCalls', {}).get('median'))
        if 'deadline60Hz' in window:
            print('   ', window['deadline60Hz'])
    print('  logical GPU texture bytes:', run['allSamples'].get('graphicsMemory', {}).get('textureBytes'))
    print('  Wasm bytes:', run['allSamples'].get('wasmBytes'))
print('Evidence:', args.out)
