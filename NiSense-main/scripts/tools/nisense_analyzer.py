"""
nisense_analyzer.py — NiSense NOR-Flash / USB-MSC Data Analyzer
================================================================
Point at the USB drive root (e.g. E:\\ or /media/NAND) and this tool
opens ppg_hr.csv, ppg_spo2.csv and glucose.csv, analyzes each record
for missing / garbage samples, then lets you plot any record.

Usage
-----
python nisense_analyzer.py [folder]

Dependencies
------------
pip install PySide6 matplotlib numpy pandas
"""

import csv
import io
import os
import signal
import sys
from pathlib import Path
from typing import Optional

import numpy as np

# ── PySide6 ────────────────────────────────────────────────────────────────
from PySide6.QtCore import (
    Qt, QAbstractTableModel, QModelIndex, QSortFilterProxyModel,
    QThread, Signal, QObject, QTimer,
)
from PySide6.QtGui import QColor, QFont, QAction, QIcon
from PySide6.QtWidgets import (
    QApplication, QMainWindow, QWidget, QVBoxLayout, QHBoxLayout,
    QPushButton, QLabel, QFileDialog, QTabWidget, QTableView,
    QAbstractItemView, QHeaderView, QSplitter, QStatusBar,
    QMessageBox, QLineEdit, QGroupBox, QSizePolicy,
    QProgressBar, QToolBar, QStyle, QFrame,
)

# ── Matplotlib inside Qt ────────────────────────────────────────────────────
from matplotlib.backends.backend_qtagg import FigureCanvasQTAgg as FigureCanvas
from matplotlib.backends.backend_qtagg import NavigationToolbar2QT as NavToolbar
from matplotlib.figure import Figure
import matplotlib.patches as mpatches

# ===========================================================================
# Constants
# ===========================================================================
PPG_HR_FILE   = "ppg_hr.csv"
PPG_SPO2_FILE = "ppg_spo2.csv"
GLUCOSE_FILE  = "glucose.csv"

VERDICT_COLOR = {
    "LIKELY_PPG":  "#27ae60",
    "MIXED":       "#e67e22",
    "UNRELIABLE":  "#e74c3c",
    "CLEAN":       "#27ae60",
    "SUSPECT":     "#e67e22",
    "CORRUPT":     "#e74c3c",
    "NO DATA":     "#95a5a6",
}

BG_VERDICT = {
    "LIKELY_PPG":  QColor("#123524"),
    "MIXED":       QColor("#3b2d11"),
    "UNRELIABLE":  QColor("#3a1616"),
    "CLEAN":       QColor("#123524"),
    "SUSPECT":     QColor("#3b2d11"),
    "CORRUPT":     QColor("#3a1616"),
    "NO DATA":     QColor("#1f2933"),
}

BG_VERDICT_LIGHT = {
    "LIKELY_PPG":  QColor("#e8f8f0"),
    "MIXED":       QColor("#fef5e7"),
    "UNRELIABLE":  QColor("#fdecea"),
    "CLEAN":       QColor("#e8f8f0"),
    "SUSPECT":     QColor("#fef5e7"),
    "CORRUPT":     QColor("#fdecea"),
    "NO DATA":     QColor("#f4f4f4"),
}


# ===========================================================================
# Signal / parse helpers
# ===========================================================================

def safe_read(path: Path) -> list[dict]:
    """Read a CSV that may contain raw binary garbage bytes."""
    raw = path.read_bytes()
    text = raw.decode("utf-8", errors="replace").replace("\x00", "")
    reader = csv.DictReader(io.StringIO(text))
    rows = list(reader)

    normalized = []
    for row in rows:
        clean = {}
        for k, v in row.items():
            key = (k or "").replace("\ufeff", "").strip()
            clean[key] = "" if v is None else str(v).strip()
        normalized.append(clean)
    return normalized


def get_first(row: dict, keys: list[str], default: str = "") -> str:
    """Get first non-empty value from candidate keys (case-sensitive first, then normalized)."""
    for k in keys:
        v = row.get(k, "")
        if str(v).strip() != "":
            return str(v).strip()

    # Fallback: tolerate header variants like spaces/hyphens/case changes.
    norm_map = {}
    for k, v in row.items():
        nk = "".join(ch for ch in str(k).lower() if ch.isalnum())
        norm_map[nk] = str(v).strip()

    for k in keys:
        nk = "".join(ch for ch in k.lower() if ch.isalnum())
        v = norm_map.get(nk, "")
        if v != "":
            return v

    return default


def parse_int_safe(text: str, default: int = 0) -> int:
    t = str(text or "").strip()
    if not t:
        return default
    try:
        return int(float(t))
    except ValueError:
        return default


def parse_float_safe(text: str, default: float = 0.0) -> float:
    t = str(text or "").strip()
    if not t:
        return default
    try:
        return float(t)
    except ValueError:
        return default


def parse_packed(text: str) -> tuple[np.ndarray, int]:
    """Split semicolon-packed string → (float_array, bad_token_count)."""
    if not text or not text.strip():
        return np.empty(0, np.float64), 0
    vals, bad = [], 0
    for t in text.split(";"):
        t = t.strip()
        try:
            vals.append(float(t))
        except ValueError:
            bad += 1
    return np.array(vals, np.float64), bad


