#!/usr/bin/env python3
"""Check that the private distance control preserves every other quality policy."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[3]
source=r'''
#include <cassert>
#include <tuple>
#include <initializer_list>
#include <main/old3dsperformance.h>
namespace Old3dsPerformance { unsigned mode=7;
#ifdef SRR2_OLD3DS_VISUAL_UPGRADE
float visibilityDistance=120.0f;
#endif
}
auto other(){using namespace Old3dsPerformance;return std::make_tuple(Enabled(),ParticleEmissionScale(),DistantPedestrianActions(),FlatLighting(),ReducedAudioSearch(),AggressiveSmallCull(),PhysicsStepMilliseconds(),TrafficLimit(),PedestrianLimit());}
int main(){using namespace Old3dsPerformance;
 for(mode=0;mode<8;++mode){const auto reference=other();
  for(float value:{120.f,160.f,200.f,240.f}){(void)value;
#ifdef SRR2_OLD3DS_VISUAL_UPGRADE
   visibilityDistance=value;
   assert(DrawDistance()==(mode>=4?value:mode>=2?70.f:100.f));
#else
   assert(DrawDistance()==(mode>=4?120.f:mode>=2?70.f:100.f));
#endif
   assert(reference==other());
  }
 }
}
'''
with tempfile.TemporaryDirectory(prefix='srr-visibility-policy-') as name:
    p=Path(name);(p/'main').mkdir();(p/'main/graphics3ds.h').write_text('namespace Graphics3DS { inline bool ImprovedLighting(){return false;} }\n');(p/'test.cpp').write_text(source)
    for enabled in (False,True):
        out=p/('diagnostic' if enabled else 'baseline');flags=['-DSRR2_OLD3DS_VISUAL_UPGRADE'] if enabled else []
        subprocess.run(['clang++','-std=c++17','-O2','-Wall','-Wextra','-Werror','-DSRR2_OLD_3DS','-DSRR2_OLD3DS_NATIVE_BENCH',*flags,'-I'+str(p),'-I'+str(root/'code'),str(p/'test.cpp'),'-o',str(out)],check=True)
        subprocess.run([str(out)],check=True)
print('PASS: private visibility distances 120/160/200/240 across all eight modes; physics, traffic, pedestrians, lighting, audio, particles and small-object policy unchanged. Non-experimental policy remains fixed.')
