"""Local resource locations shared by development tools."""
from pathlib import Path
import os

ROOT = Path(__file__).resolve().parents[1]


def default_game_root() -> Path:
    if os.environ.get('PH3_GAME_ROOT'):
        return Path(os.environ['PH3_GAME_ROOT']).resolve()
    candidates = [parent / suffix for parent in [ROOT, *ROOT.parents[:3]]
                  for suffix in ['fangame/games/th_hsob_ver1.12/dnh', 'games/th_hsob_ver1.12/dnh']]
    return next((path for path in candidates if (path / 'script/th_hsob/Main.dnh').is_file()), candidates[0])


def default_font() -> Path:
    return Path(os.environ.get('PH3_FONT', 'C:/Windows/Fonts/msgothic.ttc'))


def shader_tools() -> Path:
    if os.environ.get('PH3_SHADER_TOOLS'):
        return Path(os.environ['PH3_SHADER_TOOLS']).resolve()
    candidates = [parent / 'toolchains/ph3-shaders' for parent in [ROOT, *ROOT.parents[:3]]]
    return next((path for path in candidates if (path / 'origin.json').is_file()), candidates[0])


def emscripten() -> tuple[Path, dict]:
    configured = os.environ.get('EMSDK')
    candidates = [Path(configured)] if configured else [
        parent / 'toolchains/emsdk' for parent in [ROOT, *ROOT.parents[:3]]]
    for sdk in candidates:
        for relative in ['upstream/emscripten/emcc.py', 'install/emscripten/emcc.py']:
            compiler = sdk / relative
            if compiler.is_file():
                return compiler.resolve(), {**os.environ, 'EMSDK': str(sdk.resolve()),
                    'EM_CONFIG': os.environ.get('EM_CONFIG', str(sdk.resolve() / '.emscripten'))}
    raise FileNotFoundError('Set EMSDK to an installed Emscripten SDK')
