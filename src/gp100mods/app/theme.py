"""One dark theme for the whole app (a QSS stylesheet). Small icons are drawn at start-up, so no image files ship."""
from pathlib import Path

from PySide6.QtCore import QPointF, Qt
from PySide6.QtGui import QColor, QPainter, QPalette, QPen, QPixmap

C = dict(bg="#313338", card="#2b2d31", sidebar="#1e1f22", input="#1e1f22", text="#dbdee1", muted="#949ba4",
         accent="#4f8cff", accent_hover="#6a9fff", border="#3f4147", button="#404249", button_hover="#4a4d55",
         warn="#e0a93b", danger="#f0646a", ok="#57c27d")


def make_assets(directory: Path) -> dict:
    """Draw the check mark and the combo-box arrow once; returns {name: url usable inside QSS}."""
    directory.mkdir(parents=True, exist_ok=True)
    out = {}

    def paint(name, draw, size=16):
        pm = QPixmap(size, size)
        pm.fill(Qt.transparent)
        p = QPainter(pm)
        p.setRenderHint(QPainter.Antialiasing)
        draw(p)
        p.end()
        path = directory / f"{name}.png"
        pm.save(str(path))
        out[name] = path.as_posix()

    def check(p):
        p.setPen(QPen(QColor("white"), 2.2, Qt.SolidLine, Qt.RoundCap, Qt.RoundJoin))
        p.drawPolyline([QPointF(4, 8.5), QPointF(7, 11.5), QPointF(12, 5)])

    def arrow(p):
        p.setPen(QPen(QColor(C["muted"]), 2, Qt.SolidLine, Qt.RoundCap, Qt.RoundJoin))
        p.drawPolyline([QPointF(4, 6), QPointF(8, 10), QPointF(12, 6)])

    paint("check", check)
    paint("arrow", arrow)
    return out


