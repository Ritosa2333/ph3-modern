"""Freeze one verified original-source Replay trace and compare Web traces.

Capture is an explicit oracle-maintenance action. Compare is read-only and
never updates the golden. Both operate on the existing diagnostic trace schema;
ordinary Web builds and game scripts are unaffected.
"""
from __future__ import annotations

import argparse
import gzip
import hashlib
from itertools import zip_longest
import json
from pathlib import Path
from typing import Iterator

from corpus import GAME, ROOT, inspect

SCHEMA = 'ph3/original-replay-golden/v1'
ORIGINAL = '5c92f28e17b6ef3721a3ac4caa9de3af3164b51e'
WOO_DAT = '900aa3ebeb329187e8c2852b8ad41caa5200ca9e'
OBSERVER = ROOT / 'portable/replay/Trace.hpp'


def sha(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open('rb') as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b''):
            digest.update(chunk)
    return digest.hexdigest()


def game_digest() -> dict:
    """Bind the original package content, excluding executable and fixture slots."""
    digest = hashlib.sha256()
    count = size = 0
    for path in sorted(GAME.rglob('*')):
        if not path.is_file():
            continue
        relative = path.relative_to(GAME).as_posix()
        if relative.startswith('script/th_hsob/replay/') or path.suffix.lower() in ('.exe', '.dll', '.lnk', '.log'):
            continue
        raw_name = relative.encode('utf-8')
        digest.update(len(raw_name).to_bytes(4, 'little'))
        digest.update(raw_name)
        digest.update(bytes.fromhex(sha(path)))
        count += 1
        size += path.stat().st_size
    return {'sha256': digest.hexdigest(), 'files': count, 'bytes': size}


def events(path: Path) -> Iterator[tuple[bytes, dict]]:
    with path.open('rb') as stream:
        for number, line in enumerate(stream, 1):
            if not line.endswith(b'\n'):
                raise ValueError(f'Incomplete trace line {number}')
            yield line, json.loads(line)


