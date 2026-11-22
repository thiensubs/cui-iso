import sys
from PIL import Image

def lin(c):
    c = c / 255.0
    return c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4

def lum(r, g, b):
    return 0.2126 * lin(r) + 0.7152 * lin(g) + 0.0722 * lin(b)

def ratio(a, b):
    hi, lo = max(a, b), min(a, b)
    return (hi + 0.05) / (lo + 0.05)

def audit(path):
    im = Image.open(path).convert("RGB")
    w, h = im.size
    px = im.load()
    counts = {}
    for y in range(0, h, 2):
        for x in range(0, w, 2):
            k = px[x, y]
            counts[k] = counts.get(k, 0) + 1
    bg = max(counts, key=lambda k: counts[k])
    nbg = counts[bg]
    Lbg = lum(*bg)
    worst = 99.0
    worst_col = None
    for col, n in counts.items():
        if n < 30 or col == bg:
            continue
        r = ratio(Lbg, lum(*col))
        if r > 1.15 and r < worst:
            worst = r
            worst_col = col
    print(path)
    print("  page background :", bg, nbg, "px")
    print("  distinct colours:", len(counts))
    print("  weakest contrast: %.2f:1  %s" % (worst, worst_col))

for f in sys.argv[1:]:
    audit(f)