def corruption_onset(series: np.ndarray, z: float = 8.0) -> Optional[int]:
    """First index of a step-jump that is a clear outlier vs the whole-series diff distribution.

    Uses median + 1.4826·MAD (robust estimators) over *all* diffs so that a
    small, noisy reference window cannot produce false positives on signals
    where the first few samples happen to be unusually smooth or noisy.
    """
    if len(series) < 10:
        return None
    diffs = np.abs(np.diff(series))
    median_d = float(np.median(diffs))
    mad = float(np.median(np.abs(diffs - median_d)))
    # 1.4826·MAD ≈ σ for a normal distribution; fall back if MAD==0
    scale = mad * 1.4826 if mad > 0 else max(median_d * 0.1, 1.0)
    for i, d in enumerate(diffs):
        if (d - median_d) / scale > z:
            return i
    return None


def naive_bpm(ir: np.ndarray, fs: float) -> float:
    if len(ir) < 8:
        return 0.0
    sm = np.convolve(ir, np.ones(3) / 3, "valid")
    thr = sm.mean()
    n = len(sm)
    peaks = sum(
        1 for i in range(1, n - 1)
        if sm[i] > sm[i - 1] and sm[i] > sm[i + 1] and sm[i] > thr
    )
    return 60.0 * peaks / (len(ir) / fs) if len(ir) > 0 else 0.0


def ppg_quality(ir: np.ndarray, red: np.ndarray,
                ir_bad: int, red_bad: int) -> tuple[int, str]:
    n    = min(len(ir), len(red))
    same = len(ir) == len(red)
    corr = float(np.corrcoef(ir[:n], red[:n])[0, 1]) if n >= 4 else 0.0
    bpm  = naive_bpm(ir, 25.0)

    score = 0
    if len(ir) >= 280 and len(red) >= 280 and same: score += 30
    if 40 <= bpm <= 160:                              score += 30
    if abs(corr) >= 0.20:                             score += 25
    if ir_bad == 0 and red_bad == 0:                  score += 15

    label = "LIKELY_PPG" if score >= 75 else ("MIXED" if score >= 45 else "UNRELIABLE")
    return score, label


# ===========================================================================
# Per-file record builders
# ===========================================================================

def build_ppg_records(rows: list[dict], file_type: str) -> list[dict]:
    """Parse a PPG HR or SpO2 CSV row list into analysis records."""
    records = []
    for i, row in enumerate(rows):
        try:
            ir, ir_bad = parse_packed(get_first(row, ["IR_Samples", "IR", "IR_Values"]))
            red, red_bad = parse_packed(get_first(row, ["Red_Samples", "RED_Samples", "Red", "RED"]))
            acc_x, acc_x_bad = parse_packed(get_first(row, ["Accel_X_Samples", "Acc_X_Samples", "Accel_X"]))
            acc_y, acc_y_bad = parse_packed(get_first(row, ["Accel_Y_Samples", "Acc_Y_Samples", "Accel_Y"]))
            acc_z, acc_z_bad = parse_packed(get_first(row, ["Accel_Z_Samples", "Acc_Z_Samples", "Accel_Z"]))

            # Skip blank/metadata-only rows that contain no waveform data.
            if len(ir) == 0 and len(red) == 0:
                continue

            total_declared = parse_int_safe(get_first(row, ["Total_Samples", "Sample_Count"]), 0)
            sample_rate = parse_float_safe(get_first(row, ["Sample_Rate_Hz", "Sample_Rate", "Rate_Hz"]), 25.0)

            score, verdict = ppg_quality(ir, red, ir_bad, red_bad)
            onset_ir = corruption_onset(ir)
            onset_red = corruption_onset(red)
            bpm = naive_bpm(ir, sample_rate)
            base_n = min(len(ir), len(red))
            accel_len_match = (len(acc_x) == base_n and len(acc_y) == base_n and len(acc_z) == base_n)

            if file_type == "hr":
                metric_key = "HR_BPM"
                metric_val = get_first(row, ["HR_BPM", "Heart_Rate_BPM", "HeartRate"])
                valid_key = "HR_Valid"
                valid_val = get_first(row, ["HR_Valid", "Heart_Rate_Valid", "HRValid"])
                conf_val = get_first(row, ["HR_Confidence", "Heart_Rate_Confidence", "HRConf"])
            else:
                metric_key = "SpO2_Percent"
                metric_val = get_first(row, ["SpO2_Percent", "SPO2_Percent", "SpO2", "SPO2"])
                valid_key = "SpO2_Valid"
                valid_val = get_first(row, ["SpO2_Valid", "SPO2_Valid", "SpO2Valid"])
                conf_val = get_first(row, ["SpO2_Confidence", "SPO2_Confidence", "SpO2Conf"])

            date_part = get_first(row, ["Date"]) 
            time_part = get_first(row, ["Time"])
            ts_part = get_first(row, ["Timestamp", "Timestamp_unix"])
            timestamp = (f"{date_part} {time_part}".strip() if (date_part or time_part) else ts_part)

            records.append({
                "_row":         i + 1,
                "_ir":          ir,
                "_red":         red,
                "_acc_x":       acc_x,
                "_acc_y":       acc_y,
                "_acc_z":       acc_z,
                "_ir_bad":      ir_bad,
                "_red_bad":     red_bad,
                "_acc_x_bad":   acc_x_bad,
                "_acc_y_bad":   acc_y_bad,
                "_acc_z_bad":   acc_z_bad,
                "_onset_ir":    onset_ir,
                "_onset_red":   onset_red,
                "Timestamp":    timestamp,
                "Total_Declared": total_declared,
                "IR_Valid":     len(ir),
                "IR_Bad":       ir_bad,
                "RED_Valid":    len(red),
                "RED_Bad":      red_bad,
                "ACC_X_Valid":  len(acc_x),
                "ACC_Y_Valid":  len(acc_y),
                "ACC_Z_Valid":  len(acc_z),
                "ACC_X_Bad":    acc_x_bad,
                "ACC_Y_Bad":    acc_y_bad,
                "ACC_Z_Bad":    acc_z_bad,
                "ACC_Len_Match": "✓" if accel_len_match else "✗",
                "Len_Match":    "✓" if len(ir) == len(red) else "✗",
                "Score":        score,
                "Verdict":      verdict,
                "Est_BPM":      f"{bpm:.0f}",
                "Corrupt_IR":   str(onset_ir) if onset_ir is not None else "—",
                "Corrupt_RED":  str(onset_red) if onset_red is not None else "—",
                metric_key:       metric_val,
                valid_key:        valid_val,
                "Confidence":    conf_val,
                "Sample_Rate":   f"{sample_rate:g}",
            })
        except Exception:
            # Keep loading remaining rows even if one row is malformed.
            continue
    return records


