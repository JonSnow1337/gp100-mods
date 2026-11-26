"""The two modes: OpenGP100 (bundled new firmware) and Original firmware + mods (patches to Valeton's V2.1)."""
from __future__ import annotations

from pathlib import Path

from PySide6.QtCore import QSettings, QThreadPool, QTimer, Qt, Signal
from PySide6.QtGui import QColor
from PySide6.QtWidgets import (QCheckBox, QColorDialog, QComboBox, QDoubleSpinBox, QFileDialog, QHBoxLayout, QLineEdit,
                               QMessageBox, QProgressBar, QPushButton, QScrollArea, QSlider, QVBoxLayout, QWidget)

from gp100mods import drums, features, frmw, presets as presets_mod, update_file
from gp100mods.bundle import latest
from gp100mods.defaults import DOGBOOT_GIF, dogboot_bytes
from gp100mods.bootanim import encode as E
from gp100mods.bootanim import format as F
from gp100mods.stock import about_text, boot_gif, master_volume, wah_in_mod

from .widgets import Card, PreviewScreen, form_row, label
from .workers import Job

MEDIA_FILTER = ("Animations, videos and images (*.gif *.apng *.webp *.png *.jpg *.jpeg *.bmp "
                "*.mp4 *.mov *.mkv *.webm *.avi *.m4v);;All files (*)")
INSTALL_HELP = ("1. Click Generate and save the file.\n"
                "2. Connect the GP-100 by USB and open the official GP-100 app.\n"
                "3. Use its firmware update and pick the saved file. Every firmware update erases your presets - back them up first.\n\n"
                "If something goes wrong: unplug the power (keep USB), hold BOTH footswitches and the PARA knob, open the GP-100 app, "
                "then plug the power back in while holding. It will let you install a firmware file again.")


# --------------------------------------------------------------------------------------------- preview column
class PreviewPanel(Card):
    def __init__(self):
        super().__init__()
        self.screen = PreviewScreen()
        self.body.addWidget(self.screen, 0, Qt.AlignHCenter)
        self.progress = QProgressBar()
        self.progress.setRange(0, 100)
        self.progress.setTextVisible(False)
        self.body.addWidget(self.progress)
        self.info = label("", role="small")
        self.info.setMinimumHeight(34)
        self.body.addWidget(self.info)
        self.space = QProgressBar()
        self.space.setRange(0, 1000)
        self.space.setTextVisible(False)
        self.body.addWidget(self.space)

    def reset(self, text):
        self.screen.show_text(text)
        self.progress.setValue(0)
        self.space.setValue(0)
        self.info.setText("")


