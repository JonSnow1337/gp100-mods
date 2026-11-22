import os
import sys
from pathlib import Path

import pytest

pytest.importorskip("PySide6")
from PIL import Image                     # noqa: E402
from PySide6.QtCore import QEventLoop, QTimer  # noqa: E402
from PySide6.QtWidgets import QApplication, QFileDialog, QMessageBox  # noqa: E402

from gp100mods import bundle, frmw, update_file as U, valeton  # noqa: E402
from gp100mods.app.main import MainWindow  # noqa: E402
from gp100mods.stock import about_text, boot_gif, master_volume, wah_in_mod  # noqa: E402

V21_FILE = os.environ.get("GP100_V21_FILE")


@pytest.fixture(scope="module")
def qapp():
    return QApplication.instance() or QApplication(sys.argv)


def wait(ms):
    loop = QEventLoop(); QTimer.singleShot(ms, loop.quit); loop.exec()


def wait_for(cond, ms=8000):
    loop = QEventLoop(); t = QTimer(); t.setInterval(50)
    t.timeout.connect(lambda: loop.quit() if cond() else None); QTimer.singleShot(ms, loop.quit); t.start(); loop.exec(); t.stop()
    return cond()


def default_ready(window):
    return wait_for(lambda: window.og.media.default_result is not None and window.stock.media.default_result is not None)


@pytest.fixture
def warnings(monkeypatch):
    seen = []
    monkeypatch.setattr(QMessageBox, "warning", staticmethod(lambda *a, **k: seen.append(a[2])))
    return seen


@pytest.fixture
def dialogs(monkeypatch, warnings):
    monkeypatch.setattr(QMessageBox, "information", staticmethod(lambda *a, **k: 0))
    monkeypatch.setattr(QMessageBox, "critical", staticmethod(lambda *a, **k: (_ for _ in ()).throw(AssertionError(a[2]))))


@pytest.fixture
def window(qapp, tmp_path, update_file, official, dialogs):
    fw = bundle.add_firmware(update_file, label="Test", version="1", directory=tmp_path / "b")
    w = MainWindow(firmwares=[fw], valeton_dir=tmp_path / "vcache", profile=official[1])
    w.settings.clear()
    w.official_path = official[0]
    w.show_page(0)
    yield w
    w.close()


def save_to(monkeypatch, path):
    monkeypatch.setattr(QFileDialog, "getSaveFileName", staticmethod(lambda *a, **k: (str(path), "")))


def make_gif(path, frames=3):
    ims = [Image.new("RGB", (64, 48), (i * 60, 0, 200)) for i in range(frames)]
    ims[0].save(path, save_all=True, append_images=ims[1:], duration=60, loop=0)


def test_default_animation_is_shown_in_the_app(window):
    assert default_ready(window)
    og = window.og
    assert og.media.mode == "default" and og.media.animation is not None and len(og.media.animation.frames) > 10
    assert og.screen.movie is not None                      # the dog boot is playing in the preview
    assert window.stock.media.gif and window.stock.screen.movie is not None
    assert "frames" in og.panel.info.text()


def test_generate_locked_until_official_file_is_given(window, tmp_path, warnings):
    default_ready(window)
    assert not window.og.generate.isEnabled() and not window.stock.generate.isEnabled()      # first run
    assert not window.banner.isHidden()
    wrong = tmp_path / "wrong.bin"; wrong.write_bytes(window.official_path.read_bytes()[:-10])
    window.set_valeton(str(wrong))
    assert warnings and "not the official" in warnings[0] and not window.og.generate.isEnabled()
    window.set_valeton(str(window.official_path))
    assert window.og.generate.isEnabled() and window.valeton_parts is not None and window.banner.isHidden()


def test_official_file_is_remembered(window, tmp_path, official):
    window.set_valeton(str(window.official_path))
    again = MainWindow(firmwares=window.firmwares, valeton_dir=tmp_path / "vcache", profile=official[1])
    assert again.valeton_parts is not None
    assert wait_for(lambda: again.og.media.default_result is not None) and again.og.generate.isEnabled()
    again.close()


def test_navigation_between_modes(window):
    window.show_page(1)
    assert window.stack.currentWidget() is window.stock and window.nav_buttons[1].isChecked()
    window.show_page(0)
    assert window.stack.currentWidget() is window.og and window.nav_buttons[0].isChecked()


