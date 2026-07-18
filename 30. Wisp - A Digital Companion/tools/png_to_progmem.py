#!/usr/bin/env python3
"""Author expression bitmaps for the digital companion.

Converts human-readable 8x8 keyframes -- ASCII art (`.txt`) or thresholded PNG --
into a compilable `src/assets/expressions.h`. Hand-hexing row bitmaps is banned;
this is the single authoring pipeline and the `.txt` files under assets_src/ are
the human-readable source of truth committed to git.

Usage:
    python3 tools/png_to_progmem.py --input assets_src/ --output src/assets/expressions.h

ASCII format (one file per expression, e.g. assets_src/happy.txt):
    # any '#'-prefixed line is a comment/metadata
    # hold: 12          -> hold_ticks for this expression (default 20)
    ........            \
    .##..##.             |  an 8-line block of 8 chars = one keyframe
    .##..##.             |  '.' or ' ' = off, anything else = on
    ........             |  bit 7 (MSB) = leftmost column, row 0 = top
    ........            /
    <blank line separates keyframes; up to MAX_KEYFRAMES per expression>

PNG format (assets_src/happy.png): an 8-pixel-tall image; every 8x8 cell across
its width is one keyframe. Requires Pillow (see tools/requirements.txt).

Dependencies: stdlib only for ASCII; Pillow only if PNG inputs are present.
"""
import argparse
import os
import sys

# Expression enum order -- MUST match `expression_t` in src/app/face.h. The
# generated EXPRESSIONS[] array is indexed by that enum, so order is load-bearing.
EXPRESSION_ORDER = [
    "NEUTRAL", "HAPPY", "SAD", "CURIOUS", "HESITANT",
    "NERVOUS", "SLEEPY", "SURPRISED", "CONTENT",
]

MAX_KEYFRAMES = 4
ROWS = 8
COLS = 8
DEFAULT_HOLD = 20


def rows_from_ascii_block(lines):
    """Turn 8 text lines into 8 row bytes (bit 7 = leftmost column)."""
    if len(lines) != ROWS:
        raise ValueError(f"keyframe must be {ROWS} lines, got {len(lines)}")
    frame = []
    for line in lines:
        line = line.rstrip("\n")
        if len(line) < COLS:
            line = line.ljust(COLS, ".")
        byte = 0
        for col in range(COLS):
            if line[col] not in (".", " "):
                byte |= 1 << (7 - col)   # leftmost char -> MSB
        frame.append(byte)
    return frame


def parse_ascii(path):
    """Return (frames, hold_ticks) from an ASCII-art expression file."""
    frames = []
    hold = DEFAULT_HOLD
    block = []
    with open(path, "r", encoding="utf-8") as fh:
        for raw in fh:
            stripped = raw.strip()
            if stripped == "":
                # blank line separates keyframes
                if block:
                    frames.append(rows_from_ascii_block(block))
                    block = []
                continue
            # A bitmap row is made ONLY of '.' and '#'. That means a lit-leftmost
            # row like "#......#" is data, not a comment -- so we can't key off a
            # leading '#'. Anything containing other characters is a comment /
            # metadata line (e.g. "# hold: 20", "# EXPR_HAPPY ...").
            if all(ch in ".#" for ch in stripped):
                block.append(raw)
                continue
            low = stripped.lstrip("#").strip().lower()
            if low.startswith("hold:"):
                hold = int(low.split(":", 1)[1])
    if block:
        frames.append(rows_from_ascii_block(block))
    if not frames:
        raise ValueError(f"{path}: no keyframes found")
    if len(frames) > MAX_KEYFRAMES:
        raise ValueError(f"{path}: {len(frames)} frames exceeds MAX_KEYFRAMES={MAX_KEYFRAMES}")
    return frames, hold


def parse_png(path):
    """Return (frames, hold_ticks) from a thresholded 8-tall PNG strip."""
    try:
        from PIL import Image
    except ImportError:
        sys.exit("Pillow is required for PNG input: pip install -r tools/requirements.txt")
    img = Image.open(path).convert("L")
    w, h = img.size
    if h != ROWS:
        raise ValueError(f"{path}: image must be {ROWS}px tall, got {h}")
    n = w // COLS
    if n < 1:
        raise ValueError(f"{path}: image must be at least {COLS}px wide")
    if n > MAX_KEYFRAMES:
        raise ValueError(f"{path}: {n} frames exceeds MAX_KEYFRAMES={MAX_KEYFRAMES}")
    px = img.load()
    frames = []
    for f in range(n):
        frame = []
        for r in range(ROWS):
            byte = 0
            for c in range(COLS):
                if px[f * COLS + c, r] < 128:   # dark pixel = lit dot
                    byte |= 1 << (7 - c)
            frame.append(byte)
        frames.append(frame)
    return frames, DEFAULT_HOLD


