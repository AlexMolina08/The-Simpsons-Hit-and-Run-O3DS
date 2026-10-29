#!/usr/bin/env python3
"""Exercise production FTT read/cache methods with bounded, synchronous fake storage."""
from pathlib import Path
import argparse
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--source', type=Path, help='Optional baseline reader for a negative control')
options = parser.parse_args()
source = (options.source or root/'libs/pure3d/p3d/fileftt.cpp').read_text()

def method(signature, end):
    return source[source.index(signature):source.index(end, source.index(signature))]

methods = '\n'.join([
    method('bool tFileFTT::GetData(', '//--------------------------------------------------------------------------'),
    method('void tFileFTT::ReadBuffer(', 'void tFileFTT::Decompress'),
    method('void tFileFTT::FillBuffer()', '//--------------------------------------------------------------------------'),
    source[source.index('void tFileFTT::OpenFile( void )'):],
])
harness = r'''
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>
using radTime64 = uint64_t;
using P3D_U32 = uint32_t;
constexpr int CACHE_SIZE = 192 * 1024, COMPRESSED_BLOCK_SIZE = 4096;
char globalCache[CACHE_SIZE * 2 + 1024];
radTime64 gLastTime = 0, gYieldTime = 5200;
radTime64 radTimeGetMicroseconds64() { return 0; }
uintptr_t radMemoryRoundUp(uintptr_t n, uintptr_t alignment) { return (n + alignment - 1) & ~(alignment - 1); }
namespace rmt { template<class T> T Min(T a, T b) { return std::min(a, b); } }
struct Manager { void SwitchTask() {} } manager;
namespace p3d { Manager* loadManager = &manager; }
#define P3DASSERT(x) assert(x)
struct FakeFile {
    std::vector<unsigned char> bytes;
    size_t cursor = 0;
    bool IsOpen() { return true; }
    int GetSize() { return bytes.size(); }
    unsigned GetOptimalSize() { return 16; }
    void ReadAsync(void* output, unsigned count) {
        auto n = std::min<size_t>(count, bytes.size() - cursor);
        memcpy(output, bytes.data() + cursor, n);
        cursor += n;
    }
} disk;
void radFileOpen(FakeFile** result, const char*) { *result = &disk; }
class tFileFTT {
public:
    enum DataType { BYTE=1, WORD=2, DWORD=4, QWORD=8 };
    FakeFile* m_pIRadFile = nullptr;
    int fileSize=0, compressedFileSize=0, position=0, filePosition=0, currentCache=0;
    char* cache[2] = {};
    char *currentBuffer=nullptr, *compressedBuffer=nullptr, *uncompressedcache=nullptr;
    int currentPos=0, currentSize=0, compressedPos=0, compressedSize=0, nextSize=0, bufferSize=0;
    bool compressed=false;
    const char* GetFullFilename() { return "missing-or-retail.p3d"; }
    void WaitForCompletion() {}
    void EndianSwap(void*, unsigned, unsigned) {}
    void Decompress(const unsigned char*, unsigned, unsigned char*, unsigned) { assert(false); }
    void OpenFile();
    void FillBuffer();
    void ReadBuffer(char*);
    bool GetData(void*, unsigned, DataType);
};
'''
checks = r'''
int main() {
    unsigned char output[32];
    for (int size : {0,1,11,12,CACHE_SIZE-1,CACHE_SIZE,CACHE_SIZE+1,2*CACHE_SIZE,2*CACHE_SIZE+3,4*CACHE_SIZE+17}) {
        disk.bytes.resize(size);
        disk.cursor=0;
        for (int i=0; i<size; ++i) disk.bytes[i]=(i*37+11)&255;
        tFileFTT file;
        memset(output, 0xa5, sizeof output);
        assert(file.GetData(output,0,tFileFTT::BYTE));
        assert(!file.GetData(output,0xffffffff,tFileFTT::QWORD));
        assert(file.position==0 && output[0]==0xa5);
        std::vector<unsigned char> actual(size);
        for (int offset=0; offset<size;) {
            int count=std::min(size-offset, (offset%997)+1);
            assert(file.GetData(actual.data()+offset,count,tFileFTT::BYTE));
            offset+=count;
        }
        assert(actual==disk.bytes);
        assert(!file.GetData(output,1,tFileFTT::BYTE));
        assert(file.position==size && output[0]==0xa5);
        assert(file.nextSize>=0 && file.currentSize>=0);
    }
    puts("FTT: empty/missing input, short files, overflow, EOF and four cache boundaries passed");
}
'''
with tempfile.TemporaryDirectory(prefix='ftt-read-test-') as folder:
    path=Path(folder)
    (path/'test.cpp').write_text(harness+methods+checks)
    subprocess.run(['clang++','-std=c++17','-fsanitize=address,undefined','-fno-omit-frame-pointer',str(path/'test.cpp'),'-o',str(path/'test')],check=True)
    subprocess.run([str(path/'test')],check=True)
