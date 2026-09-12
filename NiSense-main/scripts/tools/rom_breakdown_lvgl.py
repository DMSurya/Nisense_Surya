#!/usr/bin/env python3
import json
import sys

def children(node):
    return node.get("children") or []


def print_sorted(node, label, limit=30, depth=0, min_size=0):
    ind = "  " * depth
    print(f"{ind}=== {label} ===")
    ch = [c for c in children(node) if c.get("size", 0) >= min_size]
    for c in sorted(ch, key=lambda x: -x.get("size", 0))[:limit]:
        print(f"{ind}{c['size']:8}  {c['name']}")


def main():
    path = sys.argv[1] if len(sys.argv) > 1 else "rom.json"
    with open(path, encoding="utf-8") as f:
        d = json.load(f)
    root = d["symbols"]
    total = d.get("total_size", root["size"])
    print(f"Total: {total} B\n")

    for c in sorted(children(root), key=lambda x: -x["size"]):
        print(f"{c['size']:8}  {c['name']}")

    ws = next(c for c in children(root) if c["name"] == "WORKSPACE")
    mods = next(c for c in children(ws) if c["name"] == "modules")
    lib = next(c for c in children(mods) if c["name"] == "lib")
    gui = next(c for c in children(lib) if c["name"] == "gui")
    lvgl = next(c for c in children(gui) if c["name"] == "lvgl")
    src = next(c for c in children(lvgl) if c["name"] == "src")
    print()
    print_sorted(src, "LVGL src (top)", limit=20, min_size=5000)

    zb = next(c for c in children(root) if c["name"] == "ZEPHYR_BASE")
    sub = next(c for c in children(zb) if c["name"] == "subsys")
    print()
    print_sorted(sub, "Zephyr subsys", limit=15)


if __name__ == "__main__":
    main()
