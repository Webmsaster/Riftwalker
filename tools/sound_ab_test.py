"""A/B compare OLD vs NEW procedural sound synthesis.

Mirrors the C++ SoundGenerator math (old hard-square / raw-noise / no-declick
vs new bandlimited-square / lowpassed-noise / anti-click envelope) and renders
before/after WAVs so the change can be judged by ear, plus prints peak/RMS to
confirm no clipping regression.

Output: dist/sound_ab/<name>_OLD.wav, _NEW.wav
"""
import math, os, random, struct, wave

SR = 44100
OUT = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "dist", "sound_ab")


def blank(dur): return [0.0] * int(dur * SR)


def _span(buf, t0, t1):
    if t1 < 0: t1 = len(buf) / SR
    a = int(t0 * SR); b = min(int(t1 * SR), len(buf))
    return a, b


def add_sine(buf, freq, v0, v1, t0=0, t1=-1):
    a, b = _span(buf, t0, t1)
    for i in range(a, b):
        t = (i - a) / (b - a)
        buf[i] += math.sin(2*math.pi*freq*i/SR) * (v0 + (v1-v0)*t)


def add_sweep(buf, f0, f1, v0, v1, t0=0, t1=-1):
    a, b = _span(buf, t0, t1); ph = 0.0
    for i in range(a, b):
        t = (i - a) / (b - a)
        ph += (f0 + (f1-f0)*t) / SR
        buf[i] += math.sin(2*math.pi*ph) * (v0 + (v1-v0)*t)


def add_square_OLD(buf, freq, v0, v1, t0=0, t1=-1):
    a, b = _span(buf, t0, t1)
    for i in range(a, b):
        t = (i - a) / (b - a)
        ph = (freq*i/SR) % 1.0
        buf[i] += (1.0 if ph < 0.5 else -1.0) * (v0 + (v1-v0)*t)


def add_square_NEW(buf, freq, v0, v1, t0=0, t1=-1):
    a, b = _span(buf, t0, t1); nyq = SR*0.45
    for i in range(a, b):
        t = (i - a) / (b - a)
        theta = 2*math.pi*freq*i/SR; s = 0.0; h = 1
        while h <= 13 and h*freq < nyq:
            s += math.sin(h*theta)/h; h += 2
        buf[i] += s * (4.0/math.pi) * (v0 + (v1-v0)*t)


def add_noise_OLD(buf, v0, v1, t0=0, t1=-1):
    a, b = _span(buf, t0, t1)
    for i in range(a, b):
        t = (i - a) / (b - a)
        buf[i] += (random.random()*2-1) * (v0 + (v1-v0)*t)


def add_noise_NEW(buf, v0, v1, t0=0, t1=-1):
    a, b = _span(buf, t0, t1); lp = 0.0
    for i in range(a, b):
        t = (i - a) / (b - a)
        white = random.random()*2-1
        lp += 0.45*(white-lp)
        buf[i] += lp*1.5 * (v0 + (v1-v0)*t)


def to_int16(buf, declick):
    n = len(buf); dur = n/SR
    atk = rel = 0
    if declick and dur <= 1.2:
        q = n//4
        atk = min(q, int(0.004*SR)); rel = min(q, int(0.012*SR))
    out = []
    for i, v in enumerate(buf):
        env = 1.0
        if atk or rel:
            fe = n-1-i
            if i < atk: env = i/atk
            elif fe < rel: env = fe/rel
        out.append(max(-32767, min(32767, int(round(v*env*32767)))))
    return out


def save(name, samples):
    os.makedirs(OUT, exist_ok=True)
    with wave.open(os.path.join(OUT, name), "w") as w:
        w.setnchannels(1); w.setsampwidth(2); w.setframerate(SR)
        w.writeframes(b"".join(struct.pack("<h", s) for s in samples))


def stats(samples):
    peak = max(abs(s) for s in samples) / 32767
    rms = (sum(s*s for s in samples)/len(samples))**0.5 / 32767
    clipped = sum(1 for s in samples if abs(s) >= 32767)
    return f"peak={peak:.2f} rms={rms:.3f} clipped={clipped}"


# (name, duration, [(prim, *args)]) — prim picks OLD/NEW variant automatically.
SOUNDS = {
    "meleeHit": (0.15, [("noise",0.35,0,0,0.05),("sine",150,0.25,0,0,0.15),("square",80,0.15,0,0,0.1)]),
    "collapseWarning": (0.30, [("square",200,0.3,0,0,0.15),("square",200,0.3,0,0.15,0.3)]),
    "playerDash": (0.18, [("sweep",150,800,0.3,0),("noise",0.15,0)]),
    "criticalHit": (0.20, [("noise",0.35,0,0,0.05),("sine",1000,0.25,0,0,0.15),("sine",600,0.2,0,0.02,0.18),("square",200,0.15,0,0,0.1)]),
    "shockTrap": (0.30, [("square",120,0.4,0,0,0.25),("square",800,0.15,0,0,0.18),("noise",0,0.25,0.22,0.27),("noise",0.25,0,0.27,0.3),("sine",60,0.3,0,0,0.08)]),
}


def render(dur, layers, new):
    random.seed(42)  # same noise both ways for fair A/B
    buf = blank(dur)
    for L in layers:
        p = L[0]; args = L[1:]
        if p == "sine": add_sine(buf, *args)
        elif p == "sweep": add_sweep(buf, *args)
        elif p == "square": (add_square_NEW if new else add_square_OLD)(buf, *args)
        elif p == "noise": (add_noise_NEW if new else add_noise_OLD)(buf, *args)
    return to_int16(buf, declick=new)


def main():
    for name, (dur, layers) in SOUNDS.items():
        old = render(dur, layers, new=False)
        new = render(dur, layers, new=True)
        save(f"{name}_OLD.wav", old)
        save(f"{name}_NEW.wav", new)
        print(f"{name:18} OLD {stats(old)}   NEW {stats(new)}")
    print(f"\nWrote A/B WAVs to {OUT}")


if __name__ == "__main__":
    main()
