"""Find the first raw observer difference. This is diagnostic triage, NOT a golden gate."""
from __future__ import annotations
import argparse
import json
from itertools import zip_longest
from pathlib import Path


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--left', type=Path, required=True)
    parser.add_argument('--right', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[2]
    if not args.out.resolve().is_relative_to((root / 'build').resolve()):
        raise ValueError('Triage output must remain under build/')
    report = {'kind': 'diagnostic-only, not golden acceptance', 'left': str(args.left),
              'right': str(args.right), 'equalRecords': 0, 'difference': None,
              'supplementalPolicy': 'Optional detailShots dump compared only when both captures request it; shots digest is always strict'}
    with args.left.open(encoding='utf-8') as left, args.right.open(encoding='utf-8') as right:
        for number, (a, b) in enumerate(zip_longest(left, right), 1):
            if a is None or b is None:
                report['difference'] = {'line': number, 'reason': 'Different lengths / truncated capture'}
                break
            a, b = json.loads(a), json.loads(b)
            if ('detailShots' in a) != ('detailShots' in b):
                # This extra dump is selected at capture time for one diagnostic
                # tick. The authoritative full-list shots digest remains present.
                a.pop('detailShots', None)
                b.pop('detailShots', None)
            if a != b:
                keys = sorted(set(a) | set(b))
                report['difference'] = {'line': number, 'stage': a.get('stage'), 'tick': a.get('tick'),
                                        'fields': {k: {'left': a.get(k), 'right': b.get(k)}
                                                   for k in keys if a.get(k) != b.get(k)},
                                        'leftRecord': a, 'rightRecord': b}
                break
            report['equalRecords'] += 1
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(json.dumps(report, indent=2), encoding='utf-8')
    print(json.dumps(report, indent=2), flush=True)


if __name__ == '__main__':
    main()
