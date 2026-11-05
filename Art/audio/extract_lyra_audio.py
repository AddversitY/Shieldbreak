"""Copies the source WAVs embedded in the Lyra sound-wave .uasset files under Content/Audio to Art/audio/.lyra_src,
so Art/audio/make_sfx.py can layer them. Needs the real files (git lfs pull --include="Content/Audio/**").
Usage: python -I Art/audio/extract_lyra_audio.py
"""
import glob
import io
import os
import struct
import wave

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", "..", "Content", "Audio"))
OUT = os.path.join(HERE, ".lyra_src")


def main():
    os.makedirs(OUT, exist_ok=True)
    count = 0
    for path in glob.glob(os.path.join(ROOT, "**", "*.uasset"), recursive=True):
        data = open(path, "rb").read()
        at = data.find(b"RIFF")
        if at < 0:
            continue
        size = struct.unpack("<I", data[at + 4:at + 8])[0]
        blob = data[at:at + 8 + size]
        try:
            wave.open(io.BytesIO(blob)).getnframes()
        except Exception:
            continue
        rel = os.path.relpath(path, ROOT)[:-7].replace(os.sep, "__").replace("/", "__")
        with open(os.path.join(OUT, rel + ".wav"), "wb") as f:
            f.write(blob)
        count += 1
    print("extracted %d source waves to %s" % (count, OUT))


if __name__ == "__main__":
    main()
