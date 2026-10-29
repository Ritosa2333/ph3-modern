"""Pinned source locations for independent Replay references."""
from pathlib import Path
import os
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools'))
REVISIONS = {
    'original': '5c92f28e17b6ef3721a3ac4caa9de3af3164b51e',
    'woo': '97ca3b50329b134ed7f159d6dabbe7509292e8b3',
}


def source_repository(family: str) -> Path:
    variable = 'PH3_' + family.upper() + '_SOURCE'
    configured = os.environ.get(variable)
    candidates = [Path(configured)] if configured else [ROOT, *[
        parent / 'fangame/engines' / ('ph3-original' if family == 'original' else 'ph3-woo')
        for parent in ROOT.parents[:3]]]
    for directory in candidates:
        if not directory.is_dir():
            continue
        result = subprocess.run(['git', 'cat-file', '-e', REVISIONS[family] + '^{commit}'],
                                cwd=directory, capture_output=True, timeout=30)
        if result.returncode == 0:
            return directory.resolve()
    raise FileNotFoundError(f'Set {variable} to a repository containing {REVISIONS[family]}')