def capture(args: argparse.Namespace) -> None:
    if args.out.exists():
        raise FileExistsError('Golden destination already exists; choose a new versioned directory')
    report_path = args.oracle_run / 'report.json'
    trace_path = args.oracle_run / 'trace.raw.jsonl'
    report = json.loads(report_path.read_text(encoding='utf-8'))
    provenance = json.loads(args.provenance.read_text(encoding='utf-8'))
    fixture_name = report['fixture']['file']
    fixture = inspect(GAME / 'script/th_hsob/replay' / fixture_name)
    stages = fixture['stages'][:report['requestedStages']]
    if not report.get('complete') or report.get('failure') or not stages:
        raise ValueError('Original-source run is incomplete or failed')
    if fixture['sha256'] != report['fixture']['sha256']:
        raise ValueError('Fixture changed since original-source run')
    if provenance.get('family') != 'original' or provenance.get('base') != ORIGINAL or not provenance.get('compiled'):
        raise ValueError('Native provenance is not the pinned original source')
    if provenance.get('executableSha256') != report['oracleSha256']:
        raise ValueError('Oracle executable SHA-256 differs from build provenance')
    vendor = {item.get('sourceRevision') for item in provenance.get('transforms', []) if item.get('sourceRevision')}
    if WOO_DAT not in vendor:
        raise ValueError('Pinned Woo DAT/zlib dependency is absent from native provenance')
    observed = []
    current = None
    ticks = 0
    saw_run_end = False
    trace_hash = hashlib.sha256()
    trace_bytes = records = 0
    for line, event in events(trace_path):
        trace_hash.update(line)
        trace_bytes += len(line)
        records += 1
        kind = event['kind']
        if kind == 'begin':
            if current is not None or len(observed) >= len(stages) or event['stage'] != stages[len(observed)]['index']:
                raise ValueError('Unexpected stage begin in original trace')
            current = event['stage']
            ticks = 0
        elif kind == 'tick':
            if current != event['stage'] or event['tick'] != ticks:
                raise ValueError('Missing or reordered original tick')
            ticks += 1
        elif kind == 'end':
            if current != event['stage'] or len(observed) >= len(stages):
                raise ValueError('Unexpected stage end in original trace')
            expected = stages[len(observed)]
            if (event['frame'], int(event['score']), event['ticks']) != (expected['frameEnd'], expected['scoreLast'], ticks):
                raise ValueError('Original trace differs from recorded frame, score, or tick count')
            observed.append({'index': current, 'frame': event['frame'], 'score': int(event['score']), 'ticks': ticks})
            current = None
        elif kind == 'run-end':
            if current is not None or len(observed) != len(stages) or saw_run_end:
                raise ValueError('Premature or duplicate original run-end')
            saw_run_end = True
        else:
            raise ValueError('Unknown original trace event: ' + str(kind))
    if not saw_run_end or len(observed) != len(stages):
        raise ValueError('Original trace lacks natural completion')
    if [(x['stage'], x['frame'], int(x['score'])) for x in report.get('stageResults', [])] != [
        (x['index'], x['frame'], x['score']) for x in observed]:
        raise ValueError('Native run report disagrees with trace')

    content = game_digest()
    args.out.mkdir(parents=True)
    compressed = args.out / 'trace.jsonl.gz'
    with trace_path.open('rb') as source, compressed.open('wb') as destination:
        with gzip.GzipFile(filename='', mode='wb', fileobj=destination, mtime=0, compresslevel=9) as writer:
            for chunk in iter(lambda: source.read(1024 * 1024), b''):
                writer.write(chunk)
    manifest = {
        'schema': SCHEMA,
        'sourceCommit': ORIGINAL,
        'datCommit': WOO_DAT,
        'oracleSha256': report['oracleSha256'],
        'observerSha256': sha(OBSERVER),
        'nativeBuildProvenanceSha256': sha(args.provenance),
        'nativeRunReportSha256': sha(report_path),
        'gameContent': content,
        'fixture': {'file': fixture_name, 'sha256': fixture['sha256'], 'bytes': fixture['bytes']},
        'stages': observed,
        'trace': {'file': compressed.name, 'sha256': trace_hash.hexdigest(),
                  'compressedSha256': sha(compressed), 'bytes': trace_bytes, 'records': records},
    }
    (args.out / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n', encoding='utf-8')
    print(json.dumps({'golden': str(args.out), 'stages': observed, 'trace': manifest['trace']}, indent=2))


def compare(args: argparse.Namespace) -> None:
    manifest = json.loads(args.golden.read_text(encoding='utf-8'))
    if manifest.get('schema') != SCHEMA or manifest.get('sourceCommit') != ORIGINAL or manifest.get('datCommit') != WOO_DAT:
        raise ValueError('Incompatible golden provenance')
    if sha(OBSERVER) != manifest['observerSha256']:
        raise ValueError('Observer schema changed; this golden cannot be compared as-is')
    fixture = GAME / 'script/th_hsob/replay' / manifest['fixture']['file']
    if sha(fixture) != manifest['fixture']['sha256'] or game_digest() != manifest['gameContent']:
        raise ValueError('Current fixture or game package differs from golden provenance')
    compressed = args.golden.parent / manifest['trace']['file']
    if sha(compressed) != manifest['trace']['compressedSha256']:
        raise ValueError('Golden trace bytes changed')
    first: dict[str, dict] = {}
    compared = 0
    trace_digest = hashlib.sha256()
    with gzip.open(compressed, 'rb') as source:
        golden_lines = list(source)
    for line in golden_lines:
        trace_digest.update(line)
    if trace_digest.hexdigest() != manifest['trace']['sha256'] or len(golden_lines) != manifest['trace']['records']:
        raise ValueError('Golden trace content changed')
    golden_records = (json.loads(line) for line in golden_lines)
    actual_records = (event for _, event in events(args.actual))
    if args.stage is not None:
        golden_records = (x for x in golden_records if x.get('stage') == args.stage)
        actual_records = (x for x in actual_records if x.get('stage') == args.stage)
    for a, b in zip_longest(golden_records, actual_records):
        if a is None or b is None:
            first['length'] = {'record': compared + 1,
                               'goldenHasMore': a is not None, 'actualHasMore': b is not None}
            break
        compared += 1
        identity = ('kind', 'stage', 'tick')
        if any(a.get(key) != b.get(key) for key in identity):
            first['alignment'] = {'record': compared, 'golden': {key: a.get(key) for key in identity},
                                  'actual': {key: b.get(key) for key in identity}}
            break
        for key in set(a) | set(b):
            if key not in first and a.get(key) != b.get(key):
                first[key] = {'record': compared, 'stage': a.get('stage'), 'frame': a.get('frame'),
                              'golden': a.get(key), 'actual': b.get(key)}
    print(json.dumps({'golden': str(args.golden), 'actual': str(args.actual), 'stage': args.stage,
                      'comparedRecords': compared, 'exact': not first, 'firstDifferences': first}, indent=2))
    if first:
        raise SystemExit(1)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest='command', required=True)
    make = commands.add_parser('capture', help='Freeze a complete verified original-source run once')
    make.add_argument('--oracle-run', type=Path, required=True)
    make.add_argument('--provenance', type=Path, required=True)
    make.add_argument('--out', type=Path, required=True)
    check = commands.add_parser('compare', help='Read-only comparison of a Web diagnostic trace')
    check.add_argument('--golden', type=Path, required=True)
    check.add_argument('--actual', type=Path, required=True)
    check.add_argument('--stage', type=int)
    args = parser.parse_args()
    {'capture': capture, 'compare': compare}[args.command](args)


if __name__ == '__main__':
    main()
