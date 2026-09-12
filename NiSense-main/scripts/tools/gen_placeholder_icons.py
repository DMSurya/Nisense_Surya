#!/usr/bin/env python3
"""
Generate placeholder NiSense parameter icons.

Produces 40x40 RGBA PNGs (transparent background) with a simple branded glyph
per measurement parameter. These are PLACEHOLDERS meant to be replaced by the
final designed icons later — just drop real 40x40 PNGs over the same filenames.

The filename -> asset-ID mapping is consumed by convert_logos_to_resource.py.

Usage:
    python gen_placeholder_icons.py [--out-dir assets/icons] [--size 40]
"""

import argparse
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

# Brand palette (matches src/ui/ui_theme.h)
ACCENTS = {
    "heart":      (0xFF, 0x4A, 0x4A),
    "spo2":       (0x00, 0xA6, 0xFF),
    "glucose":    (0xFF, 0x9F, 0x1A),
    "hemoglobin": (0x8A, 0x2E, 0xFF),
    "temp":       (0x00, 0xD4, 0x6A),
    "resp":       (0x00, 0xC8, 0xC8),
    "insulin":    (0x00, 0xC8, 0xFF),
    "homa":       (0x1E, 0x4D, 0xFF),
    "battery":    (0x7E, 0xD3, 0x21),
    "bluetooth":  (0x1E, 0x4D, 0xFF),
    "wifi":       (0x00, 0xC8, 0xFF),
    "settings":   (0xA8, 0xB0, 0xD3),
    "warning":    (0xFF, 0xD6, 0x00),
    "check":      (0x00, 0xC8, 0x53),
    "battery_low":      (0xFF, 0xD6, 0x00),
    "battery_critical": (0xD5, 0x00, 0x00),
    "bluetooth_off":    (0xA8, 0xB0, 0xD3),
    "bluetooth_on":     (0x00, 0xC8, 0xFF),
    "wifi_off":         (0xA8, 0xB0, 0xD3),
    "wifi_on":          (0x00, 0xC8, 0xFF),
}

# Short glyph drawn inside each icon
GLYPHS = {
    "heart": "HR",
    "spo2": "O2",
    "glucose": "GL",
    "hemoglobin": "Hb",
    "temp": "T",
    "resp": "RR",
    "insulin": "In",
    "homa": "IR",
    "battery": "B",
    "bluetooth": "BT",
    "wifi": "Wi",
    "settings": "S",
    "warning": "!",
    "check": "OK",
    "battery_low": "B!",
    "battery_critical": "B!!",
    "bluetooth_off": "BTx",
    "bluetooth_on": "BT",
    "wifi_off": "WiX",
    "wifi_on": "Wi",
}


def _font(size: int) -> ImageFont.FreeTypeFont:
    root = Path(__file__).resolve().parents[2]
    for name in ("Montserrat-Regular.ttf", "Montserrat-Subset.ttf"):
        p = root / "fonts" / name
        if p.exists():
            try:
                return ImageFont.truetype(str(p), size)
            except Exception:
                pass
    return ImageFont.load_default()


def make_icon(name: str, accent, glyph: str, size: int) -> Image.Image:
    img = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)

    # Filled rounded circle in the accent color
    pad = 2
    d.ellipse([pad, pad, size - 1 - pad, size - 1 - pad],
              fill=(accent[0], accent[1], accent[2], 255))

    # White glyph centered
    fsize = size // 2 if len(glyph) <= 1 else int(size * 0.42)
    font = _font(fsize)
    try:
        bbox = d.textbbox((0, 0), glyph, font=font)
        tw, th = bbox[2] - bbox[0], bbox[3] - bbox[1]
        tx = (size - tw) / 2 - bbox[0]
        ty = (size - th) / 2 - bbox[1]
    except Exception:
        tw, th = d.textsize(glyph, font=font)
        tx, ty = (size - tw) / 2, (size - th) / 2
    d.text((tx, ty), glyph, font=font, fill=(255, 255, 255, 255))
    return img


def main() -> int:
    ap = argparse.ArgumentParser(description="Generate placeholder NiSense icons")
    root = Path(__file__).resolve().parents[2]
    ap.add_argument("--out-dir", type=Path, default=root / "assets" / "icons")
    ap.add_argument("--size", type=int, default=40)
    args = ap.parse_args()

    out_dir = args.out_dir.resolve()
    out_dir.mkdir(parents=True, exist_ok=True)

    for name, accent in ACCENTS.items():
        glyph = GLYPHS.get(name, name[:2].upper())
        img = make_icon(name, accent, glyph, args.size)
        path = out_dir / f"{name}.png"
        img.save(path)
        print(f"  wrote {path.relative_to(root)}")

    print(f"\nGenerated {len(ACCENTS)} placeholder icons in {out_dir}")
    print("Replace these PNGs with final designs (same names, 40x40 RGBA).")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
