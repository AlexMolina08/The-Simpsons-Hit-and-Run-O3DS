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
            assert "VSH_FLAG_u_stereoEnabled BIT(3)" in registers # b3, separate from all 25 bone matrices.
        start = source.index("mov r11, u_stereoParams" if kind == "main" else "ifu u_stereoEnabled")
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
        active = [True]
        for line in instructions.strip().splitlines():
            if not line.strip(): continue
            line = line.strip()
            if line.startswith("ifu "):
                active.append(active[-1] and bool(registers[line.split()[1]][0]))
                continue
            if line == ".end":
                active.pop()
                continue
            if not active[-1]: continue
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
                     "u_stereoOn": [float(enabled),0,0,0], "u_stereoEnabled": [float(enabled)], "zeros": [0.0]*4}
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
        expected=[1.0,2.0-(depth-7)*.045,3.0,depth]
        for kind in ("main","skin"):
            assert all(math.isclose(u,v,abs_tol=1e-10) for u,v in zip(execute(kind,clip,.045,7,True),expected))

    for _ in range(10000):
        clip = [rng.uniform(-100,100), rng.uniform(-100,100), rng.uniform(-100,100),
                rng.uniform(.0001,200)]
        shift, convergence = rng.uniform(-.045,.045), rng.uniform(2.5,7)
        raw = (clip[3]-convergence)*shift
        expected = clip[1]-raw
        world = execute("main",clip,shift,convergence,True)
        character = execute("skin",clip,shift,convergence,True)
        assert math.isclose(world[1],expected,abs_tol=1e-10)
        assert world == character
        assert world[0] == clip[0] and world[2:] == clip[2:]
        assert execute("main",clip,shift,convergence,False) == clip # Flat HUD.
        assert execute("skin",clip,shift,convergence,False) == clip # Skinned GUI objects must obey the same scope.
        assert execute("main",clip,0,convergence,True) == clip
        mirror = execute("main",clip,-shift,convergence,True)
        assert math.isclose(world[1]+mirror[1],2*clip[1],abs_tol=1e-10)

    # A foot touching a road triangle must share that triangle's projection.
    # Clamping each vertex separately bends coarse roads away from fine meshes.
    for kind in ("main", "skin"):
        a, b = [0.0,0.0,0.0,1.0], [0.0,0.0,0.0,10.0]
        blend = lambda x,y: [(2*u+v)/3 for u,v in zip(x,y)]
        contact = execute(kind,blend(a,b),.045,7,True)
        road = blend(execute(kind,a,.045,7,True),execute(kind,b,.045,7,True))
        pair_error_pixels = 400*abs(contact[1]-road[1])/contact[3]
        assert pair_error_pixels < .00001, f"{kind}: foot/road separation {pair_error_pixels:.3f} pixels"
    for _ in range(10000):
        # Shared vertices, arbitrary tessellation and near-plane clipping must
        # commute with the same projection in world and character programs.
        a=[rng.uniform(-10,10) for _ in range(4)]
        b=[rng.uniform(-10,10) for _ in range(4)]
        t=rng.random()
        blend=lambda x,y:[u*(1-t)+v*t for u,v in zip(x,y)]
        for kind in ("main","skin"):
            point=execute(kind,blend(a,b),.02,1.75,True)
            edge=blend(execute(kind,a,.02,1.75,True),execute(kind,b,.02,1.75,True))
            assert all(math.isclose(u,v,abs_tol=1e-10) for u,v in zip(point,edge))
print("PASS: stereo shaders assemble; 25 bones and packed scales retained; matching world/skin projections, foot/road contact, tessellation, clipping, eye symmetry and flat HUD.")
