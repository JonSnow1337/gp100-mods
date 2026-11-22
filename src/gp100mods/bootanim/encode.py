"""Turn a GIF / APNG / WebP / video / still image / folder of images into a GPBA animation. Pure Python (Pillow; video
needs the bundled ffmpeg from imageio-ffmpeg) - no compiler or external tools."""
from __future__ import annotations

import io
from dataclasses import dataclass
from pathlib import Path

from PIL import Image, ImageSequence

from . import format as F

IMAGE_EXT = {".gif", ".png", ".apng", ".webp", ".jpg", ".jpeg", ".bmp"}
VIDEO_EXT = {".mp4", ".mov", ".mkv", ".webm", ".avi", ".m4v"}


@dataclass
class Options:
    fit: str = "contain"            # contain | cover | stretch
    background: tuple = (0, 0, 0)
    fps: float = 20.0               # sampling rate for videos / image folders
    speed: float = 1.0
    max_ms: int = F.MAX_TOTAL_MS
    colors: int = 128               # 2..256
    dither: bool = False
    crop: tuple | None = None       # (x0, y0, x1, y1) in source pixels
    still_ms: int = 2000            # duration of a single still image
    min_frame_ms: int = 20


@dataclass
class Report:
    frames: int
    colors: int
    total_ms: int
    bytes_used: int
    notes: list


def _composite(im: Image.Image, bg) -> Image.Image:
    im = im.convert("RGBA")
    base = Image.new("RGBA", im.size, tuple(bg) + (255,))
    return Image.alpha_composite(base, im).convert("RGB")


def load_media(path, opt: Options):
    """-> list of (PIL RGB image, duration_ms) at source resolution."""
    p = Path(path)
    out = []
    if p.is_dir():
        files = sorted(f for f in p.iterdir() if f.suffix.lower() in IMAGE_EXT)
        if not files:
            raise ValueError("folder contains no images")
        for f in files:
            with Image.open(f) as im:
                out.append((_composite(im, opt.background), 1000.0 / opt.fps))
    elif p.suffix.lower() in VIDEO_EXT:
        import imageio_ffmpeg
        gen = imageio_ffmpeg.read_frames(str(p), pix_fmt="rgb24", fps=opt.fps)
        meta = next(gen)
        w, h = meta["size"]
        for raw in gen:
            out.append((Image.frombytes("RGB", (w, h), raw), 1000.0 / opt.fps))
            if len(out) * 1000.0 / opt.fps > opt.max_ms * max(opt.speed, 1.0) + 1000:
                break
    else:
        with Image.open(p) as im:
            n = getattr(im, "n_frames", 1)
            if n <= 1:
                out.append((_composite(im, opt.background), float(opt.still_ms)))
            else:
                for fr in ImageSequence.Iterator(im):
                    dur = fr.info.get("duration", 40) or 40
                    out.append((_composite(fr, opt.background), float(max(dur, opt.min_frame_ms))))
    if not out:
        raise ValueError("no frames could be read")
    return out


