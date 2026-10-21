#!/usr/bin/env python3
"""music_make.py <out folder> [piece ...]

Makes the game's music (package/music/<piece>.wav), all of it here in code:
no recordings, so nothing to license. Dark science fiction, not dance pop:
everything in D minor over a pedal on D, with the Phrygian E flat for
tension, at one tempo, on one theme - a slow, ominous line on a gliding
brass synth (CS-80 style). The same sounds throughout: dark detuned pads,
a glassy FM shimmer over them, a resonant 16th sequence and a rolling bass
under them (Tron-like), deep "braam" hits at the turns, radar pings and data
blips, cinematic half-time drums with toms, and in the tunnel dark acid
techno with metallic percussion.

  attract    the attract mode, a 32-bar loop: in, the theme on the beat, an acid
             drop, a break, the last drop
  hiscore    entering the initials, a 16-bar loop: the theme slowly, on glass
  start      START pressed: a braam and a rising arpeggio, a jingle of two seconds
  levelin    the way into an era (LEVEL_IN_TIME): a riser to the moment the
             plane bursts out of the tunnel (LEVEL_IN_OPEN), the theme after
  approach   the way out of an era (TUNNEL_APPROACH 7.4 s): four bars of
             build-up into E flat, a roll and a riser, ending as the plane goes in
  tunnel     the time tunnel (TUNNEL_TIME): eight bars of dark acid techno
  boss       a boss comes (BOSSINTRO): an alarm siren, braams, pounding toms
  victory    a boss shot down: a fanfare out of the minor into D major
  lounge     how to play, after START: relaxed - half time, swing, jazz chords on
             an electric piano, a vibraphone (the one piece in D major)

The tempo is set by the run into the tunnel: 16 beats in its 7.4 s.
Written 22050 Hz, mono, 16 bit (the cabinet's speaker is one; music is
streamed, vp_music_play). Needs numpy (WSL: python3 tools/music_make.py
package/music).
"""
import sys, os, struct
import numpy as np

SR = 22050
BPM = 16 * 60 / 7.4                 # 129.7: the run into the tunnel is four bars
BEAT = 60.0 / BPM
STEP = BEAT / 4                     # a sixteenth
BAR = BEAT * 4
rng = np.random.default_rng(2001)

def nlen(sec): return max(1, int(round(sec * SR)))
def T(bar, step=0.0): return bar * BAR + step * STEP

def hz(note):
    """'D4', 'Bb3', 'F#5' -> Hz"""
    n = {'C': 0, 'D': 2, 'E': 4, 'F': 5, 'G': 7, 'A': 9, 'B': 11}[note[0]]
    i = 1
    if note[1] == '#': n += 1; i = 2
    elif note[1] == 'b': n -= 1; i = 2
    midi = 12 * (int(note[i:]) + 1) + n
    return 440.0 * 2 ** ((midi - 69) / 12)

# --- the tune -----------------------------------------------------------------

# a bar each, round and round, over D: Dm9, Bbmaj7/D, Gm9/D, then A (sus) to lead home
CHORDS = [['D3', 'A3', 'E4', 'F4'],
          ['D3', 'F3', 'Bb3', 'A4'],
          ['D3', 'G3', 'Bb3', 'A4'],
          ['A2', 'E3', 'A3', 'D4']]
ROOT = ['D2', 'D2', 'D2', 'A1']
TENSION = ['D3', 'G3', 'Bb3', 'Eb4']           # Eb/D, the Phrygian step: unease, before the tunnel
# the theme: four bars, (sixteenth, note, sixteenths long) - slow, wide, ominous
THEME = [(0, 'D4', 4), (4, 'A4', 4), (8, 'Bb4', 6), (14, 'A4', 2),
         (16, 'G4', 4), (20, 'F4', 2), (22, 'E4', 2), (24, 'F4', 8),
         (32, 'D4', 4), (36, 'A4', 4), (40, 'C5', 6), (46, 'Bb4', 2),
         (48, 'A4', 16)]
# the sequence over the root: semitones for each sixteenth
SEQ = [0, 12, 0, 7, 0, 12, 0, 10, 0, 12, 0, 7, 0, 15, 12, 10]
SEQ_TENSE = [0, 12, 1, 7, 0, 13, 0, 10, 0, 12, 1, 7, 0, 15, 13, 10]
# the tunnel's acid line over D, Phrygian: (semitones over D2, accent, slide into the next)
ACID = [(0, 1, 0), (0, 0, 0), (12, 0, 1), (0, 1, 0), (1, 0, 0), (0, 0, 0), (10, 1, 0), (0, 0, 1),
        (0, 0, 0), (7, 1, 0), (0, 0, 0), (13, 0, 1), (12, 1, 0), (3, 0, 1), (1, 0, 0), (10, 1, 0)]
BLIPS = ['D6', 'F6', 'G6', 'A6', 'C7', 'D7']   # the data's chatter, D minor pentatonic

# --- signal ---------------------------------------------------------------------

def fft_filter(x, lo=None, hi=None, order=2):
    """zero-phase high-pass at `lo`, low-pass at `hi` (Butterworth-like magnitude)"""
    X = np.fft.rfft(x)
    f = np.fft.rfftfreq(len(x), 1.0 / SR)
    g = np.ones_like(f)
    if hi: g /= np.sqrt(1 + (f / hi) ** (2 * order))
    if lo: g /= np.sqrt(1 + (lo / np.maximum(f, 1e-3)) ** (2 * order))
    return np.fft.irfft(X * g, len(x))

