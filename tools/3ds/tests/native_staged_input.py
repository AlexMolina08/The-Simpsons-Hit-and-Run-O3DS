#!/usr/bin/env python3
"""Check private texture input, exact publication and bounded failure paths."""
from pathlib import Path
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[3]
SOURCE = r'''
#include <cassert>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <thread>
#include <vector>
#include <unistd.h>
#ifdef __APPLE__
#include <CommonCrypto/CommonDigest.h>
#else
#include <openssl/sha.h>
#endif
std::mutex loader,gate;
std::condition_variable changed;
bool waitSeek=false,entered=false,allowed=false,enabled=false;
std::atomic<bool> outside{false};
unsigned starts=0,ends=0,allocations=0;
unsigned char* target=nullptr;size_t targetBytes=0;
void Unpublished(){for(size_t i=0;i<targetBytes;++i)assert(target[i]==0xa5);}
int ControlledSeek(FILE* file,long offset,int origin){
    if(waitSeek){std::unique_lock<std::mutex> lock(gate);entered=true;changed.notify_all();
        assert(changed.wait_for(lock,std::chrono::seconds(3),[]{return allowed;}));}
    return std::fseek(file,offset,origin);
}
#define fseek ControlledSeek
#include "pddi/pica/nativetexture.hpp"
#undef fseek
struct Lease {
    bool active=enabled;
    Lease(){Unpublished();++starts;if(active){outside=true;loader.unlock();}}
    ~Lease(){Unpublished();if(active){loader.lock();outside=false;}++ends;}
};
struct Hash {
    bool fail=false;
    bool operator()(const void* data,size_t count,unsigned char* digest){
        Unpublished();
        if(fail)return false;
#ifdef __APPLE__
        return CC_SHA256(data,static_cast<CC_LONG>(count),digest)!=nullptr;
#else
        return SHA256(static_cast<const unsigned char*>(data),count,digest)!=nullptr;
#endif
    }
};
void* Allocate(size_t bytes){assert(!outside);assert(bytes<=65644);++allocations;return std::malloc(bytes);}
void* NoMemory(size_t){assert(!outside);return nullptr;}
FILE* Input(const std::vector<unsigned char>& data){FILE* f=tmpfile();assert(f);
    assert(fwrite(data.data(),1,data.size(),f)==data.size());fflush(f);return f;}
int main(){
    for(unsigned bytes:{1u,8192u,32768u,65536u}){
        std::vector<unsigned char> payload(bytes),output(bytes+32,0xa5);
        target=output.data();targetBytes=output.size();Hash hash;
        for(unsigned i=0;i<bytes;++i)payload[i]=(i*53+i/17)&255;
        unsigned char expected[108]={};
        for(unsigned i=0;i<76;++i)expected[i]=(i*11)&255;
        std::vector<unsigned char> record(expected,expected+108);
        assert(hash(payload.data(),payload.size(),record.data()+76));
        record.insert(record.end(),payload.begin(),payload.end());
        std::vector<unsigned char> bundle(37,0xcc);bundle.insert(bundle.end(),record.begin(),record.end());
        bundle.insert(bundle.end(),103,0xdd);
        for(bool lend:{false,true}){
            enabled=lend;waitSeek=true;entered=allowed=false;bool used=false,ok=false;
            FILE* f=Input(bundle);
            std::thread worker([&]{std::unique_lock<std::mutex> lock(loader);
                ok=picaNativeTexture::ReadStaged<Lease>(f,37,expected,bytes,output.data(),hash,&used,Allocate);
                assert(!outside);});
            {std::unique_lock<std::mutex> lock(gate);
                assert(changed.wait_for(lock,std::chrono::seconds(3),[]{return entered;}));}
            unsigned frames=0;for(unsigned i=0;i<40;++i){if(loader.try_lock()){++frames;Unpublished();loader.unlock();}}
            assert(frames==(lend?40u:0u));
            {std::lock_guard<std::mutex> lock(gate);allowed=true;changed.notify_all();}
            worker.join();assert(ok&&used&&starts==ends&&!outside);
            assert(!memcmp(output.data(),payload.data(),bytes));
            for(unsigned i=bytes;i<output.size();++i)assert(output[i]==0xa5);
            fclose(f);std::fill(output.begin(),output.end(),0xa5);
        }
        waitSeek=false;enabled=true;
        for(unsigned failure=0;failure<6;++failure){
            auto bad=bundle;hash.fail=failure==4;
            if(failure==0)bad[37]^=1;
            if(failure==1)bad[37+108+bytes/2]^=1;
            if(failure==2)bad.resize(37+107);
            if(failure==3)bad.resize(37+107+bytes);
            FILE* f=Input(bad);bool used=false;std::unique_lock<std::mutex> lock(loader);
            assert(!picaNativeTexture::ReadStaged<Lease>(f,failure==5?unsigned(bad.size()+1):37,
                expected,bytes,output.data(),hash,&used,Allocate));
            assert(used&&starts==ends&&!outside);Unpublished();lock.unlock();fclose(f);
        }
        hash.fail=false;FILE* f=Input(bundle);bool used=true;
        std::unique_lock<std::mutex> lock(loader);const unsigned before=starts;
        assert(!picaNativeTexture::ReadStaged<Lease>(f,37,expected,bytes,output.data(),hash,&used,NoMemory));
        assert(!used&&starts==before);Unpublished();
        for(unsigned invalid:{0u,65537u,~0u}){
            assert(!picaNativeTexture::ReadStaged<Lease>(f,37,expected,invalid,output.data(),hash,&used,Allocate));
            assert(!used&&starts==before);}
        assert(!picaNativeTexture::ReadStaged<Lease>(nullptr,0,expected,bytes,output.data(),hash,&used));
        assert(!used&&starts==before);lock.unlock();fclose(f);
    }
    // A real non-seekable FILE fails without destination writes or lost ownership.
    unsigned char expected[108]={},output[64];memset(output,0xa5,sizeof(output));
    target=output;targetBytes=sizeof(output);Hash hash;int pipes[2];assert(!pipe(pipes));
    FILE* f=fdopen(pipes[0],"r");assert(f);bool used=false;loader.lock();
    assert(!picaNativeTexture::ReadStaged<Lease>(f,0,expected,32,output,hash,&used,Allocate));
    assert(used&&!outside&&starts==ends);Unpublished();loader.unlock();fclose(f);close(pipes[1]);
    assert(allocations>0);puts("Private staged texture input passed");
}
'''

texture = (ROOT / 'libs/pure3d/pddi/pica/texture.cpp').read_text()
pure = texture.split('        bool Pure(', 1)[1].split('        bool operator()', 1)[0]
assert 'g_picaYieldHook' not in pure and 'NativeTextureCost::Phase' not in pure
assert 'ReadStaged<radNativeSeek::Lease>' in texture
with tempfile.TemporaryDirectory(prefix='srr-native-staged-') as temporary:
    folder = Path(temporary)
    source = folder / 'test.cpp'
    source.write_text(SOURCE)
    for sanitizer in ('address,undefined', 'thread'):
        binary = folder / ('test-' + sanitizer.split(',')[0])
        subprocess.run(['clang++', '-std=c++14', '-O1', '-g', '-pthread',
                        '-Wno-deprecated-declarations', '-fsanitize=' + sanitizer,
                        '-I', str(ROOT / 'libs/pure3d'), str(source),
                        *([] if sys.platform == 'darwin' else ['-lcrypto']),
                        '-o', str(binary)], check=True)
        subprocess.run([str(binary)], check=True)