def build_glucose_records(rows: list[dict]) -> list[dict]:
    """Parse glucose CSV rows into analysis records."""
    records = []
    for i, row in enumerate(rows):
        try:
            adc, adc_bad = parse_packed(get_first(row, ["ADC_Samples", "ADC", "Adc_Samples"]))
            volt, volt_bad = parse_packed(get_first(row, ["Voltage_mV", "Voltages_mV", "Voltage"]))

            # Skip blank rows with no waveform content.
            if len(adc) == 0 and len(volt) == 0:
                continue

            n_gluc = parse_float_safe(get_first(row, ["Normalized_Glucose", "Normalized_Glucose_mg_dL", "Glucose"]), 0.0)
            homa = parse_float_safe(get_first(row, ["HOMA_IR_Index", "HOMA_IR", "HOMA-IR"]), 0.0)

            total_bad = adc_bad + volt_bad
            # n_gluc may be in mmol/L (~2–35) or mg/dL (~36–600) depending on
            # firmware calibration — only treat 0.0 (no measurement) as bad.
            gluc_ok = n_gluc > 0.0

            if total_bad == 0 and gluc_ok:
                verdict = "CLEAN"
            elif total_bad > 0:
                verdict = "CORRUPT"
            else:
                verdict = "SUSPECT"

            onset_adc = corruption_onset(adc)
            date_part = get_first(row, ["Date"])
            time_part = get_first(row, ["Time"])
            ts_part = get_first(row, ["Timestamp", "Timestamp_unix"])
            timestamp = (f"{date_part} {time_part}".strip() if (date_part or time_part) else ts_part)

            records.append({
                "_row":           i + 1,
                "_adc":           adc,
                "_volt":          volt,
                "_adc_bad":       adc_bad,
                "_volt_bad":      volt_bad,
                "_onset_adc":     onset_adc,
                "Timestamp":      timestamp,
                "Patient":        get_first(row, ["Patient_Name", "Patient", "Patient_Code"]),
                "ADC_Count":      len(adc),
                "ADC_Bad":        adc_bad,
                "Volt_Count":     len(volt),
                "Volt_Bad":       volt_bad,
                "Norm_Glucose":   f"{n_gluc:.1f}",
                "HOMA_IR":        f"{homa:.3f}",
                "Verdict":        verdict,
                "Corrupt_ADC":    str(onset_adc) if onset_adc is not None else "—",
                "Total_Time_sec": get_first(row, ["Total_Time_sec", "Total_Time"]),
            })
        except Exception:
            # Keep loading remaining rows even if one row is malformed.
            continue
    return records


# ===========================================================================
# Table model
# ===========================================================================

DISPLAY_COLS_PPG_HR = [
    "#", "Timestamp", "Total_Declared", "IR_Valid", "IR_Bad",
    "RED_Valid", "RED_Bad", "ACC_X_Valid", "ACC_Y_Valid", "ACC_Z_Valid",
    "ACC_X_Bad", "ACC_Y_Bad", "ACC_Z_Bad", "ACC_Len_Match", "Len_Match", "Score", "Verdict",
    "Est_BPM", "HR_BPM", "HR_Valid", "Confidence",
    "Corrupt_IR", "Corrupt_RED", "Sample_Rate",
]

DISPLAY_COLS_PPG_SPO2 = [
    "#", "Timestamp", "Total_Declared", "IR_Valid", "IR_Bad",
    "RED_Valid", "RED_Bad", "ACC_X_Valid", "ACC_Y_Valid", "ACC_Z_Valid",
    "ACC_X_Bad", "ACC_Y_Bad", "ACC_Z_Bad", "ACC_Len_Match", "Len_Match", "Score", "Verdict",
    "Est_BPM", "SpO2_Percent", "SpO2_Valid", "Confidence",
    "Corrupt_IR", "Corrupt_RED", "Sample_Rate",
]

DISPLAY_COLS_GLUCOSE = [
    "#", "Timestamp", "Patient", "ADC_Count", "ADC_Bad",
    "Volt_Count", "Volt_Bad", "Norm_Glucose", "HOMA_IR",
    "Verdict", "Corrupt_ADC", "Total_Time_sec",
]


