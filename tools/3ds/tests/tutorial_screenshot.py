#!/usr/bin/env python3
"""Detect tutorial text crossing the rendered gold frame in a captured screen."""
import argparse
from pathlib import Path
from PIL import Image

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('image', type=Path)
parser.add_argument('--expect-overflow', action='store_true', help='Check a known-bad capture as a negative control')
args = parser.parse_args()
im = Image.open(args.image).convert('RGB')
width, height = im.size
pixel = im.load()

def gold(x, y):
    r, g, b = pixel[x, y]
    return r > 150 and g > 125 and b < 140 and r > b * 1.35

rows = []
for y in range(height // 4, height - 10):
    xs = [x for x in range(width) if gold(x, y)]
    if len(xs) > max(60, width * .06):
        rows.append((len(xs), y, min(xs), max(xs)))
candidates = [(min(a[0], b[0]), a, b) for a in rows for b in rows
              if b[1] - a[1] >= 30 and abs(a[2] - b[2]) <= 8 and abs(a[3] - b[3]) <= 8]
if not candidates:
    raise SystemExit('Could not identify the tutorial frame; capture a full unobscured tutorial screen.')
_, top_row, bottom_row = max(candidates)
top, bottom = top_row[1], bottom_row[1]
columns = []
for x in range(max(0, top_row[2] - 8), min(width, top_row[3] + 9)):
    count = sum(gold(x, y) for y in range(top + 3, bottom - 2))
    if count > (bottom - top) * .5:
        columns.append(x)
if len(columns) < 2:
    raise SystemExit('Could not identify both vertical sides of the tutorial frame.')
left, right = min(columns), max(columns)
white = []
for y in range(top + 6, bottom - 5):
    for x in range(max(0, left - 40), min(width, right + 41)):
        r, g, b = pixel[x, y]
        if r > 195 and g > 195 and b > 190 and max(r, g, b) - min(r, g, b) < 50:
            white.append((x, y))
if len(white) < 20:
    raise SystemExit('No readable tutorial text was found inside the frame area.')
outside = sum(x < left - 2 or x > right + 2 for x, _ in white)
for y in list(range(max(0, top - 8), max(0, top - 3))) + list(range(min(height, bottom + 3), min(height, bottom + 8))):
    for x in range(left + 8, right - 8):
        r, g, b = pixel[x, y]
        if r > 195 and g > 195 and b > 190 and max(r, g, b) - min(r, g, b) < 50:
            outside += 1
print(f'Frame x={left}..{right}, y={top}..{bottom}; {outside} text pixels outside the frame.')
if (outside > 0) != args.expect_overflow:
    raise SystemExit('Tutorial frame overflow check failed.')
