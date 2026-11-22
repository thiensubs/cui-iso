import sys
from PIL import Image
def lin(c):
    c=c/255.0
    return c/12.92 if c<=0.04045 else ((c+0.055)/1.055)**2.4
def lum(t): return 0.2126*lin(t[0])+0.7152*lin(t[1])+0.0722*lin(t[2])
for p in sys.argv[1:]:
    im=Image.open(p).convert("RGB"); w,h=im.size; px=im.load()
    n=0; light=0; dark=0
    for y in range(0,h,3):
        for x in range(0,w,3):
            c=px[x,y]; L=lum(c); n+=1
            if L>0.75: light+=1
            elif L<0.06: dark+=1
    print("%s  light%%=%.1f dark%%=%.1f mid%%=%.1f" % (p, 100*light/n, 100*dark/n, 100*(n-light-dark)/n))
