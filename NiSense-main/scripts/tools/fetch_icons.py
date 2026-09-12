#!/usr/bin/env python3
"""
Icon fetch + tune pipeline for the NiSense HCM ecosystem.

Downloads a curated, semantically-matched icon set from the Lucide project
(https://lucide.dev, ISC license) and tunes each icon to the NiSense brand
palette, producing assets for BOTH user interfaces from a single source:

  * App  (Qt/QML) : recoloured SVGs -> apps/pc/qml/assets/icons/*.svg
                    Qt renders SVG natively, so these stay crisp at any size.
                    Navigation icons are emitted in two tints (muted/active).
  * Firmware (LVGL): 40x40 RGBA PNGs -> assets/icons/*.png
                    These feed the existing Resource store pipeline
                    (scripts/tools/convert_logos_to_resource.py), which bakes them
                    into external flash. Filenames match the IDs in
                    include/resource.h, so no firmware code changes are
                    required to pick up the new artwork.

The colour for each parameter/state mirrors src/ui/ui_theme.h (NS_RGB_*) and
apps/pc/qml/Theme.qml, so the watch and the desktop app share one identity.

Originals in assets/icons/ are backed up to assets/icons/_backup_pre_lucide/
before being overwritten (safe + reversible).

Usage:
    python scripts/tools/fetch_icons.py              # fetch + generate all
    python scripts/tools/fetch_icons.py --app-only   # only QML SVGs
    python scripts/tools/fetch_icons.py --fw-only     # only firmware PNGs
    python scripts/tools/fetch_icons.py --no-download # use cached SVGs only

@author Ponmadasamy Muthuraj <ponmadasamy@live.com>
"""

from __future__ import annotations

import argparse
import shutil
import sys
import urllib.request
from pathlib import Path
from typing import Optional

# --------------------------------------------------------------------------
# Paths
# --------------------------------------------------------------------------
THIS_DIR = Path(__file__).resolve().parent
REPO_ROOT = THIS_DIR.parent.parent
CACHE_DIR = THIS_DIR / ".icon_cache"
APP_ICON_DIR = REPO_ROOT / "apps" / "pc" / "qml" / "assets" / "icons"
FW_ICON_DIR = REPO_ROOT / "assets" / "icons"
FW_BACKUP_DIR = FW_ICON_DIR / "_backup_pre_lucide"

LUCIDE_RAW = "https://raw.githubusercontent.com/lucide-icons/lucide/main/icons/{name}.svg"
FW_ICON_PX = 40  # firmware icons are 40x40 (matches existing Resource asset table)

# --------------------------------------------------------------------------
# Brand palette (kept in sync with src/ui/ui_theme.h NS_RGB_* + Theme.qml)
# --------------------------------------------------------------------------
C_HR        = "#FF4A4A"   # heart rate
C_SPO2      = "#00A6FF"   # SpO2
C_GLUCOSE   = "#FF9F1A"   # glucose
C_TEMP      = "#00D46A"   # temperature
C_HB        = "#8A2EFF"   # hemoglobin
C_RESP      = "#00C8C8"   # respiration
C_HRV       = "#4DD2FF"   # HRV
C_INSULIN   = "#2D7FFF"   # insulin
C_HOMA      = "#4DD2FF"   # HOMA-IR
C_INFO      = "#00C8FF"   # info / connectivity neutral
C_NORMAL    = "#00C853"   # ok / connected
C_WARNING   = "#FFD600"   # attention
C_CRITICAL  = "#D50000"   # critical
C_NEUTRAL   = "#A8B0D3"   # dimmed text / inactive state

# App navigation tints (match main.qml: inactive slate, active light-blue)
NAV_MUTED   = "#64748B"
NAV_ACTIVE  = "#93C5FD"