# --------------------------------------------------------------------------------------------- animation picker
class MediaCard(Card):
    """What plays at boot: the default dog boot, the user's own GIF/video/image, or the original logo.
    kind 'gpba' = OpenGP100's own format; kind 'gif' = the GIF stored inside the original firmware."""
    stateChanged = Signal()
    previewReady = Signal(bytes)
    previewCleared = Signal()
    infoChanged = Signal(str, int)        # text, permille of the space used (-1 = none)
    progressChanged = Signal(int)
    failed = Signal(str)

    MODES = (("Dog boot with loading bar (default)", "default"), ("My own animation", "custom"), ("Original logo", "original"))

    def __init__(self, kind: str, settings: QSettings):
        super().__init__("Boot animation")
        self.kind, self.settings = kind, settings
        self.mode = "default"
        self.media = None
        self.result = None                # converted custom animation
        self.default_result = None        # the bundled default, converted once at start-up
        self.encoding = False
        self.job_id = 0
        self.bg = QColor(0, 0, 0)
        self.debounce = QTimer(self, singleShot=True, interval=350)
        self.debounce.timeout.connect(self.start_encode)

        self.mode_box = QComboBox()
        for text, key in self.MODES:
            self.mode_box.addItem(text, key)
        # Re-laying-out the page while the popup is still closing can leave the popup stuck open on some desktops,
        # so a user's pick is handled on the next event-loop tick (set_mode() below stays synchronous).
        self.mode_box.activated.connect(lambda _i: (self.mode_box.hidePopup(), QTimer.singleShot(0, self._mode_changed)))
        self.body.addWidget(self.mode_box)
        self.hint = label("", role="small")
        self.body.addWidget(self.hint)

        self.custom = QWidget()
        cv = QVBoxLayout(self.custom)
        cv.setContentsMargins(0, 0, 0, 0)
        cv.setSpacing(10)
        row = QHBoxLayout()
        b1 = QPushButton("Choose GIF, video or image…")
        b1.clicked.connect(self.pick_media)
        b2 = QPushButton("Folder…")
        b2.clicked.connect(self.pick_folder)
        row.addWidget(b1, 3)
        row.addWidget(b2, 1)
        cv.addLayout(row)
        self.media_label = label("No file chosen yet. You can also drop one onto the window.", muted=True)
        cv.addWidget(self.media_label)

        self.look = QWidget()
        lv = QVBoxLayout(self.look)
        lv.setContentsMargins(0, 4, 0, 0)
        lv.setSpacing(10)
        self.fit = QComboBox()
        self.fit.addItem("Fit inside (bars)", "contain")
        self.fit.addItem("Fill the screen (crop)", "cover")
        self.fit.addItem("Stretch", "stretch")
        self.fps = QDoubleSpinBox(); self.fps.setRange(2, 30); self.fps.setValue(15); self.fps.setSuffix(" fps")
        self.speed = QDoubleSpinBox(); self.speed.setRange(0.25, 4); self.speed.setSingleStep(0.25); self.speed.setValue(1.0)
        self.maxs = QDoubleSpinBox(); self.maxs.setRange(0.5, 10); self.maxs.setValue(5.0); self.maxs.setSuffix(" s")
        self.colors = QSlider(Qt.Horizontal)
        self.colors.setRange(8, 256 if kind == "gpba" else 127)
        self.colors.setValue(128 if kind == "gpba" else 127)
        self.colors_label = label(str(self.colors.value()), muted=True, wrap=False)
        self.colors_label.setFixedWidth(30)
        crow = QWidget(); cl = QHBoxLayout(crow); cl.setContentsMargins(0, 0, 0, 0)
        cl.addWidget(self.colors); cl.addWidget(self.colors_label)
        self.dither = QCheckBox("Smooth gradients (dithering)")
        self.bgbtn = QPushButton()
        self.bgbtn.clicked.connect(self.pick_bg)
        self._paint_bg()
        lv.addWidget(form_row("Fit", self.fit))
        if kind == "gpba":
            lv.addWidget(form_row("Bar colour", self.bgbtn))
        lv.addWidget(form_row("Frame rate", self.fps))
        lv.addWidget(form_row("Speed", self.speed))
        if kind == "gpba":
            lv.addWidget(form_row("Max length", self.maxs))
        lv.addWidget(form_row("Colours", crow))
        if kind == "gpba":
            lv.addWidget(self.dither)
        else:
            lv.addWidget(label("The original firmware has limited space for the animation; long or detailed ones are "
                               "reduced automatically.", role="small"))
        self.look.setEnabled(False)
        cv.addWidget(self.look)
        self.body.addWidget(self.custom)

        for w in (self.fps, self.speed, self.maxs):
            w.valueChanged.connect(self.changed)
        self.fit.currentIndexChanged.connect(self.changed)
        self.colors.valueChanged.connect(lambda v: (self.colors_label.setText(str(v)), self.changed()))
        self.dither.toggled.connect(self.changed)
        self._mode_changed()
        QTimer.singleShot(0, self.start_default)

    # ---- inputs
    def _paint_bg(self):
        self.bgbtn.setText(self.bg.name().upper())
        self.bgbtn.setStyleSheet(f"background:{self.bg.name()}; color:{'#000' if self.bg.lightness() > 128 else '#fff'};")

    def pick_media(self):
        p, _ = QFileDialog.getOpenFileName(self, "Animation", self.settings.value("media_dir", ""), MEDIA_FILTER)
        if p:
            self.set_media(p)

    def pick_folder(self):
        p = QFileDialog.getExistingDirectory(self, "Folder of images (played in name order)")
        if p:
            self.set_media(p)

    def pick_bg(self):
        c = QColorDialog.getColor(self.bg, self, "Bar colour")
        if c.isValid():
            self.bg = c
            self._paint_bg()
            self.changed()

    def set_mode(self, mode):
        self.mode_box.setCurrentIndex(self.mode_box.findData(mode))
        self._mode_changed()

    def set_media(self, path):
        """Use the user's own file (switches to 'My own animation')."""
        self.media = path
        self.media_label.setText(Path(path).name)
        self.media_label.setProperty("muted", False)
        self.media_label.style().polish(self.media_label)
        self.settings.setValue("media_dir", str(Path(path).parent))
        self.look.setEnabled(True)
        self.result = None
        self.set_mode("custom")
        self.start_encode()

    def changed(self):
        if self.media:
            self.debounce.start()

    def _mode_changed(self):
        self.mode = self.mode_box.currentData()
        self.custom.setVisible(self.mode == "custom")
        self.hint.setText({"default": "Our dog boot animation with a loading bar. This is what you get unless you pick something else.",
                           "custom": "", "original": "Keeps the logo that comes with the firmware."}[self.mode])
        self.hint.setVisible(bool(self.hint.text()))
        self.publish()

    # ---- what is currently selected
    @property
    def active_result(self):
        if self.mode == "default":
            return self.default_result
        if self.mode == "custom" and self.media:
            return self.result
        return None

    @property
    def ready(self) -> bool:
        if self.mode == "original":
            return True
        if self.mode == "default":
            return self.default_result is not None
        return not self.encoding and self.media is not None and self.result is not None

    @property
    def uses_animation(self) -> bool:
        return self.mode != "original"

    @property
    def animation(self):                               # gpba
        r = self.active_result
        return r["anim"] if r and self.kind == "gpba" else None

    @property
    def gif(self):                                     # stock
        r = self.active_result
        return r["gif"] if r and self.kind == "gif" else None

    def empty_text(self):
        return {"original": "Original logo\n(no custom animation)", "default": "Loading…"}.get(self.mode, "Choose an animation")

    # ---- show the selection in the preview column
    def _text_for(self, res):
        if self.kind == "gpba":
            rep = res["report"]
            note = ("\n" + "; ".join(rep.notes)) if rep.notes else ""
            return (f"{rep.frames} frames · {rep.colors} colours · {rep.total_ms / 1000:.2f} s\n"
                    f"{rep.bytes_used / 1024:.0f} KB of {F.AREA_BYTES // 1024} KB used{note}",
                    int(1000 * rep.bytes_used / F.AREA_BYTES))
        i = res["info"]
        colours = f" · {i['colors']} colours" if i.get("colors") else ""
        return (f"{i['frames']} frames{colours} · {i['ms'] / 1000:.1f} s\n"
                f"{i['bytes'] / 1024:.0f} KB of {i['capacity'] / 1024:.0f} KB used", int(1000 * i["bytes"] / i["capacity"]))

    def publish(self):
        res = self.active_result
        if res:
            text, permille = self._text_for(res)
            self.progressChanged.emit(100)
            self.previewReady.emit(res["gif"])
            self.infoChanged.emit(text, permille)
        else:
            self.progressChanged.emit(0)
            self.previewCleared.emit()
            busy = (self.mode == "default" and self.default_result is None) or (self.mode == "custom" and self.encoding)
            self.infoChanged.emit("Converting…" if busy else "", 0 if busy else -1)
        self.stateChanged.emit()

    # ---- conversion (background)
    def _work_for(self, path, custom: bool):
        if self.kind == "gpba":
            if custom:
                opts = E.Options(fit=self.fit.currentData(), background=(self.bg.red(), self.bg.green(), self.bg.blue()),
                                 fps=self.fps.value(), speed=self.speed.value(), max_ms=int(self.maxs.value() * 1000),
                                 colors=self.colors.value(), dither=self.dither.isChecked())
            else:
                opts = E.Options(fit="contain", colors=128, dither=False)

            def work(progress):
                anim, report = E.encode(path, opts, progress=progress)
                return {"anim": anim, "report": report, "gif": E.preview_gif(anim)}
        elif custom:
            kw = dict(fit_mode=self.fit.currentData(), fps=self.fps.value(), speed=self.speed.value(), colors=self.colors.value())

            def work(progress):
                progress("converting", 0.3)
                gif, info = boot_gif.encode_gif(path, **kw)
                return {"gif": gif, "info": info}
        else:
            def work(progress):                       # the default is already in the firmware's own format: use it as is
                import io
                from PIL import Image, ImageSequence
                gif = dogboot_bytes()
                im = Image.open(io.BytesIO(gif))
                ms = sum(f.info.get("duration", 0) for f in ImageSequence.Iterator(im))
                return {"gif": gif, "info": {"frames": im.n_frames, "colors": None, "bytes": len(gif),
                                             "capacity": boot_gif.GIF_END - boot_gif.GIF_START, "ms": ms}}
        return work

    def _start(self, work, job_id, done):
        job = Job(job_id, work)
        job.signals.done.connect(done)
        job.signals.failed.connect(self._failed)
        job.signals.progress.connect(lambda i, m, p: self.progressChanged.emit(int(p * 100)) if (i == self.job_id and self.mode == "custom") else None)
        QThreadPool.globalInstance().start(job)
        return job

    def start_default(self):
        self._default_job = self._start(self._work_for(str(DOGBOOT_GIF), False), -1, self._default_done)
        self.publish()

    def _default_done(self, job_id, res):
        self.default_result = res
        if self.mode == "default":
            self.publish()
        else:
            self.stateChanged.emit()

    def start_encode(self):
        if not self.media:
            return
        self.job_id += 1
        self.result = None
        self.encoding = True
        self._job = self._start(self._work_for(self.media, True), self.job_id, self._done)
        self.progressChanged.emit(5)
        self.publish()

    def _done(self, job_id, res):
        if job_id != self.job_id:
            return
        self.encoding = False
        self.result = res
        self.publish()

    def _failed(self, job_id, msg):
        if job_id != self.job_id and job_id != -1:
            return
        if job_id == self.job_id:
            self.result = None
            self.encoding = False
        self.failed.emit(msg)
        self.stateChanged.emit()


