"""Compile the D3D9 pixel-effect envelopes used by Woo into WebGL2 shaders.

The original FX bytes are embedded in each sidecar and checked by the runtime.
Game files are read-only; all derivatives go to the private build directory.
Only recognized pass and sampler states are accepted, never silently omitted.
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import re
import struct
import subprocess

ROOT = Path(__file__).resolve().parents[1]
WORKSPACE = ROOT.parents[2]
TOOLS = WORKSPACE / "toolchains/ph3-shaders"


def run(arguments: list[object]) -> str:
    result = subprocess.run([str(item) for item in arguments], capture_output=True, text=True)
    if result.returncode:
        raise RuntimeError(result.stdout + result.stderr)
    return result.stdout


def blob(value: str | bytes) -> bytes:
    data = value.encode("utf-8") if isinstance(value, str) else value
    return struct.pack("<I", len(data)) + data


def lower_d3d9_trig(glsl: str) -> str:
    """Mirror FXC ps_2_0/ps_3_0 scalar sin/cos argument reduction.

    FXC lowers HLSL sin/cos to a frac-based wrap into [-pi, pi] before the
    shader-model-3 sincos instruction.  Leaving SPIR-V Cross' GLSL sin/cos on
    the unreduced argument is mathematically equivalent but not numerically
    equivalent enough for hash-style procedural noise such as
    frac(sin(x)*43758).  Preserve FXC's float constants/instruction order so
    WebGL sees the same reduced input before its native trig approximation.
    """
    replaced = re.sub(r"(?<![A-Za-z0-9_])sin\(", "ph3_d3d9_sin(", glsl)
    replaced = re.sub(r"(?<![A-Za-z0-9_])cos\(", "ph3_d3d9_cos(", replaced)
    if replaced == glsl:
        return glsl
    helpers = """