# --------------------------------------------------------------------------
# Icon specs
#
# Each parameter/state maps to one or more candidate Lucide names (first that
# downloads wins, so the script survives upstream icon renames) plus the brand
# colour to tint it with.
# --------------------------------------------------------------------------
# (output_name, [lucide candidates], colour)
PARAM_ICONS = [
    ("heart",              ["heart-pulse", "heart"],          C_HR),
    ("spo2",               ["activity", "waves"],             C_SPO2),
    ("glucose",            ["droplet", "droplets"],           C_GLUCOSE),
    ("temp",               ["thermometer"],                   C_TEMP),
    ("hemoglobin",         ["droplets", "droplet"],           C_HB),
    ("resp",               ["wind"],                          C_RESP),
    ("insulin",            ["syringe"],                       C_INSULIN),
    ("homa",               ["sigma", "calculator"],           C_HOMA),
    ("battery",            ["battery-medium", "battery"],     C_NORMAL),
    ("battery_low",        ["battery-low", "battery"],        C_WARNING),
    ("battery_critical",   ["battery-warning", "battery"],    C_CRITICAL),
    ("bluetooth",          ["bluetooth"],                     C_INFO),
    ("bluetooth_on",       ["bluetooth-connected"],           C_NORMAL),
    ("bluetooth_off",      ["bluetooth-off"],                 C_NEUTRAL),
    ("wifi",               ["wifi"],                          C_INFO),
    ("wifi_on",            ["wifi"],                          C_NORMAL),
    ("wifi_off",           ["wifi-off"],                      C_NEUTRAL),
    ("settings",           ["settings"],                      C_NEUTRAL),
    ("warning",            ["triangle-alert", "alert-triangle"], C_WARNING),
    ("check",              ["circle-check", "check-circle"],  C_NORMAL),
]

# Navigation icons (app only) — emitted twice, muted + active.
# (output_name, [lucide candidates])
NAV_ICONS = [
    ("scan",      ["radar", "scan-line", "search"]),
    ("dashboard", ["layout-dashboard", "gauge"]),
    ("charts",    ["chart-line", "line-chart", "activity"]),
    ("settings",  ["settings"]),
    ("services",  ["list-tree", "network", "share-2"]),
    ("logs",      ["scroll-text", "file-text"]),
    ("firmware",  ["cpu", "hard-drive-download"]),
]


# --------------------------------------------------------------------------
# Download + tune helpers
# --------------------------------------------------------------------------
def fetch_svg(candidates: list[str], allow_download: bool) -> Optional[tuple[str, str]]:
    """Return (lucide_name, raw_svg_text) for the first candidate available.

    Cached under CACHE_DIR so repeat runs are offline-friendly.
    """
    CACHE_DIR.mkdir(parents=True, exist_ok=True)
    for name in candidates:
        cached = CACHE_DIR / f"{name}.svg"
        if cached.exists():
            return name, cached.read_text(encoding="utf-8")
        if not allow_download:
            continue
        url = LUCIDE_RAW.format(name=name)
        try:
            with urllib.request.urlopen(url, timeout=20) as resp:
                if resp.status != 200:
                    continue
                text = resp.read().decode("utf-8")
            cached.write_text(text, encoding="utf-8")
            return name, text
        except Exception as exc:  # try next candidate
            print(f"   ! download failed for '{name}': {exc}")
            continue
    return None


def tint_svg(svg: str, color: str, stroke_width: Optional[float] = None) -> str:
    """Recolour a Lucide outline SVG to a solid brand colour.

    Lucide icons draw with stroke="currentColor" fill="none"; we substitute the
    stroke colour and, optionally, bump the stroke width for legibility at small
    sizes on the 240x240 watch panel.
    """
    out = svg.replace('stroke="currentColor"', f'stroke="{color}"')
    # Some icons also use fill="currentColor"; respect that.
    out = out.replace('fill="currentColor"', f'fill="{color}"')
    if stroke_width is not None:
        out = out.replace('stroke-width="2"', f'stroke-width="{stroke_width}"')
    return out


def rasterize_png(svg: str, px: int, dest: Path) -> bool:
    """Rasterize tuned SVG to a transparent RGBA PNG. cairosvg > ImageMagick."""
    dest.parent.mkdir(parents=True, exist_ok=True)
    # Preferred: cairosvg (crisp, proper alpha)
    try:
        import cairosvg  # type: ignore
        cairosvg.svg2png(
            bytestring=svg.encode("utf-8"),
            write_to=str(dest),
            output_width=px,
            output_height=px,
            background_color="transparent",
        )
        return True
    except Exception:
        pass
    # Fallback: ImageMagick `convert`
    try:
        import subprocess
        import tempfile
        with tempfile.NamedTemporaryFile("w", suffix=".svg", delete=False) as tf:
            tf.write(svg)
            tmp = tf.name
        subprocess.run(
            ["convert", "-background", "none", "-density", "300",
             "-resize", f"{px}x{px}", tmp, str(dest)],
            check=True, capture_output=True,
        )
        Path(tmp).unlink(missing_ok=True)
        return True
    except Exception as exc:
        print(f"   ! rasterize failed for {dest.name}: {exc}")
        return False


