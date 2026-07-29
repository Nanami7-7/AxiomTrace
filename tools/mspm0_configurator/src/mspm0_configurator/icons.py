"""Inline SVG icon system for MSPM0 Control Studio.

Replaces Unicode character icons with crisp, platform-independent SVG
rendering via ``QSvgRenderer``.  All icons share a 24×24 viewBox and
use a ``{COLOR}`` placeholder for runtime colour injection.

Design conventions:
- Stroke-based icons: ``stroke-width="1.5"`` ``stroke-linecap="round"``
  ``stroke-linejoin="round"`` ``fill="none"``
- Fill-based icons (play, pause, dots): ``fill="{COLOR}"`` ``stroke="none"``
- viewBox: ``0 0 24 24``
"""
from __future__ import annotations

from PySide6.QtCore import QByteArray, QRectF, Qt
from PySide6.QtGui import QIcon, QPainter, QPixmap
from PySide6.QtSvg import QSvgRenderer

# ---------------------------------------------------------------------------
# SVG templates
# ---------------------------------------------------------------------------

_SVG_TEMPLATES: dict[str, str] = {
    # ── Tab icons ────────────────────────────────────────────────────
    "motor": (
        '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" '
        'stroke="{COLOR}" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round">'
        '<path d="M19.4 15a1.65 1.65 0 00.33 1.82l.06.06a2 2 0 11-2.83 2.83l-.06-.06a1.65 1.65 0 00-1.82-.33 1.65 1.65 0 00-1 1.51V21a2 2 0 11-4 0v-.09A1.65 1.65 0 009 19.4a1.65 1.65 0 00-1.82.33l-.06.06a2 2 0 11-2.83-2.83l.06-.06a1.65 1.65 0 00.33-1.82 1.65 1.65 0 00-1.51-1H3a2 2 0 110-4h.09A1.65 1.65 0 004.6 9a1.65 1.65 0 00-.33-1.82l-.06-.06a2 2 0 112.83-2.83l.06.06a1.65 1.65 0 001.82.33H9a1.65 1.65 0 001-1.51V3a2 2 0 114 0v.09a1.65 1.65 0 001 1.51 1.65 1.65 0 001.82-.33l.06-.06a2 2 0 112.83 2.83l-.06.06a1.65 1.65 0 00-.33 1.82V9a1.65 1.65 0 001.51 1H21a2 2 0 110 4h-.09a1.65 1.65 0 00-1.51 1z"/>'
        '<circle cx="12" cy="12" r="3"/>'
        '</svg>'
    ),
    "motion": (
        '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" '
        'stroke="{COLOR}" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round">'
        '<path d="M3 12h4l3-7 3 14 3-7h4"/>'
        '</svg>'
    ),
    "device": (
        '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" '
        'stroke="{COLOR}" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round">'
        '<rect x="5" y="5" width="14" height="14" rx="2"/>'
        '<path d="M9 1v3M15 1v3M9 20v3M15 20v3M1 9h3M1 15h3M20 9h3M20 15h3"/>'
        '<rect x="9" y="9" width="6" height="6" rx="1"/>'
        '</svg>'
    ),

    # ── Telemetry toolbar icons ──────────────────────────────────────
    "stream_on": (
        '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" '
        'fill="{COLOR}" stroke="none">'
        '<path d="M6 4l14 8-14 8V4z"/>'
        '</svg>'
    ),
    "stream_off": (
        '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" '
        'fill="{COLOR}" stroke="none">'
        '<rect x="6" y="4" width="4" height="16" rx="1"/>'
        '<rect x="14" y="4" width="4" height="16" rx="1"/>'
        '</svg>'
    ),
    "pause": (
        '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" '
        'fill="{COLOR}" stroke="none">'
        '<rect x="6" y="6" width="4" height="12" rx="1"/>'
        '<rect x="14" y="6" width="4" height="12" rx="1"/>'
        '</svg>'
    ),
    "clear": (
        '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" '
        'stroke="{COLOR}" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round">'
        '<path d="M6 6l12 12M18 6L6 18"/>'
        '</svg>'
    ),
    "export": (
        '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" '
        'stroke="{COLOR}" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round">'
        '<path d="M12 3v12M7 10l5 5 5-5M5 21h14"/>'
        '</svg>'
    ),
    "focus": (
        '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" '
        'stroke="{COLOR}" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round">'
        '<path d="M4 9V5a1 1 0 011-1h4M20 9V5a1 1 0 00-1-1h-4M4 15v4a1 1 0 001 1h4M20 15v4a1 1 0 01-1 1h-4"/>'
        '</svg>'
    ),
    "restore": (
        '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" '
        'stroke="{COLOR}" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round">'
        '<path d="M9 4v4a1 1 0 01-1 1H4M15 4v4a1 1 0 001 1h4M9 20v-4a1 1 0 00-1-1H4M15 20v-4a1 1 0 011-1h4"/>'
        '</svg>'
    ),

    # ── Sidebar rail buttons ─────────────────────────────────────────
    "collapse_left": (
        '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" '
        'stroke="{COLOR}" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round">'
        '<path d="M15 6l-6 6 6 6"/>'
        '</svg>'
    ),
    "collapse_right": (
        '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" '
        'stroke="{COLOR}" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round">'
        '<path d="M9 6l6 6-6 6"/>'
        '</svg>'
    ),
    "expand_left": (
        '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" '
        'stroke="{COLOR}" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round">'
        '<path d="M9 6l6 6-6 6"/>'
        '</svg>'
    ),
    "expand_right": (
        '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" '
        'stroke="{COLOR}" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round">'
        '<path d="M15 6l-6 6 6 6"/>'
        '</svg>'
    ),

    # ── Action button icons ──────────────────────────────────────────
    "refresh": (
        '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" '
        'stroke="{COLOR}" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round">'
        '<path d="M21 12a9 9 0 11-3-6.7L21 8M21 3v5h-5"/>'
        '</svg>'
    ),
    "save": (
        '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" '
        'stroke="{COLOR}" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round">'
        '<path d="M19 21H5a2 2 0 01-2-2V5a2 2 0 012-2h11l5 5v11a2 2 0 01-2 2z"/>'
        '<path d="M17 21v-8H7v8M7 3v5h8"/>'
        '</svg>'
    ),
    "load": (
        '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" '
        'stroke="{COLOR}" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round">'
        '<path d="M12 21V3M7 8l5-5 5 5M5 21h14"/>'
        '</svg>'
    ),
    "send": (
        '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" '
        'stroke="{COLOR}" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round">'
        '<path d="M22 2L11 13M22 2l-7 20-4-9-9-4 20-7z"/>'
        '</svg>'
    ),

    # ── Status indicators ────────────────────────────────────────────
    "dot_online": (
        '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" '
        'fill="{COLOR}" stroke="none">'
        '<circle cx="12" cy="12" r="6"/>'
        '</svg>'
    ),
    "dot_offline": (
        '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" '
        'stroke="{COLOR}" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round">'
        '<circle cx="12" cy="12" r="5"/>'
        '</svg>'
    ),
    "warning": (
        '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" '
        'stroke="{COLOR}" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round">'
        '<path d="M12 2L2 20h20L12 2z"/>'
        '<path d="M12 9v5M12 17h.01"/>'
        '</svg>'
    ),

    # ── Action button icons (apply / run / stop / connect / disconnect) ──
    "apply": (
        '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" '
        'stroke="{COLOR}" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round">'
        '<path d="M5 12l5 5L20 7"/>'
        '</svg>'
    ),
    "run": (
        '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" '
        'fill="{COLOR}" stroke="none">'
        '<path d="M6 4l14 8-14 8V4z"/>'
        '</svg>'
    ),
    "stop": (
        '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" '
        'fill="{COLOR}" stroke="none">'
        '<rect x="6" y="6" width="12" height="12" rx="1"/>'
        '</svg>'
    ),
    "connect": (
        '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" '
        'stroke="{COLOR}" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round">'
        '<path d="M9 12l2 2 4-4"/>'
        '<path d="M21 12a9 9 0 11-6.7-8.7L21 5"/>'
        '</svg>'
    ),
    "disconnect": (
        '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" '
        'stroke="{COLOR}" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round">'
        '<path d="M21 12a9 9 0 00-6.7-8.7L21 5"/>'
        '<path d="M3 12a9 9 0 016.7 8.7L3 19"/>'
        '<path d="M15 9l-6 6M9 9l6 6"/>'
        '</svg>'
    ),

    # ── Emergency stop (octagon) ─────────────────────────────────────
    "emergency_stop": (
        '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" '
        'stroke="{COLOR}" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">'
        '<path d="M8 2h8l6 6v8l-6 6H8l-6-6V8l6-6z"/>'
        '<path d="M9.5 9.5l5 5M14.5 9.5l-5 5"/>'
        '</svg>'
    ),
}