highp float ph3_d3d9_wrap_angle(highp float x) {
    return fract(x * 0.159154937 + 0.5) * 6.28318548 - 3.14159274;
}
highp float ph3_d3d9_sin(highp float x) { return sin(ph3_d3d9_wrap_angle(x)); }
highp float ph3_d3d9_cos(highp float x) { return cos(ph3_d3d9_wrap_angle(x)); }
"""
    marker = "precision highp int;\n"
    if marker not in replaced:
        raise ValueError("Missing GLSL precision preamble")
    return replaced.replace(marker, marker + helpers, 1)


def compile_effect(path: Path, virtual: str, output: Path, scratch: Path,
                   glslang: Path, cross: Path) -> dict:
    raw = path.read_bytes()
    text = raw.decode("utf-8-sig") if raw.startswith(b"\xef\xbb\xbf") else raw.decode("cp932")
    text = re.sub(r"/\*.*?\*/|//[^\n]*", lambda match: "\n" * match[0].count("\n"), text, flags=re.S)
    effect = re.search(r"\btechnique\b", text)
    if not effect:
        raise ValueError("Missing technique")
    envelope = re.fullmatch(
        r"\s*technique\s+(\w+)\s*\{\s*pass\s+\w+\s*\{\s*"
        r"PixelShader\s*=\s*compile\s+ps_[23]_0\s+(\w+)\s*\(\s*\)\s*;\s*\}\s*\}\s*",
        text[effect.start():])
    if not envelope:
        raise ValueError("Unsupported FX technique/pass state")
    technique, entry = envelope.groups()
    text = text[:effect.start()]
    textures = set(re.findall(r"\btexture\s+(\w+)\s*;", text))
    text = re.sub(r"\btexture\s+\w+\s*;", "", text)
    samplers: dict[str, str] = {}
    sampler_states: dict[str, list[int]] = {}
    inherit = 0xffffffff

    def sampler(match: re.Match) -> str:
        name, body = match.groups()
        settings = {}
        for part in body.split(";"):
            if not part.strip():
                continue
            key, value = (item.strip() for item in part.split("=", 1))
            if key in settings:
                raise ValueError("Duplicate sampler state")
            settings[key] = value
        allowed = {"Texture", "MipFilter", "MinFilter", "MagFilter", "AddressU", "AddressV"}
        if set(settings) - allowed:
            raise ValueError("Unsupported sampler state")
        texture = settings.get("Texture", "").strip("<>")
        if texture not in textures:
            raise ValueError("Unknown texture parameter")
        state_values = []
        for key in ("MinFilter", "MagFilter", "MipFilter", "AddressU", "AddressV"):
            # Omitted FX state is inherited from the original D3D device slot;
            # it is NOT an implicit LINEAR override. Mask effects depend on it.
            value = settings.get(key)
            if value is None:
                state_values.append(inherit)
                continue
            accepted = ({"NONE": 0, "POINT": 1, "LINEAR": 2} if key.endswith("Filter")
                        else {"1": 1, "WRAP": 1, "MIRROR": 2, "CLAMP": 3})
            if value not in accepted:
                raise ValueError(f"Unsupported sampler state {key}={value}")
            state_values.append(accepted[value])
        samplers[name] = texture
        sampler_states[name] = state_values
        return f"sampler2D {name};"

    text = re.sub(r"\bsampler\s+(\w+)\s*=\s*sampler_state\s*\{([^{}]*)\}\s*;", sampler, text)

    def register(match: re.Match) -> str:
        name, slot = match.groups()
        if slot != "0":
            raise ValueError("Only the object texture register s0 is supported")
        samplers[name] = ""
        sampler_states[name] = [inherit] * 5
        return f"sampler2D {name} : register(s0);"

    text = re.sub(r"\bsampler\s+(\w+)\s*:\s*register\s*\(\s*s(\d+)\s*\)\s*;", register, text)
    if re.search(r"\b(texture|sampler|sampler_state|technique|pass)\b", text):
        raise ValueError("Unsupported FX declaration")
    scratch.mkdir(parents=True, exist_ok=True)
    hlsl, spirv = scratch / "input.frag.hlsl", scratch / "output.spv"
    hlsl.write_text(text, encoding="utf-8")
    run([glslang, "-D", "-V", "--hlsl-dx9-compatible", "--auto-map-bindings",
         "--auto-map-locations", "-S", "frag", "-e", entry, hlsl, "-o", spirv])
    reflection = json.loads(run([cross, spirv, "--reflect"]))
    interface = []
    for item in reflection.get("inputs", []):
        variable, field = item["name"].split(".", 1)
        signature = re.search(r"\b" + re.escape(entry) + r"\s*\(([^)]*)\)", text)
        parameter = re.search(r"\b(\w+)\s+" + re.escape(variable) + r"\b", signature[1]) if signature else None
        structure = re.search(r"\bstruct\s+" + re.escape(parameter[1]) + r"\s*\{([^}]+)\}", text) if parameter else None
        semantic = re.search(r"\b(float[234])\s+" + re.escape(field) + r"\s*:\s*(\w+)\b", structure[1]) if structure else None
        if not semantic:
            raise ValueError("Unknown pixel input")
        accepted = {("float2", "TEXCOORD0", "vec2"): "texcoord",
                    ("float4", "COLOR0", "vec4"): "tint",
                    ("float2", "VPOS", "vec2"): "pixelPosition"}
        key = (semantic[1], semantic[2], item["type"])
        if key not in accepted:
            raise ValueError("Unsupported pixel input: " + str(key))
        interface += ["--rename-interface-variable", "in", str(item["location"]), accepted[key]]
    glsl = run([cross, spirv, "--es", "--version", "300", "--glsl-emit-ubo-as-plain-uniforms", *interface])
    glsl = lower_d3d9_trig(glsl)
    prefix = ""
    if reflection.get("ubos"):
        if len(reflection["ubos"]) != 1 or reflection["ubos"][0]["name"] != "$Global":
            raise ValueError("Unsupported uniform block")
        match = re.search(r"uniform _Global (\w+);", glsl)
        if not match:
            raise ValueError("Missing emitted global uniform")
        prefix = match[1] + "."
    outputs = reflection.get("outputs", [])
    if len(outputs) != 1 or outputs[0]["type"] != "vec4" or outputs[0]["location"] != 0:
        raise ValueError("Expected COLOR0 output")
    fragment_output = re.search(r"layout\(location = 0\) out highp vec4 (\w+);", glsl)
    if not fragment_output:
        raise ValueError("Missing output")
    glsl = glsl.replace("void main()", "void ph3_effect_main()")
    # D3D9 alpha testing happens after the pixel shader, so custom FX must see
    # the same ALPHATESTENABLE/FUNC/REF state as fixed-function draws.  The old
    # Web wrapper approximated that with an unconditional alpha<=0 discard,
    # which was wrong whenever alpha test was disabled or used another ref/func.
    # Explicit HLSL clip/discard survives through glslang/spirv-cross.
    glsl += """
