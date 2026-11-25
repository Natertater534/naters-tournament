"""Synthesises the mod's short UI sounds (original), encoded to OGG with ffmpeg.

Run: python3 tools/make_sfx.py -> resources/sfx/*.ogg
"""
import os, subprocess, tempfile
import numpy as np
from scipy import signal
from scipy.io import wavfile

SR = 44100
OUT = os.path.join(os.path.dirname(__file__), '..', 'resources', 'sfx')
os.makedirs(OUT, exist_ok=True)
rng = np.random.default_rng(7)

def t(sec): return np.arange(int(SR * sec)) / SR

def env(n, attack, decay_tau):
    x = np.arange(n) / SR
    a = np.clip(x / max(attack, 1e-4), 0, 1)
    return a * np.exp(-np.maximum(0, x - attack) / decay_tau)

def chirp_sine(f0, f1, sec, curve='exp'):
    x = t(sec)
    if curve == 'exp':
        f = f0 * (f1 / f0) ** (x / sec)
    else:
        f = f0 + (f1 - f0) * x / sec
    return np.sin(2 * np.pi * np.cumsum(f) / SR)

def bp(x, lo, hi, order=4):
    sos = signal.butter(order, [lo, hi], btype='band', fs=SR, output='sos')
    return signal.sosfilt(sos, x)

def lp(x, f, order=4):
    return signal.sosfilt(signal.butter(order, f, btype='low', fs=SR, output='sos'), x)

def hp(x, f, order=4):
    return signal.sosfilt(signal.butter(order, f, btype='high', fs=SR, output='sos'), x)

def fade_tail(x, ms=8):
    n = int(SR * ms / 1000)
    x[-n:] *= np.linspace(1, 0, n)
    return x

def save(name, x, peak_db=-2.0):
    x = np.asarray(x, dtype=np.float64)
    x = fade_tail(x - np.mean(x[:64]) * 0)
    x = x / (np.max(np.abs(x)) + 1e-9) * (10 ** (peak_db / 20))
    with tempfile.NamedTemporaryFile(suffix='.wav', delete=False) as f:
        wavfile.write(f.name, SR, (x * 32767).astype(np.int16))
        subprocess.run(['ffmpeg', '-y', '-loglevel', 'error', '-i', f.name, '-c:a', 'libvorbis', '-q:a', '5',
                        f'{OUT}/{name}.ogg'], check=True)
        os.unlink(f.name)

# pop: bright pitch-drop blip with a click (poof / appear)
def pop():
    n = int(SR * .12)
    s = chirp_sine(1500, 330, .12) * env(n, .002, .045)
    click = hp(rng.standard_normal(n), 2500) * env(n, .0005, .004) * .5
    return s + click

# boop: soft up-chirp (bubble)
def boop():
    n = int(SR * .11)
    return chirp_sine(520, 980, .11) * env(n, .004, .05) * (0.6 + 0.4 * np.sin(2 * np.pi * 30 * t(.11)))

# whoosh: band-passed noise sweeping up then down
def whoosh():
    sec = .42; n = int(SR * sec)
    noise = rng.standard_normal(n)
    f, tt, Z = signal.stft(noise, SR, nperseg=1024)
    centre = 500 + 2200 * np.sin(np.pi * np.clip(tt / sec, 0, 1)) ** 1.5
    mask = np.exp(-((f[:, None] - centre[None, :]) / (350 + centre[None, :] * .35)) ** 2)
    _, y = signal.istft(Z * mask, SR, nperseg=1024)
    y = y[:n]
    shape = np.sin(np.pi * np.clip(np.arange(len(y)) / len(y), 0, 1)) ** 1.6
    return y * shape

# land: soft thud
def land():
    n = int(SR * .2)
    s = chirp_sine(150, 52, .2) * env(n, .002, .07)
    thump = lp(rng.standard_normal(n), 600) * env(n, .001, .02) * 1.4
    return s + thump

# clang: two swords meeting (inharmonic partials + bright transient)
def clang():
    sec = 1.1; n = int(SR * sec); x = t(sec); y = np.zeros(n)
    for f, a, d in [(523, 1.0, .9), (1137, .7, .6), (1873, .55, .45), (2711, .42, .33), (3529, .3, .25),
                    (4410, .2, .18), (5333, .12, .12)]:
        y += a * np.sin(2 * np.pi * f * x) * np.exp(-x / d)
        y += a * .5 * np.sin(2 * np.pi * (f + 3.1) * x + 1.3) * np.exp(-x / d)
    trans = hp(rng.standard_normal(n), 3000) * env(n, .0003, .008) * 2.2
    body = lp(rng.standard_normal(n), 900) * env(n, .001, .03) * .8
    return y * env(n, .001, 10) + trans + body

# hit: knockout impact
def hit():
    sec = .45; n = int(SR * sec)
    boom = chirp_sine(110, 38, sec) * env(n, .003, .16) * 1.3
    crunch = bp(rng.standard_normal(n), 200, 2400) * env(n, .001, .05) * 1.1
    snap = hp(rng.standard_normal(n), 4000) * env(n, .0003, .006)
    return np.tanh((boom + crunch + snap) * 1.6)

# tick: woodblock for countdowns
def tick():
    sec = .09; n = int(SR * sec); x = t(sec)
    return (np.sin(2 * np.pi * 1750 * x) + .6 * np.sin(2 * np.pi * 2830 * x)) * env(n, .0008, .018)

def tone(freq, sec, bright=.35, vib=0.0):
    x = t(sec)
    ph = 2 * np.pi * freq * x + vib * np.sin(2 * np.pi * 5.5 * x)
    return np.sin(ph) + bright * np.sin(2 * ph) * .5 + bright * .3 * np.sin(3 * ph)

# go: bright chord stab
def go():
    sec = .6; n = int(SR * sec)
    y = sum(tone(f, sec, .5) for f in (523.25, 659.25, 783.99, 1046.5)) / 4
    return y * env(n, .004, .18)

# win: little fanfare arpeggio
def win():
    notes = [(523.25, .09), (659.25, .09), (783.99, .09), (1046.5, .5)]
    parts = []
    for f, d in notes:
        n = int(SR * d)
        parts.append(tone(f, d, .45, vib=.004 if d > .2 else 0) * env(n, .003, d * .7 if d > .2 else .08))
    y = np.concatenate(parts)
    return np.concatenate([y, np.zeros(int(SR * .1))])

# drum: low tom for big reveals
def drum():
    sec = .5; n = int(SR * sec)
    return chirp_sine(160, 70, sec) * env(n, .002, .14) + lp(rng.standard_normal(n), 1500) * env(n, .001, .015)

for name, fn in [('pop', pop), ('boop', boop), ('whoosh', whoosh), ('land', land), ('clang', clang),
                 ('hit', hit), ('tick', tick), ('go', go), ('win', win), ('drum', drum)]:
    save(name, fn())
print(sorted(os.listdir(OUT)))
