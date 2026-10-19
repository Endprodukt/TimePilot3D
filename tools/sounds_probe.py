#!/usr/bin/env python3
"""sounds_probe.py <folder> - for every WAV/AIFF in the folder: its format,
its length and where it is loud (RMS per 0.25 s, peak position), to pick
the piece of each recording the game uses (tools/sounds_make.py)."""
import sys, os, struct, wave
import numpy as np

def read(path):
    """samples as float32 in -1..1, mono (channels averaged), and the rate"""
    if path.lower().endswith((".aif", ".aiff")):
        import aifc
        f = aifc.open(path, "rb")
        ch, bw, rate, n = f.getnchannels(), f.getsampwidth(), f.getframerate(), f.getnframes()
        raw = f.readframes(n)
        f.close()
        if bw == 3:                                  # 24 bit, big-endian
            b = np.frombuffer(raw[: len(raw) // 3 * 3], dtype=np.uint8).reshape(-1, 3).astype(np.int32)
            a = ((b[:, 0] << 24) | (b[:, 1] << 16) | (b[:, 2] << 8)).astype(np.float64) / 2147483648.0
            a = a.astype(np.float32)
        else:
            dt = {1: ">i1", 2: ">i2", 4: ">i4"}[bw]
            a = np.frombuffer(raw, dtype=dt).astype(np.float32) / float(2 ** (8 * bw - 1))
        return a.reshape(-1, ch).mean(axis=1), rate, ch, bw * 8
    data = open(path, "rb").read()
    if data[:4] != b"RIFF" or data[8:12] != b"WAVE":
        raise ValueError("not a WAV")
    i, fmt, body = 12, None, None
    while i + 8 <= len(data):
        cid, size = data[i:i + 4], struct.unpack("<I", data[i + 4:i + 8])[0]
        chunk = data[i + 8:i + 8 + size]
        if cid == b"fmt ":
            fmt = chunk
        elif cid == b"data":
            body = chunk
        i += 8 + size + (size & 1)
    tag, ch, rate = struct.unpack("<HHI", fmt[:8])
    bits = struct.unpack("<H", fmt[14:16])[0]
    if tag == 0xFFFE:
        tag = struct.unpack("<H", fmt[24:26])[0]
    if tag == 3:
        a = np.frombuffer(body[: len(body) // 4 * 4], dtype="<f4").astype(np.float32)
    elif bits == 8:
        a = (np.frombuffer(body, dtype=np.uint8).astype(np.float32) - 128.0) / 128.0
    elif bits == 16:
        a = np.frombuffer(body[: len(body) // 2 * 2], dtype="<i2").astype(np.float32) / 32768.0
    elif bits == 24:
        b = np.frombuffer(body[: len(body) // 3 * 3], dtype=np.uint8).reshape(-1, 3).astype(np.int32)
        v = (b[:, 0] << 8) | (b[:, 1] << 16) | (b[:, 2] << 24)
        a = v.astype(np.float32) / 2147483648.0
    elif bits == 32:
        a = np.frombuffer(body[: len(body) // 4 * 4], dtype="<i4").astype(np.float32) / 2147483648.0
    else:
        raise ValueError("%d bit" % bits)
    a = a[: len(a) // ch * ch].reshape(-1, ch).mean(axis=1)
    return a, rate, ch, bits

if __name__ == "__main__":
    folder = sys.argv[1]
    for name in sorted(os.listdir(folder)):
        p = os.path.join(folder, name)
        try:
            a, rate, ch, bits = read(p)
        except Exception as e:
            print("%-14s ERROR %s" % (name[:14], e)); continue
        seg = rate // 4
        rms = [float(np.sqrt(np.mean(a[i:i + seg] ** 2))) for i in range(0, len(a) - seg + 1, seg)]
        pk = int(np.argmax(np.abs(a)))
        print("%-10s %6d Hz %d ch %2d bit %6.2f s peak %.2f at %.2f s" % (name.split("__")[0], rate, ch, bits, len(a) / rate, float(np.max(np.abs(a))), pk / rate))
        print("    rms/0.25s:", " ".join("%.2f" % r for r in rms[:60]))
