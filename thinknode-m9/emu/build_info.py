#!/usr/bin/env python3
# Runs tools/ui_build_info.py (the pio pre-script) outside SCons, for the tdeck-plus env: writes src/ui/build_info.h
# and src/mesh/version.h exactly as a pio build would.
from pathlib import Path
root = Path(__file__).resolve().parent.parent
env = {"PROJECT_DIR": str(root), "PIOENV": "tdeck-plus"}
code = (root / "tools/ui_build_info.py").read_text()
exec(compile(code, "tools/ui_build_info.py", "exec"), {"Import": lambda name: None, "env": env})
