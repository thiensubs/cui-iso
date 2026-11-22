import sys
from PIL import Image
for path in sys.argv[1:]:
    im = Image.open(path).convert("RGB")
    w, h = im.size
    px = im.load()
    pts = [(1,1), (w-2,1), (1,h-2), (w-2,h-2)]
    print(path, im.size, [px[x,y] for x,y in pts])
