#!/usr/bin/env python3
"""Assemble stereo companions and exercise the emitted skin eye-offset arithmetic."""
from pathlib import Path
import math
import random
import re
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]
shaders = root / "libs/pure3d/pddi/pica/shaders"
with tempfile.TemporaryDirectory(prefix="srr-stereo-") as directory:
    path = Path(directory)
    for kind, name in [("main", "pddi_directional.v.pica"), ("skin", "skin_eye_directional.v.pica")]:
        generated = path / (kind + ".v.pica")
        header = path / (kind + ".h")
        subprocess.run([
            "cmake", "-DINPUT=" + str(shaders / name), "-DOUTPUT=" + str(generated),
            "-DMODE=" + kind, "-P", str(root / "cmake/stereo_shader.cmake"),
        ], check=True)
        subprocess.run([
            "/opt/devkitpro/tools/bin/picasso", "-o", str(path / (kind + ".shbin")),
            "-h", str(header), str(generated),
        ], check=True)
        registers = header.read_text()
        assert re.search(r"VSH_FVEC_u_(stereoParams|eye_quant) 0x5F", registers)
        if kind == "skin":
            assert "VSH_FVEC_u_bones 0x14" in registers
            assert "VSH_ULEN_u_bones 75" in registers
            source = generated.read_text()
            start = source.index("\tmov r11, u_eye_quant.wwww")
            offset = source[start:source.index("\tmov o_pos, r1", start)]
    rng = random.Random(731)
    for _ in range(1000):
        clip = [rng.uniform(-100, 100) for _ in range(4)]
        shift = rng.uniform(-0.045, 0.045)
        registers = {"r1": clip[:], "u_eye_quant": [1/8192, 1/32768, 1/4096, shift]}
        def read(token):
            sign = -1 if token.startswith("-") else 1
            token = token.lstrip("-")
            name, _, swizzle = token.partition(".")
            values = registers[name]
            if swizzle:
                values = [values["xyzw".index(c)] for c in swizzle]
                if len(values) == 1:
                    values *= 4
            return [sign * v for v in values]
        for line in offset.strip().splitlines():
            op, operands = line.strip().split(None, 1)
            dest, *sources = [item.strip() for item in operands.split(",")]
            a = read(sources[0])
            if op == "mov":
                result = a
            else:
                b = read(sources[1])
                result = [x*y if op == "mul" else x+y for x, y in zip(a, b)]
            name, _, mask = dest.partition(".")
            registers.setdefault(name, [0.0]*4)
            for i, component in enumerate(mask or "xyzw"):
                registers[name]["xyzw".index(component)] = result[i]
        assert math.isclose(registers["r1"][1], clip[1]-(clip[3]-7)*shift, abs_tol=1e-10)
        assert registers["r1"][0] == clip[0] and registers["r1"][2:] == clip[2:]
print("PASS: both stereo shaders assemble; all 25 GPU bones and packing scales retained; 1000 eye-offset cases match mono projection plus stereo shift.")