# --------------------------------------------------------------------------------------------- page base
class BasePage(QWidget):
    def __init__(self, title, subtitle, settings: QSettings, get_parts):
        super().__init__()
        self.settings, self.get_parts = settings, get_parts
        outer = QHBoxLayout(self)
        outer.setContentsMargins(0, 0, 0, 0)
        outer.setSpacing(20)

        left = QWidget()
        self.left = QVBoxLayout(left)
        self.left.setContentsMargins(0, 0, 8, 0)
        self.left.setSpacing(14)
        self.left.addWidget(label(title, role="h1"))
        sub = label(subtitle, muted=True)
        self.left.addWidget(sub)
        scroll = QScrollArea()
        scroll.setWidgetResizable(True)
        scroll.setWidget(left)
        scroll.setHorizontalScrollBarPolicy(Qt.ScrollBarAlwaysOff)
        outer.addWidget(scroll, 1)

        right = QWidget()
        right.setFixedWidth(F.WIDTH * 2 + 28 + 32 + 4)
        rv = QVBoxLayout(right)
        rv.setContentsMargins(0, 0, 0, 0)
        rv.setSpacing(14)
        self.panel = PreviewPanel()
        self.screen = self.panel.screen
        rv.addWidget(self.panel)
        self.generate = QPushButton("Generate firmware")
        self.generate.setProperty("primary", True)
        self.generate.clicked.connect(self.generate_file)
        rv.addWidget(self.generate)
        rv.addWidget(label("Presets on the pedal are erased by every firmware update.", role="small"))
        rv.addStretch(1)
        outer.addWidget(right, 0)

    def wire_media(self, media: MediaCard):
        self.media = media
        media.previewReady.connect(self.screen.play)
        media.previewCleared.connect(lambda: self.panel.reset(self.media.empty_text()))
        media.infoChanged.connect(self._info)
        media.progressChanged.connect(self.panel.progress.setValue)
        media.failed.connect(self._media_failed)
        media.stateChanged.connect(self.refresh)

    def finish(self):
        """Last step of a page: the install instructions, then push everything up."""
        help_card = Card("How to install")
        help_card.body.addWidget(label(INSTALL_HELP, role="small"))
        self.left.addWidget(help_card)
        self.left.addStretch(1)

    def _info(self, text, permille):
        self.panel.info.setText(text)
        if permille >= 0:
            self.panel.space.setValue(permille)

    def _media_failed(self, msg):
        self.panel.progress.setValue(0)
        self.screen.show_text("Could not convert")
        self.panel.info.setText(msg)

    def refresh(self):
        raise NotImplementedError

    def generate_file(self):
        raise NotImplementedError

    def save_dialog(self, default_name):
        start = str(Path(self.settings.value("out_dir", str(Path.home()))) / default_name)
        dst, _ = QFileDialog.getSaveFileName(self, "Save firmware", start, "Firmware (*.bin)")
        if dst:
            self.settings.setValue("out_dir", str(Path(dst).parent))
        return dst

    def done_message(self, dst):
        QMessageBox.information(self, "Firmware ready", f"Saved {Path(dst).name}.\n\nNow install it with the official GP-100 "
                                "app (see the notes on the right). Presets are erased by every update.")


