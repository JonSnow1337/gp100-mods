import random

from PIL import Image

from gp100mods.bootanim import encode as E, format as F


def make_gif(path, frames=3, size=(64, 48)):
    ims = [Image.new("RGB", size, (40 * i, 80, 255 - 40 * i)) for i in range(frames)]
    ims[0].save(path, save_all=True, append_images=ims[1:], duration=[50, 70, 90][:frames], loop=0)


def test_gif_to_animation(tmp_path):
    g = tmp_path / "a.gif"; make_gif(g)
    anim, rep = E.encode(g, E.Options(colors=16))
    assert rep.frames == 3 and anim.total_ms == 210 and len(anim.palette) <= 16
    assert F.parse_area(F.build_area(anim)).total_ms == 210


def test_still_image_uses_still_duration(tmp_path):
    p = tmp_path / "a.png"; Image.new("RGB", (100, 100), (200, 30, 30)).save(p)
    anim, _ = E.encode(p, E.Options(still_ms=1500))
    assert len(anim.frames) == 1 and anim.total_ms == 1500


def test_speed_and_max_length(tmp_path):
    g = tmp_path / "a.gif"; make_gif(g)
    assert E.encode(g, E.Options(speed=2.0))[0].total_ms == 105
    assert E.encode(g, E.Options(max_ms=100))[0].total_ms == 100


def test_fit_modes_produce_panel_size(tmp_path):
    im = Image.new("RGB", (300, 100), (255, 0, 0))
    for m in ("contain", "cover", "stretch"):
        assert E.fit_frame(im, E.Options(fit=m)).size == (F.WIDTH, F.HEIGHT)


def test_noisy_long_animation_is_reduced_to_fit(tmp_path):
    random.seed(3)
    frames = []
    for _ in range(80):
        im = Image.frombytes("RGB", (160, 128), bytes(random.randrange(256) for _ in range(160 * 128 * 3)))
        frames.append(im)
    g = tmp_path / "n.gif"
    frames[0].save(g, save_all=True, append_images=frames[1:], duration=100, loop=0)
    anim, rep = E.encode(g, E.Options(colors=256, max_ms=10_000))
    assert rep.bytes_used <= F.AREA_BYTES and rep.notes


def test_folder_of_images(tmp_path):
    for i in range(3):
        Image.new("RGB", (32, 32), (i * 80, 0, 0)).save(tmp_path / f"{i}.png")
    anim, _ = E.encode(tmp_path, E.Options(fps=10))
    assert len(anim.frames) == 3
