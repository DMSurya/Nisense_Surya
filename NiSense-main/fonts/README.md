# XIP Fonts

This directory contains TTF font files for XIP (Execute-In-Place) loading.

## Required Font

Download Montserrat Regular TTF from Google Fonts:
https://fonts.google.com/specimen/Montserrat

Place the file as: `Montserrat-Regular.ttf`

## Alternative: Quick Download

```powershell
# Download from Google Fonts static files
Invoke-WebRequest -Uri "https://github.com/JulietaUla/Montserrat/raw/master/fonts/ttf/Montserrat-Regular.ttf" -OutFile "Montserrat-Regular.ttf"
```

## File Size

Montserrat-Regular.ttf is approximately 200-250 KB.
This saves ~100 KB of internal flash compared to compiled LVGL fonts.

## Usage

After placing the TTF file here, run:
```powershell
python scripts/tools/convert_logos_to_resource.py
```

This will include the font in `build_sdk_v330/resource.bin`.