def test_one_click_uses_the_default_animation(window, tmp_path, monkeypatch):
    default_ready(window)
    window.set_valeton(str(window.official_path))
    assert window.og.generate.isEnabled() and window.og.current_firmware() is not None
    out = tmp_path / "default.bin"; save_to(monkeypatch, out)
    window.og.generate_file()
    info = U.inspect(out)
    assert info.ok and info.has_custom_animation and info.frames == len(window.og.media.animation.frames)


def test_original_logo_option(window, tmp_path, monkeypatch):
    default_ready(window)
    window.set_valeton(str(window.official_path))
    window.og.media.set_mode("original")
    assert window.og.generate.isEnabled() and window.og.media.animation is None
    out = tmp_path / "plain.bin"; save_to(monkeypatch, out)
    window.og.generate_file()
    assert U.inspect(out).ok and not U.inspect(out).has_custom_animation


def test_with_animation(window, tmp_path, monkeypatch):
    gif = tmp_path / "a.gif"; make_gif(gif)
    window.set_valeton(str(window.official_path))
    window.og.media.set_media(str(gif))
    assert not window.og.generate.isEnabled()              # still converting -> not clickable
    wait(1500)
    assert window.og.generate.isEnabled() and window.og.media.animation is not None
    out = tmp_path / "anim.bin"; save_to(monkeypatch, out)
    window.og.generate_file()
    info = U.inspect(out)
    assert info.has_custom_animation and info.frames == 3
    window.og.media.set_mode("default")                     # back to the default animation
    assert window.og.generate.isEnabled() and len(window.og.media.animation.frames) > 10


def test_bad_media_blocks_generate(window, tmp_path):
    window.set_valeton(str(window.official_path))
    bad = tmp_path / "x.gif"; bad.write_bytes(b"not an image")
    window.og.media.set_media(str(bad)); wait(1000)
    assert not window.og.generate.isEnabled()               # a file that cannot be converted blocks Generate
    window.og.media.set_mode("default")
    assert window.og.generate.isEnabled()                   # the failed file no longer blocks the default


def test_empty_app_cannot_generate(qapp):
    w = MainWindow(firmwares=[])
    wait(500)
    assert not w.og.generate.isEnabled()
    w.close()


# ---- original firmware + mods: needs the real official file (the patches only work on exactly that file) ----
needs_official = pytest.mark.skipif(not (V21_FILE and os.path.isfile(V21_FILE or "")), reason="GP100_V21_FILE not set")


@pytest.fixture
def real_window(qapp, tmp_path, dialogs):
    w = MainWindow(firmwares=[], valeton_dir=tmp_path / "vreal")
    w.settings.clear()
    w.set_valeton(V21_FILE)
    w.show_page(1)
    default_ready(w)
    yield w
    w.close()


@needs_official
def test_stock_mode_all_mods_and_animation(real_window, tmp_path, monkeypatch):
    w, st = real_window, real_window.stock
    assert st.generate.isEnabled()
    gif = tmp_path / "a.gif"; make_gif(gif, 5)
    st.wah.setChecked(True); st.volume.setChecked(True); st.about.setChecked(True)
    st.lines[0].setText("Hello"); st.lines[1].setText("World"); st.lines[2].setText("Test")
    st.media.set_media(str(gif)); wait_for(lambda: st.media.result is not None, 8000)
    assert st.generate.isEnabled() and st.media.gif
    out = tmp_path / "mod.bin"; save_to(monkeypatch, out)
    st.generate_file()
    expect = master_volume.apply(about_text.apply(boot_gif.install(wah_in_mod.apply(w.valeton_parts.file), st.media.gif),
                                                  "Hello", "World", "Test"))
    assert out.read_bytes() == expect and frmw.parse(out.read_bytes())["ok"]


@needs_official
def test_stock_mode_defaults_install_the_dogboot(real_window, tmp_path, monkeypatch):
    """No box ticked, nothing chosen: the output is the official file plus the bundled dog boot - byte for byte what was flashed."""
    from gp100mods.defaults import dogboot_bytes
    out = tmp_path / "default.bin"; save_to(monkeypatch, out)
    real_window.stock.generate_file()
    assert out.read_bytes() == boot_gif.install(real_window.valeton_parts.file, dogboot_bytes())


@needs_official
def test_stock_mode_original_logo_and_nothing_else_equals_official(real_window, tmp_path, monkeypatch):
    real_window.stock.media.set_mode("original")
    out = tmp_path / "same.bin"; save_to(monkeypatch, out)
    real_window.stock.generate_file()
    assert out.read_bytes() == real_window.valeton_parts.file


