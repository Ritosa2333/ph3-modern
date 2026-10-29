"""Read-only inventory of original replay coverage before choosing test fixtures.

RecordBuffer layout follows source/GcLib/gstd/File.cpp and DnhReplay.cpp.
No input bytes, scripts, RNG, lives or replay keys are changed.
"""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import struct


def records(data: bytes) -> dict[str, bytes]:
    offset = 0

    def take(size: int) -> bytes:
        nonlocal offset
        if size < 0 or offset + size > len(data):
            raise ValueError("Record exceeds its enclosing buffer")
        result = data[offset:offset + size]
        offset += size
        return result

    def integer() -> int:
        return struct.unpack("<i", take(4))[0]

    count = integer()
    if not 0 <= count <= 100000:
        raise ValueError("Invalid record count")
    result = {}
    for _ in range(count):
        take(1)  # RecordEntry type tag; each selected field has a known type.
        key = take(integer()).decode("utf-8")
        if key in result:
            raise ValueError(f"Duplicate key {key!r}")
        result[key] = take(integer())
    if offset != len(data):
        raise ValueError("Trailing bytes in record buffer")
    return result


def inspect(path: Path) -> dict:
    if path.stat().st_size > 16 * 1024 * 1024:
        raise ValueError("Replay exceeds the inventory size limit")
    data = path.read_bytes()
    if not data.startswith(b"REPLAY"):
        raise ValueError("Not a ph3 replay")
    root = records(data[6:])
    stage_count = struct.unpack("<i", root["stageCount"])[0]
    if not 0 <= stage_count <= 100:
        raise ValueError("Invalid stage count")
    indices = struct.unpack("<" + "i" * stage_count, root["stageIndexList"])
    stages = []
    for index in indices:
        stage = records(root[f"stage{index}"])
        common = records(stage['mapCommonData'])
        stg = records(common.get('Stg', struct.pack('<i', 0)))
        # HSoB fixture metadata only. Never infer difficulty from score/bullets
        # or change the recorded common data to create a stronger workload.
        settings = {}
        for key in ['Difficult', 'DifficultEx', 'PlayMode']:
            payload = stg.get(key)
            if payload and len(payload) == 12 and struct.unpack('<i', payload[:4])[0] == 0:
                settings[key] = struct.unpack('<d', payload[4:])[0]
        stages.append({
            "stage": index,
            "frameEnd": struct.unpack("<i", stage["frameEnd"])[0],
            "scoreLast": struct.unpack("<q", stage["scoreLast"])[0],
            "startLives": struct.unpack("<d", stage["playerLife"])[0],
            "hsobSettings": settings,
        })
    return {"fixture": path.name, "sha256": hashlib.sha256(data).hexdigest(), "stages": stages}


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("directory", type=Path)
    args = parser.parse_args()
    fixtures = sorted(args.directory.glob("Main_replay*.dat"))
    if not fixtures or len(fixtures) > 32:
        parser.error("Expected 1 to 32 main replay fixtures")
    print(json.dumps([inspect(path) for path in fixtures], indent=2))