def svf(x, fc, q, mode='lp'):
    """a state-variable filter (TPT) whose cutoff may move every sample"""
    n = len(x)
    fc = np.clip(np.broadcast_to(np.asarray(fc, float), (n,)), 20.0, SR * 0.45)
    g = np.tan(np.pi * fc / SR)
    k = 1.0 / q
    a1 = 1.0 / (1.0 + g * (g + k)); a2 = g * a1; a3 = g * a2
    a1, a2, a3, xs = a1.tolist(), a2.tolist(), a3.tolist(), x.tolist()
    ic1 = ic2 = 0.0
    out = [0.0] * n
    bp = mode == 'bp'
    for i in range(n):
        v3 = xs[i] - ic2
        v1 = a1[i] * ic1 + a2[i] * v3
        v2 = ic2 + a2[i] * ic1 + a3[i] * v3
        ic1 = 2.0 * v1 - ic1
        ic2 = 2.0 * v2 - ic2
        out[i] = v1 if bp else v2
    return np.array(out)

def polyblep(ph, dt):
    y = np.zeros_like(ph)
    m = ph < dt
    x = ph[m] / dt[m]; y[m] = x + x - x * x - 1.0
    m = ph > 1.0 - dt
    x = (ph[m] - 1.0) / dt[m]; y[m] = x * x + x + x + 1.0
    return y

def saw(f, n, phase=0.0):
    dt = np.broadcast_to(np.asarray(f, float), (n,)) / SR
    ph = (phase + np.cumsum(dt)) % 1.0
    return 2.0 * ph - 1.0 - polyblep(ph, dt)

def pulse(f, n, width=0.5, phase=0.0):
    return 0.5 * (saw(f, n, phase) - saw(f, n, (phase + width) % 1.0))

def sine(f, n, phase=0.0):
    dt = np.broadcast_to(np.asarray(f, float), (n,)) / SR
    return np.sin(2 * np.pi * (phase + np.cumsum(dt)))

def tsec(n): return np.arange(n) / SR

def fade_tail(x, sec=0.02):
    k = min(len(x), nlen(sec)); x[-k:] *= np.linspace(1, 0, k); return x

def norm(x, peak):
    return x / (np.max(np.abs(x)) + 1e-9) * peak

# --- instruments ------------------------------------------------------------

_cache = {}
def cached(key, make):
    if key not in _cache: _cache[key] = make()
    return _cache[key]

def kick(length=0.5):
    def make():
        n = nlen(length); t = tsec(n)
        f = 42 + 110 * np.exp(-t / 0.03) + 20 * np.exp(-t / 0.12)
        body = sine(f, n) * np.exp(-t / 0.2)
        click = fft_filter(rng.standard_normal(n), lo=3000) * np.exp(-t / 0.003) * 0.2
        return fade_tail(np.tanh((body * 1.5 + click) * 1.3) * 0.9)
    return cached(('kick', length), make)

def tom():
    """a cinematic tom: deep, with its skin's rattle"""
    def make():
        n = nlen(0.8); t = tsec(n)
        body = sine(60 + 55 * np.exp(-t / 0.05), n) * np.exp(-t / 0.3)
        skin = fft_filter(rng.standard_normal(n), lo=400, hi=3000) * np.exp(-t / 0.06) * 0.35
        return fade_tail(np.tanh((body + skin) * 1.6) * 0.8)
    return cached('tom', make)

def metal():
    """a metallic hit (FM, inharmonic): the clap's place, in the future"""
    def make():
        n = nlen(0.35); t = tsec(n)
        x = np.sin(2 * np.pi * 470 * t + 3.2 * np.exp(-t / 0.05) * np.sin(2 * np.pi * 470 * 1.414 * t)) * np.exp(-t / 0.09)
        x += fft_filter(rng.standard_normal(n), lo=4000) * np.exp(-t / 0.02) * 0.4
        return fade_tail(norm(x, 0.6))
    return cached('metal', make)

def snare():
    def make():
        n = nlen(0.25); t = tsec(n)
        tone = sine(190 * (1 + 0.4 * np.exp(-t / 0.01)), n) * np.exp(-t / 0.05) * 0.5
        nz = fft_filter(rng.standard_normal(n), lo=1500, hi=8000) * np.exp(-t / 0.09)
        return fade_tail(tone + norm(nz, 0.6))
    return cached('snare', make)

def hat(open_=False):
    def make():
        n = nlen(0.35 if open_ else 0.06); t = tsec(n)
        x = fft_filter(rng.standard_normal(n), lo=7000) * np.exp(-t / (0.12 if open_ else 0.02))
        return fade_tail(norm(x, 0.45))
    return cached(('hat', open_), make)

def dark_pad(chord, length, cut=1300, attack=0.9, release=1.6):
    """slow, dark detuned saws: a choir of machines"""
    def make():
        n = nlen(length + release); t = tsec(n)
        x = np.zeros(n)
        for nt in chord:
            f = hz(nt)
            for d in (-0.13, 0.0, 0.11):
                x += saw(f * 2 ** (d / 12), n, rng.random())
        x = fft_filter(x, lo=120, hi=cut)
        e = np.minimum(1.0, t / attack) ** 1.5
        rel = t > length
        e[rel] *= np.exp(-(t[rel] - length) / release * 3.0)
        return fade_tail(x * e / (3 * len(chord)))
    return cached(('dark', tuple(chord), round(length, 3), int(cut)), make)

def glass(chord, length, attack=0.6, release=1.8):
    """a glassy FM shimmer two octaves up, slowly beating"""
    def make():
        n = nlen(length + release); t = tsec(n)
        x = np.zeros(n)
        for k, nt in enumerate(chord):
            f = hz(nt) * 4
            x += np.sin(2 * np.pi * f * t + 1.1 * np.sin(2 * np.pi * f * 2.003 * t + k)) * (0.6 + 0.4 * np.sin(2 * np.pi * (0.3 + 0.07 * k) * t))
        e = np.minimum(1.0, t / attack)
        rel = t > length
        e[rel] *= np.exp(-(t[rel] - length) / release * 3.0)
        return fade_tail(fft_filter(x, hi=7000) * e / len(chord))
    return cached(('glass', tuple(chord), round(length, 3)), make)

