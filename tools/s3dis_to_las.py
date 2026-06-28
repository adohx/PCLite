#!/usr/bin/env python3
"""Convert the Stanford S3DIS (v1.2 Aligned) dataset zip into per-Area LAS files.

The zip contains one ASCII "x y z r g b" file per room
(Area_N/<room>/<room>.txt). Rooms inside the same Area already share a
coordinate system, so all rooms belonging to one Area are merged into a
single LAS file as-is (no coordinate shifting needed); each of the 6
Areas gets its own output file.

Output is LAS 1.2, point data format 2 (XYZ + RGB, no GPS time), which is
exactly what PCLite's LasReader expects (src/converter/src/attribute_reader/las_reader.cpp).
The Area index (1-6) is also stored in each point's Point Source ID.

Streams room files directly out of the zip (no full extraction) and
writes points in bounded-size chunks, so peak memory stays low even
though the full dataset is ~290M points / ~7.5GB total.
"""
import argparse
import datetime
import io
import re
import struct
import sys
import time
import zipfile
from pathlib import Path

import numpy as np

HEADER_SIZE = 227
POINT_FORMAT = 2
POINT_RECORD_LEN = 26
CHUNK_POINTS = 2_000_000

ROOM_FILE_RE = re.compile(
    r"^Stanford3dDataset_v1\.2_Aligned_Version/(Area_(\d+))/([^/]+)/\3\.txt$"
)

POINT_DTYPE = np.dtype([
    ("x", "<i4"), ("y", "<i4"), ("z", "<i4"),
    ("intensity", "<u2"),
    ("flags", "u1"), ("classification", "u1"),
    ("scan_angle", "i1"), ("user_data", "u1"),
    ("point_source_id", "<u2"),
    ("red", "<u2"), ("green", "<u2"), ("blue", "<u2"),
])
assert POINT_DTYPE.itemsize == POINT_RECORD_LEN


def find_room_files(zf):
    rooms = []
    for info in zf.infolist():
        m = ROOM_FILE_RE.match(info.filename)
        if m:
            area_name, area_idx, room_name = m.group(1), int(m.group(2)), m.group(3)
            rooms.append((area_idx, area_name, room_name, info.filename))
    rooms.sort(key=lambda r: (r[0], r[2]))
    return rooms


def write_placeholder_header(f):
    f.write(b"\x00" * HEADER_SIZE)


def pack_header(num_points, mins, maxs, scale):
    today = datetime.date.today()
    day_of_year = today.timetuple().tm_yday

    header = bytearray(HEADER_SIZE)
    header[0:4] = b"LASF"
    struct.pack_into("<H", header, 4, 0)        # File Source ID
    struct.pack_into("<H", header, 6, 0)        # Global Encoding
    # Project ID GUID (16 bytes) left zero
    header[24] = 1                              # Version Major
    header[25] = 2                              # Version Minor
    header[26:58] = b"PCLite".ljust(32, b"\x00")
    header[58:90] = b"s3dis_to_las.py".ljust(32, b"\x00")
    struct.pack_into("<H", header, 90, day_of_year)
    struct.pack_into("<H", header, 92, today.year)
    struct.pack_into("<H", header, 94, HEADER_SIZE)
    struct.pack_into("<I", header, 96, HEADER_SIZE)   # offset to point data (no VLRs)
    struct.pack_into("<I", header, 100, 0)            # number of VLRs
    header[104] = POINT_FORMAT
    struct.pack_into("<H", header, 105, POINT_RECORD_LEN)
    struct.pack_into("<I", header, 107, num_points)   # legacy point count
    struct.pack_into("<IIIII", header, 111, num_points, 0, 0, 0, 0)
    sx, sy, sz = scale
    struct.pack_into("<ddd", header, 131, sx, sy, sz)  # scale factors
    struct.pack_into("<ddd", header, 155, 0.0, 0.0, 0.0)  # offsets
    struct.pack_into("<dddddd", header, 179,
                      maxs[0], mins[0], maxs[1], mins[1], maxs[2], mins[2])
    return bytes(header)


