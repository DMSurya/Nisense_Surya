#!/usr/bin/env python3
"""
Convert glucose algorithm matrices from C arrays to CSV files.

This script extracts the 11 group elimination matrices from 
glucose_algorithm_matrices.c and saves them as separate CSV files
for loading from external flash filesystem.

Usage:
    python convert_glucose_matrices.py [--output-dir OUTPUT_DIR]

Arguments:
    --output-dir: Output directory for CSV files (default: model/glucose)

Output:
    Creates CSV files: group_1.csv through group_11.csv
    Format: lower_bound,upper_bound,correction_value
"""

import re
import os
import argparse
from pathlib import Path

# Matrix row counts (from glucose_algorithm_matrices.h)
MATRIX_ROWS = {
    'group_one': 7,
    'group_two': 629,
    'group_three': 1711,
    'group_four': 1808,
'group_five': 1437,
    'group_six': 1395,
    'group_seven': 2685,
    'group_eight': 1326,
    'group_nine': 1362,
    'group_ten': 716,
    'group_eleven': 82,
}

# Map C array names to output file names
MATRIX_FILES = {
    'group_one_matrix': 'group_1.csv',
    'group_two_matrix': 'group_2.csv',
    'group_three_matrix': 'group_3.csv',
    'group_four_matrix': 'group_4.csv',
    'group_five_matrix': 'group_5.csv',
    'group_six_matrix': 'group_6.csv',
    'group_seven_matrix': 'group_7.csv',
    'group_eight_matrix': 'group_8.csv',
    'group_nine_matrix': 'group_9.csv',
    'group_ten_matrix': 'group_10.csv',
    'group_eleven_matrix': 'group_11.csv',
}


def parse_c_matrix(content, matrix_name):
    """
    Extract matrix data from C source code.
    
    Args:
        content: C source file content as string
        matrix_name: Name of matrix variable (e.g., 'group_one_matrix')
    
    Returns:
        List of tuples: [(lower, upper, correction), ...]
    """
    # Find matrix declaration
    pattern = rf'const\s+float\s+{matrix_name}\s*\[.*?\]\s*=\s*\{{'
    match = re.search(pattern, content, re.DOTALL)
    
    if not match:
        raise ValueError(f"Matrix {matrix_name} not found in source")
    
    # Extract matrix data between braces
    start_pos = match.end()
    brace_count = 1
    end_pos = start_pos
    
    while brace_count > 0 and end_pos < len(content):
        if content[end_pos] == '{':
            brace_count += 1
        elif content[end_pos] == '}':
            brace_count -= 1
        end_pos += 1
    
    matrix_text = content[start_pos:end_pos-1]
    
    # Parse rows: {value1, value2, value3}
    row_pattern = r'\{([^}]+)\}'
    rows = []
    
    for match in re.finditer(row_pattern, matrix_text):
        row_text = match.group(1)
        # Extract numeric values (handles scientific notation)
        values = re.findall(r'[-+]?\d*\.?\d+(?:[eE][-+]?\d+)?', row_text)
        
        if len(values) == 3:
            lower = float(values[0])
            upper = float(values[1])
            correction = float(values[2])
            rows.append((lower, upper, correction))
    
    return rows


def write_csv(rows, output_path):
    """
    Write matrix rows to CSV file.
    
    Args:
        rows: List of (lower, upper, correction) tuples
        output_path: Output CSV file path
    """
    with open(output_path, 'w') as f:
        # Write header
        f.write('lower_bound,upper_bound,correction\n')
        
        # Write data rows
        for lower, upper, correction in rows:
            f.write(f'{lower:.6f},{upper:.6f},{correction:.6f}\n')


def main():
    parser = argparse.ArgumentParser(
        description='Convert glucose algorithm matrices from C to CSV'
    )
    parser.add_argument(
        '--output-dir',
        type=str,
        default=None,
        help='Output directory (default: model/glucose/<variant>)'
    )
    parser.add_argument(
        '--variant',
        type=str,
        choices=['wearable', 'pulse'],
        default='wearable',
        help='Hardware variant subfolder (default: wearable)'
    )
    parser.add_argument(
        '--source-file',
        type=str,
        default='drivers/sensor/glucose/glucose_algorithm_matrices.c',
        help='Source C file containing matrices'
    )
    
    args = parser.parse_args()
    
    # Resolve paths relative to project root
    script_dir = Path(__file__).parent
    project_root = script_dir.parent
    
    source_path = project_root / args.source_file
    if args.output_dir:
        output_dir = project_root / args.output_dir
    else:
        output_dir = project_root / 'model' / 'glucose' / args.variant
    
    # Verify source file exists
    if not source_path.exists():
        print(f"Error: Source file not found: {source_path}")
        return 1
    
    # Create output directory
    output_dir.mkdir(parents=True, exist_ok=True)
    
    # Read source file
    print(f"Reading matrices from: {source_path}")
    with open(source_path, 'r') as f:
        content = f.read()
    
    # Process each matrix
    total_rows = 0
    for matrix_name, csv_file in MATRIX_FILES.items():
        output_path = output_dir / csv_file
        
        print(f"Extracting {matrix_name}...", end=' ')
        
        try:
            rows = parse_c_matrix(content, matrix_name)
            write_csv(rows, output_path)
            
            print(f"✓ {len(rows)} rows → {output_path}")
            total_rows += len(rows)
            
        except Exception as e:
            print(f"✗ Error: {e}")
            return 1
    
    print(f"\nSuccess! Converted {total_rows} total rows across {len(MATRIX_FILES)} files")
    print(f"Output directory: {output_dir.absolute()}")
    print(f"\nTo use these files:")
    print(f"  1. Copy {output_dir.name}/ folder to external flash FAT partition")
    print(f"  2. Enable CONFIG_GLUCOSE_SENSOR_MATRICES_FROM_FILE=y in prj.conf")
    print(f"  3. Rebuild firmware")
    
    return 0


if __name__ == '__main__':
    exit(main())