class RecordTableModel(QAbstractTableModel):
    def __init__(self, records: list[dict], columns: list[str], theme: str = "dark", parent=None):
        super().__init__(parent)
        self._records = records
        self._cols    = columns
        self._theme = theme
        self._bg_map = BG_VERDICT if theme == "dark" else BG_VERDICT_LIGHT

    def rowCount(self, parent=QModelIndex()):
        return len(self._records)

    def columnCount(self, parent=QModelIndex()):
        return len(self._cols)

    def headerData(self, section, orientation, role=Qt.DisplayRole):
        if orientation == Qt.Horizontal and role == Qt.DisplayRole:
            return self._cols[section]
        return None

    def data(self, index, role=Qt.DisplayRole):
        if not index.isValid():
            return None
        rec  = self._records[index.row()]
        col  = self._cols[index.column()]
        verdict = rec.get("Verdict", "")

        if role == Qt.DisplayRole:
            if col == "#":
                return str(rec.get("_row", index.row() + 1))
            return str(rec.get(col, ""))

        if role == Qt.BackgroundRole:
            return self._bg_map.get(verdict, None)

        if role == Qt.TextAlignmentRole:
            return Qt.AlignCenter

        if role == Qt.FontRole and col == "Verdict":
            f = QFont()
            f.setBold(True)
            return f

        if role == Qt.ForegroundRole and col == "Verdict":
            return QColor(VERDICT_COLOR.get(verdict, "#000000"))

        return None

    def record_at(self, row: int) -> dict:
        return self._records[row]

    def set_theme(self, theme: str):
        self._theme = theme
        self._bg_map = BG_VERDICT if theme == "dark" else BG_VERDICT_LIGHT
        if self.rowCount() > 0 and self.columnCount() > 0:
            self.dataChanged.emit(
                self.index(0, 0),
                self.index(self.rowCount() - 1, self.columnCount() - 1),
                [Qt.BackgroundRole],
            )


# ===========================================================================
# Plot panel (embedded matplotlib)
# ===========================================================================

class PlotPanel(QWidget):
    def __init__(self, parent=None):
        super().__init__(parent)
        self._theme = "dark"
        self._palette = self._palette_for_theme(self._theme)
        self.fig    = Figure(figsize=(12, 5), tight_layout=True, facecolor=self._palette["fig_bg"])
        self.canvas = FigureCanvas(self.fig)
        self.canvas.setSizePolicy(QSizePolicy.Expanding, QSizePolicy.Expanding)
        nav = NavToolbar(self.canvas, self)

        layout = QVBoxLayout(self)
        layout.setContentsMargins(0, 0, 0, 0)
        layout.addWidget(nav)
        layout.addWidget(self.canvas)

    def _palette_for_theme(self, theme: str) -> dict:
        if theme == "light":
            return {
                "fig_bg": "#ffffff",
                "ax_bg": "#ffffff",
                "grid": "#6b7f93",
                "tick": "#1f2933",
                "spine": "#c7d2dd",
                "legend_bg": "#ffffff",
                "legend_edge": "#c7d2dd",
                "title_default": "#111827",
                "line_ir": "#1f77b4",
                "line_red": "#d62728",
                "line_acc_x": "#ff7f0e",
                "line_acc_y": "#2ca02c",
                "line_acc_z": "#9467bd",
                "line_adc": "#7c3aed",
                "line_volt": "#0f766e",
            }
        return {
            "fig_bg": "#0f1720",
            "ax_bg": "#0b121a",
            "grid": "#8aa4bf",
            "tick": "#d6dee8",
            "spine": "#4a5c70",
            "legend_bg": "#111b26",
            "legend_edge": "#4a5c70",
            "title_default": "#d6dee8",
            "line_ir": "#2980b9",
            "line_red": "#c0392b",
            "line_acc_x": "#f39c12",
            "line_acc_y": "#27ae60",
            "line_acc_z": "#9b59b6",
            "line_adc": "#8e44ad",
            "line_volt": "#16a085",
        }

    def set_theme(self, theme: str):
        self._theme = theme
        self._palette = self._palette_for_theme(theme)
        self.clear()

    def _style_axis(self, ax):
        ax.set_facecolor(self._palette["ax_bg"])
        ax.grid(True, alpha=0.22, color=self._palette["grid"])
        ax.tick_params(colors=self._palette["tick"])
        for spine in ax.spines.values():
            spine.set_color(self._palette["spine"])
        ax.xaxis.label.set_color(self._palette["tick"])
        ax.yaxis.label.set_color(self._palette["tick"])
        leg = ax.get_legend()
        if leg:
            leg.get_frame().set_facecolor(self._palette["legend_bg"])
            leg.get_frame().set_edgecolor(self._palette["legend_edge"])
            for text in leg.get_texts():
                text.set_color(self._palette["tick"])

    def clear(self):
        self.fig.clear()
        self.canvas.draw()

    def plot_ppg(self, rec: dict, fs: float = 25.0):
        self.fig.clear()
        self.fig.patch.set_facecolor(self._palette["fig_bg"])
        ir       = rec["_ir"]
        red      = rec["_red"]
        acc_x    = rec.get("_acc_x", np.empty(0, np.float64))
        acc_y    = rec.get("_acc_y", np.empty(0, np.float64))
        acc_z    = rec.get("_acc_z", np.empty(0, np.float64))
        onset_ir = rec["_onset_ir"]
        onset_rd = rec["_onset_red"]
        verdict  = rec.get("Verdict", "")

        t_ir  = np.arange(len(ir))  / fs
        t_red = np.arange(len(red)) / fs
        t_ax  = np.arange(len(acc_x)) / fs
        t_ay  = np.arange(len(acc_y)) / fs
        t_az  = np.arange(len(acc_z)) / fs

        color_map = {"LIKELY_PPG": "#27ae60", "MIXED": "#e67e22", "UNRELIABLE": "#e74c3c"}
        title_color = color_map.get(verdict, self._palette["title_default"])

        ts  = rec.get("Timestamp", "")
        bpm = rec.get("Est_BPM", "?")
        self.fig.suptitle(
            f"PPG Record — {ts}  |  Verdict: {verdict}  |  ~{bpm} BPM",
            fontsize=10, fontweight="bold", color=title_color,
        )

        ax1 = self.fig.add_subplot(3, 1, 1)
        ax2 = self.fig.add_subplot(3, 1, 2)
        ax3 = self.fig.add_subplot(3, 1, 3)

        for ax, series, t, onset, color, label in [
            (ax1, ir,  t_ir,  onset_ir, self._palette["line_ir"], "IR"),
            (ax2, red, t_red, onset_rd, self._palette["line_red"], "RED"),
        ]:
            if len(series) == 0:
                ax.text(0.5, 0.5, f"{label}: no data", ha="center", va="center",
                        transform=ax.transAxes, color=self._palette["tick"])
                self._style_axis(ax)
                continue
            ax.plot(t, series, color=color, linewidth=0.9, label=label)
            if onset is not None:
                ot = onset / fs
                ax.axvline(ot, color="magenta", linewidth=1.5, linestyle="--",
                           label=f"Corruption @ smp {onset} ({ot:.1f}s)")
                ax.axvspan(ot, t[-1], alpha=0.07, color="magenta")
            ax.set_ylabel("ADC counts")
            ax.legend(loc="upper right", fontsize=8)
            self._style_axis(ax)

        if len(acc_x) == 0 and len(acc_y) == 0 and len(acc_z) == 0:
            ax3.text(0.5, 0.5, "Accel: no data", ha="center", va="center",
                     transform=ax3.transAxes, color=self._palette["tick"])
        else:
            if len(acc_x) > 0:
                ax3.plot(t_ax, acc_x, color=self._palette["line_acc_x"], linewidth=0.9,
                         label="ACC_X (mg)")
            if len(acc_y) > 0:
                ax3.plot(t_ay, acc_y, color=self._palette["line_acc_y"], linewidth=0.9,
                         label="ACC_Y (mg)")
            if len(acc_z) > 0:
                ax3.plot(t_az, acc_z, color=self._palette["line_acc_z"], linewidth=0.9,
                         label="ACC_Z (mg)")
            ax3.legend(loc="upper right", fontsize=8)

        ax3.set_ylabel("Accel (mg)")
        self._style_axis(ax3)
        ax3.set_xlabel("Time (s)")
        self.canvas.draw()

    def plot_glucose(self, rec: dict, fs: float = 1.0):
        self.fig.clear()
        self.fig.patch.set_facecolor(self._palette["fig_bg"])
        adc      = rec["_adc"]
        volt     = rec["_volt"]
        onset    = rec["_onset_adc"]
        verdict  = rec.get("Verdict", "")
        ts       = rec.get("Timestamp", "")
        gluc     = rec.get("Norm_Glucose", "?")

        t = np.arange(len(adc))

        color_map = {"CLEAN": "#27ae60", "SUSPECT": "#e67e22", "CORRUPT": "#e74c3c"}
        self.fig.suptitle(
            f"Glucose Record — {ts}  |  Verdict: {verdict}  |  Glucose: {gluc} mg/dL",
            fontsize=10, fontweight="bold",
            color=color_map.get(verdict, self._palette["title_default"]),
        )

        ax1 = self.fig.add_subplot(2, 1, 1)
        ax2 = self.fig.add_subplot(2, 1, 2)

        for ax, series, color, label in [
            (ax1, adc,  self._palette["line_adc"], "ADC"),
            (ax2, volt, self._palette["line_volt"], "Voltage_mV"),
        ]:
            if len(series) == 0:
                ax.text(0.5, 0.5, f"{label}: no data", ha="center", va="center",
                        transform=ax.transAxes, color=self._palette["tick"])
                self._style_axis(ax)
                continue
            ax.plot(series, color=color, linewidth=0.9, label=label)
            if onset is not None:
                ax.axvline(onset, color="magenta", linewidth=1.5, linestyle="--",
                           label=f"Corruption @ smp {onset}")
                ax.axvspan(onset, len(series) - 1, alpha=0.07, color="magenta")
            ax.set_ylabel(label)
            ax.legend(loc="upper right", fontsize=8)
            self._style_axis(ax)

        ax2.set_xlabel("Sample index")
        self.canvas.draw()


