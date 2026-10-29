"""Build/run native or Web primitive probes separately; never bless candidate results."""
from __future__ import annotations
import argparse
import hashlib
import importlib.util
import json
import os
import os
from pathlib import Path
import subprocess
from xml.sax.saxutils import escape

from paths import ROOT
from project import emscripten


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('mode', choices=['native','web'])
    args = parser.parse_args()
    out = ROOT / ('build/primitives-' + args.mode)
    out.mkdir(parents=True, exist_ok=True)
    probe = ROOT / 'tests/cpp/replay-primitives.cpp'
    env = dict(os.environ)
    if args.mode == 'native':
        spec = importlib.util.spec_from_file_location('native', ROOT / 'tools/replay/build-native.py')
        native = importlib.util.module_from_spec(spec);spec.loader.exec_module(native)
        env, install = native.msvc_environment()
        source = ROOT / 'build/native-oracle/source/source/GcLib/gstd'
        if not (source / 'MersenneTwister.cpp').is_file():
            raise FileNotFoundError('Prepare/build the pinned native oracle first')
        project = out / 'primitives.vcxproj'
        project.write_text(f'''<Project DefaultTargets="Build" xmlns="http://schemas.microsoft.com/developer/msbuild/2003">
<ItemGroup Label="ProjectConfigurations"><ProjectConfiguration Include="Release|Win32"><Configuration>Release</Configuration><Platform>Win32</Platform></ProjectConfiguration></ItemGroup>
<PropertyGroup Label="Globals"><WindowsTargetPlatformVersion>10.0</WindowsTargetPlatformVersion></PropertyGroup>
<Import Project="$(VCTargetsPath)Microsoft.Cpp.Default.props"/>
<PropertyGroup Label="Configuration"><ConfigurationType>Application</ConfigurationType><PlatformToolset>v143</PlatformToolset><CharacterSet>Unicode</CharacterSet></PropertyGroup>
<Import Project="$(VCTargetsPath)Microsoft.Cpp.props"/>
<PropertyGroup><OutDir>{escape(str(out))}/</OutDir><IntDir>{escape(str(out/'obj'))}/</IntDir><TargetName>primitives</TargetName></PropertyGroup>
<ItemDefinitionGroup><ClCompile><AdditionalIncludeDirectories>{escape(str(source))};%(AdditionalIncludeDirectories)</AdditionalIncludeDirectories><Optimization>MaxSpeed</Optimization><RuntimeLibrary>MultiThreaded</RuntimeLibrary><AdditionalOptions>/utf-8 /std:c++17 %(AdditionalOptions)</AdditionalOptions></ClCompile><Link><SubSystem>Console</SubSystem></Link></ItemDefinitionGroup>
<ItemGroup><ClCompile Include="{escape(str(probe))}"/><ClCompile Include="{escape(str(source/'MersenneTwister.cpp'))}"/></ItemGroup>
<Import Project="$(VCTargetsPath)Microsoft.Cpp.targets"/></Project>''', encoding='utf-8')
        command = [str(install/'MSBuild/Current/Bin/MSBuild.exe'), str(project), '/nologo', '/v:minimal', '/p:Configuration=Release', '/p:Platform=Win32']
        run = [str(out/'primitives.exe')]
    else:
        source = ROOT / 'source/GcLib/gstd'
        compiler, env = emscripten()
        command = [os.environ.get('PH3_PYTHON', 'python'), str(compiler), str(probe), str(source/'MersenneTwister.cpp'),
                   '-I'+str(source), '-std=c++17','-O2','-fexceptions','-sDEFAULT_TO_CXX=1',
                   '-sENVIRONMENT=node','-sEXIT_RUNTIME=1','-o',str(out/'primitives.cjs')]
        run = ['node',str(out/'primitives.cjs')]
    with (out/'build.log').open('wb') as log:
        result=subprocess.run(command,cwd=ROOT,env=env,stdout=log,stderr=subprocess.STDOUT,timeout=180)
    if result.returncode:
        print((out/'build.log').read_text(encoding='utf-8',errors='replace')[-12000:]);raise SystemExit(result.returncode)
    result=subprocess.run(run,cwd=ROOT,env=env,capture_output=True,check=True,timeout=30)
    output=json.loads(result.stdout)
    output['provenance']={'mode':args.mode,'probeSha256':hashlib.sha256(probe.read_bytes()).hexdigest(),
                          'rngSourceSha256':hashlib.sha256((source/'MersenneTwister.cpp').read_bytes()).hexdigest()}
    (out/'result.json').write_text(json.dumps(output,indent=2),encoding='utf-8')
    print(json.dumps(output))


if __name__=='__main__':main()
