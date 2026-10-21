#!/usr/bin/env python3
"""sounds_make.py <source folder> <out folder>

Makes the game's sound files (package/sounds/<name>.wav) from recordings
(sounds-src/, freesound.org, CC0; which one is which: SOURCES below):
cuts the piece the game needs, fades its ends, makes a loop seamless,
brings it to a common loudness, and writes it small - 16000 Hz, mono,
16 bit (the game spreads its sounds between the ears itself).

Each line of SOURCES: the game's name, the recording's freesound number,
and what is done to it:
  at      where the piece starts: seconds, or 'peak' (the loudest quarter
          second's middle, less `lead` seconds), or 'onset' (where it first
          gets loud)
  length  seconds (None: to the end)
  fade_in, fade_out  seconds
  loop    seconds of crossfade that make the piece loop seamlessly (0: none)
  rms     how loud its loudest quarter second is made (0..1), the peak kept
          under 0.95 by a soft limit
  speed   played faster (>1) or slower (<1): higher or deeper, shorter or longer
"""
import sys, os, struct
import numpy as np
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from sounds_probe import read as read_any

RATE = 16000

SOURCES = [
    # name            id      at        lead  length fade_in fade_out loop  rms   speed
    ("portal",        322059, 0.0,      0,    3.8,  0.02,  0.8,     0,    0.30, 1.0),
    ("warp",          454683, 1.0,      0,    14.0, 0.4,   1.2,     0,    0.22, 1.0),
    ("whoosh",        264777, "peak",   1.1,  1.7,  0.1,   0.35,    0,    0.32, 1.0),
    ("shot",          531861, "onset",  0,    0.45, 0.0,   0.25,    0,    0.34, 1.0),   # a heavy blaster: synth, shotgun, kick (was 414888, too thin); a plasma zap on top (ZAP)
    ("shot_2",        531861, "onset",  0,    0.45, 0.0,   0.25,    0,    0.34, 1.0),
    ("shot_3",        531861, "onset",  0,    0.45, 0.0,   0.25,    0,    0.34, 1.0),
    ("enemy_shot",    361471, "onset",  0,    0.55, 0.0,   0.25,    0,    0.25, 1.0),
    ("explode",       235968, "onset",  0,    2.3,  0.0,   0.9,     0,    0.36, 1.0),
    ("big_explode",   523365, "onset",  0,    4.5,  0.0,   1.6,     0,    0.40, 1.0),
    ("explode_2",     136765, "onset",  0,    2.3,  0.0,   0.9,     0,    0.36, 1.0),   # more takes of an explosion: one at random
    ("explode_3",     220062, "onset",  0,    2.3,  0.0,   0.9,     0,    0.36, 1.0),
    ("explode_4",     560510, "onset",  0,    2.3,  0.0,   0.9,     0,    0.36, 1.0),
    ("big_explode_2", 165808, "onset",  0,    4.5,  0.0,   1.6,     0,    0.40, 1.0),
    ("big_explode_3", 177556, "onset",  0,    4.5,  0.0,   1.6,     0,    0.40, 1.0),
    ("death",         177556, "onset",  0,    4.0,  0.0,   1.6,     0,    0.42, 0.9),   # a blast only, deeper (was a plane crash, 668549, with its dive first)
    ("enemy_shot_1910",417688, "onset", 0,    0.13, 0.0,   0.07,    0,    0.25, 1.05),  # one round of a light machine gun
    ("enemy_shot_1940",417687, "onset", 0,    0.13, 0.0,   0.07,    0,    0.25, 0.95),
    ("enemy_shot_1970",854186, "onset", 0,    0.25, 0.0,   0.12,    0,    0.27, 1.0),   # one round of an autocannon
    ("enemy_shot_1982",611449, "onset", 0,    0.16, 0.0,   0.06,    0,    0.25, 1.0),   # a short rip of a Vulcan
    ("hit",           209771, 0.0,      0,    None, 0.0,   0.1,     0,    0.32, 1.0),
    ("hit_boss",      406562, 0.0,      0,    0.4,  0.0,   0.15,    0,    0.28, 1.0),
    ("missile",       854476, "onset",  0,    1.3,  0.0,   0.5,     0,    0.30, 1.0),
    ("stall",         670297, 0.2,      0,    0.45, 0.0,   0.08,    0,    0.22, 1.0),
    ("boss",          412171, "peak",   0.6,  1.5,  0.1,   0.3,     0,    0.30, 1.0),
    ("formation",     258193, 0.0,      0,    None, 0.0,   0.03,    0,    0.22, 1.0),
    ("bonus",         253177, 0.0,      0,    None, 0.0,   0.03,    0,    0.24, 1.0),
    ("pickup",        253172, 0.0,      0,    None, 0.0,   0.03,    0,    0.24, 1.0),
    ("oneup",         220173, 0.0,      0,    0.7,  0.0,   0.15,    0,    0.24, 1.0),
    ("coin",          350876, 0.0,      0,    None, 0.0,   0.03,    0,    0.24, 1.0),
    ("start",         368691, 0.0,      0,    None, 0.0,   0.05,    0,    0.24, 1.0),
    ("tink",          146721, 0.0,      0,    None, 0.0,   0.02,    0,    0.20, 1.0),
    ("flyby_1910",    648720, "peak",   0.5,  2.0,  0.15,  0.7,     0,    0.34, 1.0),
    ("flyby_1940",    431206, "peak",   0.5,  2.0,  0.15,  0.7,     0,    0.34, 1.0),
    ("flyby_rotor",   241190, "peak",   0.5,  2.0,  0.15,  0.7,     0,    0.34, 1.0),
    ("flyby_jet",     318797, "peak",   0.5,  2.0,  0.15,  0.7,     0,    0.36, 1.0),
    ("flyby_ufo",     64429,  "peak",   0.5,  2.0,  0.15,  0.7,     0,    0.34, 1.0),
    ("flyby_missile", 623015, "peak",   0.4,  1.4,  0.1,   0.5,     0,    0.34, 1.0),
    ("flyby_boss",    333515, "peak",   0.3,  2.0,  0.05,  0.9,     0,    0.40, 0.7),
    ("engine_outside",146770, 0.5,      0,    2.0,  0,     0,       0.3,  0.20, 1.0),
    ("engine_cockpit",100408, 3.0,      0,    4.0,  0,     0,       0.6,  0.24, 1.0),
    ("wind",          697217, 5.0,      0,    4.0,  0,     0,       0.6,  0.25, 1.0),
    ("engine_1910",   648720, 5.5,      0,    2.0,  0,     0,       0.5,  0.22, 1.0),   # the enemies' engines, looped: a biplane going away, steady
    ("engine_1940",   431206, 9.0,      0,    2.0,  0,     0,       0.5,  0.22, 1.0),   # a propeller fighter going away, steady (before 8 s it swells)
    ("engine_1970",   172667, 0.0,      0,    None, 0,     0,       0,    0.22, 1.0),
    ("engine_1982",   437888, 14.0,     0,    3.0,  0,     0,       0.6,  0.22, 1.0),   # a jet running steady, its turbine whining (the splice in it is before 2.5 s)   # the helicopters' rotor, as recorded: it loops end to start (Bell 407, 348991, is in sounds-src too: too much wind)
    ("afterburner",   854476, 0.5,      0,    2.6,  0,     0,       0.5,  0.25, 0.8),   # a rocket's sustained roar, deeper, looped
    ("burner",        854476, "onset",  0,    0.9,  0.0,   0.5,     0,    0.34, 0.7),   # its ignition, deeper still: the thump as it lights
    ("engine_missile",854476, 0.4,      0,    1.2,  0,     0,       0.3,  0.2,  1.35),  # a missile's motor: the same rocket, smaller - higher and faster
    ("fall",          668549, 11.5,     0,    4.4,  0.5,   0.3,     0,    0.24, 1.15),  # a plane's dive, the howl swelling - cut before it hits (16 s)
    ("kill",          348242, "onset",  0,    0.5,  0.0,   0.25,    0,    0.36, 1.0),   # an enemy shot down: a dry punch over a kick, a metal pipe struck on it (LAYER)
    ("kill_2",        348242, "onset",  0,    0.5,  0.0,   0.25,    0,    0.36, 0.92),
    ("kill_3",        348242, "onset",  0,    0.5,  0.0,   0.25,    0,    0.36, 1.06),
]