def fit_frame(im: Image.Image, opt: Options) -> Image.Image:
    if opt.crop:
        im = im.crop(opt.crop)
    W, H = F.WIDTH, F.HEIGHT
    if opt.fit == "stretch":
        return im.resize((W, H), Image.LANCZOS)
    scale = (max if opt.fit == "cover" else min)(W / im.width, H / im.height)
    nw, nh = max(1, round(im.width * scale)), max(1, round(im.height * scale))
    r = im.resize((nw, nh), Image.LANCZOS)
    canvas = Image.new("RGB", (W, H), tuple(opt.background))
    canvas.paste(r, ((W - nw) // 2, (H - nh) // 2))
    return canvas


def _timeline(frames, opt: Options):
    frames = [(im, max(opt.min_frame_ms, d / opt.speed)) for im, d in frames]
    out, t = [], 0.0
    for im, d in frames:                      # trim to max length
        if t >= opt.max_ms:
            break
        d = min(d, opt.max_ms - t)
        out.append((im, d)); t += d
    return out


def _quantize(images, colors, dither):
    # shared palette from a mosaic of (up to 24) evenly spaced frames
    step = max(1, len(images) // 24)
    sample = images[::step][:24]
    mosaic = Image.new("RGB", (F.WIDTH, F.HEIGHT * len(sample)))
    for i, im in enumerate(sample):
        mosaic.paste(im, (0, i * F.HEIGHT))
    pal_img = mosaic.quantize(colors=colors, method=Image.Quantize.MEDIANCUT, dither=Image.Dither.NONE)
    raw = pal_img.getpalette()[:3 * colors]
    pal565 = [F.rgb888_to_565(raw[i], raw[i + 1], raw[i + 2]) for i in range(0, len(raw), 3)]
    # re-snap the palette to what the panel can show, and map frames onto it
    snapped = Image.new("P", (1, 1))
    n = len(pal565)
    flat = []
    for c in pal565:
        flat += list(F.rgb565_to_888(c))
    # Pillow considers all 256 palette slots: pad with copies of a real colour and fold those indices back below
    flat += list(F.rgb565_to_888(pal565[-1])) * (256 - n)
    snapped.putpalette(flat)
    fold = bytes(min(i, n - 1) for i in range(256))
    mode = Image.Dither.FLOYDSTEINBERG if dither else Image.Dither.NONE
    idx = [bytes(im.quantize(palette=snapped, dither=mode).tobytes()).translate(fold) for im in images]
    return pal565, idx


def _used_bytes(anim: F.Animation) -> int:
    return len(F.build_area(anim, check_size=False))


def encode(path, opt: Options | None = None, progress=None):
    """-> (Animation, Report). Reduces frames/colours automatically until it fits the 1 MiB area."""
    opt = opt or Options()
    say = progress or (lambda *_: None)
    notes = []
    say("reading", 0.0)
    src = load_media(path, opt)
    say("scaling", 0.2)
    timeline = _timeline([(fit_frame(im, opt), d) for im, d in src], opt)
    colors = max(2, min(256, opt.colors))
    stride = 1
    while True:
        picked = timeline[::stride]
        if stride > 1:                                   # keep total time: give dropped frames' time to the kept ones
            durs = []
            for k in range(0, len(timeline), stride):
                durs.append(sum(d for _, d in timeline[k:k + stride]))
            picked = [(timeline[k][0], durs[i]) for i, k in enumerate(range(0, len(timeline), stride))]
        say("quantising", 0.5)
        pal, idx = _quantize([im for im, _ in picked], colors, opt.dither)
        anim = F.Animation(palette=pal, frames=[(b, max(1, round(d))) for b, (_, d) in zip(idx, picked)])
        used = _used_bytes(anim)
        if used <= F.AREA_BYTES:
            break
        if colors > 32 and stride == 1:
            colors = max(32, colors // 2); notes.append(f"too large: reduced to {colors} colours")
        else:
            stride += 1; notes.append(f"too large: keeping every {stride}th frame")
        if stride > 40:
            raise ValueError("this animation cannot be made small enough; use a shorter or simpler source")
    say("done", 1.0)
    return anim, Report(len(anim.frames), len(anim.palette), anim.total_ms, used, notes)


def render_frame(anim: F.Animation, i: int) -> Image.Image:
    idx, _ = anim.frames[i]
    pal = [F.rgb565_to_888(c) for c in anim.palette]
    im = Image.new("RGB", (F.WIDTH, F.HEIGHT))
    im.putdata([pal[v] for v in idx])
    return im


def preview_gif(anim: F.Animation) -> bytes:
    frames = [render_frame(anim, i) for i in range(len(anim.frames))]
    buf = io.BytesIO()
    frames[0].save(buf, format="GIF", save_all=True, append_images=frames[1:],
                   duration=[d for _, d in anim.frames], loop=0)
    return buf.getvalue()
