"""Centralised theme constants for MSPM0 Control Studio.

All colours, spacing, radii, and typography sizes live here so that the
stylesheet in ``app.py`` and the widgets in ``main_window.py`` reference a
single source of truth.  A future light-theme switch would only need to
swap the values in this module.
"""
from __future__ import annotations

# ---------------------------------------------------------------------------
# Colour palette — dark theme (only theme implemented; light reserved)
# ---------------------------------------------------------------------------

class DarkPalette:
    # Surfaces
    BG_APP          = "rgba(2, 6, 23, 244)"
    BG_WINDOW       = "rgba(2, 6, 23, 238)"
    BG_PANEL        = "rgba(15, 23, 42, 238)"
    BG_PANEL_LIGHT  = "#172033"
    BG_INPUT        = "#111C2F"
    BG_INPUT_HOVER  = "#17233A"
    BG_PRESSED      = "#0B1323"
    BG_TABLE        = "#080F1D"
    BG_TABLE_ALT    = "#0E1829"
    BG_PLOT         = "#0F172A"
    BG_PLOT_AREA    = "#0B1220"
    BG_DISABLED     = "#0B1220"

    # Borders
    BORDER_DEFAULT  = "#26364D"
    BORDER_INPUT    = "#3B4A61"
    BORDER_HOVER    = "#64748B"
    BORDER_DISABLED = "#1E293B"
    BORDER_TABLE    = "#334155"
    BORDER_GRID     = "#243247"
    BORDER_SPLITTER = "#1E293B"

    # Text
    TEXT_PRIMARY    = "#E2E8F0"
    TEXT_BRIGHT     = "#F8FAFC"
    TEXT_MUTED      = "#94A3B8"
    TEXT_SUBTLE     = "#CBD5E1"
    TEXT_DISABLED   = "#64748B"

    # Accent — primary (cyan/sky)
    ACCENT          = "#38BDF8"
    ACCENT_DIM      = "#7DD3FC"
    ACCENT_BG       = "#0369A1"
    ACCENT_BG_HOVER = "#0284C7"
    ACCENT_BORDER   = "#0EA5E9"
    ACCENT_FOCUS    = "#38BDF8"

    # Semantic
    SUCCESS         = "#4ADE80"
    SUCCESS_BG      = "#052E24"
    SUCCESS_BORDER  = "#15803D"
    SUCCESS_TEXT    = "#86EFAC"

    WARNING         = "#F59E0B"
    WARNING_BG      = "#78350F"
    WARNING_BORDER  = "#D97706"
    WARNING_TEXT    = "#FEF3C7"

    DANGER          = "#F87171"
    DANGER_BG       = "#B91C1C"
    DANGER_BG_HOVER = "#DC2626"
    DANGER_BORDER   = "#F87171"

    INFO            = "#93C5FD"
    INFO_BG         = "#172554"
    INFO_BORDER     = "#2563EB"

    SAFETY_BG       = "#2A1805"
    SAFETY_BORDER   = "#92400E"
    SAFETY_TEXT     = "#FCD34D"

    # Motor colour tags (for combo box indicators)
    MOTOR_COLORS = ("#38BDF8", "#F59E0B", "#A78BFA", "#34D399")

    # Series colours for telemetry plot
    SERIES_COLORS = (
        "#38BDF8", "#F59E0B", "#A78BFA", "#34D399",
        "#7DD3FC", "#FCD34D", "#C4B5FD", "#6EE7B7",
        "#FB7185", "#F472B6", "#E2E8F0",
    )

    # Motor RPM direction colours
    RPM_FORWARD = "#38BDF8"   # positive rotation — blue
    RPM_REVERSE = "#F59E0B"   # negative rotation — orange
    RPM_STOPPED = "#64748B"   # zero / stopped — grey

    # Connection button state colours
    CONN_DISCONNECTED = "#475569"  # grey — disconnected
    CONN_CONNECTING   = "#F59E0B"  # orange — connecting / handshake
    CONN_CONNECTED    = "#4ADE80"  # green — connected (ready)
    CONN_READY        = "#38BDF8"  # blue — fully ready

    # DTR/RTS indicator colours
    DOT_ACTIVE   = "#4ADE80"
    DOT_INACTIVE = "#475569"

    # Badge / unsaved indicator
    BADGE_UNSAVED = "#F59E0B"