def braam(root='D1'):
    """the deep brass blast of science fiction: low saws, the filter tearing open, driven hard"""
    def make():
        n = nlen(3.2); t = tsec(n)
        f = hz(root)
        x = sum(saw(f * m * 2 ** (d / 12), n, rng.random()) for m in (1, 2, 3, 4) for d in (-0.1, 0.1))
        x = svf(x, 150 + 1600 * np.exp(-((t - 0.25) / 0.35) ** 2) + 500 * np.exp(-t / 1.2), 1.6)
        x = np.tanh(x * 1.6) * np.minimum(1, t / 0.03) * np.exp(-t / 1.4)
        x += sine(f * 2, n) * np.exp(-t / 1.1) * 0.6
        return fade_tail(norm(x, 0.8))
    return cached(('braam', root), make)

def seq_note(note, bright=1.0, length=None):
    """the sequence: a pulse and a saw through a resonant filter that each note opens"""
    length = length or STEP * 0.85
    b = round(bright * 8) / 8
    def make():
        n = nlen(length + 0.15); t = tsec(n)
        f = hz(note)
        x = pulse(f, n, 0.3) + 0.6 * saw(f * 1.003, n)
        x = svf(x, 180 + (300 + 3600 * b) * np.exp(-t / 0.05), 3.0)
        e = np.ones(n); off = t > length; e[off] = np.exp(-(t[off] - length) / 0.03)
        return fade_tail(np.tanh(x * e * 1.4) * 0.45)
    return cached(('seq', note, b, round(length, 3)), make)

def bass(note, length):
    def make():
        n = nlen(length + 0.04); t = tsec(n)
        f = hz(note)
        x = svf(saw(f, n) + saw(f * 1.005, n, 0.4), 110 + 500 * np.exp(-t / 0.05), 1.2) + sine(f, n) * 0.5
        e = np.ones(n); off = t > length; e[off] = np.exp(-(t[off] - length) / 0.02)
        return fade_tail(np.tanh(x * e * 1.5) * 0.55)
    return cached(('bass', note, round(length, 3)), make)

def ping(note, length=0.5):
    """a radar's ping: a pure tone, a quick bend down at its start"""
    def make():
        n = nlen(length); t = tsec(n)
        f = hz(note) * (1 + 0.03 * np.exp(-t / 0.01))
        return fade_tail(sine(f, n) * np.exp(-t / (length * 0.35)) * np.minimum(1, t / 0.002) * 0.5)
    return cached(('ping', note, length), make)

def brass_line(events, length, cut=2400, octave=0):
    """one voice that glides from note to note, its filter swelling open as each begins,
    a slow vibrato on the long ones: the theme's voice. events: (seconds, note, seconds long)"""
    n = nlen(length + 0.8); t = tsec(n)
    f = np.zeros(n); amp = np.zeros(n); cutoff = np.full(n, 250.0); vib = np.zeros(n)
    prev = hz(events[0][1]) * 2 ** octave
    for k, (ts, nt, ln) in enumerate(events):
        fr = hz(nt) * 2 ** octave
        i = nlen(ts); nxt = nlen(events[k + 1][0]) if k + 1 < len(events) else n
        j = min(n, nxt)
        tt = tsec(j - i)
        legato = k > 0 and abs(events[k - 1][0] + events[k - 1][2] - ts) < 0.02
        f[i:j] = fr + (prev - fr) * np.exp(-tt / 0.07) if legato else fr
        on = min(len(tt), nlen(ln))
        a = np.ones(j - i)
        if not legato: a[:on] = np.minimum(1.0, tt[:on] / 0.06)
        a[on:] = np.exp(-(tt[on:] - ln) / 0.25)
        amp[i:j] = a
        cutoff[i:j] = 300 + (cut - 300) * (1 - np.exp(-tt / 0.18)) * (0.75 + 0.25 * np.exp(-tt / 0.6))
        vib[i:j] = np.clip((tt - 0.35) / 0.4, 0, 1)
        prev = fr
    fv = f * (1 + 0.006 * np.sin(2 * np.pi * 5.2 * t) * vib)
    x = saw(fv, n) + saw(fv * 1.006, n, 0.3) + 0.5 * saw(fv * 0.5, n, 0.7)
    x = svf(x, cutoff, 1.5)
    return np.tanh(x * amp * 1.2) * 0.5

def riser(length, top='D6'):
    n = nlen(length); t = tsec(n); u = t / length
    nz = svf(rng.standard_normal(n), 300 * (8000 / 300) ** u, 2.5, 'bp')
    tone = sum(saw(hz('D3') * (hz(top) / hz('D3')) ** (u ** 1.6) * 2 ** (d / 12), n, rng.random()) for d in (-0.2, 0.2)) * 0.25
    x = (nz * 0.8 + fft_filter(tone, hi=5000)) * u ** 2.2
    return norm(x, 0.5)

def downlifter(length):
    n = nlen(length); t = tsec(n); u = t / length
    x = svf(rng.standard_normal(n), 6000 * (250 / 6000) ** u, 1.5, 'bp') * (1 - u) ** 1.5
    return fade_tail(norm(x, 0.35))

def subdrop():
    def make():
        n = nlen(2.0); t = tsec(n)
        return fade_tail(sine(30 + 60 * np.exp(-t / 0.25), n) * np.exp(-t / 0.8) * 0.8)
    return cached('subdrop', make)

# --- space ------------------------------------------------------------------

def make_ir(seconds=3.4, tau=0.75):
    n = nlen(seconds); t = tsec(n)
    nz = rng.standard_normal(n)
    lo = fft_filter(nz, hi=1800)
    ir = lo * np.exp(-t / tau) + (nz - lo) * np.exp(-t / (tau * 0.35))
    ir[:nlen(0.03)] = 0
    return ir / np.sqrt(np.sum(ir ** 2))
IR = make_ir()

def convolve(x, ir):
    m = len(x) + len(ir) - 1
    N = 1 << (m - 1).bit_length()
    return np.fft.irfft(np.fft.rfft(x, N) * np.fft.rfft(ir, N), N)[:len(x)]

