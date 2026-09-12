"""
plot_ppg_runs.py  –  Analyze and plot packed PPG CSV captures.

Usage
-----
python plot_ppg_runs.py
python plot_ppg_runs.py --input ~/Downloads/ppg_raw.csv
python plot_ppg_runs.py --input ppg_raw.csv --out-dir ~/Desktop/ppg_plots

Dependencies (auto-installs if missing)
----------------------------------------
pip install matplotlib numpy pandas
"""

import argparse
import os
import re
import sys
from pathlib import Path

# ---------------------------------------------------------------------------
# Dependency bootstrap
# ---------------------------------------------------------------------------
def _ensure_deps():
    import importlib
    import importlib.util  # explicit import required in some Python distributions
    missing = [pkg for pkg in ("matplotlib", "numpy", "pandas") if not importlib.util.find_spec(pkg)]
    if missing:
        import subprocess
        subprocess.check_call([sys.executable, "-m", "pip", "install", "--quiet"] + missing)

_ensure_deps()

import numpy as np
import pandas as pd
import matplotlib
matplotlib.use("Agg")           # headless safe; swap to TkAgg for interactive
import matplotlib.pyplot as plt
import matplotlib.patches as mpatches

# ---------------------------------------------------------------------------
# Parsing helpers
# ---------------------------------------------------------------------------
def parse_series(text: str) -> tuple[np.ndarray, int]:
    """
    Split semicolon-packed string into a float array.
    Returns (valid_values, bad_token_count).
    """
    if not text or not text.strip():
        return np.empty(0, dtype=np.float64), 0

    tokens = text.split(";")
    values, bad = [], 0
    for t in tokens:
        t = t.strip()
        try:
            values.append(float(t))
        except ValueError:
            bad += 1
    return np.array(values, dtype=np.float64), bad


def detect_corruption_onset(series: np.ndarray, window: int = 5, z_thresh: float = 6.0) -> int | None:
    """
    Find the first index where consecutive samples show an anomalous
    step-jump that exceeds z_thresh standard deviations of the local diff
    distribution.  Returns sample index of first corrupt step, or None.
    """
    if len(series) < window * 2:
        return None

    diffs = np.abs(np.diff(series))
    ref_std = np.std(diffs[:window])
    ref_mean = np.mean(diffs[:window])
    if ref_std == 0:
        ref_std = max(ref_mean * 0.01, 1.0)

    for i, d in enumerate(diffs):
        z = (d - ref_mean) / ref_std
        if z > z_thresh:
            return i  # index in original series (step is between i and i+1)
    return None


def naive_bpm(ir: np.ndarray, fs: float = 33.0) -> float:
    """Rough pulse estimate via 3-tap smoothing + above-mean peak counting."""
    if len(ir) < 8:
        return 0.0
    sm = np.convolve(ir, np.ones(3) / 3, mode="valid")
    thresh = sm.mean()
    peaks = 0
    for i in range(1, len(sm) - 1):
        if sm[i] > sm[i - 1] and sm[i] > sm[i + 1] and sm[i] > thresh:
            peaks += 1
    duration_s = len(ir) / fs
    return 60.0 * peaks / duration_s if duration_s > 0 else 0.0


def quality_score(ir: np.ndarray, red: np.ndarray, ir_bad: int, red_bad: int) -> tuple[int, str]:
    score = 0
    same_len = len(ir) == len(red)
    count_ok = len(ir) >= 280 and len(red) >= 280 and same_len
    bpm = naive_bpm(ir)
    bpm_ok = 40.0 <= bpm <= 160.0

    if len(ir) >= 4 and len(red) >= 4:
        n = min(len(ir), len(red))
        corr = np.corrcoef(ir[:n], red[:n])[0, 1]
    else:
        corr = 0.0

    corr_ok = abs(corr) >= 0.20

    if count_ok:
        score += 30
    if bpm_ok:
        score += 30
    if corr_ok:
        score += 25
    if ir_bad == 0 and red_bad == 0:
        score += 15

    if score >= 75:
        label = "LIKELY_PPG"
    elif score >= 45:
        label = "MIXED"
    else:
        label = "UNRELIABLE"

    return score, label