# --------------------------------------------------------------------------------------------- mode 1: OpenGP100
class OpenGP100Page(BasePage):
    def __init__(self, settings, get_parts, firmwares):
        super().__init__("OpenGP100", "The new community firmware, installed with the official GP-100 app.", settings, get_parts)
        self.firmwares = firmwares
        self.feature_boxes = {}
        self.presets = []                 # converted presets (presets.Preset), slot P01 first
        self.preset_files = []
        c1 = Card("Firmware")                              # always the newest bundled build, so there is no version picker
        self.notes = label("", role="small")
        c1.body.addWidget(self.notes)
        self.left.addWidget(c1)

        self.options_card = Card("Options")
        self.left.addWidget(self.options_card)

        self.fine_master = QCheckBox("Finer MASTER volume steps (quieter low end)")
        self.fine_master.setChecked(True)
        self.fine_master_note = label("Fixes MASTER being too loud: the quiet end of the knob gets much finer steps, the new 20 is "
                                      "about as loud as the old 2. The loudest setting is unchanged.", role="small")
        self.options_card.body.addWidget(self.fine_master)
        self.options_card.body.addWidget(self.fine_master_note)

        self.new_drums = QCheckBox("New drum sounds")
        self.new_drums.setChecked(True)
        self.new_drums_note = label("A new set of drum sounds (CrocellKit); the rhythms stay Valeton's. Untick to keep the original "
                                    "Valeton drum sounds. Credit: drum sounds from CrocellKit by Lars Muldjord (CC BY 4.0).",
                                    role="small")
        self.options_card.body.addWidget(self.new_drums)
        self.options_card.body.addWidget(self.new_drums_note)

        self.presets_card = Card("Presets")
        self.presets_card.body.addWidget(label(
            "Add presets from the Valeton editor (.prst files). They are converted to OpenGP100 presets and installed in "
            "slots P01, P02, ... in the order you add them (up to 99). A slot shows its installed preset until you save "
            "something else there; a factory reset brings them back. With none added, every slot starts empty.", role="small"))
        row = QWidget()
        rl = QHBoxLayout(row)
        rl.setContentsMargins(0, 0, 0, 0)
        self.add_presets_btn = QPushButton("Add preset files...")
        self.add_presets_btn.clicked.connect(self.choose_preset_files)
        self.clear_presets_btn = QPushButton("Clear")
        self.clear_presets_btn.clicked.connect(self.clear_presets)
        rl.addWidget(self.add_presets_btn)
        rl.addWidget(self.clear_presets_btn)
        rl.addStretch(1)
        self.presets_card.body.addWidget(row)
        self.presets_info = label("", role="small")
        self.presets_card.body.addWidget(self.presets_info)
        self.left.addWidget(self.presets_card)
        self._update_presets_info()

        self.wire_media(MediaCard("gpba", settings))
        self.left.addWidget(self.media)
        self.finish()
        self._fill_versions()

    # -- versions + switches
    def _fill_versions(self):
        self.current = latest(self.firmwares)
        if not self.current:
            self.notes.setText("No firmware is installed in this copy of the app. Reinstall the app.")
            self.options_card.hide()
            return
        self._version_changed()

    def current_firmware(self):
        return self.current

    def _version_changed(self):
        fw = self.current_firmware()
        if not fw:
            return
        self.notes.setText(f"{fw.label}  (v{fw.version})" + (f" - {fw.notes}" if fw.notes else ""))
        has_drums = "drum samples" in {f.name for f in fw.fills}     # only builds that have the drum-sample region
        self.new_drums.setEnabled(has_drums)
        self.new_drums.setVisible(has_drums)
        self.new_drums_note.setVisible(has_drums)
        while self.options_card.body.count() > 4:           # keep the fixed checkboxes (fine MASTER, new drums) and their notes
            it = self.options_card.body.takeAt(4)
            if it.widget():
                it.widget().deleteLater()
        has_fine = features.supports_fine_master(fw.load())
        self.fine_master.setVisible(has_fine)
        self.fine_master_note.setVisible(has_fine)
        self.feature_boxes = {}
        for ft in fw.features:
            cb = QCheckBox(ft.label)
            cb.setChecked(ft.default)
            self.options_card.body.addWidget(cb)
            if ft.description:
                self.options_card.body.addWidget(label(ft.description, role="small"))
            self.feature_boxes[ft.id] = cb
        self.options_card.setVisible(bool(fw.features) or has_fine)
        self.presets_card.setVisible(features.supports_presets(fw.load()))
        self.refresh()

    # -- presets
    def choose_preset_files(self):
        start = self.settings.value("preset_dir", str(Path.home()))
        paths, _ = QFileDialog.getOpenFileNames(self, "Choose preset files", start, "Valeton presets (*.prst);;All files (*)")
        if paths:
            self.settings.setValue("preset_dir", str(Path(paths[0]).parent))
            self.add_preset_files(paths)

    def add_preset_files(self, paths):
        """Convert .prst files and append their presets. Returns the error text (empty if all went well)."""
        errors, added = [], []
        for path in paths:
            try:
                new = presets_mod.load_prst(path)
            except presets_mod.PresetError as e:
                errors.append(str(e))
                continue
            room = presets_mod.MAX_PRESETS - len(self.presets)
            if len(new) > room:
                errors.append(f"{Path(path).name}: only {room} more preset(s) fit; the rest were skipped")
                new = new[:room]
            self.presets += new
            added += new
            self.preset_files.append(Path(path).name)
        self._update_presets_info()
        if errors:
            QMessageBox.warning(self, "Some presets were not added", "\n".join(errors))
        notes = [f"{p.name}: {w}" for p in added for w in p.warnings]
        if notes:                                          # tell the user right away what the conversion had to change
            box = QMessageBox(QMessageBox.Warning, "Some presets were changed",
                              f"{len(notes)} change(s) were needed to make your presets work on the pedal. "
                              "They will sound slightly different from the originals. See Details.", QMessageBox.Ok, self)
            box.setDetailedText("\n".join(notes))
            box.exec()
        return "\n".join(errors)

    def clear_presets(self):
        self.presets, self.preset_files = [], []
        self._update_presets_info()

    def _update_presets_info(self):
        self.clear_presets_btn.setEnabled(bool(self.presets))
        if not self.presets:
            self.presets_info.setText("No presets added.")
            return
        notes = [f"{p.name}: {w}" for p in self.presets for w in p.warnings]
        text = f"{len(self.presets)} preset(s) from {', '.join(self.preset_files)}: P01 {self.presets[0].name}" \
               + (f" ... P{len(self.presets):02d} {self.presets[-1].name}" if len(self.presets) > 1 else "")
        if notes:
            text += f"\n\n{len(notes)} adjustment(s):\n" + "\n".join(notes)
        self.presets_info.setText(text)

    # -- state
    def refresh(self):
        self.generate.setEnabled(bool(self.current_firmware()) and self.get_parts() is not None and self.media.ready)

    def selected_patches(self):
        fw = self.current_firmware()
        return [p for ft in fw.features if self.feature_boxes.get(ft.id) and self.feature_boxes[ft.id].isChecked()
                for p in ft.patches]

    def build_bytes(self) -> bytes:
        fw = self.current_firmware()
        recs = [p.record for p in self.presets] if features.supports_presets(fw.load()) else ()
        sec0 = fw.load()
        flags = None
        if features.supports_fine_master(sec0) and features.supports(sec0):
            flags = features.read(sec0) or 0
            flags = (flags | features.FINE_MASTER_VOLUME) if self.fine_master.isChecked() else (flags & ~features.FINE_MASTER_VOLUME)
        kit = drums.load_kit() if self.new_drums.isEnabled() and self.new_drums.isChecked() else None
        return update_file.compose(sec0, self.get_parts(), self.media.animation, self.selected_patches(),
                                   flags=flags, fills=fw.fills, presets=recs, drums=kit)

    def default_name(self):
        return f"GP100_{self.current_firmware().id}{'_animation' if self.media.uses_animation else ''}.bin"

    def generate_file(self):
        dst = self.save_dialog(self.default_name())
        if not dst:
            return
        try:
            Path(dst).write_bytes(self.build_bytes())
            check = update_file.inspect(dst)
            if not (check.ok and check.supports_animation):
                raise RuntimeError("the generated file failed its self-check: " + check.message)
        except Exception as e:                            # noqa: BLE001
            QMessageBox.critical(self, "Could not generate the firmware", str(e))
            return
        self.done_message(dst)


