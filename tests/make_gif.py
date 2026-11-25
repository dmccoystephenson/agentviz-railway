"""Joins PNG frames into a looping GIF with Pillow (no ffmpeg needed).
    python3 tests/make_gif.py FRAME_DIR OUT.gif FPS [WIDTH]"""
import glob
import os
import sys

from PIL import Image


def main(src, out, fps, width=800):
    files = sorted(glob.glob(os.path.join(src, "*.png")))
    frames = []
    for f in files:
        im = Image.open(f).convert("RGB")
        if im.width != width:
            im = im.resize((width, round(im.height * width / im.width)), Image.LANCZOS)
        frames.append(im.quantize(colors=128, method=Image.MEDIANCUT, dither=Image.FLOYDSTEINBERG))
    frames[0].save(out, save_all=True, append_images=frames[1:], duration=round(1000 / fps), loop=0, optimize=True)


if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2], float(sys.argv[3]), int(sys.argv[4]) if len(sys.argv) > 4 else 800)
