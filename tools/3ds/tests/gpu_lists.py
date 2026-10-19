#!/usr/bin/env python3
"""Exercise production retained commands against the existing upstream fixture."""
from pathlib import Path
import subprocess,tempfile,re
root=Path(__file__).resolve().parents[3]
def clean(text):
 return '\n'.join(line for line in text.splitlines() if not line.startswith('#include')).replace('_Static_assert','static_assert')
with tempfile.TemporaryDirectory(prefix='srr-gpu-lists-') as directory:
 p=Path(directory)
 subprocess.run(['python3',str(root/'tools/3ds/tests/native_context.py'),'--emit-source',str(p/'context.cpp')],check=True)
 source=(p/'context.cpp').read_text().replace('using u16=','using GPU_Primitive_t=unsigned;\nusing u16=',1)
 source=source.replace('namespace Candidate {','''namespace Candidate {
#define SRR2_OLD3DS_GPU_LISTS
''',1)
 header=clean((root/'libs/pure3d/pddi/pica/nativegpulist.h').read_text())
 gpu=clean((root/'libs/pure3d/pddi/pica/nativegpulist.c').read_text()).replace('entries=calloc(', 'entries=(Entry*)calloc(').replace('lists=linearAlloc(', 'lists=(u32*)linearAlloc(')
 encoder=clean((root/'libs/pure3d/pddi/pica/drawpacket.c').read_text()).split('void picaDrawPacketElements(')[0]
 encoder=re.sub(r'static_assert\([^;]+;','',encoder)
 marker='typedef picaNativeCommandBlock Packet;'
 source=source.replace(marker,header+'''
void* linearAlloc(size_t size){return calloc(1,size);}
void linearFree(void* p){free(p);}
void GSPGPU_FlushDataCache(void*,unsigned){}
void __real_GPUCMD_Split(u32** address,u32* size){
 GPUCMD_AddWrite(GPUREG_FINALIZE,1);
 if(gpuCmdBufOffset&3)GPUCMD_AddWrite(GPUREG_CMDBUF_SIZE0,0);
 *address=storage;*size=gpuCmdBufOffset;
}
constexpr unsigned GPU_TRIANGLES=0,GPU_GEOMETRY_PRIM=0x300;
'''+gpu+encoder+'\n'+marker,1)
 branch=r'''
#ifdef TEST_GPU_LISTS
  if(Candidate::picaNativeCanCaptureContext() && Candidate::gpuCmdBufSize>=2048) {
   Candidate::picaGpuListsBeginFrame();Candidate::g_picaGpuListsEnabled=true;Candidate::g_picaGpuListsValidate=true;
   C3D_Context copy=initial;u32 expected[384];Candidate::Packet packet={expected,0};Candidate::compileState(&packet,&copy);
   const unsigned common=packet.count;
   const unsigned primitive=(trial%4)*0x100;
   packet.count+=Candidate::picaEncodeDrawPacket(expected+packet.count,primitive,123,true,0x456,1,0);
   assert(Candidate::picaNativeRetainedDraw(primitive,123,true,0x456,1,0));
   assert((Candidate::context.flags&~C3DiF_DrawUsed)==(Reference::context.flags&~C3DiF_DrawUsed));
   assert(Candidate::context.flags&C3DiF_DrawUsed);
   auto saved=Candidate::context;Candidate::context.flags=Reference::context.flags;
   assert(!memcmp(&Reference::context,&Candidate::context,sizeof(initial)));
   const unsigned uniformWords=Reference::gpuCmdBufOffset-common;
   assert(!memcmp(Candidate::storage,Reference::storage+common,uniformWords*4));
   const unsigned callAddress=Candidate::storage[Candidate::gpuCmdBufOffset-6];
   u32* list=nullptr;
   for(unsigned i=0;i<Candidate::Slots;++i){u32* candidate=Candidate::lists+i*Candidate::PICA_GPU_LIST_WORDS;if((Candidate::osConvertVirtToPhys(candidate)>>3)==callAddress)list=candidate;}
   assert(list && Candidate::picaGpuListCompare(list,expected,packet.count));
   u32* segment;u32 words;Candidate::__wrap_GPUCMD_Split(&segment,&words);
   assert(!Candidate::returnCount);
   // Same key must hit without regenerating common state. Uniform dirty bits
   // have already been consumed; they stay consumed on a repeated draw.
   Candidate::context=initial;Candidate::gpuCmdBufOffset=0;
   auto before=Candidate::stats;
   assert(Candidate::picaNativeRetainedDraw(primitive,123,true,0x456,1,0));
   assert(Candidate::stats.hits==before.hits+1 && !Candidate::stats.mismatches);
   assert(!memcmp(&Candidate::context,&saved,sizeof(saved)));
   Candidate::__wrap_GPUCMD_Split(&segment,&words);
   Candidate::context.flags=Reference::context.flags;
   // The legacy fixture below still checks all uniform caches and SDK state.
   memcpy(Candidate::storage,Reference::storage,Reference::gpuCmdBufOffset*4);Candidate::gpuCmdBufOffset=Reference::gpuCmdBufOffset;
  } else Candidate::picaNativeUpdateContext();
#elif defined(TEST_OWNED_SNAPSHOT)
'''
 source=source.replace('#ifdef TEST_OWNED_SNAPSHOT\n  if(Candidate::picaNativeCanCaptureContext()',branch+'  if(Candidate::picaNativeCanCaptureContext()',1)
 # Appends are intentionally replaced by GPU calls, so the old append-count
 # assertion is not an acceptance condition for the new branch.
 source=source.replace('assert(fast>9000 && newAppends<oldAppends);','assert(Candidate::stats.hits>9000 && Candidate::stats.comparisons>18000 && !Candidate::stats.mismatches);\n printf("PASS: retained hits=%u comparisons=%u mismatches=%u; SDK flags, texture state, uniforms and repeated draws match. Not hardware timing.\\n",Candidate::stats.hits,Candidate::stats.comparisons,Candidate::stats.mismatches);')

 source=source.replace(' assert(Candidate::stats.hits>9000',r"""
 // Fill all ways of one set in a single frame. A fifth key must fall back,
 // never replace commands referenced by any earlier draw in this frame.
 Candidate::picaGpuListsBeginFrame();Candidate::gpuCmdBufOffset=0;Candidate::gpuCmdBufSize=8192;
 std::vector<u32> colliding;
 for(u32 k=0;colliding.size()<5;++k)if(((2166136261u^k)*16777619u)%64==0)colliding.push_back(k);
 u32* held[4];
 for(unsigned i=0;i<4;++i){bool hit;held[i]=Candidate::picaGpuListFind(&colliding[i],1,&hit);assert(held[i] && !hit);held[i][0]=0;held[i][1]=GPUCMD_HEADER(0,15,GPUREG_CMDBUF_SIZE0);Candidate::picaGpuListCommit(held[i],2);Candidate::picaGpuListCall(held[i]);}
 bool hit;assert(!Candidate::picaGpuListFind(&colliding[4],1,&hit));
 u32* segment;u32 words;Candidate::__wrap_GPUCMD_Split(&segment,&words);
 for(unsigned i=0;i<4;++i){u32* stop=i+1<4?Candidate::returns[i+1].continuation:segment+words;assert(*Candidate::returns[i].size==u32(stop-Candidate::returns[i].continuation)*4/8);}
 Candidate::picaGpuListsBeginFrame();assert(Candidate::picaGpuListFind(&colliding[4],1,&hit) && !hit);
 puts("PASS: four-way in-flight eviction protection and exact finalized return spans.");
 assert(Candidate::stats.hits>9000""")
 (p/'test.cpp').write_text(source)
 subprocess.run(['clang++','-std=c++17','-O2','-Wall','-Wextra','-Werror','-Wno-sizeof-array-div','-DTEST_GPU_LISTS','-fsanitize=address,undefined',str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