# --------------------------------------------------------------------------------------------- mode 2: original firmware + mods
class StockPage(BasePage):
    def __init__(self, settings, get_parts):
        super().__init__("Original firmware + mods", "Valeton's own V2.1 firmware with a few changes. Pick what you want.",
                         settings, get_parts)
        c = Card("Changes")
        self.wah = QCheckBox("V-Wah and C-Wah in the modulation slot")
        c.body.addWidget(self.wah)
        c.body.addWidget(label("Adds the two pedal wahs to the MOD slot (replacing Sine and Triangle Trem), so you can run a wah "
                               "and a compressor together. The PC editor still shows the old names.", role="small"))
        self.volume = QCheckBox("Finer MASTER volume steps")
        c.body.addWidget(self.volume)
        c.body.addWidget(label("Makes the quiet end of the MASTER knob much finer: the new 20 is about as loud as the old 2. "
                               "The loudest setting is unchanged.", role="small"))
        self.about = QCheckBox("Custom text on the About screen")
        c.body.addWidget(self.about)
        self.about_fields = QWidget()
        af = QVBoxLayout(self.about_fields)
        af.setContentsMargins(28, 0, 0, 0)
        af.setSpacing(8)
        self.lines = []
        for key, default in (("line1", "Modded firmware"), ("line2", "gp100-mods"), ("line3", "My build")):
            e = QLineEdit()
            e.setMaxLength(about_text.LIMITS[key])
            e.setText(default[:about_text.LIMITS[key]])
            e.setPlaceholderText(f"up to {about_text.LIMITS[key]} characters")
            self.lines.append(e)
            af.addWidget(e)
        af.addWidget(label("Letters, digits and punctuation only. The version number is no longer shown.", role="small"))
        self.about_fields.setVisible(False)
        self.about.toggled.connect(self.about_fields.setVisible)
        c.body.addWidget(self.about_fields)
        self.left.addWidget(c)

        self.wire_media(MediaCard("gif", settings))
        self.left.addWidget(self.media)
        self.finish()

    def refresh(self):
        self.generate.setEnabled(self.get_parts() is not None and self.media.ready)

    def build_bytes(self) -> bytes:
        data = self.get_parts().file
        if self.wah.isChecked():
            data = wah_in_mod.apply(data)
        if self.media.gif:
            data = boot_gif.install(data, self.media.gif)
        if self.about.isChecked():
            data = about_text.apply(data, *[e.text() for e in self.lines])
        if self.volume.isChecked():
            data = master_volume.apply(data)
        return data

    def default_name(self):
        return "GP100_V2.1_mod.bin"

    def generate_file(self):
        dst = self.save_dialog(self.default_name())
        if not dst:
            return
        try:
            out = self.build_bytes()
            if not frmw.parse(out)["ok"]:
                raise RuntimeError("the generated file failed its self-check")
            Path(dst).write_bytes(out)
        except Exception as e:                            # noqa: BLE001
            QMessageBox.critical(self, "Could not generate the firmware", str(e))
            return
        self.done_message(dst)
