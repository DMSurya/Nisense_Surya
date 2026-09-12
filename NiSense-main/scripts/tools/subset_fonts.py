#!/usr/bin/env python3
"""
Font Subset Tool for NiSense HCM

Subsets TTF fonts to include only required characters, reducing file size
for XIP flash storage.

Full Montserrat-Regular.ttf: ~446 KB
Subsetted (ASCII only):      ~50-80 KB

Requirements:
    pip install fonttools brotli zopfli

Usage:
    python subset_fonts.py [--input INPUT] [--output OUTPUT] [--charset CHARSET]

Author: AI Agent
Created: 2026-02-24
"""

import argparse
import os
import subprocess
import sys
from pathlib import Path


# =============================================================================
# Character Sets
# =============================================================================

# ASCII printable characters (space to tilde)
CHARSET_ASCII = "U+0020-007F"

# Extended ASCII with common symbols and accents
CHARSET_EXTENDED = "U+0020-007F,U+00A0-00FF"

# Numbers and basic punctuation only (for numeric displays)
CHARSET_NUMBERS = "U+0020-003A,U+002D,U+002E,U+0025"

# Custom HCM charset: ASCII + degree symbol + common medical symbols
CHARSET_HCM = "U+0020-007F,U+00B0,U+00B1,U+00B5,U+2103,U+2109"


# =============================================================================
# Font Subsetting
# =============================================================================

def check_fonttools():
    """Check if fonttools is installed"""
    try:
        import fontTools
        return True
    except ImportError:
        return False


def install_fonttools():
    """Attempt to install fonttools"""
    print("Installing fonttools...")
    try:
        subprocess.check_call([sys.executable, "-m", "pip", "install", 
                               "fonttools", "brotli", "zopfli", "-q"])
        return True
    except subprocess.CalledProcessError:
        return False


def parse_unicode_spec(spec: str) -> set:
    """
    Parse a unicode specification string into a set of integer codepoints.
    
    Examples:
        "U+0020-007F" -> {0x20, 0x21, ..., 0x7F}
        "U+0020-007F,U+00B0" -> {0x20, ..., 0x7F, 0xB0}
    """
    codepoints = set()
    for part in spec.split(','):
        part = part.strip()
        if '-' in part:
            # Range: U+0020-007F
            start, end = part.split('-')
            start = int(start.replace('U+', ''), 16)
            end = int(end.replace('U+', ''), 16)
            codepoints.update(range(start, end + 1))
        else:
            # Single: U+00B0
            codepoints.add(int(part.replace('U+', ''), 16))
    return codepoints


def subset_font(input_path: Path, output_path: Path, unicodes: str, 
                flavor: str = None) -> bool:
    """
    Subset a TTF font to include only specified Unicode ranges.
    
    Args:
        input_path: Path to input TTF file
        output_path: Path for output TTF file
        unicodes: Unicode range string (e.g., "U+0020-007F")
        flavor: Output format ('woff', 'woff2', or None for TTF)
    
    Returns:
        True on success, False on failure
    """
    try:
        from fontTools import subset
        from fontTools.ttLib import TTFont
    except ImportError:
        print("Error: fonttools not installed. Run: pip install fonttools")
        return False
    
    print(f"Subsetting {input_path.name}...")
    print(f"  Unicode range: {unicodes}")
    
    # Parse unicode ranges to integer codepoints
    codepoint_set = parse_unicode_spec(unicodes)
    
    # Build subsetter options
    options = subset.Options()
    options.layout_features = ['*']  # Keep all OpenType features
    options.name_IDs = ['*']         # Keep all name records
    options.name_legacy = True
    options.name_languages = ['*']
    options.notdef_outline = True
    options.recalc_bounds = True
    options.recalc_timestamp = True
    options.canonical_order = True
    
    if flavor:
        options.flavor = flavor
    
    # Load font
    font = TTFont(str(input_path))
    
    # Get original glyph count
    orig_glyphs = len(font.getGlyphOrder())
    orig_size = input_path.stat().st_size
    
    # Subset with integer codepoints
    subsetter = subset.Subsetter(options=options)
    subsetter.populate(unicodes=codepoint_set)
    subsetter.subset(font)
    
    # Get new glyph count
    new_glyphs = len(font.getGlyphOrder())
    
    # Save
    font.save(str(output_path))
    font.close()
    
    # Report
    new_size = output_path.stat().st_size
    reduction = (1 - new_size / orig_size) * 100
    
    print(f"  Input:  {orig_size:,} bytes ({orig_glyphs} glyphs)")
    print(f"  Output: {new_size:,} bytes ({new_glyphs} glyphs)")
    print(f"  Reduction: {reduction:.1f}%")
    print(f"  Saved to: {output_path}")
    
    return True


