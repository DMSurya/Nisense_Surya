#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Extract matrix data from matrices_extracted.txt and integrate into matrices file.
"""

def extract_matrices(input_file):
    """Extract matrices from the input file."""
    matrices = {}
    current = None
    
    with open(input_file, 'r', encoding='utf-8') as f:
        for line in f:
            line = line.strip()
            
            # Check for group header
            if line.startswith('/*') and 'group_' in line:
                # Extract group name (everything after 'group_' until space or '-')
                parts = line.split('group_')
                if len(parts) > 1:
                    current = parts[1].split()[0].split('-')[0]
                    matrices[current] = []
            
            # Check for matrix row (starts with {)
            elif line.startswith('{') and current is not None:
                matrices[current].append(line)
    
    return matrices

def write_matrices_file(matrices, output_file):
    """Write matrices to the output file."""
    with open(output_file, 'w', encoding='utf-8') as f:
        for group_name, rows in matrices.items():
            f.write(f"/* group_{group_name} - {len(rows)} rows */\n")
            for row in rows:
                f.write(f"    {row}\n")
            f.write("\n")  # Add blank line between groups

def generate_c_matrix_code(group_name, rows):
    """Generate C array code for a matrix group."""
    lines = []
    for i, row in enumerate(rows):
        # Remove leading/trailing whitespace
        row = row.strip()
        # Ensure row ends with comma (except last row)
        if i < len(rows) - 1:
            if not row.endswith(','):
                row = row + ','
        else:
            # Last row should NOT have a comma
            if row.endswith(','):
                row = row[:-1]
        lines.append(f"    {row}")
    return '\n'.join(lines)

def update_c_file(c_file_path, matrices):
    """Update the C file with the extracted matrices."""
    # Map group names to their matrix names in C
    group_mapping = {
        'three_matrix': 'group_three_matrix',
        'four_matrix': 'group_four_matrix',
        'five_matrix': 'group_five_matrix',
        'six_matrix': 'group_six_matrix',
        'seven_matrix': 'group_seven_matrix',
        'eight_matrix': 'group_eight_matrix',
        'nine_matrix': 'group_nine_matrix',
        'ten_matrix': 'group_ten_matrix',
        'eleven_matrix': 'group_eleven_matrix',
    }
    
    with open(c_file_path, 'r', encoding='utf-8') as f:
        content = f.read()
    
    # Update each matrix group
    for group_key, c_matrix_name in group_mapping.items():
        if group_key in matrices:
            rows = matrices[group_key]
            c_code = generate_c_matrix_code(group_key, rows)
            
            # Find the matrix definition - look for the const float declaration
            pattern = f"const float {c_matrix_name}[GROUP_ROW_"
            start_idx = content.find(pattern)
            if start_idx != -1:
                # Find the opening brace after the declaration
                brace_start = content.find('{', start_idx)
                if brace_start != -1:
                    # Find everything until the closing brace and semicolon
                    # Look for the closing brace (need to handle nested braces)
                    brace_count = 0
                    brace_end = brace_start
                    for i in range(brace_start, len(content)):
                        if content[i] == '{':
                            brace_count += 1
                        elif content[i] == '}':
                            brace_count -= 1
                            if brace_count == 0:
                                brace_end = i
                                break
                    
                    # Find the semicolon after the closing brace
                    semicolon_idx = content.find(';', brace_end)
                    if semicolon_idx == -1:
                        semicolon_idx = brace_end + 1
                    
                    # Replace everything between opening brace and closing brace
                    before = content[:brace_start + 1]
                    after = content[brace_end:semicolon_idx + 1]
                    
                    # Build new content: opening brace, matrix rows, closing brace and semicolon
                    content = before + '\n' + c_code + '\n' + after + content[semicolon_idx + 1:]
                    print(f"  Updated {c_matrix_name} with {len(rows)} rows")
    
    with open(c_file_path, 'w', encoding='utf-8') as f:
        f.write(content)

if __name__ == "__main__":
    input_file = 'matrices_extracted.txt'
    output_file = 'matrices'
    c_file = 'drivers/sensor/glucose/glucose_algorithm_matrices.c'
    
    print(f"Extracting matrices from {input_file}...")
    matrices = extract_matrices(input_file)
    
    print(f"\nFound {len(matrices)} matrix groups:")
    for group_name, rows in matrices.items():
        print(f"  group_{group_name}: {len(rows)} rows")
    
    print(f"\nWriting to {output_file}...")
    write_matrices_file(matrices, output_file)
    
    print(f"\nUpdating C file {c_file}...")
    update_c_file(c_file, matrices)
    
    print(f"Done! Matrix data written to {output_file} and {c_file} updated")
