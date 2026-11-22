"""GP-100 boot animation tool: show, extract, and replace the boot animation inside a firmware image. Offline only.

The boot animation is a plain GIF stored in the firmware (V2.1: file 0x12d185..0x157400, 160x128, 128-colour global palette,
transparent sub-rectangle frames, 40 ms per frame). A small descriptor {length+1, pointer} sits at file 0x3c3b9. This tool writes a new
GIF with the SAME structure (own LZW encoder, so nothing depends on a library's choices) into the same space and fixes the length and
the 8-bit body sum. The new GIF can never be larger than the original region; if the input is too big it is reduced automatically.

  bootanim.py info FW
  bootanim.py extract FW OUT.gif
  bootanim.py demo OUT.gif                       # generate a test animation
  bootanim.py build FW INPUT OUT.bin [options]   # INPUT: .gif, a video (ffmpeg), a still image, or a folder of images
"""

import io
import struct
import subprocess
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFont, ImageSequence

from gp100mods import frmw
from gp100mods.stock import guarded

GIF_START = 0x12D185
GIF_END = 0x157400                  # exclusive; followed by "\0hardInfo:"
DESC_OFF = 0x3C3B9                  # {u32 length+1, u32 pointer 0x800f87c4}
DESC_PTR = 0x800F87C4
W, H = 160, 128
NCOL = 128                          # global palette size (LZW minimum code size 7)
TRANSPARENT = 127
CROP = None                         # optional (x0, y0, x1, y1) source crop applied before fitting


# ----------------------------------------------------------------------------- GIF reading (strict, block by block)
def gif_extent(data, start):
    """Return (end, frame_count) of the GIF starting at `start`; raises on anything unexpected."""
    if data[start:start + 6] not in (b"GIF89a", b"GIF87a"):
        raise ValueError("no GIF header at the expected offset")
    p = start + 6
    fl = data[p + 4]
    p += 7
    if fl & 0x80:
        p += 3 * (2 << (fl & 7))
    frames = 0
    while True:
        b = data[p]
        if b == 0x3B:
            return p + 1, frames
        if b == 0x21:
            p += 2
            while data[p]:
                p += data[p] + 1
            p += 1
        elif b == 0x2C:
            fl2 = data[p + 9]
            p += 10
            if fl2 & 0x80:
                p += 3 * (2 << (fl2 & 7))
            p += 1
            while data[p]:
                p += data[p] + 1
            p += 1
            frames += 1
        else:
            raise ValueError(f"unexpected GIF block 0x{b:02x} at 0x{p:x}")


def check_layout(fw):
    end, frames = gif_extent(fw, GIF_START)
    if end != GIF_END:
        raise ValueError(f"GIF ends at 0x{end:x}, expected 0x{GIF_END:x}: this is not the V2.1 layout")
    if fw[GIF_END:GIF_END + 10] != b"\x00hardInfo:":
        raise ValueError("unexpected bytes after the GIF region")
    length, ptr = struct.unpack_from("<II", fw, DESC_OFF)
    if (length, ptr) != (GIF_END - GIF_START + 1, DESC_PTR):
        raise ValueError(f"descriptor at 0x{DESC_OFF:x} is ({length:#x}, {ptr:#x}), not the expected V2.1 values")
    return frames


# ----------------------------------------------------------------------------- GIF writing (own LZW encoder)
class BitWriter:
    def __init__(self):
        self.out = bytearray()
        self.acc = 0
        self.n = 0

    def write(self, code, nbits):
        self.acc |= code << self.n
        self.n += nbits
        while self.n >= 8:
            self.out.append(self.acc & 0xFF)
            self.acc >>= 8
            self.n -= 8

    def finish(self):
        if self.n:
            self.out.append(self.acc & 0xFF)
        return bytes(self.out)


def lzw_encode(data, min_code=7):
    clear, eoi = 1 << min_code, (1 << min_code) + 1
    bw = BitWriter()
    n_bits = min_code + 1
    maxcode = (1 << n_bits) - 1
    free = eoi + 1
    table = {}

    def output(code):
        nonlocal n_bits, maxcode
        bw.write(code, n_bits)
        if free > maxcode and n_bits < 12:
            n_bits += 1
            maxcode = (1 << n_bits) - 1

    bw.write(clear, n_bits)
    ent = data[0]
    for c in data[1:]:
        key = (ent << 8) | c
        nxt = table.get(key)
        if nxt is not None:
            ent = nxt
            continue
        output(ent)
        ent = c
        if free < 4096:
            table[key] = free
            free += 1
        else:
            bw.write(clear, n_bits)
            table.clear()
            free = eoi + 1
            n_bits = min_code + 1
            maxcode = (1 << n_bits) - 1
    output(ent)
    bw.write(eoi, n_bits)
    return bw.finish()


