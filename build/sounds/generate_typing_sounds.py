#!/usr/bin/env python3
"""Generate the typing sounds of Jenny Script. They are synthesized, so they have no third party
licence, and the result is the same every time.

Run from the repository root: python build/sounds/generate_typing_sounds.py
It writes the WAV files and typing_sounds.qrc to src/core/management_layer/content/typing_sounds.
"""
import array
import math
import os
import random
import wave

RATE = 44100
OUTPUT = os.path.join("src", "core", "management_layer", "content", "typing_sounds")


def silence(seconds):
    return [0.0] * int(seconds * RATE)


def mix(*tracks):
    result = [0.0] * max(len(track) for track in tracks)
    for track in tracks:
        for index, value in enumerate(track):
            result[index] += value
    return result


def delay(track, seconds):
    return silence(seconds) + track


def gain(track, value):
    return [sample * value for sample in track]


def decay(index, seconds):
    return math.exp(-index / RATE / seconds)


def noise(rng, seconds, decay_time, level=1.0):
    return [level * rng.uniform(-1, 1) * decay(i, decay_time) for i in range(int(seconds * RATE))]


def low_pass(track, cutoff):
    alpha = 1 - math.exp(-2 * math.pi * cutoff / RATE)
    result, state = [], 0.0
    for sample in track:
        state += alpha * (sample - state)
        result.append(state)
    return result


def high_pass(track, cutoff):
    return [a - b for a, b in zip(track, low_pass(track, cutoff))]


def band_pass(track, low, high):
    return high_pass(low_pass(track, high), low)


def sine(frequency, seconds, decay_time, level=1.0):
    step = 2 * math.pi * frequency / RATE
    return [level * math.sin(step * i) * decay(i, decay_time) for i in range(int(seconds * RATE))]


def sweep(start, end, seconds, decay_time, level=1.0, wave_shape=math.sin):
    """Frequency slides from start to end, on a curve that sounds even to the ear"""
    count, phase, result = int(seconds * RATE), 0.0, []
    for i in range(count):
        frequency = start * (end / start) ** (i / count)
        phase += 2 * math.pi * frequency / RATE
        result.append(level * wave_shape(phase) * decay(i, decay_time))
    return result


def soft_square(phase):
    return math.tanh(3 * math.sin(phase))


def note(name):
    names = {"C": 0, "D": 2, "E": 4, "F": 5, "G": 7, "A": 9, "B": 11}
    return 440.0 * 2 ** ((names[name[0]] + 12 * (int(name[1]) - 4) - 9) / 12)


# ---- mechanical keyboard


def mechanical_key(rng, body, click_level=1.0, length=0.13):
    """Switch click, thock of the keycap on the case and the softer sound of the key coming back"""
    click = band_pass(noise(rng, 0.012, 0.003, click_level), 1800, 6500)
    thock = mix(
        sine(body, length, 0.016, 0.9),
        sine(body * 2.7, length, 0.009, 0.45),
        low_pass(noise(rng, 0.03, 0.008, 0.5), 2200),
    )
    back = band_pass(noise(rng, 0.008, 0.002, 0.35 * click_level), 1500, 5000)
    return mix(click, delay(thock, 0.002), delay(back, 0.075 + rng.uniform(-0.006, 0.006)))


def mechanical(rng):
    sounds = {}
    for index in range(6):
        sounds["key-%02d" % (index + 1)] = mechanical_key(rng, rng.uniform(210, 320))
    bar = mechanical_key(rng, 135, 0.8, 0.2)
    sounds["space"] = mix(bar, delay(low_pass(noise(rng, 0.03, 0.01, 0.25), 1500), 0.02))
    sounds["return"] = mix(mechanical_key(rng, 170, 1.3, 0.2), delay(mechanical_key(rng, 240, 0.6), 0.03))
    sounds["backspace"] = mechanical_key(rng, 360, 0.9)
    return sounds, 0.55


# ---- funny sounds


def pop(rng):
    return mix(sweep(900, 180, 0.07, 0.022), band_pass(noise(rng, 0.006, 0.002, 0.3), 1500, 6000))


def bubble(_rng):
    return sweep(320, 1300, 0.09, 0.035)


def boing(_rng, base=190, seconds=0.32):
    result = []
    phase = 0.0
    for i in range(int(seconds * RATE)):
        time = i / RATE
        phase += 2 * math.pi * base * (1 + 0.28 * math.sin(2 * math.pi * 22 * time) * math.exp(-time * 7)) / RATE
        result.append((math.sin(phase) + 0.25 * math.sin(2 * phase)) * decay(i, 0.1))
    return result


