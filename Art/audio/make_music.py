"""Original menu music for Shieldbreak, synthesized from scratch with numpy (no samples, no third-party audio).

Writes Art/Export/Audio/MUS_SB_Menu.wav: a seamless 48 s loop (16 bars at 80 BPM, D minor). Layers: a detuned saw
pad, a sub bass, a soft "choir" lead (pulse waves through vowel formants) in the second half, a quiet plucked arpeggio
and a low drum pulse. Filters and reverb are done in the frequency domain so the whole thing renders in seconds.
Tools/ImportArt.bat imports it to /Game/Shieldbreak/Art/Audio (looping).

Run: python3 Art/audio/make_music.py
"""
import os
import wave

import numpy as np

RATE = 44100
BPM = 80.0
BEAT = 60.0 / BPM
BAR = 4 * BEAT
BARS = 16
LENGTH = BARS * BAR
N = int(round(LENGTH * RATE))
OUT = os.path.join(os.path.dirname(__file__), "..", "Export", "Audio", "MUS_SB_Menu.wav")

NOTE = {"C": 0, "C#": 1, "D": 2, "Eb": 3, "E": 4, "F": 5, "F#": 6, "G": 7, "Ab": 8, "A": 9, "Bb": 10, "B": 11}


def hz(name):
    """'D4' -> frequency."""
    pitch, octave = name[:-1], int(name[-1])
    return 440.0 * 2.0 ** ((NOTE[pitch] + 12 * (octave + 1) - 69) / 12.0)


# Two bars per chord: Dm, Bbmaj7, Gm, A(sus4 -> A); twice.
CHORDS = [
    ["D3", "A3", "D4", "F4"],
    ["Bb2", "F3", "A3", "D4"],
    ["G2", "D3", "G3", "Bb3"],
    ["A2", "E3", "A3", "D4"],
] * 2
ROOTS = ["D2", "Bb1", "G1", "A1"] * 2
# Lead for bars 9-16: (note, start beat within the section, length in beats).
LEAD = [
    ("D5", 0, 3), ("F5", 3, 1), ("E5", 4, 2), ("C5", 6, 2),
    ("D5", 8, 4), ("A4", 12, 4),
    ("Bb4", 16, 2), ("D5", 18, 2), ("C5", 20, 3), ("A4", 23, 1),
    ("D5", 24, 2), ("E5", 26, 1), ("F5", 27, 1), ("E5", 28, 4),
]
ARP = [0, 2, 3, 1, 2, 3, 2, 1]  # indexes into the chord, eighth notes

t_all = np.arange(N) / RATE
rng = np.random.default_rng(7)


def envelope(length, attack, release, sustain=1.0):
    n = int(length * RATE)
    env = np.full(n, sustain)
    a = max(1, int(attack * RATE))
    r = max(1, int(release * RATE))
    env[:a] = np.linspace(0.0, sustain, a)[: n]
    env[-r:] *= np.linspace(1.0, 0.0, r)
    return env


def place(track, start, signal):
    """Adds signal at start seconds, wrapping past the end so the loop is seamless."""
    i = int(round(start * RATE)) % N
    end = i + len(signal)
    if end <= N:
        track[i:end] += signal
    else:
        first = N - i
        track[i:] += signal[:first]
        rest = signal[first:]
        while len(rest):
            take = min(len(rest), N)
            track[:take] += rest[:take]
            rest = rest[take:]


def spectral(signal, response):
    """Zero-phase filter: response(freqs) -> gain."""
    spectrum = np.fft.rfft(signal)
    freqs = np.fft.rfftfreq(len(signal), 1.0 / RATE)
    return np.fft.irfft(spectrum * response(freqs), len(signal))


def lowpass(cutoff, order=2):
    return lambda f: 1.0 / np.sqrt(1.0 + (f / cutoff) ** (2 * order))


def saw(freq, length, detune_cents=0.0, phase=None):
    t = np.arange(int(length * RATE)) / RATE
    f = freq * 2.0 ** (detune_cents / 1200.0)
    p = rng.random() if phase is None else phase
    return 2.0 * ((t * f + p) % 1.0) - 1.0


def circular_reverb(signal, seconds, seed):
    """Convolves with decaying noise, circularly, so the tail wraps into the loop start."""
    local = np.random.default_rng(seed)
    n = int(seconds * RATE)
    t = np.arange(n) / RATE
    ir = local.standard_normal(n) * np.exp(-t * 6.9 / seconds)  # -60 dB at the end
    ir = spectral(ir, lowpass(5000.0))
    ir[: int(0.02 * RATE)] *= np.linspace(0, 1, int(0.02 * RATE))  # predelay-ish soft onset
    ir /= np.sqrt(np.sum(ir ** 2))
    padded = np.zeros(N)
    padded[:n] = ir
    return np.fft.irfft(np.fft.rfft(signal) * np.fft.rfft(padded), N)


def pad():
    left, right = np.zeros(N), np.zeros(N)
    length = 2 * BAR
    for index, chord in enumerate(CHORDS):
        start = index * length
        env = envelope(length + 1.5, 1.2, 1.8, 1.0)
        for note in chord:
            for voice, cents in enumerate((-9.0, -4.0, 0.0, 4.0, 9.0)):
                tone = saw(hz(note), length + 1.5, cents) * env
                pan = voice / 4.0
                place(left, start, tone * (1.0 - pan) * 0.5)
                place(right, start, tone * pan * 0.5)
    # Slow filter swell: brighter in the second half.
    swell = 0.5 - 0.5 * np.cos(2 * np.pi * t_all / LENGTH)
    dark_l, dark_r = spectral(left, lowpass(500.0, 2)), spectral(right, lowpass(500.0, 2))
    bright_l, bright_r = spectral(left, lowpass(1600.0, 2)), spectral(right, lowpass(1600.0, 2))
    return (dark_l * (1 - swell) + bright_l * swell), (dark_r * (1 - swell) + bright_r * swell)