# A second recording under a piece: its freesound number, how loud against
# the piece (each brought to the same peak first), and its own speed. Cut
# at its onset, as long as the piece.
LAYER = {
    "kill":   (244983, 0.75, 1.0),
    "kill_2": (244983, 0.75, 1.12),
    "kill_3": (244983, 0.75, 0.9),
}

# The sounds that get a plasma zap made here laid over the recording, and
# the zap's make: (start Hz, end Hz, fall in seconds, FM ratio, seed).
ZAP = {
    "shot":   (2600.0, 170.0, 0.20, 1.50, 1),
    "shot_2": (2300.0, 150.0, 0.23, 1.41, 2),
    "shot_3": (2900.0, 190.0, 0.18, 1.62, 3),
}

def lowpass(a, hz):
    k = 1.0 - np.exp(-2 * np.pi * hz / RATE)
    out = np.empty_like(a)
    y = 0.0
    for i in range(len(a)):
        y += k * (a[i] - y)
        out[i] = y
    return out

def with_zap(body, make):
    """the recording as a deep thud under a falling, buzzing, shimmering
    plasma zap with an electric crackle at its start: futuristic, but heavy"""
    f0, f1, fall, ratio, seed = make
    rng = np.random.default_rng(seed)
    n = len(body)
    t = np.arange(n) / RATE
    f = f1 + (f0 - f1) * np.exp(-t / (fall / 3))                  # falls fast, then settles low
    ph = 2 * np.pi * np.cumsum(f) / RATE
    index = 3.0 * np.exp(-t / 0.06)                               # metallic at first, purer after
    voice = np.sin(ph + index * np.sin(ratio * ph))
    voice2 = np.sin(1.013 * ph + 0.7 * index * np.sin(ratio * 1.013 * ph))   # a little apart: it shimmers
    buzz = np.tanh(2.5 * np.sin(0.5 * ph))                        # an octave under, square-ish: the energy
    env = np.minimum(1.0, t / 0.002) * np.exp(-t / 0.10)
    zap = (0.55 * voice + 0.35 * voice2 + 0.35 * buzz) * env
    crackle = np.diff(rng.standard_normal(n + 1)) * (rng.random(n) < 0.25)
    zap += 0.35 * crackle * np.exp(-t / 0.025)
    thud = lowpass(body, 700.0)                                   # the blaster's weight, not its own hiss
    thud /= max(1e-6, np.max(np.abs(thud)))
    zap /= max(1e-6, np.max(np.abs(zap)))
    return (0.7 * thud + 0.8 * zap).astype(np.float32)