def flush_chunk(f, xs, ys, zs, rs, gs, bs, point_source_id, scale, running):
    n = len(xs)
    if n == 0:
        return
    xs = np.asarray(xs, dtype=np.float64)
    ys = np.asarray(ys, dtype=np.float64)
    zs = np.asarray(zs, dtype=np.float64)

    ix = np.round(xs / scale[0]).astype(np.int32)
    iy = np.round(ys / scale[1]).astype(np.int32)
    iz = np.round(zs / scale[2]).astype(np.int32)

    rs = np.clip(np.asarray(rs, dtype=np.int32), 0, 255).astype(np.uint16) * 257
    gs = np.clip(np.asarray(gs, dtype=np.int32), 0, 255).astype(np.uint16) * 257
    bs = np.clip(np.asarray(bs, dtype=np.int32), 0, 255).astype(np.uint16) * 257

    pts = np.empty(n, dtype=POINT_DTYPE)
    pts["x"] = ix
    pts["y"] = iy
    pts["z"] = iz
    pts["intensity"] = 0
    pts["flags"] = 0x09  # return number=1, number of returns=1
    pts["classification"] = 0
    pts["scan_angle"] = 0
    pts["user_data"] = 0
    pts["point_source_id"] = point_source_id
    pts["red"] = rs
    pts["green"] = gs
    pts["blue"] = bs

    f.write(pts.tobytes())

    running["count"] += n
    running["min"] = (min(running["min"][0], int(ix.min())),
                       min(running["min"][1], int(iy.min())),
                       min(running["min"][2], int(iz.min())))
    running["max"] = (max(running["max"][0], int(ix.max())),
                       max(running["max"][1], int(iy.max())),
                       max(running["max"][2], int(iz.max())))


def process_room(zf, filename, area_idx, scale, out_f, running, skipped):
    xs, ys, zs, rs, gs, bs = [], [], [], [], [], []
    with zf.open(filename) as raw:
        text = io.TextIOWrapper(raw, encoding="ascii", errors="replace")
        for line in text:
            parts = line.split()
            if len(parts) < 6:
                if parts:
                    skipped[0] += 1
                continue
            try:
                x = float(parts[0])
                y = float(parts[1])
                z = float(parts[2])
                r = int(float(parts[3]))
                g = int(float(parts[4]))
                b = int(float(parts[5]))
            except ValueError:
                skipped[0] += 1
                continue
            xs.append(x); ys.append(y); zs.append(z)
            rs.append(r); gs.append(g); bs.append(b)
            if len(xs) >= CHUNK_POINTS:
                flush_chunk(out_f, xs, ys, zs, rs, gs, bs, area_idx, scale, running)
                xs, ys, zs, rs, gs, bs = [], [], [], [], [], []
    flush_chunk(out_f, xs, ys, zs, rs, gs, bs, area_idx, scale, running)


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--zip", default="test_data/Stanford3dDataset_v1.2_Aligned_Version.zip")
    ap.add_argument("--out-dir", default="test_data/s3dis_v1.2_per_area",
                     help="directory to write one LAS file per Area into")
    ap.add_argument("--scale", type=float, default=0.001, help="LAS scale factor (meters/unit)")
    ap.add_argument("--areas", default=None,
                     help="comma-separated Area indices to include, e.g. '1,2' (default: all)")
    args = ap.parse_args()

    scale = (args.scale, args.scale, args.scale)
    area_filter = None
    if args.areas:
        area_filter = {int(a) for a in args.areas.split(",")}

    zf = zipfile.ZipFile(args.zip)
    rooms = find_room_files(zf)
    if area_filter is not None:
        rooms = [r for r in rooms if r[0] in area_filter]
    if not rooms:
        print("No matching room files found.", file=sys.stderr)
        sys.exit(1)

    areas = {}
    for area_idx, area_name, room_name, filename in rooms:
        areas.setdefault((area_idx, area_name), []).append((room_name, filename))

    print(f"Found {len(rooms)} room files across {len(areas)} area(s).")

    out_dir = Path(args.out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)

    t_total = time.time()
    for area_idx, area_name in sorted(areas):
        area_rooms = sorted(areas[(area_idx, area_name)])
        out_path = out_dir / f"s3dis_v1.2_{area_name}.las"

        running = {"count": 0, "min": (2**31 - 1,) * 3, "max": (-2**31,) * 3}
        skipped = [0]

        t0 = time.time()
        with open(out_path, "wb") as out_f:
            write_placeholder_header(out_f)
            for i, (room_name, filename) in enumerate(area_rooms):
                t_room = time.time()
                before = running["count"]
                process_room(zf, filename, area_idx, scale, out_f, running, skipped)
                added = running["count"] - before
                print(f"[{area_name} {i+1}/{len(area_rooms)}] {room_name}: "
                      f"{added:,} pts in {time.time()-t_room:.1f}s "
                      f"(area total {running['count']:,})")

            header = pack_header(
                running["count"],
                tuple(m * s for m, s in zip(running["min"], scale)),
                tuple(m * s for m, s in zip(running["max"], scale)),
                scale,
            )
            out_f.seek(0)
            out_f.write(header)

        dt = time.time() - t0
        print(f"  -> wrote {running['count']:,} points ({skipped[0]} lines skipped) "
              f"to {out_path} in {dt:.1f}s\n")

    print(f"Done in {time.time()-t_total:.1f}s. Point Source ID encodes the originating Area (1-6).")


if __name__ == "__main__":
    main()