# ===========================================================================
# Worker thread for loading / analysis
# ===========================================================================

class LoadWorker(QObject):
    progress  = Signal(int)
    finished  = Signal(dict)   # {tab_name: (records, columns)}
    error     = Signal(str)

    def __init__(self, folder: Path):
        super().__init__()
        self._folder = folder

    def run(self):
        result = {}
        files = {
            "ppg_hr":   (PPG_HR_FILE,   "hr"),
            "ppg_spo2": (PPG_SPO2_FILE, "spo2"),
            "glucose":  (GLUCOSE_FILE,  None),
        }
        steps = list(files.items())
        for idx, (key, (fname, ftype)) in enumerate(steps):
            path = self._folder / fname
            self.progress.emit(int((idx / len(steps)) * 90))
            if not path.exists():
                result[key] = ([], [])
                continue
            try:
                rows = safe_read(path)
                if ftype in ("hr", "spo2"):
                    records = build_ppg_records(rows, ftype)
                    cols = (DISPLAY_COLS_PPG_HR if ftype == "hr"
                            else DISPLAY_COLS_PPG_SPO2)
                else:
                    records = build_glucose_records(rows)
                    cols = DISPLAY_COLS_GLUCOSE

                if len(rows) > 0 and len(records) == 0:
                    self.error.emit(
                        f"{fname}: file has {len(rows)} row(s) but none were parseable; check column names/format"
                    )

                result[key] = (records, cols)
            except Exception as exc:
                self.error.emit(f"{fname}: {exc}")
                result[key] = ([], [])

        self.progress.emit(100)
        self.finished.emit(result)