# ---------------------------------------------------------------------------
# Plotting
# ---------------------------------------------------------------------------
VERDICT_COLORS = {
    "LIKELY_PPG": "#2ecc71",
    "MIXED":      "#e67e22",
    "UNRELIABLE": "#e74c3c",
}


def plot_run(run_idx: int, ir: np.ndarray, red: np.ndarray,
             ir_bad: int, red_bad: int,
             ir_onset: int | None, red_onset: int | None,
             score: int, verdict: str,
             out_path: Path, fs: float = 33.0) -> None:

    time_ir  = np.arange(len(ir))  / fs
    time_red = np.arange(len(red)) / fs

    fig, axes = plt.subplots(2, 1, figsize=(14, 6), sharex=False)
    fig.suptitle(
        f"Run {run_idx}  |  Verdict: {verdict}  |  Score: {score}/100  |  "
        f"IR={len(ir)} smp (bad={ir_bad})  RED={len(red)} smp (bad={red_bad})",
        fontsize=11, fontweight="bold",
        color=VERDICT_COLORS.get(verdict, "black")
    )

    for ax, channel, time, onset, color, label in [
        (axes[0], ir,  time_ir,  ir_onset,  "#3498db", "IR"),
        (axes[1], red, time_red, red_onset, "#e74c3c",  "RED"),
    ]:
        if len(channel) == 0:
            ax.text(0.5, 0.5, f"{label}: no data", ha="center", va="center", transform=ax.transAxes)
            continue

        ax.plot(time, channel, color=color, linewidth=0.8, label=label)

        if onset is not None:
            onset_t = onset / fs
            ax.axvline(x=onset_t, color="magenta", linewidth=1.5, linestyle="--",
                       label=f"Corruption onset @ smp {onset} ({onset_t:.2f}s)")
            ax.axvspan(onset_t, time[-1], alpha=0.08, color="magenta")

        ax.set_ylabel("ADC counts")
        ax.legend(loc="upper right", fontsize=8)
        ax.grid(True, alpha=0.3)
        ax.set_xlabel("Time (s)")

    plt.tight_layout()
    fig.savefig(str(out_path), dpi=110)
    plt.close(fig)


