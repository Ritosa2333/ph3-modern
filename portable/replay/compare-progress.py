"""Summarize first per-field differences between two read-only Replay traces."""
from __future__ import annotations

import argparse
import json
from pathlib import Path


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--left', type=Path, required=True)
    parser.add_argument('--right', type=Path, required=True)
    args = parser.parse_args()
    first: dict[str, dict] = {}
    matched = 0
    with args.left.open(encoding='utf-8') as left, args.right.open(encoding='utf-8') as right:
        for number, (a_line, b_line) in enumerate(zip(left, right), 1):
            a, b = json.loads(a_line), json.loads(b_line)
            if (a.get('kind'), a.get('stage'), a.get('tick')) != (b.get('kind'), b.get('stage'), b.get('tick')):
                first['alignment'] = {'line': number, 'left': a, 'right': b}
                break
            matched += 1
            for field in set(a) | set(b):
                if field in ('detailShots', 'detailEnemies'):
                    continue
                if field not in first and a.get(field) != b.get(field):
                    first[field] = {'line': number, 'stage': a.get('stage'), 'tick': a.get('tick'),
                                    'frame': a.get('frame'), 'left': a.get(field), 'right': b.get(field)}
    print(json.dumps({'matchedRecords': matched, 'firstDifferences': first}, indent=2))


if __name__ == '__main__':
    main()
