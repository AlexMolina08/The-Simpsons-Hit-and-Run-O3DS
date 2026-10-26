#!/usr/bin/env python3
"""Check prepared colors and execute the generated reflection instructions."""
from pathlib import Path
import math
import random
import re
import subprocess
import struct
import tempfile

root = Path(__file__).resolve().parents[3]
program = r'''
#define SRR2_OLD3DS_VISUAL_UPGRADE
#define SRR2_OLD3DS_NATIVE_BENCH
#include <pddi/pica/characterform.hpp>
#include <pddi/pica/distancehaze.hpp>
#include <cassert>
#include <cstring>
#include <limits>
#include <initializer_list>
namespace picaVisual { unsigned features=0,reflectionDraws=0; }
namespace picaCharacterForm { bool enabled=true;unsigned groups=0,vertices=0; }
namespace picaDistanceHaze { bool enabled=true;unsigned builds=0,passes=0;float nearPlane=0,farPlane=0; }
int main() {
    unsigned random=19;
    for(unsigned i=0;i<100000;++i) {
        float normal[3];unsigned char original[4];
        for(unsigned j=0;j<3;++j) {
            random=random*1664525u+1013904223u;
            normal[j]=float(int(random%2001)-1000);
            original[j]=random%256;
        }
        original[3]=random%256;
        if(normal[0]==0 && normal[1]==0 && normal[2]==0)continue;
        unsigned char legacy[4],character[4],vehicle[4];
        memcpy(legacy,original,4);memcpy(character,original,4);memcpy(vehicle,original,4);
        picaVisual::features=0;assert(picaCharacterForm::Bake(normal,legacy));
        picaVisual::features=picaVisual::Characters;assert(picaCharacterForm::Bake(normal,character));
        assert(picaVisual::Bake(normal,vehicle,true));
        assert(character[3]==original[3] && vehicle[3]==original[3]);
        for(unsigned j=0;j<3;++j) {
            assert(character[j]<=original[j] && character[j]+1>=original[j]*.70f);
            assert(vehicle[j]<=original[j] && vehicle[j]+1>=original[j]*.56f);
        }
        assert(!picaVisual::Enabled(picaVisual::Vehicles));
    }
    const float bad[][3]={{0,0,0},{1,std::numeric_limits<float>::infinity(),0},
        {std::numeric_limits<float>::quiet_NaN(),1,0}};
    for(const auto& normal:bad) {
        unsigned char colour[]={17,99,240,73},before[4];memcpy(before,colour,4);
        assert(!picaVisual::Bake(normal,colour,true));assert(!memcmp(before,colour,4));
    }
    for(unsigned features:{0u,unsigned(picaVisual::Atmosphere)}) {
        picaVisual::features=features;
        float lut[256];assert(picaDistanceHaze::Build(lut,1,8000));
        for(unsigned j=0;j<128;++j) {
            assert(std::isfinite(lut[j]) && lut[j]>=0 && lut[j]<=1);
            assert(lut[j+128]>=0);
            assert(std::fabs(lut[j]+lut[j+128]-(j==127?1:lut[j+1]))<.00001f);
        }
    }
}
'''
with tempfile.TemporaryDirectory(prefix='srr-visual-style-') as name:
    folder = Path(name)
    (folder / 'test.cpp').write_text(program)
    subprocess.run(['clang++', '-std=c++14', '-O2', '-Wall', '-Wextra', '-Werror',
                    '-fsanitize=address,undefined', '-I' + str(root / 'libs/pure3d'),
                    str(folder / 'test.cpp'), '-o', str(folder / 'test')], check=True)
    subprocess.run([str(folder / 'test')], check=True)


