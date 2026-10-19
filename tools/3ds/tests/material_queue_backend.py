#!/usr/bin/env python3
"""Execute production PICA queue eligibility, bounds and submission with GPU spies."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]
source = (root/'libs/pure3d/pddi/pica/context.cpp').read_text()
def function(signature):
    start = source.index(signature)
    first = source.index('{',start)
    depth = 1
    end = first+1
    while depth:
        depth += (source[end]=='{') - (source[end]=='}')
        end += 1
    return source[start:end]

harness = r'''
#include <cassert>
#include <cstring>
#include <vector>
#include <cstdio>
#include "libs/pure3d/pddi/pica/materialqueue.hpp"
const unsigned PICA_VERTEX_STRIDE = 36;
enum { PDDI_MATRIX_MODELVIEW, PDDI_COMPARE_LESS, PDDI_COMPARE_LESSEQUAL };
int g_picaInFrame = 1;
struct Vec { float values[4]; float operator[](unsigned i) const { return values[i]; } };
struct pddiMatrix { Vec rows[4]; const Vec& Row4(unsigned i) const { return rows[i]; } };
struct Stack { pddiMatrix value; const pddiMatrix* Top() { return &value; } };
struct FVec { float w,z,y,x; };
struct C3DMtx { FVec r[4]; };
struct pddiShader { bool opaque = true; };
struct picaMat : pddiShader {
    unsigned nativeState = 0;
    bool QueueOpaque() const { return opaque; }
    bool CanApplyQueued(picaMat* previous) { return previous && nativeState == previous->nativeState; }
    void ApplyQueued(picaMat*) {}
};
struct pddiPrimBuffer {};
std::vector<unsigned> actualDraws;
struct picaPrimBuffer : pddiPrimBuffer {
    bool transient = false, queueBoundsValid = false, supported = true, indicesValid = true;
    int elemState = -1;
    unsigned char* buffer = nullptr;
    unsigned short* indices = nullptr;
    unsigned allocated = 0, id = 0;
    float queueLow[3], queueHigh[3];
    bool PrimQueueSupported() const { return supported; }
    bool IndicesInRange() { return indicesValid; }
    bool QueuePersistent(); bool QueueBounds(float*,float*);
    void Display() { actualDraws.push_back(id); }
};
struct RenderState { bool zEnabled = true, zWrite = true; int zCompare = PDDI_COMPARE_LESS; };
struct Stencil { bool enabled = false; };
struct picaContext {
    struct Submission { pddiShader* material; picaPrimBuffer* buffer; picaQueue::Rect rect; };
    bool bottomPass = false;
    RenderState render; Stencil stencil; Stack stack;
    struct State { RenderState* renderState; Stencil* stencilState; Stack* matrixStack[1]; } state;
    C3DMtx hwProjection = {};
    unsigned materialCalls = 0;
    pddiShader* staticQueuePrevious = nullptr;
    picaContext() {
        state.renderState = &render; state.stencilState = &stencil; state.matrixStack[0] = &stack;
        std::memset(&stack,0,sizeof(stack));
        for(unsigned a=0;a<4;++a) stack.value.rows[a].values[a]=1;
        hwProjection.r[0].x=hwProjection.r[1].y=hwProjection.r[2].z=hwProjection.r[3].w=1;
    }
    bool CanQueue(pddiShader*,pddiPrimBuffer*);
    void SubmitMaterialQueue(Submission*,unsigned);
    void SubmitQueuedPrim(pddiShader*,picaPrimBuffer*);
    void SubmitQueuedOne(pddiShader*,picaPrimBuffer*,pddiShader*&);
    void DrawPrimBuffer(pddiShader*,pddiPrimBuffer* b) { ++materialCalls; static_cast<picaPrimBuffer*>(b)->Display(); }
};
'''
tests = r'''
int main() {
    picaContext context; picaMat a,b; b.nativeState=1;
    float vertices[3][27] = {};
    picaPrimBuffer buffers[3];
    unsigned short indices[3] = {0,1,2};
    for(unsigned i=0;i<3;++i) {
        for(unsigned v=0;v<3;++v) { vertices[i][v*9]=float(i)*10; vertices[i][v*9+1]=float(v); }
        buffers[i].buffer=reinterpret_cast<unsigned char*>(vertices[i]);
        buffers[i].allocated=3; buffers[i].id=i; buffers[i].indices=indices;
        assert(context.CanQueue(&a,&buffers[i]));
    }
    picaContext::Submission queue[] = {{&a,&buffers[0],{}},{&b,&buffers[1],{}},{&a,&buffers[2],{}}};
    context.SubmitMaterialQueue(queue,3);
    assert((actualDraws==std::vector<unsigned>{0,2,1}) && context.materialCalls==2);
    // Same pixels retain original ordering, including exact depth ties.
    for(unsigned i=0;i<3;++i) {
        for(unsigned v=0;v<3;++v) vertices[i][v*9]=0;
        buffers[i].queueBoundsValid=false;
        queue[i] = {i==1 ? &b : &a,&buffers[i],{}};
    }
    actualDraws.clear(); context.materialCalls=0;
    context.SubmitMaterialQueue(queue,3);
    assert((actualDraws==std::vector<unsigned>{0,1,2}) && context.materialCalls==3);
    context.render.zWrite=false; assert(!context.CanQueue(&a,&buffers[0])); context.render.zWrite=true;
    context.stencil.enabled=true; assert(!context.CanQueue(&a,&buffers[0])); context.stencil.enabled=false;
    a.opaque=false; assert(!context.CanQueue(&a,&buffers[0])); a.opaque=true;
    buffers[0].transient=true; assert(!context.CanQueue(&a,&buffers[0])); buffers[0].transient=false;
    buffers[0].elemState=-1; buffers[0].indicesValid=false; assert(!context.CanQueue(&a,&buffers[0]));
    buffers[0].elemState=-1; buffers[0].indicesValid=true; assert(context.CanQueue(&a,&buffers[0]));
    float low[3],high[3];
    vertices[0][0]=NAN; buffers[0].queueBoundsValid=false;
    assert(!buffers[0].QueueBounds(low,high));
    vertices[0][0]=INFINITY; assert(!buffers[0].QueueBounds(low,high));
    vertices[0][0]=123; assert(buffers[0].QueueBounds(low,high) && high[0]==123);
    // Streaming packets preserve draw order while sharing compatible native
    // pipeline state across different material objects.
    actualDraws.clear(); context.materialCalls=0; context.staticQueuePrevious=nullptr;
    b.nativeState=a.nativeState;
    for(unsigned i=0;i<3;++i) context.SubmitQueuedPrim(i==1 ? &b : &a,&buffers[i]);
    assert((actualDraws==std::vector<unsigned>{0,1,2}) && context.materialCalls==1);
    std::puts("PASS production CanQueue/QueuePersistent/QueueBounds/SubmitMaterialQueue: actual draw order, 3->2 material calls, unchanged draws, overlap ties, invalid indices, transient/depth/stencil/blend fallback, nonfinite bounds");
}
'''
functions = '\n'.join(function(name) for name in [
    'bool picaContext::CanQueue(', 'bool picaPrimBuffer::QueuePersistent(',
    'bool picaPrimBuffer::QueueBounds(', 'void picaContext::SubmitQueuedOne(', 'void picaContext::SubmitQueuedPrim(', 'void picaContext::SubmitMaterialQueue('])
with tempfile.TemporaryDirectory(prefix='srr2-queue-backend-') as directory:
    path=Path(directory)
    (path/'test.cpp').write_text(harness+'\n'+functions+'\n'+tests)
    subprocess.run(['clang++','-DSRR2_OLD3DS_MATERIAL_QUEUE_SORT','-std=c++11','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I',str(root),str(path/'test.cpp'),'-o',str(path/'test')],check=True)
    subprocess.run([str(path/'test')],check=True)