def bass():
    out = np.zeros(N)
    for index, root in enumerate(ROOTS):
        length = 2 * BAR
        t = np.arange(int((length + 0.5) * RATE)) / RATE
        tone = np.sin(2 * np.pi * hz(root) * t) + 0.25 * np.sin(4 * np.pi * hz(root) * t)
        place(out, index * length, tone * envelope(length + 0.5, 0.3, 0.8))
    return out


def formant(signal):
    """Vowel 'ah': three resonances."""
    def response(f):
        gain = np.zeros_like(f)
        for center, width, level in ((800.0, 90.0, 1.0), (1150.0, 110.0, 0.6), (2900.0, 150.0, 0.25)):
            gain += level / (1.0 + ((f - center) / width) ** 2)
        return gain + 0.05
    return spectral(signal, response)


def lead():
    out = np.zeros(N)
    section = 8 * BAR
    for note, beat, beats in LEAD:
        length = beats * BEAT + 0.6
        t = np.arange(int(length * RATE)) / RATE
        f = hz(note)
        vibrato = 1.0 + 0.004 * np.sin(2 * np.pi * 5.0 * t) * np.clip(t / 0.6, 0, 1)
        phase = np.cumsum(f * vibrato) / RATE
        tone = np.zeros_like(t)
        for cents in (-6.0, 0.0, 6.0):
            p = phase * 2.0 ** (cents / 1200.0) + rng.random()
            tone += np.where((p % 1.0) < 0.3, 1.0, -0.43)  # narrow pulse: nasal, voice-like
        place(out, section + beat * BEAT, tone * envelope(length, 0.25, 0.6))
    return formant(out)


def arpeggio():
    left, right = np.zeros(N), np.zeros(N)
    eighth = BEAT / 2
    for bar in range(4, BARS):
        chord = CHORDS[bar // 2]
        for step, which in enumerate(ARP):
            f = hz(chord[which]) * 2.0
            t = np.arange(int(0.9 * RATE)) / RATE
            tone = (2 * np.abs(2 * ((t * f) % 1.0) - 1) - 1) * np.exp(-t * 5.0)  # triangle pluck
            gain = 0.6 if step % 2 == 0 else 0.4
            pan = 0.3 if step % 2 == 0 else 0.7
            start = bar * BAR + step * eighth
            place(left, start, tone * gain * (1 - pan))
            place(right, start, tone * gain * pan)
    return spectral(left, lowpass(3000.0)), spectral(right, lowpass(3000.0))


def drums():
    out = np.zeros(N)
    for bar in range(8, BARS):
        for beat in (0, 2.5, 3):
            t = np.arange(int(0.8 * RATE)) / RATE
            pitch = 55.0 + 60.0 * np.exp(-t * 18.0)
            body = np.sin(2 * np.pi * np.cumsum(pitch) / RATE) * np.exp(-t * 4.5)
            noise = rng.standard_normal(len(t)) * np.exp(-t * 30.0) * 0.15
            gain = 1.0 if beat == 0 else 0.6
            place(out, bar * BAR + beat * BEAT, (body + noise) * gain)
    return spectral(out, lowpass(900.0))


def main():
    pad_l, pad_r = pad()
    arp_l, arp_r = arpeggio()
    low = bass()
    voice = lead()
    hits = drums()

    def norm(x):
        return x / (np.max(np.abs(x)) + 1e-9)

    dry_l = 0.55 * norm(pad_l) + 0.35 * norm(low) + 0.22 * norm(arp_l) + 0.3 * norm(voice) + 0.35 * norm(hits)
    dry_r = 0.55 * norm(pad_r) + 0.35 * norm(low) + 0.22 * norm(arp_r) + 0.3 * norm(voice) + 0.35 * norm(hits)
    send_l = 0.5 * norm(pad_l) + 0.4 * norm(arp_l) + 0.6 * norm(voice) + 0.15 * norm(hits)
    send_r = 0.5 * norm(pad_r) + 0.4 * norm(arp_r) + 0.6 * norm(voice) + 0.15 * norm(hits)
    wet_l = circular_reverb(send_l, 3.5, 1)
    wet_r = circular_reverb(send_r, 3.5, 2)
    left = dry_l + 0.9 * norm(wet_l) * np.max(np.abs(send_l)) * 0.6
    right = dry_r + 0.9 * norm(wet_r) * np.max(np.abs(send_r)) * 0.6

    # Gentle bus compression (tanh) then normalize to -1 dBFS.
    stereo = np.stack([left, right], axis=1)
    stereo = np.tanh(stereo / np.max(np.abs(stereo)) * 1.4)
    stereo *= 10 ** (-1.0 / 20.0) / np.max(np.abs(stereo))

    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    pcm = (stereo * 32767.0).astype(np.int16)
    with wave.open(OUT, "wb") as f:
        f.setnchannels(2)
        f.setsampwidth(2)
        f.setframerate(RATE)
        f.writeframes(pcm.tobytes())
    rms = np.sqrt(np.mean(stereo ** 2))
    print("%s: %.1f s, peak -1 dBFS, rms %.1f dBFS" % (os.path.normpath(OUT), LENGTH, 20 * np.log10(rms)))


if __name__ == "__main__":
    main()