def load_expression(input_dir, name):
    """Find <name>.txt or <name>.png (case-insensitive) and parse it."""
    base = name.lower()
    txt = os.path.join(input_dir, base + ".txt")
    png = os.path.join(input_dir, base + ".png")
    if os.path.exists(txt):
        return parse_ascii(txt), txt
    if os.path.exists(png):
        return parse_png(png), png
    sys.exit(f"missing source for {name}: expected {txt} or {png}")


def emit_header(expressions, out_path):
    lines = []
    lines.append("/* GENERATED by tools/png_to_progmem.py -- DO NOT EDIT BY HAND.")
    lines.append(" * Source of truth: the ASCII-art files under assets_src. Re-run:")
    lines.append(" *   python3 tools/png_to_progmem.py --input assets_src")
    lines.append(" *       --output src/assets/expressions.h")
    lines.append(" *")
    lines.append(" * Each expression is a set of 8x8 keyframes: frames[k][row], row 0 = top,")
    lines.append(" * bit 7 = leftmost column. Looping expressions (e.g. NERVOUS eye-dart) use")
    lines.append(" * frame_count > 1 and cycle every hold_ticks animation ticks. */")
    lines.append("#ifndef DIGITAL_COMPANION_EXPRESSIONS_H_")
    lines.append("#define DIGITAL_COMPANION_EXPRESSIONS_H_")
    lines.append("")
    lines.append("#include <stdint.h>")
    lines.append("#include <avr/pgmspace.h>")
    lines.append("")
    lines.append(f"#define MAX_KEYFRAMES {MAX_KEYFRAMES}")
    lines.append("")
    lines.append("typedef struct {")
    lines.append(f"    uint8_t frames[MAX_KEYFRAMES][{ROWS}]; /* row bitmaps */")
    lines.append("    uint8_t frame_count;")
    lines.append("    uint8_t hold_ticks;                    /* ticks to hold each keyframe */")
    lines.append("} expression_frames_t;")
    lines.append("")
    lines.append("static const expression_frames_t EXPRESSIONS[] PROGMEM = {")
    for name in EXPRESSION_ORDER:
        frames, hold = expressions[name]
        lines.append(f"    [EXPR_{name}] = {{")
        lines.append("        .frames = {")
        for fi in range(MAX_KEYFRAMES):
            if fi < len(frames):
                body = ", ".join(f"0x{b:02X}" for b in frames[fi])
                lines.append(f"            {{ {body} }},  /* frame {fi} */")
            else:
                body = ", ".join(["0x00"] * ROWS)
                lines.append(f"            {{ {body} }},")
        lines.append("        },")
        lines.append(f"        .frame_count = {len(frames)},")
        lines.append(f"        .hold_ticks  = {hold},")
        lines.append("    },")
    lines.append("};")
    lines.append("")
    lines.append("#endif /* DIGITAL_COMPANION_EXPRESSIONS_H_ */")
    text = "\n".join(lines) + "\n"
    os.makedirs(os.path.dirname(out_path), exist_ok=True)
    with open(out_path, "w", encoding="utf-8") as fh:
        fh.write(text)


def main():
    ap = argparse.ArgumentParser(description="Convert ASCII/PNG keyframes to expressions.h")
    ap.add_argument("--input", required=True, help="assets_src/ directory")
    ap.add_argument("--output", required=True, help="path to generated expressions.h")
    args = ap.parse_args()

    expressions = {}
    for name in EXPRESSION_ORDER:
        (frames, hold), src = load_expression(args.input, name)
        expressions[name] = (frames, hold)
        print(f"  EXPR_{name:<9} <- {os.path.basename(src)}  ({len(frames)} frame(s), hold {hold})")

    # The generated header uses designated initializers keyed by EXPR_* enum
    # constants, which requires face.h to be in scope where it is #included.
    emit_header(expressions, args.output)
    print(f"wrote {args.output}")


if __name__ == "__main__":
    main()