def plot_summary(summaries: list[dict], out_path: Path) -> None:
    """Bar chart comparing run scores with verdict colour coding."""
    runs = [s["run"] for s in summaries]
    scores = [s["score"] for s in summaries]
    colors = [VERDICT_COLORS.get(s["verdict"], "grey") for s in summaries]

    fig, ax = plt.subplots(figsize=(max(8, len(runs) * 1.2), 4))
    bars = ax.bar(runs, scores, color=colors, edgecolor="black", linewidth=0.5)

    for bar, score, s in zip(bars, scores, summaries):
        ax.text(bar.get_x() + bar.get_width() / 2, bar.get_height() + 1.5,
                s["verdict"], ha="center", va="bottom", fontsize=8, rotation=0)

    ax.axhline(75, linestyle="--", color="#2ecc71", linewidth=0.8, label="LIKELY_PPG threshold")
    ax.axhline(45, linestyle="--", color="#e67e22", linewidth=0.8, label="MIXED threshold")

    ax.set_xlabel("Run index")
    ax.set_ylabel("Quality score (0–100)")
    ax.set_title("PPG Run Quality Overview")
    ax.set_ylim(0, 115)
    ax.set_xticks(runs)

    patches = [
        mpatches.Patch(color=VERDICT_COLORS["LIKELY_PPG"], label="LIKELY_PPG"),
        mpatches.Patch(color=VERDICT_COLORS["MIXED"],      label="MIXED"),
        mpatches.Patch(color=VERDICT_COLORS["UNRELIABLE"], label="UNRELIABLE"),
    ]
    ax.legend(handles=patches, loc="upper right", fontsize=8)

    plt.tight_layout()
    fig.savefig(str(out_path), dpi=110)
    plt.close(fig)


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------
def main():
    parser = argparse.ArgumentParser(description="Plot and analyze packed PPG CSV captures.")
    parser.add_argument(
        "--input", "-i",
        default=str(Path.home() / "Downloads" / "ppg_raw.csv"),
        help="Path to packed CSV (IR_Samples;Red_Samples format)",
    )
    parser.add_argument(
        "--out-dir", "-o",
        default=str(Path.home() / "Downloads"),
        help="Output directory for PNG files",
    )
    parser.add_argument(
        "--fs", type=float, default=33.0,
        help="Sample rate in Hz (default 33)",
    )
    parser.add_argument(
        "--z-thresh", type=float, default=6.0,
        help="Z-score threshold for corruption onset detection (default 6)",
    )
    args = parser.parse_args()

    input_path = Path(os.path.expanduser(args.input))
    out_dir = Path(os.path.expanduser(args.out_dir))
    out_dir.mkdir(parents=True, exist_ok=True)

    if not input_path.exists():
        sys.exit(f"ERROR: input file not found: {input_path}")

    # The packed CSV may contain raw 0xFF/0xFE corruption bytes AND uses
    # quoted multi-field cells.  Read with Python's csv module (encoding-safe)
    # rather than pandas, so quoting is respected and corrupt bytes are
    # replaced rather than raising a decode error.
    import csv as _csv
    import io

    raw_text = input_path.read_text(encoding="utf-8", errors="replace")
    reader = _csv.DictReader(io.StringIO(raw_text))
    rows_raw = list(reader)

    if not rows_raw or "IR_Samples" not in rows_raw[0] or "Red_Samples" not in rows_raw[0]:
        sys.exit("ERROR: expected columns IR_Samples and Red_Samples not found.")

    df = pd.DataFrame(rows_raw)

    print(f"Input: {input_path}")
    print(f"Rows : {len(df)}")
    print(f"Output dir: {out_dir}\n")

    summaries = []
    for idx, row in df.iterrows():
        run_num = idx + 1
        ir,  ir_bad  = parse_series(str(row.get("IR_Samples",  "")))
        red, red_bad = parse_series(str(row.get("Red_Samples", "")))

        ir_onset  = detect_corruption_onset(ir,  z_thresh=args.z_thresh)
        red_onset = detect_corruption_onset(red, z_thresh=args.z_thresh)

        score, verdict = quality_score(ir, red, ir_bad, red_bad)

        plot_path = out_dir / f"ppg_run_{run_num:02d}.png"
        plot_run(
            run_idx=run_num, ir=ir, red=red,
            ir_bad=ir_bad, red_bad=red_bad,
            ir_onset=ir_onset, red_onset=red_onset,
            score=score, verdict=verdict,
            out_path=plot_path, fs=args.fs
        )

        ir_onset_str  = str(ir_onset)  if ir_onset  is not None else "none"
        red_onset_str = str(red_onset) if red_onset is not None else "none"

        bpm = naive_bpm(ir, args.fs)
        summaries.append({
            "run":          run_num,
            "verdict":      verdict,
            "score":        score,
            "ir_valid":     len(ir),
            "red_valid":    len(red),
            "ir_bad":       ir_bad,
            "red_bad":      red_bad,
            "ir_corrupt_at": ir_onset_str,
            "red_corrupt_at": red_onset_str,
            "est_bpm_ir":   round(bpm, 1),
        })

        icon = {"LIKELY_PPG": "✓", "MIXED": "~", "UNRELIABLE": "✗"}[verdict]
        print(
            f"  {icon} Run {run_num:2d}: {verdict:<12s} score={score:3d}"
            f"  IR={len(ir):>4d} (bad={ir_bad})"
            f"  RED={len(red):>4d} (bad={red_bad})"
            f"  corr_onset=[IR:{ir_onset_str}, RED:{red_onset_str}]"
            f"  ~{bpm:.0f} BPM  → {plot_path.name}"
        )

    # Summary bar chart
    summary_path = out_dir / "ppg_run_overview.png"
    plot_summary(summaries, summary_path)
    print(f"\nOverview chart: {summary_path}")

    # Summary CSV
    csv_path = out_dir / "ppg_run_summary.csv"
    pd.DataFrame(summaries).to_csv(csv_path, index=False)
    print(f"Summary CSV  : {csv_path}")


if __name__ == "__main__":
    main()
