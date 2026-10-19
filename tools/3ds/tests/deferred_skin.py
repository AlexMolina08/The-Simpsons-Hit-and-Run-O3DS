#!/usr/bin/env python3
"""Check production deferred bone recipes against original matrix/eye packing."""
from pathlib import Path
import subprocess,sys,tempfile
root=Path(__file__).resolve().parents[3]
base=subprocess.check_output([sys.executable,str(root/'tools/3ds/tests/frame_pipeline_context.py'),'--deferred','--emit-source'],text=True)
base=base[:base.index('int main(){')]
source=base+r'''
int main(){
 constexpr unsigned bones=32,rounds=1024;
 picaDeferredSkin::Pool pool;
 unsigned seed=73,checked=0;
 auto scalar=[&](){seed=1664525u*seed+1013904223u;return float(int(seed%8193)-4096)/128.0f;};
 for(unsigned frame=0;frame<rounds;++frame){
  pddiMatrix inverse[bones],posed[bones],view;view.Identity();
  const pddiMatrix* inversePointers[bones];const pddiMatrix* posedPointers[bones];bool used[bones];
  for(unsigned row=0;row<4;++row)for(unsigned column=0;column<3;++column)view.rows[row][column]=scalar();
  float eye[4][4];for(unsigned r=0;r<4;++r)for(unsigned c=0;c<4;++c)eye[r][c]=view.rows[r][c];
  for(unsigned i=0;i<bones;++i){inverse[i].Identity();posed[i].Identity();used[i]=true;inversePointers[i]=&inverse[i];posedPointers[i]=&posed[i];
   for(unsigned r=0;r<4;++r)for(unsigned c=0;c<3;++c){inverse[i].rows[r][c]=scalar();posed[i].rows[r][c]=scalar();}}
  auto* rest=picaDeferredSkin::Rest::Create(inversePointers,bones);assert(rest);
  auto* snapshot=pool.Capture(rest,posedPointers,used,view);assert(snapshot);
  picaDeferredSkin::Recipe recipes[2]={};picaPreparedSkinPalette expected[2];
  for(unsigned group=0;group<2;++group){auto& recipe=recipes[group];recipe.snapshot=snapshot;recipe.count=picaSkin::MaxBones;recipe.first=20;expected[group].count=recipe.count;
   for(unsigned i=0;i<recipe.count;++i){unsigned joint=group?(i==0?bones:i+7):i;recipe.joints[i]=joint;
    pddiMatrix product;if(joint==bones)product.Identity();else product.Mult(inverse[joint],posed[joint]);
    float m[4][4];for(unsigned r=0;r<4;++r)for(unsigned c=0;c<4;++c)m[r][c]=product.rows[r][c];
    assert(picaSkin::PackEyeMatrix(expected[group].values[i],m,eye));}}
  rest->Release(); // Skeleton owner may disappear while queued snapshots remain.
  GPU actual;active=&actual;
  std::thread consumer([&]{
   for(unsigned pass=0;pass<2;++pass){
    if(pass)snapshot->ResetConsumer();
    for(unsigned group=0;group<2;++group){assert(picaDeferredSkin::Execute(&recipes[group]));
     for(unsigned bone=0;bone<recipes[group].count;++bone)for(unsigned row=0;row<3;++row){const auto& v=actual.vectors[20+3*bone+row];const float* want=expected[group].values[bone][row];
      assert(bits(v.x)==bits(want[0]) && bits(v.y)==bits(want[1]) && bits(v.z)==bits(want[2]) && bits(v.w)==bits(want[3]));}}
    assert(snapshot->computedCount==bones+1);
   }
  });
  // All producer storage can change immediately after publication.
  for(auto& m:inverse)m={};for(auto& m:posed)m={};view={};
  consumer.join();snapshot->Release();assert(pool.stats.live==0);checked+=4*picaSkin::MaxBones;
 }
 assert(pool.stats.allocated==1 && pool.stats.allocationFailures==0);
 pddiMatrix identity;identity.Identity();const pddiMatrix* inputs[]={&identity};bool used[]={true};
 auto* rest=picaDeferredSkin::Rest::Create(inputs,1);assert(rest);
 picaDeferredSkin::Snapshot* held[picaDeferredSkin::MaximumSnapshots];
 for(auto& snapshot:held){snapshot=pool.Capture(rest,inputs,used,identity);assert(snapshot);}
 assert(!pool.Capture(rest,inputs,used,identity));assert(pool.stats.live==picaDeferredSkin::MaximumSnapshots);
 picaDeferredSkin::Recipe invalid={held[0],1,20,{2}};GPU actual;active=&actual;assert(!picaDeferredSkin::Execute(&invalid));assert(actual.trace.empty());
 for(auto* snapshot:held)snapshot->Release();assert(pool.stats.live==0);rest->Release();
 identity.rows[0].x=std::numeric_limits<float>::infinity();assert(!picaDeferredSkin::Rest::Create(inputs,1));
 printf("PASS: %u palette-bone results match original production matrix multiplication and eye packing bitwise; source poisoning, rest-owner retirement, cache resets, sparse group maps, identity sentinel, lease reuse/cap and invalid-input rejection. Host SDK spies, not hardware FPS.\n",checked);
}
'''
source='#include <limits>\n'+source
with tempfile.TemporaryDirectory(prefix='deferred-skin-') as directory:
 p=Path(directory);(p/'test.cpp').write_text(source)
 subprocess.run(['clang++','-I',str(root/'libs/pure3d'),'-std=c++17','-O1','-g','-fsanitize='+('thread' if '--tsan' in sys.argv else 'address,undefined'),'-fno-omit-frame-pointer',str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True,timeout=60)
