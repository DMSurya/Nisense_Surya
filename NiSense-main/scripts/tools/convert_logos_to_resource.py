#!/usr/bin/env python3
"""
Resource store Conversion Tool for NiSense HCM

Converts LVGL C source file (logo.c) to Resource binary format
for storage in external QSPI flash.

Output: resource.bin for programming via J-Link/Ozone

Binary Format:
  - resource_partition_header (256 bytes)
  - resource_entry[] (32 bytes each)
  - For each asset: resource_image_header (16 bytes) + pixel_data

Usage:
    python convert_logos_to_resource.py [--src-dir SRC_DIR] [--output OUTPUT]

Author: AI Agent
Created: 2026-02-14
"""

import argparse
import os
import re
import struct
import sys
import zlib
from pathlib import Path
from dataclasses import dataclass
from typing import List, Optional, Tuple
import time

# =============================================================================
# Constants (must match resource.h)
# =============================================================================

RESOURCE_PARTITION_MAGIC = 0x41504958  # "XIPA"
RESOURCE_ASSET_MAGIC = 0x54534158      # "XAST"
RESOURCE_PARTITION_VERSION = 1
RESOURCE_ASSET_VERSION = 1

# Asset types (from resource.h)
RESOURCE_TYPE_LOGO = 0x01
RESOURCE_TYPE_ICON = 0x02
RESOURCE_TYPE_FONT = 0x03
RESOURCE_TYPE_DATA = 0x06

# Image formats
RESOURCE_IMAGE_FORMAT_RGB565 = 0x00
RESOURCE_IMAGE_FORMAT_ARGB8888 = 0x02

# Asset IDs (from resource.h)
RESOURCE_ID_LOGO_BOOT = 0x0001
RESOURCE_ID_LOGO_CHARGE = 0x0002
RESOURCE_ID_LOGO_SHUTDOWN = 0x0003
RESOURCE_ID_LOGO = RESOURCE_ID_LOGO_BOOT  # alias for logo.c packing
RESOURCE_ID_FONT_SMALL = 0x0200   # Montserrat TTF font

# PNG logos under assets/logo/ (RGB565, full-screen brand art)
LOGO_PNG_ASSET_IDS = {
    "charge": RESOURCE_ID_LOGO_CHARGE,
    "logo_charge": RESOURCE_ID_LOGO_CHARGE,
    "shutdown": RESOURCE_ID_LOGO_SHUTDOWN,
    "logo_shutdown": RESOURCE_ID_LOGO_SHUTDOWN,
}

# Icon filename -> asset ID map (from resource.h, 0x0100+ range)
ICON_ASSET_IDS = {
    "heart":      0x0100,
    "spo2":       0x0101,
    "glucose":    0x0102,
    "temp":       0x0103,
    "battery":    0x0104,
    "bluetooth":  0x0105,
    "wifi":       0x0106,
    "settings":   0x0107,
    "warning":    0x0108,
    "check":      0x0109,
    "hemoglobin": 0x010A,
    "resp":       0x010B,
    "insulin":    0x010C,
    "homa":       0x010D,
    "battery_low":      0x0110,
    "battery_critical": 0x0111,
    "bluetooth_off":    0x0112,
    "bluetooth_on":     0x0113,
    "wifi_off":         0x0114,
    "wifi_on":          0x0115,
}

# Structure sizes
PARTITION_HEADER_SIZE = 256
ASSET_ENTRY_SIZE = 32
IMAGE_HEADER_SIZE = 16


# =============================================================================
# Data Structures
# =============================================================================

@dataclass
class ImageAsset:
    """Parsed image asset from LVGL C source"""
    name: str
    asset_id: int
    width: int
    height: int
    format: int
    bits_per_pixel: int
    pixel_data: bytes


@dataclass
class FontAsset:
    """Raw TTF font asset"""
    name: str
    asset_id: int
    font_data: bytes


# =============================================================================
# LVGL C Source Parser
# =============================================================================

