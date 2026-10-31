#!/usr/bin/env python3
"""Exercise native mission-list clipping, animated touch targets and navigation."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]
with tempfile.TemporaryDirectory(prefix='mission-menu-') as directory:
    binary = Path(directory)/'check'
    subprocess.run(['c++', '-std=c++17', '-O1', '-g', '-fsanitize=address,undefined',
                    '-fno-sanitize-recover=all', '-I', str(root),
                    str(root/'tools/3ds/tests/dual_screen_ui.cpp'), '-o', str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