# ===========================================================================
# Per-tab widget
# ===========================================================================

class DataTab(QWidget):
    def __init__(self, tab_type: str, parent=None):
        super().__init__(parent)
        self._type = tab_type      # "ppg_hr" | "ppg_spo2" | "glucose"
        self._theme = "dark"

        # ── Summary bar ───────────────────────────────────────────────────
        self._summary = QLabel("No data loaded")
        self._summary.setStyleSheet("font-weight:bold; padding:4px 8px;")

        # ── Filter bar ────────────────────────────────────────────────────
        filter_box = QHBoxLayout()
        filter_box.addWidget(QLabel("Filter:"))
        self._filter_edit = QLineEdit()
        self._filter_edit.setPlaceholderText("Type to filter any column…")
        self._filter_edit.textChanged.connect(self._on_filter)
        filter_box.addWidget(self._filter_edit)

        self._plot_btn = QPushButton("📈  Plot Selected")
        self._plot_btn.setEnabled(False)
        self._plot_btn.setFixedWidth(150)
        filter_box.addWidget(self._plot_btn)

        # ── Table ─────────────────────────────────────────────────────────
        self._table = QTableView()
        self._table.setSelectionBehavior(QAbstractItemView.SelectRows)
        self._table.setSelectionMode(QAbstractItemView.SingleSelection)
        self._table.setAlternatingRowColors(False)
        self._table.setSortingEnabled(True)
        self._table.horizontalHeader().setSectionResizeMode(QHeaderView.ResizeToContents)
        self._table.verticalHeader().setVisible(False)
        self._table.setEditTriggers(QAbstractItemView.NoEditTriggers)
        self._table.doubleClicked.connect(self._on_double_click)

        self._proxy = QSortFilterProxyModel()
        self._proxy.setFilterCaseSensitivity(Qt.CaseInsensitive)
        self._proxy.setFilterKeyColumn(-1)
        self._table.setModel(self._proxy)

        # ── Plot panel ────────────────────────────────────────────────────
        self._plot_panel = PlotPanel()
        self._plot_panel.setMinimumHeight(320)

        splitter = QSplitter(Qt.Vertical)
        splitter.addWidget(self._table)
        splitter.addWidget(self._plot_panel)
        splitter.setSizes([400, 340])

        # ── Layout ────────────────────────────────────────────────────────
        top_frame = QFrame()
        top_layout = QVBoxLayout(top_frame)
        top_layout.setContentsMargins(0, 0, 0, 0)
        top_layout.addWidget(self._summary)
        top_layout.addLayout(filter_box)

        layout = QVBoxLayout(self)
        layout.addWidget(top_frame)
        layout.addWidget(splitter)

        self._plot_btn.clicked.connect(self._plot_selected)
        self._table.selectionModel() if False else None   # placeholder

    # ── Public API ────────────────────────────────────────────────────────

    def load(self, records: list[dict], columns: list[str]):
        self._records = records
        self._model   = RecordTableModel(records, columns, theme=self._theme)
        self._proxy.setSourceModel(self._model)
        self._table.selectionModel().selectionChanged.connect(self._on_selection)
        self._table.horizontalHeader().setSectionResizeMode(QHeaderView.ResizeToContents)

        if not records:
            self._summary.setText("No data found")
            self._summary.setStyleSheet("color:#e74c3c; font-weight:bold; padding:4px 8px;")
            return

        n_total     = len(records)
        n_clean     = sum(1 for r in records if r["Verdict"] in ("LIKELY_PPG", "CLEAN"))
        n_mixed     = sum(1 for r in records if r["Verdict"] in ("MIXED", "SUSPECT"))
        n_corrupt   = sum(1 for r in records if r["Verdict"] in ("UNRELIABLE", "CORRUPT"))

        self._summary.setText(
            f"Records: {n_total}   ✓ OK: {n_clean}   "
            f"~ Mixed: {n_mixed}   ✗ Corrupt: {n_corrupt}"
        )
        self._summary.setStyleSheet(
            "font-weight:bold; padding:4px 8px;"
            + ("color:#27ae60;" if n_corrupt == 0 else "color:#e74c3c;")
        )

    # ── Slots ─────────────────────────────────────────────────────────────

    def _on_filter(self, text: str):
        self._proxy.setFilterFixedString(text)

    def _on_selection(self):
        has = bool(self._table.selectionModel().selectedRows())
        self._plot_btn.setEnabled(has)

    def _on_double_click(self, _):
        self._plot_selected()

    def _plot_selected(self):
        rows = self._table.selectionModel().selectedRows()
        if not rows:
            return
        proxy_row  = rows[0].row()
        source_row = self._proxy.mapToSource(self._proxy.index(proxy_row, 0)).row()
        rec        = self._model.record_at(source_row)

        if self._type == "glucose":
            self._plot_panel.plot_glucose(rec)
        else:
            fs = float(rec.get("Sample_Rate", 25) or 25)
            self._plot_panel.plot_ppg(rec, fs=fs)

    def set_theme(self, theme: str):
        self._theme = theme
        self._plot_panel.set_theme(theme)
        if hasattr(self, "_model") and self._model is not None:
            self._model.set_theme(theme)


# ===========================================================================
# Main window
# ===========================================================================