def verify_png(path: Path) -> bool:
    """Confirm a generated PNG is the right size and not fully empty."""
    try:
        from PIL import Image
        with Image.open(path) as im:
            ok = im.size == (FW_ICON_PX, FW_ICON_PX) and im.getbbox() is not None
            if not ok:
                print(f"   ! {path.name}: size={im.size} empty={im.getbbox() is None}")
            return ok
    except Exception as exc:
        print(f"   ! verify failed for {path.name}: {exc}")
        return False


# --------------------------------------------------------------------------
# Generation
# --------------------------------------------------------------------------
def backup_firmware_icons() -> None:
    if not FW_ICON_DIR.exists():
        return
    FW_BACKUP_DIR.mkdir(parents=True, exist_ok=True)
    for png in FW_ICON_DIR.glob("*.png"):
        target = FW_BACKUP_DIR / png.name
        if not target.exists():
            shutil.copy2(png, target)
    print(f"[backup] originals -> {FW_BACKUP_DIR.relative_to(REPO_ROOT)}")


def write_license_note(folder: Path) -> None:
    folder.mkdir(parents=True, exist_ok=True)
    (folder / "ICONS_LICENSE.txt").write_text(
        "Icons derived from Lucide (https://lucide.dev) — ISC License.\n"
        "Recoloured to the NiSense brand palette via scripts/tools/fetch_icons.py.\n"
        "Lucide is a community fork of Feather Icons (MIT).\n",
        encoding="utf-8",
    )


def generate(do_app: bool, do_fw: bool, allow_download: bool) -> int:
    if do_fw:
        backup_firmware_icons()
        write_license_note(FW_ICON_DIR)
    if do_app:
        write_license_note(APP_ICON_DIR)

    failures = 0

    print("\n[parameter + state icons]")
    for out_name, candidates, color in PARAM_ICONS:
        got = fetch_svg(candidates, allow_download)
        if not got:
            print(f"  ✗ {out_name}: no source icon found ({candidates})")
            failures += 1
            continue
        lucide_name, svg = got
        tuned = tint_svg(svg, color, stroke_width=2.1)

        if do_app:
            dest = APP_ICON_DIR / f"{out_name}.svg"
            dest.parent.mkdir(parents=True, exist_ok=True)
            dest.write_text(tuned, encoding="utf-8")
        if do_fw:
            dest = FW_ICON_DIR / f"{out_name}.png"
            if rasterize_png(tuned, FW_ICON_PX, dest):
                if not verify_png(dest):
                    failures += 1
            else:
                failures += 1
        print(f"  ✓ {out_name:<18} <- {lucide_name:<18} {color}")

    if do_app:
        print("\n[navigation icons — app, muted + active]")
        for out_name, candidates in NAV_ICONS:
            got = fetch_svg(candidates, allow_download)
            if not got:
                print(f"  ✗ {out_name}: no source icon found ({candidates})")
                failures += 1
                continue
            lucide_name, svg = got
            (APP_ICON_DIR / f"nav_{out_name}.svg").write_text(
                tint_svg(svg, NAV_MUTED, stroke_width=2.0), encoding="utf-8")
            (APP_ICON_DIR / f"nav_{out_name}_active.svg").write_text(
                tint_svg(svg, NAV_ACTIVE, stroke_width=2.0), encoding="utf-8")
            print(f"  ✓ nav_{out_name:<14} <- {lucide_name}")

    print(f"\nDone. {failures} failure(s).")
    if do_app:
        print(f"  App SVGs : {APP_ICON_DIR.relative_to(REPO_ROOT)}")
    if do_fw:
        print(f"  FW  PNGs : {FW_ICON_DIR.relative_to(REPO_ROOT)}  "
              f"(run scripts/tools/convert_logos_to_resource.py to bake into XIP)")
    return 1 if failures else 0


def main() -> int:
    ap = argparse.ArgumentParser(description="Fetch + tune NiSense UI icons (Lucide).")
    ap.add_argument("--app-only", action="store_true", help="generate only QML SVGs")
    ap.add_argument("--fw-only", action="store_true", help="generate only firmware PNGs")
    ap.add_argument("--no-download", action="store_true", help="use cached SVGs only")
    args = ap.parse_args()

    do_app = not args.fw_only
    do_fw = not args.app_only
    return generate(do_app, do_fw, allow_download=not args.no_download)


if __name__ == "__main__":
    sys.exit(main())
