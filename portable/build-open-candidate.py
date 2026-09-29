"""Generate an isolated ph3-original + our Web-port overlay candidate.

This is an experiment. It never edits ph3-original or the active ph3-woo-web
source tree. The generated tree lives under build/open-web-candidate/source.

For each tracked file we changed after the pinned Woo baseline, use a real
three-way merge:

    base   = Woo 97ca3b5 before our Web port
    ours   = current ph3-woo-web working-tree file
    theirs = original ph3 open source

The one deliberate upstream exception is Woo's historically verified DAT/zlib
mechanism (900aa3e), because the public ph3 source intentionally omits the real
archive decompressor used by games such as Hollow Song of Birds.
"""
from __future__ import annotations

import argparse
import io
import json
from pathlib import Path
import subprocess
import tarfile
import tempfile

ROOT = Path(__file__).resolve().parents[1]
ORIGINAL = ROOT.parent / 'ph3-original'
ORIGINAL_REV = '5c92f28e17b6ef3721a3ac4caa9de3af3164b51e'
WOO_BASE = '97ca3b50329b134ed7f159d6dabbe7509292e8b3'
WOO_DAT = '900aa3ebeb329187e8c2852b8ad41caa5200ca9e'

TEXT_SUFFIXES = {'.c', '.cc', '.cpp', '.h', '.hpp', '.inl', '.rc'}


def git_bytes(repo: Path, revision: str, path: str) -> bytes:
    return subprocess.run(['git', 'show', revision + ':' + path], cwd=repo,
                          capture_output=True, check=True, timeout=30).stdout


def utf8(data: bytes, original: bool = False) -> str:
    if data.startswith(b'\xef\xbb\xbf'):
        return data.decode('utf-8-sig')
    if data.startswith((b'\xff\xfe', b'\xfe\xff')):
        return data.decode('utf-16')
    for encoding in (('cp932', 'utf-8') if original else ('utf-8', 'cp932')):
        try:
            return data.decode(encoding)
        except UnicodeDecodeError:
            pass
    raise UnicodeDecodeError('utf-8', data, 0, 1, 'unsupported source encoding')