class MainWindow(QMainWindow):
    def __init__(self, initial_folder: Optional[Path] = None):
        super().__init__()
        self.setWindowTitle("NiSense Data Analyzer")
        self.resize(1300, 820)

        self._folder: Optional[Path] = None
        self._thread: Optional[QThread] = None
        self._worker: Optional[LoadWorker] = None
        self._theme: str = "dark"

        self._build_ui()
        if initial_folder and initial_folder.is_dir():
            self._load_folder(initial_folder)

    def _cleanup_load_thread(self, wait_ms: int = 5000):
        """Gracefully stop the active loader thread (if any)."""
        if self._thread is None:
            return

        if self._thread.isRunning():
            self._thread.quit()
            self._thread.wait(wait_ms)

        self._thread = None
        self._worker = None

    # ── UI construction ───────────────────────────────────────────────────

    def _build_ui(self):
        # Toolbar
        toolbar = QToolBar("Main")
        toolbar.setMovable(False)
        self.addToolBar(toolbar)

        open_act = QAction("📂  Open Folder", self)
        open_act.setToolTip("Open NiSense USB drive folder")
        open_act.triggered.connect(self._browse_folder)
        toolbar.addAction(open_act)

        refresh_act = QAction("🔄  Refresh", self)
        refresh_act.setToolTip("Reload CSVs from current folder")
        refresh_act.triggered.connect(lambda: self._load_folder(self._folder))
        toolbar.addAction(refresh_act)

        self._theme_act = QAction("☾ Dark", self)
        self._theme_act.setToolTip("Toggle Light/Dark theme")
        self._theme_act.triggered.connect(self._toggle_theme)
        toolbar.addAction(self._theme_act)

        toolbar.addSeparator()

        self._path_label = QLabel("  No folder selected")
        self._path_label.setStyleSheet("font-style:italic;")
        toolbar.addWidget(self._path_label)

        # Progress bar (hidden when idle)
        self._progress = QProgressBar()
        self._progress.setFixedWidth(220)
        self._progress.setVisible(False)
        toolbar.addWidget(self._progress)

        # Tabs
        self._tabs = QTabWidget()
        self._tab_hr    = DataTab("ppg_hr")
        self._tab_spo2  = DataTab("ppg_spo2")
        self._tab_gluc  = DataTab("glucose")

        self._tabs.addTab(self._tab_hr,   "❤  PPG / HR")
        self._tabs.addTab(self._tab_spo2, "🩸  PPG / SpO2")
        self._tabs.addTab(self._tab_gluc, "🧪  Glucose")

        self.setCentralWidget(self._tabs)

        # Status bar
        self._status = QStatusBar()
        self.setStatusBar(self._status)
        self._status.showMessage("Open a folder to begin analysis.")

        # Style
        self._apply_theme()

    def _apply_theme(self):
        if self._theme == "dark":
            self.setStyleSheet("""
                QMainWindow   { background: #0f1720; color: #d6dee8; }
                QWidget       { color: #d6dee8; }
                QToolBar      { background: #111b26; spacing: 6px; padding: 4px;
                                border-bottom: 1px solid #2a3a4a; }
                QToolBar QLabel { color: #d6dee8; }
                QToolBar QToolButton { color: #d6dee8; padding: 4px 10px;
                                       border-radius: 4px; }
                QToolBar QToolButton:hover { background: #1f2b39; }
                QTabWidget::pane { border: 1px solid #2a3a4a; border-radius: 4px;
                                   background: #0f1720; }
                QTabBar::tab     { padding: 6px 18px; font-size: 11px;
                                   background: #172230; color: #b4c3d3;
                                   border: 1px solid #2a3a4a; }
                QTabBar::tab:selected { background: #1f7ae0; color: white;
                                        border-radius: 4px 4px 0 0; }
                QTableView       { border: 1px solid #2a3a4a; gridline-color: #223140;
                                   background: #0b121a; alternate-background-color: #101a24;
                                   selection-background-color: #1f7ae0;
                                   selection-color: #ffffff;
                                   font-size: 11px; }
                QHeaderView::section { background: #172230; color: #d6dee8;
                                       padding: 4px; border: 1px solid #2a3a4a;
                                       font-weight: bold;
                                       font-size: 11px; }
                QLineEdit        { background: #111b26; border: 1px solid #2a3a4a;
                                   border-radius: 4px; padding: 4px 6px; }
                QLabel           { color: #d6dee8; }
                QSplitter::handle { background: #2a3a4a; }
                QStatusBar       { background: #111b26; color: #b4c3d3;
                                   border-top: 1px solid #2a3a4a; }
                QProgressBar     { background: #0b121a; border: 1px solid #2a3a4a;
                                   border-radius: 4px; text-align: center; }
                QProgressBar::chunk { background: #1f7ae0; }
                QPushButton      { background: #1f7ae0; color: white;
                                   border-radius: 4px; padding: 5px 14px;
                                   font-size: 11px; }
                QPushButton:hover    { background: #2d8cf0; }
                QPushButton:disabled { background: #4a5c70; color: #9aa8b5; }
            """)
            self._path_label.setStyleSheet("color:#b4c3d3; font-style:italic;")
            self._theme_act.setText("☾ Dark")
        else:
            self.setStyleSheet("""
                QMainWindow   { background: #f5f7fa; color: #1f2933; }
                QWidget       { color: #1f2933; }
                QToolBar      { background: #e8eef5; spacing: 6px; padding: 4px;
                                border-bottom: 1px solid #c7d2dd; }
                QToolBar QLabel { color: #1f2933; }
                QToolBar QToolButton { color: #1f2933; padding: 4px 10px;
                                       border-radius: 4px; }
                QToolBar QToolButton:hover { background: #dce6f1; }
                QTabWidget::pane { border: 1px solid #c7d2dd; border-radius: 4px;
                                   background: #ffffff; }
                QTabBar::tab     { padding: 6px 18px; font-size: 11px;
                                   background: #eef3f8; color: #334155;
                                   border: 1px solid #c7d2dd; }
                QTabBar::tab:selected { background: #2563eb; color: white;
                                        border-radius: 4px 4px 0 0; }
                QTableView       { border: 1px solid #c7d2dd; gridline-color: #e3e8ee;
                                   background: #ffffff; alternate-background-color: #f9fbfd;
                                   selection-background-color: #2563eb;
                                   selection-color: #ffffff;
                                   font-size: 11px; }
                QHeaderView::section { background: #eef3f8; color: #1f2933;
                                       padding: 4px; border: 1px solid #c7d2dd;
                                       font-weight: bold;
                                       font-size: 11px; }
                QLineEdit        { background: #ffffff; border: 1px solid #c7d2dd;
                                   border-radius: 4px; padding: 4px 6px; }
                QLabel           { color: #1f2933; }
                QSplitter::handle { background: #c7d2dd; }
                QStatusBar       { background: #e8eef5; color: #475569;
                                   border-top: 1px solid #c7d2dd; }
                QProgressBar     { background: #ffffff; border: 1px solid #c7d2dd;
                                   border-radius: 4px; text-align: center; }
                QProgressBar::chunk { background: #2563eb; }
                QPushButton      { background: #2563eb; color: white;
                                   border-radius: 4px; padding: 5px 14px;
                                   font-size: 11px; }
                QPushButton:hover    { background: #3b82f6; }
                QPushButton:disabled { background: #94a3b8; color: #e2e8f0; }
            """)
            self._path_label.setStyleSheet("color:#475569; font-style:italic;")
            self._theme_act.setText("☀ Light")

        self._tab_hr.set_theme(self._theme)
        self._tab_spo2.set_theme(self._theme)
        self._tab_gluc.set_theme(self._theme)

    def _toggle_theme(self):
        self._theme = "light" if self._theme == "dark" else "dark"
        self._apply_theme()

    # ── Folder management ─────────────────────────────────────────────────

    def _browse_folder(self):
        folder = QFileDialog.getExistingDirectory(
            self, "Select NiSense USB Drive / NOR Flash folder",
            str(self._folder or Path.home()),
        )
        if folder:
            self._load_folder(Path(folder))

    def _load_folder(self, folder: Optional[Path]):
        if not folder or not folder.is_dir():
            return

        if self._thread is not None and self._thread.isRunning():
            self._status.showMessage("Load already in progress - please wait")
            return

        self._folder = folder
        self._path_label.setText(f"  {folder}")

        found = [f for f in (PPG_HR_FILE, PPG_SPO2_FILE, GLUCOSE_FILE)
                 if (folder / f).exists()]
        if not found:
            QMessageBox.warning(
                self, "No files found",
                f"None of ppg_hr.csv / ppg_spo2.csv / glucose.csv found in:\n{folder}"
            )
            return

        self._progress.setVisible(True)
        self._progress.setValue(0)
        self._status.showMessage(f"Loading from {folder} …")

        # Run in a thread to keep UI responsive
        self._thread = QThread()
        self._worker = LoadWorker(folder)
        self._worker.moveToThread(self._thread)

        self._thread.started.connect(self._worker.run)
        self._worker.progress.connect(self._progress.setValue)
        self._worker.finished.connect(self._on_loaded)
        self._worker.error.connect(
            lambda msg: self._status.showMessage(f"Warning: {msg}")
        )
        self._worker.finished.connect(self._thread.quit)
        self._worker.finished.connect(self._worker.deleteLater)
        self._thread.finished.connect(self._thread.deleteLater)
        self._thread.finished.connect(lambda: setattr(self, "_thread", None))
        self._thread.finished.connect(lambda: setattr(self, "_worker", None))
        self._thread.start()

    def _on_loaded(self, result: dict):
        self._progress.setVisible(False)

        ppg_hr_recs,   ppg_hr_cols   = result.get("ppg_hr",   ([], []))
        ppg_spo2_recs, ppg_spo2_cols = result.get("ppg_spo2", ([], []))
        gluc_recs,     gluc_cols     = result.get("glucose",  ([], []))

        self._tab_hr.load(ppg_hr_recs,   ppg_hr_cols   or DISPLAY_COLS_PPG_HR)
        self._tab_spo2.load(ppg_spo2_recs, ppg_spo2_cols or DISPLAY_COLS_PPG_SPO2)
        self._tab_gluc.load(gluc_recs,   gluc_cols     or DISPLAY_COLS_GLUCOSE)

        total = len(ppg_hr_recs) + len(ppg_spo2_recs) + len(gluc_recs)
        self._status.showMessage(
            f"Loaded {total} records from {self._folder}  "
            f"(HR={len(ppg_hr_recs)}, SpO2={len(ppg_spo2_recs)}, "
            f"Glucose={len(gluc_recs)})"
        )

    def closeEvent(self, event):
        self._cleanup_load_thread()
        super().closeEvent(event)


# ===========================================================================
# Entry point
# ===========================================================================

def main():
    app = QApplication(sys.argv)
    app.setApplicationName("NiSense Analyzer")

    # Gracefully handle Ctrl+C when launched from a terminal.
    signal.signal(signal.SIGINT, lambda *_: app.quit())
    sigint_pump = QTimer()
    sigint_pump.timeout.connect(lambda: None)
    sigint_pump.start(200)

    initial = Path(sys.argv[1]) if len(sys.argv) > 1 else None
    win = MainWindow(initial_folder=initial)
    win.show()
    sys.exit(app.exec())


if __name__ == "__main__":
    main()
