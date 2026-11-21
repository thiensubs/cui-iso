"""Put the 10 rendered cases on one labeled sheet.
Run:  mise x python -- python3 tools/contact_sheet.py /tmp/top10 /tmp/top10_sheet.png
"""
import sys
from PIL import Image, ImageDraw

src = sys.argv[1] if len(sys.argv) > 1 else "/tmp/top10"
out = sys.argv[2] if len(sys.argv) > 2 else "/tmp/top10_sheet.png"

# plain words for each picture, so anyone can tell what it shows
labels = [
    ("01_service_topology.png", "1. Who talks to who"),
    ("02_ci_pipeline.png", "2. Build steps"),
    ("03_kubernetes.png", "3. Pods on machines"),
    ("04_data_etl.png", "4. Moving data"),
    ("05_trace.png", "5. One request's path"),
    ("06_datacenter.png", "6. Server racks"),
    ("07_mlops.png", "7. Teaching a model"),
    ("08_iot_building.png", "8. Building sensors"),
    ("09_manufacturing.png", "9. Factory line"),
    ("10_capacity.png", "10. What costs what"),
]

COLS, TW, TH, PAD, CAP = 2, 360, 260, 14, 34
rows = (len(labels) + COLS - 1) // COLS
sheet_w = COLS * TW + (COLS + 1) * PAD
sheet_h = rows * (TH + CAP) + (rows + 1) * PAD

sheet = Image.new("RGB", (sheet_w, sheet_h), (20, 22, 27))
d = ImageDraw.Draw(sheet)

for i, (fname, label) in enumerate(labels):
    col, row = i % COLS, i // COLS
    x = PAD + col * (TW + PAD)
    y = PAD + row * (TH + CAP + PAD)
    im = Image.open(f"{src}/{fname}").convert("RGB").resize((TW, TH))
    sheet.paste(im, (x, y))
    d.text((x + 2, y + TH + 9), label, fill=(232, 234, 238))

sheet.save(out)
print("wrote", out, sheet.size)