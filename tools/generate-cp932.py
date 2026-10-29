"""Generate numeric CP932 conversion tables from the host Windows API.

This captures Windows' actual best-fit encoding and duplicate-byte decoding,
instead of using UTF-8 or an identity stub for Japanese script source.
"""
from pathlib import Path
import ctypes
import hashlib
import json
import platform

ROOT = Path(__file__).resolve().parents[1]


def main():
    if platform.system() != "Windows":
        raise SystemExit("The reference CP932 table is generated on Windows.")
    kernel = ctypes.WinDLL("kernel32", use_last_error=True)
    decode = kernel.MultiByteToWideChar
    decode.argtypes = [ctypes.c_uint, ctypes.c_uint, ctypes.c_char_p, ctypes.c_int, ctypes.c_wchar_p, ctypes.c_int]
    encode = kernel.WideCharToMultiByte
    encode.argtypes = [ctypes.c_uint, ctypes.c_uint, ctypes.c_wchar_p, ctypes.c_int, ctypes.c_char_p, ctypes.c_int, ctypes.c_char_p, ctypes.POINTER(ctypes.c_int)]
    wide = ctypes.create_unicode_buffer(4)
    narrow = ctypes.create_string_buffer(8)
    decode_pairs, encode_pairs = [], []
    for code in range(65536):
        source = bytes([code]) if code < 256 else bytes([code >> 8, code & 255])
        if code >= 256 and not (0x81 <= code >> 8 <= 0x9f or 0xe0 <= code >> 8 <= 0xfc):
            continue
        count = decode(932, 8, source, len(source), wide, 4)
        if count == 1:
            decode_pairs.append((code, ord(wide[0])))
    for code in range(65536):
        used_default = ctypes.c_int()
        count = encode(932, 0, chr(code), 1, narrow, 8, None, ctypes.byref(used_default))
        if count in (1, 2) and not used_default.value:
            data = narrow.raw[:count]
            value = data[0] if count == 1 else (data[0] << 8) | data[1]
            encode_pairs.append((code, value))
    rows = ["// Generated Windows CP932 numeric mapping; run tools/generate-cp932.py.",
            "struct Cp932Pair { std::uint16_t source, destination; };"]
    for name, pairs in (("Cp932Decode", decode_pairs), ("Cp932Encode", encode_pairs)):
        rows.append(f"constexpr Cp932Pair {name}[] = {{")
        for offset in range(0, len(pairs), 8):
            rows.append("  " + ",".join(f"{{0x{a:04x},0x{b:04x}}}" for a, b in pairs[offset:offset + 8]) + ",")
        rows.append("};")
    destination = ROOT / "source/GcLib/portable/Cp932Table.inc"
    content = ("\n".join(rows) + "\n").encode("utf-8")
    destination.write_bytes(content)
    print(json.dumps({"decode": len(decode_pairs), "encode": len(encode_pairs),
                      "sha256": hashlib.sha256(content).hexdigest()}, indent=2))


if __name__ == "__main__":
    main()