def stylesheet(a: dict) -> str:
    return f"""
* {{ font-family: "Inter", "Segoe UI", "SF Pro Text", "Helvetica Neue", "Noto Sans", "DejaVu Sans", sans-serif; font-size: 13px; }}
QMainWindow, QWidget#root {{ background: {C['bg']}; }}
QWidget {{ color: {C['text']}; }}
QLabel {{ background: transparent; }}
QLabel[muted="true"] {{ color: {C['muted']}; }}
QLabel[role="h1"] {{ font-size: 22px; font-weight: 700; }}
QLabel[role="h2"] {{ font-size: 15px; font-weight: 600; }}
QLabel[role="small"] {{ color: {C['muted']}; font-size: 12px; }}

QWidget#sidebar {{ background: {C['sidebar']}; }}
QLabel#brand {{ font-size: 15px; font-weight: 700; }}
QPushButton[nav="true"] {{ background: transparent; border: none; border-radius: 6px; padding: 9px 12px; text-align: left; color: {C['muted']}; font-weight: 600; }}
QPushButton[nav="true"]:hover {{ background: {C['card']}; color: {C['text']}; }}
QPushButton[nav="true"]:checked {{ background: {C['button']}; color: white; }}

QFrame#card {{ background: {C['card']}; border-radius: 8px; }}
QFrame#card[warn="true"] {{ border: 1px solid {C['warn']}; }}
QLabel#cardTitle {{ font-size: 12px; font-weight: 700; color: {C['muted']}; letter-spacing: 1px; }}

QPushButton {{ background: {C['button']}; border: none; border-radius: 6px; padding: 8px 14px; color: {C['text']}; }}
QPushButton:hover {{ background: {C['button_hover']}; }}
QPushButton:disabled {{ color: #6d6f78; background: #35363c; }}
QPushButton[primary="true"] {{ background: {C['accent']}; color: white; font-weight: 700; font-size: 14px; padding: 11px 16px; }}
QPushButton[primary="true"]:hover {{ background: {C['accent_hover']}; }}
QPushButton[primary="true"]:disabled {{ background: #3a4a6b; color: #8a94ab; }}
QPushButton[link="true"] {{ background: transparent; color: {C['accent']}; padding: 2px 0; text-align: left; }}
QPushButton[link="true"]:hover {{ color: {C['accent_hover']}; }}

QComboBox, QDoubleSpinBox, QLineEdit {{ background: {C['input']}; border: 1px solid transparent; border-radius: 6px; padding: 6px 10px; min-height: 18px; selection-background-color: {C['accent']}; }}
QComboBox:hover, QDoubleSpinBox:hover, QLineEdit:hover {{ border-color: {C['border']}; }}
QComboBox:focus, QDoubleSpinBox:focus, QLineEdit:focus {{ border-color: {C['accent']}; }}
QComboBox::drop-down {{ border: none; width: 26px; }}
QComboBox::down-arrow {{ image: url({a['arrow']}); width: 16px; height: 16px; }}
QComboBox QAbstractItemView {{ background: {C['input']}; border: 1px solid {C['border']}; selection-background-color: {C['accent']}; outline: none; padding: 4px; }}
QDoubleSpinBox::up-button, QDoubleSpinBox::down-button {{ width: 0; border: none; }}

QCheckBox {{ spacing: 10px; background: transparent; }}
QCheckBox::indicator {{ width: 18px; height: 18px; border-radius: 4px; background: {C['input']}; border: 1px solid #4e5058; }}
QCheckBox::indicator:hover {{ border-color: {C['accent']}; }}
QCheckBox::indicator:checked {{ background: {C['accent']}; border-color: {C['accent']}; image: url({a['check']}); }}

QSlider::groove:horizontal {{ height: 4px; background: transparent; border-radius: 2px; }}
QSlider::sub-page:horizontal {{ background: {C['accent']}; border-radius: 2px; height: 4px; }}
QSlider::add-page:horizontal {{ background: {C['input']}; border-radius: 2px; height: 4px; }}
QSlider::handle:horizontal {{ width: 14px; height: 14px; margin: -5px 0; border-radius: 7px; background: white; }}
QSlider:disabled::sub-page:horizontal {{ background: #4a4d55; }}

QProgressBar {{ background: {C['input']}; border: none; border-radius: 3px; max-height: 6px; min-height: 6px; text-align: center; }}
QProgressBar::chunk {{ background: {C['accent']}; border-radius: 3px; }}

QScrollArea {{ background: transparent; border: none; }}
QScrollArea > QWidget > QWidget {{ background: transparent; }}
QScrollBar:vertical {{ background: transparent; width: 10px; margin: 0; }}
QScrollBar::handle:vertical {{ background: #4a4d55; border-radius: 5px; min-height: 30px; }}
QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {{ height: 0; }}
QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical {{ background: transparent; }}
QToolTip {{ background: {C['sidebar']}; color: {C['text']}; border: 1px solid {C['border']}; padding: 4px 6px; }}
QMessageBox {{ background: {C['bg']}; }}
"""


def apply(app, assets_dir: Path):
    """Fusion style + dark palette (so no widget can fall back to light grey) + the stylesheet."""
    app.setStyle("Fusion")
    pal = QPalette()
    for role, key in ((QPalette.Window, "bg"), (QPalette.Base, "input"), (QPalette.AlternateBase, "card"), (QPalette.Button, "button"),
                      (QPalette.ToolTipBase, "sidebar")):
        pal.setColor(role, QColor(C[key]))
    for role in (QPalette.WindowText, QPalette.Text, QPalette.ButtonText, QPalette.ToolTipText):
        pal.setColor(role, QColor(C["text"]))
    pal.setColor(QPalette.Highlight, QColor(C["accent"]))
    pal.setColor(QPalette.HighlightedText, QColor("white"))
    app.setPalette(pal)
    app.setStyleSheet(stylesheet(make_assets(assets_dir)))