def parse_lvgl_c_source(filepath: Path) -> Optional[ImageAsset]:
    """
    Parse an LVGL image C source file to extract:
    - Array name (e.g., logo_map)
    - Pixel data (hex bytes)
    - Width and height from lv_image_dsc_t
    - Color format
    """
    print(f"Parsing {filepath.name}...")
    
    with open(filepath, 'r', encoding='utf-8', errors='ignore') as f:
        content = f.read()
    
    # Extract array name from variable declaration
    # Pattern: const LV_ATTRIBUTE_LARGE_CONST uint8_t logo_map[] = {
    array_match = re.search(
        r'(?:const\s+)?(?:\w+\s+)*uint8_t\s+(\w+_map)\s*\[\s*\]\s*=\s*\{',
        content,
        re.MULTILINE
    )
    if not array_match:
        print(f"  Warning: Could not find pixel array in {filepath.name}")
        return None
    
    array_name = array_match.group(1)
    print(f"  Found array: {array_name}")
    
    # Find the array start and end
    array_start = array_match.end()
    
    # Find matching closing brace
    brace_count = 1
    pos = array_start
    while pos < len(content) and brace_count > 0:
        if content[pos] == '{':
            brace_count += 1
        elif content[pos] == '}':
            brace_count -= 1
        pos += 1
    
    array_content = content[array_start:pos-1]
    
    # Extract all hex bytes from the array
    hex_pattern = re.compile(r'0x([0-9a-fA-F]{2})')
    hex_matches = hex_pattern.findall(array_content)
    
    if not hex_matches:
        print(f"  Warning: No pixel data found in {filepath.name}")
        return None
    
    pixel_data = bytes(int(h, 16) for h in hex_matches)
    print(f"  Extracted {len(pixel_data)} bytes of pixel data")
    
    # Extract lv_image_dsc_t structure for dimensions
    dsc_match = re.search(
        r'const\s+lv_image_dsc_t\s+(\w+)\s*=\s*\{[^}]*'
        r'\.header\.w\s*=\s*(\d+)\s*,'
        r'[^}]*\.header\.h\s*=\s*(\d+)',
        content,
        re.DOTALL
    )
    
    if dsc_match:
        var_name = dsc_match.group(1)
        width = int(dsc_match.group(2))
        height = int(dsc_match.group(3))
        print(f"  Dimensions: {width}x{height}")
    else:
        # Fallback: try to extract from data_size
        # data_size = width * height * 2 for RGB565
        size_match = re.search(r'\.data_size\s*=\s*(\d+)\s*\*\s*2', content)
        if size_match:
            total_pixels = int(size_match.group(1))
            # Assume square image
            import math
            width = height = int(math.sqrt(total_pixels))
            print(f"  Inferred dimensions: {width}x{height}")
        else:
            print(f"  Warning: Could not determine dimensions for {filepath.name}")
            return None
    
    # Determine asset ID from filename
    base_name = filepath.stem
    if base_name == 'logo' or 'logo' in base_name:
        asset_id = RESOURCE_ID_LOGO
    else:
        # Generate ID from hash for unknown assets
        asset_id = hash(base_name) & 0xFFFF
    
    # Verify data size
    expected_size = width * height * 2  # RGB565 = 2 bytes/pixel
    if len(pixel_data) != expected_size:
        print(f"  Warning: Data size mismatch. Expected {expected_size}, got {len(pixel_data)}")
        # Pad or truncate if needed
        if len(pixel_data) < expected_size:
            pixel_data += b'\x00' * (expected_size - len(pixel_data))
        else:
            pixel_data = pixel_data[:expected_size]
    
    return ImageAsset(
        name=base_name,
        asset_id=asset_id,
        width=width,
        height=height,
        format=RESOURCE_IMAGE_FORMAT_RGB565,
        bits_per_pixel=16,
        pixel_data=pixel_data
    )


def load_ttf_font(filepath: Path) -> Optional[FontAsset]:
    """
    Load a TTF font file for Resource store.
    
    @param filepath: Path to the .ttf file
    @return: FontAsset with raw TTF data, or None if failed
    """
    print(f"Loading TTF font: {filepath.name}...")
    
    if not filepath.exists():
        print(f"  Warning: File not found: {filepath}")
        return None
    
    with open(filepath, 'rb') as f:
        font_data = f.read()
    
    # Verify TTF magic (first 4 bytes)
    # TTF files start with 0x00010000 (TrueType) or 'OTTO' (OpenType/CFF)
    if len(font_data) < 4:
        print(f"  Warning: File too small to be a TTF: {len(font_data)} bytes")
        return None
    
    magic = font_data[:4]
    if magic == b'\x00\x01\x00\x00':
        print(f"  Font type: TrueType")
    elif magic == b'OTTO':
        print(f"  Font type: OpenType/CFF")
    elif magic == b'true' or magic == b'typ1':
        print(f"  Font type: TrueType variant")
    else:
        print(f"  Warning: Unknown font magic: {magic.hex()}")
        # Still proceed, TinyTTF may handle it
    
    base_name = filepath.stem
    print(f"  Font size: {len(font_data)} bytes ({len(font_data)/1024:.1f} KB)")
    
    return FontAsset(
        name=base_name,
        asset_id=RESOURCE_ID_FONT_SMALL,
        font_data=font_data
    )