def squeak(_rng):
    return mix(sweep(1500, 2500, 0.05, 0.05), delay(sweep(2500, 1600, 0.07, 0.04), 0.05))


def quack(rng):
    tone = sweep(330, 240, 0.13, 0.09, 1.0, lambda phase: 2 * ((phase / (2 * math.pi)) % 1) - 1)
    return band_pass(tone, 500 + rng.uniform(-40, 40), 1900)


def laser(_rng):
    return sweep(2300, 320, 0.13, 0.06, 1.0, soft_square)


def coin(_rng):
    return mix(sine(note("B5"), 0.05, 0.05), delay(sine(note("E6"), 0.17, 0.05), 0.05))


def funny(rng):
    keys = [pop, bubble, squeak, quack, laser, coin, pop, bubble]
    sounds = {"key-%02d" % (i + 1): make(rng) for i, make in enumerate(keys)}
    sounds["space"] = boing(rng)
    fanfare = [delay(sine(note(name), 0.3, 0.09), 0.08 * step)
               for step, name in enumerate(["C5", "E5", "G5", "C6"])]
    sounds["return"] = mix(*fanfare)
    sounds["backspace"] = sweep(620, 130, 0.25, 0.1, 1.0, soft_square)
    return sounds, 0.35


# ---- piano


def piano_note(rng, frequency, seconds=0.65):
    result = [0.0] * int(seconds * RATE)
    inharmonicity = 0.0004
    for partial in range(1, 9):
        partial_frequency = frequency * partial * math.sqrt(1 + inharmonicity * partial * partial)
        if partial_frequency > RATE / 2.5:
            break
        step = 2 * math.pi * partial_frequency / RATE
        level = 1.0 / partial ** 1.35
        decay_time = 0.42 / partial ** 0.75
        for i in range(len(result)):
            result[i] += level * math.sin(step * i) * decay(i, decay_time)
    hammer = low_pass(noise(rng, 0.012, 0.004, 0.1), 3500)
    attack = int(0.002 * RATE)
    for i in range(attack):
        result[i] *= i / attack
    return mix(result, hammer)


def piano(rng):
    scale = ["C4", "D4", "E4", "G4", "A4", "C5", "D5", "E5"]
    sounds = {"key-%02d" % (i + 1): piano_note(rng, note(name)) for i, name in enumerate(scale)}
    sounds["space"] = gain(piano_note(rng, note("G3")), 0.8)
    chord = [delay(piano_note(rng, note(name), 0.9), 0.018 * step)
             for step, name in enumerate(["C4", "E4", "G4", "C5"])]
    sounds["return"] = mix(*chord)
    sounds["backspace"] = gain(piano_note(rng, note("C3"), 0.45), 0.8)
    return sounds, 0.4


# ---- drums


def hi_hat(rng, seconds, decay_time, level=1.0):
    return high_pass(noise(rng, seconds, decay_time, level), 6500)


def tom(rng, frequency):
    return mix(sweep(frequency * 1.5, frequency, 0.2, 0.08), low_pass(noise(rng, 0.02, 0.006, 0.3), 3000))


def snare(rng, level=1.0):
    return gain(mix(sweep(260, 185, 0.14, 0.05, 0.6), high_pass(noise(rng, 0.16, 0.055, 0.9), 1400)), level)


def kick(rng):
    return mix(sweep(170, 48, 0.24, 0.09), low_pass(noise(rng, 0.006, 0.002, 0.4), 4000))


def rim(rng):
    return mix(sine(1650, 0.04, 0.007, 0.7), band_pass(noise(rng, 0.01, 0.003, 0.5), 1500, 7000))


def drums(rng):
    sounds = {
        "key-01": hi_hat(rng, 0.05, 0.012),
        "key-02": hi_hat(rng, 0.05, 0.016, 0.8),
        "key-03": hi_hat(rng, 0.09, 0.03, 0.8),
        "key-04": rim(rng),
        "key-05": snare(rng, 0.65),
        "key-06": tom(rng, 210),
        "key-07": tom(rng, 150),
    }
    sounds["space"] = kick(rng)
    crash = high_pass(noise(rng, 0.8, 0.26, 0.7), 3200)
    sounds["return"] = mix(crash, kick(rng), snare(rng, 0.6))
    sounds["backspace"] = tom(rng, 95)
    return sounds, 0.45


