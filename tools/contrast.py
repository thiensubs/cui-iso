import sys
from PIL import Image
def lin(c):
    c=c/255.0
    return c/12.92 if c<=0.04045 else ((c+0.055)/1.055)**2.4
def lum(t): return 0.2126*lin(t[0])+0.7152*lin(t[1])+0.0722*lin(t[2])
def ratio(a,b):
    h,l=max(a,b),min(a,b)
    return (h+0.05)/(l+0.05)
for path in sys.argv[1:]:
    im=Image.open(path).convert("RGB"); w,h=im.size; px=im.load()
    c={}
    for y in range(0,h,2):
        for x in range(0,w,2):
            k=px[x,y]; c[k]=c.get(k,0)+1
    bg=max(c,key=lambda k:c[k]); Lbg=lum(bg)
    weak=[(ratio(Lbg,lum(k)),k,n) for k,n in c.items() if k!=bg and n>=60 and ratio(Lbg,lum(k))>1.05]
    weak.sort()
    print(path); print("  page bg",bg,"lum %.3f"%Lbg); print("  colours",len(c))
    print("  worst pairs:");  [print("    %.2f:1 %s (%d px)"%(r,k,n)) for r,k,n in weak[:3]]
