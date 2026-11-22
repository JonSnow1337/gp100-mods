"""Feature-flag block and the fine-MASTER switch (flag bit 1)."""
from gp100mods import features


def test_fine_master_flag_is_a_separate_bit_and_round_trips():
    assert features.FINE_MASTER_VOLUME != features.STARTER_PRESET
    both = features.STARTER_PRESET | features.FINE_MASTER_VOLUME
    assert features.parse_block(features.build_block(both)) == both
    assert features.parse_block(features.build_block(features.FINE_MASTER_VOLUME)) & features.STARTER_PRESET == 0


def test_supports_fine_master_needs_the_marker_before_the_flag_block():
    assert not features.supports_fine_master(bytes(features.REL + 64))
    sec0 = bytearray(features.REL + 64)
    sec0[100:100 + 19] = b"GP100-FINE-MASTER-1"
    assert features.supports_fine_master(bytes(sec0))
