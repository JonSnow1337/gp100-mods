"""Small reusable pieces: cards, the pedal-screen preview, form rows."""
from __future__ import annotations

import tempfile
from pathlib import Path

from PySide6.QtCore import QSize, Qt
from PySide6.QtGui import QMovie
from PySide6.QtWidgets import QFrame, QHBoxLayout, QLabel, QVBoxLayout, QWidget

from gp100mods.bootanim import format as F


def label(text="", role=None, muted=False, wrap=True) -> QLabel:
    lb = QLabel(text)
    lb.setWordWrap(wrap)
    if role:
        lb.setProperty("role", role)
    if muted:
        lb.setProperty("muted", True)
    return lb


class Card(QFrame):
    """A rounded panel with an optional small-caps title; add content with .body (a QVBoxLayout)."""

    def __init__(self, title: str | None = None):
        super().__init__()
        self.setObjectName("card")
        outer = QVBoxLayout(self)
        outer.setContentsMargins(16, 14, 16, 16)
        outer.setSpacing(10)
        if title:
            t = QLabel(title.upper())
            t.setObjectName("cardTitle")
            outer.addWidget(t)
        self.body = QVBoxLayout()
        self.body.setSpacing(10)
        outer.addLayout(self.body)

    def set_warn(self, on: bool):
        self.setProperty("warn", on)
        self.style().unpolish(self)
        self.style().polish(self)


def form_row(text: str, widget: QWidget, width: int = 120) -> QWidget:
    row = QWidget()
    lay = QHBoxLayout(row)
    lay.setContentsMargins(0, 0, 0, 0)
    lay.setSpacing(12)
    lb = QLabel(text)
    lb.setProperty("muted", True)
    lb.setFixedWidth(width)
    lay.addWidget(lb)
    lay.addWidget(widget, 1)
    return row


class PreviewScreen(QLabel):
    """The animation at 2x inside a dark bezel, like the pedal's display."""

    def __init__(self):
        super().__init__()
        self.setFixedSize(F.WIDTH * 2 + 28, F.HEIGHT * 2 + 28)
        self.setAlignment(Qt.AlignCenter)
        self.setStyleSheet("background:#0e0e10; border:2px solid #3a3b41; border-radius:10px; color:#6d6f78;")
        self.movie = None
        self._tmp = None
        self.show_text("No animation")

    def show_text(self, text):
        self._stop()
        self.setText(text)

    def _stop(self):
        if self.movie:
            self.movie.stop()
            self.movie.deleteLater()
            self.movie = None

    def play(self, gif_bytes: bytes):
        self._stop()
        if self._tmp is None:
            self._tmp = tempfile.TemporaryDirectory(prefix="gp100mods-")
        path = Path(self._tmp.name) / "preview.gif"
        path.write_bytes(gif_bytes)
        self.movie = QMovie(str(path))
        self.movie.setScaledSize(QSize(F.WIDTH * 2, F.HEIGHT * 2))
        self.setMovie(self.movie)
        self.movie.start()
