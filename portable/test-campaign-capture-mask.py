"""Ensure the FPS exclusion cannot hide unrelated bottom-of-game regressions."""
import json
from pathlib import Path
import subprocess
import sys
from PIL import Image

root = Path(__file__).resolve().parent.parent
out = root / 'build' / 'capture-mask-regression'
reference, candidate = out / 'reference', out / 'candidate'
for directory in (reference, candidate):
    directory.mkdir(parents=True, exist_ok=True)
    report = dict(fixtureSha256='same-fixture', build=dict(sources={}),
                  captures=[dict(file='stage1-frame1.png')],
                  requestedCaptures=[dict(stage=1, frame=1)])
    (directory / 'report.json').write_text(json.dumps(report), encoding='utf8')
    (directory / 'trace.jsonl').write_text('{"kind":"tick","frame":1}\n{"kind":"run-end"}\n', encoding='utf8')
image = Image.new('RGBA', (100, 100), (0, 0, 0, 255))
image.save(reference / 'stage1-frame1.png')

def compare(changes, mask='90,95,99,99', expected=0):
    modified = image.copy()
    for point, color in changes:
        modified.putpixel(point, color)
    modified.save(candidate / 'stage1-frame1.png')
    command = [sys.executable, str(root / 'portable' / 'compare-campaign-evidence.py'),
               str(reference), str(candidate), '--ignore-fps-rect=' + mask]
    run = subprocess.run(command, capture_output=True, text=True, encoding='utf8')
    assert run.returncode == expected, (command, run.stdout, run.stderr)
    if expected != 2:
        result = json.loads(run.stdout)
        assert result['passed'] == (expected == 0)
        return result

compare([])
result = compare([((92, 96), (255, 0, 0, 255))])
assert result['captures'][0]['fullChangedBounds'] == [92, 96, 93, 97]
compare([((2, 99), (255, 0, 0, 255))], expected=1)
compare([((50, 98), (0, 0, 0, 0)), ((92, 96), (255, 0, 0, 255))], expected=1)
compare([], mask='75,90,100,100', expected=2)
compare([], mask='0,0,1,1', expected=2)
print('PASS exact FPS rectangle, RGBA differences, bottom-game coverage and oversized-mask rejection')
