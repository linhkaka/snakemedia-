"""Set build provenance and generate an original, dependency-free SNAKEMEDIA icon.

Uses only Python standard library. Does not alter the upstream license or .starc format.
"""
from __future__ import annotations

import argparse
import binascii
import math
import re
import struct
import zlib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def png(size=256):
    # Rounded emerald tile, inset frame and a geometric letter S. Supersampled for small sizes.
    factor = 3
    scale = size * factor
    image = bytearray()
    polyline = [(72, 29), (38, 29), (28, 38), (28, 44), (38, 50),
                (62, 50), (72, 57), (72, 63), (62, 72), (28, 72)]

    def distance(x, y, a, b):
        vx, vy = b[0] - a[0], b[1] - a[1]
        t = max(0, min(1, ((x - a[0]) * vx + (y - a[1]) * vy) / (vx * vx + vy * vy)))
        return math.hypot(x - a[0] - t * vx, y - a[1] - t * vy)

    def pixel(x, y):
        dx, dy = max(0, abs(x - 50) - 34), max(0, abs(y - 50) - 34)
        if math.hypot(dx, dy) > 13:
            return (0, 0, 0, 0)
        if min(distance(x, y, a, b) for a, b in zip(polyline, polyline[1:])) <= 4.7:
            return (211, 237, 181, 255)
        return (25, 59, 44, 255)

    for y in range(size):
        image.append(0)
        for x in range(size):
            colors = [pixel((x * factor + xx + 0.5) / scale * 100,
                            (y * factor + yy + 0.5) / scale * 100)
                      for yy in range(factor) for xx in range(factor)]
            image.extend(round(sum(c[k] for c in colors) / len(colors)) for k in range(4))

    def chunk(kind, value):
        return struct.pack(">I", len(value)) + kind + value + struct.pack(">I", binascii.crc32(kind + value) & 0xffffffff)

    return (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", size, size, 8, 6, 0, 0, 0))
            + chunk(b"IDAT", zlib.compress(bytes(image), 9)) + chunk(b"IEND", b""))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--repository", default="", help="GitHub owner/repo for corresponding build source")
    args = parser.parse_args()
    if args.repository and not re.fullmatch(r"[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+", args.repository):
        parser.error("repository must be owner/repo")
    header = ROOT / "src/include/snakemedia_brand.h"
    source = "https://github.com/" + args.repository if args.repository else ""
    text = header.read_text(encoding="utf-8")
    text, count = re.subn(r'kSourceUrl\[\] = "[^"]*";', 'kSourceUrl[] = "' + source + '";', text)
    if count != 1:
        raise SystemExit("brand header missing source URL marker")
    header.write_text(text, encoding="utf-8")
    icon = png()
    (ROOT / "src/core/resources/images/snakemedia.png").write_bytes(icon)
    ico = struct.pack("<HHH", 0, 1, 1) + struct.pack("<BBBBHHII", 0, 0, 0, 0, 1, 32, len(icon), 22) + icon
    (ROOT / "src/app/snakemedia.ico").write_bytes(ico)
    print("SNAKEMEDIA brand configured; source: " + (source or "local build (not yet published)"))


if __name__ == "__main__":
    main()