# Active palette reference (swap here for light theme later)
Palette = DarkPalette

# ---------------------------------------------------------------------------
# Spacing system (px) — proportional to base font size
# ---------------------------------------------------------------------------

_BASE = 10  # base font size in pt

SPACING_XS  = 3   # 0.3x
SPACING_SM  = 5   # 0.5x
SPACING_MD  = 6   # 0.6x
SPACING_LG  = 8   # 0.8x
SPACING_XL  = 10  # 1.0x
SPACING_2XL = 12  # 1.2x
SPACING_3XL = 14  # 1.4x

# Outer margins
MARGIN_LEFT   = 16
MARGIN_TOP    = 14
MARGIN_RIGHT  = 16
MARGIN_BOTTOM = 12

# ---------------------------------------------------------------------------
# Radii (px)
# ---------------------------------------------------------------------------

RADIUS_CONTROL = 5    # buttons, inputs, combos
RADIUS_CONTAINER = 7  # group boxes, tab panes
RADIUS_PANEL = 9      # large panels
RADIUS_BADGE = 6      # status badges

# ---------------------------------------------------------------------------
# Typography (pt) — scalable with system DPI
# ---------------------------------------------------------------------------

FONT_FAMILY = '"Segoe UI Variable", "Segoe UI", "Microsoft YaHei UI", "Noto Sans CJK SC", sans-serif'
FONT_MONO = '"Cascadia Mono", "Cascadia Code", "Consolas", "Courier New", monospace'

FONT_H1 = 16   # brand title
FONT_H2 = 13   # large value (mono)
FONT_H3 = 11   # section title
FONT_BODY = 10  # default
FONT_CAPTION = 9  # muted captions

FONT_WEIGHT_BOLD = 700
FONT_WEIGHT_SEMI = 650
FONT_WEIGHT_MED = 600

# ---------------------------------------------------------------------------
# Unicode icon set (no external resources)
# ---------------------------------------------------------------------------

class Icons:
    # Sidebar / navigation
    COLLAPSE_LEFT  = "\u2190"   # ←
    EXPAND_LEFT    = "\u2192"   # →
    COLLAPSE_RIGHT = "\u2192"   # →
    EXPAND_RIGHT   = "\u2190"   # ←

    # Tab icons (prefix)
    MOTOR   = "\u2699"   # ⚙
    MOTION  = "\u27A4"   # ➤
    DEVICE  = "\u2139"   # ℹ

    # Telemetry toolbar
    STREAM_ON  = "\u25B6"   # ▶
    STREAM_OFF = "\u23F8"   # ⏸ (actually double-bar, close enough)
    PAUSE      = "\u2389"   # ⎉
    CLEAR      = "\u2715"   # ✕
    EXPORT     = "\u2B07"   # ⬇
    FOCUS      = "\u26F6"   # ⛶
    RESTORE    = "\u21A9"   # ↩

    # Motor indicators
    MOTOR_A = "\u25CF"  # ● red-ish handled by color
    MOTOR_B = "\u25CF"
    MOTOR_C = "\u25CF"
    MOTOR_D = "\u25CF"

    # Connection
    DOT_ONLINE  = "\u25CF"   # ●
    DOT_OFFLINE = "\u25CB"   # ○

    # Misc
    GEAR     = "\u2699"   # ⚙
    WARNING  = "\u26A0"   # ⚠
    SAVE     = "\u2B07"   # ⬇
    LOAD     = "\u2B06"   # ⬆
    SEND     = "\u27A4"   # ➤
    REFRESH  = "\u21BB"   # ↻


# ---------------------------------------------------------------------------
# Stylesheet builder — assembles the QSS from the palette constants
# ---------------------------------------------------------------------------

