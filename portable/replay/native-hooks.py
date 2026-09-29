"""Describe read-only diagnostic instrumentation of the pinned Woo baseline."""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[2]
WOO_REPO = ROOT.parent / 'ph3-woo-web'


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--family', choices=['woo', 'original'], default='woo')
    parser.add_argument('--legacy-rng', action='store_true',
                        help='Use the independently pinned mkm RNG in a labelled compatibility reference')
    parser.add_argument('--archive-backend', action='store_true',
                        help='Provide the original public source with its missing zlib decompressor; gameplay remains original')
    args = parser.parse_args()
    if args.family == 'original':
        original_hooks(archive_backend=args.archive_backend)
        return
    changes = []
    def replace(path: str, before: str, after: str, purpose: str, count: int = 1) -> None:
        changes.append(dict(path=path, before=before, after=after, purpose=purpose, count=count))

    replace('source/GcLib/gstd/GstdConstant.hpp', '#define __GSTD_GAMESTDCONSTANT__',
            '#define __GSTD_GAMESTDCONSTANT__\n#define WOO_REPLAY_VERIFY 1\n'
            + ('#define WOO_TRACE_MKM_RNG 1\n' if args.legacy_rng else '')
            + 'namespace woo { namespace verify { struct Access; } }',
            'Enable read-only test observer in the separate oracle build')
    for path, cls in [('source/GcLib/gstd/MersenneTwister.hpp', 'MersenneTwister'),
                      ('source/GcLib/directx/DirectInput.hpp', 'KeyReplayManager'),
                      ('source/TouhouDanmakufu/Common/StgShot.hpp', 'StgShotManager'),
                      ('source/TouhouDanmakufu/Common/StgItem.hpp', 'StgItemManager')]:
        if args.legacy_rng and cls == 'MersenneTwister':
            continue
        replace(path, 'class ' + cls + ' {', 'class ' + cls + ' {\n\tfriend struct ::woo::verify::Access;',
                'Grant observer read-only access; no layout or game behavior changes')
    stage = 'source/TouhouDanmakufu/Common/StgStageController.cpp'
    replace(stage, '#include "StgSystem.hpp"', '#include "StgSystem.hpp"\n#include "../../../portable/replay/Boundary.hpp"',
            'Compile the same trace schema as the Web verifier')
    replace(stage, 'infoStage_->SetStageStartTime(timeGetTime());',
            'infoStage_->SetStageStartTime(timeGetTime());\n\twoo::replay::Boundary(this, true);', 'Observe completed stage initialization')
    replace(stage, 'void StgStageController::CloseScene()\n{',
            'void StgStageController::CloseScene()\n{\n\twoo::replay::Boundary(this, false);', 'Observe natural stage end')
    replace(stage, 'void StgStageController::Work()\n{',
            'void StgStageController::Work()\n{\n\twoo::verify::Step traceStep(this);', 'Observe complete consumed input ticks including terminal early returns')
    replace('DnhExecutor.vcxproj', '<AdditionalOptions>/utf-8 %(AdditionalOptions)</AdditionalOptions>',
            '<AdditionalOptions>/utf-8 /std:c++17 %(AdditionalOptions)</AdditionalOptions>',
            'Enable C++17 inline observer variables; keep original floating point and optimization settings')
    replace('source/TouhouDanmakufu/DnhExecutor/GcLibImpl.cpp',
            'if (hWndFocused != hWndGraphics && hWndFocused != hWndLogger) {',
            'if (hWndFocused != hWndGraphics && hWndFocused != hWndLogger && !std::getenv("WOO_ORACLE_BACKGROUND")) {',
            'Optional test-host background execution; replay owns effective stage input and all original ticks')
    independent_inputs = []
    if args.legacy_rng:
        # This is a source compatibility reference, not a certified retail
        # binary. Its RNG definition comes from the independent mkm archive,
        # never from candidate code or candidate results.
        original_revision = '5c92f28e17b6ef3721a3ac4caa9de3af3164b51e'
        woo_revision = '97ca3b50329b134ed7f159d6dabbe7509292e8b3'
        for name in ('MersenneTwister.hpp', 'MersenneTwister.cpp'):
            path = 'source/GcLib/gstd/' + name
            before = subprocess.run(['git','show',woo_revision+':'+path],cwd=ROOT,
                                    capture_output=True,check=True).stdout.decode('utf-8-sig').replace('\r\n','\n')
            original = subprocess.run(['git','show',original_revision+':'+path],cwd=ROOT.parent/'ph3-original',
                                      capture_output=True,check=True).stdout.decode('cp932').replace('\r\n','\n')
            if name.endswith('.hpp'):
                original, count = re.subn(r'(class\s+MersenneTwister\s*\{)',
                                         r'\1\n\tfriend struct ::woo::verify::Access;', original)
                if count != 1:raise ValueError('Original RNG observer anchor changed')
            replace(path, before, original, 'Independent mkm RNG definition from '+original_revision)
            independent_inputs.append(dict(revision=original_revision,path=path))
        replace(stage, 'GetMersenneTwister()->SeedRNG()', 'GetMersenneTwister()->GetSeed()',
                'Original ph3 seed-recording API; playback still uses the fixture seed')
    out = ROOT / ('build/replay/native-hooks-legacy.json' if args.legacy_rng else 'build/replay/native-hooks.json')
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(json.dumps({'profile': 'woo-source-mkm-rng' if args.legacy_rng else 'woo-source',
                              'independentInputs': independent_inputs, 'changes': changes, 'copies': [
        {'source': 'portable/replay/' + name, 'target': 'portable/replay/' + name}
        for name in ('Boundary.hpp', 'Trace.hpp')]}, indent=2), encoding='utf-8')
    print(out)


