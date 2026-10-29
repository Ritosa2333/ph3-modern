"""Build an isolated pinned MSVC Woo source oracle; never modify the dirty baseline.

Extracted/transformed inputs are generated build artifacts. This does not prove
the shipped game executable was built from the same source revision.
"""
from __future__ import annotations
import argparse
import hashlib
import io
import json
import os
import shutil
from pathlib import Path
import subprocess
import tarfile

from paths import ROOT, source_repository
BASE = '97ca3b50329b134ed7f159d6dabbe7509292e8b3'
ORIGINAL_BASE = '5c92f28e17b6ef3721a3ac4caa9de3af3164b51e'


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def msvc_environment() -> tuple[dict, Path]:
    env = dict(os.environ)
    for key, value in {'SystemRoot': 'C:/Windows', 'SystemDrive': 'C:',
                       'ProgramFiles': 'C:/Program Files',
                       'ProgramFiles(x86)': 'C:/Program Files (x86)',
                       'ProgramW6432': 'C:/Program Files'}.items():
        env.setdefault(key, value)
    vswhere = Path('C:/Program Files (x86)/Microsoft Visual Studio/Installer/vswhere.exe')
    candidates = []
    if vswhere.is_file():
        found = subprocess.run([str(vswhere), '-latest', '-products', '*', '-property', 'installationPath'],
                               capture_output=True, text=True, env=env, timeout=30)
        candidates += [Path(line) for line in found.stdout.splitlines() if line.strip()]
    candidates += [Path('C:/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools')]
    install = next((p for p in candidates if (p / 'VC/Auxiliary/Build/vcvarsall.bat').is_file()), None)
    if install is None:
        raise FileNotFoundError('MSVC build tools were not found')
    # Microsoft.Cpp.props discovers compiler/SDK paths for a vcxproj itself.
    # Do not rely on developer-command-prompt batch parsing in an MCP shell.
    env.setdefault('PROCESSOR_ARCHITECTURE', 'AMD64')
    return env, install


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out', type=Path)
    parser.add_argument('--family', choices=['woo', 'original'], default='woo')
    parser.add_argument('--hook-spec', type=Path, help='Explicit diagnostic-only source transformations')
    parser.add_argument('--prepare-only', action='store_true')
    args = parser.parse_args()
    base = BASE if args.family == 'woo' else ORIGINAL_BASE
    source_repo = source_repository(args.family)
    output = (args.out or ROOT / ('build/native-oracle' if args.family == 'woo' else 'build/native-original')).resolve()
    if not output.is_relative_to((ROOT / 'build').resolve()):
        raise ValueError('Oracle artifacts must stay under this repository build/')
    source, binary, objects = output / 'source', output / 'bin', output / 'objects'
    for directory in (source, binary, objects):
        directory.mkdir(parents=True, exist_ok=True)
    archive = subprocess.run(['git', 'archive', '--format=tar', base], cwd=source_repo, capture_output=True, check=True).stdout
    generated: dict[str, bytes] = {}
    with tarfile.open(fileobj=io.BytesIO(archive)) as tar:
        for entry in tar.getmembers():
            target = (source / entry.name).resolve()
            if not target.is_relative_to(source) or entry.issym() or entry.islnk():
                raise ValueError('Unsafe source archive entry')
            if entry.isfile():
                generated[entry.name] = tar.extractfile(entry).read()
    transforms = []
    # The original resource script does not use MFC, only Win32 resource IDs.
    # Build Tools does not install MFC's afxres.h. Replace only that include in
    # the generated input, preserving the compiled game sources verbatim.
    resource = 'source/TouhouDanmakufu/DnhExecutor/DnhExecuter.rc'
    before = generated[resource]
    needle = b'#include "afxres.h"'
    if before.count(needle) == 1:
        generated[resource] = before.replace(needle, b'#include <windows.h>\n#ifndef IDC_STATIC\n#define IDC_STATIC (-1)\n#endif')
        transforms.append({'path': resource, 'purpose': 'Win32-only resource header without optional MFC',
                           'original': hashlib.sha256(before).hexdigest(), 'generated': hashlib.sha256(generated[resource]).hexdigest()})
    elif args.family != 'original' or b'#include <windows.h>' not in before:
        raise ValueError('Unexpected native resource include')
    if args.hook_spec:
        spec = json.loads(args.hook_spec.read_text(encoding='utf-8'))
        for item in spec.get('copies', []):
            origin = (ROOT / item['source']).resolve()
            target = (source / item['target']).resolve()
            if not origin.is_relative_to(ROOT) or not target.is_relative_to(source):
                raise ValueError('Invalid diagnostic source copy')
            revision = item.get('sourceRevision')
            if revision:
                generated[item['target']] = subprocess.run(
                    ['git', 'show', revision + ':' + item['source']], cwd=item.get('sourceRepo', ROOT),
                    capture_output=True, check=True, timeout=30).stdout
            else:
                generated[item['target']] = origin.read_bytes()
            if item.get('expectedSha256') and hashlib.sha256(generated[item['target']]).hexdigest() != item['expectedSha256']:
                raise ValueError('Pinned platform dependency changed: ' + item['source'])
            if item.get('transcodeCp932'):
                generated[item['target']] = generated[item['target']].decode('utf-8-sig').encode('cp932')
            transforms.append({'path': item['target'], 'purpose': item.get('role', 'Shared read-only diagnostic observer'),
                               'source': item['source'], 'generated': hashlib.sha256(generated[item['target']]).hexdigest(),
                               'sourceRevision': item.get('sourceRevision')})
        for change in spec['changes']:
            path = (source / change['path']).resolve()
            if not path.is_relative_to(source):
                raise ValueError('Invalid generated-source path')
            before = generated[change['path']]
            encoding = 'cp932' if args.family == 'original' else 'utf-8-sig'
            text = before.decode(encoding).replace('\r\n', '\n')
            if text.count(change['before']) != change.get('count', 1):
                raise ValueError('Diagnostic hook anchor mismatch: ' + change['path'] + ': ' + change['purpose'])
            text = text.replace(change['before'], change['after'])
            generated[change['path']] = text.encode('cp932' if args.family == 'original' else 'utf-8')
            transforms.append({'path': change['path'], 'purpose': change['purpose'],
                               'original': hashlib.sha256(before).hexdigest(), 'generated': hashlib.sha256(generated[change['path']]).hexdigest()})
    # Preserve timestamps of identical generated inputs for incremental builds.
    for name, data in generated.items():
        path = source / name
        if not path.is_file() or path.read_bytes() != data:
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(data)
    project = source / 'DnhExecutor.vcxproj'
    manifest = {'kind': 'Pinned source reference, not certified retail equivalent', 'family': args.family, 'base': base,
                'sourceRepository': str(source_repo),
                'archiveSha256': hashlib.sha256(archive).hexdigest(), 'transforms': transforms,
                'inputs': {name: hashlib.sha256(data).hexdigest() for name, data in generated.items()},
                'builderSha256': sha(Path(__file__)), 'compiled': False}
    (output / 'provenance.json').write_text(json.dumps(manifest, indent=2), encoding='utf-8')
    if args.prepare_only:
        print(source)
        return
    env, install = msvc_environment()
    msbuild = install / 'MSBuild/Current/Bin/MSBuild.exe'
    command = [str(msbuild), str(project), '/t:Build', '/m:2', '/nologo', '/v:minimal',
               '/p:Configuration=Release', '/p:Platform=Win32', '/p:PlatformToolset=v143',
               '/p:WindowsTargetPlatformVersion=10.0', '/p:OutDir=' + str(binary) + '/',
               '/p:IntDir=' + str(objects) + '/']
    manifest['command'] = command
    manifest['msbuildSha256'] = sha(msbuild)
    toolsets = sorted((install / 'VC/Tools/MSVC').iterdir(), key=lambda p: tuple(int(v) for v in p.name.split('.')))
    compiler = toolsets[-1] / 'bin/HostX86/x86/cl.exe'
    if compiler.is_file():
        manifest['compiler'] = {'path': str(compiler), 'sha256': sha(compiler), 'toolsetDirectory': toolsets[-1].name}
    with (output / 'build.log').open('wb') as log:
        result = subprocess.run(command, cwd=source, env=env, stdout=log, stderr=subprocess.STDOUT, timeout=900)
    manifest['exitCode'] = result.returncode
    manifest['changedDuringBuild'] = [name for name, expected in manifest['inputs'].items() if sha(source/name) != expected]
    executable = binary / 'DnhExecutor.exe'
    if result.returncode == 0 and executable.is_file() and not manifest['changedDuringBuild']:
        manifest['compiled'] = True
        manifest['executableSha256'] = sha(executable)
        manifest['executable'] = str(executable)
    (output / 'provenance.json').write_text(json.dumps(manifest, indent=2), encoding='utf-8')
    print(json.dumps({k: manifest[k] for k in ('compiled','exitCode','executableSha256') if k in manifest}))
    if result.returncode:
        text = (output / 'build.log').read_text(encoding='utf-8', errors='replace')
        print('\n'.join(line for line in text.splitlines() if 'error' in line.lower())[-16000:])
        raise SystemExit(result.returncode)
    if not manifest['compiled']:
        raise SystemExit('Oracle build identity incomplete or inputs changed while compiling')


if __name__ == '__main__':
    main()
