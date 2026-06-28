#!/usr/bin/env python3
"""Generates test_data/sample.las: a small (1000-point), fully deterministic
LAS 1.2 / point-data-format-2 file committed to git, so CI (and any fresh
clone) has a real point cloud fixture to test against without needing the
much larger, gitignored test_data/office.las.

Points form a 10x10x10 grid at integer coordinates 0..9 on each axis, so the
expected bounding box/point count are exact and trivial to hardcode in
tests (see Tests/converter/test_las_reader.cpp). RGB cycles through a
simple deterministic pattern, mirroring the synthetic-LAS convention already
used by Tests/viewer/integration/test_converter_viewer_integration.cpp.

Re-run this script (no arguments) to regenerate the file if the format ever
needs to change; the output is fully deterministic.
"""
import struct
import datetime

OUT_PATH = "test_data/sample.las"
GRID = range(10)  # 10x10x10 = 1000 points, coordinates 0..9 per axis
SCALE = 0.001
OFFSET = 0.0

HEADER_SIZE = 227
POINT_FORMAT = 2
POINT_RECORD_LEN = 26


def build_points():
    points = []
    i = 0
    for x in GRID:
        for y in GRID:
            for z in GRID:
                r = i % 256
                g = (i * 3) % 256
                b = (i * 7) % 256
                points.append((x, y, z, r, g, b))
                i += 1
    return points


def pack_header(num_points, mins, maxs):
    today = datetime.date.today()
    header = bytearray(HEADER_SIZE)
    header[0:4] = b"LASF"
    struct.pack_into("<H", header, 4, 0)
    struct.pack_into("<H", header, 6, 0)
    header[24] = 1
    header[25] = 2
    header[26:58] = b"PCLite".ljust(32, b"\x00")
    header[58:90] = b"generate_sample_las.py".ljust(32, b"\x00")
    struct.pack_into("<H", header, 90, today.timetuple().tm_yday)
    struct.pack_into("<H", header, 92, today.year)
    struct.pack_into("<H", header, 94, HEADER_SIZE)
    struct.pack_into("<I", header, 96, HEADER_SIZE)
    struct.pack_into("<I", header, 100, 0)
    header[104] = POINT_FORMAT
    struct.pack_into("<H", header, 105, POINT_RECORD_LEN)
    struct.pack_into("<I", header, 107, num_points)
    struct.pack_into("<IIIII", header, 111, num_points, 0, 0, 0, 0)
    struct.pack_into("<ddd", header, 131, SCALE, SCALE, SCALE)
    struct.pack_into("<ddd", header, 155, OFFSET, OFFSET, OFFSET)
    struct.pack_into("<dddddd", header, 179,
                      maxs[0], mins[0], maxs[1], mins[1], maxs[2], mins[2])
    return bytes(header)


def main():
    points = build_points()
    mins = (min(p[0] for p in points), min(p[1] for p in points), min(p[2] for p in points))
    maxs = (max(p[0] for p in points), max(p[1] for p in points), max(p[2] for p in points))

    with open(OUT_PATH, "wb") as f:
        f.write(pack_header(len(points), mins, maxs))
        for x, y, z, r, g, b in points:
            ix = round(x / SCALE)
            iy = round(y / SCALE)
            iz = round(z / SCALE)
            f.write(struct.pack(
                "<iiiHBBbBHHHH",
                ix, iy, iz,
                0,            # intensity
                0x09,         # return number=1, number of returns=1
                2,            # classification
                0,            # scan angle rank
                0,            # user data
                0,            # point source id
                r * 257, g * 257, b * 257,
            ))

    print(f"Wrote {len(points)} points to {OUT_PATH}")
    print(f"Bounds: min={mins} max={maxs}")


if __name__ == "__main__":
    main()
