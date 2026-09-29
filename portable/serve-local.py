"""Serve the private Woo preview on loopback only; never modifies game files."""
from __future__ import annotations

import argparse
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import json
import mimetypes
from pathlib import Path
import shutil
from urllib.parse import quote, unquote, urlsplit

ROOT = Path(__file__).resolve().parents[1]
MIME = {".mjs": "text/javascript; charset=utf-8", ".wasm": "application/wasm",
        ".json": "application/json; charset=utf-8", ".html": "text/html; charset=utf-8"}


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", type=int, default=8797)
    parser.add_argument("--runtime", type=Path, default=ROOT / "build/web")
    parser.add_argument("--game-root", type=Path, default=ROOT.parents[1] / "games/th_hsob_ver1.12/dnh")
    parser.add_argument("--font", type=Path, default=Path("C:/Windows/Fonts/msgothic.ttc"))
    args = parser.parse_args()
    game = args.game_root.resolve()
    runtime = args.runtime.resolve()
    effects = (ROOT / "build/effects").resolve()
    font = args.font.resolve()
    if not (game / "script/th_hsob/Main.dnh").is_file():
        raise FileNotFoundError("Hollow Song of Birds game files not found")
    if not font.is_file():
        raise FileNotFoundError("A local Japanese font is required: use --font")

    class Handler(BaseHTTPRequestHandler):
        def log_message(self, format: str, *values: object) -> None:
            if values and str(values[1] if len(values) > 1 else "") != "200":
                super().log_message(format, *values)

        def send_bytes(self, data: bytes, content_type: str) -> None:
            self.send_response(200)
            self.send_header("Content-Type", content_type)
            self.send_header("Content-Length", str(len(data)))
            self.send_header("Cache-Control", "no-store")
            self.send_header("X-Content-Type-Options", "nosniff")
            self.end_headers()
            if self.command != "HEAD":
                self.wfile.write(data)

        def manifest(self) -> dict:
            files = []
            # Enumerate only this explicit game root, not the workspace.
            for path in sorted(game.rglob("*")):
                if not path.is_file() or path.suffix.lower() in (".exe", ".dll", ".lnk", ".log"):
                    continue
                relative = path.relative_to(game).as_posix()
                files.append({"path": "/game/" + relative, "url": "/assets/" + quote(relative), "bytes": path.stat().st_size})
            launcher = game.parent / "launcher.ini"
            if launcher.is_file():
                files.append({"path": "/launcher.ini", "url": "/launcher.ini", "bytes": launcher.stat().st_size})
            files.append({"path": "/game/msgothic.ttc", "url": "/local-font", "bytes": font.stat().st_size})
            effect_manifest = effects / "effects.json"
            if not effect_manifest.is_file():
                raise FileNotFoundError("Run python portable/compile-effects.py first")
            metadata = json.loads(effect_manifest.read_text(encoding="utf-8"))
            if metadata["unsupported"]:
                raise ValueError("Some game effects have not compiled; see build/effects/effects.json")
            for item in metadata["effects"]:
                sidecar = item["sidecar"]
                local = effects / sidecar.lstrip("/")
                files.append({"path": sidecar, "url": "/effects" + quote(sidecar), "bytes": local.stat().st_size})
            return {"game": "Hollow Song of Birds 1.12", "files": files}

        def do_HEAD(self) -> None:
            self.do_GET()

        def do_GET(self) -> None:
            # Loopback binding plus a Host check prevents accidental LAN serving
            # and DNS-rebinding access to the user's private game/font files.
            if self.headers.get("Host", "").split(":")[0].lower() not in ("127.0.0.1", "localhost"):
                self.send_error(403)
                return
            route = unquote(urlsplit(self.path).path)
            try:
                if route == "/local-manifest.json":
                    self.send_bytes(json.dumps(self.manifest(), ensure_ascii=False).encode("utf-8"), MIME[".json"])
                    return
                if route == "/local-font":
                    path, base = font, font.parent
                elif route == "/launcher.ini":
                    path, base = game.parent / "launcher.ini", game.parent
                elif route.startswith("/assets/"):
                    base = game
                    path = (base / route.removeprefix("/assets/")).resolve()
                elif route.startswith("/effects/"):
                    base = effects
                    path = (base / route.removeprefix("/effects/")).resolve()
                else:
                    base = runtime
                    path = (base / ("index.html" if route == "/" else route.lstrip("/"))).resolve()
                if not path.is_relative_to(base) or not path.is_file():
                    self.send_error(404)
                    return
                self.send_response(200)
                self.send_header("Content-Type", MIME.get(path.suffix, mimetypes.guess_type(path)[0] or "application/octet-stream"))
                self.send_header("Content-Length", str(path.stat().st_size))
                self.send_header("Cache-Control", "no-store")
                self.send_header("X-Content-Type-Options", "nosniff")
                self.end_headers()
                if self.command != "HEAD":
                    with path.open("rb") as stream:
                        shutil.copyfileobj(stream, self.wfile, length=1024 * 1024)
            except (BrokenPipeError, ConnectionResetError):
                pass
            except Exception as error:
                self.send_error(500, str(error))

    server = ThreadingHTTPServer(("127.0.0.1", args.port), Handler)
    print(f"Woo local preview: http://127.0.0.1:{args.port}/", flush=True)
    print(f"Game assets are read-only: {game}", flush=True)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close()


if __name__ == "__main__":
    main()
