#!/usr/bin/env python3
# dvgview.py <record> <out-prefix> [t0 t1 step]  -  pictures from a record made
# by tools/fakedvg.py: one PNG per <step> seconds between t0 and t1 (default:
# the whole record, every second). Prints per picture the time, the number of
# lines and the beam's way. Needs Pillow. See BUILD.md.
import struct, sys
from PIL import Image, ImageDraw

rec, prefix = sys.argv[1], sys.argv[2]
t0 = float(sys.argv[3]) if len(sys.argv) > 3 else 0.0
t1 = float(sys.argv[4]) if len(sys.argv) > 4 else 1e9
step = float(sys.argv[5]) if len(sys.argv) > 5 else 1.0
SIZE = 1024

frames = []            # (time, [words])
cur, pend = [], b""
with open(rec, "rb") as f:
    while True:
        head = f.read(12)
        if len(head) < 12:
            break
        t, n = struct.unpack("<dI", head)
        pend += f.read(n)
        k = len(pend) // 4 * 4
        for i in range(0, k, 4):
            w = int.from_bytes(pend[i:i + 4], "big")
            cur.append(w)
            if w >> 29 == 0:
                frames.append((t, cur))
                cur = []
        pend = pend[k:]

print("frames", len(frames), "from", frames[0][0] if frames else 0, "to", frames[-1][0] if frames else 0)
want = t0
n = 0
for t, words in frames:
    if t < want or t > t1:
        continue
    want = t + step
    img = Image.new("RGB", (SIZE, SIZE), (0, 0, 0))
    d = ImageDraw.Draw(img)
    x = y = None
    col = (255, 255, 255)
    lines = 0
    way = 0.0
    for w in words:
        fl = w >> 29
        if fl == 1:
            col = ((w >> 16) & 255, (w >> 8) & 255, w & 255)
        elif fl == 2:
            nx, ny = (w >> 14) & 0x3fff, w & 0x3fff
            if x is not None:
                way += ((nx - x) ** 2 + (ny - y) ** 2) ** 0.5
                if not (w >> 28) & 1:
                    lines += 1
                    s = SIZE / 4096.0
                    c = tuple(min(255, 60 + v) if v else 0 for v in col)
                    d.line((x * s, SIZE - 1 - y * s, nx * s, SIZE - 1 - ny * s), fill=c, width=1)
            x, y = nx, ny
    n += 1
    name = "%s.%03d.png" % (prefix, n)
    img.save(name)
    print("%s t=%.2f lines=%d way=%.0f" % (name, t, lines, way))
