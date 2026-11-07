#!/usr/bin/env python3
"""Reference screenshots: open the main pages in the emulator and compare each one with emu/golden/<name>.png.
  python3 emu/tests/golden.py [--update] [--only NAME]
The status bar (clock, battery) is masked; a page fails when more than 0.5 % of its pixels change ("name ~3" in
pages.txt: 3 % for a page with a big clock). --update rewrites the
references after a change that is meant to be seen (look at them before committing). Pages: emu/golden/pages.txt, one per
line "name | script steps separated by ;" (steps as in emu scripts: tap, roll, type, serial, with times relative to the
page start, each page in a fresh emulator from Home). The board comes from the tree: artifacts/emu-m9 or artifacts/emu-plus."""
import os, shutil, subprocess, sys, tempfile
from PIL import Image, ImageChops

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
GOLD = os.path.join(ROOT, 'emu', 'golden')
STATUS_H, TOLERANCE = 18, 0.005

def pages():
    out = []
    for line in open(os.path.join(GOLD, 'pages.txt')):
        line = line.strip()
        if not line or line.startswith('#'): continue
        name, steps = (x.strip() for x in line.split('|', 1))
        name, _, tol = name.partition('~')  # "home ~3": a page with a clock may change up to 3 %
        out.append((name.strip(), [s.strip() for s in steps.split(';') if s.strip()], float(tol) / 100 if tol else TOLERANCE))
    return out

def run_one(emu, item, data, index):
    """One page in its own emulator run (serial commands queue up when sent back to back, so a shared run drifts):
    from Home, the page's steps, a shot."""
    name, steps, _ = item
    folder = os.path.join(data, name); os.makedirs(folder)
    for x in ('sd', 'nvs.json'):
        src = os.path.join(ROOT, 'emu', 'profiles', 'configured', x)
        (shutil.copytree if os.path.isdir(src) else shutil.copy)(src, os.path.join(folder, x))
    script, at = [], 9000  # the Plus emulator ignores serial commands until its boot ends (7 s was sometimes short: the page stayed on Home)
    for st in steps:
        script.append(f'{at} {st}'); at += 1200
    at += 800; script.append(f'{at} shot'); at += 600; script.append(f'{at} quit')
    open(os.path.join(folder, 's.emu'), 'w').write('\n'.join(script) + '\n')
    env = dict(os.environ, MM_EMU_UNLOCKED='1')
    with open(os.path.join(folder, 'log'), 'w') as log:
        subprocess.run([emu, '--headless', '--data', folder, '--air', 'off', '--port-offset', str(3000 + 10 * index),
                        '--script', os.path.join(folder, 's.emu')], cwd=ROOT, env=env, stdin=subprocess.DEVNULL,
                       stdout=log, stderr=subprocess.STDOUT, timeout=at / 1000 + 60)
    return os.path.join(folder, 'screenshot-1.ppm')

def run(emu, items, data):
    from concurrent.futures import ThreadPoolExecutor
    with ThreadPoolExecutor(4) as pool:
        return list(pool.map(lambda a: run_one(emu, a[1], data, a[0]), enumerate(items)))

def diff(a, b):
    a, b = a.convert('RGB'), b.convert('RGB')
    if a.size != b.size: return 1.0, None
    w, h = a.size
    d = ImageChops.difference(a.crop((0, STATUS_H, w, h)), b.crop((0, STATUS_H, w, h))).convert('L').point(lambda v: 255 if v > 24 else 0)
    changed = sum(1 for v in d.getdata() if v)
    return changed / (w * (h - STATUS_H)), d

def main():
    update = '--update' in sys.argv
    only = sys.argv[sys.argv.index('--only') + 1] if '--only' in sys.argv else None
    emu = next((os.path.join(ROOT, 'artifacts', e) for e in ('emu-m9', 'emu-plus') if os.path.exists(os.path.join(ROOT, 'artifacts', e))), None)
    if not emu: sys.exit('build the emulator first (make -f emu/Makefile)')
    items = [p for p in pages() if not only or p[0] == only]
    data = tempfile.mkdtemp()
    try:
        shots, failed = run(emu, items, data), []
        for (name, _, tolerance), shot in zip(items, shots):
            ref = os.path.join(GOLD, name + '.png')
            if not os.path.exists(shot): failed.append(f'{name}: no screenshot'); continue
            img = Image.open(shot)
            if update or not os.path.exists(ref):
                img.save(ref); print(f'saved {name}'); continue
            share, mask = diff(img, Image.open(ref))
            if share > tolerance:
                out = os.path.join(ROOT, 'artifacts', 'golden-diff'); os.makedirs(out, exist_ok=True)
                img.save(os.path.join(out, name + '.png'))
                if mask: mask.save(os.path.join(out, name + '-diff.png'))
                failed.append(f'{name}: {share * 100:.2f} % of the pixels changed (artifacts/golden-diff/{name}.png)')
            else: print(f'ok {name} ({share * 100:.2f} %)')
        if failed:
            print('FAIL: ' + '; '.join(failed)); sys.exit(1)
        print(f'PASS: {len(items)} pages match their reference screenshots')
    finally:
        shutil.rmtree(data, ignore_errors=True)

main()
