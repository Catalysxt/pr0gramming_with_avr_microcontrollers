#!/usr/bin/env python3
"""Emit the companion's FSM diagram from its transition table.

The state machine in src/app/fsm.c is a data table, and each legal edge carries
a machine-readable comment so the diagram can never drift from the code:

    // @fsm FROM -> TO : guard-label

This tool scrapes those lines, writes docs/fsm.dot (Graphviz), and renders
docs/fsm.svg. If the Graphviz `dot` binary is on PATH it is used; otherwise a
self-contained circular-layout SVG is generated with the standard library only,
so `make fsm-diagram` always produces a committable diagram.

Usage:
    python3 tools/fsm_dot.py --source src/app/fsm.c --dot docs/fsm.dot --svg docs/fsm.svg
"""
import argparse
import math
import os
import re
import shutil
import subprocess

EDGE_RE = re.compile(r"@fsm\s+(\w+)\s*->\s*(\w+)\s*:\s*(.+?)\s*$")


def parse_edges(source_path):
    edges = []
    with open(source_path, "r", encoding="utf-8") as fh:
        for line in fh:
            m = EDGE_RE.search(line)
            if m:
                edges.append((m.group(1), m.group(2), m.group(3)))
    if not edges:
        raise SystemExit(f"{source_path}: no '@fsm FROM -> TO : label' comments found")
    return edges


def ordered_nodes(edges):
    seen = []
    for src, dst, _ in edges:
        for n in (src, dst):
            if n not in seen:
                seen.append(n)
    return seen


def write_dot(edges, dot_path):
    lines = ["digraph companion_fsm {", "    rankdir=LR;",
             '    node [shape=ellipse, style=filled, fillcolor="#eef", fontname="Helvetica"];',
             '    edge [fontname="Helvetica", fontsize=10];']
    for src, dst, label in edges:
        lines.append(f'    {src} -> {dst} [label="{label}"];')
    lines.append("}")
    os.makedirs(os.path.dirname(dot_path) or ".", exist_ok=True)
    with open(dot_path, "w", encoding="utf-8") as fh:
        fh.write("\n".join(lines) + "\n")


def render_with_graphviz(dot_path, svg_path):
    dot = shutil.which("dot")
    if not dot:
        return False
    try:
        subprocess.run([dot, "-Tsvg", dot_path, "-o", svg_path], check=True)
        return True
    except (subprocess.CalledProcessError, OSError):
        return False


def render_fallback_svg(edges, nodes, svg_path):
    """Stdlib-only circular layout: nodes on a circle, edges as arrowed arcs."""
    W = H = 640
    cx, cy, R = W / 2, H / 2, 230
    rnode = 46
    pos = {}
    n = len(nodes)
    for i, name in enumerate(nodes):
        ang = -math.pi / 2 + 2 * math.pi * i / n
        pos[name] = (cx + R * math.cos(ang), cy + R * math.sin(ang))

    out = [f'<svg xmlns="http://www.w3.org/2000/svg" width="{W}" height="{H}" '
           f'viewBox="0 0 {W} {H}" font-family="Helvetica,Arial,sans-serif">']
    out.append('<defs><marker id="arrow" markerWidth="10" markerHeight="10" '
               'refX="9" refY="3" orient="auto" markerUnits="strokeWidth">'
               '<path d="M0,0 L9,3 L0,6 z" fill="#556"/></marker></defs>')
    out.append(f'<rect width="{W}" height="{H}" fill="white"/>')
    out.append(f'<text x="{cx}" y="28" text-anchor="middle" font-size="18" '
               'font-weight="bold" fill="#334">Digital Companion FSM</text>')

    def edge_point(a, b):
        ax, ay = pos[a]; bx, by = pos[b]
        dx, dy = bx - ax, by - ay
        d = math.hypot(dx, dy) or 1.0
        return ax + dx / d * rnode, ay + dy / d * rnode, bx - dx / d * rnode, by - dy / d * rnode

    for src, dst, label in edges:
        if src == dst:
            continue
        x1, y1, x2, y2 = edge_point(src, dst)
        mx, my = (x1 + x2) / 2, (y1 + y2) / 2
        nx, ny = -(y2 - y1), (x2 - x1)
        d = math.hypot(nx, ny) or 1.0
        cxp, cyp = mx + nx / d * 26, my + ny / d * 26   # bow the arc outward
        out.append(f'<path d="M{x1:.0f},{y1:.0f} Q{cxp:.0f},{cyp:.0f} {x2:.0f},{y2:.0f}" '
                   'fill="none" stroke="#889" stroke-width="1.5" marker-end="url(#arrow)"/>')
        out.append(f'<text x="{cxp:.0f}" y="{cyp:.0f}" text-anchor="middle" '
                   f'font-size="9" fill="#a33">{label}</text>')

    for name, (x, y) in pos.items():
        out.append(f'<circle cx="{x:.0f}" cy="{y:.0f}" r="{rnode}" fill="#eef" '
                   'stroke="#446" stroke-width="1.5"/>')
        short = name.replace("EXPR_", "")
        out.append(f'<text x="{x:.0f}" y="{y+4:.0f}" text-anchor="middle" '
                   f'font-size="11" fill="#223">{short}</text>')
    out.append("</svg>")
    os.makedirs(os.path.dirname(svg_path) or ".", exist_ok=True)
    with open(svg_path, "w", encoding="utf-8") as fh:
        fh.write("\n".join(out) + "\n")


def main():
    ap = argparse.ArgumentParser(description="Generate the FSM .dot/.svg from fsm.c")
    ap.add_argument("--source", default="src/app/fsm.c")
    ap.add_argument("--dot", default="docs/fsm.dot")
    ap.add_argument("--svg", default="docs/fsm.svg")
    args = ap.parse_args()

    edges = parse_edges(args.source)
    nodes = ordered_nodes(edges)
    write_dot(edges, args.dot)
    print(f"wrote {args.dot} ({len(edges)} edges, {len(nodes)} states)")

    if render_with_graphviz(args.dot, args.svg):
        print(f"wrote {args.svg} (via Graphviz dot)")
    else:
        render_fallback_svg(edges, nodes, args.svg)
        print(f"wrote {args.svg} (stdlib fallback; install Graphviz for nicer layout)")


if __name__ == "__main__":
    main()