def delay(x, time, fb=0.45, taps=7):
    y = np.zeros_like(x)
    for k in range(1, taps + 1):
        d = nlen(k * time)
        if d >= len(x): break
        y[d:] += x[:-d] * fb ** (k - 1)
    return fft_filter(y, lo=300, hi=3000)

def reverse_swell(chord, length):
    """the chord's reverb, backwards: it swells into the moment `length` on"""
    wet = convolve(np.concatenate([dark_pad(chord, 0.5, 2500, attack=0.01), np.zeros(nlen(length))]), IR)[:nlen(length)]
    x = wet[::-1] * np.linspace(0, 1, nlen(length)) ** 1.5
    return norm(x, 0.5)

# --- a piece ------------------------------------------------------------------

class Song:
    def __init__(s, seconds):
        s.n = nlen(seconds)
        s.drums, s.music, s.fx = np.zeros(s.n), np.zeros(s.n), np.zeros(s.n)
        s.verb, s.dly = np.zeros(s.n), np.zeros(s.n)
        s.kicks = []

    def add(s, sig, t, bus='music', verb=0.0, dly=0.0, gain=1.0):
        i = int(round(t * SR))
        if i >= s.n or i + len(sig) <= 0: return
        if i < 0: sig, i = sig[-i:], 0
        j = min(s.n, i + len(sig))
        x = sig[:j - i] * gain
        getattr(s, bus)[i:j] += x
        if verb: s.verb[i:j] += x * verb
        if dly: s.dly[i:j] += x * dly

    def kick(s, t, gain=1.0, muffled=False):
        k = kick()
        if muffled: k = cached('kick_muffled', lambda: fft_filter(kick(), hi=240))
        s.add(k, t, 'drums', gain=gain)
        if not muffled: s.kicks.append(t)

    def mix(s, pump=0.35, rumble=0.0):
        # the kick pushes the rest down a little as it hits (no dance-floor pumping)
        side = np.ones(s.n)
        L = nlen(0.3); tt = tsec(L)
        duck = pump * np.where(tt < 0.004, tt / 0.004, np.exp(-(tt - 0.004) / 0.08))
        for tk in s.kicks:
            i = int(round(tk * SR)); j = min(s.n, i + L)
            if i < s.n: side[i:j] = np.minimum(side[i:j], 1.0 - duck[:j - i])
        wet = convolve(s.verb, IR)
        echo = delay(s.dly, 0.75 * BEAT)
        music = (s.music + wet * 0.9 + echo * 0.5) * side
        if rumble:
            music += fft_filter(convolve(s.drums, IR), hi=160) * rumble * side
        out = fft_filter(s.drums + music + s.fx, lo=28)
        return out - 0.35 * fft_filter(out, hi=90)      # a little less of the deepest: a cabinet speaker booms there

# --- the parts, each over bars b0 .. b1-1 --------------------------------------

def chord_of(b, tense=False): return TENSION if tense else CHORDS[b % 4]
def root_of(b): return ROOT[b % 4]

def pads(s, b0, b1, cut=1300, gain=0.9, t0=0.0, shimmer=0.25, tense=()):
    for b in range(b0, b1):
        ch = chord_of(b, b in tense)
        s.add(dark_pad(ch, BAR, cut), t0 + T(b), verb=0.45, gain=gain)
        if shimmer: s.add(glass(ch, BAR), t0 + T(b), verb=0.6, dly=0.2, gain=shimmer)

def sequence(s, b0, b1, bright0, bright1=None, gain=0.5, t0=0.0, tense=(), every=1):
    bright1 = bright0 if bright1 is None else bright1
    for b in range(b0, b1):
        pat = SEQ_TENSE if b in tense else SEQ
        r = hz(root_of(b)) * 2
        for st in range(0, 16, every):
            u = ((b - b0) * 16 + st) / max(1, (b1 - b0) * 16 - 1)
            semi = pat[st]
            note = note_from(r * 2 ** (semi / 12))
            s.add(seq_note(note, bright0 + (bright1 - bright0) * u), t0 + T(b, st), verb=0.15, dly=0.3,
                  gain=gain * (1.0 if st % 4 == 0 else 0.7))