# ---- violin


def violin_note(rng, frequency, seconds=0.38, vibrato=True):
    count, phases, result = int(seconds * RATE), [0.0] * 12, []
    for i in range(count):
        time = i / RATE
        depth = min(1.0, max(0.0, (time - 0.08) / 0.12)) * 0.006 if vibrato else 0.0
        current = frequency * (1 + depth * math.sin(2 * math.pi * 5.6 * time))
        sample = 0.0
        for partial in range(1, 13):
            partial_frequency = current * partial
            if partial_frequency > RATE / 2.5:
                break
            phases[partial - 1] += 2 * math.pi * partial_frequency / RATE
            # The wood of the violin makes some partials stronger
            body = 1.0 + 0.9 * math.exp(-(((partial_frequency - 2800) / 900) ** 2))
            body += 0.5 * math.exp(-(((partial_frequency - 500) / 200) ** 2))
            sample += body * math.sin(phases[partial - 1]) / partial
        envelope = min(1.0, time / 0.035) * min(1.0, (seconds - time) / 0.09)
        result.append(sample * envelope)
    bow = band_pass(noise(rng, seconds, 0.2, 0.05), 2000, 7000)
    envelope_bow = [value * min(1.0, (seconds - i / RATE) / 0.09) for i, value in enumerate(bow)]
    return mix(result, envelope_bow)


def pizzicato(frequency, seconds=0.35):
    result = [0.0] * int(seconds * RATE)
    for partial in range(1, 7):
        partial_frequency = frequency * partial
        step = 2 * math.pi * partial_frequency / RATE
        for i in range(len(result)):
            result[i] += math.sin(step * i) * decay(i, 0.16 / partial ** 0.8) / partial ** 1.2
    return result


def violin(rng):
    scale = ["G4", "A4", "B4", "D5", "E5", "G5", "A5", "B5"]
    sounds = {"key-%02d" % (i + 1): violin_note(rng, note(name)) for i, name in enumerate(scale)}
    sounds["space"] = pizzicato(note("D4"))
    sounds["return"] = mix(violin_note(rng, note("G4"), 0.7), violin_note(rng, note("D5"), 0.7))
    sounds["backspace"] = pizzicato(note("G3"), 0.3)
    return sounds, 0.3


# ---- output


def finish(track, peak):
    loudest = max(abs(sample) for sample in track)
    assert loudest > 0
    fade = min(int(0.005 * RATE), len(track))
    result = [sample / loudest * peak for sample in track]
    for i in range(fade):
        result[-1 - i] *= i / fade
    return result


def write_wav(path, track):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    data = array.array("h", (int(max(-1.0, min(1.0, sample)) * 32767) for sample in track))
    if array.array("h", [1]).tobytes()[0] != 1:
        data.byteswap()
    with wave.open(path, "wb") as output:
        output.setnchannels(1)
        output.setsampwidth(2)
        output.setframerate(RATE)
        output.writeframes(data.tobytes())


def main():
    assert os.path.isdir(OUTPUT), "run from the repository root"
    packs = {
        "mechanical": mechanical,
        "funny": funny,
        "piano": piano,
        "drums": drums,
        "violin": violin,
    }
    qrc = ["<RCC>", '    <qresource prefix="/typing-sounds">']
    total = 0
    for seed, (pack, make) in enumerate(packs.items()):
        sounds, peak = make(random.Random(1000 + seed))
        keys = sum(name.startswith("key-") for name in sounds)
        assert {"space", "return", "backspace"} <= set(sounds) and keys >= 4
        print("%s: %d key sounds" % (pack, keys))
        for name in sorted(sounds):
            track = finish(sounds[name], peak)
            relative = "audio/%s/%s.wav" % (pack, name)
            path = os.path.join(OUTPUT, *relative.split("/"))
            write_wav(path, track)
            total += os.path.getsize(path)
            rms = math.sqrt(sum(sample * sample for sample in track) / len(track))
            print("  %-10s %.3f s  rms %.3f" % (name, len(track) / RATE, rms))
            qrc.append('        <file alias="%s/%s">%s</file>' % (pack, name, relative))
    qrc += ["    </qresource>", "</RCC>", ""]
    with open(os.path.join(OUTPUT, "typing_sounds.qrc"), "w", encoding="utf-8", newline="\n") as out:
        out.write("\n".join(qrc))
    print("total %.2f MB" % (total / 1024 / 1024))


if __name__ == "__main__":
    main()
