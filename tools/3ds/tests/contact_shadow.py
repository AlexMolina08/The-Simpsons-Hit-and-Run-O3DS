#!/usr/bin/env python3
"""Check bounded shadow data and execute the production GPU plane projection."""
import math
import random
import re
import runpy
import subprocess
import tempfile
from pathlib import Path

root=Path(__file__).resolve().parents[3]
execute=runpy.run_path(str(root/'tools/3ds/tests/visual_style.py'))['execute']
program=r'''
#include <pddi/pica/contactshadow.hpp>
#include <cassert>
#include <limits>
#include <cstring>
#include <random>
int main(){
 using namespace picaContact;
 Vertex vertices[Capacity*Vertices+1];unsigned short indices[Capacity*Indices+1];
 memset(vertices,0xa7,sizeof(vertices));memset(indices,0xa7,sizeof(indices));
 Mesh(vertices,indices);
 assert(indices[Capacity*Indices]==0xa7a7);
 const unsigned char* guard=reinterpret_cast<unsigned char*>(vertices+Capacity*Vertices);
 for(unsigned i=0;i<sizeof(Vertex);++i)assert(guard[i]==0xa7);
 for(unsigned instance=0;instance<Capacity;++instance){
  for(unsigned i=0;i<Indices;++i)assert(indices[instance*Indices+i]>=instance*Vertices && indices[instance*Indices+i]<(instance+1)*Vertices);
  for(unsigned i=0;i<Vertices;++i){const Vertex& v=vertices[instance*Vertices+i];
   assert(v.offset[0]==3*instance && !v.offset[1] && !v.offset[2] && !v.offset[3]);
   assert(v.x*v.x+v.y*v.y<=1.000001f && v.alpha==(i<=8?1:0));}
 }
 float c[]={1,2,3},n[]={0,1,0},f[]={0,0,1};Instance out;
 float point[]={4,5,6},slope[]={.2f,1,.3f},height=0;
 assert(GroundY(height,c,point,slope) && std::fabs(height-6.5f)<.00001f);
 slope[1]=0;assert(!GroundY(height,c,point,slope));
 slope[1]=1;point[0]=std::numeric_limits<float>::infinity();assert(!GroundY(height,c,point,slope));
 float view[]={1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
 assert(Depth(view,c)==3);view[14]=12;assert(Depth(view,c)==15);
 Matrix projection;projection.SetPerspective(1.0f,400.0f/240.0f,1.0f,8000.0f);
 assert(projection.Row4(2).w==1 && projection.Row4(3).w==0);
 assert(Depth(view,c)*projection.Row4(2).w==15);
 assert(Prepare(out,c,n,f,1,2,.48f));assert(out.center[1]>2 && out.center[3]==.48f);
 Instance before=out;n[1]=0;assert(!Prepare(out,c,n,f,1,2,.48f));assert(!memcmp(&out,&before,sizeof(out)));
 n[1]=1;c[0]=std::numeric_limits<float>::quiet_NaN();assert(!Prepare(out,c,n,f,1,2,.48f));
}
'''
matrix=(root/'libs/radmath/radmath/matrix.cpp').read_text()
perspective=matrix[matrix.index('void Matrix::SetPerspective('):matrix.index('void Matrix::SetOrthographic(')]
declarations=r'''
struct Row {float x,y,z,w;void Set(float a,float b,float c,float d){x=a;y=b;z=c;w=d;}};
struct Matrix {Row rows[4];Row& Row4(unsigned i){return rows[i];}
 void SetPerspective(float,float,float,float);};
float Tan(float x){return std::tan(x);}
'''
program=program.replace('int main(){',declarations+perspective+'\nint main(){')
with tempfile.TemporaryDirectory(prefix='srr-contact-shadow-') as directory:
    folder=Path(directory);(folder/'test.cpp').write_text(program)
    subprocess.run(['clang++','-std=c++14','-O2','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',
                    '-I'+str(root/'libs/pure3d'),str(folder/'test.cpp'),'-o',str(folder/'test')],check=True)
    subprocess.run([str(folder/'test')],check=True)

