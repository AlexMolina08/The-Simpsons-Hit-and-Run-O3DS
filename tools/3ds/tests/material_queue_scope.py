#!/usr/bin/env python3
"""Compile the production geometry dispatcher against side-effect/lifetime probes."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]
source = (root / 'libs/pure3d/p3d/geometry.cpp').read_text()
start = source.index('void tGeometry::Display()')
end = source.index('//------------------------------------------------------------------------', start)
body = source[start:end]
harness = r'''
#include <vector>
#include <typeinfo>
#include <cassert>
#include <cstdio>
#include "libs/pure3d/pddi/pica/materialqueue.hpp"
unsigned g_old3dsActiveEntity = 0;
enum { O3_STATIC = 1 };
struct pddiShader { unsigned revision = 0; };
struct pddiPrimBuffer { unsigned revision = 0; bool persistent = true; virtual ~pddiPrimBuffer() {} };
struct picaPrimBuffer : pddiPrimBuffer {};
struct tShader { pddiShader shader; pddiShader* GetShader() { return &shader; } };
struct Draw { unsigned material, vertices; };
std::vector<Draw> draws;
void (*nestedHook)() = nullptr;
struct picaContext {
    struct Submission { pddiShader* material; picaPrimBuffer* buffer; picaQueue::Rect rect; };
    bool staticQueueActive = false;
    unsigned staticQueueCount = 0;
    pddiShader* staticQueuePrevious = nullptr;
    Submission staticQueue[picaQueue::Capacity];
    unsigned flushes = 0;
    void BeginStaticQueue() { staticQueueActive=true; staticQueueCount=0; staticQueuePrevious=nullptr; }
    void EndStaticQueue() { SubmitMaterialQueue(staticQueue,staticQueueCount); staticQueueCount=0; staticQueuePrevious=nullptr; staticQueueActive=false; }
    void SubmitQueuedPrim(pddiShader* material,picaPrimBuffer* buffer) { draws.push_back({material->revision,buffer->revision}); staticQueuePrevious=material; }
    bool CanQueue(pddiShader*,pddiPrimBuffer* b) { return b && b->persistent; }
    void SubmitMaterialQueue(Submission* entries,unsigned count) {
        assert(count <= picaQueue::Capacity);
        if(count) ++flushes;
        for(unsigned i=0;i<count;++i) draws.push_back({entries[i].material->revision,entries[i].buffer->revision});
    }
};
namespace p3d { picaContext context; picaContext* pddi = &context; }
struct tPrimGroup {
    tShader material;
    virtual ~tPrimGroup() {}
    virtual void Display() = 0;
    tShader* GetShader() { return &material; }
};
struct tPrimGroupOptimised : tPrimGroup {
    picaPrimBuffer buffer;
    unsigned calls = 0;
    pddiPrimBuffer* GetBuffer() { return &buffer; }
    void Display() override { ++calls; draws.push_back({material.shader.revision,buffer.revision}); }
};
struct Mutator : tPrimGroupOptimised {
    tPrimGroupOptimised* target;
    explicit Mutator(tPrimGroupOptimised* t):target(t) {}
    void Display() override {
        // Deferring across this point would observe mutated data in earlier draws.
        ++calls;
        if(nestedHook) nestedHook();
        ++target->material.shader.revision;
        ++target->buffer.revision;
    }
};
struct Groups : std::vector<tPrimGroup*> { unsigned Size() const { return size(); } };
struct tGeometry { Groups primGroup; void Display(); };
'''
tests = r'''
int main() {
    tPrimGroupOptimised a;
    Mutator mutation(&a);
    tGeometry geometry;
    geometry.primGroup = Groups();
    geometry.primGroup.push_back(&a);
    geometry.primGroup.push_back(nullptr);
    geometry.primGroup.push_back(&mutation);
    geometry.primGroup.push_back(&a);
    g_old3dsActiveEntity = O3_STATIC;
    geometry.Display();
    assert(draws.size()==2 && draws[0].vertices==0 && draws[1].vertices==1);
    assert(draws[0].material==0 && draws[1].material==1 && mutation.calls==1);
    draws.clear();
    geometry.primGroup.assign(97,&a);
    geometry.Display();
    assert(draws.size()==97);
#ifdef SRR2_OLD3DS_MATERIAL_QUEUE
    assert(a.calls==0);
#ifdef SRR2_OLD3DS_MATERIAL_QUEUE_SORT
    assert(p3d::context.flushes==6);
#endif
#else
    assert(a.calls==99);
#endif
    draws.clear();
    a.buffer.persistent=false;
    geometry.Display();
    assert(draws.size()==97);
    g_old3dsActiveEntity=0;
    draws.clear();
    geometry.Display();
    assert(draws.size()==97);
#if defined(SRR2_OLD3DS_MATERIAL_QUEUE) && defined(SRR2_OLD3DS_MATERIAL_QUEUE_SORT)
    a.buffer.persistent=true;
    p3d::context.staticQueueActive=true;
    p3d::context.staticQueueCount=0;
    draws.clear();
    geometry.primGroup.assign(1,&a);
    geometry.Display(); geometry.Display();
    assert(draws.empty() && p3d::context.staticQueueCount==2);
    geometry.primGroup.assign(1,&mutation);
    geometry.Display();
    assert(draws.size()==2 && draws[0].material==1 && draws[1].material==1);
    assert(mutation.calls==2 && p3d::context.staticQueueCount==0);
    geometry.primGroup.assign(1,&a); geometry.Display();
    assert(draws.size()==2 && p3d::context.staticQueueCount==1);
    p3d::context.SubmitMaterialQueue(p3d::context.staticQueue,p3d::context.staticQueueCount);
    p3d::context.staticQueueCount=0; p3d::context.staticQueueActive=false;
    assert(draws.size()==3 && draws[2].material==2);
    draws.clear();
    p3d::context.staticQueueActive=true;
    geometry.primGroup.assign(1,&mutation);
    static tGeometry* nestedGeometry;
    static tGeometry nested;
    nested.primGroup.assign(1,&a); nestedGeometry=&nested;
    nestedHook=[]() { nestedGeometry->Display(); };
    geometry.Display();
    assert(p3d::context.staticQueueCount==0 && draws.size()==1 && draws[0].material==2);
    assert(a.material.shader.revision==3 && p3d::context.staticQueueActive);
    nestedHook=nullptr;
    p3d::context.staticQueueActive=false;
#endif
    std::puts("PASS production Display: side effects once, pre-mutation flush, capacity, null groups, nonpersistent and nonstatic fallback");
}
'''
with tempfile.TemporaryDirectory(prefix='srr2-queue-scope-') as directory:
    path = Path(directory)
    (path/'test.cpp').write_text(harness + '\n' + body + '\n' + tests)
    for enabled,sort in [(False,False),(True,False),(True,True)]:
        command = ['clang++','-std=c++11','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I',str(root)]
        if enabled: command += ['-DSRR2_OLD3DS_MATERIAL_QUEUE']
        if sort: command += ['-DSRR2_OLD3DS_MATERIAL_QUEUE_SORT']
        subprocess.run(command+[str(path/'test.cpp'),'-o',str(path/'test')],check=True)
        subprocess.run([str(path/'test')],check=True)