@needs_official
def test_stock_mode_rejects_bad_about_text(real_window, tmp_path, monkeypatch):
    st = real_window.stock
    st.about.setChecked(True)
    st.lines[0].setMaxLength(100); st.lines[0].setText("x" * 40)          # bypass the input limit
    monkeypatch.setattr(QFileDialog, "getSaveFileName", staticmethod(lambda *a, **k: (str(tmp_path / "o.bin"), "")))
    with pytest.raises(AssertionError, match="do not fit"):               # surfaced through the error dialog
        st.generate_file()
    assert not (tmp_path / "o.bin").exists()


def test_dropdown_pick_is_handled_after_the_popup_closes(window):
    """A user's pick (the `activated` signal) must not re-lay-out the page synchronously - that left the popup stuck open."""
    default_ready(window)
    m = window.og.media
    m.mode_box.setCurrentIndex(m.mode_box.findData("original"))
    m.mode_box.activated.emit(m.mode_box.currentIndex())
    assert m.mode == "default"                              # not applied yet: the popup is still closing
    wait(100)
    assert m.mode == "original" and not m.custom.isVisible()
    m.set_mode("custom")                                    # programmatic changes are immediate
    assert m.mode == "custom"


def test_presets_are_converted_and_installed(window, tmp_path, monkeypatch):
    default_ready(window)
    window.set_valeton(str(window.official_path))
    og = window.og
    assert not og.presets_card.isHidden()                                  # the bundled firmware can carry presets
    from test_presets import make_prst
    assert og.add_preset_files([str(make_prst(tmp_path / 'two.prst', 2))]) == "" and len(og.presets) == 2
    out = tmp_path / "p.bin"; save_to(monkeypatch, out)
    og.generate_file()
    assert U.inspect(out).presets == 2
    og.clear_presets()
    out2 = tmp_path / "q.bin"; save_to(monkeypatch, out2)
    og.generate_file()
    assert U.inspect(out2).presets == 0


def test_a_bad_preset_file_is_reported_not_added(window, tmp_path, monkeypatch):
    from PySide6.QtWidgets import QMessageBox
    monkeypatch.setattr(QMessageBox, "warning", lambda *a, **k: None)
    bad = tmp_path / "bad.prst"; bad.write_text("not xml")
    assert "not a readable preset file" in window.og.add_preset_files([str(bad)]) and window.og.presets == []


def test_firmware_without_preset_support_hides_the_presets_card(qapp, tmp_path, update_file, official, dialogs):
    from gp100mods import features
    data = bytearray(update_file.read_bytes())
    old_sec0 = U.section0_of(bytes(data))
    legacy = bytearray(old_sec0).replace(b"GP100-FACTORY-PRESETS-1", b"XXXXXXXXXXXXXXXXXXXXXXX"); legacy[features.REL:features.REL + features.BLOCK_BYTES] = b"\xff" * features.BLOCK_BYTES
    d = tmp_path / "b"; d.mkdir()
    import hashlib, json
    (d / "legacy.sec0").write_bytes(bytes(legacy))
    (d / "manifest.json").write_text(json.dumps({"firmwares": [{"id": "legacy-1", "label": "Legacy", "version": "1",
        "file": "legacy.sec0", "sha256": hashlib.sha256(bytes(legacy)).hexdigest(), "features": []}]}))
    w = MainWindow(firmwares=bundle.load_manifest(d), valeton_dir=tmp_path / "v", profile=official[1])
    w.settings.clear()
    assert w.og.presets_card.isHidden() and w.og.options_card.isHidden()
    w.close()


@needs_official
def test_opengp100_fine_master_checkbox_sets_the_flag(qapp, tmp_path, dialogs, monkeypatch):
    """With the real bundled OpenGP100 build and the real official file: the box is shown, on by default, and writes flag bit 1."""
    from gp100mods import features, update_file as U
    w = MainWindow(firmwares=bundle.load_manifest(), valeton_dir=tmp_path / "vreal")
    w.settings.clear()
    w.set_valeton(V21_FILE)
    w.show_page(0)
    default_ready(w)
    page = w.og
    assert page.fine_master.isVisibleTo(page) and page.fine_master.isChecked()
    for checked in (True, False):
        page.fine_master.setChecked(checked)
        out = tmp_path / f"fm{checked}.bin"; save_to(monkeypatch, out)
        page.generate_file()
        flags = features.read(U.section0_of(out.read_bytes()))
        assert flags is not None and bool(flags & features.FINE_MASTER_VOLUME) == checked
    w.close()
