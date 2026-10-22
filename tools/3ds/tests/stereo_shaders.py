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
    offsets = {}
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
        source = generated.read_text()
        if kind == "skin":
            assert "VSH_FVEC_u_bones 0x14" in registers and "VSH_ULEN_u_bones 75" in registers
        start = source.index("mov r11, u_" + ("stereoParams" if kind == "main" else "eye_quant"))
        offsets[kind] = source[start:source.index("mov\to_pos, r1" if kind == "main" else "mov o_pos, r1", start)]
        if kind == "main":
            offsets[kind] = offsets[kind].replace("\t.end", "")
        else:
            # Execute emitted decoders independently of their stereo parameter values.
            decoding = source[source.index("mul r0.xyz"):source.index("mova a0.x")]
            decoding += source[source.index("mul r10.xy"):source.index("mov o_tex0")]

    rng = random.Random(731)
    def run_instructions(instructions, registers):
        def read(token):
            sign = -1 if token.startswith("-") else 1
            name, _, swizzle = token.lstrip("-").partition(".")
            values = registers[name]
            if swizzle:
                values = [values["xyzw".index(c)] for c in swizzle]
                if len(values) == 1: values *= 4
            return [sign*v for v in values]
        for line in instructions.strip().splitlines():
            if not line.strip(): continue
            op, operands = line.strip().split(None, 1)
            dest, *sources = [item.strip() for item in operands.split(",")]
            result = read(sources[0])
            if op != "mov":
                other = read(sources[1])
                operation = {"mul":lambda a,b:a*b, "add":lambda a,b:a+b,
                             "min":min, "max":max}[op]
                result = [operation(a,b) for a,b in zip(result,other)]
            name, _, mask = dest.partition(".")
            registers.setdefault(name,[0.0]*4)
            for i, component in enumerate(mask or "xyzw"):
                registers[name]["xyzw".index(component)] = result[i]
        return registers

    def execute(kind, clip, shift, convergence, enabled):
        parameters = [1/32768, abs(shift), convergence*shift, shift]
        registers = {"r1": clip[:], "u_eye_quant": parameters, "u_stereoParams": parameters,
                     "u_stereoOn": [float(enabled),0,0,0], "zeros": [0.0]*4}
        return run_instructions(offsets[kind], registers)["r1"]

    for value in range(-32768,32768):
        registers = run_instructions(decoding, {
            "u_eye_quant": [1/32768,.023,-.173,.041], "ones": [1.0]*4, "zeros": [0.0]*4,
            "in_pos": [value]*4, "in_normal": [value]*4, "in_tex0": [value]*4})
        assert registers["r0"] == [value/8192.0]*3+[1]
        assert registers["r6"][:3] == [value/32768.0]*3
        assert registers["r10"] == [value/4096.0]*2+[0,1]
    for depth in (0.0,-1.0,-100.0):
        clip=[1.0,2.0,3.0,depth]
        assert execute("main",clip,.045,7,True)==clip
        assert execute("skin",clip,.045,7,True)==clip

    for _ in range(10000):
        clip = [rng.uniform(-100,100), rng.uniform(-100,100), rng.uniform(-100,100),
                rng.uniform(.0001,200)]
        shift, convergence = rng.uniform(-.045,.045), rng.uniform(2.5,7)
        raw = (clip[3]-convergence)*shift
        limit = clip[3]*abs(shift)
        expected = clip[1]-max(-limit,min(limit,raw))
        world = execute("main",clip,shift,convergence,True)
        character = execute("skin",clip,shift,convergence,True)
        assert math.isclose(world[1],expected,abs_tol=1e-10)
        assert world == character
        assert world[0] == clip[0] and world[2:] == clip[2:]
        assert abs(world[1]-clip[1])/clip[3] <= abs(shift)+1e-8
        assert execute("main",clip,shift,convergence,False) == clip # Flat HUD.
        assert execute("main",clip,0,convergence,True) == clip
        mirror = execute("main",clip,-shift,convergence,True)
        assert math.isclose(world[1]+mirror[1],2*clip[1],abs_tol=1e-10)
print("PASS: stereo shaders assemble; all 25 bones retained; packed scales exact; 10000 world/skin depth, foreground limit, eye symmetry and flat-HUD cases pass.")