def main():
    parser = argparse.ArgumentParser(
        description='Subset TTF fonts for XIP flash storage'
    )
    parser.add_argument(
        '--input', '-i',
        type=Path,
        default=None,
        help='Input TTF file (default: fonts/Montserrat-Regular.ttf)'
    )
    parser.add_argument(
        '--output', '-o',
        type=Path,
        default=None,
        help='Output TTF file (default: fonts/Montserrat-Subset.ttf)'
    )
    parser.add_argument(
        '--charset', '-c',
        type=str,
        choices=['ascii', 'extended', 'numbers', 'hcm'],
        default='hcm',
        help='Character set to include (default: hcm)'
    )
    parser.add_argument(
        '--unicodes', '-u',
        type=str,
        default=None,
        help='Custom Unicode range (e.g., "U+0020-007F,U+00B0")'
    )
    parser.add_argument(
        '--force', '-f',
        action='store_true',
        help='Force regeneration even if output exists and is newer'
    )
    
    args = parser.parse_args()
    
    # Resolve paths
    project_root = Path(__file__).parent.parent.parent
    fonts_dir = project_root / 'fonts'
    
    if args.input is None:
        input_path = fonts_dir / 'Montserrat-Regular.ttf'
    else:
        input_path = args.input.resolve()
    
    if args.output is None:
        output_path = fonts_dir / 'Montserrat-Subset.ttf'
    else:
        output_path = args.output.resolve()
    
    # Check input exists
    if not input_path.exists():
        print(f"Error: Input file not found: {input_path}")
        print("\nDownload Montserrat font:")
        print("  Invoke-WebRequest -Uri 'https://github.com/JulietaUla/Montserrat/raw/master/fonts/ttf/Montserrat-Regular.ttf' -OutFile fonts/Montserrat-Regular.ttf")
        return 1
    
    # Check if regeneration needed
    if output_path.exists() and not args.force:
        if output_path.stat().st_mtime > input_path.stat().st_mtime:
            print(f"Output is up-to-date: {output_path}")
            print(f"  Size: {output_path.stat().st_size:,} bytes")
            print("Use --force to regenerate")
            return 0
    
    # Check fonttools
    if not check_fonttools():
        if not install_fonttools():
            print("Error: Failed to install fonttools")
            print("Please install manually: pip install fonttools brotli")
            return 1
    
    # Select charset
    if args.unicodes:
        unicodes = args.unicodes
    else:
        charset_map = {
            'ascii': CHARSET_ASCII,
            'extended': CHARSET_EXTENDED,
            'numbers': CHARSET_NUMBERS,
            'hcm': CHARSET_HCM,
        }
        unicodes = charset_map[args.charset]
    
    print("=" * 60)
    print("Font Subset Tool")
    print("=" * 60)
    print(f"Input:   {input_path}")
    print(f"Output:  {output_path}")
    print(f"Charset: {args.charset if not args.unicodes else 'custom'}")
    print()
    
    # Subset
    if not subset_font(input_path, output_path, unicodes):
        return 1
    
    print()
    print("=" * 60)
    print("Done!")
    print("=" * 60)
    
    return 0


if __name__ == '__main__':
    sys.exit(main())
