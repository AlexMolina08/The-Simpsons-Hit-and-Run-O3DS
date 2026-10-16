#!/usr/bin/env python3
"""Exercise actual PICA index invalidation methods with host stubs and sanitizers."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]
source = (root / 'libs/pure3d/pddi/pica/context.cpp').read_text()
signatures = [
    'void picaPrimBuffer::Unlock(pddiPrimBufferStream*)',
    'unsigned char* picaPrimBuffer::LockIndexBuffer()',
    'void picaPrimBuffer::UnlockIndexBuffer(int count)',
    'void picaPrimBuffer::SetIndices(unsigned short* i, int count)',
    'bool picaPrimBuffer::IndicesInRange(void) const',
]

def extract(signature):
    start = source.index(signature + '\n{')
    opening = source.index('{', start)
    depth = 1
    end = opening + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]

harness = r'''
#include <cassert>
#include <cstring>
#include <cstdio>
#define PDDIASSERT(x) assert(x)
const unsigned PICA_VERTEX_STRIDE = 36;
struct pddiPrimBufferStream {};
void picaFlushForGpu(const void*, unsigned) {}
struct picaPrimBuffer {
    float *coord = nullptr, *normal = nullptr, *uv = nullptr;
    unsigned char *colour = nullptr, *buffer = nullptr;
    unsigned short* indices;
    unsigned total = 0, mem = 0, indexCount = 6, allocated = 4;
    bool transient = false, expandDirty = false;
    int elemState = -1;
    void Unlock(pddiPrimBufferStream*);
    unsigned char* LockIndexBuffer();
    void UnlockIndexBuffer(int);
    void SetIndices(unsigned short*, int);
    bool IndicesInRange() const;
    bool Validate() {
        if(elemState < 0) elemState = IndicesInRange() ? 1 : 0;
        return elemState == 1;
    }
};
'''
tests = r'''
int main() {
    unsigned short indices[6] = {0,1,2,2,1,3};
    picaPrimBuffer b; b.indices = indices;
    assert(b.Validate());
    // Vertex writes cannot change index validity. Expanded copies still dirty.
    b.Unlock(nullptr);
    assert(b.expandDirty);
#ifdef SRR2_OLD3DS_INDEX_CACHE
    assert(b.elemState == 1);
#else
    assert(b.elemState == -1);
#endif
    assert(b.Validate());
    // Direct index writes, including same-count writes, invalidate validation.
    auto* writable = reinterpret_cast<unsigned short*>(b.LockIndexBuffer());
    writable[5] = 4;
    b.UnlockIndexBuffer(0);
    assert(b.elemState == -1 && !b.Validate());
    b.Unlock(nullptr);
#ifdef SRR2_OLD3DS_INDEX_CACHE
    assert(b.elemState == 0);
#else
    assert(b.elemState == -1);
#endif
    assert(!b.Validate());
    unsigned short valid[6] = {3,2,1,1,2,0};
    b.SetIndices(valid,6);
    assert(b.elemState == -1 && b.Validate());
    unsigned short invalid[6] = {0,1,2,2,1,65535};
    b.SetIndices(invalid,6);
    assert(b.elemState == -1 && !b.Validate());
    b.UnlockIndexBuffer(3);
    assert(b.elemState == -1 && b.Validate());
    for(unsigned frame=0; frame<10000; ++frame) {
        b.Unlock(nullptr);
        assert(b.Validate());
    }
    std::puts("index invalidation: vertex updates, index mutation, invalid fallback and count changes passed");
}
'''
with tempfile.TemporaryDirectory(prefix='srr-index-test-') as temp:
    temp = Path(temp)
    cpp = temp / 'test.cpp'
    cpp.write_text(harness + '\n'.join(map(extract, signatures)) + tests)
    for mode in ['reference', 'cache']:
        binary = temp / mode
        args = ['clang++', '-std=c++11', '-Wall', '-Wextra', '-Werror',
                '-fsanitize=address,undefined', str(cpp), '-o', str(binary)]
        if mode == 'cache':
            args.append('-DSRR2_OLD3DS_INDEX_CACHE')
        subprocess.run(args, check=True)
        subprocess.run([str(binary)], check=True)