def load_png_icon(filepath: Path) -> Optional[ImageAsset]:
    """
    Load a PNG icon and convert to ARGB8888 for Resource store.

    Pixel byte order matches LVGL's ARGB8888 in-memory layout on little-endian:
    B, G, R, A per pixel. Transparent backgrounds are preserved via the alpha
    channel so icons blend onto any card color.

    @param filepath: Path to the .png file (filename stem must be in ICON_ASSET_IDS)
    @return: ImageAsset (ARGB8888) or None if unmapped/unloadable
    """
    base_name = filepath.stem.lower()
    asset_id = ICON_ASSET_IDS.get(base_name)
    if asset_id is None:
        print(f"  Skipping {filepath.name}: no asset ID mapping for '{base_name}'")
        return None

    try:
        from PIL import Image
    except ImportError:
        print("  ERROR: Pillow (PIL) is required for PNG icons. pip install pillow")
        return None

    print(f"Loading PNG icon: {filepath.name} (id=0x{asset_id:04X})...")
    img = Image.open(filepath).convert("RGBA")
    width, height = img.size

    # Convert RGBA -> BGRA byte order for LVGL ARGB8888
    px = img.tobytes()  # RGBA, 4 bytes/pixel
    out = bytearray(len(px))
    out[0::4] = px[2::4]  # B
    out[1::4] = px[1::4]  # G
    out[2::4] = px[0::4]  # R
    out[3::4] = px[3::4]  # A

    print(f"  Dimensions: {width}x{height}, ARGB8888 ({len(out)} bytes)")
    return ImageAsset(
        name=base_name,
        asset_id=asset_id,
        width=width,
        height=height,
        format=RESOURCE_IMAGE_FORMAT_ARGB8888,
        bits_per_pixel=32,
        pixel_data=bytes(out),
    )


def load_png_logo(filepath: Path) -> Optional[ImageAsset]:
    """
    Load a PNG brand logo and convert to RGB565 for Resource store.

    Resized to 240x240 to match the display. Stem must be in LOGO_PNG_ASSET_IDS.
    """
    base_name = filepath.stem.lower()
    asset_id = LOGO_PNG_ASSET_IDS.get(base_name)
    if asset_id is None:
        print(f"  Skipping logo PNG {filepath.name}: no LOGO_PNG_ASSET_IDS entry")
        return None

    try:
        from PIL import Image
    except ImportError:
        print("  ERROR: Pillow (PIL) is required for PNG logos. pip install pillow")
        return None

    print(f"Loading PNG logo: {filepath.name} (id=0x{asset_id:04X})...")
    img = Image.open(filepath).convert("RGB")
    if img.size != (240, 240):
        print(f"  Resizing {img.size[0]}x{img.size[1]} -> 240x240")
        img = img.resize((240, 240), Image.Resampling.LANCZOS)

    width, height = img.size
    px = img.tobytes()  # RGB888
    out = bytearray(width * height * 2)
    for i in range(width * height):
        r = px[i * 3]
        g = px[i * 3 + 1]
        b = px[i * 3 + 2]
        rgb565 = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)
        out[i * 2] = rgb565 & 0xFF
        out[i * 2 + 1] = (rgb565 >> 8) & 0xFF

    print(f"  Dimensions: {width}x{height}, RGB565 ({len(out)} bytes)")
    return ImageAsset(
        name=base_name,
        asset_id=asset_id,
        width=width,
        height=height,
        format=RESOURCE_IMAGE_FORMAT_RGB565,
        bits_per_pixel=16,
        pixel_data=bytes(out),
    )


# =============================================================================
# Resource Binary Generation
# =============================================================================

