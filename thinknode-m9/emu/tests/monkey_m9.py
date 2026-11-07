#!/usr/bin/env python3
# Random key presses for the M9 emulator: writes a --script file. seed, duration in s, output file.
import random,sys
seed,seconds,out=int(sys.argv[1]),int(sys.argv[2]),sys.argv[3]
r=random.Random(seed);t=4000;L=[]
fn=[34,36,37,38,39,40,41]
letters='abcdefghijklmnopqrstuvwxyz '
while t<seconds*1000:
    x=r.random()
    if x<.35:L.append(f'{t} roll {r.choice([-1,0,1])} {r.choice([-1,0,1])}')
    elif x<.55:L.append(f'{t} pressclick')
    elif x<.70:L.append(f'{t} tap {r.choice(fn)}')
    elif x<.78:L.append(f'{t} tap 11')
    elif x<.92:L.append(f'{t} type {"".join(r.choice(letters) for _ in range(r.randint(1,6))).strip() or "a"}')
    elif x<.96:L.append(f'{t} serial status')
    else:L.append(f'{t} shot')
    t+=r.randint(60,500)
L.append(f'{t+500} quit')
open(out,'w').write('\n'.join(L)+'\n')