# ---------------------------------------------------------------------------
# Rendering helpers
# ---------------------------------------------------------------------------

def _render_svg(svg_str: str, size: int = 24, color: str = "#94A3B8") -> QIcon:
    """Render an SVG template string to a ``QIcon`` at the given size."""
    svg_colored = svg_str.replace("{COLOR}", color)
    renderer = QSvgRenderer(QByteArray(svg_colored.encode("utf-8")))
    pixmap = QPixmap(size, size)
    pixmap.fill(Qt.transparent)
    painter = QPainter(pixmap)
    painter.setRenderHint(QPainter.Antialiasing)
    renderer.render(painter, QRectF(0, 0, size, size))
    painter.end()
    return QIcon(pixmap)


def icon(name: str, size: int = 24, color: str = "#94A3B8") -> QIcon:
    """Return a ``QIcon`` by name.  Returns a null icon if not found."""
    svg = _SVG_TEMPLATES.get(name)
    if svg is None:
        return QIcon()
    return _render_svg(svg, size, color)


def pixmap(name: str, size: int = 24, color: str = "#94A3B8") -> QPixmap:
    """Return a ``QPixmap`` by name.  Returns a null pixmap if not found."""
    svg = _SVG_TEMPLATES.get(name)
    if svg is None:
        return QPixmap()
    svg_colored = svg.replace("{COLOR}", color)
    renderer = QSvgRenderer(QByteArray(svg_colored.encode("utf-8")))
    pm = QPixmap(size, size)
    pm.fill(Qt.transparent)
    painter = QPainter(pm)
    painter.setRenderHint(QPainter.Antialiasing)
    renderer.render(painter, QRectF(0, 0, size, size))
    painter.end()
    return pm