def sub_blocks(raw):
    out = bytearray()
    for i in range(0, len(raw), 255):
        chunk = raw[i:i + 255]
        out.append(len(chunk))
        out += chunk
    out.append(0)
    return bytes(out)


def write_gif(palette, frames, loop=0):
    """palette: 128x3 bytes. frames: list of (x, y, w, h, indices bytes, delay_cs)."""
    out = bytearray(b"GIF89a")
    out += struct.pack("<HHBBB", W, H, 0xE6, TRANSPARENT, 0)        # same flags as the original: 128-colour GCT
    out += bytes(palette)
    out += b"\x21\xff\x0bNETSCAPE2.0\x03\x01" + struct.pack("<H", loop) + b"\x00"
    for x, y, w, h, idx, delay in frames:
        out += b"\x21\xf9\x04" + bytes([0x05]) + struct.pack("<H", delay) + bytes([TRANSPARENT]) + b"\x00"   # disposal 1 + transparency
        out += b"\x2c" + struct.pack("<HHHHB", x, y, w, h, 0x00)
        out += b"\x07" + sub_blocks(lzw_encode(idx, 7))
    out += b"\x3b"
    return bytes(out)


# ----------------------------------------------------------------------------- input -> list of (RGB image 160x128, duration ms)
def fit(im, mode):
    im = im.convert("RGB")
    if CROP:
        im = im.crop(CROP)
    if mode == "stretch":
        return im.resize((W, H), Image.LANCZOS)
    s = max(W / im.width, H / im.height) if mode == "cover" else min(W / im.width, H / im.height)
    r = im.resize((max(1, round(im.width * s)), max(1, round(im.height * s))), Image.LANCZOS)
    canvas = Image.new("RGB", (W, H), (0, 0, 0))
    canvas.paste(r, ((W - r.width) // 2, (H - r.height) // 2))
    return canvas


def load_frames(path, mode, fps):
    p = Path(path)
    if p.is_dir():
        files = sorted(x for x in p.iterdir() if x.suffix.lower() in (".png", ".jpg", ".jpeg", ".bmp", ".webp"))
        if not files:
            raise SystemExit(f"no images in {p}")
        return [(fit(Image.open(f), mode), round(1000 / fps)) for f in files]
    if p.suffix.lower() == ".gif":
        im = Image.open(p)
        out = []
        for fr in ImageSequence.Iterator(im):
            rgba = fr.convert("RGBA")
            bg = Image.new("RGBA", rgba.size, (0, 0, 0, 255))
            bg.alpha_composite(rgba)
            out.append((fit(bg, mode), max(20, fr.info.get("duration", 40))))
        return out
    if p.suffix.lower() in (".png", ".jpg", ".jpeg", ".bmp", ".webp"):
        return [(fit(Image.open(p), mode), 2000)]
    import imageio_ffmpeg  # video
    exe = imageio_ffmpeg.get_ffmpeg_exe()
    vf = f"fps={fps}"
    if CROP:
        vf = f"crop={CROP[2] - CROP[0]}:{CROP[3] - CROP[1]}:{CROP[0]}:{CROP[1]}," + vf
    if mode == "stretch":
        vf += f",scale={W}:{H}"
    elif mode == "cover":
        vf += f",scale={W}:{H}:force_original_aspect_ratio=increase,crop={W}:{H}"
    else:
        vf += f",scale={W}:{H}:force_original_aspect_ratio=decrease,pad={W}:{H}:(ow-iw)/2:(oh-ih)/2:black"
    cmd = [exe, "-v", "error", "-i", str(p), "-vf", vf, "-f", "rawvideo", "-pix_fmt", "rgb24", "-"]
    raw = subprocess.run(cmd, capture_output=True, check=True).stdout
    n = len(raw) // (W * H * 3)
    if n == 0:
        raise SystemExit("ffmpeg produced no frames")
    return [(Image.frombuffer("RGB", (W, H), raw[i * W * H * 3:(i + 1) * W * H * 3], "raw", "RGB", 0, 1).copy(), round(1000 / fps))
            for i in range(n)]


def resample(frames, max_frames, speed):
    """Keep total duration (scaled by speed); choose at most max_frames frames evenly in time."""
    durs = [d / speed for _, d in frames]
    total = sum(durs)
    if len(frames) <= max_frames:
        return [(im, d) for (im, _), d in zip(frames, durs)]
    ends = np.cumsum(durs)
    step = total / max_frames
    out = []
    for k in range(max_frames):
        t = (k + 0.5) * step
        i = int(np.searchsorted(ends, t))
        out.append((frames[min(i, len(frames) - 1)][0], step))
    return out


# ----------------------------------------------------------------------------- quantise + diff-encode
def make_palette(images, ncolors):
    n = len(images)
    pick = images if n <= 12 else [images[int(i * (n - 1) / 11)] for i in range(12)]
    mosaic = Image.new("RGB", (W * len(pick), H))
    for i, im in enumerate(pick):
        mosaic.paste(im, (i * W, 0))
    q = mosaic.quantize(colors=ncolors, method=Image.Quantize.MEDIANCUT, dither=Image.Dither.NONE)
    pal = (q.getpalette() or [])[:3 * ncolors]
    pal += [0] * (3 * ncolors - len(pal))
    return pal


def encode_animation(frames, ncolors, extras=()):
    """extras: fixed palette colours that are always kept (e.g. for overlays); they use part of the colour budget."""
    images = [im for im, _ in frames]
    pal = make_palette(images, ncolors - len(extras))
    for c in extras:
        pal += list(c)
    pal_img = Image.new("P", (1, 1))
    pal_img.putpalette(pal + [0] * (768 - len(pal)))
    full_pal = bytes(pal + [0] * (3 * NCOL - len(pal)))               # indices >= ncolors stay black; 127 = transparent
    canvas = None
    out = []
    for im, dur in frames:
        idx = np.array(im.quantize(palette=pal_img, dither=Image.Dither.NONE), dtype=np.uint8)
        delay = max(2, round(dur / 10))
        if canvas is None:
            out.append([0, 0, W, H, idx.copy(), delay])
            canvas = idx.copy()
            continue
        diff = idx != canvas
        if not diff.any():
            out[-1][5] += delay                                          # identical frame: just hold the previous one longer
            continue
        ys, xs = np.where(diff)
        x0, x1, y0, y1 = xs.min(), xs.max() + 1, ys.min(), ys.max() + 1
        sub = np.where(diff[y0:y1, x0:x1], idx[y0:y1, x0:x1], TRANSPARENT).astype(np.uint8)
        out.append([int(x0), int(y0), int(x1 - x0), int(y1 - y0), sub, delay])
        canvas = idx.copy()
    cap = (GIF_END - GIF_START)
    gif = write_gif(full_pal, [(x, y, w, h, bytes(a.tobytes()), d) for x, y, w, h, a, d in out])
    return gif, len(out)


def fit_to_budget(frames, max_colors, max_frames, verbose=True, extras=()):
    cap = GIF_END - GIF_START
    colour_steps = [c for c in (127, 95, 63, 47, 31, 15) if c <= max_colors] or [max_colors]
    for keep in (1.0, 0.8, 0.6, 0.45, 0.3):
        mf = max(2, int(max_frames * keep))
        fr = resample(frames, mf, 1.0) if len(frames) > mf else [(im, d) for im, d in frames]
        for nc in colour_steps:
            gif, n = encode_animation(fr, nc, extras)
            if verbose:
                print(f"  try: {len(fr)} source frames, {nc} colours -> {len(gif)} bytes ({n} encoded frames); budget {cap}")
            if len(gif) <= cap:
                return gif, n, nc, len(fr)
    raise SystemExit("could not fit the animation into the firmware's space even at 15 colours and 30% of the frames; use a shorter or simpler input")


# ----------------------------------------------------------------------------- timeline + loading bar overlay
LOAD_MESSAGES = [(0.30, "LOADING GOOD BOY"), (0.60, "FETCHING TREATS"), (0.90, "WAGGING TAIL"), (1.01, "ALMOST WOOF")]
BAR_EXTRAS = [(255, 255, 255), (0, 0, 0), (60, 255, 110), (45, 45, 45)]      # white, black, bar green, bar track


def extend_timeline(frames, total_ms, hold_ms):
    """Loop the source frames until total_ms - hold_ms is filled."""
    target = total_ms - hold_ms
    out, t, i = [], 0.0, 0
    while t < target - 1:
        im, d = frames[i % len(frames)]
        out.append((im, min(d, target - t)))
        t += out[-1][1]
        i += 1
    return out


def overlay_loading(im, p, final=False):
    arr = np.array(im.convert("RGB"), dtype=np.float32)
    arr[96:, :, :] *= 0.40                                              # darken a band at the bottom for legibility
    im = Image.fromarray(arr.astype(np.uint8))
    d = ImageDraw.Draw(im)
    try:
        font = ImageFont.load_default(size=10)
    except TypeError:
        font = ImageFont.load_default()
    msg = "GOOD BOY READY! 100%" if final else next(m for lim, m in LOAD_MESSAGES if p < lim) + f" {int(p * 100)}%"
    d.text((W / 2, 104), msg, font=font, fill=(255, 255, 255), anchor="mm", stroke_width=1, stroke_fill=(0, 0, 0))
    d.rectangle((10, 112, 149, 123), outline=(255, 255, 255), fill=(45, 45, 45))
    fw = int(round(136 * p))
    if fw > 0:
        d.rectangle((12, 114, 12 + fw - 1, 121), fill=(60, 255, 110))
    return im


def add_loading_bar(frames, hold_ms):
    total = sum(d for _, d in frames)
    out, t = [], 0.0
    for im, d in frames:
        out.append((overlay_loading(im, min(t / total, 0.99)), d))
        t += d
    out.append((overlay_loading(frames[-1][0], 1.0, final=True), hold_ms))   # final frame: 100%, held
    return out


# ----------------------------------------------------------------------------- commands
def encode_gif(input_path, *, fit_mode="contain", fps=25.0, speed=1.0, colors=127, max_frames=59, crop=None,
               duration=None, hold=0.5, loading_bar=False):
    """Convert a GIF / video / image / image folder into the GIF the firmware expects. -> (gif bytes, info dict).
    Needs no firmware. Raises ValueError for unusable input."""
    global CROP
    CROP = tuple(crop) if crop else None
    try:
        frames = load_frames(input_path, fit_mode, fps)
        if speed != 1.0:
            frames = [(im, d / speed) for im, d in frames]
        hold_ms = round(hold * 1000) if loading_bar else 0
        if duration:
            frames = extend_timeline(frames, duration * 1000, hold_ms)
        if loading_bar:
            frames = add_loading_bar(frames, hold_ms)
        gif, n_enc, ncol, n_src = fit_to_budget(frames, min(colors, 127), max_frames, verbose=False,
                                                extras=BAR_EXTRAS if loading_bar else ())
    except SystemExit as e:                         # the helpers report user errors with SystemExit
        raise ValueError(str(e)) from None
    finally:
        CROP = None
    dec = Image.open(io.BytesIO(gif))
    if dec.n_frames != n_enc or dec.size != (W, H):
        raise RuntimeError("internal error: the encoder produced an unexpected animation")
    cap = GIF_END - GIF_START
    return gif, {"frames": n_enc, "colors": ncol, "bytes": len(gif), "capacity": cap,
                 "ms": round(sum(d for _, d in frames))}


@guarded
def install(original: bytes, gif: bytes) -> bytes:
    """Write an encoded GIF into the stock firmware's boot-animation region."""
    if not frmw.parse(original)["ok"]:
        raise ValueError("firmware fails the container checks")
    try:
        check_layout(original)
    except ValueError as e:
        raise ValueError(f"not the V2.1 firmware: {e}") from None
    cap = GIF_END - GIF_START
    if len(gif) > cap:
        raise ValueError("the animation is too large for the firmware")
    data = bytearray(original)
    data[GIF_START:GIF_END] = gif + bytes(cap - len(gif))
    struct.pack_into("<I", data, DESC_OFF, len(gif) + 1)
    data[0x0E] = sum(data[frmw.HEADER_SIZE:]) & 0xFF
    if not frmw.parse(bytes(data))["ok"]:
        raise RuntimeError("internal error: output fails the container checks")
    return bytes(data)


def apply(original: bytes, input_path, **options):
    """encode_gif + install. -> (new firmware bytes, gif bytes, info dict)."""
    gif, info = encode_gif(input_path, **options)
    return install(original, gif), gif, info
