import pytest

from gp100mods.bootanim import format as F

PIX = F.WIDTH * F.HEIGHT


def anim(frames, ncol=4):
    return F.Animation(palette=list(range(ncol)), frames=frames)


def test_roundtrip_raw_and_rle():
    noisy = bytes((i * 7 + (i >> 3)) % 4 for i in range(PIX))        # incompressible-ish -> raw
    flat = bytes([2]) * PIX                                            # RLE
    a = anim([(noisy, 40), (flat, 120)])
    b = F.parse_area(F.build_area(a))
    assert b.frames == a.frames and b.palette == a.palette


def test_rle_runs_longer_than_255():
    data = bytes([3]) * 1000 + bytes([1]) * (PIX - 1000)
    assert F.rle_decode(F.rle_encode(data), PIX) == data


def test_rle_rejects_wrong_length():
    with pytest.raises(ValueError):
        F.rle_decode(F.rle_encode(bytes(10)), PIX)


def test_empty_area_means_no_animation():
    assert F.parse_area(F.empty_area()).frames == []


@pytest.mark.parametrize("bad", ["index", "length", "size"])
def test_build_rejects_invalid(bad):
    if bad == "index":
        a = anim([(bytes([9]) * PIX, 40)], ncol=4)
    elif bad == "length":
        a = anim([(bytes(PIX), 6000), (bytes(PIX), 6000)])
    else:
        a = anim([(bytes(10), 40)])
    with pytest.raises(ValueError):
        F.build_area(a)


def test_checksum_detects_corruption():
    b = bytearray(F.build_area(anim([(bytes([1]) * PIX, 40)])))
    b[F.HEADER_BYTES + 3] ^= 0xFF
    with pytest.raises(ValueError):
        F.parse_area(bytes(b))


def test_rgb565_roundtrip_is_stable():
    for c in (0x0000, 0xFFFF, 0xF800, 0x07E0, 0x001F, 0x1234):
        r, g, b = F.rgb565_to_888(c)
        assert F.rgb888_to_565(r, g, b) == c


def test_feature_block_roundtrip_and_damage():
    from gp100mods import features as FT
    assert FT.parse_block(FT.build_block(0)) == 0 and FT.parse_block(FT.build_block(1)) == 1
    assert FT.parse_block(b"\xff" * 0x40) is None                            # erased flash = no block
    b = bytearray(FT.build_block(1)); b[13] ^= 0x01
    assert FT.parse_block(bytes(b)) is None                                  # damaged complement
    assert len(FT.build_block(5)) == FT.BLOCK_BYTES and FT.REL == 0x14BF00