def create_resource_partition_header(
    total_size: int,
    asset_count: int,
    data_for_crc: bytes,
    build_id: str = ""
) -> bytes:
    """Create the 256-byte partition header"""
    
    # Calculate CRC32 of all data after header
    crc32 = zlib.crc32(data_for_crc) & 0xFFFFFFFF
    
    # Unix timestamp
    timestamp = int(time.time())
    
    # Build ID (max 32 chars)
    build_id_bytes = build_id.encode('utf-8')[:31] + b'\x00'
    build_id_bytes = build_id_bytes.ljust(32, b'\x00')
    
    # Pack header fields
    header = struct.pack(
        '<IIIIII',          # 6 x uint32_t = 24 bytes
        RESOURCE_PARTITION_MAGIC,
        RESOURCE_PARTITION_VERSION,
        total_size,
        crc32,
        asset_count,
        timestamp
    )
    
    # Add build_id (32 bytes)
    header += build_id_bytes
    
    # Add reserved bytes to reach 256
    reserved_size = PARTITION_HEADER_SIZE - len(header)
    header += b'\x00' * reserved_size
    
    assert len(header) == PARTITION_HEADER_SIZE
    return header


def create_resource_entry(
    asset: ImageAsset,
    offset: int,
    data_size: int
) -> bytes:
    """Create a 32-byte asset table entry"""
    
    # Calculate CRC32 of asset data (image header + pixels)
    # This will be calculated later with actual data
    crc32 = 0  # Placeholder, calculated later
    
    entry = struct.pack(
        '<IIIIHBBHH',       # 4I + H + 2B + 2H = 16 + 2 + 2 + 4 = 24 bytes
        RESOURCE_ASSET_MAGIC,
        offset,
        data_size,
        crc32,
        asset.asset_id,
        RESOURCE_TYPE_LOGO,
        asset.format,
        asset.width,
        asset.height
    )
    
    # Add reserved bytes (8 bytes to reach 32)
    reserved_size = ASSET_ENTRY_SIZE - len(entry)
    entry += b'\x00' * reserved_size
    
    assert len(entry) == ASSET_ENTRY_SIZE
    return entry