def original_hooks(archive_backend: bool = False) -> None:
    """Observe the independent mkm source, never transplant candidate gameplay."""
    repo = ROOT.parent / 'ph3-original'
    revision = '5c92f28e17b6ef3721a3ac4caa9de3af3164b51e'
    changes = []
    sources = {}

    def source(path: str) -> str:
        if path not in sources:
            raw = subprocess.run(['git', 'show', revision + ':' + path], cwd=repo,
                                 capture_output=True, check=True).stdout
            sources[path] = raw.decode('cp932').replace('\r\n', '\n')
        return sources[path]

    def anchored(path: str, pattern: str, replacement, purpose: str) -> None:
        text = source(path)
        found = list(re.finditer(pattern, text))
        if len(found) != 1:
            raise ValueError(f'Original observer anchor must be unique: {path}: {pattern}')
        before = found[0].group()
        after = replacement(before) if callable(replacement) else replacement
        changes.append(dict(path=path, before=before, after=after, purpose=purpose))
        sources[path] = text.replace(before, after, 1)

    anchored('source/GcLib/gstd/GstdConstant.hpp', r'#define __GSTD_GAMESTDCONSTANT__',
             lambda s: s+'\n#define WOO_REPLAY_VERIFY 1\n#define WOO_TRACE_MKM_RNG 1\nnamespace woo { namespace verify { struct Access; } }',
             'Independent original-source read-only observer, legacy RNG')
    for path, cls in [('source/GcLib/gstd/MersenneTwister.hpp', 'MersenneTwister'),
                      ('source/GcLib/directx/DirectInput.hpp', 'KeyReplayManager'),
                      ('source/TouhouDanmakufu/Common/StgShot.hpp', 'StgShotManager'),
                      ('source/TouhouDanmakufu/Common/StgItem.hpp', 'StgItemManager')]:
        anchored(path, r'\bclass\s+'+cls+r'\s*\{',
                 lambda s: s+'\n\tfriend struct ::woo::verify::Access;',
                 'Read-only friend access; no layout, input, RNG or rule modifications')
    stage = 'source/TouhouDanmakufu/Common/StgStageController.cpp'
    anchored(stage, r'#include\s*"StgSystem.hpp"',
             lambda s: s+'\n#include "../../../portable/replay/Boundary.hpp"', 'Shared observation schema')
    anchored(stage, r'infoStage_->SetStageStartTime\(timeGetTime\(\)\);',
             lambda s: s+'\n\twoo::replay::Boundary(this, true);', 'Observe completed initialization')
    anchored(stage, r'void StgStageController::CloseScene\(\)\s*\{',
             lambda s: s+'\n\twoo::replay::Boundary(this, false);', 'Observe natural stage termination')
    anchored(stage, r'void StgStageController::Work\(\)\s*\{',
             lambda s: s+'\n\twoo::verify::Step traceStep(this);', 'Observe fully consumed input ticks')
    anchored('DnhExecutor.vcxproj', r'<ItemDefinitionGroup Condition="[^"\n]*Release\|Win32[^"\n]*">\s*<ClCompile>\s*',
             lambda s: s+'<AdditionalOptions>/std:c++17 /source-charset:.932 /execution-charset:.932 %(AdditionalOptions)</AdditionalOptions>\n',
             'Preserve original CP932 source/literals and enable C++17 observer')
    project = 'DnhExecutor.vcxproj'
    old_output = '<OutputFile>bin_th_dnh/th_dnh.exe</OutputFile>'
    if source(project).count(old_output) != 2:
        raise ValueError('Unexpected original linker output paths')
    changes.append(dict(path=project, before=old_output,
                        after='<OutputFile>$(OutDir)DnhExecutor.exe</OutputFile>', count=2,
                        purpose='Keep build output separate from archived reference binaries'))
    # Unlike the earlier Woo capture, this reference has no background/focus
    # behavior override. The runner uses the actual foreground keyboard route.
    copies = [{'source': 'portable/replay/'+name, 'target': 'portable/replay/'+name}
              for name in ('Boundary.hpp', 'Trace.hpp')]
    if archive_backend:
        # Woo's archive support was introduced as a self-contained File/zlib
        # change. Reuse that exact, historically labelled DAT-compatible state
        # instead of inventing a third decompressor implementation.
        vendor_revision = '900aa3ebeb329187e8c2852b8ad41caa5200ca9e'
        for path in ('source/GcLib/gstd/File.cpp', 'source/GcLib/gstd/File.hpp'):
            raw = subprocess.run(['git', 'show', vendor_revision + ':' + path], cwd=WOO_REPO,
                                 capture_output=True, check=True).stdout
            copies.append({'source': path, 'target': path, 'expectedSha256': hashlib.sha256(raw).hexdigest(),
                           'sourceRevision': vendor_revision, 'sourceRepo': str(WOO_REPO),
                           'transcodeCp932': True,
                           'role': 'Woo DAT/archive mechanism only'})
        listing = subprocess.run(['git', 'ls-tree', '-r', '--name-only', vendor_revision, 'source/GcLib/zlib'],
                                 cwd=WOO_REPO, capture_output=True, check=True, text=True).stdout.splitlines()
        c_files = []
        for path in listing:
            raw = subprocess.run(['git', 'show', vendor_revision + ':' + path], cwd=WOO_REPO,
                                 capture_output=True, check=True).stdout
            copies.append({'source': path, 'target': path, 'expectedSha256': hashlib.sha256(raw).hexdigest(),
                           'sourceRevision': vendor_revision, 'sourceRepo': str(WOO_REPO),
                           'role': 'Woo-pinned third-party zlib for DAT support'})
            if path.endswith('.c'):
                c_files.append(path.replace('/', '\\\\'))
        anchored(project, r'<ClCompile Include="source\\GcLib\\gstd\\File.cpp"\s*/>',
                 lambda s: s+'\n'+''.join('    <ClCompile Include="'+path+'" />\n' for path in c_files),
                 'Compile the exact Woo-pinned zlib sources used by its DAT mechanism')
    out = ROOT / ('build/replay/original-hooks-zlib.json' if archive_backend else 'build/replay/original-hooks.json')
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(json.dumps({'profile': 'original-open-zlib' if archive_backend else 'original-open',
                              'changes': changes, 'copies': copies}, indent=2), encoding='utf-8')
    print(out)


if __name__ == '__main__':
    main()