def merge_text(ours: str, base: str, theirs: str) -> tuple[str, bool]:
    with tempfile.TemporaryDirectory(prefix='ph3-open-merge-') as td:
        paths = [Path(td) / name for name in ('ours', 'base', 'theirs')]
        for path, content in zip(paths, (ours, base, theirs)):
            path.write_text(content, encoding='utf-8', newline='\n')
        result = subprocess.run(['git', 'merge-file', '-p', '--diff3', *map(str, paths)],
                                capture_output=True, timeout=30)
        # git merge-file returns the number of conflicts (capped at 127), not
        # just a boolean 0/1 status.
        if result.returncode < 0 or result.returncode >= 128:
            raise RuntimeError(result.stderr.decode('utf-8', errors='replace'))
        return result.stdout.decode('utf-8'), result.returncode == 0


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out', type=Path, default=ROOT / 'build/open-web-candidate/source')
    parser.add_argument('--resolve-platform', action='store_true',
                        help='For merge conflicts, prefer current Web files only in explicit platform-owner paths; keep original gameplay elsewhere')
    parser.add_argument('--boundary-hooks', action='store_true',
                        help='Inject read-only replay stage lifecycle observation into the generated candidate')
    args = parser.parse_args()
    out = args.out.resolve()
    build_root = (ROOT / 'build').resolve()
    if not out.is_relative_to(build_root):
        raise ValueError('Candidate must stay under this repository build/')
    if out.exists() and any(out.iterdir()):
        raise FileExistsError(f'Candidate output is not empty: {out}')
    out.mkdir(parents=True, exist_ok=True)

    archive = subprocess.run(['git', 'archive', '--format=tar', ORIGINAL_REV], cwd=ORIGINAL,
                             capture_output=True, check=True, timeout=60).stdout
    with tarfile.open(fileobj=io.BytesIO(archive)) as tar:
        for entry in tar.getmembers():
            if not entry.isfile():
                continue
            path = (out / entry.name).resolve()
            if not path.is_relative_to(out):
                raise ValueError('Unsafe original archive path')
            data = tar.extractfile(entry).read()
            if entry.name.startswith('source/') and Path(entry.name).suffix.lower() in TEXT_SUFFIXES:
                try:
                    data = utf8(data, original=True).encode('utf-8')
                except UnicodeDecodeError as error:
                    raise UnicodeDecodeError(error.encoding, error.object, error.start, error.end,
                                             f'{entry.name}: {error.reason}') from error
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(data)

    # Bring over exactly the Woo DAT/archive implementation and its pinned zlib.
    for path in ('source/GcLib/gstd/File.cpp', 'source/GcLib/gstd/File.hpp'):
        target = out / path
        target.write_text(utf8(git_bytes(ROOT, WOO_DAT, path)), encoding='utf-8', newline='\n')
    listing = subprocess.run(['git', 'ls-tree', '-r', '--name-only', WOO_DAT, 'source/GcLib/zlib'],
                             cwd=ROOT, capture_output=True, check=True, text=True, timeout=30).stdout.splitlines()
    for path in listing:
        target = out / path
        target.parent.mkdir(parents=True, exist_ok=True)
        data = git_bytes(ROOT, WOO_DAT, path)
        if Path(path).suffix.lower() in TEXT_SUFFIXES:
            data = utf8(data).encode('utf-8')
        target.write_bytes(data)

    changed = subprocess.run(['git', 'diff', '--name-only', WOO_BASE, '--'], cwd=ROOT,
                             capture_output=True, check=True, text=True, timeout=30).stdout.splitlines()
    report = {'original': ORIGINAL_REV, 'webPortBase': WOO_BASE, 'datSource': WOO_DAT,
              'merged': [], 'conflicts': [], 'platformPreferred': [], 'originalPreferred': [], 'newFiles': []}
    platform_prefixes = ('source/GcLib/directx/', 'source/GcLib/gstd/')
    platform_exact = {
        'source/TouhouDanmakufu/Common/DnhCommon.cpp',
        'source/TouhouDanmakufu/Common/DnhGcLibImpl.cpp',
        'source/TouhouDanmakufu/Common/DnhGcLibImpl.hpp',
        'source/TouhouDanmakufu/DnhExecutor/GcLibImpl.cpp',
        'source/TouhouDanmakufu/DnhExecutor/GcLibImpl.hpp',
    }
    original_even_in_platform = {
        'source/GcLib/gstd/MersenneTwister.cpp',
        'source/GcLib/gstd/MersenneTwister.hpp',
    }
    for path in changed:
        source = ROOT / path
        target = out / path
        if not source.is_file() or not target.is_file():
            continue
        # Project/docs are not part of the C++ Web overlay unless already in the
        # original candidate. Binary resources are deliberately untouched.
        if source.suffix.lower() not in TEXT_SUFFIXES | {'.mjs', '.py', '.html'}:
            continue
        try:
            base = utf8(git_bytes(ROOT, WOO_BASE, path))
        except subprocess.CalledProcessError:
            continue
        ours = source.read_text(encoding='utf-8-sig')
        theirs = target.read_text(encoding='utf-8')
        try:
            merged, clean = merge_text(ours, base, theirs)
        except Exception as error:
            raise RuntimeError(f'three-way merge failed for {path}: {error}') from error
        if clean:
            target.write_text(merged, encoding='utf-8', newline='\n')
            report['merged'].append(path)
        elif args.resolve_platform:
            prefer_platform = ((path.startswith(platform_prefixes) or path in platform_exact)
                               and path not in original_even_in_platform)
            if prefer_platform:
                target.write_text(ours, encoding='utf-8', newline='\n')
                report['platformPreferred'].append(path)
            else:
                # Keep the original/open (or explicit DAT target) file. Missing
                # Web adaptations will then surface as concrete compiler errors
                # instead of silently importing Woo gameplay behavior.
                target.write_text(theirs, encoding='utf-8', newline='\n')
                report['originalPreferred'].append(path)
        else:
            target.write_text(merged, encoding='utf-8', newline='\n')
            report['conflicts'].append(path)

    # Pure Web additions have no original counterpart and are copied verbatim.
    additions = [ROOT / 'portable', ROOT / 'source/GcLib/portable']
    explicit = [ROOT / 'source/GcLib/gstd/ScriptReal.hpp']
    for root in additions:
        for source in root.rglob('*'):
            if not source.is_file() or 'build' in source.parts:
                continue
            relative = source.relative_to(ROOT)
            target = out / relative
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(source.read_bytes())
            report['newFiles'].append(relative.as_posix())
    for source in explicit:
        if source.is_file():
            relative = source.relative_to(ROOT)
            target = out / relative
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(source.read_bytes())
            report['newFiles'].append(relative.as_posix())

    # MSVC's `long double` is binary64. Emscripten's is binary128. Preserve
    # the original Windows numeric contract by spelling those engine/game
    # values through the Web port's explicit binary64 alias.
    precision_files = list((out / 'source/TouhouDanmakufu').rglob('*.cpp'))
    precision_files += list((out / 'source/TouhouDanmakufu').rglob('*.hpp'))
    precision_files += [out / 'source/GcLib/gstd/MersenneTwister.cpp',
                        out / 'source/GcLib/gstd/MersenneTwister.hpp']
    precision_changed = []
    for path in precision_files:
        if not path.is_file():
            continue
        text = path.read_text(encoding='utf-8')
        replaced = text.replace('long double', 'gstd::script_real')
        if replaced != text:
            path.write_text(replaced, encoding='utf-8', newline='\n')
            precision_changed.append(path.relative_to(out).as_posix())
    report['binary64Adapted'] = precision_changed

    # Win32 min/max macros accept mixed arithmetic types; std::min/std::max do
    # not. Supply only mixed-type overloads so the old expressions preserve the
    # Windows conversion behavior while same-type calls still use std::min/max.
    constants = out / 'source/GcLib/gstd/GstdConstant.hpp'
    text = constants.read_text(encoding='utf-8')
    anchor = 'using std::max;\nusing std::min;\n'
    helpers = '''using std::max;\nusing std::min;\ntemplate<class A, class B, typename std::enable_if<!std::is_same<typename std::decay<A>::type, typename std::decay<B>::type>::value, int>::type = 0>
constexpr typename std::common_type<A, B>::type min(A a, B b) {
\tusing C = typename std::common_type<A, B>::type;
\treturn static_cast<C>(a) < static_cast<C>(b) ? static_cast<C>(a) : static_cast<C>(b);
}
template<class A, class B, typename std::enable_if<!std::is_same<typename std::decay<A>::type, typename std::decay<B>::type>::value, int>::type = 0>
constexpr typename std::common_type<A, B>::type max(A a, B b) {
\tusing C = typename std::common_type<A, B>::type;
\treturn static_cast<C>(a) > static_cast<C>(b) ? static_cast<C>(a) : static_cast<C>(b);
}
'''
    if anchor not in text:
        raise ValueError('Portable min/max anchor changed')
    text = text.replace('#include <vector>\n', '#include <vector>\n#include <type_traits>\n', 1)
    constants.write_text(text.replace(anchor, helpers, 1), encoding='utf-8', newline='\n')

    # Portable C++ cannot bind the temporary returned by GetMesh() to a
    # non-const lvalue reference. Holding a ref_count_ptr value preserves the
    # same mesh object and lifetime.
    system = out / 'source/TouhouDanmakufu/Common/StgSystem.cpp'
    text = system.read_text(encoding='utf-8')
    old = 'gstd::ref_count_ptr<DxMesh>& mesh = objMesh->GetMesh();'
    count = text.count(old)
    if count != 2:
        raise ValueError(f'Expected two StgSystem mesh-ref portability sites, found {count}')
    system.write_text(text.replace(old, 'gstd::ref_count_ptr<DxMesh> mesh = objMesh->GetMesh();'),
                      encoding='utf-8', newline='\n')
    stage_script = out / 'source/TouhouDanmakufu/Common/StgStageScript.cpp'
    text = stage_script.read_text(encoding='utf-8')
    old = 'return script->CreateRealArrayValue(std::vector<gstd::script_real>());'
    if text.count(old) != 1:
        raise ValueError('Expected one temporary real-array portability site')
    text = text.replace(old, '{\n\t\t\t\tstd::vector<gstd::script_real> empty;\n\t\t\t\treturn script->CreateRealArrayValue(empty);\n\t\t\t}', 1)
    stage_script.write_text(text, encoding='utf-8', newline='\n')
    report['portableCompileFixes'] = ['mixed-type Win32 min/max semantics',
                                      'StgSystem temporary ref_count_ptr binding',
                                      'StgStageScript temporary vector binding']

    if args.boundary_hooks:
        controller = out / 'source/TouhouDanmakufu/Common/StgStageController.cpp'
        text = controller.read_text(encoding='utf-8')
        include_candidates = ('#include "StgStageSystem.hpp"', '#include "StgSystem.hpp"',
                              '#include"StgStageSystem.hpp"', '#include"StgSystem.hpp"')
        include_anchor = next((value for value in include_candidates if value in text), None)
        if include_anchor is None:
            raise ValueError('StgStageController include anchor changed')
        text = text.replace(include_anchor, include_anchor + '\n#include "../../../portable/replay/Boundary.hpp"', 1)
        init_anchor = '\tinfoStage_->SetStageStartTime(timeGetTime());\n\n\n}\nvoid StgStageController::CloseScene()\n{\n'
        if init_anchor not in text:
            raise ValueError('StgStageController lifecycle anchor changed')
        text = text.replace(
            init_anchor,
            '\tinfoStage_->SetStageStartTime(timeGetTime());\n\twoo::replay::Boundary(this, true);\n\n\n}\nvoid StgStageController::CloseScene()\n{\n\twoo::replay::Boundary(this, false);\n',
            1)
        controller.write_text(text, encoding='utf-8', newline='\n')
        report['boundaryHooks'] = True
    else:
        report['boundaryHooks'] = False

    report_path = out.parent / 'merge-report.json'
    report_path.write_text(json.dumps(report, indent=2), encoding='utf-8')
    print(json.dumps({'merged': len(report['merged']), 'conflicts': report['conflicts'],
                      'platformPreferred': len(report['platformPreferred']),
                      'originalPreferred': len(report['originalPreferred']),
                      'binary64Adapted': len(report['binary64Adapted']),
                      'newFiles': len(report['newFiles']), 'output': str(out)}, indent=2))
    if report['conflicts']:
        raise SystemExit(2)


if __name__ == '__main__':
    main()
