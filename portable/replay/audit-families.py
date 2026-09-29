"""Compare pinned original/Woo source tokens; generate evidence, never edit sources."""
from __future__ import annotations

import argparse
import difflib
import hashlib
import json
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[2]
ORIGINAL = ROOT.parent / 'ph3-original'
ORIGINAL_REV = '5c92f28e17b6ef3721a3ac4caa9de3af3164b51e'
WOO_REV = '97ca3b50329b134ed7f159d6dabbe7509292e8b3'
TOKEN = re.compile(r'//[^\n]*|/\*.*?\*/|(?:u8|[LuU])?"(?:\\.|[^"\\])*"|'
                   r"(?:[LuU])?'(?:\\.|[^'\\])*'|[A-Za-z_][A-Za-z_0-9]*|"
                   r'[0-9]+(?:\.[0-9]*)?(?:[eE][+-]?[0-9]+)?[a-zA-Z]*|'
                   r'::|->|<<|>>|==|!=|<=|>=|&&|\|\||\+\+|--|[^\s]', re.S)


def read(repo: Path, revision: str, path: str, encoding: str) -> tuple[bytes, str]:
    raw = subprocess.run(['git', 'show', revision + ':' + path], cwd=repo,
                         capture_output=True, check=True, timeout=30).stdout
    return raw, raw.decode(encoding).replace('\r\n', '\n')


def tokens(text: str) -> list[str]:
    return [m[0] for m in TOKEN.finditer(text) if not m[0].startswith(('//', '/*'))]


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out', type=Path, default=ROOT / 'build/replay/family-audit.json')
    args = parser.parse_args()
    paths = ['source/GcLib/gstd/' + name for name in
             ('MersenneTwister.cpp', 'MersenneTwister.hpp', 'Script.cpp', 'Script.hpp',
              'ScriptClient.cpp', 'SmartPointer.hpp', 'File.cpp')]
    paths += ['source/TouhouDanmakufu/Common/' + name for name in
              ('DnhReplay.cpp', 'StgCommon.cpp', 'StgEnemy.cpp', 'StgIntersection.cpp',
               'StgItem.cpp', 'StgPlayer.cpp', 'StgShot.cpp', 'StgStageController.cpp',
               'StgStageScript.cpp', 'StgPackageScript.cpp')]
    entries = []
    for path in paths:
        raw_a, a = read(ORIGINAL, ORIGINAL_REV, path, 'cp932')
        raw_b, b = read(ROOT, WOO_REV, path, 'utf-8-sig')
        at, bt = tokens(a), tokens(b)
        diffs = []
        for tag, a0, a1, b0, b1 in difflib.SequenceMatcher(None, at, bt, autojunk=False).get_opcodes():
            if tag == 'equal':
                continue
            diffs.append({'operation': tag, 'context': ' '.join(at[max(0, a0 - 20):a0]),
                          'original': ' '.join(at[a0:a1]), 'woo': ' '.join(bt[b0:b1]),
                          'after': ' '.join(at[a1:a1 + 12])})
        entry = {'path': path, 'originalSha256': hashlib.sha256(raw_a).hexdigest(),
                 'wooSha256': hashlib.sha256(raw_b).hexdigest(),
                 'sameTokens': at == bt, 'changes': diffs}
        entries.append(entry)
        print(f'{len(diffs):3d} token change groups: {path}', flush=True)
    report = {'originalRevision': ORIGINAL_REV, 'wooRevision': WOO_REV, 'files': entries,
              'scope': 'Source-token comparison only; not a runtime equivalence verdict'}
    out = args.out.resolve()
    if not out.is_relative_to((ROOT / 'build').resolve()):
        raise ValueError('Audit output must remain under build/')
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')


if __name__ == '__main__':
    main()