def note_from(f):
    names = ['C', 'Db', 'D', 'Eb', 'E', 'F', 'Gb', 'G', 'Ab', 'A', 'Bb', 'B']
    midi = int(round(69 + 12 * np.log2(f / 440.0)))
    return '%s%d' % (names[midi % 12], midi // 12 - 1)

def rolling(s, b0, b1, gain=0.6, t0=0.0, tense=()):
    """the bass between the kicks: the three sixteenths after each beat"""
    for b in range(b0, b1):
        nt = root_of(b)
        for beat_ in range(4):
            for st in (1, 2, 3):
                n2 = 'Eb2' if (b in tense and beat_ == 3 and st == 3) else nt
                s.add(bass(n2, STEP * 0.8), t0 + T(b, beat_ * 4 + st), gain=gain * (1.0 if st == 2 else 0.8))

def drone(s, b0, b1, gain=0.5, t0=0.0):
    for b in range(b0, b1):
        s.add(bass(root_of(b), BAR * 0.98), t0 + T(b), gain=gain)

def theme(s, b0, gain=0.7, t0=0.0, octave=0, steps=64, stretch=1, voice='brass'):
    ev = [(T(0, st * stretch), nt, ln * STEP * stretch * 0.97) for st, nt, ln in THEME if st < steps]
    if voice == 'brass':
        x = brass_line(ev, ev[-1][0] + ev[-1][2], octave=octave)
        s.add(x, t0 + T(b0), verb=0.4, dly=0.2, gain=gain)
    else:
        for ts, nt, ln in ev:
            f = note_from(hz(nt) * 2 ** octave)
            s.add(glass([f], ln, attack=0.01, release=1.2) * 0.45, t0 + T(b0) + ts, verb=0.5, dly=0.35, gain=gain)

def pings(s, b0, b1, t0=0.0, gain=0.3, every=2):
    for b in range(b0, b1, every):
        s.add(ping('D6', 0.8), t0 + T(b), verb=0.5, dly=0.6, gain=gain)

def blips(s, b0, b1, density=0.18, t0=0.0, gain=0.18):
    r = np.random.default_rng(7 + b0)
    for b in range(b0, b1):
        for st in range(16):
            if r.random() < density:
                s.add(ping(BLIPS[r.integers(len(BLIPS))], 0.08), t0 + T(b, st), verb=0.2, dly=0.4, gain=gain)

def cinematic(s, b0, b1, t0=0.0, gain=1.0):
    """half time: the kick on one and on the 'and' of three, a tom on three, ticks between"""
    for b in range(b0, b1):
        for st in range(16):
            t = t0 + T(b, st)
            if st in (0, 10): s.kick(t, 0.7 * gain)
            if st == 8: s.add(tom(), t, 'drums', verb=0.5, gain=0.7 * gain)
            if st % 2 == 1: s.add(hat(), t, 'drums', gain=0.18 * gain)
            if st == 14 and b % 2: s.add(metal(), t, 'drums', verb=0.3, gain=0.3 * gain)

def techno(s, b0, b1, t0=0.0, open_hats=True, metal_=True, gain=1.0):
    for b in range(b0, b1):
        for st in range(16):
            t = t0 + T(b, st)
            if st % 4 == 0: s.kick(t, 0.7 * gain)
            if metal_ and st in (4, 12): s.add(metal(), t, 'drums', verb=0.25, gain=0.4 * gain)
            if st % 4 != 0: s.add(hat(), t, 'drums', gain=(0.3 if st % 4 == 2 else 0.16) * gain)
            if open_hats and st % 4 == 2: s.add(hat(True), t, 'drums', gain=0.16 * gain)

def acid(s, b0, b1, cut0, cut1, gain=0.5, t0=0.0):
    """a 303-like line: a saw through a resonant filter that each note opens, accents more, slides glide"""
    n0, n1 = int(round((t0 + T(b0)) * SR)), int(round((t0 + T(b1)) * SR))
    n = n1 - n0; t = tsec(n)
    steps = (b1 - b0) * 16
    f = np.zeros(n); amp = np.zeros(n); env = np.zeros(n)
    prev = hz('D2')
    for k in range(steps):
        semi, acc, slide = ACID[k % 16]
        i, j = int(round(k * STEP * SR)), min(n, int(round((k + 1) * STEP * SR)))
        target = hz('D2') * 2 ** (semi / 12)
        ts = tsec(j - i)
        glide = ACID[(k - 1) % 16][2] if k else 0
        f[i:j] = target + (prev - target) * np.exp(-ts / 0.03) if glide else target
        on = (j - i) if slide else int((j - i) * 0.65)
        a = np.zeros(j - i); a[:on] = 1.0
        amp[i:j] = a * (1.25 if acc else 0.9)
        env[i:j] = np.exp(-ts / (0.12 if acc else 0.07)) * (2.6 if acc else 1.3)
        prev = target
    amp = fft_filter(amp, hi=300)                  # no clicks at the gates
    base = cut0 * (cut1 / cut0) ** (t / t[-1])
    x = svf(saw(f, n), base * (1 + env), 7.0)
    x = np.tanh(x * amp * 2.2) * gain
    s.add(x, t0 + T(b0), verb=0.12, dly=0.15)

def fill(s, b, t0=0.0, gain=0.6):
    """toms down over the bar's last beat, into the next"""
    for k, st in enumerate((12, 13, 14, 15)):
        s.add(tom(), t0 + T(b, st), 'drums', verb=0.3, gain=gain * (0.6 + 0.15 * k))

def siren(length):
    """an alarm: two tones a hair apart winding up and down, a bar each way round"""
    n = nlen(length); t = tsec(n)
    f = 620 + 480 * (0.5 - 0.5 * np.cos(2 * np.pi * t / BAR))
    x = pulse(f, n, 0.5) + 0.6 * saw(f * 1.012, n, 0.3)
    x = fft_filter(x, lo=300, hi=3200) * np.minimum(1, t / 0.3)
    return fade_tail(norm(x, 0.4), 0.3)

def roll(s, t_from, t_to, gain=0.5):
    """a roll that gets faster and louder: eighths, sixteenths, thirty-seconds - toms, then the snare"""
    t = t_from
    span = t_to - t_from
    while t < t_to - 0.01:
        u = (t - t_from) / span
        s.add(tom() if u < 0.5 else snare(), t, 'drums', verb=0.25, gain=gain * (0.35 + 0.65 * u))
        t += (2 * STEP if u < 0.4 else STEP if u < 0.75 else STEP / 2)

# --- the pieces ---------------------------------------------------------------

def attract():
    bars = 32; tail = 4.0
    s = Song(T(bars) + tail)
    # 1-4: it wakes - pads, the sequence opening, pings; the kick comes in muffled, a riser
    pads(s, 0, 4, cut=900); drone(s, 0, 4, 0.45)
    sequence(s, 0, 4, 0.1, 0.6, gain=0.42)
    pings(s, 0, 4); blips(s, 1, 4, 0.12)
    for b in range(1, 4):
        for q in range(4): s.kick(T(b, q * 4), 0.55, muffled=True)
    s.add(riser(T(1)), T(3), 'fx', verb=0.3, gain=0.7)
    # 5-12: the theme on the brass over the beat
    s.add(braam(), T(4), 'fx', verb=0.5, gain=0.8)
    s.add(subdrop(), T(4), 'fx', gain=0.6)
    pads(s, 4, 12, cut=1400); rolling(s, 4, 12, 0.5)
    sequence(s, 4, 12, 0.7, gain=0.38)
    techno(s, 4, 12, open_hats=False)
    theme(s, 4); theme(s, 8)
    fill(s, 7); fill(s, 11)
    # 13-20: the drop - acid under it, the theme an octave up, the data chattering
    s.add(braam(), T(12), 'fx', verb=0.5, gain=0.9)
    s.add(subdrop(), T(12), 'fx', gain=0.7)
    pads(s, 12, 20, cut=1800, gain=0.7)
    acid(s, 12, 20, 300, 2600, gain=0.36)
    sequence(s, 12, 20, 0.9, gain=0.3, every=2)
    techno(s, 12, 20)
    theme(s, 12, gain=0.6, octave=1); theme(s, 16, gain=0.6)
    blips(s, 12, 20, 0.15)
    fill(s, 15); fill(s, 19)
    # 21-24: a break - glass, the theme on glass, a roll and a riser into the last drop
    pads(s, 20, 24, cut=900, gain=0.8, shimmer=0.35); drone(s, 20, 24, 0.4)
    theme(s, 20, voice='glass', gain=0.5, steps=64)
    sequence(s, 20, 24, 0.2, 0.9, gain=0.35)
    pings(s, 20, 24)
    roll(s, T(22), T(24), 0.45)
    s.add(riser(T(2)), T(22), 'fx', verb=0.3, gain=0.85)
    # 25-32: everything
    s.add(braam(), T(24), 'fx', verb=0.5, gain=1.0)
    s.add(subdrop(), T(24), 'fx', gain=0.7)
    pads(s, 24, 32, cut=2000, gain=0.7); rolling(s, 24, 32, 0.5)
    acid(s, 24, 32, 500, 3000, gain=0.3)
    sequence(s, 24, 32, 1.0, gain=0.35)
    techno(s, 24, 32)
    theme(s, 24, gain=0.6); theme(s, 28, gain=0.55); theme(s, 28, gain=0.3, octave=1)
    blips(s, 24, 32, 0.12)
    fill(s, 27)
    s.add(downlifter(T(0, 8)), T(31, 8), 'fx', verb=0.4, gain=0.5)
    return s.mix(pump=0.4), T(bars)

def hiscore():
    bars = 16; tail = 4.0
    s = Song(T(bars) + tail)
    pads(s, 0, 16, cut=1000, shimmer=0.35); drone(s, 0, 16, 0.35)
    theme(s, 0, voice='glass', stretch=2, gain=0.55, steps=32)
    theme(s, 8, voice='glass', stretch=2, gain=0.55, steps=32, octave=-1)
    sequence(s, 4, 16, 0.25, 0.4, gain=0.22, every=2)
    cinematic(s, 4, 16, gain=0.55)
    pings(s, 0, 16, gain=0.25); blips(s, 0, 16, 0.06, gain=0.12)
    return s.mix(pump=0.2), T(bars)

def start():
    """the jingle: a braam, and a fast arpeggio climbing two octaves to a ringing D"""
    s = Song(2.6)
    s.kick(0.0, 0.9)
    s.add(braam(), 0.0, 'fx', verb=0.4, gain=0.6)
    run = ['D4', 'A4', 'D5', 'F5', 'A5', 'D6']
    for k, nt in enumerate(run):
        s.add(seq_note(nt, 1.0, STEP * 0.45), k * STEP / 2, verb=0.3, dly=0.25, gain=0.7)
    top = len(run) * STEP / 2
    s.add(ping('D6', 1.6), top, verb=0.5, dly=0.4, gain=0.8)
    s.add(glass(['D4', 'A4', 'E5'], 0.6, attack=0.01, release=1.4), top, verb=0.6, gain=1.4)
    return s.mix(pump=0.2), None

def levelin():
    t0 = 1.6                                    # LEVEL_IN_OPEN: the plane bursts out of the tunnel
    s = Song(t0 + T(2) + 2.5)
    s.add(riser(t0, 'A5'), 0.0, 'fx', verb=0.3, gain=0.7)
    s.add(reverse_swell(CHORDS[0], t0), 0.0, 'fx', gain=0.7)
    s.add(braam(), t0, 'fx', verb=0.4, gain=0.85)
    s.add(subdrop(), t0, 'fx', gain=0.6)
    pads(s, 0, 2, cut=1500, t0=t0)
    rolling(s, 0, 2, 0.5, t0=t0)
    sequence(s, 0, 2, 0.8, gain=0.38, t0=t0)
    techno(s, 0, 2, t0=t0)
    fill(s, 1, t0=t0)
    theme(s, 0, t0=t0, steps=32)
    s.add(ping('D6', 1.2), t0 + T(2), verb=0.6, dly=0.5, gain=0.5)
    s.add(glass(CHORDS[0], 0.8, attack=0.05), t0 + T(2), verb=0.6, gain=0.5)
    return s.mix(), None

def approach():
    s = Song(T(4) + 0.3)                        # TUNNEL_APPROACH: four bars to the mouth
    tense = (2, 3)
    for b, cut in enumerate((700, 1000, 1500, 2200)):
        s.add(dark_pad(chord_of(b, b in tense), BAR if b < 3 else BAR - BEAT, cut, attack=0.4), T(b), verb=0.4, gain=0.9)
    sequence(s, 0, 4, 0.1, 1.0, gain=0.45, tense=tense)
    for b in range(2):
        for q in range(4): s.kick(T(b, q * 4), 0.5, muffled=True)
    for q in range(4): s.kick(T(2, q * 4), 0.6)
    for q in range(3): s.kick(T(3, q * 4), 0.65)
    rolling(s, 2, 4, 0.4, tense=tense)
    roll(s, T(2), T(3, 12), 0.5)
    s.add(riser(T(2)), T(2), 'fx', verb=0.3, gain=0.85)
    pings(s, 0, 2)
    out = s.mix()
    # the last beat: all gone but the riser - the tunnel's first beat is the drop
    i = int(round(T(3, 12) * SR))
    keep = np.zeros(s.n); keep[:i] = 1.0
    out = out * keep + fft_filter(s.fx, lo=28) * (1 - keep)
    return out, None

def tunnel():
    bars = 8
    s = Song(T(bars) + 2.0)
    s.add(braam(), 0.0, 'fx', verb=0.4, gain=0.8)
    s.add(subdrop(), 0.0, 'fx', gain=0.7)
    techno(s, 0, 4, open_hats=False)
    techno(s, 4, 8)
    acid(s, 0, 8, 240, 2400, gain=0.42)
    pads(s, 0, 8, cut=1100, gain=0.5, shimmer=0.15, tense=(3, 7))
    theme(s, 4, gain=0.55)
    blips(s, 0, 8, 0.1)
    roll(s, T(7, 8), T(8), 0.45)
    return s.mix(pump=0.45, rumble=0.3), T(bars)

def boss():
    s = Song(T(4) + 2.0)                        # BOSSINTRO (7.5 s): four bars
    tense = (1, 3)
    s.add(siren(T(4)), 0.0, 'fx', verb=0.35, gain=0.6)
    s.add(braam('D1'), T(0), 'fx', verb=0.5, gain=0.9)
    s.add(braam('Eb1'), T(2), 'fx', verb=0.5, gain=0.9)
    s.add(subdrop(), T(0), 'fx', gain=0.7)
    for b in range(4):
        s.add(dark_pad(chord_of(b, b in tense), BAR, 1400, attack=0.2), T(b), verb=0.4, gain=0.8)
    sequence(s, 0, 4, 0.5, 1.0, gain=0.4, tense=tense)
    for b in range(4):
        for st in range(0, 16, 2 if b < 2 else 1):          # toms pounding, eighths, then sixteenths
            s.add(tom(), T(b, st), 'drums', verb=0.25, gain=(0.35 if st % 4 else 0.6) * (0.7 + 0.1 * b))
        for q in range(4): s.kick(T(b, q * 4), 0.65)
    for b in (0, 2): s.add(ping('A6', 0.3), T(b, 8), verb=0.4, dly=0.4, gain=0.4)
    s.add(braam('D1'), T(4), 'fx', verb=0.5, gain=0.9)
    s.kick(T(4), 0.9)
    return s.mix(pump=0.3), None

# --- the lounge: how to play, before the first era ----------------------------

LOUNGE = [(['B3', 'D4', 'F#4', 'A4'], 'G2'),       # Gmaj9
          (['A3', 'C#4', 'E4', 'G#4'], 'F#2'),     # F#m7 (add 9 on top: G#)
          (['G3', 'B3', 'D4', 'F#4'], 'E2'),       # Em9
          (['G3', 'C#4', 'F#4', 'B4'], 'A2')]      # A13 (shell, the 9th on top)

def rhodes(note, length):
    """an electric piano: a sine struck by a decaying FM ping (the tine), warm, a slow tremolo"""
    def make():
        n = nlen(length + 0.9); t = tsec(n)
        f = hz(note)
        index = 1.4 * np.exp(-t / 0.18) + 0.25
        x = np.sin(2 * np.pi * f * t + index * np.sin(2 * np.pi * f * t))
        x += 0.12 * np.sin(2 * np.pi * f * 7.0 * t) * np.exp(-t / 0.03)          # the tine's tick
        e = np.minimum(1.0, t / 0.004) * np.exp(-t / 1.6)
        off = t > length; e[off] *= np.exp(-(t[off] - length) / 0.25)
        x *= e * (1 + 0.12 * np.sin(2 * np.pi * 4.5 * t))
        return fade_tail(fft_filter(x, hi=4000) * 0.32)
    return cached(('rhodes', note, round(length, 3)), make)

def vibe(note, length=1.4):
    """a vibraphone: two pure partials, a soft mallet, a wobble"""
    def make():
        n = nlen(length); t = tsec(n)
        f = hz(note)
        x = (np.sin(2 * np.pi * f * t) + 0.25 * np.sin(2 * np.pi * f * 4.0 * t) * np.exp(-t / 0.15)) * np.exp(-t / 0.7)
        x *= np.minimum(1.0, t / 0.002) * (1 + 0.25 * np.sin(2 * np.pi * 5.5 * t))
        return fade_tail(x * 0.3)
    return cached(('vibe', note, length), make)

def upright(note, length):
    """a round bass: a sine and a little of its octave, a soft pluck"""
    def make():
        n = nlen(length + 0.1); t = tsec(n)
        f = hz(note)
        x = np.sin(2 * np.pi * f * t) + 0.3 * np.sin(2 * np.pi * 2 * f * t) * np.exp(-t / 0.2)
        e = np.minimum(1.0, t / 0.01) * np.exp(-t / 1.2)
        off = t > length; e[off] *= np.exp(-(t[off] - length) / 0.05)
        return fade_tail(x * e * 0.5)
    return cached(('upright', note, round(length, 3)), make)

def rim():
    def make():
        n = nlen(0.12); t = tsec(n)
        x = sine(1700, n) * np.exp(-t / 0.008) * 0.5 + fft_filter(rng.standard_normal(n), lo=2500, hi=7000) * np.exp(-t / 0.015) * 0.4
        return fade_tail(norm(x, 0.4))
    return cached('rim', make)

def lounge():
    """relaxed: half time, a swing, jazz chords in D major on an electric piano, a vibraphone now and then"""
    bars = 16; tail = 3.5
    s = Song(T(bars) + tail)
    swing = 0.3                                     # the off sixteenths a little late
    def at(b, st): return T(b, st + (swing if st % 2 else 0))
    for b in range(bars):
        ch, root = LOUNGE[b % 4]
        # the piano: the chord on one, again short on the 'and' of two, a little touch before four
        for st, ln in ((0, 5 * STEP), (7, 2 * STEP), (13, 2.5 * STEP)):
            for k, nt in enumerate(ch):
                s.add(rhodes(nt, ln), at(b, st) + 0.008 * k, verb=0.35, gain=0.55 if st == 0 else 0.38)
        # the bass: the root, its fifth, a step to the next
        nxt = LOUNGE[(b + 1) % 4][1]
        fifth = note_from(hz(root) * 1.5)
        s.add(upright(root, 6 * STEP), at(b, 0), gain=0.8)
        s.add(upright(fifth, 3 * STEP), at(b, 8), gain=0.6)
        s.add(upright(note_from(hz(nxt) * 2 ** (-1 / 12)), 2 * STEP), at(b, 14), gain=0.5)
        # the drums, soft: a kick on one and on the 'and' of three, a rim click on three, a shaker
        if b >= 1:
            s.kick(at(b, 0), 0.35)
            s.kick(at(b, 10), 0.22)
            s.add(rim(), at(b, 8), 'drums', verb=0.3, gain=0.5)
            for st in range(16):
                s.add(hat(), at(b, st), 'drums', gain=0.09 if st % 2 else 0.14)
        s.add(dark_pad([nt for nt in ch], BAR, 900, attack=0.8, release=1.2), T(b), verb=0.4, gain=0.25)
    # the vibraphone: a few notes over the second half, answering the piano
    tune = [(4, 2, 'A5'), (4, 6, 'F#5'), (5, 2, 'E5'), (6, 0, 'D5'), (6, 6, 'E5'), (7, 4, 'C#5'),
            (8, 2, 'A5'), (8, 6, 'B5'), (9, 2, 'G#5'), (10, 0, 'F#5'), (10, 10, 'D5'), (11, 4, 'E5'),
            (12, 2, 'D6'), (12, 6, 'B5'), (13, 2, 'A5'), (14, 0, 'F#5'), (14, 6, 'G5'), (15, 4, 'E5')]
    for b, st, nt in tune:
        s.add(vibe(nt), at(b, st), verb=0.5, dly=0.35, gain=0.55)
    return s.mix(pump=0.0), T(bars)

def victory():
    """a boss down: the dark tune lifts - B flat, C, and up into D major, the brass climbing, held, shining"""
    s = Song(T(1) + 3.2)
    for st, ch, cut in ((0, ['Bb2', 'F3', 'Bb3', 'D4'], 2200), (8, ['C3', 'G3', 'C4', 'E4'], 2600)):
        s.add(dark_pad(ch, 8 * STEP, cut, attack=0.03, release=0.3), T(0, st), verb=0.4, gain=0.9)
        s.add(bass(ch[0], 8 * STEP * 0.95), T(0, st), gain=0.5)
        s.kick(T(0, st), 0.75)
    hold = ['D3', 'A3', 'D4', 'F#4']
    s.add(dark_pad(hold, 2.0, 3000, attack=0.03, release=1.4), T(1), verb=0.5, gain=1.0)
    s.add(glass(['D4', 'F#4', 'A4'], 1.6, attack=0.05, release=1.6), T(1), verb=0.6, dly=0.3, gain=0.6)
    s.add(bass('D2', 2.0), T(1), gain=0.55)
    s.add(braam('D1'), T(1), 'fx', verb=0.5, gain=0.7)
    s.kick(T(1), 0.9)
    for k in range(4): s.add(tom(), T(0, 12 + k), 'drums', verb=0.3, gain=0.4 + 0.1 * k)
    melody = [(T(0, 0), 'Bb4', 2 * STEP), (T(0, 2), 'C5', 2 * STEP), (T(0, 4), 'D5', 4 * STEP),
              (T(0, 8), 'C5', 2 * STEP), (T(0, 10), 'D5', 2 * STEP), (T(0, 12), 'E5', 4 * STEP),
              (T(1), 'F#5', 1.9)]
    s.add(brass_line(melody, T(1) + 1.9, cut=3200), 0.0, verb=0.4, dly=0.15, gain=0.8)
    s.add(ping('A6', 1.2), T(1), verb=0.5, dly=0.4, gain=0.4)
    return s.mix(pump=0.25), None

# loudness: the loops by their loudest two seconds, the short pieces as well (they were set by their peak and came out quiet)
PIECES = {'attract': (attract, 0.23), 'hiscore': (hiscore, 0.2), 'start': (start, 0.26),
          'levelin': (levelin, 0.24), 'approach': (approach, 0.24), 'tunnel': (tunnel, 0.25), 'boss': (boss, 0.25),
          'victory': (victory, 0.26), 'lounge': (lounge, 0.15)}

def finish(x, loop, rms):
    if loop:
        L = nlen(loop)
        tail = x[L:]
        x = x[:L].copy()
        x[:len(tail)] += tail[:L]                # the end's echoes run on into the start: no seam
    seg = min(SR * 2, len(x) - 1)
    r = max(np.sqrt(np.mean(x[i:i + seg] ** 2)) for i in range(0, max(1, len(x) - seg), max(1, seg // 2)))
    x = x * (rms / r)
    # a soft knee: under 0.6 untouched, above it bent smoothly towards 0.97
    a = np.abs(x); k = 0.6
    over = a > k
    a[over] = k + (0.97 - k) * np.tanh((a[over] - k) / (0.97 - k))
    x = np.sign(x) * a
    if not loop: x = fade_tail(x, 0.3)
    return x

def write(path, x):
    pcm = (np.clip(x, -1, 1) * 32767).astype('<i2').tobytes()
    with open(path, 'wb') as f:
        f.write(b'RIFF' + struct.pack('<I', 36 + len(pcm)) + b'WAVE')
        f.write(b'fmt ' + struct.pack('<IHHIIHH', 16, 1, 1, SR, SR * 2, 2, 16))
        f.write(b'data' + struct.pack('<I', len(pcm)) + pcm)

if __name__ == '__main__':
    out = sys.argv[1]
    names = sys.argv[2:] or list(PIECES)
    os.makedirs(out, exist_ok=True)
    for name in names:
        make, rms = PIECES[name]
        x, loop = make()
        x = finish(x, loop, rms)
        write(os.path.join(out, name + '.wav'), x)
        print('%-9s %6.2f s %s %5d KB' % (name, len(x) / SR, 'loop' if loop else '    ', (44 + 2 * len(x)) // 1024), flush=True)