def execute(code, registers):
    def read(source):
        if source.startswith('-'):
            return [-value for value in read(source[1:])]
        name, _, swizzle = source.partition('.')
        if '[a0' in source:
            match=re.match(r'(\w+)\[a0\.x\+(\d+)\](?:\.(\w+))?$',source)
            assert match,source
            name=f'{match[1]}[{int(registers["a0"][0])+int(match[2])}]'
            swizzle=match[3] or ''
        values = registers[name]
        swizzle = swizzle or 'xyzw'
        swizzle = swizzle.translate(str.maketrans('rgba','xyzw'))
        if len(swizzle) == 1:
            swizzle *= 4
        return [values['xyzw'.index(c)] for c in swizzle]

    active=[True]
    for line in code.splitlines():
        line = line.split(';', 1)[0].strip()
        if not line:
            continue
        if line=='.end':
            active.pop()
            continue
        if line=='.else':
            active[-1]=active[-2] and not active[-1]
            continue
        if line.startswith('ifu '):
            active.append(active[-1] and bool(read(line.split()[1])[0]))
            continue
        if line.startswith('ifc '):
            condition=line.split()[1]
            value=bool(read(condition.lstrip('!'))[0]) if active[-1] else False
            active.append(active[-1] and (not value if condition.startswith('!') else value))
            continue
        if not active[-1]:
            continue
        operation, arguments = line.split(None, 1)
        destination, *sources = [item.strip() for item in arguments.split(',')]
        values = [read(source) for source in sources]
        if operation == 'mov':
            result = values[0]
        elif operation == 'dp3':
            result = [sum(x*y for x, y in zip(values[0][:3], values[1][:3]))]*4
        elif operation == 'dp4':
            result = [sum(x*y for x, y in zip(values[0], values[1]))]*4
        elif operation == 'mova':
            result = [int(x) for x in values[0]]
        elif operation == 'rsq':
            result = [1/math.sqrt(value) for value in values[0]]
        elif operation == 'max':
            result = [max(x, y) for x, y in zip(*values)]
        elif operation == 'min':
            result = [min(x, y) for x, y in zip(*values)]
        elif operation == 'mul':
            result = [x*y for x, y in zip(*values)]
        elif operation == 'add':
            result = [x+y for x, y in zip(*values)]
        elif operation == 'mad':
            result = [x*y+z for x, y, z in zip(*values)]
        else:
            raise AssertionError(operation)
        name, _, mask = destination.partition('.')
        if name == 'o2':
            registers['output_writes'] = registers.get('output_writes',0)+1
        mask = mask.translate(str.maketrans('rgba','xyzw'))
        for component in mask or 'xyzw':
            index = 'xyzw'.index(component)
            registers[name][index] = result[index]
    return registers['o2'][2:] if 'o2' in registers else registers['o_env'][:2]


