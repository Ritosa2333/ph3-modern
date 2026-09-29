"""Generate numeric D3D9 ABI constants from the installed MinGW SDK.

Only numeric interface values are emitted, not an alternate implementation of
Direct3D. The Web driver implements the used operations separately. Probe output
is generated in build/ and the tiny numeric table is reproducible/checkable.
"""
from pathlib import Path
import argparse
import hashlib
import re
import subprocess

ROOT = Path(__file__).resolve().parents[1]
PREFIXES = (
    "D3DCMP_", "D3DXIFF_", "D3DX_FILTER_", "D3DFMT_", "D3DPOOL_",
    "D3DCLEAR_", "D3DUSAGE_", "D3DLOCK_", "D3DFVF_", "D3DDECLTYPE_",
    "D3DDECLMETHOD_", "D3DDECLUSAGE_", "D3DPT_", "D3DTS_", "D3DRS_",
    "D3DTSS_", "D3DTOP_", "D3DTA_", "D3DSAMP_", "D3DTEXF_", "D3DTADDRESS_",
    "D3DBLEND_", "D3DBLENDOP_", "D3DCULL_", "D3DFILL_", "D3DSHADE_",
    "D3DFOG_", "D3DVBF_", "D3DRTYPE_", "D3DLIGHT_", "D3DCOLORWRITEENABLE_",
    "D3DSWAPEFFECT_", "D3DPRESENT_INTERVAL_", "D3DMULTISAMPLE_", "D3DCREATE_",
    "D3DDEVCAPS_", "D3DPMISCCAPS_", "D3DPTEXTURECAPS_", "D3DERR_",
)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--sdk", type=Path, default=Path("C:/msys64/ucrt64"))
    args = parser.parse_args()
    names = set()
    hashes = {}
    for name in ("d3d9.h", "d3d9types.h", "d3d9caps.h", "d3dx9.h", "d3dx9tex.h"):
        path = args.sdk / "include" / name
        text = path.read_text(encoding="utf-8")
        hashes[name] = hashlib.sha256(path.read_bytes()).hexdigest()
        text = re.sub(r"/\*.*?\*/|//[^\n]*", "", text, flags=re.S)
        for match in re.finditer(r"^\s*#define\s+(\w+)([^\n]*)", text, re.M):
            token, tail = match.groups()
            if token.startswith(PREFIXES) and not tail.startswith("("):
                names.add(token)
        for match in re.finditer(r"typedef\s+enum\b[^{}]*\{([^}]+)\}", text):
            for token in re.findall(r"(?:^|,)\s*(D3D\w+)", match[1]):
                if token.startswith(PREFIXES):
                    names.add(token)
    names.update(("D3DX_DEFAULT", "D3D_SDK_VERSION", "D3DADAPTER_DEFAULT", "D3DDEVTYPE_HAL", "D3DDEVTYPE_REF"))
    # Not a numeric enum: macro expands to an initializer fragment.
    names.discard("D3DFVF_RESERVED0") if False else None
    build = ROOT / "build/dx-constants"
    build.mkdir(parents=True, exist_ok=True)
    probe = build / "probe.cpp"
    probe.write_text("#include <d3d9.h>\n#include <d3dx9.h>\n#include <cstdio>\nint main(){\n" +
        "\n".join(f'printf("{name}=%u\\n", static_cast<unsigned>({name}));' for name in sorted(names)) + "\n}\n", encoding="utf-8")
    compiler = args.sdk / "bin/g++.exe"
    executable = build / "probe.exe"
    subprocess.run([str(compiler), "-std=c++17", "-static", str(probe), "-o", str(executable)], check=True)
    output = subprocess.check_output([str(executable)], text=True)
    rows = [line.split("=", 1) for line in output.splitlines()]
    destination = ROOT / "source/GcLib/portable/DxConstants.inc"
    preamble = ["// Generated numeric D3D9 interface values; do not edit by hand.",
                "// Reproduce with: python portable/generate-dx-constants.py"]
    preamble.extend(f"// {name} sha256: {digest}" for name, digest in hashes.items())
    constants = [f"constexpr DWORD {name} = 0x{int(value):08x}u;" for name, value in rows]
    destination.write_text("\n".join(preamble + constants) + "\n", encoding="utf-8")
    print(f"Generated {len(constants)} SDK-verified constants: {destination}")


if __name__ == "__main__":
    main()
