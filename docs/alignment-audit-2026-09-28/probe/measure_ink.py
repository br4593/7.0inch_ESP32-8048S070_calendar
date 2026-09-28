from pathlib import Path
import math
import sys

ROOT = Path(sys.argv[1] if len(sys.argv) > 1 else '/tmp/calendar-alignment-captures')

def read_ppm(path):
    data = path.read_bytes()
    parts = data.split(b'\n', 3)
    assert parts[0] == b'P6'
    w, h = map(int, parts[1].split())
    assert int(parts[2]) == 255
    return w, h, parts[3]

def rgb_at(raw, w, x, y):
    i = (y*w+x)*3
    return raw[i], raw[i+1], raw[i+2]

def ink_bbox(path, rect, fg):
    w,h,raw=read_ppm(path)
    x,y,rw,rh=rect
    pts=[]
    for py in range(max(0,y+3), min(h,y+rh-3)):
        for px in range(max(0,x+3), min(w,x+rw-3)):
            c=rgb_at(raw,w,px,py)
            if math.dist(c,fg) <= 62:
                pts.append((px,py))
    if not pts:return None
    xs=[p[0] for p in pts];ys=[p[1] for p in pts]
    return min(xs),min(ys),max(xs),max(ys),len(pts)

rows=[]
for line in (ROOT/'geometry.tsv').read_text().splitlines():
    f=line.split('\t')
    if len(f)<27 or f[0]!='BUTTON' or f[2].startswith('dynamic/'):
        continue
    screen,name=f[1],f[2]
    if name in ('header/wifi','weather/card') or name.startswith('day/'):
        continue
    fghex=f[23][-6:]
    fg=tuple(int(fghex[i:i+2],16) for i in (0,2,4))
    rect=tuple(map(int,f[3:7]))
    bbox=ink_bbox(ROOT/f'{screen}.ppm',rect,fg)
    if not bbox:continue
    x,y,w,h=rect;ix1,iy1,ix2,iy2,count=bbox
    dx=(ix1+ix2-(2*x+w-1))/2
    dy=(iy1+iy2-(2*y+h-1))/2
    rows.append((screen,name,rect,bbox[:4],dx,dy,f[26]))

with (ROOT/'ink-centers.tsv').open('w') as out:
    out.write('screen\tname\thitbox\tink_bbox\tdx\tdy\ttext\n')
    for r in rows:
        out.write('\t'.join((r[0],r[1],','.join(map(str,r[2])),','.join(map(str,r[3])),f'{r[4]:.1f}',f'{r[5]:.1f}',r[6]))+'\n')

# Direct heading checks use the same rendered foreground.
for screen,rect,label in [('appearance',(230,18,340,28),'appearance heading'),('firmware',(230,18,340,28),'firmware heading')]:
    bbox=ink_bbox(ROOT/f'{screen}.ppm',rect,(29,36,48))
    print(label,rect,bbox)
print(ROOT/'ink-centers.tsv')