def resample(a, rate, speed):
    """to RATE, played `speed` times as fast: low-pass under the new Nyquist, then interpolate"""
    step = rate * speed / RATE                       # source samples per output sample
    if step > 1.0:
        cut = 0.45 / step                            # of the source rate
        n = 127
        t = np.arange(n) - (n - 1) / 2
        h = np.sinc(2 * cut * t) * np.hamming(n)
        h /= h.sum()
        a = np.convolve(a, h, mode="same")
    out_n = int(len(a) / step)
    x = np.arange(out_n) * step
    return np.interp(x, np.arange(len(a)), a).astype(np.float32)

def loudest(a, rate):
    seg = rate // 4
    if len(a) <= seg:
        return 0.0, float(np.sqrt(np.mean(a ** 2)))
    r = np.array([np.sqrt(np.mean(a[i:i + seg] ** 2)) for i in range(0, len(a) - seg + 1, seg // 2)])
    i = int(np.argmax(r))
    return (i * (seg // 2) + seg / 2) / rate, float(r[i])

def onset(a, rate):
    thr = 0.1 * np.max(np.abs(a))
    i = int(np.argmax(np.abs(a) > thr))
    return max(0, i - int(0.005 * rate)) / rate

def make(src_dir, out_dir, row):
    name, sid, at, lead, length, fin, fout, loop, rms, speed = row
    path = [os.path.join(src_dir, f) for f in os.listdir(src_dir) if f.startswith("%d__" % sid) and f.endswith((".wav", ".aif", ".aiff"))][0]
    a, rate, ch, bits = read_any(path)
    a = np.nan_to_num(a)
    a = a - np.mean(a)                                 # no offset
    if at == "peak":
        t0 = max(0.0, loudest(a, rate)[0] - lead)
    elif at == "onset":
        t0 = onset(a, rate)
    else:
        t0 = float(at)
    i0 = int(t0 * rate)
    extra = loop if loop else 0
    i1 = len(a) if length is None else min(len(a), i0 + int((length + extra) * rate))
    piece = a[i0:i1]
    piece = resample(piece, rate, speed)
    if name in LAYER:
        lid, gain, lspeed = LAYER[name]
        lpath = [os.path.join(src_dir, f) for f in os.listdir(src_dir) if f.startswith("%d__" % lid) and f.endswith((".wav", ".aif", ".aiff"))][0]
        b, brate, _, _ = read_any(lpath)
        b = np.nan_to_num(b)
        b = b - np.mean(b)
        b = resample(b[int(onset(b, brate) * brate):], brate, lspeed)[:len(piece)]
        b = np.pad(b, (0, len(piece) - len(b)))
        piece = (piece / max(1e-6, np.max(np.abs(piece))) + gain * b / max(1e-6, np.max(np.abs(b)))).astype(np.float32)
    if name in ZAP:
        piece = with_zap(piece, ZAP[name])
    if loop:
        x = int(loop * RATE)
        body, tail = piece[:-x].copy(), piece[-x:]
        w = np.linspace(0, 1, x, dtype=np.float32)
        body[:x] = body[:x] * np.sqrt(w) + tail * np.sqrt(1 - w)   # the end runs into the start: no seam
        piece = body
    n = len(piece)
    if fin > 0:
        k = min(n, int(fin * RATE)); piece[:k] *= np.linspace(0, 1, k)
    if fout > 0:
        k = min(n, int(fout * RATE)); piece[n - k:] *= np.linspace(1, 0, k) ** 1.5
    _, r = loudest(piece, RATE)
    if r > 0:
        piece *= rms / r
    piece = np.tanh(piece / 0.95) * 0.95 if np.max(np.abs(piece)) > 0.95 else piece
    pcm = (np.clip(piece, -1, 1) * 32767).astype("<i2").tobytes()
    out = os.path.join(out_dir, name + ".wav")
    with open(out, "wb") as f:
        f.write(b"RIFF" + struct.pack("<I", 36 + len(pcm)) + b"WAVE")
        f.write(b"fmt " + struct.pack("<IHHIIHH", 16, 1, 1, RATE, RATE * 2, 2, 16))
        f.write(b"data" + struct.pack("<I", len(pcm)) + pcm)
    print("%-15s from %-7d %5.2f s  %3d KB  at %.2f s of %.1f" % (name, sid, n / RATE, (44 + len(pcm)) // 1024, t0, len(a) / rate))

if __name__ == "__main__":
    src, out = sys.argv[1], sys.argv[2]
    only = set(sys.argv[3:])                           # names after the folders: just those
    os.makedirs(out, exist_ok=True)
    for row in SOURCES:
        if not only or row[0] in only:
            make(src, out, row)
