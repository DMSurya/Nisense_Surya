#!/usr/bin/env python3
"""Summarize Zephyr rom.json (footprint) by top-level tree nodes."""
import argparse
import json


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("rom_json", help="Path to rom.json from size_report")
    ap.add_argument("--depth", type=int, default=2, help="Print tree down to this depth")
    args = ap.parse_args()

    with open(args.rom_json, encoding="utf-8") as f:
        d = json.load(f)
    root = d["symbols"]
    total = d.get("total_size", root.get("size", 0))

    def print_level(node, depth, indent=""):
        if depth > args.depth:
            return
        ch = node.get("children") or []
        if not ch and depth > 0:
            return
        for c in sorted(ch, key=lambda x: -x.get("size", 0)):
            sz = c.get("size", 0)
            pct = 100.0 * sz / total if total else 0
            name = c.get("name", "?")
            print(f"{indent}{sz:9d} B ({pct:5.2f}%)  {name}")
            print_level(c, depth + 1, indent + "  ")

    print(f"Total ROM (report): {total} B\n")
    print(f"=== Tree depth {args.depth} ===\n")
    print_level(root, 0)


if __name__ == "__main__":
    main()
