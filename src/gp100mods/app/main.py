"""GP-100 Firmware Builder: OpenGP100 (bundled new firmware) or the original Valeton firmware with a few mods."""
from __future__ import annotations

import os
import sys
import tempfile
from pathlib import Path

from PySide6.QtCore import QSettings, QThreadPool, QTimer, Qt
from PySide6.QtWidgets import (QApplication, QButtonGroup, QFileDialog, QHBoxLayout, QMainWindow, QMessageBox, QPushButton,
                               QStackedWidget, QVBoxLayout, QWidget)

from gp100mods import __version__, valeton
from gp100mods.bundle import load_manifest

from . import theme
from .pages import OpenGP100Page, StockPage
from .widgets import Card, label

_ARGV: list = []


class MainWindow(QMainWindow):
    def __init__(self, firmwares=None, valeton_dir=None, profile=valeton.V21):
        super().__init__()
        self.valeton_dir, self.profile = valeton_dir, profile
        self.setWindowTitle("GP-100 Firmware Builder")
        self.setAcceptDrops(True)
        self.settings = QSettings("gp100-mods", "firmware-builder")
        self.firmwares = load_manifest() if firmwares is None else firmwares
        self.valeton_parts = valeton.load(self.valeton_dir, self.profile)

        root = QWidget()
        root.setObjectName("root")
        self.setCentralWidget(root)
        outer = QHBoxLayout(root)
        outer.setContentsMargins(0, 0, 0, 0)
        outer.setSpacing(0)

        # ---- sidebar
        side = QWidget()
        side.setObjectName("sidebar")
        side.setFixedWidth(250)
        sv = QVBoxLayout(side)
        sv.setContentsMargins(14, 18, 14, 14)
        sv.setSpacing(6)
        brand = label("GP-100 Firmware Builder", wrap=False)
        brand.setObjectName("brand")
        sv.addWidget(brand)
        sv.addSpacing(14)
        self.nav = QButtonGroup(self)
        self.nav.setExclusive(True)
        self.nav_buttons = []
        for text in ("OpenGP100", "Original firmware + mods"):
            b = QPushButton(text)
            b.setCheckable(True)
            b.setProperty("nav", True)
            self.nav.addButton(b)
            sv.addWidget(b)
            self.nav_buttons.append(b)
        sv.addStretch(1)
        self.status = label("", role="small")
        sv.addWidget(self.status)
        self.change = QPushButton("Change…")
        self.change.setProperty("link", True)
        self.change.clicked.connect(self.pick_valeton)
        sv.addWidget(self.change)
        sv.addWidget(label(f"Version {__version__}", role="small"))
        outer.addWidget(side)

        # ---- content
        content = QWidget()
        cv = QVBoxLayout(content)
        cv.setContentsMargins(28, 24, 24, 24)
        cv.setSpacing(16)
        self.banner = Card("Valeton firmware needed (once)")
        self.banner.set_warn(True)
        self.banner_text = label("", role="small")
        self.banner.body.addWidget(self.banner_text)
        self.banner_btn = QPushButton("Select the official Valeton firmware file…")
        self.banner_btn.setProperty("primary", True)
        self.banner_btn.clicked.connect(self.pick_valeton)
        self.banner.body.addWidget(self.banner_btn)
        cv.addWidget(self.banner)
        self.stack = QStackedWidget()
        self.og = OpenGP100Page(self.settings, lambda: self.valeton_parts, self.firmwares)
        self.stock = StockPage(self.settings, lambda: self.valeton_parts)
        self.stack.addWidget(self.og)
        self.stack.addWidget(self.stock)
        cv.addWidget(self.stack, 1)
        outer.addWidget(content, 1)

        for i, b in enumerate(self.nav_buttons):
            b.clicked.connect(lambda _=False, i=i: self.show_page(i))
        self.show_page(int(self.settings.value("page", 0)))
        self._refresh_valeton()
        self.resize(1080, 780)
        self.setMinimumSize(1040, 680)

    def show_page(self, i):
        i = 0 if i not in (0, 1) else i
        self.stack.setCurrentIndex(i)
        self.nav_buttons[i].setChecked(True)
        self.settings.setValue("page", i)

    # ---- the user's own official Valeton firmware
    def _refresh_valeton(self):
        ok = self.valeton_parts is not None
        self.banner.setVisible(not ok)
        self.change.setVisible(ok)
        if ok:
            self.status.setText(f"✔ {self.profile.name} loaded")
        else:
            self.status.setText("Official firmware not loaded yet")
            self.banner_text.setText(
                f"To protect your pedal, this app only builds on top of the unmodified official {self.profile.name} update file. "
                "Select your copy once (the official GP-100 app can download it). The app checks it and refuses anything else, "
                "such as other versions or modified files. Valeton's files are never included in this app, and your copy stays "
                "on this computer.")
        self.og.refresh()
        self.stock.refresh()

    def pick_valeton(self):
        p, _ = QFileDialog.getOpenFileName(self, f"Official {self.profile.name}", self.settings.value("valeton_dir", ""),
                                           "Firmware (*.bin);;All files (*)")
        if p:
            self.set_valeton(p)

    def set_valeton(self, path):
        try:
            self.valeton_parts = valeton.import_official(path, self.valeton_dir, self.profile)
        except valeton.NotOfficialError as e:
            QMessageBox.warning(self, "Wrong firmware file", str(e))
            return
        except OSError as e:
            QMessageBox.warning(self, "Could not read the file", str(e))
            return
        self.settings.setValue("valeton_dir", str(Path(path).parent))
        self._refresh_valeton()

    # ---- drag and drop: a .bin is the official firmware, anything else is an animation for the visible page
    def dragEnterEvent(self, e):
        if e.mimeData().hasUrls():
            e.acceptProposedAction()

    def dropEvent(self, e):
        urls = e.mimeData().urls()
        if not urls:
            return
        p = urls[0].toLocalFile()
        if p.lower().endswith(".bin"):
            self.set_valeton(p)
        else:
            self.stack.currentWidget().media.set_media(p)


def run(argv=None):
    global _ARGV
    argv = list(argv or sys.argv)
    selftest = "--selftest" in argv
    _ARGV = [a for a in argv if a != "--selftest"]        # Qt keeps pointers into argv: it must outlive the app
    app = QApplication(_ARGV)
    theme.apply(app, Path(tempfile.mkdtemp(prefix="gp100mods-theme-")))
    w = MainWindow()
    w.show()
    if selftest:                                       # used by the packaging CI: window builds, encoder imports, exit 0
        import imageio_ffmpeg
        imageio_ffmpeg.get_ffmpeg_exe()
        for fw in w.firmwares:                         # every bundled firmware must be present and match its checksum
            fw.load()
        print(f"selftest ok: {len(w.firmwares)} bundled firmware(s): " + ", ".join(f.id for f in w.firmwares))
        if not w.firmwares and os.environ.get("GP100_REQUIRE_FIRMWARE"):
            return 3
        QTimer.singleShot(300, app.quit)
    code = app.exec()
    # Tear down in Qt's order: window (and its movies/timers) first, the QApplication last.
    QThreadPool.globalInstance().waitForDone()
    w.close()
    w.deleteLater()
    app.processEvents()
    del w
    return code


if __name__ == "__main__":
    sys.exit(run())