uniform bool ph3AlphaTest;
uniform int ph3AlphaFunc;
uniform float ph3AlphaRef;
bool ph3_alpha_pass(float a) {
    if (ph3AlphaFunc == 1) return false;
    if (ph3AlphaFunc == 2) return a < ph3AlphaRef;
    if (ph3AlphaFunc == 3) return a == ph3AlphaRef;
    if (ph3AlphaFunc == 4) return a <= ph3AlphaRef;
    if (ph3AlphaFunc == 5) return a > ph3AlphaRef;
    if (ph3AlphaFunc == 6) return a != ph3AlphaRef;
    if (ph3AlphaFunc == 7) return a >= ph3AlphaRef;
    return true;
}
"""
    glsl += (f"\nvoid main() {{ ph3_effect_main(); "
             f"{fragment_output[1]}=clamp({fragment_output[1]},0.0,1.0); "
             f"if (ph3AlphaTest && !ph3_alpha_pass({fragment_output[1]}.a)) discard; }}\n")
    bindings = []
    for texture in reflection.get("textures", []):
        if texture["type"] != "sampler2D" or texture["name"] not in samplers:
            raise ValueError("Unsupported reflected sampler")
        bindings.append({"uniform": texture["name"], "parameter": samplers[texture["name"]],
                         "slot": texture["binding"], "states": sampler_states[texture["name"]]})
    if reflection.get("separate_samplers") or reflection.get("separate_images"):
        raise ValueError("Uncombined D3D9 sampler")
    bundle = b"PH3FX002" + blob(raw) + blob(technique) + blob(glsl) + blob(prefix) + struct.pack("<I", len(bindings))
    for binding in bindings:
        bundle += blob(binding["uniform"]) + blob(binding["parameter"])
        bundle += struct.pack("<6I", binding["slot"], *binding["states"])
    destination = output / (virtual.lstrip("/") + ".pfx")
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_bytes(bundle)
    (scratch / (path.stem + ".glsl")).write_text(glsl, encoding="utf-8")
    return {"path": virtual, "sidecar": virtual + ".pfx", "sourceSha256": hashlib.sha256(raw).hexdigest(),
            "compiledSha256": hashlib.sha256(bundle).hexdigest(), "technique": technique, "samplers": bindings}


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--game-root", type=Path, default=ROOT.parents[1] / "games/th_hsob_ver1.12/dnh")
    parser.add_argument("--out", type=Path, default=ROOT / "build/effects")
    args = parser.parse_args()
    glslang = TOOLS / "glslang/bin/glslang.exe"
    cross = TOOLS / "cross-build/Release/spirv-cross.exe"
    for tool in (glslang, cross):
        if not tool.is_file():
            raise FileNotFoundError(f"Shader compiler tool is missing: {tool}")
    effects, unsupported = [], []
    directory = args.game_root / "script/th_hsob"
    # Player/equipment and stage-local effects are as important as shared FX.
    # Compile the explicit game's complete effect set, never just the title's
    # shared shader directory; keep missing/unsupported shaders fail-closed.
    for path in sorted(directory.rglob("*")):
        if not path.is_file() or path.suffix.lower() not in (".fx", ".txt"):
            continue
        if path.suffix.lower() == ".txt" and not re.search(rb"\btechnique\b", path.read_bytes()):
            continue
        virtual = "/game/" + path.relative_to(args.game_root).as_posix()
        try:
            scratch = ROOT / "build/fx" / path.relative_to(directory).with_suffix("")
            effects.append(compile_effect(path, virtual, args.out, scratch, glslang, cross))
            print("Compiled", virtual)
        except (ValueError, RuntimeError) as error:
            unsupported.append({"path": virtual, "reason": str(error)})
            print("Unsupported", virtual, str(error))
    manifest = {"effects": effects, "unsupported": unsupported,
                "toolchain": {tool.name: hashlib.sha256(tool.read_bytes()).hexdigest() for tool in (glslang, cross)},
                "converterSha256": hashlib.sha256(Path(__file__).read_bytes()).hexdigest()}
    args.out.mkdir(parents=True, exist_ok=True)
    (args.out / "effects.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    if unsupported:
        raise SystemExit(2)


if __name__ == "__main__":
    main()