generator = root / 'cmake/reflection_shader.cmake'
source = root / 'libs/pure3d/pddi/pica/shaders/pddi_directional.v.pica'
with tempfile.TemporaryDirectory(prefix='srr-reflection-shader-') as name:
    output = Path(name) / 'reflection.v.pica'
    subprocess.run(['cmake', '-DINPUT=' + str(source), '-DOUTPUT=' + str(output),
                    '-P', str(generator)], check=True)
    shader = output.read_text()
    # Validate the assembler's output map, not only the generator's source text.
    # UV0 and UV1 must share register 2 without increasing the three-register
    # output footprint used by ordinary geometry.
    picasso = Path('/opt/devkitpro/tools/bin/picasso')
    binary = Path(name) / 'reflection.shbin'
    subprocess.run([str(picasso), '-o', str(binary), str(output)+'.ordinary', str(output)], check=True)
    assembled = binary.read_bytes()
    assert struct.unpack_from('<I',assembled,4)[0]==2
    ordinary_dvle=struct.unpack_from('<I',assembled,8)[0]
    dvle = struct.unpack_from('<I', assembled, 12)[0]
    offset, count = struct.unpack_from('<II', assembled, dvle+0x28)
    entries = [struct.unpack_from('<HHB3x', assembled, dvle+offset+8*i) for i in range(count)]
    assert entries == [(0,0,15), (2,1,15), (3,2,3), (5,2,12)], entries
    assert re.findall(r'^\.fvec.*$',shader,re.M)==re.findall(r'^\.fvec.*$',source.read_text(),re.M)
    assert re.findall(r'^\.bool.*$',shader,re.M)==re.findall(r'^\.bool.*$',source.read_text(),re.M)
    assert 'call vmain' in shader
    def parts(data):
        count=struct.unpack_from('<I',data,4)[0];at=8+4*count
        code,n=struct.unpack_from('<II',data,at+8);desc,dn=struct.unpack_from('<II',data,at+16)
        return data[at+code:at+code+4*n],data[at+desc:at+desc+8*dn]
    def uniforms(data,at):
        off,n=struct.unpack_from('<II',data,at+0x30);symbols=struct.unpack_from('<I',data,at+0x38)[0];result={}
        for i in range(n):
            symbol,first,last=struct.unpack_from('<IHH',data,at+off+8*i);start=at+symbols+symbol
            result[data[start:data.index(b'\0',start)].decode()]=(first,last)
        return result
    baseline=Path(name)/'baseline.shbin'
    subprocess.run([str(picasso),'-o',str(baseline),str(source)],check=True)
    old_code,old_desc=parts(baseline.read_bytes());new_code,new_desc=parts(assembled)
    assert new_code[:len(old_code)]==old_code,'Ordinary GPU instructions changed'
    assert new_desc[:len(old_desc)]==old_desc,'Ordinary operand descriptors changed'
    assert uniforms(assembled,ordinary_dvle)==uniforms(assembled,dvle)
    block = shader.split('; PackedCoordinatesBegin')[1].split('; PackedCoordinatesEnd')[0]
    assert 'ifu u_reflective' not in shader
    assert shader.index('Done:')<shader.index('; PackedCoordinatesBegin')
    output_writes = re.findall(r'^\s*\w+\s+o2(?:\.([xyzw]+))?\s*,', block, re.M)
    assert output_writes==['',''], 'Reflective entry needs one complete reflection output write'
    generator_random = random.Random(25010)
    for case in range(10000):
        angle = generator_random.uniform(-math.pi, math.pi)
        cosine, sine = math.cos(angle), math.sin(angle)
        normal = [generator_random.uniform(-1, 1) for _ in range(3)]
        length = math.sqrt(sum(value*value for value in normal))
        normal = [value/length for value in normal]
        view = [generator_random.uniform(-100, 100), generator_random.uniform(-30, 30),
                generator_random.uniform(1, 150)]
        registers = {f'r{i}': [0.0]*4 for i in range(16)}
        registers.update({'in_normal': normal+[0], 'in_tex0': [.2, .7, 0, 1],
            'o2': [0]*4, 'fixclr': [1/255]*4, 'zeros': [0]*4,'ones':[1]*4,'cmp':[0]*4,
            'u_stereoOn': [case%2, .5, 0, 0], 'worldSun':[0]*4, 'u_reflective':[1]*4,'r8': view+[1],
            'u_mv[0]': [cosine, 0, sine, 5], 'u_mv[1]': [0, 1, 0, -3],
            'u_mv[2]': [-sine, 0, cosine, 10]})
        actual = execute(block, registers)
        assert registers['output_writes']==1
        transformed = [cosine*normal[0]+sine*normal[2], normal[1], -sine*normal[0]+cosine*normal[2]]
        view_length = math.sqrt(sum(value*value for value in view))
        direction = [value/view_length for value in view]
        dot = sum(x*y for x, y in zip(transformed, direction))
        reflection = [x-2*dot*y for x, y in zip(direction, transformed)]
        expected = [.5+.5*reflection[0], .5-.5*reflection[1]]
        assert all(abs(x-y)<1e-12 for x, y in zip(actual, expected))
        assert all(-1e-12<=value<=1+1e-12 for value in actual)
        assert registers['o2'][:2] == [.2, .7]
        assert registers['r8'] == view+[1]
        registers['worldSun']=[1]*4
        registers['in_pos']=[generator_random.uniform(-1000,1000),0,generator_random.uniform(-1000,1000),1]
        registers['u_lightPos[0]']=[.001,-.002,.25,.75]
        registers['output_writes']=0
        before_colour=registers.get('r0',[0]*4)[:]
        execute(block,registers)
        assert registers['output_writes']==1
        assert registers['o2'][:2]==[.2,.7]
        assert registers['o2'][2:]==[registers['in_pos'][0]*.001+.25,registers['in_pos'][2]*-.002+.75]
        assert registers.get('r0',[0]*4)==before_colour
        # Ordinary geometry is established by the byte-identical baseline prefix.

    vehicle_block = shader.split('; PreparedVehicleLightBegin')[1].split('; PreparedVehicleLightEnd')[0]
    character_shader = Path(name)/'character.v.pica'
    subprocess.run(['cmake', '-DINPUT='+str(root/'libs/pure3d/pddi/pica/shaders/skin_eye_directional.v.pica'),
                    '-DOUTPUT='+str(character_shader), '-P',str(root/'cmake/character_light_shader.cmake')],check=True)
    character_source = character_shader.read_text()
    character_block = character_source.split('; PreparedCharacterLightBegin')[1].split('; PreparedCharacterLightEnd')[0]
    for case in range(10000):
        normal=[generator_random.uniform(-1,1) for _ in range(3)]
        length=math.sqrt(sum(x*x for x in normal))
        unit=[x/length for x in normal]
        colour=[generator_random.random() for _ in range(4)]
        for enabled in (False,True):
            registers={f'r{i}':[0.0]*4 for i in range(16)}
            registers.update({'r0':colour[:],'r5':normal+[0],'fixclr':[1/255]*4,
                'zeros':[0]*4,'ones':[1]*4,'o_env':[0]*4,
                'u_eye_matAmbient':[-.35,.65,-.68,1] if enabled else [0,0,0,1],
                'u_eye_matEmissive':[.50,.44,.34,0] if enabled else [0]*4,
                'u_eye_sceneAmbient':[.70,.74,.82,1/255] if enabled else [1,1,1,1/255]})
            execute(character_block,registers)
            # The shader's finite-normal floor is deliberate and matches production.
            inverse=1/math.sqrt(max(1/255,length*length))
            key=max(0,sum(a*b*inverse for a,b in zip(normal,[-.35,.65,-.68]))) if enabled else 0
            expected=[colour[i]*([.70,.74,.82][i]+[.50,.44,.34][i]*key) if enabled else colour[i] for i in range(3)]
            assert all(abs(x-y)<1e-12 for x,y in zip(registers['r0'][:3],expected))
            assert registers['r0'][3]==colour[3]
            registers={f'r{i}':[0.0]*4 for i in range(16)}
            view=[generator_random.uniform(-1,1) for _ in range(3)]
            view_length=math.sqrt(sum(x*x for x in view));view=[x/view_length for x in view]
            dot=sum(x*y for x,y in zip(unit,view))
            registers.update({'r0':colour[:],'r2':unit+[0],'r3':[0,0,2*dot,0],
                              'zeros':[0]*4,'ones':[1]*4,'o_env':[0]*4,'u_reflective':[1]*4,'cmp':[0]*4,
                              'u_stereoOn':[case%2,.5,.62 if enabled else 1,.56 if enabled else 0]})
            execute(vehicle_block,registers)
            shade=.62+.56*max(0,min(1,.5+.5*(unit[1]-unit[0]))) if enabled else 1
            assert all(abs(registers['r0'][i]-colour[i]*shade)<1e-12 for i in range(3))
            assert registers['r0'][3]==colour[3]
            registers['r0']=colour[:];registers['cmp']=[1]*4
            execute(vehicle_block,registers)
            assert registers['r0']==colour, 'Lit materials retain authored lighting without an extra body term'
print('PASS: 100000 prepared color cases, exact alpha, legacy switch, finite fog LUTs; '
      '10000 emitted reflection, world XZ coordinate, GPU key/fill and side-key cases preserve alpha, base UV and eye position. Not hardware performance evidence.')