def build_stylesheet() -> str:
    p = Palette
    return f"""
QWidget {{
    background: {p.BG_APP};
    color: {p.TEXT_PRIMARY};
    font-family: {FONT_FAMILY};
    font-size: {FONT_BODY}pt;
}}
QMainWindow, QStatusBar {{ background: {p.BG_WINDOW}; }}
QLabel, QCheckBox {{ background: transparent; }}

/* --- Panels & containers --- */
QFrame#headerPanel, QGroupBox, QTabWidget::pane {{
    background: {p.BG_PANEL};
    border: 1px solid {p.BORDER_DEFAULT};
    border-radius: {RADIUS_CONTAINER}px;
}}
QGroupBox {{
    margin-top: 10px;
    padding: 10px {SPACING_MD}px {SPACING_MD}px {SPACING_MD}px;
    font-weight: {FONT_WEIGHT_MED};
}}
QGroupBox::title {{
    subcontrol-origin: margin;
    left: {SPACING_XL}px;
    padding: 0 {SPACING_SM}px;
    color: {p.TEXT_SUBTLE};
    border-bottom: 2px solid {p.ACCENT};
}}

/* --- Typography object names --- */
QLabel#brandTitle {{ font-size: {FONT_H1}pt; font-weight: {FONT_WEIGHT_BOLD}; color: {p.TEXT_BRIGHT}; }}
QLabel#sectionTitle {{ font-size: {FONT_H3}pt; font-weight: {FONT_WEIGHT_SEMI}; color: {p.TEXT_BRIGHT}; }}
QLabel#largeValue {{ font-family: {FONT_MONO}; font-size: {FONT_H2}pt; font-weight: {FONT_WEIGHT_SEMI}; }}
QLabel#largeValue[active="true"] {{ color: {p.SUCCESS}; }}
QLabel#mutedText {{ color: {p.TEXT_MUTED}; }}

/* --- Badges --- */
QLabel#deviceBadge, QLabel#stateBadge {{
    background: {p.BG_PANEL_LIGHT};
    border: 1px solid {p.BORDER_TABLE};
    border-radius: {RADIUS_BADGE}px;
    padding: 7px {SPACING_LG}px;
    color: {p.TEXT_SUBTLE};
}}
QLabel#stateBadge[state="ready"] {{
    background: {p.SUCCESS_BG}; border-color: {p.SUCCESS_BORDER}; color: {p.SUCCESS_TEXT};
}}
QLabel#stateBadge[state="connecting"], QLabel#stateBadge[state="handshake"] {{
    background: {p.INFO_BG}; border-color: {p.INFO_BORDER}; color: {p.INFO};
}}
QLabel#stateBadge[state="incompatible"] {{
    background: #450A0A; border-color: {p.DANGER}; color: #FCA5A5;
}}
QLabel#safetyBanner {{
    background: {p.SAFETY_BG}; border: 1px solid {p.SAFETY_BORDER};
    border-radius: {RADIUS_BADGE}px;
    color: {p.SAFETY_TEXT}; padding: 9px {SPACING_XL}px; font-weight: {FONT_WEIGHT_MED};
}}

/* --- Inputs & buttons --- */
QPushButton, QLineEdit, QSpinBox, QDoubleSpinBox {{
    min-height: 28px;
    padding: {SPACING_XS}px {SPACING_LG}px;
    border-radius: {RADIUS_CONTROL}px;
    border: 1px solid {p.BORDER_INPUT};
    background: {p.BG_INPUT};
    color: {p.TEXT_PRIMARY};
}}
QPushButton {{ font-weight: {FONT_WEIGHT_MED}; }}
QPushButton:hover, QLineEdit:hover, QSpinBox:hover, QDoubleSpinBox:hover {{
    border-color: {p.BORDER_HOVER};
    background: {p.BG_INPUT_HOVER};
}}
QPushButton:pressed {{ background: {p.BG_PRESSED}; }}

/* --- QComboBox (separate rules to avoid Fusion rendering bugs) --- */
QComboBox {{
    min-height: 28px;
    padding: {SPACING_XS}px {SPACING_LG}px;
    border: 1px solid {p.BORDER_INPUT};
    border-radius: {RADIUS_CONTROL}px;
    background: {p.BG_INPUT};
    color: {p.TEXT_PRIMARY};
    /* Reserve space for the drop-down button on the right */
    padding-right: 24px;
}}
QComboBox:hover {{
    border-color: {p.BORDER_HOVER};
    background: {p.BG_INPUT_HOVER};
}}
QComboBox::drop-down {{
    subcontrol-origin: padding;
    subcontrol-position: top right;
    width: 24px;
    border: none;
    border-left: 1px solid {p.BORDER_DEFAULT};
    border-top-right-radius: {RADIUS_CONTROL}px;
    border-bottom-right-radius: {RADIUS_CONTROL}px;
    background: transparent;
}}
QComboBox::drop-down:hover {{
    background: {p.BG_INPUT_HOVER};
}}
QComboBox::down-arrow {{
    image: none;
    width: 10px;
    height: 10px;
    border-left: 5px solid transparent;
    border-right: 5px solid transparent;
    border-top: 6px solid {p.TEXT_MUTED};
}}
QComboBox::down-arrow:on {{
    border-top: none;
    border-bottom: 6px solid {p.ACCENT};
}}
/* Popup list (dropdown items) */
QComboBox QAbstractItemView {{
    background: {p.BG_PANEL};
    border: 1px solid {p.BORDER_DEFAULT};
    border-radius: {RADIUS_CONTROL}px;
    padding: 4px;
    outline: 0;
    selection-background-color: {p.ACCENT_BG};
    selection-color: white;
    color: {p.TEXT_PRIMARY};
}}
QComboBox QAbstractItemView::item {{
    padding: {SPACING_XS}px {SPACING_LG}px;
    min-height: 28px;
    border-radius: {RADIUS_CONTROL}px;
}}
QComboBox QAbstractItemView::item:hover {{
    background: {p.BG_INPUT_HOVER};
}}
QComboBox QAbstractItemView::item:selected {{
    background: {p.ACCENT_BG};
    color: white;
}}
/* Read-only combo (no editable text) */
QComboBox:!editable {{
    color: {p.TEXT_PRIMARY};
}}
QComboBox:disabled {{
    color: {p.TEXT_DISABLED};
    background: {p.BG_DISABLED};
    border-color: {p.BORDER_DISABLED};
}}
/* Editable combo line edit */
QComboBox QLineEdit {{
    border: none;
    background: transparent;
    padding: 0;
    border-radius: 0;
}}
QPushButton#primaryButton {{ background: {p.ACCENT_BG}; border-color: {p.ACCENT_BORDER}; color: white; }}
QPushButton#primaryButton:hover {{ background: {p.ACCENT_BG_HOVER}; }}
QPushButton#warningButton {{ background: {p.WARNING_BG}; border-color: {p.WARNING_BORDER}; color: {p.WARNING_TEXT}; }}
QPushButton#emergencyButton {{
    background: {p.DANGER_BG};
    border: 2px solid {p.DANGER};
    color: white;
    font-size: 12pt;
    font-weight: 800;
    padding: 5px 16px;
}}
QPushButton#emergencyButton:hover {{ background: {p.DANGER_BG_HOVER}; }}
QPushButton:disabled, QComboBox:disabled, QLineEdit:disabled,
QSpinBox:disabled, QDoubleSpinBox:disabled {{
    color: {p.TEXT_DISABLED};
    background: {p.BG_DISABLED};
    border-color: {p.BORDER_DISABLED};
}}
QPushButton:focus, QLineEdit:focus, QSpinBox:focus,
QDoubleSpinBox:focus, QTabBar::tab:focus, QCheckBox:focus {{
    border: 2px solid {p.ACCENT_FOCUS};
}}
QComboBox:focus {{
    border: 2px solid {p.ACCENT_FOCUS};
}}

/* --- QSpinBox / QDoubleSpinBox buttons --- */
QSpinBox::up-button, QDoubleSpinBox::up-button {{
    subcontrol-origin: border;
    subcontrol-position: top right;
    width: 18px;
    border: none;
    border-left: 1px solid {p.BORDER_DEFAULT};
    border-top-right-radius: {RADIUS_CONTROL}px;
    background: transparent;
}}
QSpinBox::down-button, QDoubleSpinBox::down-button {{
    subcontrol-origin: border;
    subcontrol-position: bottom right;
    width: 18px;
    border: none;
    border-left: 1px solid {p.BORDER_DEFAULT};
    border-bottom-right-radius: {RADIUS_CONTROL}px;
    background: transparent;
}}
QSpinBox::up-button:hover, QDoubleSpinBox::up-button:hover,
QSpinBox::down-button:hover, QDoubleSpinBox::down-button:hover {{
    background: {p.BG_INPUT_HOVER};
}}
QSpinBox::up-arrow, QDoubleSpinBox::up-arrow {{
    image: none;
    width: 8px;
    height: 8px;
    border-left: 4px solid transparent;
    border-right: 4px solid transparent;
    border-bottom: 5px solid {p.TEXT_MUTED};
}}
QSpinBox::down-arrow, QDoubleSpinBox::down-arrow {{
    image: none;
    width: 8px;
    height: 8px;
    border-left: 4px solid transparent;
    border-right: 4px solid transparent;
    border-top: 5px solid {p.TEXT_MUTED};
}}
QSpinBox::up-arrow:hover, QDoubleSpinBox::up-arrow:hover {{
    border-bottom-color: {p.ACCENT};
}}
QSpinBox::down-arrow:hover, QDoubleSpinBox::down-arrow:hover {{
    border-top-color: {p.ACCENT};
}}
QSpinBox::up-button:disabled, QDoubleSpinBox::up-button:disabled,
QSpinBox::down-button:disabled, QDoubleSpinBox::down-button:disabled {{
    border-left-color: {p.BORDER_DISABLED};
}}
QSpinBox::up-arrow:disabled, QDoubleSpinBox::up-arrow:disabled,
QSpinBox::down-arrow:disabled, QDoubleSpinBox::down-arrow:disabled {{
    border-top-color: {p.TEXT_DISABLED};
    border-bottom-color: {p.TEXT_DISABLED};
}}

/* --- Icon-button (compact, toolbar) --- */
QPushButton#iconButton {{
    min-height: 26px;
    min-width: 32px;
    max-width: 36px;
    padding: {SPACING_XS}px;
    font-size: 14px;
}}

/* --- Tabs --- */
QTabBar::tab {{
    background: {p.BG_PRESSED};
    border: 1px solid {p.BORDER_DEFAULT};
    padding: 7px 12px;
    min-width: 72px;
}}
QTabBar::tab:selected {{
    background: {p.BG_INPUT_HOVER};
    color: {p.ACCENT_DIM};
    border-bottom: 3px solid {p.ACCENT};
}}
QTabBar::tab:hover:!selected {{
    background: {p.BG_PANEL_LIGHT};
}}

/* --- Table & text --- */
QTextEdit, QTableWidget {{
    background: {p.BG_TABLE};
    border: 1px solid {p.BORDER_DEFAULT};
    border-radius: {RADIUS_CONTROL}px;
    selection-background-color: #075985;
}}
QTableWidget {{
    gridline-color: {p.BORDER_GRID};
    alternate-background-color: {p.BG_TABLE_ALT};
}}
QHeaderView::section {{
    background: {p.BG_PANEL_LIGHT};
    color: {p.TEXT_SUBTLE};
    border: 0;
    border-bottom: 1px solid {p.BORDER_TABLE};
    padding: 6px 8px;
    font-weight: {FONT_WEIGHT_MED};
}}
QCheckBox {{ spacing: 7px; }}
QCheckBox::indicator {{ width: 16px; height: 16px; }}
QSplitter::handle {{ background: {p.BORDER_SPLITTER}; width: 2px; }}
QStatusBar {{ color: {p.TEXT_MUTED}; border-top: 1px solid {p.BORDER_SPLITTER}; }}

/* --- Collapsed rail --- */
QFrame#leftRail {{
    background: {p.BG_PANEL};
    border: 1px solid {p.BORDER_DEFAULT};
    border-radius: {RADIUS_CONTAINER}px;
    max-width: 48px;
}}
QFrame#leftRail QPushButton {{
    min-height: 36px;
    max-width: 36px;
    border-radius: {RADIUS_CONTROL}px;
    font-size: 16px;
    text-align: center;
    margin: 2px;
}}
QFrame#rightRail {{
    background: {p.BG_PANEL};
    border: 1px solid {p.BORDER_DEFAULT};
    border-radius: {RADIUS_CONTAINER}px;
    max-width: 56px;
}}

/* --- Pulse animation for connecting state --- */
QLabel#stateBadge[state="connecting"], QLabel#stateBadge[state="handshake"] {{
    color: {p.INFO};
}}
QLabel#stateBadge[state="connecting"][pulse="on"], QLabel#stateBadge[state="handshake"][pulse="on"] {{
    background: {p.INFO_BG}; border-color: {p.INFO_BORDER}; color: {p.INFO}; opacity: 0.7;
}}
QLabel#stateBadge[state="connecting"][pulse="off"], QLabel#stateBadge[state="handshake"][pulse="off"] {{
    background: {p.BG_PANEL_LIGHT}; border-color: {p.BORDER_TABLE}; color: {p.INFO}; opacity: 1.0;
}}

/* Emergency button breathing animation */
QPushButton#emergencyButton[pulse="on"] {{
    background: {p.DANGER_BG}; border: 2px solid {p.DANGER};
}}
QPushButton#emergencyButton[pulse="off"] {{
    background: {p.DANGER_BG_HOVER}; border: 2px solid {p.DANGER};
}}

/* --- v0.3.1 additions --- */

/* Emergency button inner glow */
QPushButton#emergencyButton {{
    box-shadow: inset 0 0 8px rgba(248, 113, 113, 0.35);
}}

/* Motor colour bar (left border on motor_state_box) */
QGroupBox#motorStateBox[motorColor="0"] {{ border-left: 4px solid {Palette.MOTOR_COLORS[0]}; }}
QGroupBox#motorStateBox[motorColor="1"] {{ border-left: 4px solid {Palette.MOTOR_COLORS[1]}; }}
QGroupBox#motorStateBox[motorColor="2"] {{ border-left: 4px solid {Palette.MOTOR_COLORS[2]}; }}
QGroupBox#motorStateBox[motorColor="3"] {{ border-left: 4px solid {Palette.MOTOR_COLORS[3]}; }}

/* Console bottom toolbar */
QFrame#consoleToolbar {{
    background: {p.BG_PANEL};
    border: 1px solid {p.BORDER_DEFAULT};
    border-radius: {RADIUS_CONTROL}px;
    padding: {SPACING_XS}px {SPACING_MD}px;
}}
QLabel#byteCounter {{
    color: {p.TEXT_MUTED};
    font-family: {FONT_MONO};
    font-size: {FONT_CAPTION}pt;
}}

/* Quick command buttons (compact) */
QPushButton#quickCmdButton {{
    min-height: 24px;
    max-height: 24px;
    padding: {SPACING_XS}px {SPACING_MD}px;
    font-size: {FONT_CAPTION}pt;
    font-family: {FONT_MONO};
}}

/* Connection button state colours */
QPushButton#connectButton[state="disconnected"] {{
    background: {Palette.CONN_DISCONNECTED}; border-color: {Palette.CONN_DISCONNECTED}; color: {p.TEXT_PRIMARY};
}}
QPushButton#connectButton[state="connecting"],
QPushButton#connectButton[state="handshake"] {{
    background: {Palette.CONN_CONNECTING}; border-color: {Palette.CONN_CONNECTING}; color: white;
}}
QPushButton#connectButton[state="ready"] {{
    background: {Palette.CONN_READY}; border-color: {Palette.CONN_READY}; color: white;
}}

/* DTR/RTS indicator dots */
QLabel#dtrDot, QLabel#rtsDot {{
    background: transparent;
}}

/* Gradient divider */
QFrame#gradientDivider {{
    background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
        stop:0 {p.BORDER_DEFAULT}, stop:1 rgba(38, 54, 77, 0));
    max-height: 1px;
    min-height: 1px;
}}
"""
