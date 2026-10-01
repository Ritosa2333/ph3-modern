"""Compare complete diagnostic input/state traces and requested render captures.

This does not turn a replay that already ends early into a golden pass. The
recorded-file end checks are kept separately from before/after equivalence.
"""
from __future__ import annotations
import argparse
import hashlib
from itertools import zip_longest
import json
from pathlib import Path
import sys
import numpy as np
from PIL import Image

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('reference', type=Path)
parser.add_argument('candidate', type=Path)
parser.add_argument('--ignore-bottom', type=float, default=0,
                    help='Explicit fraction reserved for the wall-clock FPS strip; full-image differences are still reported')
parser.add_argument('--ignore-fps-rect', type=str,
                    help='Exact x0,y0,x1,y1 FPS-number rectangle; at most 1%% of the image, in its bottom-right corner')
args = parser.parse_args()
if not 0 <= args.ignore_bottom <= .1:
    parser.error('FPS exclusion must be between zero and 10 percent of image height')
fps_rect = None
if args.ignore_fps_rect:
    try:
        fps_rect = [int(value) for value in args.ignore_fps_rect.split(',')]
    except ValueError:
        parser.error('FPS rectangle must contain four integers')
    if len(fps_rect) != 4 or args.ignore_bottom:
        parser.error('Use one four-integer FPS rectangle or the legacy bottom strip, not both')
reference = json.loads((args.reference / 'report.json').read_text(encoding='utf8'))
candidate = json.loads((args.candidate / 'report.json').read_text(encoding='utf8'))
result = {'reference': str(args.reference), 'candidate': str(args.candidate),
          'scope': 'before/after diagnostic equivalence, not phone performance or a full-game golden pass',
          'sameFixture': reference['fixtureSha256'] == candidate['fixtureSha256'],
          'sameSourceSnapshot': reference['build']['sources'] == candidate['build']['sources'],
          'records': 0, 'ticks': 0, 'firstTraceDifference': None, 'traceComplete': [False, False],
          'referenceStageEnds': [], 'candidateStageEnds': [], 'captures': [], 'missingCaptures': [],
          'ignoredBottomFraction': args.ignore_bottom, 'ignoredFpsRectangle': fps_rect}
hashes = [hashlib.sha256(), hashlib.sha256()]
with (args.reference / 'trace.jsonl').open('rb') as a, (args.candidate / 'trace.jsonl').open('rb') as b:
    for index, lines in enumerate(zip_longest(a, b), 1):
        parsed = [json.loads(line) if line else None for line in lines]
        for side, (line, record) in enumerate(zip(lines, parsed)):
            if line:
                hashes[side].update(line)
            if record and record.get('kind') == 'run-end':
                result['traceComplete'][side] = True
            if record and record.get('kind') == 'end':
                end = dict(record, recordedFrameMatches=record['frame'] == record['expectedFrame'],
                           recordedScoreMatches=record['score'] == record['expectedScore'])
                result['referenceStageEnds' if side == 0 else 'candidateStageEnds'].append(end)
        result['records'] = index
        if parsed[0] and parsed[0].get('kind') == 'tick':
            result['ticks'] += 1
        if parsed[0] != parsed[1] and result['firstTraceDifference'] is None:
            keys = sorted(set(parsed[0] or {}) | set(parsed[1] or {}))
            result['firstTraceDifference'] = {'line': index, 'referenceFrame': (parsed[0] or {}).get('frame'),
                'candidateFrame': (parsed[1] or {}).get('frame'),
                'fields': {key: [(parsed[0] or {}).get(key), (parsed[1] or {}).get(key)]
                           for key in keys if (parsed[0] or {}).get(key) != (parsed[1] or {}).get(key)}}
result['traceSha256'] = [value.hexdigest() for value in hashes]
reference_captures = {entry['file']: entry for entry in reference['captures']}
candidate_captures = {entry['file']: entry for entry in candidate['captures']}
requested = {f"stage{entry['stage']}-frame{entry['frame']}.png" for report in [reference, candidate]
             for entry in report.get('requestedCaptures', [])}
for name in sorted(requested):
    if name not in reference_captures or name not in candidate_captures:
        result['missingCaptures'].append(name)
        continue
    a = np.asarray(Image.open(args.reference / name).convert('RGBA'))
    b = np.asarray(Image.open(args.candidate / name).convert('RGBA'))
    if a.shape != b.shape:
        result['captures'].append({'file': name, 'sameDimensions': False, 'acceptedPixelsMatch': False})
        continue
    difference = a.astype(np.int16) - b.astype(np.int16)
    height = a.shape[0] - int(round(a.shape[0] * args.ignore_bottom))
    changed = np.any(difference != 0, axis=2)
    accepted = changed.copy()
    accepted[height:] = False
    if fps_rect:
        left, top, right, bottom = fps_rect
        w, h = a.shape[1], a.shape[0]
        if not (0 <= left < right <= w and 0 <= top < bottom <= h
                and left >= .75 * w and top >= .9 * h
                and (right - left) * (bottom - top) <= .01 * w * h):
            parser.error('FPS rectangle must be in the bottom-right corner and cover at most 1% of each image')
        accepted[top:bottom, left:right] = False
    ys, xs = np.nonzero(changed)
    changed_bounds = ([int(xs.min()), int(ys.min()), int(xs.max()) + 1, int(ys.max()) + 1]
                      if len(xs) else None)
    result['captures'].append({'file': name, 'sameDimensions': True,
        'width': a.shape[1], 'height': a.shape[0], 'acceptedHeight': height,
        'fullChangedPixels': int(changed.sum()), 'fullChangedBounds': changed_bounds,
        'fullRmse': float(np.sqrt(np.mean(difference.astype(np.float64) ** 2))),
        'acceptedChangedPixels': int(accepted.sum()),
        'acceptedPixelsMatch': not bool(accepted.any())})
result['passed'] = bool(result['sameFixture'] and result['ticks'] > 0 and all(result['traceComplete'])
                        and result['firstTraceDifference'] is None and not result['missingCaptures']
                        and all(capture['acceptedPixelsMatch'] for capture in result['captures']))
(args.candidate / 'equivalence.json').write_text(json.dumps(result, indent=2) + '\n', encoding='utf8')
print(json.dumps(result, indent=2))
sys.exit(0 if result['passed'] else 1)
