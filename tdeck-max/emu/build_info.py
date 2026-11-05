#!/usr/bin/env python3
"""Generate emulator build metadata without writing into the firmware tree."""
from pathlib import Path
import sys
root = Path(__file__).resolve().parent.parent
board, destination = sys.argv[1:]
out = Path(destination).resolve()
(out / 'ui').mkdir(parents=True, exist_ok=True)
(out / 'mesh').mkdir(parents=True, exist_ok=True)
env = {'PROJECT_DIR': str(root), 'PIOENV': 'tdeck-plus' if board == 'plus' else 'mattermesh'}
code = (root / 'tools/ui_build_info.py').read_text()
code = code.replace("root/'src/mesh/version.h'", "output_root/'mesh/version.h'")
code = code.replace("root/'src/ui/build_info.h'", "output_root/'ui/build_info.h'")
exec(compile(code, 'tools/ui_build_info.py', 'exec'),
     {'Import': lambda name: None, 'env': env, 'output_root': out})
