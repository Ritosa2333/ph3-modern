"""Read (never modify) Woo REPLAY/RecordBuffer fixtures and describe coverage.

The file is an input fixture, not a golden trace. Stored stage lengths and
scores are expectations to verify during playback, not completion evidence.
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import struct
from typing import Any

ROOT = Path(__file__).resolve().parents[2]


def _default_game_root() -> Path:
    candidates = [
        ROOT.parents[1] / "games/th_hsob_ver1.12/dnh",
        ROOT.parents[1] / "fangame/games/th_hsob_ver1.12/dnh",
    ]
    return next((path for path in candidates if (path / "script/th_hsob/Main.dnh").is_file()), candidates[0])


GAME = _default_game_root()


class Reader:
    def __init__(self, data: bytes) -> None:
        self.data, self.offset = data, 0

    def take(self, size: int) -> bytes:
        if size < 0 or size > len(self.data) - self.offset:
            raise ValueError(f"Truncated record at {self.offset}, requested {size}")
        start = self.offset
        self.offset += size
        return self.data[start:self.offset]

    def number(self, fmt: str) -> Any:
        return struct.unpack("<" + fmt, self.take(struct.calcsize("<" + fmt)))[0]

    def complete(self) -> None:
        if self.offset != len(self.data):
            raise ValueError(f"{len(self.data) - self.offset} trailing record bytes")


def records(data: bytes, depth: int = 0) -> dict[str, tuple[int, bytes]]:
    if depth > 32 or len(data) > 64 * 1024 * 1024:
        raise ValueError("Record exceeds inspection limits")
    reader = Reader(data)
    count = reader.number("i")
    if not 0 <= count <= 100000:
        raise ValueError("Invalid record count")
    result = {}
    for _ in range(count):
        kind = reader.number("b")
        key = reader.take(reader.number("i")).decode("cp932")
        value = reader.take(reader.number("i"))
        if key in result:
            raise ValueError("Duplicate record key: " + key)
        result[key] = (kind, value)
    reader.complete()
    return result


def scalar(entry: tuple[int, bytes]) -> Any:
    kind, data = entry
    fmt = {1: "?", 2: "i", 3: "f", 4: "d"}.get(kind)
    if fmt:
        return struct.unpack("<" + fmt, data)[0]
    if kind in (5, 7):
        return data.decode("cp932" if kind == 5 else "utf-16le")
    return {"bytes": len(data), "sha256": hashlib.sha256(data).hexdigest()}


def common_value(reader: Reader, depth: int = 0) -> Any:
    if depth > 32:
        raise ValueError("Common data nesting exceeds inspection limits")
    kind = reader.number("i")
    if kind == 0:
        return reader.number("d")
    if kind == 1:
        return reader.take(2).decode("utf-16le", errors="surrogatepass")
    if kind == 2:
        return reader.number("?")
    if kind == 3:
        count = reader.number("i")
        if not 0 <= count <= 1000000:
            raise ValueError("Invalid common array length")
        values = [common_value(reader, depth + 1) for _ in range(count)]
        return "".join(values) if values and all(isinstance(v, str) for v in values) else values
    raise ValueError(f"Unknown common value type {kind}")


def common_data(data: bytes) -> dict[str, Any]:
    source, result = records(data), {}
    for key, (_, raw) in source.items():
        size = source.get(key + "_size")
        if size is None:
            continue
        if scalar(size) != len(raw):
            raise ValueError("Common data size mismatch: " + key)
        reader = Reader(raw)
        result[key] = common_value(reader)
        reader.complete()
    return result


def inspect(path: Path, verbose: bool = False) -> dict:
    raw = path.read_bytes()
    if not raw.startswith(b"REPLAY"):
        raise ValueError(f"Not a Woo replay: {path}")
    root = records(raw[6:])
    if scalar(root["version"]) != 1:
        raise ValueError("Unsupported replay version")
    count = scalar(root["stageCount"])
    indices = struct.unpack("<" + "i" * count, root["stageIndexList"][1])
    if len(set(indices)) != count:
        raise ValueError("Duplicate replay stage")
    result = {
        "file": path.name, "sha256": hashlib.sha256(raw).hexdigest(),
        "bytes": len(raw), "version": 1,
        "player": scalar(root["playerScriptID"]),
        "comment": scalar(root["comment"]),
        "totalScore": struct.unpack("<q", root["totalScore"][1])[0],
        "userData": common_data(root["userData"][1]), "stages": [],
        "completion": "not-yet-played",
    }
    for index in indices:
        stage = records(root[f"stage{index}"][1])
        item = {"index": index}
        for key in ("mainScriptID", "mainScriptName", "mainScriptRelativePath", "frameEnd",
                    "randSeed", "playerScriptID", "playerLife", "playerBombCount", "playerPower"):
            item[key] = scalar(stage[key])
        for key in ("scoreStart", "scoreLast"):
            item[key] = struct.unpack("<q", stage[key][1])[0]
        areas = records(stage["mapCommonData"][1])
        item["commonDataAreas"] = list(areas)
        stage_data = common_data(areas["Stg"][1]) if "Stg" in areas else {}
        system_data = common_data(areas["System"][1]) if "System" in areas else {}
        item["selection"] = {k: stage_data.get(k) for k in
                             ("StageIndex", "Difficult", "DifficultEx", "PlayMode")}
        item["equipment"] = {k: system_data.get(k) for k in
                             ("PlayerType", "PlayerType_Base", "PlayerSubWeapon")}
        if verbose:
            item["commonData"] = {key: common_data(entry[1]) for key, entry in areas.items()}
            item["keys"] = {key: scalar(entry) for key, entry in records(stage["recordKey"][1]).items()}
        result["stages"].append(item)
    result["declaredFrames"] = sum(stage["frameEnd"] for stage in result["stages"])
    result["declaredMinutesAt60Hz"] = round(result["declaredFrames"] / 3600, 2)
    return result


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--game-root", type=Path, default=GAME)
    parser.add_argument("--out", type=Path, default=ROOT / "build/replay/corpus.json")
    parser.add_argument("--verbose", action="store_true")
    args = parser.parse_args()
    files = sorted((args.game_root / "script/th_hsob/replay").glob("Main_replay*.dat"))
    fixtures = [inspect(path, args.verbose) for path in files]
    result = {"schema": "woo/replay-corpus/v1", "gameRoot": str(args.game_root.resolve()),
              "fixtures": fixtures, "declaredFrames": sum(f["declaredFrames"] for f in fixtures),
              "note": "Declared metadata only. Completion and fidelity require actual playback."}
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(json.dumps(result, ensure_ascii=True, indent=2) + "\n", encoding="utf-8")
    for f in fixtures:
        print(json.dumps({k: f[k] for k in ("file", "player", "declaredMinutesAt60Hz", "userData")}, ensure_ascii=True))
        print("  stages:", ", ".join(f"{s['index']}:{s['frameEnd']}" for s in f["stages"]))
    print(f"Inspected {len(fixtures)} fixtures; {result['declaredFrames']} declared frames. {args.out}")


if __name__ == "__main__":
    main()