def create_resource_image_header(asset: ImageAsset) -> bytes:
    """Create the 16-byte image header embedded in asset data"""
    
    stride = asset.width * (asset.bits_per_pixel // 8)
    data_size = len(asset.pixel_data)
    
    header = struct.pack(
        '<HHBBHII',         # 2H + 2B + H + 2I = 4 + 2 + 2 + 8 = 16 bytes
        asset.width,
        asset.height,
        asset.format,
        asset.bits_per_pixel,
        stride,
        data_size,
        0                   # palette_entries (0 for RGB565)
    )
    
    assert len(header) == IMAGE_HEADER_SIZE
    return header


def create_resource_binary(assets: list, build_id: str = "") -> bytes:
    """
    Create complete Resource binary from list of assets (ImageAsset or FontAsset)
    
    Layout:
        [0x000]     Partition header (256 bytes)
        [0x100]     Asset entry table (32 bytes × N)
        [0x100+N*32] Asset 0: data
        [...]       Asset 1: data
        ...
    
    For ImageAsset: image header (16 bytes) + pixel data
    For FontAsset: raw TTF data (no header)
    """
    
    if not assets:
        print("Error: No assets to convert")
        return bytes()
    
    asset_count = len(assets)
    
    # Calculate offsets
    header_offset = 0
    table_offset = PARTITION_HEADER_SIZE
    data_offset = table_offset + (ASSET_ENTRY_SIZE * asset_count)
    
    # Align data to 4-byte boundary
    if data_offset % 4:
        data_offset = (data_offset + 3) & ~3
    
    print(f"\nBinary layout:")
    print(f"  Partition header: 0x{header_offset:04X} - 0x{PARTITION_HEADER_SIZE-1:04X}")
    print(f"  Asset table:      0x{table_offset:04X} - 0x{data_offset-1:04X}")
    print(f"  Asset data start: 0x{data_offset:04X}")
    
    # Build asset data and entries
    asset_entries = []
    asset_data_blocks = []
    current_offset = data_offset
    
    for asset in assets:
        if isinstance(asset, ImageAsset):
            # Image asset: header + pixel data
            img_header = create_resource_image_header(asset)
            asset_data = img_header + asset.pixel_data
            # Icons live in the 0x0100-0x01FF asset-ID range
            asset_type = (RESOURCE_TYPE_ICON
                          if 0x0100 <= asset.asset_id < 0x0200
                          else RESOURCE_TYPE_LOGO)
            asset_format = asset.format
            asset_width = asset.width
            asset_height = asset.height
        elif isinstance(asset, FontAsset):
            # Font asset: raw TTF data (no header)
            asset_data = asset.font_data
            asset_type = RESOURCE_TYPE_FONT
            asset_format = 0  # Not applicable for fonts
            asset_width = 0
            asset_height = 0
        else:
            print(f"  Warning: Unknown asset type: {type(asset)}, skipping")
            continue
        
        asset_size = len(asset_data)
        
        # Calculate CRC for this asset
        asset_crc = zlib.crc32(asset_data) & 0xFFFFFFFF
        
        # Create entry with correct CRC
        entry = struct.pack(
            '<IIIIHBBHH',
            RESOURCE_ASSET_MAGIC,
            current_offset,
            asset_size,
            asset_crc,
            asset.asset_id,
            asset_type,
            asset_format,
            asset_width,
            asset_height
        )
        entry += b'\x00' * (ASSET_ENTRY_SIZE - len(entry))
        
        type_str = "IMAGE" if isinstance(asset, ImageAsset) else "FONT"
        print(f"  {asset.name} [{type_str}]: offset=0x{current_offset:06X}, size={asset_size}, crc=0x{asset_crc:08X}")
        
        asset_entries.append(entry)
        asset_data_blocks.append(asset_data)
        
        # Align next asset to 4-byte boundary
        current_offset += asset_size
        if current_offset % 4:
            padding = 4 - (current_offset % 4)
            asset_data_blocks.append(b'\x00' * padding)
            current_offset += padding
    
    # Calculate total size
    total_size = current_offset
    
    # Build the complete data after header (for CRC calculation)
    entries_data = b''.join(asset_entries)
    
    # Pad entries to data_offset if needed
    entries_padding = data_offset - table_offset - len(entries_data)
    if entries_padding > 0:
        entries_data += b'\x00' * entries_padding
    
    all_asset_data = b''.join(asset_data_blocks)
    data_after_header = entries_data + all_asset_data
    
    # Create partition header with CRC
    partition_header = create_resource_partition_header(
        total_size=total_size,
        asset_count=asset_count,
        data_for_crc=data_after_header,
        build_id=build_id
    )
    
    # Combine everything
    binary = partition_header + data_after_header
    
    print(f"\nTotal binary size: {len(binary)} bytes ({len(binary)/1024:.1f} KB)")
    
    return binary


# =============================================================================
# Main Entry Point
# =============================================================================

def main():
    parser = argparse.ArgumentParser(
        description='Convert LVGL logo C sources to Resource binary format'
    )
    parser.add_argument(
        '--src-dir', '-s',
        type=Path,
        default=Path(__file__).parent.parent.parent / 'src',
        help='Source directory containing logo*.c files'
    )
    parser.add_argument(
        '--output', '-o',
        type=Path,
        default=None,
        help='Output binary file (default: build_sdk_v330/resource.bin)'
    )
    parser.add_argument(
        '--build-id', '-b',
        type=str,
        default='',
        help='Build identifier string (max 31 chars)'
    )
    parser.add_argument(
        '--verbose', '-v',
        action='store_true',
        help='Verbose output'
    )
    parser.add_argument(
        '--font-dir', '-f',
        type=Path,
        default=None,
        help='Directory containing TTF font files (default: fonts/)'
    )
    parser.add_argument(
        '--no-fonts',
        action='store_true',
        help='Exclude fonts from the output (logo only)'
    )
    parser.add_argument(
        '--icons-dir', '-i',
        type=Path,
        default=None,
        help='Directory containing PNG icon files (default: assets/icons/)'
    )
    
    args = parser.parse_args()
    
    # Resolve paths
    src_dir = args.src_dir.resolve()
    if not src_dir.exists():
        print(f"Error: Source directory not found: {src_dir}")
        sys.exit(1)
    
    # Font directory (default: fonts/ next to src/)
    project_root = Path(__file__).parent.parent.parent
    if args.font_dir is None:
        font_dir = project_root / 'fonts'
    else:
        font_dir = args.font_dir.resolve()

    # Icons directory (default: assets/icons/)
    if args.icons_dir is None:
        icons_dir = project_root / 'assets' / 'icons'
    else:
        icons_dir = args.icons_dir.resolve()

    logo_png_dir = project_root / 'assets' / 'logo'
    
    # Default output path
    if args.output is None:
        output_path = project_root / 'build_sdk_v330' / 'resource.bin'
    else:
        output_path = args.output.resolve()
    
    # Ensure output directory exists
    output_path.parent.mkdir(parents=True, exist_ok=True)
    
    print("=" * 60)
    print("Resource store Conversion Tool")
    print("=" * 60)
    print(f"Source directory: {src_dir}")
    print(f"Font directory:   {font_dir}")
    print(f"Icons directory:  {icons_dir}")
    print(f"Logo PNG dir:     {logo_png_dir}")
    print(f"Output file:      {output_path}")
    
    # Find logo source files (exclude runtime stub-only files with no pixel map).
    # Search this directory and one level of subdirs so either src/ or src/core/ works.
    logo_candidates = list(src_dir.glob('logo*.c')) + list(src_dir.glob('*/logo*.c'))
    seen = set()
    logo_files = []
    for f in sorted(logo_candidates):
        if f.name == 'logo_stubs_resource.c' or f.resolve() in seen:
            continue
        seen.add(f.resolve())
        logo_files.append(f)
    print(f"\nFound {len(logo_files)} logo C file(s):")
    for f in logo_files:
        print(f"  - {f}")

    logo_png_files = []
    if logo_png_dir.exists():
        for f in sorted(logo_png_dir.glob('*.png')):
            if f.stem.lower() in LOGO_PNG_ASSET_IDS:
                logo_png_files.append(f)
        if logo_png_files:
            print(f"\nFound {len(logo_png_files)} logo PNG(s) in {logo_png_dir.name}/")
            for f in logo_png_files:
                print(f"  - {f.name}")

    # Find PNG icon files (only those mapped in ICON_ASSET_IDS)
    icon_files = []
    if icons_dir.exists():
        icon_files = sorted(icons_dir.glob('*.png'))
        if icon_files:
            print(f"\nFound {len(icon_files)} icon file(s) in {icons_dir.name}/")
    else:
        print(f"\nIcons directory not found: {icons_dir} (no icons included)")
    
    # Find TTF font files (unless --no-fonts)
    # Prefer subsetted font (smaller) over full font
    ttf_files = []
    if args.no_fonts:
        print(f"\nFont loading disabled (--no-fonts)")
    elif font_dir.exists():
        # Look for subset font first (smaller), then fall back to full font
        subset_font = font_dir / 'Montserrat-Subset.ttf'
        full_font = font_dir / 'Montserrat-Regular.ttf'
        
        if subset_font.exists():
            ttf_files = [subset_font]
            print(f"\nUsing subsetted font: {subset_font.name}")
        elif full_font.exists():
            ttf_files = [full_font]
            print(f"\nUsing full font: {full_font.name}")
            print("  (Run scripts/tools/subset_fonts.py to create smaller subset)")
        else:
            # Fall back to any TTF file
            ttf_files = sorted(font_dir.glob('*.ttf'))
        
        if ttf_files:
            print(f"Font file(s):")
            for f in ttf_files:
                print(f"  - {f.name} ({f.stat().st_size / 1024:.1f} KB)")
    else:
        print(f"\nFont directory not found: {font_dir}")
        print("  (No fonts will be included - download Montserrat.ttf)")
    
    if not logo_files and not logo_png_files and not ttf_files and not icon_files:
        print("\nError: No assets found (logos, icons, or fonts)")
        sys.exit(1)
    
    # Parse all logo files
    assets = []
    for filepath in logo_files:
        asset = parse_lvgl_c_source(filepath)
        if asset:
            assets.append(asset)

    for filepath in logo_png_files:
        asset = load_png_logo(filepath)
        if asset:
            assets.append(asset)

    # Load PNG icons (ARGB8888)
    for filepath in icon_files:
        asset = load_png_icon(filepath)
        if asset:
            assets.append(asset)

    # Load all TTF fonts
    for filepath in ttf_files:
        asset = load_ttf_font(filepath)
        if asset:
            assets.append(asset)
    
    if not assets:
        print("\nError: No valid assets were parsed")
        sys.exit(1)
    
    print(f"\nSuccessfully loaded {len(assets)} asset(s)")
    
    # Generate build ID if not provided
    build_id = args.build_id
    if not build_id:
        build_id = time.strftime("%Y%m%d_%H%M%S")
    
    # Create Resource binary
    binary = create_resource_binary(assets, build_id=build_id)
    
    if not binary:
        print("\nError: Failed to create binary")
        sys.exit(1)
    
    # Write output file
    with open(output_path, 'wb') as f:
        f.write(binary)
    
    print(f"\nWrote {len(binary)} bytes to {output_path}")
    print("\nTo flash the Resource store:")
    print("  .\\scripts\\flash\\flash_resource.ps1")
    
    print("\n" + "=" * 60)
    print("Done!")
    print("=" * 60)
    
    return 0


if __name__ == '__main__':
    sys.exit(main())
