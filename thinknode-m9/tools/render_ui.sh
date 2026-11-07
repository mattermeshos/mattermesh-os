#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
g++ -std=c++17 -O2 -DARDUINO=10800 -Isim/stubs -Isrc -I'hardware/lib/Adafruit GFX Library' sim/preview.cpp src/ui/components.cpp src/ui/artwork.cpp src/ui/renderer.cpp 'hardware/lib/Adafruit GFX Library/Adafruit_GFX.cpp' -o artifacts/ui-preview -lz
artifacts/ui-preview screenshots/mattermesh
"${PYTHON:-python3}" - <<'PY'
from pathlib import Path
from PIL import Image
for p in Path('screenshots/mattermesh').glob('*.pbm'):
 im=Image.open(p);im.save(p.with_suffix('.png'));im.resize((im.width*3,im.height*3),Image.Resampling.NEAREST).save(p.with_name(p.stem+'-3x.png'))
PY