shader=(root/'libs/pure3d/pddi/pica/shaders/contact_shadow.v.pica').read_text()
context=(root/'libs/pure3d/pddi/pica/context.cpp').read_text()
teardown=context[context.index('picaContext::~picaContext()'):]
drain=teardown.index('C3Di_RenderQueueWaitDone();')
for release in ('picaNativeRegisterContactProgram(nullptr)','linearFree(contactVertices)',
                'linearFree(contactIndices)','shaderProgramFree(&contactProgram)'):
    assert drain<teardown.index(release), 'Final GPU work must retire before shadow resources are released'
vehicle=(root/'code/worldsim/redbrick/vehicle.cpp').read_text()
queue=vehicle.split('if(picaVisual::Enabled(picaVisual::ContactShadows) && IsSimpleShadow())')[1].split('#endif')[0]
assert not re.search(r'\bGetGroundY\s*\(',queue), 'Shadow placement must not request lazy wheel animation'
code=shader.split('.proc vmain')[1].split('.end')[0].replace('    end','')
for output in ('o_pos','o_colour'):
    writes=re.findall(r'^\s*\w+\s+'+output+r'(?:\.([xyzwrgba]+))?\s*,',code,re.M)
    assert writes==[''],(output,writes)

def cross(a,b):return [a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]]
def unit(a):
    scale=math.sqrt(sum(v*v for v in a));return [v/scale for v in a]
def transform(rows,a):return [sum(x*y for x,y in zip(row,a)) for row in rows]
rng=random.Random(25108)
for trial in range(5000):
    normal=unit([rng.uniform(-.6,.6),1,rng.uniform(-.6,.6)])
    facing=unit([rng.uniform(-1,1),rng.uniform(-.5,.5),rng.uniform(-1,1)])
    center=[rng.uniform(-20,20) for _ in range(3)]
    width,length,alpha=rng.uniform(.2,2),rng.uniform(.2,3),rng.uniform(.1,.5)
    x,y,falloff=rng.uniform(-1,1),rng.uniform(-1,1),rng.choice([0.,1.])
    view=[[1,0,0,3],[0,1,0,2],[0,0,1,8],[0,0,0,1]]
    projection=[[0,1.3,0,0],[-1.9,0,0,0],[0,0,1.01,-1.01],[0,0,1,0]]
    stereo=trial%2;shift=rng.uniform(-.02,.02);bias=shift*rng.uniform(5,20)
    registers={f'r{i}':[0.0]*4 for i in range(16)}
    instance=trial%12
    registers.update({'a0':[0]*4,'in_offset':[instance*3,0,0,0],'in_corner':[x,y,falloff,0],
        'ones':[1]*4,'zeros':[0]*4,'epsilon':[.000001]*4,'o_pos':[0]*4,'o_colour':[0]*4,'o_env':[0]*4,
        'u_constants':[1,0,.000001,stereo],'u_stereoParams':[1/32768,abs(shift),bias,shift],
        f'u_instances[{3*instance}]':center+[alpha],f'u_instances[{3*instance+1}]':normal+[width],
        f'u_instances[{3*instance+2}]':facing+[length]})
    registers.update({f'u_view[{i}]':v for i,v in enumerate(view)})
    registers.update({f'u_proj[{i}]':v for i,v in enumerate(projection)})
    execute(code,registers)
    side=cross(normal,facing);inverse=1/math.sqrt(max(.000001,sum(v*v for v in side)))
    side=[v*inverse for v in side];forward=cross(side,normal)
    point=[center[i]+side[i]*width*x+forward[i]*length*y for i in range(3)]
    expected=transform(projection,transform(view,point+[1]))
    expected[1]-=stereo*(expected[3]*shift-bias)
    assert max(abs(a-b) for a,b in zip(registers['o_pos'],expected))<1e-10
    assert registers['o_colour']==[0,0,0,alpha*falloff]
print('PASS: bounded 12-instance mesh, invalid ground fallback, 5000 GPU ground-plane/stereo projections and complete outputs. Not hardware timing.')
